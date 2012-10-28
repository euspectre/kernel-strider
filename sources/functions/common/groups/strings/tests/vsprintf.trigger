[group]
# Name of the target function
function.name = vsprintf

# Other definitions needed for the trigger function
trigger.add_before =>>
/* This is actually almost the same as the code of sprintf(). */
static int 
trigger_vsprintf_wrapper(char *buf, const char *fmt, ...)
{
	va_list args;
	int i;

	va_start(args, fmt);
	i = vsprintf(buf, fmt, args);
	va_end(args);

	return i;
}
<< 

# The code to trigger a call to this function.
trigger.code =>>
	char *bytes = NULL;
	char *buf = NULL;
	const char *str = "-16179-";
	
	bytes = kmalloc(20, GFP_KERNEL);
	buf = kmalloc(20, GFP_KERNEL);
	if (bytes != NULL) {
		strcpy(&bytes[0], str);
		trigger_vsprintf_wrapper(buf, "@%s@", bytes);
	}
	kfree(bytes);
	kfree(buf);
<<
#######################################################################
