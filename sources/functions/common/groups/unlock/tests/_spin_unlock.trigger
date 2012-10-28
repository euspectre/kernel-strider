[group]
function.name = _spin_unlock
trigger.code =>>
	DEFINE_SPINLOCK(lock);
	spin_lock(&lock);
	spin_unlock(&lock);
<<
