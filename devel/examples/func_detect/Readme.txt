This example demonstrates how to obtain information (name, start address 
and size estimation) about the functions in the target module after it has 
been loaded into memory.

Usage:
  insmod kedr_sample.ko target_name=<specify_target_name_here>

kedr_sample.ko should be loaded before the target module and will be 
notified when the target is loaded and is about to begin its initialization 
(similar to KEDR). 

When this happens, kedr_sample.ko will collect appropriate data and output 
them to the system log (tagged with "[sample]" string).
