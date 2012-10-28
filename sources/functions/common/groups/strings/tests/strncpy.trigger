[group]
# Name of the target function
function.name = strncpy
	
# The code to trigger a call to this function.
trigger.code =>>
	char *bytes = NULL;
	const char *str = "A qui";
	bytes = kmalloc(20, GFP_KERNEL);
	if (bytes != NULL) {
		strncpy(&bytes[0], str, 10 * value_one);
		kfree(bytes);
	}
<<
#######################################################################
