[group]
# Name of the target function
function.name = strlcat
	
# The code to trigger a call to this function.
trigger.code =>>
	char *bytes = NULL;
	const char *str = "A quick brown fox jumps over a lazy dog.";
	
	bytes = kzalloc(20, GFP_KERNEL);
	if (bytes != NULL) {
		bytes[0] = 'a';
		bytes[1] = 'b';
		strlcat(bytes, str, 12 * value_one);
		kfree(bytes);
	}
<<
#######################################################################
