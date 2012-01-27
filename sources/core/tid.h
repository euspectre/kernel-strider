#ifndef TID_H_1743_INCLUDED
#define TID_H_1743_INCLUDED

/* Returns the ID of the current thread. The caller should not rely on it
 * being some address or whatever, this is an implementation detail and is
 * subject to change. 
 * In addition to the "regular" threads, the function can be called in the 
 * interrupt service routines (ISRs). The IDs it returns for ISRs can never 
 * collide with the IDs it returns for the regular threads. */
unsigned long
kedr_get_thread_id(void);

#endif /* TID_H_1743_INCLUDED */
