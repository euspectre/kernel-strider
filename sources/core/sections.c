/* sections.c - API to search for the section addresses of a loaded kernel
 * module. */

/* ========================================================================
 * Copyright (C) 2012, KEDR development team
 * Authors: 
 *      Eugene A. Shatokhin <spectre@ispras.ru>
 *      Andrey V. Tsyvarev  <tsyvarev@ispras.ru>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 ======================================================================== */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/debugfs.h>
#include <linux/kmod.h>	/* user-mode helper API */
#include <asm/uaccess.h>

#include "sections.h"
#include "core_impl.h"
#include "config.h"
/* ====================================================================== */

/* Name and the full path to a helper script that should obtain the
 * addresses of the sections from sysfs. */
#define KEDR_HELPER_SCRIPT_NAME "kedr_get_sections.sh"
static char *umh_script = NULL;
static char umh_filename_part[] = "/" KEDR_HELPER_SCRIPT_NAME;
/* ====================================================================== */

/* The file in debugfs to be used by the user-mode helper to pass the 
 * collected data to our module. */
static struct dentry *data_file = NULL;
const char *debug_data_name = KEDR_SECTIONS_FILE;

#define KEDR_SECTION_BUFFER_SIZE 4096
static char *section_buffer = NULL;

/* This mutex is used to serialize accesses to the section buffer. */
static DEFINE_MUTEX(section_buffer_mutex);

/* This mutex is used to serialize execution of kedr_get_sections(). 
 * It is possible for that function to execute for different modules at the
 * same time, so we need to make sure it has completed a request before 
 * processing another one. 
 * Among other things, this serializes the execution of the user-mode
 * helper script, i.e., no more than one instance of the script can be 
 * executing at the same time. This is why 'section_buffer_mutex' alone 
 * is not enough. We cannot keep it locked while the helper script is 
 * running because that mutex should also be taken in write() file 
 * operation. */
static DEFINE_MUTEX(section_mutex);
/* ====================================================================== */

/* A convenience macro to define variable of type struct file_operations
 * for a write only file in debugfs associated with the specified buffer.
 * 
 * __fops - the name of the variable
 * __buf  - pointer to the output buffer (char *)
 */
#define DEBUG_UTIL_DEFINE_FOPS_WO(__fops, __buf)                        \
static int __fops ## _open(struct inode *inode, struct file *filp)      \
{                                                                       \
        filp->private_data = (void *)(__buf);                           \
        return nonseekable_open(inode, filp);                           \
}                                                                       \
static const struct file_operations __fops = {                          \
        .owner      = THIS_MODULE,                                      \
        .open       = __fops ## _open,                                  \
        .release    = debug_release_common,                             \
        .write      = debug_write_common,                               \
};

static int 
debug_release_common(struct inode *inode, struct file *filp)
{
	filp->private_data = NULL;
	/* nothing more to do: open() did not allocate any resources */
	return 0;
}

static ssize_t 
debug_write_common(struct file *filp, const char __user *buf, size_t count,
	loff_t *f_pos)
{
	ssize_t ret = 0;
	loff_t pos = *f_pos;
	char *sb = (char *)filp->private_data;

	if (sb == NULL) 
		return -EINVAL;
	
	if (mutex_lock_killable(&section_buffer_mutex) != 0)
	{
		pr_warning(KEDR_MSG_PREFIX "debug_write_common: "
			"got a signal while trying to acquire a mutex.\n");
		return -EINTR;
	}
	
	/* Writing outside of the buffer is not allowed. Note that one byte 
	 * should always be reserved for the terminating '\0'. */
	if ((pos < 0) || (pos > KEDR_SECTION_BUFFER_SIZE - 1)) {
		ret = -EINVAL;
		goto out;
	}
	
	/* We only accept data that fit into the buffer as a whole. */
	if (pos + count >= KEDR_SECTION_BUFFER_SIZE - 1) {
		pr_warning(KEDR_MSG_PREFIX "debug_write_common: "
		"a request to write %u bytes while the in-kernel buffer "
		"is only %u bytes long (without the terminating 0).\n",
			(unsigned int)count, 
			KEDR_SECTION_BUFFER_SIZE - 1);
		ret = -ENOSPC;
		goto out;
	}

	/* 0 bytes requested */
	if (count == 0)
		goto out;

	if (copy_from_user(&sb[pos], buf, count) != 0) {
		ret = -EFAULT;
		goto out;
	}
	sb[pos + count] = '\0';
	
	mutex_unlock(&section_buffer_mutex);

	*f_pos += count;
	return count;

out:
	mutex_unlock(&section_buffer_mutex);
	return ret;
}

/* Definition of file_operations structure for the file in debugfs. */
DEBUG_UTIL_DEFINE_FOPS_WO(fops_wo, section_buffer);
/* ====================================================================== */

