# Static library for construct CTF metadata using sequence of
# instructions or parse a file.
#
# Constructed metadata may be used for read CTF trace in abstract
# context.

set(ctf_meta_constructor_name "ctf_meta_constructor")

include_directories("${CMAKE_CURRENT_SOURCE_DIR}")

########################################################################
# If KEDR_GENERATE_CTF_META_PARSER is defined, C source file implementing
# parser of CTF metadata will be generated from flex source file.
# Otherwise C source file will be simply copied from source tree.
#
# In any case, build tree will contain C source file at building stage.
########################################################################
option(KEDR_GENERATE_CTF_META_PARSER
	"Generate KEDR Reference Manual from the sources in DocBook XML format." 
	ON
)

# Name of source file implementing scanner without extension
set(ctf_meta_scanner_basename "ctf_meta_lexer")
# Same for parser
set(ctf_meta_parser_basename "ctf_meta_parser")

if(KEDR_GENERATE_CTF_META_PARSER)
    message(STATUS "Checking for flex scanner generator")
    find_program(FLEX flex)
    if(FLEX)
        message(STATUS "Checking for flex scanner generator - ${FLEX}")
    else(FLEX)
        message(FATAL_ERROR "Flex is not found")
    endif(FLEX)

    find_program(BISON bison)
    if(FLEX)
        message(STATUS "Checking for bison parser generator - ${FLEX}")
    else(FLEX)
        message(FATAL_ERROR "Bison is not found")
    endif(FLEX)

    add_custom_command(
        OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${ctf_meta_parser_basename}.tab.c"
            "${CMAKE_CURRENT_BINARY_DIR}/${ctf_meta_parser_basename}.tab.h"
        COMMAND ${BISON} -Wall -d --location
            "${CMAKE_CURRENT_SOURCE_DIR}/${ctf_meta_parser_basename}.y"
        DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/${ctf_meta_parser_basename}.y"
    )
    # Currently don't use '-l' option to FLEX, otherwise -Wall produce
    # warnings. See
    # http://sourceforge.net/tracker/?func=detail&aid=3510440&group_id=97492&atid=618177
    # for details.
    add_custom_command(
        OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${ctf_meta_scanner_basename}.c"
        COMMAND ${FLEX} "--header-file=${ctf_meta_scanner_basename}.h"
                        -o "${ctf_meta_scanner_basename}.c"
            "${CMAKE_CURRENT_SOURCE_DIR}/${ctf_meta_scanner_basename}.l"
        DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/${ctf_meta_scanner_basename}.l"
                "${CMAKE_CURRENT_BINARY_DIR}/${ctf_meta_parser_basename}.tab.h"
    )
    # When generate reentrant scanner, additional argument may be not
    # used by some LEX functions. Disable warnings about unused
    # arguments when compile generated source.
    set_source_files_properties("${ctf_meta_scanner_basename}.c"
        PROPERTIES COMPILE_FLAGS "-Wno-unused-parameter")
    
    # Because flex and bison generates header files, binary dir should
    # be included for compile c-sources.
    include_directories("${CMAKE_CURRENT_BINARY_DIR}")
else(KEDR_GENERATE_CTF_META_PARSER)
    rule_copy_source("${ctf_meta_parser_basename}.tab.c")
    rule_copy_source("${ctf_meta_scanner_basename}.c")
endif(KEDR_GENERATE_CTF_META_PARSER)


add_library(${ctf_meta_constructor_name} STATIC
    "ctf_meta.c"
    "ctf_type.c"
    "ctf_scope.c"
    "ctf_tag.c"
    "${ctf_meta_parser_basename}.tab.c"
    "${ctf_meta_scanner_basename}.c"
    "ctf_meta_parse.c"
    "ctf_meta_constructor.c"
    "ctf_ast.c"
)

kedr_test_add_subdirectory(tests)