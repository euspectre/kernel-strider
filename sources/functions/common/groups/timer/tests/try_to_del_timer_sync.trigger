[group]
# Name of the target function
function.name = try_to_del_timer_sync
	
# The code to trigger a call to this function.
trigger.code =>>
	struct timer_list timer;
	setup_timer(&timer, test_timer_fn, 0);
	timer.expires = 0;
	
	mod_timer(&timer, jiffies + msecs_to_jiffies(test_timeout_msec));
	msleep(wait_timeout_msec);
	
	if (try_to_del_timer_sync(&timer) < 0)
		del_timer_sync(&timer); /* to stop the timer for sure. */
<<
#######################################################################
