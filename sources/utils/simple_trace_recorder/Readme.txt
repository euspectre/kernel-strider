A simple data output system
---------------------------

This output subsystem ("simple_trace_recorder") is less powerful than the 
full-fledged one (see sources/output) but still can be handy. 

"simple_trace_recorder" allows to save the trace collected by KernelStrider 
to a binary file on the same machine.

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

- no_call_events
0 by default. If non-zero, function entry/exit and call pre/post events 
will not be recorded in the trace. 
This can be used to reduce the intensity of the event stream and the size
of the trace if there are much more such events than memory and
synchronization events. 

Note that call-related events are often used to maintain the call stack 
information. If recording of such events is disabled, that information 
will not be available, only the address of the instruction that generated
the event will be in the trace.
============================================================================

Prerequisites:
- KernelStrider installed to the default location (/usr/local)
- Debugfs mounted to /sys/kernel/debug
============================================================================
