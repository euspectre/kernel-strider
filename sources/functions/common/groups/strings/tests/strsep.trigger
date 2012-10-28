[group]
# Name of the target function
function.name = strsep
	
# The code to trigger a call to this function.
trigger.code =>>
	char *bytes = NULL;
	const char *str = "A quick brown fox jumps over a lazy dog.";
	
	bytes = kzalloc(20, GFP_KERNEL);
	if (bytes != NULL) {
		char *p;
		char *s;
		s = bytes;
		memcpy(&bytes[0], str, 18);
		p = strsep(&s, "oldr");
		if (p != NULL && s != NULL)
			bytes[0] = *p;
		kfree(bytes);
	}
<<
#######################################################################
