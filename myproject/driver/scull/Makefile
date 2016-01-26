# KERNELRELEASE can not KERNEL_RELEASE, then MODPOST 0 modules
DEBUG=y

ifeq ($(DEBUG), y)
  DEBFLAGS = -O -g -DSCULL_DEBUG
else
  DEBFLAGS = -O2
endif

EXTRA_CFLAGS += $(DEBFLAGS)

ifneq ($(KERNELRELEASE),)
	scull-objs := scull_sz.o pipe_sz.o # main objs
	obj-m := scull.o # obj main
else
	KERNELDIR ?= /lib/modules/$(shell uname -r)/build
# /usr/src/linux-headers-2.6.32-38-generic in default ubuntu 10.04.4
#	KERNELDIR := /usr/src/linux-source-3.2.0/linux-source-3.2.0
	PWD := $(shell pwd)
default:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules
clean:
	rm -rf *.ko *.o *.mod.c modules.* Module.*
endif
