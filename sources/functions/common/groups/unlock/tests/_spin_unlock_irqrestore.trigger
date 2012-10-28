[group]
function.name = _spin_unlock_irqrestore
trigger.code =>>
	DEFINE_SPINLOCK(lock);
	unsigned long flags;
	spin_lock_irqsave(&lock, flags);
	spin_unlock_irqrestore(&lock, flags);
<<
