A simple data output system
---------------------------

This output subsystem ("simple_trace_recorder") is less powerful than the 
full-fledged one (see sources/output) but still can be handy. 

"simple_trace_recorder" allows to save the trace collected by KernelStrider 
to a binary file on the same machine. A special converter which is also 
provided here, can be used to prepare the trace in the format required by 
ThreadSanitizer offline from that file.

The subsystem consists of two main parts:
- kernel-space part: stored the trace from the KernelStrider core in a 
memory buffer that can be mmapped to the user space;
- user-space part: mmaps the buffer and retrieves the data from the buffer 
and retrieves the data from it waiting until the data become available if 
needed.

The following parameters can be set for the kernel part of the subsystem 
(the parameters of "kedr_simple_trace_recorder" module):

- nr_data_pages 
Number of memory pages in the ring buffer used to transfer data from the 
kernel space to the use space. Must be a power of 2. Must be less than or 
equal to 65536.

If the user-space part of the subsystem retrieving the data from the buffer 
cannot keep up with the kernel-space part writing the data there, the 
information about the newer events in the target module will be lost. You 
can read the current count of such events from 
"kedr_simple_trace_recorder/events_lost" in debugfs.

If some events are lost, you can try increasing the size of the ring buffer 
via "nr_data_pages" parameter.

For the workloads where the stream of events from the target module is not 
very intensive, the default value of "nr_data_pages" should be acceptable.

- notify_mark
For each "notify_mark" data pages filled in the buffer, the kernel-space 
part wakes up the user-space part of the subsystem waiting for the data to 
become available for reading. The default value should be OK in most cases.

- no_call_events
0 by default. If non-zero, function entry/exit and call pre/post events 
will not be recorded in the trace. 
This can be used to reduce the intensity of the event stream and the size
of the trace if there are much more such events than memory and
synchronization events. 

Note that call-related events are often used to maintain the call stack 
information. If recording of such events is disabled, that information 
will not be available, only the address of the instruction that generated
the event will be in the trace. */
============================================================================

Prerequisites:
- KernelStrider installed to the default location (/usr/local)
- Debugfs mounted to /sys/kernel/debug
============================================================================

Common use case
---------------

Here is how the output subsystem (and KernelStrider in general) can be used 
in conjunction with ThreadSanitizer (TSan) offline version 1.x. 

Replace "<name_of_the_target_module>" below with the name of the kernel 
module you are going to analyze.

[NB] TSan offline should be built and installed on your system, of course. 
Its executable ("x86-linux-debug-ts_offline", or whatever name it has on 
your system) is expected to be in $PATH.

The operations with the kernel-space part of our instrumentation and data 
collection system require root privileges.

1.
Load the core components of KernelStrider:
 
 # insmod /usr/local/lib/modules/`uname -r`/misc/kedr_mem_core.ko \
   targets=<name_of_the_target_module>
 # insmod /usr/local/lib/modules/`uname -r`/misc/kedr_func_drd.ko 

2. 
Load the kernel-space part of the output subsystem:

 # insmod /usr/local/lib/modules/`uname -r`/misc/kedr_simple_trace_recorder.ko 

If you would like to set the size of the buffer used for data transfer to, 
say, 4096 memory pages (16Mb buffer on the systems with 4Kb pages), you can 
load this part as follows instead:

 # insmod /usr/local/lib/modules/`uname -r`/misc/kedr_simple_trace_recorder.ko \
   nr_data_pages=4096

3. 
Load the user-space part of the system, specifying the path to the trace 
file as an argument:

 # /usr/local/bin/kedr_st_recorder trace.dat &

4. 
Load the target module.

5. 
The kernel modules do not have fixed load addresses. So it is recommended 
to obtain the memory addresses at least for the loaded sections of the 
target module now:

 # /usr/local/bin/kedr_show_target_sections.sh \
   <name_of_the_target_module> > sections.dat

The information collected at this step will be used later to properly 
resolve the addresses of the instructions in the report produced by TSan.

If you like, you could save more detailed information instead, not only for 
the sections but also for the symbols in the target module:

 # /usr/local/bin/kedr_show_target_symbols.sh \
   <name_of_the_target_module> > symbols.dat

6.
<do something with the target module, then unload it>

kedr_st_recorder process should stop automatically when the target is 
unloaded. If it fails to stop for some reason, send SIGINT or SIGTERM to it
('kill -INT <PID>' or 'kill <PID>').

