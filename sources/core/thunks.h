/* thunks.h - thunks (special functions) used when handling function calls.
 * The thunks are responsible for calling pre- and post- handlers and the 
 * replacement function for the target in the correct environment 
 * (registers, etc.). */

#ifndef THUNKS_H_1137_INCLUDED
#define THUNKS_H_1137_INCLUDED

/* The thunks do not change the values of the non-scratch registers.
 * After a thunks exits, %rax and %rdx (%eax and %edx on x86-32) have
 * the same values as they would have after the call to the target 
 * function. That is, %rax or %rdx:%rax contain the return value of 
 * the target (or, more exactly, of the replacement function) if the 
 * latter returns value there, otherwise the values in these registers
 * are unspecified. 
 *
 * Each thunk accepts a single parameter, the address of the local storage.
 * The parameter is passed in %rax. The usual calling conventions are not
 * used for thunks, hence their signature of 'void thunk_name(void)'.
 *
 * The original value of %rax (the value it would have in the original 
 * code just before the call to the target function) should be in the 
 * spill slot for %rax in the local storage before a thunk is called.
 *
 * 'info' field of the local storage must contain the address of the 
 * corresponding kedr_call_info instance before a thunk is called.
 * The kedr_call_info instance must be fully initialized by that time. */

/* Used in handling functon calls performed using CALL instruction.
 * Should be called the same way, i.e. with CALL instruction. */
void
kedr_thunk_call(void);

/* Used in handling functon calls performed using JMP instruction.
 * Should be called the same way, i.e. with JMP instruction. 
 * As the control is not expected to return to the caller after a function
 * invocation, the thunk also handles the exit from the instrumented
 * instance. 
 * Because this is the exit from the function, all the registers except
 * %rax should have their original values (the values they would have
 * in the original code at this point) on entry to the thunk. */
void
kedr_thunk_jmp(void);
#endif /* THUNKS_H_1137_INCLUDED */
