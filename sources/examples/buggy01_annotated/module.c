/* A relatively simple module that may have concurrency-related 
 * problems. */

#include <linux/module.h>
#include <linux/init.h>

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/debugfs.h>
#include <linux/mutex.h>

#include <asm/uaccess.h>

#include "kedr_annotations.h"

MODULE_LICENSE("GPL");
/* ====================================================================== */

/* IDs of "happens-before" arcs (links between the different locations in
 * the code). */
enum id_happens_before
{
	/* 0 should not be used as an ID. */
	ID_HB_INVALID = 0,
	
	/* 1. No file operation callback can start before 
	 * debugfs_create_file() starts for the corresponding file. */
	ID_CREATE_HB_OPEN,
	ID_CREATE_HB_READ,
	ID_CREATE_HB_RELEASE,
	
	/* 2. No file operation callback can be completed later than 
	 * debugfs_remove() called for the corresponding file returns. */
	ID_OPEN_HB_REMOVE,
	ID_READ_HB_REMOVE,
	ID_RELEASE_HB_REMOVE,
	
	/* 3. No file operation callback can be completed later than 
	 * the exit function of the target module starts. */
	ID_OPEN_HB_EXIT,
	ID_READ_HB_EXIT,
	ID_RELEASE_HB_EXIT,
};
/* ====================================================================== */

#define TEST_MSG_PREFIX "[buggy01] "
/* ====================================================================== */

static size_t max_len = 32;
static char *some_string = "Hello!";
/* ====================================================================== */

struct some_data
{
	char *buf;
	int count;
};

static DEFINE_MUTEX(some_lock);

static struct some_data *some_data = NULL;
static struct dentry *dir_dentry = NULL;
static struct dentry *file_dentry = NULL;
/* ====================================================================== */

static int 
sample_open(struct inode *inode, struct file *filp)
{
	int ret;
	KEDR_ANNOTATE_MEMORY_ACQUIRED(filp, sizeof(*filp));
	KEDR_ANNOTATE_HAPPENS_AFTER(ID_CREATE_HB_OPEN);
	
	pr_info(TEST_MSG_PREFIX "Opening, count is %d.\n", 
		some_data->count);
	
	mutex_lock(&some_lock);
	++some_data->count;
	snprintf(some_data->buf, strlen(some_string) + max_len, 
		"#%d: %s\n", some_data->count, some_string);
	filp->private_data = some_data->buf;
	mutex_unlock(&some_lock);
	
	ret = nonseekable_open(inode, filp);
	
	KEDR_ANNOTATE_HAPPENS_BEFORE(ID_OPEN_HB_REMOVE);
	KEDR_ANNOTATE_HAPPENS_BEFORE(ID_OPEN_HB_EXIT);
	return ret;
}

static int 
sample_release(struct inode *inode, struct file *filp)
{
	KEDR_ANNOTATE_HAPPENS_AFTER(ID_CREATE_HB_RELEASE);
	
	mutex_lock(&some_lock);
	pr_info(TEST_MSG_PREFIX "Closing, count is %d.\n", 
		some_data->count);
	mutex_unlock(&some_lock);
	
	KEDR_ANNOTATE_HAPPENS_BEFORE(ID_RELEASE_HB_REMOVE);
	KEDR_ANNOTATE_HAPPENS_BEFORE(ID_RELEASE_HB_EXIT);
	KEDR_ANNOTATE_MEMORY_RELEASED(filp);
	return 0;
}

static ssize_t 
sample_read(struct file *filp, char __user *buf, size_t count, 
	loff_t *f_pos)
{
	size_t len;
	ssize_t ret = 0;
	loff_t pos; 
	const char *data; 
	
	KEDR_ANNOTATE_HAPPENS_AFTER(ID_CREATE_HB_READ);
	pos = *f_pos;
	data = (const char *)filp->private_data;
	if (data == NULL) {
		KEDR_ANNOTATE_HAPPENS_BEFORE(ID_READ_HB_REMOVE);
		KEDR_ANNOTATE_HAPPENS_BEFORE(ID_READ_HB_EXIT);
		return -EINVAL;
	}
	
	mutex_lock(&some_lock);
	pr_info(TEST_MSG_PREFIX "Reading, count is %d.\n", 
		some_data->count);
	
	/* Reading outside of the data buffer is not allowed */
	len = strlen(data);
	if (pos < 0 || pos > len) {
		ret = -EINVAL;
		goto out;
	}
	
	/* EOF reached or 0 bytes requested */
	if (count == 0 || pos == len) 
		goto out;
	
	if (pos + count > len) 
		count = len - pos;
		
	if (copy_to_user(buf, &data[pos], count) != 0) {
		ret = -EFAULT;
		goto out;
	}
	
	*f_pos += count;
	ret = count;
out:
	mutex_unlock(&some_lock);

	KEDR_ANNOTATE_HAPPENS_BEFORE(ID_READ_HB_REMOVE);
	KEDR_ANNOTATE_HAPPENS_BEFORE(ID_READ_HB_EXIT);
	return ret;
}

