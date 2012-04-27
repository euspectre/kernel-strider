[group]
function.name = _read_unlock
trigger.code =>>
	DEFINE_RWLOCK(lock);
	read_lock(&lock);
	read_unlock(&lock);
<<
