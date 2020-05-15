#include "lib_mach_info.h"
#include <objc/objc-auto.h>

#define CALayerSet	 	0x2bd80114
#define CALayerDisplay	0x2bd80118

#if __x86_64__
/* -[CALayer setNeedsDisplay]:
* 0000000000009f5f	55 	pushq	%rbp
* 0000000000009f60	48 89 e5 	movq	%rsp, %rbp
* 0000000000009f63	48 83 ec 20 	subq	$0x20, %rsp
* 0000000000009f67	48 8b 35 62 e1 1e 00 	movq	0x1ee162(%rip), %rsi
* 0000000000009f6e	48 8b 05 ab 50 1c 00 	movq	0x1c50ab(%rip), %rax
* 0000000000009f75	48 8b 48 18 	movq	0x18(%rax), %rcx
* 0000000000009f79	48 89 4c 24 18 	movq	%rcx, 0x18(%rsp)
*/
void shell_CALayer_setNeedsDisplay(void *arg0)
{
	save_registers
	kdebug_trace(CALayerSet, arg0, 0ULL, 0ULL, 0ULL, 0);
	restore_registers
	asm volatile("movq %%rcx, 0x28(%%rbp)\n"::);
}
#elif __i386__
/* -[CALayer setNeedsDisplay]:
* 0000a98e	55 	pushl	%ebp
* 0000a98f	89 e5 	movl	%esp, %ebp
* 0000a991	83 ec 18 	subl	$0x18, %esp
* 0000a994	e8 00 00 00 00 	calll	0xa999
* 0000a999	58 	popl	%eax
* 0000a99a	8b 4d 08 	movl	0x8(%ebp), %ecx
* 0000a99d	8b 90 fb 5d 1e 00 	movl	0x1e5dfb(%eax), %edx
*/
void shell_CALayer_setNeedsDisplay(void *arg0)
{
	save_registers
	kdebug_trace(CALayerSet, arg0, 0ULL, 0ULL, 0ULL, 0);
	restore_registers
	asm volatile("movl 0x1e5dfb(%%eax), %%edx\n"::);
}

#endif

#if __x86_64
/*__ZN2CA5Layer17display_if_neededEPNS_11TransactionE:
000000000001124a	55 	pushq	%rbp
000000000001124b	48 89 e5 	movq	%rsp, %rbp
000000000001124e	41 57 	pushq	%r15
0000000000011250	41 56 	pushq	%r14
0000000000011252	41 55 	pushq	%r13
0000000000011254	41 54 	pushq	%r12

00000000000113ef	49 8b 5d 00 	movq	(%r13), %rbx
00000000000113f3	48 83 7b 10 00 	cmpq	$0x0, 0x10(%rbx)
00000000000113f8	bf 00 00 00 00 	movl	$0x0, %edi
00000000000113fd	74 0c 	je	0x1140b
00000000000113ff	48 8d 7b 10 	leaq	0x10(%rbx), %rdi
0000000000011403	e8 d8 31 18 00 	callq	0x1945e0 ##objc_read_weak
0000000000011408	48 89 c7 	movq	%rax, %rdi
000000000001140b	48 8b 35 9e 6b 1e 00 	movq	0x1e6b9e(%rip), %rsi


0000000000011481	48 8b 3b 	movq	(%rbx), %rdi
0000000000011484	48 83 7f 10 00 	cmpq	$0x0, 0x10(%rdi)
0000000000011489	b8 00 00 00 00 	movl	$0x0, %eax
000000000001148e	74 09 	je	0x11499
0000000000011490	48 83 c7 10 	addq	$0x10, %rdi
0000000000011494	e8 47 31 18 00 	callq	0x1945e0
0000000000011499	48 89 c7 	movq	%rax, %rdi
000000000001149c	4c 89 ee 	movq	%r13, %rsi
*/
void *shell_display_if_needed(void *arg0)
{
	save_registers
	kdebug_trace(CALayerDisplay, objc_read_weak(arg0), 0ULL, 0ULL, 0ULL, 0);
	restore_registers
	return objc_read_weak(arg0);
}
#elif __i386__
/*
__ZN2CA5Layer17display_if_neededEPNS_11TransactionE:
00011ebe	55 	pushl	%ebp
00011ebf	89 e5 	movl	%esp, %ebp
00011ec1	53 	pushl	%ebx
00011ec2	57 	pushl	%edi
00011ec3	56 	pushl	%esi

00012072	8d 46 0c 	leal	0xc(%esi), %eax
00012075	83 ec 0c 	subl	$0xc, %esp
00012078	50 	pushl	%eax
00012079	e8 8c 0d 1a 00 	calll	0x1b2e0a

00012150	83 c1 0c 	addl	$0xc, %ecx
00012153	83 ec 0c 	subl	$0xc, %esp
00012156	51 	pushl	%ecx
00012157	e8 ae 0c 1a 00 	calll	0x1b2e0a
*/
void *shell_display_if_needed(void *arg0)
{
	save_registers
	kdebug_trace(CALayerDisplay, objc_read_weak(arg0), 0ULL, 0ULL, 0ULL, 0);
	restore_registers
	return objc_read_weak(arg0);
}
#endif

void detour(struct mach_o_handler *handler_ptr)
{
#if __x86_64__
	detour_function(handler_ptr, "[CALayer setNeedsDisplay]",
		shell_CALayer_setNeedsDisplay, 0x1a, 5);
	detour_function(handler_ptr, "_ZN2CA5Layer17display_if_neededEPNS_11TransactionE",
		shell_display_if_needed, 0x1b9, 5);
	//detour_function(handler_ptr, "_ZN2CA5Layer17display_if_neededEPNS_11TransactionE",
		//shell_display_if_needed, 0x24a, 5);
#elif __i386__
	detour_function(handler_ptr, "[CALayer setNeedsDisplay]",
		shell_CALayer_setNeedsDisplay, 0xf, 6);
	detour_function(handler_ptr, "_ZN2CA5Layer17display_if_neededEPNS_11TransactionE", 
		shell_display_if_needed, 0x1bb, 5);
	//detour_function(handler_ptr, "_ZN2CA5Layer17display_if_neededEPNS_11TransactionE", 
		//shell_display_if_needed, 0x299, 5);
#endif
}
