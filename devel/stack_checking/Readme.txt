Here is a patch to add checking for stack corruption in the instrumented
functions (x86-64 only, change ls->arg_regs.rsp to ls->arg_regs.esp for
i586).

For each such function, the stack pointer (%rsp) and the value on top of the
stack (which should be the return address) are read right after entry to and
right before exit from the function. If they do not match, warnings will be
printed to the system log.

May help in debugging of KernelStrider itself.

The patch was made w.r.t. rev. 61adef7b7f72.

