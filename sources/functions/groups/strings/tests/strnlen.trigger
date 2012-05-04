[group]
# Name of the target function
function.name = strnlen
	
# The code to trigger a call to this function.
trigger.code =>>
	char *bytes = NULL;
	const char *str = "A quick brown fox jumps over a lazy dog.";
	bytes = kzalloc(20, GFP_KERNEL);
	if (bytes != NULL) {
		size_t ret;
		memcpy(&bytes[0], str, 15); 
		ret = strnlen(&bytes[0], 10 * value_one);
		bytes[0] = (char)ret;
		kfree(bytes);
	}
<<
#######################################################################
