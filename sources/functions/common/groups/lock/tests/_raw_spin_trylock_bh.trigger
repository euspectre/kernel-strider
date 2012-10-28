[group]
function.name = _raw_spin_trylock_bh
trigger.code =>>
	DEFINE_SPINLOCK(s);
	if (spin_trylock_bh(&s))
		spin_unlock_bh(&s);
<<
