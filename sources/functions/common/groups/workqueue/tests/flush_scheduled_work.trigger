[group]
# Name of the target function
function.name = flush_scheduled_work
	
# The code to trigger a call to this function.
trigger.code =>>
	DECLARE_WORK(w1, work_func1);

	schedule_work(&w1);
	msleep(wait_timeout_msec);
	
	flush_scheduled_work();
<<
#######################################################################
