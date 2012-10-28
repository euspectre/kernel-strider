[group]
function.name = _write_unlock_irq
trigger.code =>>
	DEFINE_RWLOCK(lock);
	write_lock_irq(&lock);
	write_unlock_irq(&lock);
<<
