/* libdispatch.dylib
 * version 501.40.12
 * arch i386 x86_64
 ************************/
#include "lib_mach_info.h"
#include <dispatch/dispatch.h>

#define DISPATCH_EXECUTE 0x210a0010
#define DISPATCH_MIG_SERVER 0x210a0018
#define CALLOUT_BEGIN 0
#define CALLOUT_END 1

#if __x86_64__
/* __dispatch_client_callout:
 * begin
 * 0000000000002403    55                   pushq   %rbp
 * ->0000000000002404    4889e5             movq    %rsp, %rbp
 * ->0000000000002407    53                 pushq   %rbx 
 * ->0000000000002408    50                 pushq   %rax
 * 00000000000020b6    ffd6					callq   *%rsi
 */

void shell_dispatch_client_callout_begin(void * ctxt, dispatch_function_t f)
{
	save_registers
	back_trace((uint64_t)f);
	kdebug_trace(DISPATCH_EXECUTE|CALLOUT_BEGIN,
		(uint64_t)f, (uint64_t)ctxt, CALLOUT_BEGIN, 0, 0);
	restore_registers
	//emulate pushq and adjust prev_rbp
	//stack should be transformed
	// retold  --> rbx
	// rbpoold --> rax --> store func on it
	//          retold
	//			rbpold
	asm volatile("subq $0x10, %%rsp\n"
		"movq %%rsi, %%rax\n"
		"pushq %%rax\n"
		//push rbpold
		"movq %%rbp, %%rax\n"
		"addq $0x10, %%rax\n"
		"movq %%rax, -0x10(%%rbp)\n"
		//mov rax_val to rpbold position
		"popq %%rax\n"
		"movq %%rax, (%%rbp)\n"
		//mov ret 16 bytes down
		"movq 0x8(%%rbp), %%rax\n"
		"movq %%rax, -0x8(%%rbp)\n"
		//mov rbx to retold position
		"movq %%rbx, 0x8(%%rbp)\n"
		//adjust cur_rbp
		"subq $0x10, %%rbp\n"
		::);
}
#elif __i386__
/*__dispatch_client_callout:
 * 0x0	0000180d	55 	pushl	%ebp
 * 0x1L	0000180e	89 e5 	movl	%esp, %ebp
 * 0x3L	00001810	57 	pushl	%edi
 * 0x4L	00001811	56 	pushl	%esi
 * 0x5L	00001812	83 ec 10 	subl	$0x10, %esp
 * ->0x8L	00001815	8b 45 0c 	movl	0xc(%ebp), %eax
 * ->0xbL	00001818	8b 4d 08 	movl	0x8(%ebp), %ecx
 * 0xeL	0000181b	65 8b 35 dc 00 00 00 	movl	%gs:0xdc, %esi
 */
void shell_dispatch_client_callout_begin()
{
	uint64_t ctxt, func;
	save_registers
	asm volatile("movl (%%ebp), %%ebx\n"
		"movl 0xc(%%ebx), %%eax\n"
		"movl %%eax, %1\n"
		"movl 0x8(%%ebx), %%ecx\n"
		"movl %%ecx, %0"
		:"=m"(ctxt), "=m"(func):);
	back_trace(func);
	uint32_t debug_id = DISPATCH_EXECUTE|CALLOUT_BEGIN;
	kdebug_trace(debug_id, func, ctxt, CALLOUT_BEGIN, 0, 0);
	restore_registers
	//simulation
	asm volatile("movl (%%ebp), %%esi\n"
		"movl 0xc(%%esi), %%eax\n"
		"movl 0x8(%%esi), %%ecx"
		::);
}
#endif

#if __x86_64__
/* __dispatch_client_callout:
 * end
 * 000000000000240b    4883c408            addq    $0x8, %rsp
 * 000000000000240f    5b                  popq    %rbx
 */
