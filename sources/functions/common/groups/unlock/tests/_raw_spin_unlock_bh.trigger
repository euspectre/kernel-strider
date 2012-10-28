[group]
function.name = _raw_spin_unlock_bh
trigger.code =>>
	DEFINE_SPINLOCK(lock);
	spin_lock_bh(&lock);
	spin_unlock_bh(&lock);
<<
