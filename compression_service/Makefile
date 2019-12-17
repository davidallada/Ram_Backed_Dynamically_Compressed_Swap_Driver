obj-m += ex_module.o
ex_module-y := example_module.o network_compression_client.o
ccflags-y := -std=gnu99 -Wno-declaration-after-statement -Wno-vla

all:
		make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
		make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
