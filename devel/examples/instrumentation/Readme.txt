This example demonstrates instrumentation of memory read/write operations 
as well as of the indirect calls and jumps in a kernel module. 

[!] TODO: describe details of what the example does and how it works.

Usage:

1. Copy kedr_get_sections.sh to /usr/local/bin or make a symlink for it 
there.

2. Make sure the kernel module to be processed ("target module") is not 
loaded.

3. Execute the following command, specify the name of the target module as 
the value of 'target_name' parameter: 
  insmod kedr_sample.ko target_name="<specify_target_name_here>"

4. Load the target module. You can do this directly via insmod or modprobe,
you can plug in a device if the module is a driver for that device, etc. 
Anyway, do something to have the target module loaded.

5. Work with the target kernel module as usual. If it is a device driver, 
you can make a request to the device it services; if it is a file system 
module, you can mount a partition formatted to that file system and do 
something to the files and directories on it, etc.

6. See the messages marked with "[sample]" in the system log and the 
information in /sys/kernel/debug/kedr_sample/output.

7. Unload the target module.

8. Execute the following command to unload the kernel-mode part of the 
example:
  rmmod kedr_sample
