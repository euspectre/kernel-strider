/* Utilities for the FH plugins provided with KernelStrider. */
#ifndef FH_PLUGIN_H_1644_INCLUDED
#define FH_PLUGIN_H_1644_INCLUDED

#include <linux/list.h>
#include <kedr/kedr_mem/functions.h>

/* Each group submits such structure to the plugin. */
struct kedr_fh_group
{
	struct list_head list;
	
	/* The array of pointers to the handler structures 
	 * and its size. */
	struct kedr_fh_handlers **handlers;
	unsigned int num_handlers;
	
	/* If not NULL, this function will be called from the exit
	 * function of the plugin. */
	void (*cleanup)(void);
};

/* Each group must define the following function:
 * struct kedr_fh_group *kedr_fh_get_group_<group_name>(void)
 * The function should return the filled struct kedr_fh_group instance
 * for the given group.
 * 
 * For each group, 'KEDR_FH_DECLARE_GROUP(group_name);' must be 
 * placed somewhere in the main file of the plugin and 
 * 'KEDR_FH_ADD_GROUP(group_name, group_list);' - in its init function
 * before preparing the data needed to register the plugin. 
 * The latter macro obtains the struct kedr_fh_group instance for the 
 * group and adds it to the list 'group_list' (struct list_head). */
#define KEDR_FH_DECLARE_GROUP(group_name) \
struct kedr_fh_group * \
kedr_fh_get_group_##group_name(void)
	
#define KEDR_FH_ADD_GROUP(group_name, group_list) do { \
	struct kedr_fh_group * grp;	\
	grp = kedr_fh_get_group_##group_name();	\
	list_add(&grp->list, &(group_list)); \
} while (0)

/* Helper function. 
 * Allocates an array of appropriate size and copies the pointers to the 
 * handlers defined by each group to it. The array will be NULL-terminated.
 * The function returns the created array if successful, NULL if it failed 
 * to allocate the needed amount of memory. */
struct kedr_fh_handlers **
kedr_fh_combine_handlers(struct list_head *groups);

/* Helper function. 
 * For each function group in the list that has defined a cleanup 
 * function, calls that function. */
void
kedr_fh_do_cleanup_calls(struct list_head *groups);
#endif /* FH_PLUGIN_H_1644_INCLUDED */
