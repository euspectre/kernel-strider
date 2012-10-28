[group]
# Name of the target function
function.name = strrchr
	
# The code to trigger a call to this function.
trigger.code =>>
	char *bytes = NULL;
	const char *str = "A quick brown fox jumps over a lazy dog.";
	
	bytes = kzalloc(20, GFP_KERNEL);
	if (bytes != NULL) {
		void *p;
		memcpy(&bytes[0], str, 9);
		p = strrchr(&bytes[0], 0xcc);
		if (p != NULL)
			bytes[0] = *(char *)p;
		kfree(bytes);
	}
<<
#######################################################################
