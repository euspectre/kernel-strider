configure_file (
	"${CMAKE_CURRENT_SOURCE_DIR}/test_shallow.sh.in"
	"${CMAKE_CURRENT_BINARY_DIR}/test_shallow.sh"
	@ONLY
)

kedr_test_add_script (mem_core.sampling.shallow.01
	test_shallow.sh
)
