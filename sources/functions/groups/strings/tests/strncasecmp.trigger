[group]
# Name of the target function
function.name = strncasecmp
	
# The code to trigger a call to this function.
trigger.code =>>
	char *bytes = NULL;
	const char *str = "A quick brown fox jumps over a lazy dog.";
	const char *s = "A QuiCk brown";
	bytes = kmalloc(20, GFP_KERNEL);
	if (bytes != NULL) {
		int ret;
		memcpy(&bytes[0], str, 15); 
		ret = strncasecmp(&bytes[0], s, 10 * value_one);
		bytes[0] = (char)ret;
		kfree(bytes);
	}
<<
#######################################################################
