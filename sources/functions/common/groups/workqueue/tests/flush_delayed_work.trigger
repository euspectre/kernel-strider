[group]
# Name of the target function
function.name = flush_delayed_work
	
# The code to trigger a call to this function.
trigger.code =>>
	struct workqueue_struct *wq;
	DECLARE_DELAYED_WORK(dw1, work_func1);

	wq = create_workqueue("test_wq");
	if (wq == NULL) {
		pr_info("Test: failed to create wq.\n");
		return;
	}
	
	queue_delayed_work(wq, &dw1, msecs_to_jiffies(test_timeout_msec));
	msleep(wait_timeout_msec);
	flush_delayed_work(&dw1);
	
	flush_workqueue(wq);
	destroy_workqueue(wq);
<<
#######################################################################
