/* object_types.h - constants for different types of objects. */

#ifndef OBJECT_TYPES_H_1631_INCLUDED
#define OBJECT_TYPES_H_1631_INCLUDED

/* Types of memory events. */
enum kedr_memory_event_type {
	KEDR_ET_MREAD = 0, /* read from memory */
	KEDR_ET_MWRITE,    /* write to memory */
	KEDR_ET_MUPDATE    /* update of memory (read + write) */
};

/* Types of locks. */
enum kedr_lock_type {
	KEDR_LT_MUTEX = 0, /* mutex */
	KEDR_LT_SPINLOCK   /* spinlock */
}; 

/* Types of memory barriers. */
enum kedr_barrier_type {
	/* barrier for both loads from memory and stores to memory */
	KEDR_BT_FULL = 0,
	
	/* barrier only for loads from memory */
	KEDR_BT_LOAD,
	
	/* barrier only for stores to memory */
	KEDR_BT_STORE
};

/* Types of the objects used in signal/wait operations */
enum kedr_sw_object_type {
	KEDR_SWT_COMMON = 0
};

#endif // OBJECT_TYPES_H_1631_INCLUDED
