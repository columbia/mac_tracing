#include "lib_mach_info.h"
#include <pthread.h>

#define CBROADCAST		0x29000098
#define CSIGNAL			0x2900009c
#define CWAIT			0x290000a0

#if __x86_64__
/*
_pthread_cond_broadcast:
00000000000022bb	be01000000      	movl	$0x1, %esi
00000000000022c0	31d2            	xorl	%edx, %edx
00000000000022c2	e900000000      	jmp	0x22c7
*/
void shell_pthread_cond_broadcast(void * ocond)
{
	save_registers
	back_trace((uint64_t)ocond);
	restore_registers
	asm volatile("movl $0x1, %%esi"::);
}
#elif __i386__
/* _pthread_cond_broadcast:
 * 0x0	00001fd0	55 	pushl	%ebp
 * 0x1L	00001fd1	89 e5 	movl	%esp, %ebp
 * 0x3L	00001fd3	83 ec 08 	subl	$0x8, %esp
 * 0x6L	00001fd6	8b 4d 08 	movl	0x8(%ebp), %ecx
 * 0x9L	00001fd9	c7 04 24 00 00 00 00 	movl	$0x0, (%esp)
 * 0x10L	00001fe0	ba 01 00 00 00 	movl	$0x1, %edx
 * 0x15L	00001fe5	e8 05 00 00 00 	calll	0x1fef
 * 0x1aL	00001fea	83 c4 08 	addl	$0x8, %esp
 * 0x1dL	00001fed	5d 	popl	%ebp
 * 0x1eL	00001fee	c3 	retl
 */
void shell_pthread_cond_broadcast(void * ocond)
{
	save_registers
	back_trace((uint64_t)ocond);
	restore_registers
	asm volatile("movl $0x1, %%edx"::);
}
#endif


#if __x86_64__
/*
__pthread_cond_signal:
00000000000022c7	55              	pushq	%rbp
00000000000022c8	4889e5          	movq	%rsp, %rbp
00000000000022cb	4157            	pushq	%r15
00000000000022cd	4156            	pushq	%r14
00000000000022cf	4155            	pushq	%r13
00000000000022d1	4154            	pushq	%r12
00000000000022d3	53              	pushq	%rbx
00000000000022d4	4883ec48        	subq	$0x48, %rsp
00000000000022d8	8955c4          	movl	%edx, -0x3c(%rbp)
-->00000000000022db	4189f7          	movl	%esi, %r15d
-->00000000000022de	4989fe          	movq	%rdi, %r14
*/
void shell__pthread_cond_signal(void *ocond)
{
	save_registers
	back_trace((uint64_t)ocond);
	restore_registers
	asm volatile("movl %%esi, %%r15d\n"
				"movq %%rdi, %%r14"::);
}
#elif __i386__
/* __pthread_cond_signal:
 * 0x0	00001fef	55 	pushl	%ebp
 * 0x1L	00001ff0	89 e5 	movl	%esp, %ebp
 * 0x3L	00001ff2	53 	pushl	%ebx
 * 0x4L	00001ff3	57 	pushl	%edi
 * 0x5L	00001ff4	56 	pushl	%esi
 * 0x6L	00001ff5	83 ec 6c 	subl	$0x6c, %esp
 * 0x9L	00001ff8	89 d3 	movl	%edx, %ebx
 * 0xbL	00001ffa	89 ce 	movl	%ecx, %esi
 * 0xdL	00001ffc	c6 45 f3 00 	movb	$0x0, -0xd(%ebp)
 */ 
void shell__pthread_cond_signal(void *ocond)
{
	save_registers
	back_trace((uint64_t)ocond);
	restore_registers
	asm volatile(
		"movl (%%ebp), %%esi\n"
		"movb $0x0, -0xd(%%esi)\n"
		"movl %%ecx, %%esi"::);
}
#endif

#if __x86_64__
/*
__pthread_cond_wait:
0000000000004429	55              	pushq	%rbp
000000000000442a	4889e5          	movq	%rsp, %rbp
000000000000442d	4157            	pushq	%r15
000000000000442f	4156            	pushq	%r14
0000000000004431	4155            	pushq	%r13
0000000000004433	4154            	pushq	%r12
0000000000004435	53              	pushq	%rbx
0000000000004436	4881ec88000000  	subq	$0x88, %rsp
-->000000000000443d	4189ce          	movl	%ecx, %r14d
-->0000000000004440	4989d4          	movq	%rdx, %r12
*/
void shell__pthread_cond_wait(void *ocond)
{
	save_registers
	back_trace((uint64_t)ocond);
	restore_registers
	asm volatile("movl %%ecx, %%r14d\n"
				"movq %%rdx, %%r12"::);
}
#elif __i386__
/* __pthread_cond_wait:
 * 0x0	00004243	55 	pushl	%ebp
 * 0x1L	00004244	89 e5 	movl	%esp, %ebp
 * 0x3L	00004246	53 	pushl	%ebx
 * 0x4L	00004247	57 	pushl	%edi
 * 0x5L	00004248	56 	pushl	%esi
 * 0x6L	00004249	83 ec 7c 	subl	$0x7c, %esp
 * 0x9L	0000424c	e8 00 00 00 00 	calll	0x4251
 * 0xeL	00004251	58 	popl	%eax
 * 0xfL	00004252	89 45 b4 	movl	%eax, -0x4c(%ebp)
 * 0x12L	00004255	8b 7d 08 	movl	0x8(%ebp), %edi
 * 0x15L	00004258	c7 45 e8 00 00 00 00 	movl	$0x0, -0x18(%ebp)
 */
void shell__pthread_cond_wait(void *ocond)
{
	save_registers
	back_trace((uint64_t)ocond);
	asm volatile("movl (%%ebp), %%esi\n"
		"movl $0x0, -0x18(%%esi)"::);
	restore_registers
}
#endif

void detour(struct mach_o_handler *handler_ptr)
{
#if ___x86_64__
	detour_function(handler_ptr, "pthread_cond_broadcast",
		shell_pthread_cond_broadcast, 0, 5);
	detour_function(handler_ptr, "_pthread_cond_signal",
		shell__pthread_cond_signal, 0x14, 6);
	detour_function(handler_ptr, "_pthread_cond_wait",
		shell__pthread_cond_wait, 0x14, 6);
#elif __i386__
	detour_function(handler_ptr, "pthread_cond_broadcast",
		shell_pthread_cond_broadcast, 0x10, 5);
	detour_function(handler_ptr, "_pthread_cond_signal",
		shell__pthread_cond_signal, 0xb, 6);
	detour_function(handler_ptr, "_pthread_cond_wait",
		shell__pthread_cond_wait, 0x15, 7);
#endif
}
