[group]
function.name = _atomic_dec_and_lock
trigger.code =>>
	atomic_t cnt = ATOMIC_INIT(1);
	DEFINE_SPINLOCK(s);
	if (atomic_dec_and_lock(&cnt, &s))
		spin_unlock(&s);
<<
