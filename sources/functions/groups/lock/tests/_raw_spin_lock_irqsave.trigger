[group]
function.name = _raw_spin_lock_irqsave
trigger.code =>>
    DEFINE_SPINLOCK(lock);
    unsigned long flags;
    spin_lock_irqsave(&lock, flags);
    spin_unlock_irqrestore(&lock, flags);
<<
