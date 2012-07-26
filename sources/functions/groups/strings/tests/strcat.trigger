[group]
# Name of the target function
function.name = strcat
	
# The code to trigger a call to this function.
trigger.code =>>
	char *bytes = NULL;
	char *what = NULL;
	const char *str = "A quick b";
	
	bytes = kzalloc(20, GFP_KERNEL);
	what = kzalloc(20, GFP_KERNEL);
		
	if (bytes != NULL && what != NULL) {
		strcpy(what, str);
		bytes[0] = 'a';
		bytes[1] = 'b';
		what[value_one] = 'c';
		strcat(bytes, what);
	}
	kfree(bytes);
	kfree(what);
<<
#######################################################################
