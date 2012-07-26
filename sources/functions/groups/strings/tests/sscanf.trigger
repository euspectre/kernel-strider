[group]
# Name of the target function
function.name = sscanf
	
# The code to trigger a call to this function.
trigger.code =>>
	char *bytes = NULL;
	const char *str = "1711 222 ";
	
	bytes = kmalloc(20, GFP_KERNEL);
	if (bytes != NULL) {
		int a;
		int b; 
		memcpy(&bytes[0], str, 15);
		if (sscanf(&bytes[0], "%d %d", &a, &b) != 2 || a != b)
			bytes[0] = 0;
		kfree(bytes);
	}
<<
#######################################################################
