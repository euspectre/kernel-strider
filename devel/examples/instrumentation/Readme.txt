This example demonstrates instrumentation of memory read/write operations 
as well as of the indirect calls and jumps in a kernel module. 

Our system (implemented as a kernel module, "kedr_sample") resides in the 
kernel space and waits for the given kernel module to load ("target 
module"). As soon as that module is loaded and probably processed by other 
kernel facilities like Ftrace, but before it begins its initialization, our 
system instruments the code of that module in memory. 

Technically, an instrumented instance is created for each function in the 
module (except those smaller than 5 bytes, the size of a near jump on x86) 
and placed in a specially allocated area in the module mapping space. For 
each of the corresponding original functions, a jump to its instrumented 
instance is placed at the beginning. The remaining code of the original 
will not execute (it will be filled with 'int 3' instructions just in case 
it does get called in some unexpected way).

In the instrumented code, everything is organized so as to record memory 
accesses from the most common machine instructions as well as function 
entry and exit events. The collected data are available in 
"kedr_sample/output" file in debugfs once the target module is unloaded (to 
simplify the implementation of this example, they are not visible when the 
target module is loaded).

The following kinds of events are recorded:
- function entry
- function exit
- read from memory
- write to memory
- locked update of memory - read and write from an instruction with LOCK 
prefix, typically representing an atomic operations.

Memory accesses are recorded for the following kinds of machine 
instructions:
- different kinds of MOV, XCHG, MOVSX, MOVZX, MOVBE;
- conditional set and mov: SETcc, CMOVcc;
- arithmetical and logical operations for integers (except MMX, SSE and 
the like, they are rarely used in the kernel anyway);
- bit operations;
- comparisons (CMP, CMPXCHG, CMPXCHG8B/16B);
- "string" operations: LODS, STOS, SCAS, INS, OUTS, MOVS, CMPS. 

Here is an extract from a sample output file:
-----------------------------------------
<...>
TID=0xf0b30c90 entry: addr=0xf86571cf ("cfake_open")
TID=0xf0b30c90 read at 0xf86571dc (cfake_open+0xd/0x9d [kedr_sample_target]): addr=0xf34bdc38, size=4
<...>
TID=0xf0b30c90 write at 0xf8657219 (cfake_open+0x4a/0x9d [kedr_sample_target]): addr=0xf0a90bb4, size=4
TID=0xf0b30c90 read at 0xf865721f (cfake_open+0x50/0x9d [kedr_sample_target]): addr=0xf34bdd40, size=4
TID=0xf0b30c90 read at 0xf865723c (cfake_open+0x6d/0x9d [kedr_sample_target]): addr=0xf0a0289c, size=4
TID=0xf0b30c90 read at 0xf8657241 (cfake_open+0x72/0x9d [kedr_sample_target]): addr=0xf0a028a0, size=4
TID=0xf0b30c90 entry: addr=0xf86571bb ("kzalloc.constprop.1")
TID=0xf0b30c90 exit: addr=0xf86571bb ("kzalloc.constprop.1")
TID=0xf0b30c90 write at 0xf865724b (cfake_open+0x7c/0x9d [kedr_sample_target]): addr=0xf0a0289c, size=4
TID=0xf0b30c90 exit: addr=0xf86571cf ("cfake_open")
TID=0xf0b30c90 entry: addr=0xf8657072 ("cfake_write")
TID=0xf0b30c90 read at 0xf8657085 (cfake_write+0x13/0xa6 [kedr_sample_target]): addr=0xf0a90bb4, size=4
TID=0xf0b30c90 write at 0xf8657088 (cfake_write+0x16/0xa6 [kedr_sample_target]): addr=0xf37cff54, size=4
TID=0xf0b30c90 write at 0xf8657090 (cfake_write+0x1e/0xa6 [kedr_sample_target]): addr=0xf37cff58, size=4
<...>
TID=0xf0b30c90 exit: addr=0xf8657072 ("cfake_write")
TID=0xf0b30c90 entry: addr=0xf8657000 ("cfake_release")
TID=0xf0b30c90 exit: addr=0xf8657000 ("cfake_release")
<...>
TID=0xeb830c90 entry: addr=0xf86572f0 ("cfake_exit_module")
TID=0xeb830c90 read at 0xf86572f0 (cfake_exit_module+0x0/0xf [kedr_sample_target]): addr=0xf86575b0, size=4
TID=0xeb830c90 entry: addr=0xf865726c ("cfake_cleanup_module")
TID=0xeb830c90 read at 0xf8657279 (cfake_cleanup_module+0xd/0x84 [kedr_sample_target]): addr=0xf865773c, size=4
TID=0xeb830c90 read at 0xf8657289 (cfake_cleanup_module+0x1d/0x84 [kedr_sample_target]): addr=0xf8657740, size=4
<...>
TID=0xeb830c90 exit: addr=0xf865726c ("cfake_cleanup_module")
TID=0xeb830c90 exit: addr=0xf86572f0 ("cfake_exit_module")
-----------------------------------------

Main fields:
TID  - thread ID - ID of the thread where the event occurred.
addr - for function entry and exit events, it is the address of the 
       function (the original function);
     - for memory access events, it is the address of the memory area 
       that was accessed.
size - the size of the accessed memory area.
pc   - ("program counter") address of the instruction (in the original 
       function) that performed that memory access (or, to be exact,
       that would perform that memory access if the code was not 
       instrumented).
resolved_PC - details about 'pc' resolved by kallsyms system: 
       <function>+<offset>/<size_of_function> [<module>]

Format of records for the function events:
   TID=<TID> {entry|exit}: addr=<addr> ("<name_of_the_function>")

Format of records for the memory events:
   TID=<TID> {read|write|locked update} at <pc> (<resolved_PC>): addr=<addr>, size=<size>

Note that, for demonstration purposes, only the first 512 events are 
recorded in detail in "kedr_sample/output". If you would like to increase 
this limit, you need to change the value of KEDR_DEMO_NUM_RECORDS in demo.c 
and rebuild this example.
=======================================================================

Building:

To build the system, just execute 'make' in its directory.
=======================================================================

Usage:

1. Copy kedr_get_sections.sh to /usr/local/bin or make a symlink for it 
there.

2. Make sure the kernel module to be processed ("target module") is not 
loaded.

3. Execute the following command, specify the name of the target module as 
the value of 'target_name' parameter: 
  insmod kedr_sample.ko target_name="<target_name_here>"

Note that if the name of the target module's file contains dashes (e.g. 
"my-module.ko"), you need to replace them with underscores in 
<target_name_here> ("my_module" in this case).

By default, memory accesses from the machine instructions with 
%esp/%rsp-based addressing are not recorded. If you would like them to be, 
set a non-zero value for "process_sp_accesses" parameter when inserting 
kedr_sample.ko

4. Load the target module. You can do this directly via insmod or modprobe,
you can plug in a device if the module is a driver for that device, etc. 
Anyway, do something to have the target module loaded.

5. Work with the target kernel module as usual. If it is a device driver, 
you can make a request to the device it services; if it is a file system 
module, you can mount a partition formatted to that file system and do 
something to the files and directories on it, etc.

6. Unload the target module.

7. After the target module has been unloaded, the information about the 
events that have occurred in it is available in 
/sys/kernel/debug/kedr_sample/output. 
We assume that debugfs is mounted to /sys/kernel/debug.

In addition to that, the system outputs diagnostic messages to the system 
log (the data concerning the size of the original and instrumented code, 
etc.), see the records marked with "[sample]".

8. Execute the following command to unload the kernel-mode part of the 
example:
  rmmod kedr_sample
=======================================================================
