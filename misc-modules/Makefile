ifneq ($(KERNELRELEASE),)
	obj-m += complete.o

else
	KDIR ?= ~/kernels/staging
	PWD := $(shell pwd)

modules:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean

endif