[group]
function.name = _raw_spin_unlock
trigger.code =>>
	DEFINE_SPINLOCK(lock);
	spin_lock(&lock);
	spin_unlock(&lock);
<<
