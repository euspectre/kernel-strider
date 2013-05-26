kedr_load_test_prefixes()

set(functions_to_test
	"mutex_lock"
	"mutex_lock_interruptible"
	"mutex_lock_killable"
	"mutex_trylock"
	
	"mutex_lock_nested"
	"mutex_lock_interruptible_nested"
	"mutex_lock_killable_nested"
	
	"atomic_dec_and_mutex_lock"
	
	"_raw_spin_lock"
	"_raw_spin_lock_bh"
	"_raw_spin_lock_irq"
	"_raw_spin_lock_irqsave"
	"_spin_lock"
	"_spin_lock_bh"
	"_spin_lock_irq"
	"_spin_lock_irqsave"
	
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
)

# The regular expression the trace obtained in the test should match
# for the test to pass.
set(TEST_MATCH_EXPR " LOCK [A-Z0-9]+ PRE.* LOCK [A-Z0-9]+ POST")

add_tests_for_group(${PLUGIN_SHORT_NAME} ${GROUP_NAME} ${functions_to_test})
########################################################################
