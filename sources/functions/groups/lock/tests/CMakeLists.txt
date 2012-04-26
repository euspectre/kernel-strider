kedr_load_test_prefixes()

set(functions_to_test
	"mutex_lock"
	"mutex_lock_interruptible"
	"mutex_lock_killable"
	"mutex_trylock"
	"_raw_spin_lock"
	"_raw_spin_lock_irq"
	"_raw_spin_lock_irqsave"
	"_spin_lock"
	"_spin_lock_irq"
	"_spin_lock_irqsave"

	# TODO: more
)

# The regular expression the trace obtained in the test should match
# for the test to pass.
set(TEST_MATCH_EXPR "LOCK [A-Z0-9]+ PRE.*LOCK [A-Z0-9]+ POST")

add_tests_for_group(${GROUP_NAME} ${functions_to_test})
########################################################################
