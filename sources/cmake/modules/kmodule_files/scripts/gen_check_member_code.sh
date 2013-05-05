#!/bin/sh

##############################################################################
# Generates the code of a fake kernel module to check if a member is present
# in a struct or union. The code is output to stdout.
# Arguments:
#	$1 - name of the structure or union (e.g. "struct module");
#	$2 - name of the member to check (e.g. "init");
#	$3 - header file to #include (e.g. "linux/module.h")
##############################################################################

if test -z "$1" || test -z "$2" || test -z "$3"; then
	printf "Usage: $0 <type> <member> <header>\n"
	exit 1
fi

cat <<END
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>

END

printf "#include <$3>\n\n"

cat <<END
static int __init
my_init(void)
{
END

printf "\t$1 obj;\n"
printf "\tvoid *member = (void *)&(obj.$2);\n"

cat <<END
	pr_info("%p\n", member);
	return 0;
}

static void __exit
my_exit(void)
{
	return;
}

module_init(my_init);
module_exit(my_exit);
END

exit 0
