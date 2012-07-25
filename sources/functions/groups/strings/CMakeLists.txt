set(GROUP_NAME "strings")

kmodule_configure_kernel_functions(functions OPTIONAL
	"memset"
	"memcpy"
	"__memcpy"
	"memmove"
	"memcmp"
	"memscan"
	"memchr"
	"memchr_inv"

	"strcmp"
	"strncmp"	
	"strnicmp"
	"strcasecmp"
	"strncasecmp"

	"strcpy"
	"strncpy"
	"strlcpy"

	"strcat"
	"strncat"
	"strlcat"

	"strchr"
	"strrchr"
	"strnchr"

	"skip_spaces"
	"strim"
	"strstrip"

	"strlen"
	"strnlen"

	"strspn"
	"strcspn"
	"strpbrk"

	"strsep"

	"strstr"
	"strnstr"
)
create_handlers_group(${GROUP_NAME} ${functions})

kedr_test_add_subdirectory(tests)
########################################################################
