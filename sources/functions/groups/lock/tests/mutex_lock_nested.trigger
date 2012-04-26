[group]
function.name = mutex_lock_nested
trigger.code =>>
	DEFINE_MUTEX(m);
	mutex_lock(&m);
	mutex_unlock(&m);
<<
