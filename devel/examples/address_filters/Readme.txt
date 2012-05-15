An example to demonstrate how to determine if and address belongs to 
the stack or to the user-space memory. See is_stack_address() and 
is_user_space_address() in cfake.c for details.

Build:
	make

Usage:
	kedr_sample_target load
	echo "asdasdasda" > /dev/cfake0
	dmesg | less  # see the items marked with "[DBG]"
	kedr_sample_target unload
