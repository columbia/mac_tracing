#include "lib_mach_info.h"
#include <dispatch/dispatch.h>

#if defined(__LP64__)
#define DISPATCH_DEQUEUE 0x210a000c

/*3
 * __dispatch_root_queue_drain
 * 0000000000005b39    55                  pushq   %rbp
 * 0000000000005c20    48c7c3ffffffff      movq    $-0x1, %rbx
 * 0000000000005c27    49871e              xchgq   %rbx, (%r14) 
 *
 * start:
 * ->0000000000005c20    48c7c3ffffffff      movq    $-0x1, %rbx 
 * ->0000000000005c27    49871e              xchgq   %rbx, (%r14)
 *
 * __dispatch_root_queue_drain:
 * _dispatch_queue_concurrent_drain_one(dispatch_queue_t dq) 
 * next = fastpath(head->do_next);
 * 0000000000005c92    488b4310            movq    0x10(%rbx), %rax
 * 0000000000005c96    4885c0              testq   %rax, %rax
 * if next == NULL
 * 0000000000005c99    7521                jne 0x5cbc
 * ->0000000000005c9b    49c70600000000      movq    $0x0, (%r14)
 * if next != NULL
 * ->0000000000005cbc    498906              movq    %rax, (%r14)
 * ->0000000000005cbf    498b4500            movq    (%r13), %rax
 *
 * #define DISPATCH_OBJ_IS_VTABLE(x) ((unsigned long)(x)->do_vtable > 0xfful)
 */

void shell__dispatch_root_queue_drain()
{
	save_registers;
	uint64_t item;
	asm volatile("movq $-0x1, %%rbx\n"
			"xchgq %%rbx, (%%r14)\n"
			"movq %%rbx, %0\n":"=m"(item):);
	if (item != (uint64_t)-1 && item != 0) {
		uint64_t dq, invoke_ptr = 0;
		uint64_t vtable_ptr, ctxt, func;
		asm volatile("movq %%r12, %0\n"
				"movq (%%rbx), %%rax\n"
				"movq %%rax, %1\n"
				"movq 0x20(%%rbx), %%rax\n"
				"movq %%rax, %2\n"
				"movq 0x28(%%rbx), %%rax\n"
				"movq %%rax, %3"
				:"=m"(dq), "=m"(vtable_ptr), "=m"(func), "=m"(ctxt):);

		if (vtable_ptr > 0xfful) {
			asm volatile("movq (%%rbx), %%rax\n" 
					"movq 0x40(%%rax), %%rbx\n"
					"movq %%rbx, %0"
					:"=m"(invoke_ptr):);
		}

		kdebug_trace(DISPATCH_DEQUEUE, 2, dq, item, ctxt, 0);
		kdebug_trace(DISPATCH_DEQUEUE, (1UL << 32) | 2, func, invoke_ptr, vtable_ptr, 0);
	}
	restore_registers;
	asm volatile("mov %0, %%rbx"::"m"(item));
}

/*
void shell__dispatch_root_queue_drain_next0()
{
	save_registers;
	uint64_t dq, item, invoke_ptr = 0;
	uint64_t vtable_ptr, ctxt, func;
	asm volatile("movq %%r12, %0\n"
			"movq %%rbx, %1\n"
			"movq (%%rbx), %%rax\n"
			"movq %%rax, %2\n"
			"movq 0x20(%%rbx), %%rax\n"
			"movq %%rax, %3\n"
			"movq 0x28(%%rbx), %%rax\n"
			"movq %%rax, %4"
			:"=m"(dq), "=m"(item), "=m"(vtable_ptr), "=m"(func), "=m"(ctxt):);
	if (vtable_ptr > 0xfful) {
		asm volatile("movq (%%rbx), %%rax\n" 
				"movq 0x40(%%rax), %%rbx\n"
				"movq %%rbx, %0"
				:"=m"(invoke_ptr):);
	}
	kdebug_trace(DISPATCH_DEQUEUE, 3, dq, item, ctxt, 0);
	kdebug_trace(DISPATCH_DEQUEUE, (1UL << 32) | 3, func, invoke_ptr, vtable_ptr, 0);
	restore_registers;
	asm volatile("movq $0x0, (%%r14)"
	::);
}

void shell__dispatch_root_queue_drain_next1()
{
	save_registers;
	uint64_t dq, item, invoke_ptr = 0;
	uint64_t vtable_ptr, ctxt, func;
	asm volatile("movq %%r12, %0\n"
				"movq %%rbx, %1\n"
				"movq (%%rbx), %%rax\n"
				"movq %%rax, %2\n"
				"movq 0x20(%%rbx), %%rax\n"
				"movq %%rax, %3\n"
				"movq 0x28(%%rbx), %%rax\n"
				"movq %%rax, %4"
			:"=m"(dq), "=m"(item), "=m"(vtable_ptr), "=m"(func), "=m"(ctxt):);
	if (vtable_ptr > 0xfful) {
		asm volatile("movq (%%rbx), %%rax\n" 
					"movq 0x40(%%rax), %%rbx\n"
					"movq %%rbx, %0"
			:"=m"(invoke_ptr):);
	}
	kdebug_trace(DISPATCH_DEQUEUE, 4, dq, item, ctxt, 0);
	kdebug_trace(DISPATCH_DEQUEUE,  (1UL << 32) | 4, func, invoke_ptr, vtable_ptr, 0);
	restore_registers;
	asm volatile("movq %%rax, (%%r14)\n"
				"movq (%%r13), %%rax"
			::);
}
*/

