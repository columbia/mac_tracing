#include "lib_mach_info.h"

#include <CoreFoundation/CFRunLoop.h>

#define DEBUG_CreateObserver	0x23456780
#define DEBUG_AddObserver		0x23456784
#define DEBUG_RemoveObserver	0x23456788
#define DEBUG_CallObserver		0x2345678c

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

void detour(struct hack_handler * hack_handler_ptr)
{
	detour_function(hack_handler_ptr, "CFRunLoopAddObserver", shell_CFRunLoopAddObserver, 0x11, 6);
	detour_function(hack_handler_ptr, "CFRunLoopRemoveObserver", shell_CFRunLoopRemoveObserver, 0x11, 6);
	detour_function(hack_handler_ptr, "__CFRUNLOOP_IS_CALLING_OUT_TO_AN_OBSERVER_CALLBACK_FUNCTION__", shell___CFRUNLOOP_IS_CALLING_OUT_TO_AN_OBSERVER_CALLBACK_FUNCTION__, 0xc, 6);
	detour_function(hack_handler_ptr, "CFRunLoopObserverCreate", shell_CFRunLoopObserverCreate, 0x177, 5);
}

#endif
