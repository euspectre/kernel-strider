# Model for character devices.
set(plugin_name "kedr_plugin_cdev")

kbuild_include_directories("${CMAKE_CURRENT_BINARY_DIR}")
kbuild_include_directories("${KEDR_COI_INSTALL_DIR}/include")

kbuild_use_symbols("${KEDR_COI_INSTALL_DIR}/lib/modules/${KBUILD_VERSION_STRING}/symvers/kedr_coi.symvers")

kbuild_use_symbols("${CMAKE_BINARY_DIR}/core/Module.symvers")
kbuild_add_dependencies("kedr_mem_core")

kbuild_use_symbols("${CMAKE_BINARY_DIR}/functions/Module.symvers")
kbuild_add_dependencies("kedr_func_drd")

kbuild_add_module(${plugin_name}
    "module.c"
    "file_operations_model.c"
    "file_operations_model.h"
    "file_operations_interceptor.c"
    "${CMAKE_CURRENT_BINARY_DIR}/cdev_file_operations_interceptor.c"
    "${CMAKE_CURRENT_BINARY_DIR}/cdev_file_operations_interceptor.h"
)

rule_copy_file("file_operations_interceptor.c"
    "${interceptors_dir}/file_operations_interceptor.c")

rule_generate_file("cdev_file_operations_interceptor.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/cdev_file_operations.data"
    "${interceptor_templates_dir}/kedr_coi_factory_interceptor.c")

rule_generate_file("cdev_file_operations_interceptor.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/cdev_file_operations.data"
    "${interceptor_templates_dir}/kedr_coi_factory_interceptor.h")

rule_generate_file("file_operations_model.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/file_operations.data"
    "${model_templates_dir}/operations_model.c")
rule_generate_file("file_operations_model.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/file_operations.data"
    "${model_templates_dir}/operations_model.h")

kedr_install_kmodule(${plugin_name})
