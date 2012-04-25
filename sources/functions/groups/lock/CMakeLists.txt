set(GROUP_NAME "lock")

kmodule_configure_kernel_functions(functions OPTIONAL
	"mutex_lock"
	"mutex_lock_killable"
	"mutex_lock_interruptible"
	
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
