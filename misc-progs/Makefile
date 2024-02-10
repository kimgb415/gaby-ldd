FILES = scull_ioctl non_blocking_test poll_scull_pipe

KERNELDIR ?= /lib/modules/$(shell uname -r)/build
INCLUDEDIR := $(KERNELDIR)/include
CFLAGS = -O2 -fomit-frame-pointer -Wall -I$(INCLUDEDIR)

all: $(FILES)

clean:
	rm -f $(FILES) *~ core

