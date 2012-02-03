/* API for core stub, additional to one contained in core_api.h */
#ifndef CORE_STUB_API_H
#define CORE_STUB_API_H

#include "kedr/kedr_mem/core_api.h"

/* 
 * Wrappers around event handler callbacks.
 */
void stub_on_target_loaded(struct module* m);
void stub_on_target_about_to_unload(struct module* m);

void stub_on_function_entry(unsigned long tid, unsigned long func);
void stub_on_function_exit(unsigned long tid, unsigned long func);

void stub_on_call_pre(unsigned long tid, unsigned long pc,
    unsigned long func);
void stub_on_call_post(unsigned long tid, unsigned long pc,
    unsigned long func);

void stub_begin_memory_events(unsigned long tid, unsigned long num_events, 
    void **pdata /* out param*/);
void stub_end_memory_events(unsigned long tid, void *data);
void stub_on_memory_event(unsigned long tid, 
    unsigned long pc, unsigned long addr, unsigned long size, 
    enum kedr_memory_event_type type,
    void *data);

void stub_on_locked_op_pre(unsigned long tid, unsigned long pc, 
    void **pdata);
void stub_on_locked_op_post(unsigned long tid, unsigned long pc, 
    unsigned long addr, unsigned long size, 
    enum kedr_memory_event_type type, void *data);

void stub_on_io_mem_op_pre(unsigned long tid, unsigned long pc, 
    void **pdata);
void stub_on_io_mem_op_post(unsigned long tid, unsigned long pc, 
    unsigned long addr, unsigned long size, 
    enum kedr_memory_event_type type, void *data);

void stub_on_memory_barrier_pre(unsigned long tid, unsigned long pc, 
    enum kedr_barrier_type type);
void stub_on_memory_barrier_post(unsigned long tid, unsigned long pc, 
    enum kedr_barrier_type type);

void stub_on_alloc_pre(unsigned long tid, unsigned long pc, 
    unsigned long size);
void stub_on_alloc_post(unsigned long tid, unsigned long pc, 
    unsigned long size, unsigned long addr);
void stub_on_free_pre(unsigned long tid, unsigned long pc, 
    unsigned long addr);
void stub_on_free_post(unsigned long tid, unsigned long pc, 
    unsigned long addr);

void stub_on_lock_pre(unsigned long tid, unsigned long pc, 
    unsigned long lock_id, enum kedr_lock_type type);
void stub_on_lock_post(unsigned long tid, unsigned long pc, 
    unsigned long lock_id, enum kedr_lock_type type);
void stub_on_unlock_pre(unsigned long tid, unsigned long pc, 
    unsigned long lock_id, enum kedr_lock_type type);
void stub_on_unlock_post(unsigned long tid, unsigned long pc, 
    unsigned long lock_id, enum kedr_lock_type type);

void stub_on_signal_pre(unsigned long tid, unsigned long pc, 
    unsigned long obj_id, enum kedr_sw_object_type type);
void stub_on_signal_post(unsigned long tid, unsigned long pc, 
    unsigned long obj_id, enum kedr_sw_object_type type);
void stub_on_wait_pre(unsigned long tid, unsigned long pc, 
    unsigned long obj_id, enum kedr_sw_object_type type);
void stub_on_wait_post(unsigned long tid, unsigned long pc, 
	unsigned long obj_id, enum kedr_sw_object_type type);

void stub_on_thread_create_pre(unsigned long tid, unsigned long pc);
void stub_on_thread_create_post(unsigned long tid, unsigned long pc, 
    unsigned long child_tid);
void stub_on_thread_join_pre(unsigned long tid, unsigned long pc, 
    unsigned long child_tid);
void stub_on_thread_join_post(unsigned long tid, unsigned long pc, 
    unsigned long child_tid);


#endif /* CORE_STUB_API_H */