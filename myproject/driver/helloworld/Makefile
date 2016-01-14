# KERNELRELEASE can not KERNEL_RELEASE, then MODPOST 0 modules
ifneq ($(KERNELRELEASE),)
	obj-m := hello.o
else
	KERNELDIR ?= /lib/modules/$(shell uname -r)/build
#	KERNELDIR := /usr/src/linux-source-3.2.0/linux-source-3.2.0
	PWD := $(shell pwd)
default:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules
clean:
	rm *.ko *.o *.mod.c modules.* Module.*
endif
