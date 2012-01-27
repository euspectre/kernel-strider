/* tid.c - the means to obtain thread ID. */

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
#include <linux/hardirq.h>	/* in_interrupt() */
#include <linux/smp.h>		/* smp_processor_id() */
#include <linux/sched.h>	/* current */

unsigned long
kedr_get_thread_id(void)
{
	return (in_interrupt() 	? (unsigned long)smp_processor_id() 
				: (unsigned long)current);
}
