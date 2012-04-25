[group]
function.name = mutex_unlock
trigger.code =>>
	DEFINE_MUTEX(m);
	mutex_lock(&m);
	mutex_unlock(&m);
<<