static int 
kedr_run_um_helper(char *target_name)
{
	int ret = 0;
	unsigned int ret_status = 0;
	
	char *argv[] = {"/bin/sh", NULL, NULL, NULL};
	static char *envp[] = {
		"HOME=/",
		"TERM=linux",
		"PATH=/sbin:/bin:/usr/sbin:/usr/bin", NULL};
	
	BUG_ON(umh_script == NULL);
	argv[1] = umh_script;
	argv[2] = target_name;
			
	/* Invoke our shell script with the target name as a parameter and
	 * wait for its completion. */
	ret = call_usermodehelper(argv[0], argv, envp, UMH_WAIT_PROC);
	
	/* call_usermodehelper() actually returns a 2-byte code, see the 
	 * explanation here:
	 * http://lkml.indiana.edu/hypermail/linux/kernel/0904.1/00766.html 
	 */
	ret_status = (unsigned int)ret & 0xff;
	if (ret_status != 0) {
		pr_warning(KEDR_MSG_PREFIX 
			"Failed to execute %s, status is 0x%x\n",
			umh_script,
			ret_status);
		return -EINVAL;
	}
	
	ret >>= 8;
	if (ret != 0) {
		if (ret == 127) 
			pr_warning(KEDR_MSG_PREFIX "%s is missing.\n", 
				umh_script);
		else 
			pr_warning(KEDR_MSG_PREFIX
			"The helper failed (%s), error code: %d. "
			"See the comments in that helper script "  
			"for the description of this error code.\n", 
				umh_script, ret);
		return -EINVAL;
	}
	
	return 0;
}
/* ====================================================================== */

/* Create struct kedr_section instance with 'name' being a copy of the 
 * string from [name_beg, name_beg+len) and with the specified address. */
static struct kedr_section *
kedr_section_create(const char *name_beg, size_t len, unsigned long addr)
{
	struct kedr_section *sec;
	char *sec_name;
	
	sec = kzalloc(sizeof(*sec), GFP_KERNEL);
	if (sec == NULL)
		return NULL;
	
	sec_name = kstrndup(name_beg, len, GFP_KERNEL);
	if (sec_name == NULL) {
		kfree(sec);
		return NULL;
	}
	
	sec->name = sec_name;
	sec->addr = addr;
	return sec;
}

static void
kedr_section_destroy(struct kedr_section *sec)
{
	if (sec == NULL)
		return;
	kfree(sec->name);
	kfree(sec);
}

int 
kedr_init_section_subsystem(struct dentry *debugfs_dir)
{
	int ret = 0;
	size_t len = 0;
	
	len = strlen(umh_dir);
	umh_script = kzalloc(len + ARRAY_SIZE(umh_filename_part) + 1,
		GFP_KERNEL);
	if (umh_script == NULL)
		return -ENOMEM;
	strncpy(umh_script, umh_dir, len);
	strncpy(&umh_script[len], umh_filename_part, 
		ARRAY_SIZE(umh_filename_part));
	
	section_buffer = kzalloc(KEDR_SECTION_BUFFER_SIZE, GFP_KERNEL);
	if (section_buffer == NULL) {
		ret = -ENOMEM;
		goto out_free_path;
	}

	data_file = debugfs_create_file(debug_data_name, 
		S_IWUSR | S_IWGRP, debugfs_dir, NULL, &fops_wo);
	if (data_file == NULL) {
		pr_warning(KEDR_MSG_PREFIX
	"failed to create the file in debugfs for the sections data\n");
		ret = -EINVAL;
		goto out_free_sb;
	}

	return 0;

out_free_sb:
	kfree(section_buffer);
	section_buffer = NULL;

out_free_path:
	kfree(umh_script);
	umh_script = NULL;
	return ret;
}

void
kedr_cleanup_section_subsystem(void)
{
	if (data_file != NULL)
		debugfs_remove(data_file);
	
	kfree(section_buffer);
	section_buffer = NULL;
	
	kfree(umh_script);
	umh_script = NULL;
}

static int 
reset_section_buffer(void)
{
	BUG_ON(section_buffer == NULL);
	if (mutex_lock_killable(&section_buffer_mutex) != 0)
	{
		pr_warning(KEDR_MSG_PREFIX "reset_section_buffer: "
			"got a signal while trying to acquire a mutex.\n");
		return -EINTR;
	}
	
	memset(section_buffer, 0, KEDR_SECTION_BUFFER_SIZE);
	mutex_unlock(&section_buffer_mutex);
	return 0;
}

/* Non-zero if 'addr' is the address within "init" or "core" area of the 
 * given module, 0 otherwise. */
static int
is_valid_section_address(unsigned long addr, struct module *mod)
{
	unsigned long init_start = (unsigned long)mod->module_init;
	unsigned long core_start = (unsigned long)mod->module_core;
	
	if (init_start != 0 && addr >= init_start && 
	    addr < init_start + mod->init_size)
		return 1;
	
	if (core_start != 0 && addr >= core_start && 
	    addr < core_start + mod->core_size)
		return 1;
	
	return 0;
}

