[group]
function.name = _read_unlock_irqrestore
trigger.code =>>
	DEFINE_RWLOCK(lock);
	unsigned long flags;
	read_lock_irqsave(&lock, flags);
	read_unlock_irqrestore(&lock, flags);
<<
