MODULE_NAME := kedr_foo

ifeq ($(KERNELRELEASE),)
# 'Out-of-kernel' part
# It is used in the first place when the user executes 'make [target]'.
# Eventually, it will invoke the kernel build system and that, in turn, will
# execute the 'KBuild' part of this Makefile
# (see "$(MAKE) -C $(KERNELDIR) M=$(PWD)" stuff).

KERNELDIR ?= /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

modules:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) clean

.PHONY: modules modules_install clean

else
# KBuild part. 
# It is used by the kernel build system to actually build the module.

obj-m := $(MODULE_NAME).o
$(MODULE_NAME)-y := \
	module.o \
	sections.o
	
endif
