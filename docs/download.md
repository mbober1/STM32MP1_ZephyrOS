echo -n /lib/firmware > /sys/module/firmware_class/parameters/path
echo -n zephyr.elf > /sys/class/remoteproc/remoteproc0/firmware
echo start > /sys/class/remoteproc/remoteproc0/state
cat /sys/class/remoteproc/remoteproc0/state

modprobe rpmsg_client_sample
modprobe rpmsg_tty