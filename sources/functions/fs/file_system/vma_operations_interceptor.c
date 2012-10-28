#include "vma_operations_interceptor.h"
#include "vma_operations_clone_interceptor.h"

#include <linux/fs.h> /* struct file_operations */

/* Determine lifetime of vma object clone from its operation */
static void vma_operations_open_post_vma_clone_lifetime(
    struct vm_area_struct* vma,
    struct kedr_coi_operation_call_info* call_info)
{
    vma_operations_clone_interceptor_watch(vma, vma->vm_file, &vma->vm_ops);
}


/* Determine lifetime of vma object from its operation */
static void vma_operations_close_post_vma_lifetime(
    struct vm_area_struct* vma,
    struct kedr_coi_operation_call_info* call_info)
{
    vma_operations_interceptor_internal_forget(vma);
}



/* Determine lifetime of vma object clone from its operation */
static void vma_operations_close_post_vma_clone_lifetime(
    struct vm_area_struct* vma,
    struct kedr_coi_operation_call_info* call_info)
{
    vma_operations_clone_interceptor_forget(vma, &vma->vm_ops);
}


static struct kedr_coi_post_handler vma_operations_post_handlers[] =
{
    vma_operations_open_post_external(vma_operations_open_post_vma_clone_lifetime),
    vma_operations_close_post_external(vma_operations_close_post_vma_lifetime),
    vma_operations_close_post_external(vma_operations_close_post_vma_clone_lifetime),
    kedr_coi_post_handler_end
};

static struct kedr_coi_payload vma_operations_payload =
{
    .mod = THIS_MODULE,
    .post_handlers = vma_operations_post_handlers
};

/* Determine lifetime of vma object from file operation */
static void file_operations_mmap_post_vma_lifetime(
    struct file* filp, struct vm_area_struct* vma, int returnValue,
    struct kedr_coi_operation_call_info* call_info)
{
    if(returnValue == 0)
    {
        vma_operations_interceptor_internal_watch(vma);
    }
}

/* Determine lifetime of vma object cloning from file operation */
static void file_operations_mmap_post_vma_clone_lifetime(
    struct file* filp, struct vm_area_struct* vma, int returnValue,
    struct kedr_coi_operation_call_info* call_info)
{
    if(returnValue == 0)
    {
        vma_operations_clone_interceptor_watch(vma, filp, &vma->vm_ops);
    }
}

static struct kedr_coi_post_handler file_operations_post_handlers[] =
{
    { 
		offsetof(struct file_operations, mmap),
		(void*)&file_operations_mmap_post_vma_lifetime
	},
	{ 
		offsetof(struct file_operations, mmap),
		(void*)&file_operations_mmap_post_vma_clone_lifetime
	},
    kedr_coi_post_handler_end
};

static struct kedr_coi_payload file_operations_payload =
{
    .mod = THIS_MODULE,
    .post_handlers = file_operations_post_handlers
};

/* Initialize everything except connection to the file. */
static int interceptor_init(void)
{
	int result;

	result = vma_operations_interceptor_internal_init(NULL);
	if(result) goto interceptor_err;
	
	result = vma_operations_clone_interceptor_init(
		vma_operations_interceptor_internal_creation_interceptor_create, NULL);
	if(result) goto clone_interceptor_err;

	result = vma_operations_interceptor_internal_payload_register(
		&vma_operations_payload);
	if(result) goto payload_err;
	
	return 0;

payload_err:
	vma_operations_clone_interceptor_destroy();
clone_interceptor_err:
	vma_operations_interceptor_internal_destroy();
interceptor_err:
	return result;
}

/* 
 * Destroy everything except connection to the file.
 * 
 * Called after deregistering payload for file operations interceptor,
 * so shouldn't fail.
 */
static void interceptor_destroy(void)
{
	vma_operations_interceptor_internal_payload_unregister(
		&vma_operations_payload);
	vma_operations_clone_interceptor_destroy();
	vma_operations_interceptor_internal_destroy();
}


/* 
 * Initialize interceptor for vma operations and connect it
 * to the interceptor for file operations.
 * 
 * 'file_interceptor' should be interceptor for file operations.
 */
int vma_operations_interceptor_register(struct kedr_coi_interceptor* file_interceptor)
{
	int result;

	result = interceptor_init();
	if(result) return result;
	
	result = kedr_coi_payload_register(file_interceptor,
		&file_operations_payload);
	if(result)
	{
		interceptor_destroy();
		return result;
	}
	
	return 0;
}
/* 
 * Disconnect interceptor for vma operations from interceptor for file
 * operations and destroy former.
 * 
 * 'file_interceptor' should be same as one in register() function.
 */
int vma_operations_interceptor_unregister(struct kedr_coi_interceptor* file_interceptor)
{
	int result = kedr_coi_payload_unregister(file_interceptor,
		&file_operations_payload);
	if(result) return result;
	
	interceptor_destroy();
	return 0;
}

/* 
 * Same as register() and unregister(), but for generated interceptor
 * for file operations.
 */
int vma_operations_interceptor_connect(
	int (*file_payload_register)(struct kedr_coi_payload* payload))
{
	int result;

	result = interceptor_init();
	if(result) return result;
	
	result = file_payload_register(&file_operations_payload);
	if(result)
	{
		interceptor_destroy();
		return result;
	}
	
	return 0;
}
int vma_operations_interceptor_disconnect(
	int (*file_payload_unregister)(struct kedr_coi_payload* payload))
{
	int result = file_payload_unregister(&file_operations_payload);
	if(result) return result;
	
	interceptor_destroy();
	return 0;
}

int vma_operations_interceptor_payload_register(struct kedr_coi_payload* payload)
{
	return vma_operations_interceptor_internal_payload_register(payload);
}
int vma_operations_interceptor_payload_unregister(struct kedr_coi_payload* payload)
{
	return vma_operations_interceptor_internal_payload_unregister(payload);
}

int vma_operations_interceptor_start(void)
{
	return vma_operations_interceptor_internal_start();
}
int vma_operations_interceptor_stop(void)
{
	return vma_operations_interceptor_internal_stop();
}