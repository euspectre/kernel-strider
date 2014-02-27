Note. This output subsystem is not actively maintained at the moment.

1. 
If you would like to use it, this guide could be helpful. The example
considered here is the same as in the main tutorial (see doc/tutorial.txt).

The core components of KernelStrider are loaded in the usual way. kedr.py
helper script cannot be used here though, you need to load the modules
manually.

2. Collecting Data

(See output/user/README in KernelStrider sources for more details about
this output subsystem.)

2.1
Load the kernel-mode component of the output subsystem:
    # insmod /usr/local/lib/modules/`uname -r`/misc/kedr_trace_sender.ko

2.2
At this step, we will instruct the output system to prepare to record the
trace to /home/user/temp/trace/. It is recommended to start recording trace
to a clean directory, so remove /home/user/temp/trace if it exists before
executing the commands below.

Operations with "kedr_save_trace" do not require root privileges.

    $ /usr/local/bin/kedr_save_trace --start /home/user/temp/trace/
    $ /usr/local/bin/kedr_save_trace --init-session 127.0.0.1

Here we assume the data are collected and processed on the same machine, so
127.0.0.1 is used as an IP address of the machine where the target module
(as well as "kedr_trace_sender" module) operates. If the target is on a
different machine than "kedr_save_trace", we would need to specify the IP
address of that machine instead.

Note. If "kedr_save_trace" hangs, you can kill its process, unload
"kedr_trace_sender" module then load it once more and execute
"kedr_save_trace" again as described above.

The next 3 steps are the same as with the "simple" output subsystem.

2.3
Load the target module.

    # insmod /home/user/temp/buggy01/buggy01.ko

2.4
The kernel modules do not have fixed load addresses. So it is recommended
to obtain the memory addresses at least for the loaded sections of the
target module now, while it is loaded:

    # /usr/local/bin/kedr_show_target_sections.sh \
        buggy01 > /home/user/temp/sections.dat

The information collected at this step will be used later to properly
resolve the addresses of the instructions in the report produced by
ThreadSanitizer.

If you like, you could save more detailed information instead, not only for
the sections but also for the symbols in the target module:

    # /usr/local/bin/kedr_show_target_symbols.sh \
        buggy01 > /home/user/temp/symbols.dat

2.5
Do something with the target module then unload it.

    # /home/user/temp/test_buggy01/test_buggy01
    # rmmod buggy01

2.6
Stop recording the trace:

    $ /usr/local/bin/kedr_save_trace --break-session 127.0.0.1
    $ /usr/local/bin/kedr_save_trace --stop

2.7
If you would like to check some other operations with the target module and
record the corresponding trace, you can start from step 2.2 (probably
with a different path to save the trace to) and go on as described above.

2.8
When you have collected all the traces you wanted to, the kernel-space
components of KernelStrider can be unloaded.

    # rmmod kedr_trace_sender
    # rmmod kedr_fh_drd_common kedr_fh_drd_cdev
    # rmmod kedr_mem_core

2.9
Convert the saved trace to a format that ThreadSanitizer understands:
    $ /usr/local/bin/kedr_trace_converter_tsan \
        /home/user/temp/trace > /home/user/temp/trace_tsan.tst

trace_tsan.tst now contains the trace in the needed format.

2.10
Here and below we assume the current directory is /home/user/temp/.

Let us use ThreadSanitizer on the trace:
    $ ts_offline  \
        --show-pc < trace_tsan.tst 2> tsan_report_raw.txt

By default, ThreadSanitizer operates in the so called "pure happens-before"
mode. If you would like it to operate in the "hybrid" mode instead (more
predictable, allows to find more races but may also result in more false
positives), execute it as follows instead:

    $ ts_offline --hybrid=yes \
        --show-pc < trace_tsan.tst 2> tsan_report_raw.txt

The detailed description of these modes is available here:
http://code.google.com/p/data-race-test/wiki/PureHappensBeforeVsHybrid

2.11
Now tsan_report_raw.txt contains the information about the potential data
races found by ThreadSanitizer.

At this point, only the raw memory addresses of the relevant locations in
the code are shown there, for example:

