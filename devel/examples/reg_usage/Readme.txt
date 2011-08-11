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
[21690.345382] [sample] module: "btrfs", processing function 
  "btrfs_test_super" (address is ffffffffa028b000, size is 64)
[21690.345386] [DBG] Gathering register usage info for btrfs_test_super()
[21690.345389] [DBG]   0: RDX RDI 
[21690.345390] [DBG]   6: RSP RBP 
[21690.345392] [DBG]   7: RAX 
[21690.345393] [DBG]   9: RCX RDI 
[21690.345394] [DBG]  10: RSP RBP 
[21690.345395] [DBG]  13: RDX 
[21690.345397] [DBG]  15: 
[21690.345398] [DBG]  17: RCX RDX 
[21690.345399] [DBG]  1e: RAX RSI 
[21690.345400] [DBG]  25: RAX RCX 
[21690.345401] [DBG]  2c: RAX 
[21690.345402] [DBG]  2e: RCX RDX 
[21690.345404] [DBG]  35: RAX 
[21690.345405] [DBG]  38: RSP RBP 
[21690.345406] [DBG]  39: RSP 
[21690.345407] [DBG]  3a: 
[21690.345408] [DBG] for_each_insn_in_function() returned 0
[21690.345409] [DBG] Register usage totals:
[21690.345410] [DBG]   RAX: 5
[21690.345411] [DBG]   RCX: 4
[21690.345412] [DBG]   RDX: 4
[21690.345413] [DBG]   RBX: 0
[21690.345414] [DBG]   RSP: 4
[21690.345415] [DBG]   RBP: 3
[21690.345416] [DBG]   RSI: 1
[21690.345417] [DBG]   RDI: 2
[21690.345418] [DBG]   R8: 0
[21690.345419] [DBG]   R9: 0
[21690.345419] [DBG]   R10: 0
[21690.345420] [DBG]   R11: 0
[21690.345421] [DBG]   R12: 0
[21690.345422] [DBG]   R13: 0
[21690.345423] [DBG]   R14: 0
[21690.345424] [DBG]   R15: 0
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
