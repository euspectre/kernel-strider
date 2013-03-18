[group]
# Name of the target function
function.name = schedule_work_on
	
# The code to trigger a call to this function.
trigger.code =>>
	DECLARE_WORK(w1, work_func1);

	schedule_work_on(0, &w1);
	msleep(wait_timeout_msec);
	
	flush_scheduled_work();
<<
#######################################################################
