#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/debugfs.h>

#include "sections.h"
/* ====================================================================== */

MODULE_AUTHOR("Eugene A. Shatokhin");
MODULE_LICENSE("GPL");
/* ====================================================================== */

/* Name of the module to analyze. An empty name will match no module. */
static char *target_name = "";
module_param(target_name, charp, S_IRUGO);
/* ====================================================================== */

/* A directory for our system in debugfs. */
struct dentry *debugfs_dir_dentry = NULL;
const char *debugfs_dir_name = "kedr_foo";
/* ====================================================================== */

static int __init
kedr_init(void)
{
	int ret = 0;
	
	debugfs_dir_dentry = debugfs_create_dir(debugfs_dir_name, NULL);
	if (IS_ERR(debugfs_dir_dentry)) {
		pr_err("[sample] debugfs is not supported\n");
		ret = -ENODEV;
		goto out;
	}

	if (debugfs_dir_dentry == NULL) {
		pr_err("[sample] "
			"failed to create a directory in debugfs\n");
		ret = -EINVAL;
		goto out;
	}
	
	ret = kedr_init_section_subsystem(debugfs_dir_dentry);
	if (ret != 0)
		goto out_rmdir;
	
	/* This is called in init function only to demonstrate that it 
	 * works. In a real system, the information about the sections 
	 * should probably be obtained when handling the loading of the 
	 * target module. */
	ret = kedr_print_section_info(target_name);
	if (ret != 0)
		goto out_rmdir;
	
	return 0;

out_rmdir:
	kedr_cleanup_section_subsystem();
	debugfs_remove(debugfs_dir_dentry);
out:
	return ret;
}

static void __exit
kedr_exit(void)
{
	kedr_cleanup_section_subsystem();
	debugfs_remove(debugfs_dir_dentry);
}

module_init(kedr_init);
module_exit(kedr_exit);
/* ====================================================================== */
