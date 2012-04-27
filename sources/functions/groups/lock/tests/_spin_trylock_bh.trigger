[group]
function.name = _spin_trylock_bh
trigger.code =>>
	DEFINE_SPINLOCK(s);
	if (spin_trylock_bh(&s))
		spin_unlock_bh(&s);
<<
