[group]
# Name of the target function
function.name = mod_timer
	
# The code to trigger a call to this function.
trigger.code =>>
	struct timer_list timer;
	setup_timer(&timer, test_timer_fn, 0);
	timer.expires = 0;
	
	mod_timer(&timer, jiffies + msecs_to_jiffies(test_timeout_msec));
	msleep(wait_timeout_msec);
	
	del_timer_sync(&timer);
<<
#######################################################################
