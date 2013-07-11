[group]
# Name of the target function
function.name = wait_for_completion_interruptible_timeout
	
# The code to trigger a call to this function.
trigger.code =>>
	struct completion compl;
	struct timer_list timer;
	long ret;

	init_completion(&compl);

	setup_timer(&timer, timer_fn_complete, (unsigned long)&compl);
	timer.expires = jiffies + msecs_to_jiffies(test_timeout_msec);
	
	add_timer(&timer);
	msleep(wait_timeout_msec);
	
	ret = (long)wait_for_completion_interruptible_timeout(
		&compl, msecs_to_jiffies(wait_timeout_msec));
	if (ret <= 0) {
		pr_info("test: "
		"wait_for_completion_interruptible_timeout() failed");
	}

	del_timer_sync(&timer); /* just in case */
<<
#######################################################################
