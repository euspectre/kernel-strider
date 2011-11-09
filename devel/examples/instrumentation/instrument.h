/* instrument.h - instrumentation-related facilities.  */

#ifndef INSTRUMENT_H_1649_INCLUDED
#define INSTRUMENT_H_1649_INCLUDED

#include "ifunc.h"

struct module;

/* Creates the instrumented instance of the function in the temporary memory
 * buffer. The resulting code will only need relocation before it can be 
 * used. On success, 'tbuf' and 'i_size' become defined, 'tbuf' 
 * pointing to that buffer. In case of failure, 'tbuf' will remain NULL.
 * 
 * [NB] The value of 'i_addr' will be defined at the deployment stage, when 
 * the function is copied to its final location. 
 *
 * [NB] Here, we can assume that the size of the function is not less than
 * the size of 'jmp near rel32'.
 *
 * The function allocates memory for the instrumented instance as needed.
 * 
 * The instructions to be relocated at the deployment phase (call/jmp rel32 
 * outside of the function, instructions with RIP-relative addressing) will 
 * be listed in 'func->relocs'.
 * 
 * Among other things, the function allocates and fills the jump tables (if 
 * any) for the instrumented instance with the "pointers" to the appropriate
 * positions in the instrumented function. The quotation marks are here to 
 * imply that these values are not actually pointers at this stage. They are 
 * computed as if the instrumented function had the start address of 0. They
 * will be fixed up during the deployment phase.
 * 
 * The return value is 0 on success and a negative error code on failure. */
int
instrument_function(struct kedr_ifunc *func, struct module *mod);

#endif // INSTRUMENT_H_1649_INCLUDED
