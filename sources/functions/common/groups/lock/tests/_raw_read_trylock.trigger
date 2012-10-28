[group]
function.name = _raw_read_trylock
trigger.code =>>
	DEFINE_RWLOCK(s);
	if (read_trylock(&s))
		read_unlock(&s);
<<
