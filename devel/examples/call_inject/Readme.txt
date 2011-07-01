This example demonstrates the "injection" of a custom function call into 
the beginning of the each function in the target kernel module. The 
injection is performed at the level of binary code after the target kernel 
module is loaded but before it begins its initialization.

Usage:
  insmod kedr_sample.ko target_name=<specify_target_name_here>

The example is based on "func_detour" example. The execution of the 
functions is detoured (only for the functions with binary code 5 or more 
bytes long), the instrumented functions are created.

The call to a special wrapper of kedr_get_primary_storage() function is 
placed at the beginning of each instrumented function. See the comments for 
kedr_ps*() functions and KEDR_* macros in functions.c for details.

The "holder-wrapper" technique along with other useful tricks were inspired 
by the implementation of kretprobes on x86, see arch/x86/kernel/kprobes.c 
in the kernel sources.

Note 1.
Indirect jumps are not processed in this example. Such jumps may 
take the execution back to the original function from its instrumented 
counterpart, e.g. the jumps via tables of pre-defined destination addresses 
like those used in optimization of 'switch' statements can do so. In this 
example, this probably does no harm as long as there are no jumps in the 
original function to its first 5 bytes, the only place in that function 
that we change. 

Nevertheless, in a full-fledged system, such conditions should be taken 
into account. Other kinds of indirect jumps should probably be handled as 
well.

Note 2.
The handling of short jumps leading outside of the function 
is quite fragile here. It relies on the assumption that there are no more 
instuctions in the function that need to be fixed if we replace that short 
jump with a longer instruction. For the short jumps we have seen so far, 
this is not a problem. If they lead outside of the function, they are 
probably located at the end of the function (such jumps arise presumably 
during tail call optimization). But we cannot guarantee that this is always
the case.

Note 3. 
The access to 'call_no' variable in kedr_get_primary_storage() is not 
serialized. The data races on 'call_no' are possible but it is not very 
significant in this example.
