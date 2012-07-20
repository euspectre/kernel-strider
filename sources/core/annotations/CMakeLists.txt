kedr_load_install_prefixes()
set(KEDR_ANN_FILES_DIR "${KEDR_INSTALL_PREFIX_READONLY}/annotations")

install(FILES   
	"${CMAKE_CURRENT_SOURCE_DIR}/kedr_annotations.c"
	"${CMAKE_CURRENT_SOURCE_DIR}/kedr_annotations.h"
	DESTINATION "${KEDR_ANN_FILES_DIR}"
)

kedr_test_add_subdirectory(tests)
