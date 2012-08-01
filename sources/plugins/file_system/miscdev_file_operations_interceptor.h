#ifndef KEDR_COI_INTERCEPTOR_miscdev_file_operations_interceptor
#define KEDR_COI_INTERCEPTOR_miscdev_file_operations_interceptor

/* Wrapper around internal interceptor for simplify using*/

#include "miscdev_file_operations_interceptor_internal.h"

/* ========================================================================
 * Copyright (C) 2011, Andrey V. Tsyvarev  <tsyvarev@ispras.ru>
 *
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 ======================================================================== */

/* Interceptor miscdev_file_operations_interceptor */
static inline int miscdev_file_operations_interceptor_init(
    struct kedr_coi_factory_interceptor* (*factory_interceptor_create)(
        const char* name,
        size_t factory_operations_field_offset,
        const struct kedr_coi_factory_intermediate* intermediate_operations,
        void (*trace_unforgotten_factory)(void* factory)),
    void (*trace_unforgotten_object)(struct miscdevice* factory))
{
    int result = miscdev_file_operations_interceptor_internal_init(
        factory_interceptor_create, trace_unforgotten_object);
    if(result) return result;
    
    result = miscdev_list_init();
    if(result)
    {
        miscdev_file_operations_interceptor_internal_destroy();
        return result;
    }
    
    return 0;
}

static inline void miscdev_file_operations_interceptor_destroy(void)
{
    miscdev_list_destroy();
    miscdev_file_operations_interceptor_internal_destroy();
}

static inline int miscdev_file_operations_interceptor_watch(
    struct miscdevice *factory)
{
    int result = miscdev_list_add(factory);
    if(result) return result;
    
    result = miscdev_file_operations_interceptor_internal_watch(factory);
    if(result)
    {
        miscdev_list_remove(factory);
        return result;
    }
    
    return 0;
}
static inline int miscdev_file_operations_interceptor_forget(
    struct miscdevice *factory)
{
    int result = miscdev_file_operations_interceptor_internal_forget(factory);
    miscdev_list_remove(factory);
    
    return result;
}

static inline int miscdev_file_operations_interceptor_forget_norestore(
    struct miscdevice *factory)
{
    int result = miscdev_file_operations_interceptor_internal_forget_norestore(factory);
    miscdev_list_remove(factory);
    
    return result;
}

/* Helper for find miscdevice for file */
static inline struct miscdevice* misc_for_file(struct file* filp)
{
    return miscdev_list_find(filp->f_dentry->d_inode->i_rdev);
}

#endif /* KEDR_COI_INTERCEPTOR_miscdev_file_operations_interceptor */