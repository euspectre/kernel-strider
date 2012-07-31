set(APP_NAME "test_buggy01")

install(FILES
	"${CMAKE_CURRENT_SOURCE_DIR}/test_buggy01.c"
	DESTINATION "${KEDR_EXAMPLE_PATH}/${APP_NAME}"
)

# Build the application to be used in the tests. This will test it too,
# implicitly.
add_executable("${APP_NAME}"
	test_buggy01.c
)
kedr_test_add_target(${APP_NAME})
