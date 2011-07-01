This example demonstrates "detoured" execution of the target kernel module.

Usage:
  insmod kedr_sample.ko target_name=<specify_target_name_here>

When the target is loaded, "kedr_sample" is notified (similar to KEDR 
itself). Then "kedr_sample" allocates a "detour buffer" from the same 
memory address area as the code of the kernel modules is allocated from. 
Because of this, the detour buffer is "not too far" from the code of the 
target module (reachable by 'jmp near rel32' and OK for rip-relative 
addressing).

The code of the target is then copied to that buffer, function by function, 
with probably some gaps inbetween. The copies are properly fixed up, taking 
jumps, calls and rip-relative addressing into account.

At last, a jump to the appropriate copied function is placed at the 
beginning of each original function. 

Then the target module is allowed to execute. It should work as usual.

The debugging facilities are demonstrated here too. For example, uncomment
the debug output code in instrument_function() (functions.c) to make 
"kedr_sample" output the code of the original and the "instrumented" 
functions, instruction by instruction, in hex.

[NB] The module notifier is given priority of (-1) in this example to make 
sure ftrace does its modifications to the code of the module earlier and 
thus to avoid conflicts. 

See the comments in the code for details.

Tested on:

Ubuntu 11.04 x86-64, kernel 2.6.38
Fedora 14 x86-64, kernel 2.6.35
OpenSUSE 11.3 x86, vanilla kernel 2.6.39-rc4
Debian 6 x86, kernel 2.6.32
OpenSUSE 11.3 x86, kernel 2.6.37
RHEL6 x86-64, kernel 2.6.32
Mandriva 2010 x86, kernel 2.6.31
OpenSUSE 11.2 x86, kernel 2.6.31