7. 
Check if the output system has been able to save information for all events:

 # cat /sys/kernel/debug/kedr_simple_trace_recorder/events_lost

If "events_lost" contains a non-zero value, you would probably want to 
unload the target module and retry from the step 2 with a greater value of 
"nr_data_pages" parameter.

8.
If you no longer need the kernel part of our instrumentation and data 
collection system, you can unload it now or later:
 # rmmod kedr_simple_trace_recorder kedr_func_drd kedr_mem_core

The operations described below do not need root privileges.

9. 
"trace.dat" should now contain the collected trace (in a binary format).
You can use a special converter provided with KernelStrider to produce a 
trace in TSan format from that file:

 $ /usr/local/bin/kedr_convert_trace_to_tsan trace.dat > trace_tsan.tst

10.
Let's now use ThreadSanitizer offline to analyze the trace:

 $ x86-linux-debug-ts_offline  \
   --show-pc < trace_tsan.tst 2> tsan_report_raw.txt

[NB] On x86-64, the TSan executable can be named 
"amd64-linux-debug-ts_offline" or something like that.

tsan_report_raw.txt will now contain information about the potential data 
races found by TSan. At this point, only the raw memory addresses of the 
relevant locations in the code are shown there, for example:

---------------------------
WARNING: Possible data race during read of size 4 at 0xc4fc3500: {{{
   T3 (L{}):
    #0  0xf7e4929a:  
  Concurrent write(s) happened at (OR AFTER) these points:
   T2 (L{}):
    #0  0xf7e492bf:  
  Location 0xc4fc3500 is 0 bytes inside a block starting at 0xc4fc3500 
  of size 184 allocated by T1 from heap:
    #0  0xf7e240ae:  
    #1  0xf7e240ae:  
   Race verifier data: 0xf7e4929a,0xf7e492bf
}}}
---------------------------

[NB] The top stack frame of the allocation event is mentioned twice due to 
a quirk of the trace collection system. This duplication does not matter.

11. 
Analyzing the report as it is now can be a pain (especially due to the fact 
that the kernel modules obviously do not have a fixed load address). To make
things easier, you can use the information about the module's sections 
collected at the step 5 to resolve the addresses:

 $ kedr_symbolize_tsan_report \
   tsan_report_raw.txt sections.dat > tsan_report.txt

The relevant portion of "tsan_report.txt":

---------------------------
WARNING: Possible data race during read of size 4 at 0xc4fc3500: {{{
   T3 (L{}):
    #0  .text+0x29a (0xf7e4929a):  
  Concurrent write(s) happened at (OR AFTER) these points:
   T2 (L{}):
    #0  .text+0x2bf (0xf7e492bf):  
  Location 0xc4fc3500 is 0 bytes inside a block starting at 0xc4fc3500 
  of size 184 allocated by T1 from heap:
    #0  .init.text+0xae (0xf7e240ae):  
    #1  .init.text+0xae (0xf7e240ae):  
   Race verifier data: 0xf7e4929a,0xf7e492bf
}}}
---------------------------

Now that you have a section name and the offset in the section for each code
address of interest, you will be able to analyze what actually happened in 
the target module. "objdump" and similar tools may help here.

If the target module was built with debug information, you can also use 
"objdump", "addr2line" or some other similar tool to find the relevant 
positions in the source code of the target module.

Instead of section information, you may use more detailed symbol information
(also obtained earlier) to resolve the addresses in the report:

 $ kedr_symbolize_tsan_report \
   tsan_report_raw.txt symbols.dat > tsan_report.txt

The relevant portion of "tsan_report.txt" would look like this in this case:

---------------------------
WARNING: Possible data race during read of size 4 at 0xc4fc3500: {{{
   T3 (L{}):
    #0  cfake_open+0x3a (0xf7e4929a):  
  Concurrent write(s) happened at (OR AFTER) these points:
   T2 (L{}):
    #0  cfake_open+0x5f (0xf7e492bf):  
  Location 0xc4fc3500 is 0 bytes inside a block starting at 0xc4fc3500 
  of size 184 allocated by T1 from heap:
    #0  .init.text+0xae (0xf7e240ae):  
    #1  .init.text+0xae (0xf7e240ae):  
   Race verifier data: 0xf7e4929a,0xf7e492bf
}}}
---------------------------

[NB] The symbols from .init.text have not been resolved because they are not
listed in the kernel's symbol table after the target module has completed 
its initialization.

12.
That's it.
============================================================================
