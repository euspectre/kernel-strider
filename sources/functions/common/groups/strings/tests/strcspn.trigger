[group]
# Name of the target function
function.name = strcspn
	
# The code to trigger a call to this function.
trigger.code =>>
	char *bytes = NULL;
	const char *str = "A quick brown fox jumps over a lazy dog.";
	
	bytes = kzalloc(20, GFP_KERNEL);
	if (bytes != NULL) {
		size_t pos;
		memcpy(&bytes[0], str, 9);
		pos = strcspn(&bytes[0], "oldr");
		bytes[0] = (char)pos;
		kfree(bytes);
	}
<<
#######################################################################
