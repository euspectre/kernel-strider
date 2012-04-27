[group]
function.name = _raw_read_unlock
trigger.code =>>
	DEFINE_RWLOCK(lock);
	read_lock(&lock);
	read_unlock(&lock);
<<
