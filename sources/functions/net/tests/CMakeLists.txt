# This test checks only that the plugin can be loaded.
configure_file (
	"${CMAKE_CURRENT_SOURCE_DIR}/test.sh.in"
	"${CMAKE_CURRENT_BINARY_DIR}/test.sh"
	@ONLY
)

kedr_test_add_script (functions.net.01
	test.sh
)
