set(KMODULE_NAME "buggy01")

configure_file(
	"${CMAKE_CURRENT_SOURCE_DIR}/makefile_installed.in"
	"${CMAKE_CURRENT_BINARY_DIR}/makefile_installed"
	@ONLY)
configure_file(
	"${CMAKE_CURRENT_SOURCE_DIR}/Kbuild.in"
	"${CMAKE_CURRENT_BINARY_DIR}/Kbuild"
	@ONLY)

install(FILES
	"${CMAKE_CURRENT_SOURCE_DIR}/module.c"
	"${CMAKE_CURRENT_SOURCE_DIR}/Readme.txt"
	DESTINATION "${KEDR_EXAMPLE_PATH}/buggy01"
)
install(FILES
	"${CMAKE_CURRENT_BINARY_DIR}/makefile_installed"
	DESTINATION "${KEDR_EXAMPLE_PATH}/buggy01"
	RENAME "Makefile"
)
install(FILES
	"${CMAKE_CURRENT_BINARY_DIR}/Kbuild"
	DESTINATION "${KEDR_EXAMPLE_PATH}/buggy01"
)

kedr_test_add_subdirectory(tests)
