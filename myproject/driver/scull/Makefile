# KERNELRELEASE can not KERNEL_RELEASE, then MODPOST 0 modules
ifneq ($(KERNELRELEASE),)
	obj-m := scull.o
else
	KERNELDIR ?= /lib/modules/$(shell uname -r)/build
# /usr/src/linux-headers-2.6.32-38-generic in default ubuntu 10.04.4
#	KERNELDIR := /usr/src/linux-source-3.2.0/linux-source-3.2.0
	PWD := $(shell pwd)
default:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules
clean:
	rm *.ko *.o *.mod.c modules.* Module.*
endif