/* Operations for a read-only file. */
struct file_operations sample_fops = {
	.owner =    THIS_MODULE,
	.read =     sample_read,
	.open =     sample_open,
	.release =  sample_release,
};
/* ====================================================================== */

static int __init
sample_init_module(void)
{
	int err = 0;
	
	some_data = kzalloc(sizeof(struct some_data), GFP_KERNEL);
	if (some_data == NULL) {
		err = -ENOMEM;
		goto fail0;
	}
	some_data->count = 0;
	
	dir_dentry = debugfs_create_dir("buggy01", NULL);
	if (dir_dentry == NULL) {
		pr_err(TEST_MSG_PREFIX 
			"Failed to create directory in debugfs\n");
		return -EPERM;
	}
	
	/* KEDR_ANNOTATE_HAPPENS_BEFORE() is usually placed right before
	 * the operation to be annotated, KEDR_ANNOTATE_HAPPENS_AFTER() -
	 * right after. */
	KEDR_ANNOTATE_HAPPENS_BEFORE(ID_CREATE_HB_OPEN);
	KEDR_ANNOTATE_HAPPENS_BEFORE(ID_CREATE_HB_READ);
	KEDR_ANNOTATE_HAPPENS_BEFORE(ID_CREATE_HB_RELEASE);
	file_dentry = debugfs_create_file("data", S_IRUGO, dir_dentry, NULL, 
		&sample_fops);
	if (file_dentry == NULL) {
		pr_err(TEST_MSG_PREFIX 
			"Failed to create file in debugfs\n");
		err = -EPERM;
		goto fail1;
	}
	
	some_data->buf = kzalloc(strlen(some_string) + max_len, 
		GFP_KERNEL);
	if (some_data->buf == NULL) {
		err = -ENOMEM;
		goto fail2;
	}
	some_data->buf[0] = 0;
	return 0;

fail2:
	debugfs_remove(file_dentry);
	KEDR_ANNOTATE_HAPPENS_AFTER(ID_OPEN_HB_REMOVE);
	KEDR_ANNOTATE_HAPPENS_AFTER(ID_READ_HB_REMOVE);
	KEDR_ANNOTATE_HAPPENS_AFTER(ID_RELEASE_HB_REMOVE);
fail1:
	debugfs_remove(dir_dentry);
fail0:
	kfree(some_data);
	return err;
}

static void __exit
sample_exit_module(void)
{
	KEDR_ANNOTATE_HAPPENS_AFTER(ID_OPEN_HB_EXIT);
	KEDR_ANNOTATE_HAPPENS_AFTER(ID_READ_HB_EXIT);
	KEDR_ANNOTATE_HAPPENS_AFTER(ID_RELEASE_HB_EXIT);
	
	pr_info(TEST_MSG_PREFIX "Opened %d time(s).\n", 
		some_data->count);
	
	debugfs_remove(file_dentry);
	/* The following 3 annotations are redundant because each callback
	 * finishes before the exit functions of the module starts. The 
	 * annotations with ids 'ID_*_HB_EXIT' let KernelStrider know about
	 * that. Still, leaving the annotations here makes no harm. */
	KEDR_ANNOTATE_HAPPENS_AFTER(ID_OPEN_HB_REMOVE);
	KEDR_ANNOTATE_HAPPENS_AFTER(ID_READ_HB_REMOVE);
	KEDR_ANNOTATE_HAPPENS_AFTER(ID_RELEASE_HB_REMOVE);
	
	debugfs_remove(dir_dentry);
	kfree(some_data->buf);
	kfree(some_data);
	return;
}

module_init(sample_init_module);
module_exit(sample_exit_module);
/* ====================================================================== */
