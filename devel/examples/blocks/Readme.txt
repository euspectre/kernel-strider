This example demonstrates splitting the code of the target kernel module 
into blocks (the blocks can later be used for instrumentation).

"Blocks" is based on "Detour" example (with some parts disabled).

Usage:
  insmod kedr_sample.ko target_name=<specify_target_name_here>

When the target is loaded, "kedr_sample" is notified (similar to KEDR 
itself). Then "kedr_sample" processes each function and detects the 
boundaries of the blocks in it. 

A block contains one or more machine instructions. 
The rules used to split the function code into such blocks: 
- if an instruction may transter control outside of the current function,
  it constitutes a separate block; note that in addition to some of the 
  calls and jumps, the instructions like 'ret' and 'int' fall into this 
  group;
- if an instruction transfers control to a location before it within the 
  function (a "backward jump" in case of 'for'/'while'/'do' constructs, 
  etc.), it constitutes a separate block;
  note that rep-prefixed instructions do not fall into this group;
- each 'jmp near r/m32' instruction constitutes a separate block, same
  for 'jmp near r/m64';
- near indirect jumps must always transfer control to the beginning of
  a block;
- if an instruction transfers control to a location before it within the 
  function, it is allowed to transfer control only to the beginning of 
  a block; 
- it is allowed for a block to contain the instructions that transfer 
  control forward within the function, not necessarily within the block
  such instructions need not be placed in separate blocks. */

See the comments in the code for details.

For each function "kedr_sample" outputs the maximum size of a block (in 
bytes) via the custom debug output file (<debugfs>/kedr_sample/output).

[NB] It seems, this example can be used even if an ftrace tracer other than 
'nop' is enabled at the same time. That is, even if ftrace overwrites 'call 
mcount' with something nontrivial in the module. Ftrace does this before 
our system instruments the module, so we'll see the changes it have made 
and split the code into blocks accordingly.

-----------------------------------------
Tested on the following systems (modules: wacom, btrfs, xfs, nfs, e1000, 
drbd, ...):
- OpenSUSE 11.4 x86-64, kernel 2.6.37
- OpenSUSE 11.3 x86, kernel 2.6.34
- OpenSUSE 11.2 x86, kernel 2.6.31
- OpenSUSE 11.3 x86, vanilla kernel 2.6.39-rc4
- Fedora 15 x86, kernel 2.6.38 (PAE)
- Fedora 14 x86-64, kernel 2.6.35
- Ubuntu 11.04 x86-64, kernel 2.6.38
- Mandriva 2010 x86, kernel 2.6.31
- Debian 6 x86, kernel 2.6.32
- Debian 6 x86-64, kernel 2.6.32
- RHEL6 x86-64, kernel 2.6.32
- RHEL6 x86-64, kernel 2.6.32-debug
