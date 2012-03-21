An example to demonstrate how to create a list of definitions of the field 
offsets into structures in a header file. The file can then be used even
from the assembly code.

This example is based on what is done in the kernel itself (as of version 
3.3.0), see the top-level Kbuild file, generation of asm-offsets.h.

Build:
	make

Results:
	see kedr_asm_offsets.h
