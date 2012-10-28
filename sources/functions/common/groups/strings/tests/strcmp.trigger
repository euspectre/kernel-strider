[group]
# Name of the target function
function.name = strcmp
	
# The code to trigger a call to this function.
trigger.code =>>
	char *bytes = NULL;
	const char *str = "A quick brown fox jumps over a lazy dog.";
	const char *s = "A quick b";
	bytes = kmalloc(20, GFP_KERNEL);
	if (bytes != NULL) {
		int ret;
		memcpy(&bytes[0], str, 15); 
		ret = strcmp(&bytes[0], s);
		bytes[0] = (char)ret;
		kfree(bytes);
	}
<<
#######################################################################