/* Parse the data in the section buffer and populate the list of sections.
 * The data format is expected to be as follows:
 *   <name> <hex_address>[ <name> <hex_address>...], for example:
 *   .text 0xffc01234 .data 0xbaadf00d
 * The function must be called with 'section_buffer_mutex' locked. 
 * [NB] If an error occurs we don't need to free the items of the section 
 * list created so far. They will be freed when resetting or cleaning up 
 * the subsystem anyway. */
static int 
parse_section_data(struct module *mod, struct list_head *sections)
{
	const char *ws = " \t\n\r";
	size_t pos;
	
	pos = strspn(section_buffer, ws);
	while (pos < (KEDR_SECTION_BUFFER_SIZE - 1) && 
		section_buffer[pos] != '\0') {
		char *addr_begin;
		char *addr_end;
		unsigned long addr;
		size_t len;
		size_t num;
		struct kedr_section *sec;
		
		len = strcspn(&section_buffer[pos], ws);
		if (len == 0)
			return -EINVAL;
		
		/* skip spaces, get to where a hex number is expected */
		num = pos + len + strspn(&section_buffer[pos + len], ws);
		if (num >= KEDR_SECTION_BUFFER_SIZE - 1)
		    	return -EINVAL;
		
		addr_begin = &section_buffer[num];
		addr = simple_strtoul(addr_begin, &addr_end, 16);
		if (addr == 0)
			return -EINVAL;
		BUG_ON(addr_begin >= addr_end);
		
		num = strspn(addr_end, ws);
		if (*addr_end != '\0' && num == 0)
			return -EINVAL;
		
		/* A sanity check for the obtained section address: it must
		 * be located in the "init" or "core" area of the module's
		 * image. */
		if (!is_valid_section_address(addr, mod)) {
			pr_warning(KEDR_MSG_PREFIX
			"The obtained section address (0x%lx) is outside "
			"of the module.", 
				addr);
			return -EFAULT;
		}
		
		sec = kedr_section_create(&section_buffer[pos], len, addr);
		if (sec == NULL)
			return -ENOMEM;
		list_add_tail(&sec->list, sections);
		
		pos = (addr_end - section_buffer) + num;
	}
	return 0;
}

int
kedr_get_sections(struct module *mod, struct list_head *sections)
{
	int ret = 0;
	char *target_name;
	
	BUG_ON(section_buffer == NULL);
	BUG_ON(mod == NULL);
	BUG_ON(sections == NULL);
	BUG_ON(!list_empty(sections));
	
	if (mutex_lock_killable(&section_mutex) != 0)
	{
		pr_warning(KEDR_MSG_PREFIX "kedr_get_sections: "
			"got a signal while trying to acquire a mutex.\n");
		return -EINTR;
	}
	
	target_name = module_name(mod);
	
	ret = reset_section_buffer();
	if (ret != 0)
		goto out;
	
	ret = kedr_run_um_helper(target_name);
	if (ret != 0)
		goto out;
	
	/* By this moment, the information about the sections must be in 
	 * section buffer. Lock the mutex to make sure we see the buffer in
	 * a consistent state and parse the data it contains. Note that it
	 * is possible that someone tries to write to the "channel file" 
	 * in debugfs manually for some obscure reason. We do not check for
	 * this. Either the buffer has valid data at this point or it does 
	 * not, no matter how the data got there. The data must be validated
	 * anyway. 
	 * [NB] We copy the names of the sections because the contents of 
	 * the buffer may change after we release the mutex. */
	if (mutex_lock_killable(&section_buffer_mutex) != 0)
	{
		pr_warning(KEDR_MSG_PREFIX "kedr_get_sections: "
			"got a signal while trying to acquire a mutex.\n");
		ret = -EINTR;
		goto out;
	}
	
	ret = parse_section_data(mod, sections);
	if (ret != 0) {
		pr_warning(KEDR_MSG_PREFIX
		"Failed to parse section data for \"%s\" module.\n",
			target_name);
		pr_warning(KEDR_MSG_PREFIX
		"The buffer contains the following: %s\n",
			section_buffer);
		kedr_release_sections(sections); /* just in case */
		goto out_unlock;
	}
	
	if (list_empty(sections)) {
		pr_warning(KEDR_MSG_PREFIX
		"No section information found for \"%s\" module.\n",
			target_name);
		ret = -EINVAL;
		goto out_unlock;
	}
	
	mutex_unlock(&section_buffer_mutex);
	mutex_unlock(&section_mutex);
	return 0;

out_unlock:
	mutex_unlock(&section_buffer_mutex);
out:
	mutex_unlock(&section_mutex);
	return ret;
}

void
kedr_release_sections(struct list_head *sections)
{
	struct kedr_section *sec;
	struct kedr_section *tmp;
	
	list_for_each_entry_safe(sec, tmp, sections, list) {
		list_del(&sec->list);
		kedr_section_destroy(sec);
	}
}
/* ====================================================================== */
