[group]
function.name = _spin_trylock
trigger.code =>>
	DEFINE_SPINLOCK(s);
	if (spin_trylock(&s))
		spin_unlock(&s);
<<
