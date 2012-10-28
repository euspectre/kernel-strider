[group]
function.name = mutex_trylock
trigger.code =>>
	DEFINE_MUTEX(m);
	if (mutex_trylock(&m))
		mutex_unlock(&m);
<<
