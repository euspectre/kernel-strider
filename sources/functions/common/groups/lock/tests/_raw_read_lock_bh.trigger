[group]
function.name = _raw_read_lock_bh
trigger.code =>>
	DEFINE_RWLOCK(lock);
	read_lock_bh(&lock);
	read_unlock_bh(&lock);
<<
