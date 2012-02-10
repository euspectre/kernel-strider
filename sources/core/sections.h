/* sections.h - API to search for the section addresses of a loaded kernel
 * module. */
#ifndef SECTIONS_H_1141_INCLUDED
#define SECTIONS_H_1141_INCLUDED

#include <linux/list.h>

struct dentry;
struct module;

/* Initialize the subsystem. This function should be called during the 
 * initialization of the module. 
 * The function may create files in debugfs, so the directory for our system
 * should be created in there before the function is called. 
 * The corresponding dentry should be passed to the function as a parameter.
 *
 * The caller must ensure that no other function from this subsystem is 
 * called before kedr_init_section_subsystem() finishes. */
int 
kedr_init_section_subsystem(struct dentry *debugfs_dir);

/* Cleanup the subsystem. This function should be called during the cleanup
 * of the module, before the directory for our system is removed from 
 * debugfs. 
 * 
 * The caller must ensure that no other function from this sybsystem is 
 * called after kedr_cleanup_section_subsystem() starts. */
void
kedr_cleanup_section_subsystem(void);

/* Information about an ELF section of a loaded module. */
struct kedr_section
{
	struct list_head list;
	
	/* Name of the section. */
	char *name;
	
	/* The address of the section in memory. Note that it is the address
	 * where the section was placed when the target module was loaded.
	 * The section may have been dropped from memory since that time but
	 * the address remains the same. */
	unsigned long addr;
};

/* Finds the loaded ELF sections of the given kernel module.
 * 'sections' list must be empty when passed to this function.
 * If successful, the function creates the appropriate instances of 
 * struct kedr_section, adds them to 'sections' list and returns 0. 
 * In case of a failure, -errno is returned and 'sections' list will remain
 * unchanged (i.e. empty). 
 * 
 * The list of sections is not intended to be modified. When it is no longer
 * needed, call kedr_release_sections() for it. */
int
kedr_get_sections(struct module *mod, struct list_head *sections);

/* Empty the list and properly delete the elements it contains. */
void
kedr_release_sections(struct list_head *sections);

#endif // SECTIONS_H_1141_INCLUDED
