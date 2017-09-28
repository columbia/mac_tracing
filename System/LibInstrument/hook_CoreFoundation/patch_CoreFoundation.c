#include "lib_mach_info.h"
#include <CoreFoundation/CFRunLoop.h>

#define DEBUG_CreateObserver	0x23456780
#define DEBUG_AddObserver		0x23456784
#define DEBUG_RemoveObserver	0x23456788
#define DEBUG_CallObserver		0x2345678c

#define RunLoopObserverDebug	0x2bd80130

#if defined(__LP64__)
//_CFRunLoopAddObserver:
//_CFRunLoopRemoveObserver:
//___CFRUNLOOP_IS_CALLING_OUT_TO_AN_OBSERVER_CALLBACK_FUNCTION__:
/*
 * _CFRunLoopAddObserver:
 * 0000000000072c50    55  pushq   %rbp
 * ->0000000000072c61    48 89 d3    movq    %rdx, %rbx
 * ->0000000000072c64    49 89 f7    movq    %rsi, %r15
 */
void shell_CFRunLoopAddObserver(uint64_t rl, uint64_t observer, uint64_t mode)
{
	save_registers
	kdebug_trace(DEBUG_AddObserver, rl, observer, mode, 0, 0);
	restore_registers
	asm volatile("movq %%rdx, %%rbx\n"
				"movq %%rsi, %%r15"::);
}
/*
 * _CFRunLoopRemoveObserver
 * 00000000000aa070    55  pushq   %rbp
 * ->00000000000aa081    48 89 d3    movq    %rdx, %rbx
 * ->00000000000aa084    49 89 f6    movq    %rsi, %r14
 */
void shell_CFRunLoopRemoveObserver(uint64_t rl, uint64_t observer, uint64_t mode)
{
	save_registers
	kdebug_trace(DEBUG_RemoveObserver, rl, observer, mode, 0, 0);
	restore_registers
	asm volatile("movq %%rdx, %%rbx\n"
				"movq %%rsi, %%r14"::);
}

/* ___CFRUNLOOP_IS_CALLING_OUT_TO_AN_OBSERVER_CALLBACK_FUNCTION__
 * 00000000000aa050    55  pushq   %rbp
 * ->00000000000aa05c    48 89 f7    movq    %rsi, %rdi
 * ->00000000000aa05f    48 89 d6    movq    %rdx, %rsi
 */

void shell___CFRUNLOOP_IS_CALLING_OUT_TO_AN_OBSERVER_CALLBACK_FUNCTION__(uint64_t observer, uint64_t arg1, uint64_t arg2, uint64_t arg3)
{
	save_registers
	kdebug_trace(DEBUG_CallObserver, observer, arg1, arg2, arg3, 0);
	restore_registers
	asm volatile("movq %%rsi, %%rdi\n"
				"movq %%rdx, %%rsi"::);
}

/* _CFRunLoopObserverCreate(CFAllocatorRef allocator, CFOptionFlags activities, Boolean repeats, CFIndex order, CFRunLoopObserverCallBack callout, CFRunLoopObserverContext *context)
 * 0000000000072ab0    55  pushq   %rbp
 * ->0000000000072c27    5b  popq    %rbx
 * ->0000000000072c28    41 5c   popq    %r12
 * ->0000000000072c2a    41 5d   popq    %r13
 */
void shell_CFRunLoopObserverCreate()
{
	save_registers
	uint64_t simu_rbx, simu_r12, simu_r13;
	uint64_t observer, callout;
	asm volatile("movq 0x10(%%rbp), %%rax\n"
				"movq %%rax, %0\n"
				"movq 0x18(%%rbp), %%rax\n"
				"movq %%rax, %1\n"
				"movq 0x20(%%rbp), %%rax\n"
				"movq %%rax, %2\n"
				"movq %%r15, %3\n"
				"movq 0x80(%%rbx), %%rax\n"
				"movq %%rax, %4\n"
				"movq 0x8(%%rbp), %%rax\n"
				"movq %%rax, 0x20(%%rbp)\n"
				"movq (%%rbp), %%rax\n"
				"movq %%rax, 0x18(%%rbp)\n"
				:"=m"(simu_rbx), "=m"(simu_r12), "=m"(simu_r13), "=m"(observer), "=m"(callout):);
	kdebug_trace(DEBUG_CreateObserver, observer, callout, 0, 0, 0);
	restore_registers
	asm volatile("movq %0, %%rbx\n"
				"movq %1, %%r12\n"
				"movq %2, %%r13\n"
				"addq $0x18, %%rsp"
				::"m"(simu_rbx), "m"(simu_r12), "m"(simu_r13));
}

