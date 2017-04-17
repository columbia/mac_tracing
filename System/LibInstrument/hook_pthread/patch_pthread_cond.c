#include "lib_mach_info.h"
#include <pthread.h>

#if defined(__LP64__)

#define CBROADCAST		0x29000098
#define CSIGNAL			0x2900009c
#define CWAIT			0x290000a0

/*
_pthread_cond_broadcast:
00000000000022bb	be01000000      	movl	$0x1, %esi
00000000000022c0	31d2            	xorl	%edx, %edx
00000000000022c2	e900000000      	jmp	0x22c7
*/
void shell_pthread_cond_broadcast(void * ocond)
{
	save_registers
	back_trace(ocond);
	restore_registers
	asm volatile("movl $0x1, %%esi"::);
}

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
	back_trace(ocond);
	restore_registers
	asm volatile("movl %%esi, %%r15d\n"
				"movq %%rdi, %%r14"::);
}

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
void shell__pthread_cond_wait(void * ocond)
{
	save_registers
	back_trace(ocond);
	restore_registers
	asm volatile("movl %%ecx, %%r14d\n"
				"movq %%rdx, %%r12"::);
}

void detour(struct hack_handler * hack_handler_ptr)
{
	detour_function(hack_handler_ptr, "pthread_cond_broadcast", shell_pthread_cond_broadcast, 0, 5);
	detour_function(hack_handler_ptr, "_pthread_cond_signal", shell__pthread_cond_signal, 0x14, 6);
	detour_function(hack_handler_ptr, "_pthread_cond_wait", shell__pthread_cond_wait, 0x14, 6);
}
#endif