void shell_dispatch_client_callout_end()
{
	uint64_t func;
	asm volatile("pushq %%rax\n"
		"movq 0x10(%%rbp), %%rax\n"
		"movq %%rax, %0\n"
		:"=m"(func)
		:);
	save_registers
	kdebug_trace(DISPATCH_EXECUTE|CALLOUT_END,
		func, 0, CALLOUT_END, 0, 0);
	restore_registers
	/* simulate
	 * addq $0x8, %rsp
	 * popq %rbx
	 * keep the value of rax, ret_val of callq
	 * before simulation in the last inst
	 */
	asm volatile("movq 0x18(%%rbp), %%rbx\n"
		"movq 0x8(%%rbp), %%rax\n"
		"movq %%rax, 0x18(%%rbp)\n"
		"movq 0x0(%%rbp), %%rax\n"
		"movq %%rax, 0x10(%%rbp)\n"
		"popq %%rax\n"
		"addq $0x10, %%rbp\n"
		"addq $0x10, %%rsp" 
		::); 
}
#elif __i386__
/* __dispatch_client_callout:
 * 0x0	0000180d	55 	pushl	%ebp
 * 0x23L	00001830	c7 86 00 01 00 00 00 00 00 00 	movl	$0x0, 0x100(%esi)
 * 0x2dL	0000183a	89 0c 24 	movl	%ecx, (%esp)
 * 0x30L	0000183d	ff d0 	calll	*%eax
 * 0x32L	0000183f	8b 86 00 01 00 00 	movl	0x100(%esi), %eax
 * 0x38L	00001845	85 c0 	testl	%eax, %eax
 * 0x3aL	00001847	74 09 	je	0x1852
 */
void shell_dispatch_client_callout_end()
{
	save_registers
	uint64_t ctxt, func;
	asm volatile("movl (%%ebp), %%esi\n"
		"movl 0x8(%%esi), %%eax\n"
		"movl %%eax, %0\n"
		"movl 0xc(%%esi), %%eax\n"
		"movl %%eax, %1\n"
		:"=m"(ctxt), "=m"(func), "=m"(esi):);
	uint32_t debug_id = DISPATCH_EXECUTE|CALLOUT_END;
	kdebug_trace(debug_id,
		func, ctxt, CALLOUT_END, 0, 0);
	restore_registers
	//simulation
	asm volatile("movl 0x100(%%esi), %%eax"::);
}
#endif

#if __x86_64__
/*
_dispatch_mig_server:
00000000000168b8	55              	pushq	%rbp
00000000000168b9	4889e5          	movq	%rsp, %rbp
00000000000168bc	4157            	pushq	%r15
00000000000168be	4156            	pushq	%r14
00000000000168c0	4155            	pushq	%r13
00000000000168c2	4154            	pushq	%r12
00000000000168c4	53              	pushq	%rbx
00000000000168c5	4883ec28        	subq	$0x28, %rsp
00000000000168c9	488955c0        	movq	%rdx, -0x40(%rbp)
00000000000168cd	48897db8        	movq	%rdi, -0x48(%rbp)
00000000000168d1	488b0550770100  	movq	0x17750(%rip), %rax
00000000000168d8	488b00          	movq	(%rax), %rax
00000000000168db	488945d0        	movq	%rax, -0x30(%rbp)
00000000000168df	8d4644          	leal	0x44(%rsi), %eax
00000000000168e2	8945cc          	movl	%eax, -0x34(%rbp)
00000000000168e5	4889e0          	movq	%rsp, %rax
00000000000168e8	4883c653        	addq	$0x53, %rsi
00000000000168ec	4883e6f0        	andq	$-0x10, %rsi
00000000000168f0	4829f0          	subq	%rsi, %rax
00000000000168f3	4889c4          	movq	%rax, %rsp
00000000000168f6	4989e7          	movq	%rsp, %r15
00000000000168f9	4929f7          	subq	%rsi, %r15
00000000000168fc	4c89fc          	movq	%r15, %rsp
00000000000168ff	41c7470400000000	movl	$0x0, 0x4(%r15)
0000000000016907	c7402000000000  	movl	$0x0, 0x20(%rax)
000000000001690e	be02090004      	movl	$0x4000902, %esi        ## imm = 0x4000902
0000000000016913	bbe8030000      	movl	$0x3e8, %ebx            ## imm = 0x3E8
*/
void shell_dispatch_mig_server()
{
	save_registers
	kdebug_trace(DISPATCH_MIG_SERVER, 0, 0, 0, 0, 0);
	restore_registers
	asm volatile("movl $0x0, 0x20(%%rax)"::);
}
#elif __i386__
#endif

void detour_blockinvoke(struct mach_o_handler * handler_ptr)
{
#if __x86_64__
	detour_function(handler_ptr, "_dispatch_client_callout",
		shell_dispatch_client_callout_begin, 1, 5);
	detour_function(handler_ptr, "_dispatch_client_callout",
		shell_dispatch_client_callout_end, 0x8, 5);
	detour_function(handler_ptr, "dispatch_mig_server",
		shell_dispatch_mig_server, 0x4f, 7);
#elif __i386__
	detour_function(handler_ptr, "_dispatch_client_callout",
		shell_dispatch_client_callout_begin, 0x8, 6);
	detour_function(handler_ptr, "_dispatch_client_callout",
		shell_dispatch_client_callout_end, 0x32, 6);
#endif
}
