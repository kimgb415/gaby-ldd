# KERNELRELEASE variable is set by the kernel build system
ifneq ($(KERNELRELEASE),)

scull-objs := main.o
obj-m := scull.o

# if make is run by user in scull directory
else

KERNELDIR ?= /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

modules:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) LDDINC=$(PWD)/../include modules

endif

clean:
	rm -rf *.o *~ core .*.cmd *.ko *.mod.c *.mod .tmp_versions Module.symvers modules.order