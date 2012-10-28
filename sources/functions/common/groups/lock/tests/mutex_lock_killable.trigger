[group]
function.name = mutex_lock_killable
trigger.code =>>
	DEFINE_MUTEX(m);
	if (mutex_lock_killable(&m) == 0)
		mutex_unlock(&m);
<<
