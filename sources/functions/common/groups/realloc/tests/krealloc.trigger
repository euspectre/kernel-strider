[group]
# Name of the target function
function.name = krealloc
	
# The code to trigger a call to this function.
trigger.code =>>
	size_t size = 20;
	void *p;
	p = __kmalloc(size, GFP_KERNEL);
	if (p != NULL) {
		void *p1;
		p1 = krealloc(p, ksize(p) + size, GFP_KERNEL);
		kfree(p1 != NULL ? p1 : p);
	}
<<
#######################################################################
