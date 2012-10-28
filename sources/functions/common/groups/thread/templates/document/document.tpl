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

#include <kedr/kedr_mem/core_api.h>
#include <kedr/kedr_mem/local_storage.h>
#include <kedr/kedr_mem/functions.h>
#include <kedr/object_types.h>

#ifndef KEDR_ARG_DEFINED
static inline unsigned long kedr_arg(struct kedr_local_storage *ls, int n)
{
	if(n == 1)
		return KEDR_LS_ARG1(ls);
	else if(n == 2)
		return KEDR_LS_ARG2(ls);
	else if(n == 3)
		return KEDR_LS_ARG3(ls);
	else if(n == 4)
		return KEDR_LS_ARG4(ls);
	else if(n == 5)
		return KEDR_LS_ARG5(ls);
	else if(n == 6)
		return KEDR_LS_ARG6(ls);
	else if(n == 7)
		return KEDR_LS_ARG7(ls);
	else if(n == 8)
		return KEDR_LS_ARG8(ls);
	else
		return *(unsigned long*)(0);// error
}

/* 
 * Helpers for generate events.
 * 
 * (Really, them should be defined by the core).
 */

/* Pattern for handlers wrappers */
#define GENERATE_HANDLER_CALL(handler_name, ...) do {               \
    struct kedr_event_handlers *eh = kedr_get_event_handlers();     \
    if(eh && eh->handler_name) eh->handler_name(eh, ##__VA_ARGS__); \
}while(0)

static inline void generate_signal_pre(unsigned long tid, unsigned long pc,
    unsigned long obj_id, enum kedr_sw_object_type type)
{
    GENERATE_HANDLER_CALL(on_signal_pre, tid, pc, obj_id, type);
}

static inline void generate_signal_post(unsigned long tid, unsigned long pc,
    unsigned long obj_id, enum kedr_sw_object_type type)
{
    GENERATE_HANDLER_CALL(on_signal_post, tid, pc, obj_id, type);
}

static inline void generate_signal(unsigned long tid, unsigned long pc,
    unsigned long obj_id, enum kedr_sw_object_type type)
{
    generate_signal_pre(tid, pc, obj_id, type);
    generate_signal_post(tid, pc, obj_id, type);
}

static inline void generate_wait_pre(unsigned long tid, unsigned long pc,
    unsigned long obj_id, enum kedr_sw_object_type type)
{
    GENERATE_HANDLER_CALL(on_wait_pre, tid, pc, obj_id, type);
}

static inline void generate_wait_post(unsigned long tid, unsigned long pc,
    unsigned long obj_id, enum kedr_sw_object_type type)
{
    GENERATE_HANDLER_CALL(on_wait_post, tid, pc, obj_id, type);
}

static inline void generate_wait(unsigned long tid, unsigned long pc,
    unsigned long obj_id, enum kedr_sw_object_type type)
{
    generate_wait_pre(tid, pc, obj_id, type);
    generate_wait_post(tid, pc, obj_id, type);
}


static inline void generate_alloc_pre(unsigned long tid, unsigned long pc,
    unsigned long size)
{
    GENERATE_HANDLER_CALL(on_alloc_pre, tid, pc, size);
}

static inline void generate_alloc_post(unsigned long tid, unsigned long pc,
    unsigned long size, unsigned long pointer)
{
    GENERATE_HANDLER_CALL(on_alloc_post, tid, pc, size, pointer);
}

static inline void generate_alloc(unsigned long tid, unsigned long pc,
    unsigned long size, unsigned long pointer)
{
    generate_alloc_pre(tid, pc, size);
    generate_alloc_post(tid, pc, size, pointer);
}


static inline void generate_free_pre(unsigned long tid, unsigned long pc,
    unsigned long pointer)
{
    GENERATE_HANDLER_CALL(on_free_pre, tid, pc, pointer);
}

static inline void generate_free_post(unsigned long tid, unsigned long pc,
    unsigned long pointer)
{
    GENERATE_HANDLER_CALL(on_free_post, tid, pc, pointer);
}

static inline void generate_free(unsigned long tid, unsigned long pc,
    unsigned long pointer)
{
    generate_free_pre(tid, pc, pointer);
    generate_free_post(tid, pc, pointer);
}

static inline void generate_tcreate_pre(unsigned long tid, unsigned long pc)
{
    GENERATE_HANDLER_CALL(on_thread_create_pre, tid, pc);
}

static inline void generate_tcreate_post(unsigned long tid, unsigned long pc,
	unsigned long child_tid)
{
    GENERATE_HANDLER_CALL(on_thread_create_post, tid, pc, child_tid);
}

static inline void generate_tjoin_pre(unsigned long tid, unsigned long pc,
	unsigned long child_tid)
{
    GENERATE_HANDLER_CALL(on_thread_join_pre, tid, pc, child_tid);
}

static inline void generate_tjoin_post(unsigned long tid, unsigned long pc,
	unsigned long child_tid)
{
    GENERATE_HANDLER_CALL(on_thread_join_post, tid, pc, child_tid);
}


#define KEDR_ARG_DEFINED
#endif /* KEDR_ARG_DEFINED*/

<$if concat(header)$><$header: join(\n)$><$endif$>
/* ====================================================================== */

<$if concat(function.name)$><$block : join(\n\n)$>
<$endif$>/* ====================================================================== */
