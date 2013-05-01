[group]
# Name of the target function
function.name = __tasklet_hi_schedule
	
# The code to trigger a call to this function.
trigger.code =>>
	struct tasklet_struct t;
	tasklet_init(&t, test_tasklet_fn, 0);
	
	tasklet_schedule(&t);
	msleep(wait_timeout_msec);
	
	tasklet_kill(&t);
<<
#######################################################################
