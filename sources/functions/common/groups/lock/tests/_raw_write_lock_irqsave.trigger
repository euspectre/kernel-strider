[group]
function.name = _raw_write_lock_irqsave
trigger.code =>>
	DEFINE_RWLOCK(lock);
	unsigned long flags;
	write_lock_irqsave(&lock, flags);
	write_unlock_irqrestore(&lock, flags);
<<
