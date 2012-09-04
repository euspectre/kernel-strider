#include <kedr/kedr_mem/core_api.h>

#include <linux/slab.h> /* kmalloc */
#include <linux/errno.h> /* EINVAL, ENOMEM,...*/
#include <linux/string.h> /* memcpy, memset */
#include <linux/module.h> /* EXPORT_SYMBOL */

/* 
 * Use per-cpu array(!) of pointers for store states in
 * execution_event_memory_accesses_begin().
 */
#include <linux/percpu.h>

/* Since 2.6.33 __percpu attribute is used for per cpu variables. */
#ifndef __percpu
#define __percpu
#endif


extern int kedr_register_event_handlers_internal(struct kedr_event_handlers* eh);
extern void kedr_unregister_event_handlers_internal(struct kedr_event_handlers* eh);

struct event_handlers_wrapper
{
	struct kedr_event_handlers handlers;
	
	/* Array of wrapped handlers */
	struct kedr_event_handlers** eh_array;
	int eh_array_size;
};

/* 
 * Whether all wrapped handlers was successfully fixed 
 * when target module is loaded.
 * 
 * Used only when target module is loaded.
 */
static int handlers_are_used;

/* 
 * Combine all 'data' for memory operations callbacks
 * from handlers into one structure.
 * 
 * Pointer to that structure will be used as 'data' for memory operations
 * callbacks in wrapper handler.
 */
struct ma_data_wrapper
{
	/* For use per-cpu pre allocated structures */
	unsigned long flags;
	void* eh_data[0];
};

/* 
 * Pre allocated structures with wrapped data, for each cpu.
 * 
 * Used only when target module is loaded and all handlers are successfully fixed.
 */
struct ma_data_wrapper* __percpu eh_data_array;

static void wrapper_on_target_loaded(struct kedr_event_handlers *eh, 
	struct module *target_module)
{
	struct event_handlers_wrapper* wrapper = container_of(eh,
		typeof(*wrapper), handlers);
	
	int i;
	for(i = 0; i < wrapper->eh_array_size; i++)
	{
		struct kedr_event_handlers* tmp = wrapper->eh_array[i];
		if(tmp->owner && (try_module_get(tmp->owner) == 0))
		{
			pr_err("Failed to fix module(via try_module_get) with event handlers.");
			pr_err("All event handlers will be disabled during this target session.");
			goto err;
		}
	}
	/* Allocate array of 'void*' */
	eh_data_array = (typeof(struct ma_data_wrapper) __percpu*)__alloc_percpu(
		sizeof(struct ma_data_wrapper) + sizeof(void*) * wrapper->eh_array_size,
		__alignof__(struct ma_data_wrapper));
	
	if(eh_data_array == NULL)
	{
		pr_err("Failed allocate per-cpu array of pointers.");
		goto err;
	}

	handlers_are_used = 1;
	

	for(i = 0; i < wrapper->eh_array_size; i++)
	{
		struct kedr_event_handlers* tmp = wrapper->eh_array[i];
		if(tmp->on_target_loaded)
			tmp->on_target_loaded(tmp, target_module);
	}

	return;

err:
	/* Rollback try_module_get for previously fixed handlers*/
	for(--i; i >= 0; --i)
	{
		struct kedr_event_handlers* tmp1 = wrapper->eh_array[i];
		if(tmp1->owner) module_put(tmp1->owner);
	}
	
	handlers_are_used = 0;
	/* Wrapper will do nothing */
	return;

}
static void wrapper_on_target_about_to_unload(struct kedr_event_handlers *eh, 
	struct module *target_module)
{
	int i;
	struct event_handlers_wrapper* wrapper = container_of(eh,
		typeof(*wrapper), handlers);

	if(!handlers_are_used) return;

	free_percpu(eh_data_array);

	for(i = 0; i < wrapper->eh_array_size; i++)
	{
		struct kedr_event_handlers* tmp = wrapper->eh_array[i];
		if(tmp->on_target_about_to_unload)
			tmp->on_target_about_to_unload(tmp, target_module);
	}

	for(i = 0; i < wrapper->eh_array_size; i++)
	{
		struct kedr_event_handlers* tmp = wrapper->eh_array[i];
		if(tmp->owner) module_put(tmp->owner);
	}
}

