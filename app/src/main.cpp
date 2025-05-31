#include "ipc.hpp"
#include <zephyr/drivers/ipm.h>
#include <openamp/open_amp.h>
#include <zephyr/logging/log.h>
#include <metal/device.h>
#include <resource_table.h>

LOG_MODULE_REGISTER(openamp_rsc_table);

#define APP_TTY_TASK_STACK_SIZE (1536)
static struct k_thread thread_tty_data;
K_THREAD_STACK_DEFINE(thread_tty_stack, APP_TTY_TASK_STACK_SIZE);

int main(void)
{
	LOG_INF("Starting custom!");
	k_thread_create(&thread_tty_data, thread_tty_stack, APP_TTY_TASK_STACK_SIZE,
			ipc_task,
			NULL, NULL, NULL, K_PRIO_COOP(7), 0, K_NO_WAIT);
	return 0;
}
