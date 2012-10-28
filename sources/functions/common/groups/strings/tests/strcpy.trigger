[group]
# Name of the target function
function.name = strcpy
	
# The code to trigger a call to this function.
trigger.code =>>
	char *bytes = NULL;
	char *from = NULL;
	const char *str = "A quick brown fox jumps over a lazy dog.";
	
	bytes = kzalloc(20, GFP_KERNEL);
	from = kzalloc(20, GFP_KERNEL);
	
	if (bytes != NULL && from != NULL) {
		memcpy(&from[0], str, 9);
		strcpy(&bytes[0], from);
	}

	kfree(bytes);
	kfree(from);
<<
#######################################################################
