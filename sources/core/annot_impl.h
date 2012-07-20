/* annot_impl.h - handlers for the dynamic annotations. */

#ifndef ANNOT_IMPL_H_1057_INCLUDED
#define ANNOT_IMPL_H_1057_INCLUDED
/* ====================================================================== */

/* Types of the annotations our system currently supports */
enum kedr_annotation_type 
{
	KEDR_ANN_TYPE_HAPPENS_BEFORE = 0,
	KEDR_ANN_TYPE_HAPPENS_AFTER = 1,
	KEDR_ANN_TYPE_MEMORY_ACQUIRED = 2,
	KEDR_ANN_TYPE_MEMORY_RELEASED = 3,
	KEDR_ANN_NUM_TYPES /* total number of types */
};

struct kedr_local_storage;
struct kedr_annotation_handlers
{
	const char *name; /* name of the annotation function */
	void (*pre)(struct kedr_local_storage *);
	void (*post)(struct kedr_local_storage *);
};

extern struct kedr_annotation_handlers
kedr_annotation_handlers[KEDR_ANN_NUM_TYPES];
/* ====================================================================== */
#endif /* ANNOT_IMPL_H_1057_INCLUDED */
