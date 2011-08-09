This example demonstrates the enhanced instruction analysis facilities 
(based on the decoder from KProbes). Among other things, they now allow to 
determine which general registers a given instruction uses.

Usage:
  insmod kedr_sample.ko target_name="<specify_target_name_here>" \
    target_function="<specify_target_function_here>""

The example is based on "call_inject" example. 
As soon as the target module is loaded, it will be processed. For the 
specified function in that module, information about the registers each its 
instruction uses will be output to the system log, followed by totals for 
each register.

Example (OpenSUSE 11.4 x86-64):

# insmod kedr_sample.ko target_name="btrfs" \ 
    target_function="btrfs_test_super"
...

From the system log:
---------------------------
[71320.711583] [sample] module: "btrfs", processing function 
  "btrfs_test_super" (address is ffffffffa029e000, size is 64)
[71320.711588] [DBG] Gathering register usage info for btrfs_test_super()
[71320.711591] [DBG]   0: RDX RDI 
[71320.711593] [DBG]   6: RSP RBP 
[71320.711594] [DBG]   7: RAX 
[71320.711596] [DBG]   9: RCX RDI 
[71320.711597] [DBG]  10: RSP RBP 
[71320.711599] [DBG]  13: RDX 
[71320.711600] [DBG]  15: 
[71320.711602] [DBG]  17: RCX RDX 
[71320.711604] [DBG]  1e: RAX RSI 
[71320.711605] [DBG]  25: RAX RCX 
[71320.711606] [DBG]  2c: RAX 
[71320.711608] [DBG]  2e: RCX RDX 
[71320.711609] [DBG]  35: RAX 
[71320.711611] [DBG]  38: RSP RBP 
[71320.711613] [DBG]  39: RAX RCX RDX RSP RSI RDI R8 R9 R10 R11 
[71320.711615] [DBG]  3a: 
[71320.711616] [DBG] for_each_insn_in_function() returned 0
[71320.711617] [DBG] Register usage totals:
[71320.711619] [DBG]   RAX: 6
[71320.711620] [DBG]   RCX: 5
[71320.711621] [DBG]   RDX: 5
[71320.711623] [DBG]   RBX: 0
[71320.711624] [DBG]   RSP: 4
[71320.711625] [DBG]   RBP: 3
[71320.711626] [DBG]   RSI: 2
[71320.711627] [DBG]   RDI: 3
[71320.711628] [DBG]   R8: 1
[71320.711630] [DBG]   R9: 1
[71320.711631] [DBG]   R10: 1
[71320.711632] [DBG]   R11: 1
[71320.711633] [DBG]   R12: 0
[71320.711634] [DBG]   R13: 0
[71320.711636] [DBG]   R14: 0
[71320.711637] [DBG]   R15: 0
---------------------------

The code of that function looks as follows:

---------------------------
0000000000000000 <btrfs_test_super>:
       0:       8b 97 ac 00 00 00       mov    0xac(%rdi),%edx
       6:       55                      push   %rbp
       7:       31 c0                   xor    %eax,%eax
       9:       48 8b 8f 88 02 00 00    mov    0x288(%rdi),%rcx
      10:       48 89 e5                mov    %rsp,%rbp
      13:       85 d2                   test   %edx,%edx
      15:       74 21                   je     38 <btrfs_test_super+0x38>
      17:       48 8b 91 20 01 00 00    mov    0x120(%rcx),%rdx
      1e:       48 8b 86 20 01 00 00    mov    0x120(%rsi),%rax
      25:       48 8b 88 48 23 00 00    mov    0x2348(%rax),%rcx
      2c:       31 c0                   xor    %eax,%eax
      2e:       48 39 8a 48 23 00 00    cmp    %rcx,0x2348(%rdx)
      35:       0f 94 c0                sete   %al
      38:       c9                      leaveq 
      39:       c3                      retq   
      3a:       66 0f 1f 44 00 00       nopw   0x0(%rax,%rax,1)
---------------------------
