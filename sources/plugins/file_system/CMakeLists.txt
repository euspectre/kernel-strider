# Model for character devices.
set(plugin_name "kedr_plugin_file_system")

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
    
    "fs_interception.c"
    "fs_interception.h"
    
    "file_operations_interceptor.c"
    "inode_file_operations_interceptor.c"
    "file_system_type_interceptor.c"
    "super_operations_interceptor.c"
    "inode_operations_interceptor.c"
    "dentry_operations_interceptor.c"
    
    "file_operations_model.c"
    "file_operations_model.h"
    "inode_operations_model.c"
    "inode_operations_model.h"
    "super_operations_model.c"
    "super_operations_model.h"
    "file_system_type_model.c"
    "file_system_type_model.h"
)

# Interception for whole file system is taken from example in KEDR COI
rule_copy_file("fs_interception.c"
    "${KEDR_COI_INSTALL_DIR}/share/kedr-coi/examples/file_access_counter/fs_interception.c")
rule_copy_file("fs_interception.h"
    "${KEDR_COI_INSTALL_DIR}/share/kedr-coi/examples/file_access_counter/fs_interception.h")

# Interceptors itself taken from correponded places
rule_copy_file("file_operations_interceptor.c"
    "${interceptors_dir}/file_operations_interceptor.c")
rule_copy_file("inode_file_operations_interceptor.c"
    "${interceptors_dir}/inode_file_operations_interceptor.c")
rule_copy_file("file_system_type_interceptor.c"
    "${interceptors_dir}/file_system_type_interceptor.c")
rule_copy_file("super_operations_interceptor.c"
    "${interceptors_dir}/super_operations_interceptor.c")
rule_copy_file("inode_operations_interceptor.c"
    "${interceptors_dir}/inode_operations_interceptor.c")
rule_copy_file("dentry_operations_interceptor.c"
    "${interceptors_dir}/dentry_operations_interceptor.c")





rule_generate_file("file_operations_model.c"
    "${CMAKE_CURRENT_BINARY_DIR}/file_operations_all.data"
    "${model_templates_dir}/operations_model.c")

rule_generate_file("file_operations_model.h"
    "${CMAKE_CURRENT_BINARY_DIR}/file_operations_all.data"
    "${model_templates_dir}/operations_model.h")

# Combine data of file operations itself and their connection with inode
# object into one datafile.
add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/file_operations_all.data"
    COMMAND cat "${CMAKE_CURRENT_SOURCE_DIR}/../cdev/file_operations.data"
        "${CMAKE_CURRENT_SOURCE_DIR}/inode_file_operations.data"
        > "${CMAKE_CURRENT_BINARY_DIR}/file_operations_all.data"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/../cdev/file_operations.data"
        "${CMAKE_CURRENT_SOURCE_DIR}/inode_file_operations.data"
)

rule_generate_file("inode_operations_model.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/inode_operations.data"
    "${model_templates_dir}/operations_model.c")

rule_generate_file("inode_operations_model.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/inode_operations.data"
    "${model_templates_dir}/operations_model.h")


rule_generate_file("super_operations_model.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/super_operations.data"
    "${model_templates_dir}/operations_model.c")

rule_generate_file("super_operations_model.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/super_operations.data"
    "${model_templates_dir}/operations_model.h")


rule_generate_file("file_system_type_model.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/file_system_type.data"
    "${model_templates_dir}/operations_model.c")

rule_generate_file("file_system_type_model.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/file_system_type.data"
    "${model_templates_dir}/operations_model.h")


kedr_install_kmodule(${plugin_name})
