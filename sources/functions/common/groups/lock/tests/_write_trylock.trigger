[group]
function.name = _write_trylock
trigger.code =>>
	DEFINE_RWLOCK(s);
	if (write_trylock(&s))
		write_unlock(&s);
<<
