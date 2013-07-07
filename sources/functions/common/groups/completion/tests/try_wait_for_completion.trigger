[group]
# Name of the target function
function.name = try_wait_for_completion
	
# The code to trigger a call to this function.
trigger.code =>>
	struct completion compl;
	struct timer_list timer;
	int ret;

	init_completion(&compl);

	setup_timer(&timer, timer_fn_complete, (unsigned long)&compl);
	timer.expires = jiffies + msecs_to_jiffies(test_timeout_msec);
	
	add_timer(&timer);
	msleep(wait_timeout_msec);
	
	ret = (int)try_wait_for_completion(&compl);
	if (ret == 0)
		pr_info("tests: try_wait_for_completion() failed.");

	del_timer_sync(&timer); /* just in case */
<<
#######################################################################
