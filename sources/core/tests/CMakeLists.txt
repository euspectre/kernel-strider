set(CORE_MODULE_DIR "${CMAKE_BINARY_DIR}/core")
set(CORE_MODULE_NAME "${KMODULE_NAME}")

# A local version of the user-mode helper script. It is needed
# because the tests are expected to be run before installation
# of our system. It is more reliable to have the script locally.
configure_file(
	"${CMAKE_SOURCE_DIR}/core/um_helpers/kedr_get_sections.sh.in"
	"${CMAKE_CURRENT_BINARY_DIR}/kedr_get_sections.sh"
	@ONLY
)
# The directory where the tests should look for the script.
set(KEDR_UMH_DIR "${CMAKE_CURRENT_BINARY_DIR}")

kbuild_include_directories("${CMAKE_CURRENT_BINARY_DIR}")

add_subdirectory(simple_ins_rm)
add_subdirectory(reg_unreg)
add_subdirectory(ls_allocator)
add_subdirectory(providers)
add_subdirectory(sections)
add_subdirectory(module_ms_alloc)
add_subdirectory(i13n)
add_subdirectory(reporter)
add_subdirectory(callbacks)
########################################################################