static void wrapper_begin_memory_events(struct kedr_event_handlers *eh, 
	unsigned long tid, unsigned long num_events, 
	void **pdata)
{
	int i;
	struct event_handlers_wrapper* wrapper = container_of(eh,
		typeof(*wrapper), handlers);
	int cpu;
	struct ma_data_wrapper* data_wrapper;
	
	if(!handlers_are_used) return;

	/* 
	 * Memory events-related callbacks should work correctly
	 * with disabled preemption.
	 */
	cpu = get_cpu();

	data_wrapper = per_cpu_ptr(eh_data_array, cpu);

	/* Before using element of data_array, disable IRQs. */
	local_irq_save(data_wrapper->flags);
	
	for(i = 0; i < wrapper->eh_array_size; i++)
	{
		struct kedr_event_handlers* tmp = wrapper->eh_array[i];
		if(tmp->begin_memory_events)
			tmp->begin_memory_events(tmp, tid, num_events,
				&data_wrapper->eh_data[i]);

	}
	
	*pdata = data_wrapper;
}
static void wrapper_end_memory_events(struct kedr_event_handlers *eh, 
	unsigned long tid, void *data)
{
	int i;
	struct event_handlers_wrapper* wrapper = container_of(eh,
		typeof(*wrapper), handlers);

	struct ma_data_wrapper* data_wrapper;
	
	if(!handlers_are_used) return;
	
	data_wrapper = (struct ma_data_wrapper*)data;
	
	for(i = 0; i < wrapper->eh_array_size; i++)
	{
		struct kedr_event_handlers* tmp = wrapper->eh_array[i];
		if(tmp->end_memory_events)
			tmp->end_memory_events(tmp, tid, data_wrapper->eh_data[i]);

	}

	/* Enable IRQs after element of data_array become unused. */
	local_irq_restore(data_wrapper->flags);

	put_cpu();
}
static void wrapper_on_memory_event(struct kedr_event_handlers *eh, 
	unsigned long tid, 
	unsigned long pc, unsigned long addr, unsigned long size, 
	enum kedr_memory_event_type type,
	void *data)
{
	int i;
	struct event_handlers_wrapper* wrapper = container_of(eh,
		typeof(*wrapper), handlers);

	struct ma_data_wrapper* data_wrapper;
	
	if(!handlers_are_used) return;
	
	data_wrapper = (struct ma_data_wrapper*)data;
	
	for(i = 0; i < wrapper->eh_array_size; i++)
	{
		struct kedr_event_handlers* tmp = wrapper->eh_array[i];
		if(tmp->on_memory_event)
			tmp->on_memory_event(tmp, tid, pc, addr, size, type,
				data_wrapper->eh_data[i]);
	}
}

/* Call callback for each event handler in wrapper corresponded to 'eh'.*/

#define each_callback(eh, callback_name, ...) 							\
struct event_handlers_wrapper* wrapper = container_of(eh, 				\
	typeof(*wrapper), handlers); 										\
