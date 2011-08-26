MODULE_NAME := kedr_sample

# This subdirectory contains necessary files for both x86 and x86-64.
ARCH_DIR := arch/x86

# This auxiliary file will be generated during the build (x86 instruction 
# tables as C code).
INAT_TABLES_FILE := inat-tables.h

ifeq ($(KERNELRELEASE),)
# 'Out-of-kernel' part

MODULE_SOURCES := \
	module.c \
	functions.c \
	debug_util.c \
	detour_buffer.c

KBUILD_DIR ?= /lib/modules/$(shell uname -r)/build 
PWD := $(shell pwd)

all: $(MODULE_NAME).ko

$(MODULE_NAME).ko: $(MODULE_SOURCES)
	$(MAKE) -C $(KBUILD_DIR) M=$(PWD) modules
	
install:
	$(MAKE) -C $(KBUILD_DIR) M=$(PWD) modules_install 

clean:
	$(MAKE) -C $(KBUILD_DIR) M=$(PWD) clean
	rm -f $(ARCH_DIR)/lib/$(INAT_TABLES_FILE)

.PHONY: all clean install

else
# KBuild part. 
# It is used by the kernel build system to actually build the module.
ccflags-y :=  -I$(src) -I$(src)/$(ARCH_DIR)/include -I$(obj)/$(ARCH_DIR)/lib

obj-m := $(MODULE_NAME).o
$(MODULE_NAME)-y := \
	module.o \
	functions.o \
	debug_util.o \
	detour_buffer.o \
	$(ARCH_DIR)/lib/inat.o \
	$(ARCH_DIR)/lib/insn.o

$(obj)/$(ARCH_DIR)/lib/inat.o: $(obj)/$(ARCH_DIR)/lib/$(INAT_TABLES_FILE) $(src)/$(ARCH_DIR)/lib/inat.c

# Note "LC_ALL=C" before awk, it is necessary. Without it, the awk script 
# will report semantic error in the input file on some systems. This is 
# probably due to some locale-related issues that affect separator characters,
# string parsing, etc.
$(obj)/$(ARCH_DIR)/lib/$(INAT_TABLES_FILE): $(src)/$(ARCH_DIR)/lib/x86-opcode-map.txt 
	LC_ALL=C awk -f $(src)/$(ARCH_DIR)/tools/gen-insn-attr-x86.awk $< > $@

endif
