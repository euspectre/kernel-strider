[group]
# Name of the target function
function.name = schedule_delayed_work
	
# The code to trigger a call to this function.
trigger.code =>>
	DECLARE_DELAYED_WORK(dw1, work_func1);

	schedule_delayed_work(&dw1, msecs_to_jiffies(test_timeout_msec));
	msleep(wait_timeout_msec);
	
	flush_scheduled_work();
<<
#######################################################################
