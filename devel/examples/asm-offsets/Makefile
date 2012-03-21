OFFSETS_HEADER := kedr_asm_offsets.h

KBUILD_DIR ?= /lib/modules/$(shell uname -r)/build 
PWD := $(shell pwd)

HEADERS = functions.h local_storage.h

all: $(OFFSETS_HEADER)

$(OFFSETS_HEADER): kedr_asm_offsets.c Kbuild $(HEADERS)
	$(MAKE) -C $(KBUILD_DIR) M=$(PWD)

clean:
	$(MAKE) -C $(KBUILD_DIR) M=$(PWD) clean

.PHONY: all clean
