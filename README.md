# Systems-Capstone
Repository for the Systems Capstone Fall 2019 Device Driver Group.
Most of the important stuff is found in the ram_driver folder.

Overview:
This repository creates a device driver that lives in ram. We install a swap file onto this device and use it
to compress incoming pages dynamically. 

Installation:
Before you run this code, you need to update your FSTAB file. 
Your fstab should look like: 
/swapfile none swap sw,pri=1 0 0
/dev/ram0p1 swap swap defaults,pri=3 0 0

To Run: You can change the parameters of the device in the ./install_ram script. These include part compressed, high watermark, low watermark, and size.
To create the swap partition, run ./create_swap
Your device should now be functionining. You can run dmesg on the command line to see debug output.

Useful References: 

Device Driver Basics: https://www.tldp.org/LDP/khg/HyperNews/get/devices/basics.html

pdflush info: http://bogdan.org.ua/wp-content/uploads/2013/07/The-Linux-Page-Cache-and-pdflush.pdf

Swap Priority Editing: https://www.thegeekdiary.com/centos-rhel-how-to-prioritize-the-devices-used-for-swap-partition/

Textbook for Device Drivers: https://lwn.net/Kernel/LDD3/

Code References for the Textbook:
https://github.com/jesstess/ldd4/blob/master/sbull/sbull.c
