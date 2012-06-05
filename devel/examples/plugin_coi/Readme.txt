This example demonstrates how a plugin to the function handling subsystem
can be used.

In this case, the plugin allows to use KEDR-COI 
(http://code.google.com/p/kedr-callback-operations-interception/) 
with KernelStrider to establish several kinds of happens-before arcs and 
this way, to reduce the number of false positives reported by the offline 
race detectors.
============================================================================

Prerequisites:

- KernelStrider, should be installed to the default location (/usr/local).

- Some output system for the data collected by the KernelStrider's core, 
for example, the primitive output system ("kedr_test_reporter") used in the 
tests for KernelStrider:
<build_dir_for_KernelStrider>/core/tests/reporter/kedr_test_reporter.ko

- Header files provided by KernelStrider should be available in 
/usr/local/include/kedr/ and its appropriate subdirectories.

- KEDR-COI, should be installed to the default location (/usr/local).

- "kedr-gen" tool from KEDR or KEDR_COI, should be available as 
/usr/local/lib/kedr/kedr_gen.
============================================================================

Building the example:
	make
============================================================================

Usage (unless specifically stated, the steps below require root privileges):

1. 
Load necessary components: KernelStrider core ("kedr_mem_core"), KEDR-COI 
core ("kedr_coi"), the function handling subsystem ("kedr_func_drd") and 
the plugin ("kedr_drd_plugin_coi"), the output system 
("kedr_test_reporter").

	insmod /usr/local/lib/modules/`uname -r`/misc/kedr_mem_core.ko \
		target_name=<name_of_the_target_module>
	insmod /usr/local/lib/modules/`uname -r`/misc/kedr_coi.ko
	insmod /usr/local/lib/modules/`uname -r`/misc/kedr_func_drd.ko
	insmod kedr_drd_plugin_coi.ko
	insmod <build_dir_for_KernelStrider>/core/tests/reporter/kedr_test_reporter.ko

Note that the "reporter" can output only a limited number of events (65536 
by default). You can use 'max_events' parameter of that module to change 
this limit.

2.
Load the target module, do something with it and then unload it.

[NB] You may also want to record the start addresses of the target module's 
sections while it is loaded. This could help make the race detector reports 
more readable.

3. 
Save the raw trace (assuming debugfs is mounted to /sys/kernel/debug):
	cat /sys/kernel/debug/kedr_test_reporter/output > raw_trace.txt

4. Now the trace can be converted to a format supported by the chosen data 
race detector and then analyzed.

5. 
If needed, repeat steps 2-4. For example, you might want to do some other 
operations with the target module to trigger execution of more code paths 
in it and check if there are data races there.

6. 
Unload the components:
	rmmod kedr_test_reporter kedr_drd_plugin_coi kedr_func_drd kedr_coi \
		kedr_mem_core.ko
============================================================================
