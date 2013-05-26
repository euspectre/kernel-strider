kmodule_configure_kernel_functions(functions OPTIONAL
	"queue_work_on"
	"queue_work"
	"queue_delayed_work_on"
	"queue_delayed_work"

	"mod_delayed_work_on"
	"mod_delayed_work"

	"flush_work"
	"cancel_work_sync"
	"flush_delayed_work"
	"cancel_delayed_work_sync"

	"schedule_work_on"
	"schedule_work"
	"schedule_delayed_work_on"
	"schedule_delayed_work"

	"flush_scheduled_work"

	"flush_workqueue"
	"drain_workqueue"

	"destroy_workqueue"

	"__alloc_workqueue_key"
	"__create_workqueue_key"
)
group_create_handlers(${GROUP_NAME} ${functions})

# Since kernel version 3.3, __alloc_workqueue_key() accepts a variable
# argument list. Because of that, the function must be handled differently
# in the different versions of the kernel: KEDR_LS_ARGn_VA() VS 
# KEDR_LS_ARGn() to access the arguments, etc. 
# Check the signature and set KEDR_ALLOC_WQ_KEY_VARARG appropriately.
list(FIND functions "__alloc_workqueue_key" func_index)
if (NOT (func_index EQUAL -1))
	check_alloc_wq_key()
endif() 

group_set_sources(${PLUGIN_SHORT_NAME} ${GROUP_NAME} 
	"handlers_${GROUP_NAME}.c"
)

kedr_test_add_subdirectory(tests)
########################################################################