/* ___CFRunLoopDoObservers:
* 00000000000a9e50	55              	pushq	%rbp
* 00000000000a9e51	4889e5          	movq	%rsp, %rbp
* 00000000000a9e54	4157            	pushq	%r15
* 00000000000a9e56	4156            	pushq	%r14
* 00000000000a9e58	4155            	pushq	%r13
* 00000000000a9e5a	4154            	pushq	%r12
* 00000000000a9e5c	53              	pushq	%rbx
* 00000000000a9e5d	4883ec38        	subq	$0x38, %rsp
* 00000000000a9e61	488955c8        	movq	%rdx, -0x38(%rbp)
* 00000000000a9e65	4989f7          	movq	%rsi, %r15
* 00000000000a9e68	4989fd          	movq	%rdi, %r13
*/

void shell___CFRunLoopDoObservers(void *rl, void *mode, void *ob)
{
	save_registers
	kdebug_trace(RunLoopObserverDebug, rl, mode, ob, 0ULL, 0);
	restore_registers
	asm volatile("movq %%rsi, %%r15\n"
		"movq %%rdi, %%r13"::);
}

/*
* ___CFRunLoopRun:
* 0000000000089140	55              	pushq	%rbp
* 0000000000089141	4889e5          	movq	%rsp, %rbp
* 0000000000089144	4157            	pushq	%r15
* 0000000000089146	4156            	pushq	%r14
* 0000000000089148	4155            	pushq	%r13
* 000000000008914a	4154            	pushq	%r12
* 000000000008914c	53              	pushq	%rbx
* 000000000008914d	4881eca80c0000  	subq	$0xca8, %rsp            ## imm = 0xCA8
* 0000000000089154	89d3            	movl	%edx, %ebx
* 0000000000089156	f20f118598f3ffff	movsd	%xmm0, -0xc68(%rbp)
* 000000000008915e	4989f6          	movq	%rsi, %r14
* 0000000000089161	4989fd          	movq	%rdi, %r13
* 0000000000089164	4c8b3d85df3e00  	movq	0x3edf85(%rip), %r15
* 000000000008916b	4d8b3f          	movq	(%r15), %r15
* 000000000008916e	4c897dd0        	movq	%r15, -0x30(%rbp)
* 0000000000089172	e8c3bc1400      	callq	0x1d4e3a
* 0000000000089177	498b4d58        	movq	0x58(%r13), %rcx
*/

uint64_t shell___CFRunLoopRun(void *rl, void *mode)
{
	save_registers
	kdebug_trace(RunLoopObserverDebug, rl, mode, (1ULL << 8), 0ULL, 0);
	restore_registers
	return mach_absolute_time();
}

void detour(struct mach_o_handler * handler_ptr)
{
	/*
	detour_function(handler_ptr, "CFRunLoopAddObserver", shell_CFRunLoopAddObserver, 0x11, 6);
	detour_function(handler_ptr, "CFRunLoopRemoveObserver", shell_CFRunLoopRemoveObserver, 0x11, 6);
	detour_function(handler_ptr, "__CFRUNLOOP_IS_CALLING_OUT_TO_AN_OBSERVER_CALLBACK_FUNCTION__", shell___CFRUNLOOP_IS_CALLING_OUT_TO_AN_OBSERVER_CALLBACK_FUNCTION__, 0xc, 6);
	detour_function(handler_ptr, "CFRunLoopObserverCreate", shell_CFRunLoopObserverCreate, 0x177, 5);
	*/
	detour_function(handler_ptr, "__CFRunLoopDoObservers", shell___CFRunLoopDoObservers, 0x15, 6);
	detour_function(handler_ptr, "__CFRunLoopRun", shell___CFRunLoopRun, 0x32, 5);
}

#endif
