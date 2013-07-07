[group]
# Name of the target function
function.name = wait_for_completion_io
	
# The code to trigger a call to this function.
trigger.code =>>
	struct completion compl;
	struct timer_list timer;

	init_completion(&compl);

	setup_timer(&timer, timer_fn_complete, (unsigned long)&compl);
	timer.expires = jiffies + msecs_to_jiffies(test_timeout_msec);
	
	add_timer(&timer);
	
	wait_for_completion_io(&compl);
	del_timer_sync(&timer); /* just in case */
<<
#######################################################################
