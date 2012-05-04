[group]
# Name of the target function
function.name = memcmp
	
# The code to trigger a call to this function.
trigger.code =>>
	char *bytes = NULL;
	const char *str = "A quick brown fox jumps over a lazy dog.";
	bytes = kmalloc(20, GFP_KERNEL);
	if (bytes != NULL) {
		int ret;
		memcpy(&bytes[0], str, 15); 
		ret = memcmp(&bytes[0], str, 10 * value_one);
		bytes[0] = (char)ret;
		kfree(bytes);
	}
<<
#######################################################################
