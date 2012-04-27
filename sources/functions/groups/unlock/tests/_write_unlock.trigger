[group]
function.name = _write_unlock
trigger.code =>>
	DEFINE_RWLOCK(lock);
	write_lock(&lock);
	write_unlock(&lock);
<<
