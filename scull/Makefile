# If KERNELRELEASE is defined, we've been invoked from the
# kernel build system and can use its language.
ifneq ($(KERNELRELEASE),)
	obj-$(CONFIG_SCULL) += scull.o
	scull-y := main.o pipe.o
	ccflags-$(CONFIG_DEBUG_SCULL) := -O -g -DSCULL_DEBUG

# Otherwise we were called directly from the command
# line; invoke the kernel build system.
else
	KDIR ?= ../../../
	PWD := $(shell pwd)

modules:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean

endif


# dependency tracking in modern kernel is document in
# https://github.com/torvalds/linux/blob/master/Documentation/kbuild/makefiles.rst#dependency-tracking