void detour_callout__dispatch_root_queue_drain(struct hack_handler *hack_handler_ptr)
{
	detour_function(hack_handler_ptr, "_dispatch_root_queue_drain", 
		shell__dispatch_root_queue_drain, 0xe7, 10);
	/*
	detour_function(hack_handler_ptr, "_dispatch_root_queue_drain", 
		shell__dispatch_root_queue_drain_next0, 0x162, 7);
	detour_function(hack_handler_ptr, "_dispatch_root_queue_drain", 
		shell__dispatch_root_queue_drain_next1, 0x183, 7);
	*/
}


/*5
 * __dispatch_queue_drain:
 * 0000000000006d49	55              	pushq	%rbp
 * 000000000000702c	498b742420      	movq	0x20(%r12), %rsi
 * 0000000000007031	498b7c2428      	movq	0x28(%r12), %rdi
 * 0000000000007036	e8c8b3ffff      	callq	0x2403
 *
 *6
 * __dispatch_queue_drain:
 * 0000000000006d49	55              	pushq	%rbp
 * 000000000000754c	498b742420      	movq	0x20(%r12), %rsi
 * 0000000000007551	498b7c2428      	movq	0x28(%r12), %rdi
 * 0000000000007556	e8a8aeffff      	callq	0x2403
 *
 */
void shell_callout__dispatch_queue_drain()
{
	save_registers;
	uint64_t dq, item, ctxt, func;
	asm volatile("movq %%r14, %0\n"
				"movq %%r12, %1\n"
				"movq 0x20(%%r12), %%rax\n"
				"movq %%rax, %2\n"
				"movq 0x28(%%r12), %%rax\n"
				"movq %%rax, %3"
			:"=m"(dq), "=m"(item), "=m"(func), "=m"(ctxt):);
	kdebug_trace(DISPATCH_DEQUEUE, 5, dq, item, ctxt, 0);
	kdebug_trace(DISPATCH_DEQUEUE,  (1UL << 32) | 5, func, 0, 0, 0);
	restore_registers;
	asm volatile("movq 0x20(%%r12), %%rsi\n"
				"movq 0x28(%%r12), %%rdi"
	::);
}

void detour_callout__dispatch_queue_drain(struct hack_handler *hack_handler_ptr)
{
	detour_function(hack_handler_ptr, "_dispatch_queue_drain", 
		shell_callout__dispatch_queue_drain, 0x2e3, 8);
	detour_function(hack_handler_ptr, "_dispatch_queue_drain", 
		shell_callout__dispatch_queue_drain, 0x803, 8);
}

/*21
 * __dispatch_main_queue_callback_4CF:
 * 0000000000015587	55              	pushq	%rbp
 * 0000000000015a00	498b7520        	movq	0x20(%r13), %rsi
 * 0000000000015a04	498b7d28        	movq	0x28(%r13), %rdi
 * 0000000000015a08	e8f6c9feff      	callq	0x2403
 * calculateing dispath mainq address:
 * (0x155f2 + 7 + 0x27447 - 0x15a05) (%%ret) = 0x2703b(%%ret)
 * or gs:0xa0
 *22
 * __dispatch_main_queue_callback_4CF:
 * 0000000000015587	55              	pushq	%rbp
 * 0000000000015c0f	498b7520        	movq	0x20(%r13), %rsi
 * 0000000000015c13	498b7d28        	movq	0x28(%r13), %rdi
 * 0000000000015c17	e8e7c7feff      	callq	0x2403
 * calculateing dispath mainq address:
 * (0x155f2 + 7 + 0x27447 - 0x15c14) (%%ret) = 0x26e2c(%%ret)
 * or gs:0xa0
 */
