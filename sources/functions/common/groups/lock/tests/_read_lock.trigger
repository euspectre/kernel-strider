[group]
function.name = _read_lock
trigger.code =>>
	DEFINE_RWLOCK(lock);
	read_lock(&lock);
	read_unlock(&lock);
<<
