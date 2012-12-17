This example demonstrates how to use debug info in DWARF format to determine
source file names and line numbers corresponding to a given address in the
code of a kernel module.

DWARF processing facilities from elfutils and related packages are used
here.
============================================================================

Description

The example outputs the source line information for a given address based on
the debug info from the file specified as the argument (a .ko file of a
kernel module or .ko.debug file if the debug info is in this separate file).
	
The address is currently hard-coded in main.cpp as a pair
(offset, section_start).

'offset' is an offset in an ELF section of the kernel module.

'section_start' is the offset of the section in the .ko file minus the
minimal offset of the loadable sections there (that is, this is where the
section would be loaded if the first loadable section were loaded at the
address 0 and the sections were loaded according to their offsets only).
It can be computed using libelf API but I have no time for this now.

One can change 'offset' and 'section_start' as needed and rebuild the
application.

[NB] The application also handles the cases when the address in question is
in an inlined function. The full chain of inlines should be output in this
case in a manner similar to a stack trace.
============================================================================

Prerequisites

- elfutils-devel and elfutils
- libdw/libdw1 and libdw-devel (if these are in separate packages) 
- libelf and libelf-devel (if these are in separate packages)
- libebl/libebl1 (if it is in a separate package)

After installing necessary packages, please check that
/usr/lib{|64}/elfutils/libebl_<arch>.so exist, where <arch> is the
architecture of the system the file with debug info belongs to.
============================================================================

Build

g++ -Wall -Wextra -o dw_addr2line main.cpp -lelf -ldw
============================================================================

Usage

./dw_addr2line <file_with_debug_info>
============================================================================

Notes

There is a memory leak of 32 bytes in libdw that shows up in this example.
This is somehow related to the loading of the appropriate libebl_*.so file.

Currently, I don't know how to fix it, it is deep inside libdw and it is a
problem in that library perhaps.

Valgrind says these 32 bytes are still reachable at the application's exit.

Still, this leak should not be much of a problem: it happens only once per
loading of libebl_*.so file, so no more than once per a file with debug info
that is loaded.
============================================================================
