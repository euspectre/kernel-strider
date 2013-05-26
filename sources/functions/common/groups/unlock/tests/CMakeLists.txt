kedr_load_test_prefixes()

set(functions_to_test
	"mutex_unlock"
	
	"_raw_spin_unlock"
	"_raw_spin_unlock_bh"
	"_raw_spin_unlock_irq"
	"_raw_spin_unlock_irqrestore"
	
	"_spin_unlock"
	"_spin_unlock_bh"
	"_spin_unlock_irq"
	"_spin_unlock_irqrestore"
	
	"_raw_read_unlock"
	"_raw_read_unlock_irqrestore"
	"_raw_read_unlock_irq"
	"_raw_read_unlock_bh"

	"_raw_write_unlock"
	"_raw_write_unlock_irqrestore"
	"_raw_write_unlock_irq"
	"_raw_write_unlock_bh"

	"_read_unlock"
	"_read_unlock_irqrestore"
	"_read_unlock_irq"
	"_read_unlock_bh"

	"_write_unlock"
	"_write_unlock_irqrestore"
	"_write_unlock_irq"
	"_write_unlock_bh"
)

set(TEST_MATCH_EXPR " UNLOCK [A-Z0-9]+ PRE.* UNLOCK [A-Z0-9]+ POST")

add_tests_for_group(${PLUGIN_SHORT_NAME} ${GROUP_NAME} ${functions_to_test})
########################################################################
