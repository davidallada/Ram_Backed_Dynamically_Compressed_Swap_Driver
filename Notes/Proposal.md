# Proposal

In modern computer systems memory is a precious resource that is critical to performance. 
Thus, finding ways to best utilize the given memory in a system without sacrificing efficiency is of particular interest. 
One approach that permits greater memory efficiency is to compress the existing memory, but this requires additional computational power.
Unfortunately, using processing power from the CPU is inefficient for the system as a whole. 
Luckily, many systems come with dedicated GPUs that are often sitting idle. The natural solution would then be to utilize the 
available GPU processing power to do the required compression. In this way, we gain the efficiency of compressed memory without adding overhead to the CPU and use otherwise unused GPU computation.

# Overview

Get device driver installed and running properly. Also redirect requests to a different device.  20%

Allocate memory in memory and be able to store data in that “Ram Disk” 15%

Be able to compress memory in said “Ram Disk” 20%

Offload the computation needed to the GPU 40%

Create a memory manager to efficiently determine which data should be compressed. 5%

## David’s Brain Dump

GOAL: Create a device driver, register it, and forward writes/reads through our device to an existing disk. 

Create a device driver:

Initialize the device with alloc_Disk and then register it with the kernel. Then we need to set all the values in the gen_disk struct. Call add_disk to add the gendisk to the list of gendisks, and create a partition.

Forward reads/writes through our device to an existing device
Our device driver reads a request queue (sbd_request) and then calls sbd_transfer to actually transfer the bits. Instead, we think that we can take the request, change the target device pointer to an existing disk (gendisk) and then add the modified request to the existing disk’s request_queue

File here for sbd_request: https://lwn.net/Articles/58720/ 

Device references might be in /dev

ISSUE: HOW DO WE GET THE REFERENCE FOR OTHER GENDISKS/PARTITIONS SO THAT WE KNOW WHICH DEVICE TO FORWARD ONTO *MAYBE JUST HARDCODE, WHO KNOWS

https://github.com/torvalds/linux/blob/master/include/linux/genhd.h
http://www.cs.uni.edu/~diesburg/courses/dd/notes/lecture_block.ppt 

Maybe disk_part_table? Dev_to_disk? part_to_dev? We need to find the gendisk reference to the other hard disks to be able to add to their queues.

In /dev there is /dev/graphics which might help with getting the graphics device

NEXT STEPS:
Register our own device driver (Using example block driver as start)
Modify that device driver and find a way to get a reference to another device and forward the requests on.

# QUESTION
- Whats a distinction between registering a device with the kernel and mounting a file system.
Also how is this important for being “backed swap”
- Is it possible that hard coding is the best way to access the other disks?