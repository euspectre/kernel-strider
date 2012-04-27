[group]
function.name = _raw_write_lock
trigger.code =>>
	DEFINE_RWLOCK(lock);
	write_lock(&lock);
	write_unlock(&lock);
<<
