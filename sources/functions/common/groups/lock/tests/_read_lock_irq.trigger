[group]
function.name = _read_lock_irq
trigger.code =>>
	DEFINE_RWLOCK(lock);
	read_lock_irq(&lock);
	read_unlock_irq(&lock);
<<
