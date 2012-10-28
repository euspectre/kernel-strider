[group]
# Name of the target function
function.name = memset
	
# The code to trigger a call to this function.
trigger.code =>>
	char *bytes = NULL;
	bytes = kmalloc(20, GFP_KERNEL);
	if (bytes != NULL) {
		memset(&bytes[0], 0xcc * value_one, 10 * value_one);
		kfree(bytes);
	}
<<
#######################################################################
