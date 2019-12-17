obj-m+=ram.o
obj-m += rle_simple_module.o

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules
clean:
	rm -f *.o *.ko *.mod.c \
		.rle_simple_module.ko.cmd \
		.rle_simple_module.mod.o.cmd \
		.rle_simple_module.o.cmd \
		.ram.ko.cmd \
		.ram.mod.o.cmd \
		.ram.o.cmd \
		Module.symvers \
		modules.order \
		.tmp_versions/*
