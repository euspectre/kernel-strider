kedr_load_test_prefixes()

set(functions_to_test
	"mutex_unlock"
	"_raw_spin_unlock"
	"_raw_spin_unlock_irq"
	"_raw_spin_unlock_irqrestore"
	"_spin_unlock"
	"_spin_unlock_irq"
	"_spin_unlock_irqrestore"
	
	# TODO: more
)

set(TEST_MATCH_EXPR "UNLOCK [A-Z0-9]+ PRE.*UNLOCK [A-Z0-9]+ POST")

add_tests_for_group(${GROUP_NAME} ${functions_to_test})
########################################################################
