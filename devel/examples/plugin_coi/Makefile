MODULE_NAME := kedr_drd_plugin_coi

KBUILD_DIR := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

KEDR_INSTALL_PREFIX := /usr/local
SYMVERS_DIR := ${KEDR_INSTALL_PREFIX}/lib/modules/$(shell uname -r)/symvers
TEMPLATES_DIR := ${KEDR_INSTALL_PREFIX}/share/kedr-coi/templates
INTERCEPTORS_DIR := ${KEDR_INSTALL_PREFIX}/share/kedr-coi/interceptors
KEDR_GEN_TOOL := ${KEDR_INSTALL_PREFIX}/lib/kedr/kedr_gen

SYMVERS_FILES := \
	${SYMVERS_DIR}/kedr_mem_core.symvers	\
	${SYMVERS_DIR}/kedr_func_drd.symvers	\
	${SYMVERS_DIR}/kedr_coi.symvers

INTERCEPTOR_FILES := \
	cdev_fops_interceptor.c \
	cdev_fops_interceptor.h \
	fops_interceptor.c

ifeq ($(KERNELRELEASE),)
# 'Out-of-kernel' part 

all: ${MODULE_NAME}.ko

${MODULE_NAME}.ko: module.c ${INTERCEPTOR_FILES} ${SYMVERS_FILES}
	cat ${SYMVERS_FILES} > Module.symvers
	$(MAKE) -C ${KBUILD_DIR} M=${PWD} modules

fops_interceptor.c: ${INTERCEPTORS_DIR}/file_operations_interceptor.c
	cp -p $^ $@

cdev_fops_interceptor.c cdev_fops_interceptor.h: \
	cdev_fops_interceptor.%: cdev_fops.data
	${KEDR_GEN_TOOL} ${TEMPLATES_DIR}/kedr_coi_factory_interceptor.$*/ $^ > $@
 
clean:
	$(MAKE) -C ${KBUILD_DIR} M=${PWD} clean
	rm -f fops_interceptor.c
	rm -f cdev_fops_interceptor.c cdev_fops_interceptor.h

.PHONY: all clean

else
# KBuild part. 
# It is used by the kernel build system to actually build the module.
ccflags-y := -g -I$(src) -I${KEDR_INSTALL_PREFIX}/include
obj-m := ${MODULE_NAME}.o
${MODULE_NAME}-y := module.o cdev_fops_interceptor.o fops_interceptor.o

endif
