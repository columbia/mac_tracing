#include "lib_mach_info.h"
//#define DISPATCH_EXECUTE 0x210a0010
//#define CALLOUT_BEGIN 0
//#define CALLOUT_END 1

/* __dispatch_client_callout:
 * begin
 * 0000000000002403    55                  pushq   %rbp
 * ->0000000000002404    4889e5              movq    %rsp, %rbp
 * ->0000000000002407    53                  pushq   %rbx 
 * ->0000000000002408    50                  pushq   %rax
 */

#define BACK_TRACE_BUFFER 17
#define MSG_BACKTRACE   0x29000094

#if defined(__LP64__)

static void back_trace(uint64_t func)
{
 	void *callstack[BACK_TRACE_BUFFER] = {(void *)0};
	uint32_t frame_index = 0;
	vm_offset_t stackptr, stackptr_prev, raddr;

	__asm__ volatile("movq %%rbp, %0" : "=m"(stackptr));

	for(frame_index = 0; frame_index < BACK_TRACE_BUFFER; frame_index++) {
		stackptr_prev = stackptr;
		stackptr = *((vm_offset_t *) stackptr_prev);
		if (stackptr < stackptr_prev)
			break;
		raddr = *((vm_offset_t *)(stackptr + 8));
		if (raddr < 4096)
			break;
		callstack[frame_index] = (void*) raddr;
	}

	kdebug_trace(MSG_BACKTRACE, func, frame_index, callstack[0], callstack[1], 0);
 	for (int i = 2; i < frame_index && i + 2 < BACK_TRACE_BUFFER; i += 3) {
		if (callstack[i] == (void*)0)
			break;
		kdebug_trace(MSG_BACKTRACE, func, callstack[i], callstack[i + 1], callstack[i + 2], 0);
	}
}

void shell_dispatch_client_callout_begin(void * ctxt, dispatch_function_t f)
{
	save_registers;
	back_trace(f);
	kdebug_trace(DISPATCH_EXECUTE|CALLOUT_BEGIN, f, ctxt, CALLOUT_BEGIN, 0, 0);
	restore_registers;
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
	save_registers;
	kdebug_trace(DISPATCH_EXECUTE|CALLOUT_END, func, 0, CALLOUT_END, 0, 0);
	restore_registers;
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
#endif

void detour_blockinvoke(struct hack_handler * hack_handler_ptr)
{
	#if defined(__LP64__)
	detour_function(hack_handler_ptr, "_dispatch_client_callout", shell_dispatch_client_callout_begin, 1, 5);
	detour_function(hack_handler_ptr, "_dispatch_client_callout", shell_dispatch_client_callout_end, 8, 5);
	#endif
}
