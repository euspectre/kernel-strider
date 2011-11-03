This example demonstrates how to obtain the names and addresses of the 
sections of a loaded kernel module. As it seems to be impractical to rely 
on the internal definitions of module-related and sysfs-related structures, 
the example does the following trick. It uses user-mode helper API 
(call_usermodehelper(), see linux/kmod.h) to launch a script that collects 
all necessary information from sysfs in the user space and then passes it 
back to the kernel via a file in debugfs.

In this example, the information about sections is simply dumped to the 
system log, just to demonstrate that it has been obtained.

The kernel part of this example expects the helper script 
(kedr_get_sections.sh) to be located in /usr/local/bin/

To build the kernel part of this example, simply run 'make'.

Usage:
   insmod kedr_foo.ko target_name="<specify_target_name_here>" 

Make sure the target module is loaded before the above command is executed.

Example (run as root):
   cp kedr_get_sections.sh /usr/local/bin/
   insmod kedr_foo.ko target_name="e1000" 
   # see what has been output to the system log:
   dmesg | less
   rmmod kedr_foo

Note that validation of the obtained data is omitted here. In a production 
quality system, it should be present (see TODOs in the code for details). 

[NB] "kedr_" substring occurs in many places in this example. It just means 
that we intend to use this functionality in KEDR framework some time in the 
future.
  