[group]
function.name = _write_unlock_irqrestore
trigger.code =>>
	DEFINE_RWLOCK(lock);
	unsigned long flags;
	write_lock_irqsave(&lock, flags);
	write_unlock_irqrestore(&lock, flags);
<<
