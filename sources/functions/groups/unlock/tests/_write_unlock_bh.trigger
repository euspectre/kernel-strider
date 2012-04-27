[group]
function.name = _write_unlock_bh
trigger.code =>>
	DEFINE_RWLOCK(lock);
	write_lock_bh(&lock);
	write_unlock_bh(&lock);
<<
