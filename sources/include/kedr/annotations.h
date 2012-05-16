/* annotations.h - dynamic annotations that can be used to make the 
 * analysis more precise. 
 * The annotations provided here are "dynamic" in a sense that they are not
 * comments, i.e. they are not ignored by the compiler but rather they 
 * expand to the appropriate code fragments. 
 *
 * To use these annotations, you need to #include this header in the code 
 * of the target module and #define KEDR_ANNOTATIONS_ENABLED to a non-zero 
 * value. It is also needed to use .symvers files from kedr_mem_core.ko 
 * module (it exports kedr_annotate*() functions when building the target
 * module. */

#ifndef ANNOTATIONS_H_1536_INCLUDED
#define ANNOTATIONS_H_1536_INCLUDED

#ifndef KEDR_ANNOTATIONS_ENABLED
#define KEDR_ANNOTATIONS_ENABLED 0
#endif

#if KEDR_ANNOTATIONS_ENABLED != 0

/* These annotations are similar to ANNOTATE_HAPPENS_BEFORE() and 
 * ANNOTATE_HAPPENS_AFTER() provided by ThreadSanitizer (see 
 * http://code.google.com/p/data-race-test/wiki/DynamicAnnotations
 * for details). 
 *
 * A happens-before arc will be created by a race detector from 
 * KEDR_ANNOTATE_HAPPENS_BEFORE() to KEDR_ANNOTATE_HAPPENS_AFTER() with the
 * same value of 'obj' (if the former is observed first). 
 *
 * Note that these annotations are not required to do anything themselves.
 * It is likely that the calls to the respective kedr_annotate_*() functions
 * are to be intercepted by some other our subsystem and the real work will
 * be done in the call handlers. */
#define KEDR_ANNOTATE_HAPPENS_BEFORE(obj) \
	kedr_annotate_happens_before(obj)
#define KEDR_ANNOTATE_HAPPENS_AFTER(obj) \
	kedr_annotate_happens_after(obj)
	
#else /* KEDR_ANNOTATIONS_ENABLED == 0 */

/* Define the annotations as no-ops. */
#define KEDR_ANNOTATE_HAPPENS_BEFORE(obj) do { } while (0)
#define KEDR_ANNOTATE_HAPPENS_AFTER(obj) do { } while (0)

#endif /* KEDR_ANNOTATIONS_ENABLED != 0 */

/* Do not use kedr_annotate_*() functions directly, use KEDR_ANNOTATE_*
 * instead. */
extern void kedr_annotate_happens_before(unsigned long obj);
extern void kedr_annotate_happens_after(unsigned long obj);

#endif /* ANNOTATIONS_H_1536_INCLUDED */