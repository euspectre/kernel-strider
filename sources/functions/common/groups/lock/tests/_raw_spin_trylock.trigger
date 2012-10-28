[group]
function.name = _raw_spin_trylock
trigger.code =>>
	DEFINE_SPINLOCK(s);
	if (spin_trylock(&s))
		spin_unlock(&s);
<<