int i;																	\
if(handlers_are_used) for(i = 0; i < wrapper->eh_array_size; i++)		\
{																		\
	struct kedr_event_handlers* tmp = wrapper->eh_array[i];				\
	if(tmp->callback_name)												\
		tmp->callback_name(tmp, ##__VA_ARGS__);							\
}


static void wrapper_on_function_entry(struct kedr_event_handlers *eh, 
	unsigned long tid, unsigned long func)
{
	each_callback(eh, on_function_entry, tid, func);
}
static void wrapper_on_function_exit(struct kedr_event_handlers *eh, 
	unsigned long tid, unsigned long func)
{
	each_callback(eh, on_function_exit, tid, func);
}


static void wrapper_on_call_pre(struct kedr_event_handlers *eh, 
	unsigned long tid, unsigned long pc, unsigned long func)
{
	each_callback(eh, on_call_pre, tid, pc, func);
}
static void wrapper_on_call_post(struct kedr_event_handlers *eh, 
	unsigned long tid, unsigned long pc, unsigned long func)
{
	each_callback(eh, on_call_post, tid, pc, func);
}

static void wrapper_on_locked_op_pre(struct kedr_event_handlers *eh, 
	unsigned long tid, unsigned long pc, 
	void **pdata)
{
	each_callback(eh, on_locked_op_pre, tid, pc, pdata);
}

static void wrapper_on_locked_op_post(struct kedr_event_handlers *eh, 
	unsigned long tid, unsigned long pc, 
	unsigned long addr, unsigned long size, 
	enum kedr_memory_event_type type, void *data)
{
	each_callback(eh, on_locked_op_post, tid, pc, addr, size, type, data);
}

static void wrapper_on_io_mem_op_pre(struct kedr_event_handlers *eh, 
	unsigned long tid, unsigned long pc, 
	void **pdata)
{
	each_callback(eh, on_io_mem_op_pre, tid, pc, pdata);
}

static void wrapper_on_io_mem_op_post(struct kedr_event_handlers *eh, 
	unsigned long tid, unsigned long pc, 
	unsigned long addr, unsigned long size, 
	enum kedr_memory_event_type type, void *data)
{
	each_callback(eh, on_io_mem_op_post, tid, pc, addr, size, type, data);
}

static void wrapper_on_memory_barrier_pre(struct kedr_event_handlers *eh, 
	unsigned long tid, unsigned long pc, 
	enum kedr_barrier_type type)
{
	each_callback(eh, on_memory_barrier_pre, tid, pc, type);
}

static void wrapper_on_memory_barrier_post(struct kedr_event_handlers *eh, 
	unsigned long tid, unsigned long pc, 
	enum kedr_barrier_type type)
{
	each_callback(eh, on_memory_barrier_post, tid, pc, type);
}

static void wrapper_on_alloc_pre(struct kedr_event_handlers *eh, 
	unsigned long tid, unsigned long pc, 
	unsigned long size)
{
	each_callback(eh, on_alloc_pre, tid, pc, size);
}
static void wrapper_on_alloc_post(struct kedr_event_handlers *eh, 
	unsigned long tid, unsigned long pc, 
	unsigned long size, unsigned long addr)
{
	each_callback(eh, on_alloc_post, tid, pc, size, addr);
}

static void wrapper_on_free_pre(struct kedr_event_handlers *eh, 
	unsigned long tid, unsigned long pc, 
	unsigned long addr)
{
	each_callback(eh, on_free_pre, tid, pc, addr);
}
static void wrapper_on_free_post(struct kedr_event_handlers *eh, 
	unsigned long tid, unsigned long pc, 
	unsigned long addr)
{
	each_callback(eh, on_free_post, tid, pc, addr);
}

static void wrapper_on_lock_pre(struct kedr_event_handlers *eh, 
	unsigned long tid, unsigned long pc, 
	unsigned long lock_id, enum kedr_lock_type type)
{
	each_callback(eh, on_lock_pre, tid, pc, lock_id, type);
}
static void wrapper_on_lock_post(struct kedr_event_handlers *eh, 
	unsigned long tid, unsigned long pc, 
	unsigned long lock_id, enum kedr_lock_type type)
{
	each_callback(eh, on_lock_post, tid, pc, lock_id, type);
}

static void wrapper_on_unlock_pre(struct kedr_event_handlers *eh, 
	unsigned long tid, unsigned long pc, 
	unsigned long lock_id, enum kedr_lock_type type)
{
	each_callback(eh, on_unlock_pre, tid, pc, lock_id, type);
}
static void wrapper_on_unlock_post(struct kedr_event_handlers *eh, 
	unsigned long tid, unsigned long pc, 
	unsigned long lock_id, enum kedr_lock_type type)
{
	each_callback(eh, on_unlock_post, tid, pc, lock_id, type);
}

static void wrapper_on_signal_pre(struct kedr_event_handlers *eh, 
	unsigned long tid, unsigned long pc, 
	unsigned long obj_id, enum kedr_sw_object_type type)
{
	each_callback(eh, on_signal_pre, tid, pc, obj_id, type);
}
static void wrapper_on_signal_post(struct kedr_event_handlers *eh, 
	unsigned long tid, unsigned long pc, 
	unsigned long obj_id, enum kedr_sw_object_type type)
{
	each_callback(eh, on_signal_post, tid, pc, obj_id, type);
}
static void wrapper_on_wait_pre(struct kedr_event_handlers *eh, 
	unsigned long tid, unsigned long pc, 
	unsigned long obj_id, enum kedr_sw_object_type type)
{
	each_callback(eh, on_wait_pre, tid, pc, obj_id, type);
}
static void wrapper_on_wait_post(struct kedr_event_handlers *eh, 
	unsigned long tid, unsigned long pc, 
	unsigned long obj_id, enum kedr_sw_object_type type)
{
	each_callback(eh, on_wait_post, tid, pc, obj_id, type);
}

static void wrapper_on_thread_create_pre(struct kedr_event_handlers *eh, 
	unsigned long tid, unsigned long pc)
{
	each_callback(eh, on_thread_create_pre, tid, pc);
}
static void wrapper_on_thread_create_post(struct kedr_event_handlers *eh, 
	unsigned long tid, unsigned long pc, 
	unsigned long child_tid)
{
	each_callback(eh, on_thread_create_post, tid, pc, child_tid);
}
static void wrapper_on_thread_join_pre(struct kedr_event_handlers *eh, 
	unsigned long tid, unsigned long pc, 
	unsigned long child_tid)
{
	each_callback(eh, on_thread_join_pre, tid, pc, child_tid);
}
static void wrapper_on_thread_join_post(struct kedr_event_handlers *eh,
	unsigned long tid, unsigned long pc, 
	unsigned long child_tid)
{
	each_callback(eh, on_thread_join_post, tid, pc, child_tid);
}

/* 
 * Accept event_handlers_wrapper structure with filled array of handlers.
 * Setup functions pointers in 'handlers' member
 */
void event_handlers_wrapper_set_functions(struct event_handlers_wrapper* wrapper)
{
	int i;
	
	/* callbacks are set only when at least one wrapped handler has them*/
	for(i = 0; i < wrapper->eh_array_size; i++)
	{
		struct kedr_event_handlers* tmp = wrapper->eh_array[i];

#define set_callback(callback_name) if(tmp->callback_name) \
wrapper->handlers.callback_name = &wrapper_##callback_name
	
		set_callback(on_function_entry);
		set_callback(on_function_exit);
		set_callback(on_call_pre);
		set_callback(on_call_post);
		set_callback(begin_memory_events);
		set_callback(end_memory_events);
		set_callback(on_memory_event);

		set_callback(on_locked_op_pre);
		set_callback(on_locked_op_post);
		
		set_callback(on_io_mem_op_pre);
		set_callback(on_io_mem_op_post);
		
		set_callback(on_memory_barrier_pre);
		set_callback(on_memory_barrier_post);
		
		set_callback(on_alloc_pre);
		set_callback(on_alloc_post);
		
		set_callback(on_free_pre);
		set_callback(on_free_post);

		set_callback(on_lock_pre);
		set_callback(on_lock_post);

		set_callback(on_unlock_pre);
		set_callback(on_unlock_post);

		set_callback(on_signal_pre);
		set_callback(on_signal_post);
		set_callback(on_wait_pre);
		set_callback(on_wait_post);

		set_callback(on_thread_create_pre);
		set_callback(on_thread_create_post);
		set_callback(on_thread_join_pre);
		set_callback(on_thread_join_post);

#undef set_callback
	}
	
	/* begin_memory_events and end_memory_events should be wrapped in pair */
	if(wrapper->handlers.begin_memory_events || wrapper->handlers.end_memory_events)
	{
		wrapper->handlers.begin_memory_events = &wrapper_begin_memory_events;
		wrapper->handlers.end_memory_events = &wrapper_end_memory_events;
	}
}

/* Alloc wrapper for n handlers and setup common fields */
static struct event_handlers_wrapper* event_handlers_wrapper_alloc(int n)
{
	struct event_handlers_wrapper* wrapper = kmalloc(sizeof(*wrapper), GFP_KERNEL);
	if(wrapper == NULL)
	{
		pr_err("Failed to allocate wrapper structure.");
		return NULL;
	}
	
	wrapper->eh_array = kmalloc(sizeof(*wrapper->eh_array) * n, GFP_KERNEL);
	if(wrapper->eh_array == NULL)
	{
		pr_err("Failed to allocate array of pointer to wrapped handlers.");
		kfree(wrapper);
		return NULL;
	}
	
	wrapper->eh_array_size = n;
	
	memset(&wrapper->handlers, 0, sizeof(wrapper->handlers));
	
	wrapper->handlers.owner = THIS_MODULE;
	
	/* These callbacks should be set in any case */
	wrapper->handlers.on_target_loaded = &wrapper_on_target_loaded;
	wrapper->handlers.on_target_about_to_unload = &wrapper_on_target_about_to_unload;

	return wrapper;
}

static void event_handlers_wrapper_free(struct event_handlers_wrapper* wrapper)
{
	kfree(wrapper->eh_array);
	kfree(wrapper);
}

//TODO: protect handlers registration/deregistration with mutex.

static struct event_handlers_wrapper* current_wrapper = NULL;

int kedr_register_event_handlers(struct kedr_event_handlers* eh)
{
	struct event_handlers_wrapper* wrapper;
	int result;
	
	if(current_wrapper == NULL)
	{
		wrapper = event_handlers_wrapper_alloc(1);
		if(wrapper == NULL) return -ENOMEM;
		
		wrapper->eh_array[0] = eh;
		
		event_handlers_wrapper_set_functions(wrapper);
		
		result = kedr_register_event_handlers_internal(&wrapper->handlers);
		if(result)
		{
			event_handlers_wrapper_free(wrapper);
			return result;
		}
		
		current_wrapper = wrapper;
	}
	else
	{
		wrapper = event_handlers_wrapper_alloc(current_wrapper->eh_array_size + 1);
		if(wrapper == NULL) return -ENOMEM;
		
		memcpy(wrapper->eh_array, current_wrapper->eh_array,
			sizeof(eh) * current_wrapper->eh_array_size);
		
		wrapper->eh_array[current_wrapper->eh_array_size] = eh;
		
		event_handlers_wrapper_set_functions(wrapper);
		
		kedr_unregister_event_handlers_internal(&current_wrapper->handlers);
		event_handlers_wrapper_free(current_wrapper);
		
		result = kedr_register_event_handlers_internal(&wrapper->handlers);
		
		if(result)
		{
			/* 
			 * NOTE: Error occures, but we cannot revert to initial state.
			 * 
			 * This situation is possible if target module is loaded between
			 * unregistration of old event handlers and registration of
			 * new ones.
			 */
			pr_err("Attempt to register additional event handler leads to "
				"unregistering all event handlers.");
			event_handlers_wrapper_free(wrapper);
			
			current_wrapper = NULL;
			
			return result;
		}
		
		current_wrapper = wrapper;
	}
	
	return 0;
}
EXPORT_SYMBOL(kedr_register_event_handlers);

void 
kedr_unregister_event_handlers(struct kedr_event_handlers *eh)
{
	struct event_handlers_wrapper* wrapper;
	int result;
	
	int i;
	int index;
	
	if(current_wrapper == NULL)
	{
		pr_err("Attempt to unregister event handler while it is not registered.");
		return;
	}
	
	for(index = 0; index < current_wrapper->eh_array_size; index++)
	{
		if(current_wrapper->eh_array[index] == eh) break;
	}
		
	if(index == current_wrapper->eh_array_size)
	{
		pr_err("Attempt to unregister event handler while it is not registered.");
		return;
	}

	if(current_wrapper->eh_array_size == 1)
	{
		kedr_unregister_event_handlers_internal(&current_wrapper->handlers);
		event_handlers_wrapper_free(current_wrapper);
		current_wrapper = NULL;
		return;
	}
	
	/* Need to register new wrapper instead of old one */

	wrapper = event_handlers_wrapper_alloc(current_wrapper->eh_array_size - 1);
	if(wrapper == NULL)
	{
		/* 
		 * NOTE: Error occures, but we should unregister event handler
		 * in any case.
		 */
		
		kedr_unregister_event_handlers_internal(&current_wrapper->handlers);
		event_handlers_wrapper_free(current_wrapper);
		current_wrapper = NULL;
		
		pr_err("Attempt to unregister event handler leads to "
			"unregistering all other event handlers.");

		return;
	}
		
	for(i = 0; i < index; i++)
		wrapper->eh_array[i] = current_wrapper->eh_array[i];
	
	for(i = index; i < wrapper->eh_array_size; i++)
		wrapper->eh_array[i] = current_wrapper->eh_array[i + 1];
		
	event_handlers_wrapper_set_functions(wrapper);
		
	kedr_unregister_event_handlers_internal(&current_wrapper->handlers);
	event_handlers_wrapper_free(current_wrapper);

	result = kedr_register_event_handlers_internal(&wrapper->handlers);
	
	if(result)
	{
		/* 
		 * NOTE: Error occures, but we cannot revert to initial state.
		 * 
		 * This situation is possible if target module is loaded between
		 * deregistration old event handlers and registration new ones.
		 */
		pr_err("Attempt to unregister event handler leads to "
			"unregistering all other event handlers.");

		event_handlers_wrapper_free(wrapper);
		current_wrapper = NULL;
	}
		
	current_wrapper = wrapper;
}
EXPORT_SYMBOL(kedr_unregister_event_handlers);