------------------------------------
WARNING: Possible data race during read of size 4 at 0xffff88004b3dbda8: {{{
   T2 (L{}):
    #0  0x3ffffffa00f6060:
  Concurrent write(s) happened at (OR AFTER) these points:
   T1 (L{}):
    #0  0xffffffffa0005016:
  Location 0xffff88004b3dbda8 is 8 bytes inside a block
  starting at 0xffff88004b3dbda0 of size 16 allocated by T1 from heap:
    #0  0xffffffffa00f61d9:
    #1  0xffffffffa000500e:
   Race verifier data: 0x3ffffffa00f6060,0xffffffffa0005016
}}}
------------------------------------

Note. On x86-64, ThreadSanitizer uses the higher bits of the addresses for
its own purposes. As a result, some of the addresses in the raw report may
start with 0x3ff rather than 0xffff as they should. This is no problem
because the raw reports are rarely used by themselves. The converters you
will apply at the next steps will take care of these addresses
automatically.

2.12
It is not very convenient to analyze the raw addresses in the report. If
the target kernel module contains debugging information, you can use a tool
provided with KernelStrider to resolve the appropriate lines of the source
code. To do this, you need the information about the target's sections
(file "sections.dat" prepared on step 2.4) and the binary file
of the target module ("buggy.ko").

    $ /usr/local/bin/tsan_report_addr2line \
        /home/user/temp/buggy01/buggy01.ko sections.dat \
        < tsan_report_raw.txt > tsan_report.txt

Note. On some systems, the debug information for the stock kernel modules
is available in separate files rather than in the ".ko" files of the
modules themselves. When processing the reports for such modules with
tsan_report_addr2line, you can try to use the path to the file with the
debugging information for the module instead of the ".ko" file itself.

The relevant part of "tsan_report.txt" should now look like this:

------------------------------------
WARNING: Possible data race during read of size 4 at 0xffff88004b3dbda8: {{{
   T2 (L{}):
    #0  sample_open module.c:43:
  Concurrent write(s) happened at (OR AFTER) these points:
   T1 (L{}):
    #0  sample_init_module module.c:122:
  Location 0xffff88004b3dbda8 is 8 bytes inside a block
  starting at 0xffff88004b3dbda0 of size 16 allocated by T1 from heap:
    #0  kmalloc slab_def.h:161:
    #1  sample_init_module module.c:122:
   Race verifier data: 0x3ffffffa00f6060,0xffffffffa0005016
}}}
------------------------------------

Now that the relevant positions in the source code of "buggy01" have been
found, the report should be easier to analyze.

If the target module does not have debugging information, you still can use
the tools provided by KernelStrider to at least resolve section or symbol
names in the report. "sections.dat" and "symbols.dat" prepared during step
2.4 are needed here.

    $ /usr/local/bin/kedr_symbolize_tsan_report \
        tsan_report_raw.txt sections.dat > tsan_report_with_sections.txt

Here is the corresponding fragment of "tsan_report_with_sections.txt":

------------------------------------
WARNING: Possible data race during read of size 4 at 0xffff88004b3dbda8: {{{
   T2 (L{}):
    #0  .text+0x60 (0xffffffffa00f6060):
  Concurrent write(s) happened at (OR AFTER) these points:
   T1 (L{}):
    #0  .init.text+0x16 (0xffffffffa0005016):
  Location 0xffff88004b3dbda8 is 8 bytes inside a block
  starting at 0xffff88004b3dbda0 of size 16 allocated by T1 from heap:
    #0  .text.unlikely+0x9 (0xffffffffa00f61d9):
    #1  .init.text+0xe (0xffffffffa000500e):
   Race verifier data: 0x3ffffffa00f6060,0xffffffffa0005016
}}}
------------------------------------

You can now disassemble the target module (for example, with "objdump"
tool), look for the locations in the code mentioned in
"tsan_report_with_sections.txt" in the resulting assembly listing and
analyze what was going on. This can be less convenient than if debug
information were available but still much better than nothing.

If you would like to resolve symbol names instead of section names, just
use "symbols.dat" file instead of "sections.dat" in the command above.
