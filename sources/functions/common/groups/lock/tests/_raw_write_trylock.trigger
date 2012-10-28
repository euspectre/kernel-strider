[group]
function.name = _raw_write_trylock
trigger.code =>>
	DEFINE_RWLOCK(s);
	if (write_trylock(&s))
		write_unlock(&s);
<<
