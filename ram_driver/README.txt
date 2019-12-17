A couple of things.
When using sudo insmod ram.ko, ram0 to ram15 show up in /dev. They are probably using our device driver.
dmesg shows kernel module output

USE "modinfo ram.ko" to see options and info
use df /dev/ram0 for info

TO ALLOCATE 1 GIG OF RAM BACKED STORAGE:
sudo insmod ram.ko rd_nr=1 rd_size=1000000 max_part=1
sudo fdisk -l


cat /sys/block/sd*/*/nr_requests


https://github.com/torvalds/linux/blob/master/include/linux/blkdev.h
