[group]
# Name of the target function
function.name = destroy_workqueue
	
# The code to trigger a call to this function.
trigger.code =>>
	struct workqueue_struct *wq;
	DECLARE_WORK(w1, work_func1);

	wq = create_workqueue("test_wq");
	if (wq == NULL) {
		pr_info("Test: failed to create wq.\n");
		return;
	}
	
	queue_work(wq, &w1);
	msleep(wait_timeout_msec);
	flush_work(&w1);
	
	flush_workqueue(wq);
	destroy_workqueue(wq);
<<
#######################################################################
