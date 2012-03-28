set(KMODULE_NAME "kedr_null_alloc")
set(TOP_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/include/kedr") 

kbuild_use_symbols("${CMAKE_BINARY_DIR}/core/Module.symvers") 
kbuild_add_module(${KMODULE_NAME} 
# sources
	"module.c"

# headers	
	"${TOP_INCLUDE_DIR}/kedr_mem/local_storage.h"
)

kedr_test_add_target (${KMODULE_NAME})
