/* common.h - API provided by the function handling plugin that processes 
 * operations common to many kinds of kernel modules. */

#ifndef FH_DRD_COMMON_H_1226_INCLUDED
#define FH_DRD_COMMON_H_1226_INCLUDED

/* kedr_*_(start|end) functions declared below should be called at the start
 * and at the end of the code fragments of appropriate type: BH functions
 * (timer/tasklet/softirq functions, etc.), IRQ handlers, fragments with
 * BH disabled, fragments with IRQ disabled, respectively.
 *
 * These functions will generate appropriate events to pass the information
 * about the happens-before relations involved here to the trace.
 *
 * Common arguments:
 * - tid - thread ID;
 * - func - address of the BH function or the IRQ handler;
 * - pc - address of the current instruction. */

void
kedr_bh_start(unsigned long tid, unsigned long func);

void
kedr_bh_end(unsigned long tid, unsigned long func);

void
kedr_bh_disabled_start(unsigned long tid, unsigned long pc);

void
kedr_bh_disabled_end(unsigned long tid, unsigned long pc);

/* [NB] kedr_irq_*() functions call kedr_bh_disabled_*() among other things.
 * The caller does not need to additionally call kedr_bh_disabled_*() here.
 */

void
kedr_irq_start(unsigned long tid, unsigned long func);

void
kedr_irq_end(unsigned long tid, unsigned long func);

void
kedr_irq_disabled_start(unsigned long tid, unsigned long pc);

void
kedr_irq_disabled_end(unsigned long tid, unsigned long pc);

/* ====================================================================== */
#endif /* FH_DRD_COMMON_H_1226_INCLUDED */
