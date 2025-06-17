```
echo start > /sys/class/remoteproc/remoteproc0/state
echo stop > /sys/class/remoteproc/remoteproc0/state
dmesg | tail

west build -b stm32mp157c_dk2 app
```

TODO:
create ultrasonic class