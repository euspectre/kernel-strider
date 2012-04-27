[group]
function.name = _write_lock
trigger.code =>>
	DEFINE_RWLOCK(lock);
	write_lock(&lock);
	write_unlock(&lock);
<<
