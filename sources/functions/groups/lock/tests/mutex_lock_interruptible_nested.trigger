[group]
function.name = mutex_lock_interruptible_nested
trigger.code =>>
	DEFINE_MUTEX(m);
	if (mutex_lock_interruptible(&m) == 0)
		mutex_unlock(&m);
<<
