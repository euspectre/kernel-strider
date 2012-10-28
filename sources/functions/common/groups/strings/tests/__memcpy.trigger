[group]
# Name of the target function
function.name = __memcpy
	
# The code to trigger a call to this function.
trigger.code =>>
	char *bytes = NULL;
	const char *str = "A quick brown fox jumps over a lazy dog.";
	bytes = kmalloc(20, GFP_KERNEL);
	if (bytes != NULL) {
		/* __memcpy() cannot be called directly. The chances are, 
		 * memcpy expands to it eventually. */
		memcpy(&bytes[0], str, 10 * value_one);
		kfree(bytes);
	}
<<
#######################################################################
