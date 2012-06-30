# Model for character devices.
set(plugin_name "kedr_plugin_cdev")

kbuild_include_directories("${KEDR_COI_INSTALL_DIR}/include")

kbuild_use_symbols("${KEDR_COI_INSTALL_DIR}/lib/modules/${KBUILD_VERSION_STRING}/symvers/kedr_coi.symvers")

kbuild_use_symbols("${CMAKE_BINARY_DIR}/core/Module.symvers")
kbuild_add_dependencies("kedr_mem_core")

kbuild_use_symbols("${CMAKE_BINARY_DIR}/functions/Module.symvers")
kbuild_add_dependencies("kedr_func_drd")

kbuild_add_module(${plugin_name}
    "module.c"
    "file_operations_interceptor.c"
    "${CMAKE_CURRENT_BINARY_DIR}/cdev_file_operations_interceptor.c"
    "${CMAKE_CURRENT_BINARY_DIR}/cdev_file_operations_interceptor.h"
)

rule_copy_file("file_operations_interceptor.c"
    "${interceptors_dir}/file_operations_interceptor.c")

rule_factory_interceptor_source("cdev_file_operations_interceptor.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/cdev_file_operations.data")

rule_factory_interceptor_header("cdev_file_operations_interceptor.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/cdev_file_operations.data")


kedr_install_kmodule(${plugin_name})
