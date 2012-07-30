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

MODULE_LICENSE("GPL");
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
	pr_info(TEST_MSG_PREFIX "Opening, count is %d.\n", 
		some_data->count);
	
	mutex_lock(&some_lock);
	++some_data->count;
	snprintf(some_data->buf, strlen(some_string) + max_len, 
		"#%d: %s\n", some_data->count, some_string);
	filp->private_data = some_data->buf;
	mutex_unlock(&some_lock);
	
	return nonseekable_open(inode, filp);
}

static int 
sample_release(struct inode *inode, struct file *filp)
{
	mutex_lock(&some_lock);
	pr_info(TEST_MSG_PREFIX "Closing, count is %d.\n", 
		some_data->count);
	mutex_unlock(&some_lock);
	return 0;
}

static ssize_t 
sample_read(struct file *filp, char __user *buf, size_t count, 
	loff_t *f_pos)
{
	size_t len;
	ssize_t ret = 0;
	loff_t pos = *f_pos;
	const char *data = (const char *)filp->private_data;
	
	if (data == NULL) 
		return -EINVAL;
	
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
fail1:
	debugfs_remove(dir_dentry);
fail0:
	kfree(some_data);
	return err;
}

static void __exit
sample_exit_module(void)
{
	pr_info(TEST_MSG_PREFIX "Opened %d time(s).\n", 
		some_data->count);
	
	debugfs_remove(file_dentry);
	debugfs_remove(dir_dentry);
	kfree(some_data->buf);
	kfree(some_data);
	return;
}

module_init(sample_init_module);
module_exit(sample_exit_module);
/* ====================================================================== */
