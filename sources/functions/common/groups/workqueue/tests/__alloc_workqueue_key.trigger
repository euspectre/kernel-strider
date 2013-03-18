[group]
# Name of the target function
function.name = __alloc_workqueue_key
	
# The code to trigger a call to this function.
trigger.code =>>
	struct workqueue_struct *wq_normal;
	struct workqueue_struct *wq_normal2;
	struct workqueue_struct *wq_ordered;
	DECLARE_WORK(w1, work_func1);
	DECLARE_WORK(w2, work_func2);

	wq_normal = create_workqueue("test_wq_norm");
	if (wq_normal == NULL) {
		pr_info("Test: failed to create wq_normal.\n");
		return;
	}

	wq_ordered = create_singlethread_workqueue("test_wq_ordered");
	if (wq_ordered == NULL) {
		pr_info("Test: failed to create wq_ordered.\n");
		destroy_workqueue(wq_normal);
		return;
	}

	queue_work(wq_normal, &w1);
	msleep(wait_timeout_msec);

	queue_work(wq_ordered, &w2);
	msleep(wait_timeout_msec);
	
	destroy_workqueue(wq_ordered);
	
	wq_normal2 = create_workqueue("test_wq_norm2");
	if (wq_normal2 == NULL) {
		pr_info("Test: failed to create wq_normal2.\n");
		destroy_workqueue(wq_normal);
		return;
	}
	
	queue_work(wq_normal2, &w2);
	msleep(wait_timeout_msec);
	
	destroy_workqueue(wq_normal2);
	destroy_workqueue(wq_normal);
<<
#######################################################################
