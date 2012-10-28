[group]
function.name = atomic_dec_and_mutex_lock
trigger.code =>>
	atomic_t cnt = ATOMIC_INIT(1);
	DEFINE_MUTEX(m);
	if (atomic_dec_and_mutex_lock(&cnt, &m))
		mutex_unlock(&m);
<<
