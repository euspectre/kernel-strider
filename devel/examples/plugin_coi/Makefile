MODULE_NAME := kedr_drd_plugin_coi

KBUILD_DIR := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

KEDR_INSTALL_PREFIX := /usr/local
KEDR_INCLUDE_DIR := 
SYMVERS_DIR := ${KEDR_INSTALL_PREFIX}/lib/modules/$(shell uname -r)/symvers

SYMVERS_FILES := \
	${SYMVERS_DIR}/kedr_mem_core.symvers	\
	${SYMVERS_DIR}/kedr_func_drd.symvers	\
	${SYMVERS_DIR}/kedr_coi.symvers

ifeq ($(KERNELRELEASE),)
# 'Out-of-kernel' part 

all: ${MODULE_NAME}.ko

${MODULE_NAME}.ko: module.c ${SYMVERS_FILES}
	cat ${SYMVERS_FILES} > Module.symvers
	$(MAKE) -C ${KBUILD_DIR} M=${PWD} modules

clean:
	$(MAKE) -C ${KBUILD_DIR} M=${PWD} clean

.PHONY: all clean

else
# KBuild part. 
# It is used by the kernel build system to actually build the module.
ccflags-y := -g -I$(src) -I${KEDR_INSTALL_PREFIX}/include
obj-m := ${MODULE_NAME}.o
${MODULE_NAME}-y := module.o 

endif
