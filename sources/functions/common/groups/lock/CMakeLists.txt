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
	
	"_atomic_dec_and_lock"
	
	"_raw_spin_trylock"
	"_raw_spin_trylock_bh"
	"_spin_trylock"
	"_spin_trylock_bh"

	"_raw_read_trylock"
	"_raw_read_lock"
	"_raw_read_lock_irqsave"
	"_raw_read_lock_irq"
	"_raw_read_lock_bh"

	"_raw_write_trylock"
	"_raw_write_lock"
	"_raw_write_lock_irqsave"
	"_raw_write_lock_irq"
	"_raw_write_lock_bh"

	"_raw_spin_lock_nested"
	"_raw_spin_lock_irqsave_nested"
	"_raw_spin_lock_nest_lock"

	"_read_trylock"
	"_read_lock"
	"_read_lock_irqsave"
	"_read_lock_irq"
	"_read_lock_bh"

	"_write_trylock"
	"_write_lock"
	"_write_lock_irqsave"
	"_write_lock_irq"
	"_write_lock_bh"

	"_spin_lock_nested"
	"_spin_lock_irqsave_nested"
	"_spin_lock_nest_lock"
)
group_create_handlers(${GROUP_NAME} ${functions})
group_set_sources(${PLUGIN_SHORT_NAME} ${GROUP_NAME} 
	"handlers_${GROUP_NAME}.c"
)

kedr_test_add_subdirectory(tests)
########################################################################
