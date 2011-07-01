MODULE_NAME := kedr_sample

ifeq ($(KERNELRELEASE),)
# 'Out-of-kernel' part

KBUILD_DIR ?= /lib/modules/$(shell uname -r)/build 
PWD := $(shell pwd)

all: ${MODULE_NAME}.ko

${MODULE_NAME}.ko: module.c functions.c
	$(MAKE) -C $(KBUILD_DIR) M=$(PWD) modules
	
install:
	$(MAKE) -C $(KBUILD_DIR) M=$(PWD) modules_install 

clean:
	$(MAKE) -C $(KBUILD_DIR) M=$(PWD) clean

.PHONY: all clean install

else
# KBuild part. 
# It is used by the kernel build system to actually build the module.
ccflags-y :=  -I$(src)
obj-m := ${MODULE_NAME}.o
${MODULE_NAME}-y := module.o functions.o

endif