void shell_callout__dispatch_main_queue_callback_4CF_1()
{
	save_registers;
	uint64_t dq, item, ctxt, func; //, check_dq;
	asm volatile("movq 0x8(%%rbp), %%rax\n"
				"addq 0x2703b, %%rax\n"
				//"movq %%rax, %4\n"
				//"movq %%gs:0xa0, %%rax\n"
				"movq %%rax, %0\n"
				"movq %%r13, %1\n"
				"movq 0x20(%%r13), %%rax\n"
				"movq %%rax, %2\n"
				"movq 0x28(%%r13), %%rax\n"
				"movq %%rax, %3"
			:"=m"(dq), "=m"(item), "=m"(func), "=m"(ctxt));//, "=m"(check_dq):);

	kdebug_trace(DISPATCH_DEQUEUE, 21, dq, item, ctxt, 0);
	kdebug_trace(DISPATCH_DEQUEUE,  (1UL << 32) | 21, func, 0, 0, 0);

	restore_registers;
	asm volatile("movq 0x20(%%r13), %%rsi\n"
				"movq 0x28(%%r13), %%rdi"
	::);
}

void shell_callout__dispatch_main_queue_callback_4CF_2()
{
	save_registers;
	uint64_t dq, item, ctxt, func;
	asm volatile("movq 0x8(%%rbp), %%rax\n"
				 "addq $0x26e2c, %%rax\n"
				//"movq %%gs:0xa0, %%rax\n"
				"movq %%rax, %0\n"
				"movq %%r13, %1\n"
				"movq 0x20(%%r13), %%rax\n"
				"movq %%rax, %2\n"
				"movq 0x28(%%r13), %%rax\n"
				"movq %%rax, %3"
			:"=m"(dq), "=m"(item), "=m"(func), "=m"(ctxt):);

	kdebug_trace(DISPATCH_DEQUEUE, 22, dq, item, ctxt, 0);
	kdebug_trace(DISPATCH_DEQUEUE,  (1UL << 32) | 22, func, 0, 0, 0);

	restore_registers;
	asm volatile("movq 0x20(%%r13), %%rsi\n"
				"movq 0x28(%%r13), %%rdi"
	::);
}


void detour_callout__dispatch_main_queue_callback_4CF(struct hack_handler *hack_handler_ptr)
{
	detour_function(hack_handler_ptr, "_dispatch_main_queue_callback_4CF", 
		shell_callout__dispatch_main_queue_callback_4CF_1, 0x479, 8);
	detour_function(hack_handler_ptr, "_dispatch_main_queue_callback_4CF", 
		shell_callout__dispatch_main_queue_callback_4CF_2, 0x688, 8);
}

/*23
 * __dispatch_runloop_root_queue_perform_4CF:
 * 000000000001766c	55              	pushq	%rbp
 * 0000000000017ade	488b7320        	movq	0x20(%rbx), %rsi
 * 0000000000017ae2	488b7b28        	movq	0x28(%rbx), %rdi
 * 0000000000017ae6	e818a9feff      	callq	0x2403
 *24
 * __dispatch_runloop_root_queue_perform_4CF:
 * 000000000001766c	55              	pushq	%rbp
 * 0000000000017e27	488b7320        	movq	0x20(%rbx), %rsi
 * 0000000000017e2b	488b7b28        	movq	0x28(%rbx), %rdi
 * 0000000000017e2f	e8cfa5feff      	callq	0x2403
 *
void shell_callout__dispatch_runloop_root_queue_perform_4CF()
{
	save_registers;
	uint64_t dq,item, ctxt, func;
	asm volatile("movq %%r15, %0\n"
				"movq %%rbx, %1\n"
				"movq 0x20(%%rbx), %%rax\n"
				"movq %%rax, %2\n"
				"movq 0x28(%%rbx), %%rax\n"
				"movq %%rax, %3"
			:"=m"(dq), "=m"(item), "=m"(func), "=m"(ctxt):);

	kdebug_trace(DISPATCH_DEQUEUE, 23, dq, item, ctxt, 0);
	kdebug_trace(DISPATCH_DEQUEUE,  (1 << 32) | 23, func, 0, 0, 0);
	restore_registers;
	asm volatile("movq 0x20(%%rbx), %%rsi\n"
				"movq 0x28(%%rbx), %%rdi"
	::);
}

void detour_callout__dispatch_runloop_root_queue_perform_4CF(struct hack_handler *hack_handler_ptr)
{
	detour_function(hack_handler_ptr, "_dispatch_runloop_root_queue_perform_4CF", 
		shell_callout__dispatch_runloop_root_queue_perform_4CF, 0x472, 8);
	detour_function(hack_handler_ptr, "_dispatch_runloop_root_queue_perform_4CF", 
		shell_callout__dispatch_runloop_root_queue_perform_4CF, 0x7bb, 8);
}
*/

#endif

void detour_dequeue(struct hack_handler * hack_handler_ptr)
{
	#if defined(__LP64__)
	detour_callout__dispatch_root_queue_drain(hack_handler_ptr);
	detour_callout__dispatch_queue_drain(hack_handler_ptr);
	detour_callout__dispatch_main_queue_callback_4CF(hack_handler_ptr);
	#endif
}
