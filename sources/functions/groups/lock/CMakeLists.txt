set(GROUP_NAME "lock")

kmodule_configure_kernel_functions(functions OPTIONAL
	"mutex_lock"
	"mutex_lock_killable"
	"mutex_lock_interruptible"
	
	"mutex_lock_nested"
	"mutex_lock_interruptible_nested"
	"mutex_lock_killable_nested"
	"_mutex_lock_nest_lock"
	
	"mutex_trylock"
	"atomic_dec_and_mutex_lock"
	
	"_raw_spin_lock"
	"_raw_spin_lock_irqsave"
	"_raw_spin_lock_irq"
	"_raw_spin_lock_bh"

	"_spin_lock"
	"_spin_lock_irqsave"
	"_spin_lock_irq"
	"_spin_lock_bh"

	# TODO: more
)
create_handlers_group(${GROUP_NAME} ${functions})

kedr_test_add_subdirectory(tests)
########################################################################
