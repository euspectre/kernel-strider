[group]
# Name of the target function
function.name = strcat
	
# The code to trigger a call to this function.
trigger.code =>>
	char *bytes = NULL;
	const char *str = "A quick b";
	
	bytes = kzalloc(20, GFP_KERNEL);
		
	if (bytes != NULL) {
		bytes[0] = 'a';
		bytes[1] = 'b';
		strcat(bytes, str);
		kfree(bytes);
	}
<<
#######################################################################
