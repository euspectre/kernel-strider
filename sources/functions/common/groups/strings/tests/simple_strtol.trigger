[group]
# Name of the target function
function.name = simple_strtol
	
# The code to trigger a call to this function.
trigger.code =>>
	char *bytes = NULL;
	const char *str = "123456789 A quick brown fox";
	
	bytes = kzalloc(20, GFP_KERNEL);
	if (bytes != NULL) {
		long v;
		memcpy(&bytes[0], str, 15);
		v = simple_strtol(&bytes[0], NULL, 10);
		if (v == 0)
			bytes[0] = 0;
		kfree(bytes);
	}
<<
#######################################################################
