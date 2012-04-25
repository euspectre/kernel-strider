set(GROUP_NAME "unlock")

kmodule_configure_kernel_functions(functions OPTIONAL
	"mutex_unlock"
	
	"_raw_spin_unlock"
	"_raw_spin_unlock_irqrestore"
	"_raw_spin_unlock_irq"
	"_raw_spin_unlock_bh"
	
	"_spin_unlock"
	"_spin_unlock_irqrestore"
	"_spin_unlock_irq"
	"_spin_unlock_bh"

	# TODO: more
)
create_handlers_group(${GROUP_NAME} ${functions})

kedr_test_add_subdirectory(tests)
########################################################################
