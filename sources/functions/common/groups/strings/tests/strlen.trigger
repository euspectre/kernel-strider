[group]
# Name of the target function
function.name = strlen
	
# The code to trigger a call to this function.
trigger.code =>>
	char *bytes = NULL;
	const char *str = "A quick brown fox jumps over a lazy dog.";
	bytes = kzalloc(20, GFP_KERNEL);
	if (bytes != NULL) {
		size_t ret;
		memcpy(&bytes[0], str, 9); 
		ret = strlen(&bytes[0]);
		bytes[0] = (char)ret;
		kfree(bytes);
	}
<<
#######################################################################
