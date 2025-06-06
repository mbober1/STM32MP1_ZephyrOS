#include "ipc.hpp"

#include <zephyr/device.h>
#include <string.h>
#include <stdlib.h>

#include <zephyr/drivers/ipm.h>

#include <openamp/open_amp.h>
#include <metal/device.h>
#include <resource_table.h>

#include "ultrasonic.hpp"

#define RPMSG_CHAN_NAME "rpmsg-tty"
#define SHM_DEVICE_NAME "shm"
// #define CONFIG_OPENAMP_IPC_DEV_NAME "$(dt_chosen_label,$(DT_CHOSEN_Z_IPC))"

#if !DT_HAS_CHOSEN(zephyr_ipc_shm)
#error "Application requires definition of shared memory for rpmsg"
#endif

/* Constants derived from device tree */
#define SHM_NODE        DT_CHOSEN(zephyr_ipc_shm)
#define SHM_START_ADDR  DT_REG_ADDR(SHM_NODE)
#define SHM_SIZE        DT_REG_SIZE(SHM_NODE)

static const struct device *ipm_handle;

static metal_phys_addr_t shm_physmap = SHM_START_ADDR;

struct metal_device shm_device = {
    .name = SHM_DEVICE_NAME,
    .num_regions = 2,
    .regions = {
        {.virt = NULL}, /* shared memory */
        {.virt = NULL}, /* rsc_table memory */
    },
    .node = { NULL },
    .irq_num = 0,
    .irq_info = NULL
};

static struct metal_io_region *shm_io;
static struct rpmsg_virtio_shm_pool shpool;

static struct metal_io_region *rsc_io;
static struct rpmsg_virtio_device rvdev;

static fw_resource_table *rsc_table;

static char rcv_msg[20];
static char tx_buffer[64];
static unsigned int rcv_len;
static struct rpmsg_endpoint rcv_ept;

static K_SEM_DEFINE(data_sem, 0, 1);
static K_SEM_DEFINE(data_rx_sem, 0, 1);

static void platform_ipm_callback(const struct device *device, void *context, uint32_t id, volatile void *data)
{
    k_sem_give(&data_sem);
}

static int rpmsg_recv_callback(struct rpmsg_endpoint *ept, void *data, size_t len, uint32_t src, void *priv)
{
    memcpy(rcv_msg, data, len);
    rcv_len = len;
    k_sem_give(&data_rx_sem);

    return RPMSG_SUCCESS;
}

static void receive_message(char **msg, unsigned int *len)
{
    while (k_sem_take(&data_rx_sem, K_NO_WAIT) != 0) {
        int status = k_sem_take(&data_sem, K_FOREVER);

        if (status == 0)
            rproc_virtio_notified(rvdev.vdev, VRING1_ID);
    }
    *len = rcv_len;
    *msg = rcv_msg;
}

static void new_service_cb(struct rpmsg_device *rdev, const char *name,
                           uint32_t src)
{
    printk("%s: unexpected ns service receive for name %s\n", __func__, name);
}

int mailbox_notify(void *priv, uint32_t id)
{
    ARG_UNUSED(priv);

    ipm_send(ipm_handle, 0, id, NULL, 0);

    return 0;
}

int platform_init(void)
{
    static struct fw_resource_table *rsc_tab_addr;
    int rsc_size;
    struct metal_device *device;
    struct metal_init_params metal_params = METAL_INIT_DEFAULTS;
    int status;

    status = metal_init(&metal_params);
    if (status) {
        printk("metal_init: failed: %d\n", status);
        return -1;
    }

    status = metal_register_generic_device(&shm_device);
    if (status) {
        printk("Couldn't register shared memory: %d\n", status);
        return -1;
    }

    status = metal_device_open("generic", SHM_DEVICE_NAME, &device);
    if (status) {
        printk("metal_device_open failed: %d\n", status);
        return -1;
    }

    /* declare shared memory region */
    metal_io_init(&device->regions[0], (void *)SHM_START_ADDR, &shm_physmap,
                  SHM_SIZE, -1, 0, NULL);

    shm_io = metal_device_io_region(device, 0);
    if (!shm_io) {
        printk("Failed to get shm_io region\n");
        return -1;
    }

    /* declare resource table region */
    rsc_table_get(&rsc_tab_addr, &rsc_size);
    rsc_table = (fw_resource_table*)rsc_tab_addr;

    metal_io_init(&device->regions[1], rsc_table,
                  (metal_phys_addr_t *)rsc_table, rsc_size, -1, 0, NULL);

    rsc_io = metal_device_io_region(device, 1);
    if (!rsc_io) {
        printk("Failed to get rsc_io region\n");
        return -1;
    }

    /* setup IPM */
    ipm_handle = device_get_binding(CONFIG_OPENAMP_IPC_DEV_NAME);
    if (!ipm_handle) {
        printk("Failed to find ipm device\n");
        return -1;
    }

    ipm_register_callback(ipm_handle, platform_ipm_callback, NULL);

    status = ipm_set_enabled(ipm_handle, 1);
    if (status) {
        printk("ipm_set_enabled failed\n");
        return -1;
    }

    return 0;
}

