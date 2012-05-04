[group]
# Name of the target function
function.name = strpbrk
	
# The code to trigger a call to this function.
trigger.code =>>
	char *bytes = NULL;
	const char *str = "A quick brown fox jumps over a lazy dog.";
	
	bytes = kzalloc(20, GFP_KERNEL);
	if (bytes != NULL) {
		char *p;
		memcpy(&bytes[0], str, 9);
		p = strpbrk(&bytes[0], "oldr");
		if (p != NULL)
			bytes[0] = *p;
		kfree(bytes);
	}
<<
#######################################################################
