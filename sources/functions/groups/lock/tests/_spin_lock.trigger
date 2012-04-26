[group]
function.name = _spin_lock
trigger.code =>>
    DEFINE_SPINLOCK(lock);
    spin_lock(&lock);
    spin_unlock(&lock);
<<