struct  rpmsg_device *
platform_create_rpmsg_vdev(unsigned int vdev_index,
                           unsigned int role,
                           void (*rst_cb)(struct virtio_device *vdev),
                           rpmsg_ns_bind_cb ns_cb)
{
    struct fw_rsc_vdev_vring *vring_rsc;
    struct virtio_device *vdev;
    int ret;

    vdev = rproc_virtio_create_vdev(VIRTIO_DEV_DEVICE, VDEV_ID,
                                    rsc_table_to_vdev(rsc_table),
                                    rsc_io, NULL, mailbox_notify, NULL);
    if (!vdev) {
        printk("failed to create vdev\r\n");
        return NULL;
    }

    /* wait master rpmsg init completion */
    rproc_virtio_wait_remote_ready(vdev);

    vring_rsc = rsc_table_get_vring0(rsc_table);
    ret = rproc_virtio_init_vring(vdev, 0, vring_rsc->notifyid,
                                  (void *)vring_rsc->da, rsc_io,
                                  vring_rsc->num, vring_rsc->align);
    if (ret) {
        printk("failed to init vring 0\r\n");
        goto failed;
    }

    vring_rsc = rsc_table_get_vring1(rsc_table);
    ret = rproc_virtio_init_vring(vdev, 1, vring_rsc->notifyid,
                                  (void *)vring_rsc->da, rsc_io,
                                  vring_rsc->num, vring_rsc->align);
    if (ret) {
        printk("failed to init vring 1\r\n");
        goto failed;
    }

    rpmsg_virtio_init_shm_pool(&shpool, NULL, SHM_SIZE);
    ret =  rpmsg_init_vdev(&rvdev, vdev, ns_cb, shm_io, &shpool);

    if (ret) {
        printk("failed rpmsg_init_vdev\r\n");
        goto failed;
    }

    return rpmsg_virtio_get_rpmsg_device(&rvdev);

failed:
    rproc_virtio_remove_vdev(vdev);

    return NULL;
}


void ipc_task(void *arg1, void *arg2, void *arg3)
{
    ARG_UNUSED(arg1);
    ARG_UNUSED(arg2);
    ARG_UNUSED(arg3);

    struct rpmsg_device *rpdev;
    char *msg;
    unsigned int len;
    int ret = 0;

    /* Initialize platform */
    ret = platform_init();
    if (ret) {
        printk("Failed to initialize platform\n");
        return;
    }

    rpdev = platform_create_rpmsg_vdev(0, VIRTIO_DEV_DEVICE, NULL,
                                       new_service_cb);
    if (!rpdev) {
        printk("Failed to create rpmsg virtio device\n");
        return;
    }

    ret = rpmsg_create_ept(&rcv_ept, rpdev, RPMSG_CHAN_NAME,
                           RPMSG_ADDR_ANY, RPMSG_ADDR_ANY,
                           rpmsg_recv_callback, NULL);
    if (ret != 0)
        printk("error while creating endpoint(%d)\n", ret);

    /* Handle incoming messsages and send replies when needed */
    for (;;) {
        receive_message(&msg, &len);
        
        int ret = 0;

        if (strcmp(msg, "dist\n") == 0)
        {
            float distance = get_distance();
            ret = snprintf(tx_buffer, sizeof(tx_buffer), "%f", distance);

        }
        else
        {
            ret = snprintf(tx_buffer, sizeof(tx_buffer), "Not known command");
        }

        if (ret > 0)
        {
            rpmsg_send(&rcv_ept, tx_buffer, ret);
        }
        // printk("MESSAGE: %s\n", msg);
    }
}
