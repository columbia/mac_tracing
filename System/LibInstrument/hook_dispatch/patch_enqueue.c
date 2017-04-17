#include "lib_mach_info.h"
#include <dispatch/dispatch.h>

#if defined(__LP64__)
#define DISPATCH_ENQUEUE 0x210a0008

/* 2.
** __dispatch_queue_wakeup_with_qos_slow:                    
** 0000000000003a88    55                  pushq   %rbp      
** 0000000000003de3    4889c1              movq    %rax, %rcx                                                               
** -->0000000000003de6    49874c3c40          xchgq   %rcx, 0x40(%r12,%rdi)
** 0000000000003deb    4885c9              testq   %rcx, %rcx
*/
void shell__dispatch_queue_wakeup_with_qos_slow()
{
	save_registers;
	uint64_t dq, item;
	asm volatile("movq %%rcx, %0\n"
				"movq %%r12, %%rax\n"
				"addq %%rdi, %%rax\n"
				"movq %%rax, %1"
			:"=m"(item), "=m"(dq):);
	kdebug_trace(DISPATCH_ENQUEUE, dq, item, 0x2, 0x2, 0);
	restore_registers;
	asm volatile("xchgq %%rcx, 0x40(%%r12, %%rdi)"::);
}

void detour__dispatch_queue_wakeup_with_qos_slow(struct hack_handler *hack_handler_ptr)
{
	detour_function(hack_handler_ptr, "_dispatch_queue_wakeup_with_qos_slow", 
		shell__dispatch_queue_wakeup_with_qos_slow, 0x35e, 5);
}

/* 3
 *  __dispatch_barrier_async_detached_f:                      
 * 0000000000004f33    4889d0              movq    %rdx, %rax
 * ->0000000000004fcc    4889d1              movq    %rdx, %rcx
 * ->0000000000004fcf    48874f40            xchgq   %rcx, 0x40(%rdi)
 * 0000000000004fd3    4885c9              testq   %rcx, %rcx
 */
void shell__dispatch_barrier_async_detached_f()
{
	save_registers;
	uint64_t dq, item;
	asm volatile("movq %%rdx, %0\n"
			"movq  %%rdi, %1"
			:"=m"(item), "=m"(dq):);
	kdebug_trace(DISPATCH_ENQUEUE, dq, item, 0x3, 0x3, 0);
	restore_registers;
	asm volatile(	"movq %%rdx, %%rcx\n"
					"xchgq %%rcx, 0x40(%%rdi)"
				::);
}

void detour__dispatch_barrier_async_detached_f(struct hack_handler *hack_handler_ptr)
{
	detour_function(hack_handler_ptr, "_dispatch_barrier_async_detached_f", 
		shell__dispatch_barrier_async_detached_f, 0x99, 7);
}

/* 4
 * __dispatch_barrier_async_f_slow:
 * 0000000000004ffe    55                  pushq   %rbp
 *-> 0000000000005156    4889c2              movq    %rax, %rdx
 *-> 0000000000005159    49875640            xchgq   %rdx, 0x40(%r14)
 * 000000000000515d    4885d2              testq   %rdx, %rdx
 */
void shell__dispatch_barrier_async_f_slow()
{
	save_registers;
	uint64_t dq, item;
	asm volatile("movq %%rax, %0\n"
			"movq %%r14, %1"
			:"=m"(item), "=m"(dq):);
	kdebug_trace(DISPATCH_ENQUEUE, dq, item, 0x4, 0x4, 0);
	restore_registers;
	asm volatile("movq %%rax, %%rdx\n"
				"xchgq %%rdx, 0x40(%%r14)"
				::);
}

void detour__dispatch_barrier_async_f_slow(struct hack_handler *hack_handler_ptr)
{
	detour_function(hack_handler_ptr, "_dispatch_barrier_async_f_slow", 
		shell__dispatch_barrier_async_f_slow, 0x158, 7);
}

/* 5
 * __dispatch_wakeup:
 * 0000000000005379    55                  pushq   %rbp
 * ->0000000000005401    4889d8              movq    %rbx, %rax
 * ->0000000000005404    49874640            xchgq   %rax, 0x40(%r14)
 * 0000000000005408    4885c0              testq   %rax, %rax
 */
void shell__dispatch_wakeup()
{
	save_registers;
	uint64_t dq, item;
	asm volatile("movq %%rbx, %0\n"
			"movq %%r14, %1"
			:"=m"(item), "=m"(dq):);
	kdebug_trace(DISPATCH_ENQUEUE, dq, item, 0x5, 0x5, 0);
	restore_registers;
	asm volatile("movq %%rbx, %%rax\n"
				"xchgq %%rax, 0x40(%%r14)"
				::);
}

void detour__dispatch_wakeup(struct hack_handler *hack_handler_ptr)
{
	detour_function(hack_handler_ptr, "_dispatch_wakeup", 
		shell__dispatch_wakeup, 0x88, 7);
}

/* 6
 * __dispatch_queue_push_queue: 
 * 0000000000005508    55                  pushq   %rbp
 * ->0000000000005655    4889d9              movq    %rbx, %rcx
 * ->0000000000005658    49874e40            xchgq   %rcx, 0x40(%r14)
 * 000000000000565c    4885c9              testq   %rcx, %rcx
 *
 * 7
 * 00000000000056ca    4889c1              movq    %rax, %rcx
 * ->00000000000056cd    4b874c3c40          xchgq   %rcx, 0x40(%r12,%r15)
 * 00000000000056d2    4885c9              testq   %rcx, %rcx
 */
void shell__dispatch_queue_push_queue_1()
{
	save_registers;
	uint64_t dq, item;
	asm volatile("movq %%rbx, %0\n"
			"movq %%r14, %1"
			:"=m"(item), "=m"(dq):);
	kdebug_trace(DISPATCH_ENQUEUE, dq, item, 0x6, 0x6, 0);
	restore_registers;
	asm volatile("movq %%rbx, %%rcx\n"
				"xchgq %%rcx, 0x40(%%r14)"
				::);
}

void shell__dispatch_queue_push_queue_2()
{
	save_registers;
	uint64_t dq, item, do_func;
	asm volatile("movq %%rcx, %0\n"
			"movq %%r12, %%rax\n"
			"addq %%r15, %%rax\n"
			"movq %%rax, %1\n"
			"movq 0x20(%%rcx), %%rax\n"
			"movq %%rax, %2"
			:"=m"(item), "=m"(dq), "=m"(do_func):);
	kdebug_trace(DISPATCH_ENQUEUE, dq, item, 0x7, do_func, 0);
	restore_registers;
	asm volatile("xchgq %%rcx, 0x40(%%r12, %%r15)" ::);
}

void detour__dispatch_queue_push_queue(struct hack_handler *hack_handler_ptr)
{
	detour_function(hack_handler_ptr, "_dispatch_queue_push_queue", 
		shell__dispatch_queue_push_queue_1, 0x14d, 7);
	detour_function(hack_handler_ptr, "_dispatch_queue_push_queue", 
		shell__dispatch_queue_push_queue_2, 0x1c5, 5);
}

/* 10
 * __dispatch_kq_init:                                       
 * 0000000000006594    55                  pushq   %rbp      
 * ->0000000000006619    4889c1              movq    %rax, %rcx
 * ->000000000000661c    48874f40            xchgq   %rcx, 0x40(%rdi)
 * 0000000000006620    4885c9              testq   %rcx, %rcx
 */
void shell__dispatch_kq_init()
{
	save_registers;
	uint64_t dq, item;
	asm volatile("movq %%rax, %0\n"
			"movq %%rdi, %1"
			:"=m"(item), "=m"(dq):);
	kdebug_trace(DISPATCH_ENQUEUE, dq, item, 0xa, 0xa, 0);
	restore_registers;
	asm volatile("movq %%rax, %%rcx\n"
				"xchgq %%rcx, 0x40(%%rdi)"
				::);
}

void detour__dispatch_kq_init(struct hack_handler *hack_handler_ptr)
{
	detour_function(hack_handler_ptr, "_dispatch_kq_init", 
		shell__dispatch_kq_init, 0x85, 7);
}


/* 11
 * _dispatch_mach_receive_barrier_f:                         
 * 00000000000089d2    55                  pushq   %rbp      
 * ->0000000000008ad0    4889c2              movq    %rax, %rdx
 * ->0000000000008ad3    49875640            xchgq   %rdx, 0x40(%r14)
 * 0000000000008ad7    4885d2              testq   %rdx, %rdx
 */
void shell_dispatch_mach_receive_barrier_f()
{
	save_registers;
	uint64_t dq, item;
	asm volatile("movq %%rax, %0\n"
			"movq %%r14, %1"
			:"=m"(item), "=m"(dq):);
	kdebug_trace(DISPATCH_ENQUEUE, dq, item, 0xb, 0xb, 0);
	restore_registers;
	asm volatile("movq %%rax, %%rdx\n"
				"xchgq %%rdx, 0x40(%%r14)"
				::);
}

void detour_dispatch_mach_receive_barrier_f(struct hack_handler *hack_handler_ptr)
{
	detour_function(hack_handler_ptr, "dispatch_mach_receive_barrier_f", 
		shell_dispatch_mach_receive_barrier_f, 0xfe, 7);
}

/* 12
 * _dispatch_async_f:
 * 0000000000008b29    0fb74750            movzwl  0x50(%rdi), %eax
 * ->0000000000008c73    4889c6              movq    %rax, %rsi
 * ->0000000000008c76    48877740            xchgq   %rsi, 0x40(%rdi)
 * 0000000000008c7a    4885f6              testq   %rsi, %rsi
 */
void shell_dispatch_async_f()
{
	save_registers;
	uint64_t dq, item;
	asm volatile("movq %%rax, %0\n"
			"movq %%rdi, %1"
			:"=m"(item), "=m"(dq):);
	kdebug_trace(DISPATCH_ENQUEUE, dq, item, 0xc, 0xc, 0);
	restore_registers;
	asm volatile("movq %%rax, %%rsi\n"
				"xchgq %%rsi, 0x40(%%rdi)"
				::);
}

void detour_dispatch_async_f(struct hack_handler *hack_handler_ptr)
{
	detour_function(hack_handler_ptr, "dispatch_async_f", 
		shell_dispatch_async_f, 0x14a, 7);
}

/* 13
 * __dispatch_async_f_slow:
 * 0000000000008cae    55                  pushq   %rbp
 * ->0000000000008e29    4889c2              movq    %rax, %rdx
 * ->0000000000008e2c    49875640            xchgq   %rdx, 0x40(%r14)
 * 0000000000008e30    4885d2              testq   %rdx, %rdx
 */
void shell__dispatch_async_f_slow()
{
	save_registers;
	uint64_t dq, item;
	asm volatile("movq %%rax, %0\n"
			"movq %%r14, %1"
			:"=m"(item), "=m"(dq):);
	kdebug_trace(DISPATCH_ENQUEUE, dq, item, 0xd, 0xd, 0);
	restore_registers;
	asm volatile("movq %%rax, %%rdx\n"
				"xchgq %%rdx, 0x40(%%r14)"
				::);
}

void detour__dispatch_async_f_slow(struct hack_handler *hack_handler_ptr)
{
	detour_function(hack_handler_ptr, "_dispatch_async_f_slow", 
		shell__dispatch_async_f_slow, 0x17b, 7);
}

/* 14
 * _dispatch_mach_send: 
 * 00000000000097d6    55                  pushq   %rbp      
 * ->00000000000098f5    4889c2              movq    %rax, %rdx
 * ->00000000000098f8    48875140            xchgq   %rdx, 0x40(%rcx)
 * 00000000000098fc    4885d2              testq   %rdx, %rdx
 * 15                                                        
 * ->0000000000009916    4c89f9              movq    %r15, %rcx
 * ->0000000000009919    48874840            xchgq   %rcx, 0x40(%rax)
 * 000000000000991d    4885c9              testq   %rcx, %rcx
 *
void shell_dispatch_mach_send_1()
{
	save_registers;
	uint64_t dq, item;
	asm volatile("movq %%rax, %0\n"
			"movq %%rcx, %1"
			:"=m"(item), "=m"(dq):);
	kdebug_trace(DISPATCH_ENQUEUE, dq, item, 0xe, 0xe, 0);
	restore_registers;
	asm volatile("movq %%rax, %%rdx\n"
				"xchgq %%rdx, 0x40(%%rcx)"
				::);
}

void shell_dispatch_mach_send_2()
{
	save_registers;
	uint64_t dq, item;
	asm volatile("movq %%r15, %0\n"
			"movq %%rax, %1"
			:"=m"(item), "=m"(dq):);
	kdebug_trace(DISPATCH_ENQUEUE, dq, item, 0xf, 0xf, 0);
	restore_registers;
	asm volatile("movq %%r15, %%rcx\n"
				"xchgq %%rcx, 0x40(%%rax)"
				::);
}

void detour_dispatch_mach_send(struct hack_handler *hack_handler_ptr)
{
     detour_function(hack_handler_ptr, "dispatch_mach_send",
         shell_dispatch_mach_send_1, 0x11f, 7);
     detour_function(hack_handler_ptr, "dispatch_mach_send",
         shell_dispatch_mach_send_2, 0x140, 7);
}
*/

/* 16
 * __dispatch_mach_msg_send: 
 * 000000000000999f    55                  pushq   %rbp      
 * ->000000000000a113    4c89e1              movq    %r12, %rcx
 * ->000000000000a116    49874d40            xchgq   %rcx, 0x40(%r13)
 * 000000000000a11a    4885c9              testq   %rcx, %rcx
 * 17
 * ->000000000000a1b5    4889d9              movq    %rbx, %rcx                     
 * ->000000000000a1b8    48870a              xchgq   %rcx, (%rdx)
 * 000000000000a1bb    4885c9              testq   %rcx, %rcx
 */
void shell__dispatch_mach_msg_send_1()
{
	save_registers;
	uint64_t dq, item;
	asm volatile("movq %%r12, %0\n"
			"movq %%r13, %1"
			:"=m"(item), "=m"(dq):);
	kdebug_trace(DISPATCH_ENQUEUE, dq, item, 0x10, 0x10, 0);
	restore_registers;
	asm volatile("movq %%r12, %%rcx\n"
				"xchgq %%rcx, 0x40(%%r13)"
				::);
}

void shell__dispatch_mach_msg_send_2()
{
	save_registers;
	uint64_t dq, item;
	asm volatile("movq %%rbx, %0\n"
			"movq %%rdx, %%rax\n"
			"subq $0x40, %%rax\n"
			"movq %%rax, %1"
			:"=m"(item), "=m"(dq):);
	kdebug_trace(DISPATCH_ENQUEUE, dq, item, 0x11, 0x11, 0);
	restore_registers;
	asm volatile("movq %%rbx, %%rcx\n"
				"xchgq %%rcx, (%%rdx)"
				::);
}

void detour__dispatch_mach_msg_send(struct hack_handler *hack_handler_ptr)
{
     detour_function(hack_handler_ptr, "_dispatch_mach_msg_send",
         shell__dispatch_mach_msg_send_1, 0x774, 7);
     detour_function(hack_handler_ptr, "_dispatch_mach_msg_send",
         shell__dispatch_mach_msg_send_2, 0x816, 6);
}

/* 18
 * __dispatch_mach_send_drain:
 * 000000000000aa98    55                  pushq   %rbp      
 * ->000000000000ac04    4c89e9              movq    %r13, %rcx
 * ->000000000000ac07    49874e40            xchgq   %rcx, 0x40(%r14)
 * 000000000000ac0b    4885c9              testq   %rcx, %rcx
 *
 */
void shell__dispatch_mach_send_drain()
{
	save_registers;
	uint64_t dq, item;
	asm volatile("movq %%r13, %0\n"
			"movq %%r14, %1"
			:"=m"(item), "=m"(dq):);
	kdebug_trace(DISPATCH_ENQUEUE, dq, item, 0x12, 0x12, 0);
	restore_registers;
	asm volatile("movq %%r13, %%rcx\n"
				"xchgq %%rcx, 0x40(%%r14)"
				::);
}

void detour__dispatch_mach_send_drain(struct hack_handler *hack_handler_ptr)
{
     detour_function(hack_handler_ptr, "_dispatch_mach_send_drain",
         shell__dispatch_mach_send_drain, 0x16c, 7);
}

/* 19
 * __dispatch_mach_msg_recv:
 * 000000000000b7a9    55                  pushq   %rbp      
 * ->000000000000b92b    4889c1              movq    %rax, %rcx
 * ->000000000000b92e    49874e40            xchgq   %rcx, 0x40(%r14)
 * 000000000000b932    4885c9              testq   %rcx, %rcx
 */
void shell__dispatch_mach_msg_recv()
{
	save_registers;
	uint64_t dq, item;
	asm volatile("movq %%rax, %0\n"
			"movq %%r14, %1"
			:"=m"(item), "=m"(dq):);
	kdebug_trace(DISPATCH_ENQUEUE, dq, item, 0x13, 0x13, 0);
	restore_registers;
	asm volatile("movq %%rax, %%rcx\n"
				"xchgq %%rcx, 0x40(%%r14)"
				::);
}

void detour__dispatch_mach_msg_recv(struct hack_handler *hack_handler_ptr)
{
     detour_function(hack_handler_ptr, "_dispatch_mach_msg_recv",
         shell__dispatch_mach_msg_recv, 0x182, 7);
}

/* 20
 * __dispatch_mach_reply_kevent_unregister
 * 000000000000b980    55                  pushq   %rbp      
 * ->000000000000bae4    4c89f9              movq    %r15, %rcx
 * ->000000000000bae7    49874e40            xchgq   %rcx, 0x40(%r14)
 * 000000000000baeb    4885c9              testq   %rcx, %rcx
 */
void shell__dispatch_mach_reply_kevent_unregister()
{
	save_registers;
	uint64_t dq, item;
	asm volatile("movq %%r15, %0\n"
			"movq %%r14, %1"
			:"=m"(item), "=m"(dq):);
	kdebug_trace(DISPATCH_ENQUEUE, dq, item, 0x14, 0x14, 0);
	restore_registers;
	asm volatile("movq %%r15, %%rcx\n"
				"xchgq %%rcx, 0x40(%%r14)"
				::);
}
void detour__dispatch_mach_reply_kevent_unregister(struct hack_handler *hack_handler_ptr)
{
     detour_function(hack_handler_ptr, "_dispatch_mach_reply_kevent_unregister",
         shell__dispatch_mach_reply_kevent_unregister, 0x164, 7);
}

/* 21
 * _dispatch_async_enforce_qos_class_f:
 * 000000000000bbb6    0fb74750            movzwl  0x50(%rdi), %eax
 * ->000000000000bcd3    4889c6              movq    %rax, %rsi
 * ->000000000000bcd6    48877740            xchgq   %rsi, 0x40(%rdi)
 * 000000000000bcda    4885f6              testq   %rsi, %rsi
 */
void shell_dispatch_async_enforce_qos_class_f()
{
	save_registers;
	uint64_t dq, item;
	asm volatile("movq %%rax, %0\n"
			"movq %%rdi, %1"
			:"=m"(item), "=m"(dq):);
	kdebug_trace(DISPATCH_ENQUEUE, dq, item, 0x15, 0x15, 0);
	restore_registers;
	asm volatile("movq %%rax, %%rsi\n"
				"xchgq %%rsi, 0x40(%%rdi)"
				::);
}

void detour_dispatch_async_enforce_qos_class_f(struct hack_handler *hack_handler_ptr)
{
     detour_function(hack_handler_ptr, "dispatch_async_enforce_qos_class_f",
         shell_dispatch_async_enforce_qos_class_f, 0x11d, 7);
}

/* 25
 * _dispatch_async:
 * 000000000000d050    55                  pushq   %rbp
 * ->000000000000d261    4889d1              movq    %rdx, %rcx
 * ->000000000000d264    49874e40            xchgq   %rcx, 0x40(%r14)
 * 000000000000d268    4885c9              testq   %rcx, %rcx
 */
void shell_dispatch_async()
{
	save_registers;
	uint64_t dq, item;
	asm volatile("movq %%rdx, %0\n"
			"movq %%r14, %1"
			:"=m"(item), "=m"(dq):);
	kdebug_trace(DISPATCH_ENQUEUE, dq, item, 0x19, 0x19, 0);
	restore_registers;
	asm volatile("movq %%rdx, %%rcx\n"
				"xchgq %%rcx, 0x40(%%r14)"
				::);
}

void detour_dispatch_async(struct hack_handler *hack_handler_ptr)
{
     detour_function(hack_handler_ptr, "dispatch_async",
         shell_dispatch_async, 0x211, 7);
}

/* 26
 * __dispatch_barrier_async_f
 * 000000000000d33f    4989d1              movq    %rdx, %r9 
 * ->000000000000d4a8    4889d6              movq    %rdx, %rsi
 * ->000000000000d4ab    48877740            xchgq   %rsi, 0x40(%rdi)
 * 000000000000d4af    4885f6              testq   %rsi, %rsi
 */

void shell__dispatch_barrier_async_f()
{
	save_registers;
	uint64_t dq, item;
	asm volatile("movq %%rdx, %0\n"
			"movq %%rdi, %1"
			:"=m"(item), "=m"(dq):);
	kdebug_trace(DISPATCH_ENQUEUE, dq, item, 0x1a, 0x1a, 0);
	restore_registers;
	asm volatile("movq %%rdx, %%rsi\n"
				"xchgq %%rsi, 0x40(%%rdi)"
				::);
}

void detour__dispatch_barrier_async_f(struct hack_handler *hack_handler_ptr)
{
     detour_function(hack_handler_ptr, "_dispatch_barrier_async_f",
         shell__dispatch_barrier_async_f, 0x169, 7);
}

/* 27
 * _dispatch_mach_send_barrier_f
 * 000000000000d974    55                  pushq   %rbp     
 * 000000000000da94    48c7421000000000    movq    $0x0, 0x10(%rdx)`
 * ->000000000000da9c    4889d0              movq    %rdx, %rax
 * ->000000000000da9f    48874140            xchgq   %rax, 0x40(%rcx)
 * 000000000000daa3    4885c0              testq   %rax, %rax
 * 28
 * 000000000000dacf    48c7421000000000    movq    $0x0, 0x10(%rdx)
 * ->000000000000dad7    4889d1              movq    %rdx, %rcx
 * ->000000000000dada    49874e40            xchgq   %rcx, 0x40(%r14)
 * 000000000000dade    4885c9              testq   %rcx, %rcx
 */
void shell_dispatch_mach_send_barrier_f_1()
{
	save_registers;
	uint64_t dq, item;
	asm volatile("movq %%rdx, %0\n"
			"movq %%rcx, %1"
			:"=m"(item), "=m"(dq):);
	kdebug_trace(DISPATCH_ENQUEUE, dq, item, 0x1b, 0x1b, 0);
	restore_registers;
	asm volatile("movq %%rdx, %%rax\n"
				"xchgq %%rax, 0x40(%%rcx)"
				::);
}

void shell_dispatch_mach_send_barrier_f_2()
{
	save_registers;
	uint64_t dq, item;
	asm volatile("movq %%rdx, %0\n"
			"movq %%r14, %1"
			:"=m"(item), "=m"(dq):);
	kdebug_trace(DISPATCH_ENQUEUE, dq, item, 0x1c, 0x1c, 0);
	restore_registers;
	asm volatile("movq %%rdx, %%rcx\n"
				"xchgq %%rcx, 0x40(%%r14)"
				::);
}

void detour_dispatch_mach_send_barrier_f(struct hack_handler *hack_handler_ptr)
{
     detour_function(hack_handler_ptr, "dispatch_mach_send_barrier_f",
         shell_dispatch_mach_send_barrier_f_1, 0x128, 7);
     detour_function(hack_handler_ptr, "dispatch_mach_send_barrier_f",
        shell_dispatch_mach_send_barrier_f_2, 0x163, 7);
}

/* 30
 * __dispatch_mach_cancel
 * 000000000000df55    55                  pushq   %rbp
 * ->000000000000e050    4889c2              movq    %rax, %rdx
 * ->000000000000e053    48875340            xchgq   %rdx, 0x40(%rbx)
 * 000000000000e057    4885d2              testq   %rdx, %rdx
 */
void shell__dispatch_mach_cancel()
{
	save_registers;
	uint64_t dq, item;
	asm volatile("movq %%rax, %0\n"
			"movq %%rbx, %1"
			:"=m"(item), "=m"(dq):);
	kdebug_trace(DISPATCH_ENQUEUE, dq, item, 0x1e, 0x1e, 0);
	restore_registers;
	asm volatile("movq %%rax, %%rdx\n"
				"xchgq %%rdx, 0x40(%%rbx)"
				::);
}

void detour__dispatch_mach_cancel(struct hack_handler *hack_handler_ptr)
{
     detour_function(hack_handler_ptr, "_dispatch_mach_cancel",
         shell__dispatch_mach_cancel, 0xfb, 7);
}

/* 31
 * __dispatch_mach_disconnect
 * 000000000000e08b    55                  pushq   %rbp      
 * ->000000000000e16b    4889c2              movq    %rax, %rdx
 * ->000000000000e16e    4987542440          xchgq   %rdx, 0x40(%r12)
 * 000000000000e173    4885d2              testq   %rdx, %rdx  
 */
void shell__dispatch_mach_disconnect()
{
	save_registers;
	uint64_t dq, item;
	asm volatile("movq %%rax, %0\n"
			"movq %%r12, %1"
			:"=m"(item), "=m"(dq):);
	kdebug_trace(DISPATCH_ENQUEUE, dq, item, 0x1f, 0x1f, 0);
	restore_registers;
	asm volatile("movq %%rax, %%rdx\n"
				"xchgq %%rdx, 0x40(%%r12)"
				::);
}

void detour__dispatch_mach_disconnect(struct hack_handler *hack_handler_ptr)
{
     detour_function(hack_handler_ptr, "_dispatch_mach_disconnect",
         shell__dispatch_mach_disconnect, 0xe0, 7);
}

/* 32
 * _dispatch_group_async
 * 000000000000f1c7    55                  pushq   %rbp      
 * ->000000000000f3d1    4889c2              movq    %rax, %rdx
 * ->000000000000f3d4    49875640            xchgq   %rdx, 0x40(%r14)
 * 000000000000f3d8    4885d2              testq   %rdx, %rdx
 */
void shell_dispatch_group_async()
{
	save_registers;
	uint64_t dq, item;
	asm volatile("movq %%rax, %0\n"
			"movq %%r14, %1"
			:"=m"(item), "=m"(dq):);
	kdebug_trace(DISPATCH_ENQUEUE, dq, item, 0x20, 0x20, 0);
	restore_registers;
	asm volatile("movq %%rax, %%rdx\n"
				"xchgq %%rdx, 0x40(%%r14)"
				::);
}

void detour_dispatch_group_async(struct hack_handler *hack_handler_ptr)
{
     detour_function(hack_handler_ptr, "dispatch_group_async",
         shell_dispatch_group_async, 0x20a, 7);
}

/* 33
 * _dispatch_group_notify_f
 * 000000000000f6b9    55                  pushq   %rbp     
 * ->000000000000f729    4889c1              movq    %rax, %rcx
 * ->000000000000f72c    49874e58            xchgq   %rcx, 0x58(%r14)
 * 000000000000f730    4885c9              testq   %rcx, %rcx
 *
void shell_dispatch_group_notify_f()
{
	save_registers;
	uint64_t dq, item;
	asm volatile("movq %%rax, %0\n"
			"movq %%r14, %1"
			:"=m"(item), "=m"(dq):);
	kdebug_trace(DISPATCH_ENQUEUE, dq, item, 0x21, 0x21, 0);
	restore_registers;
	asm volatile("movq %%rax, %%rcx\n"
				"xchgq %%rcx, 0x58(%%r14)"
				::);
}

void detour_dispatch_group_notify_f(struct hack_handler *hack_handler_ptr)
{
     detour_function(hack_handler_ptr, "dispatch_group_notify_f",
         shell_dispatch_group_notify_f, 0x70, 7);
}
*/

/* 35
 * __dispatch_barrier_sync_f_slow
 * 0000000000010742    55                  pushq   %rbp      
 * ->000000000001095c    4889ca              movq    %rcx, %rdx
 * ->000000000001095f    48875340            xchgq   %rdx, 0x40(%rbx)
 * 0000000000010963    4885d2              testq   %rdx, %rdx  
 */
void shell__dispatch_barrier_sync_f_slow()
{
	save_registers;
	uint64_t dq, item;
	asm volatile("movq %%rcx, %0\n"
			"movq %%rbx, %1"
			:"=m"(item), "=m"(dq):);
	kdebug_trace(DISPATCH_ENQUEUE, dq, item, 0x23, 0x23, 0);
	restore_registers;
	asm volatile("movq %%rcx, %%rdx\n"
				"xchgq %%rdx, 0x40(%%rbx)"
				::);
}

void detour__dispatch_barrier_sync_f_slow(struct hack_handler *hack_handler_ptr)
{
     detour_function(hack_handler_ptr, "_dispatch_barrier_sync_f_slow",
         shell__dispatch_barrier_sync_f_slow, 0x21a, 7);
}

/* 37
 * _dispatch_group_async_f
 * 000000000001393c    55                  pushq   %rbp      
 * ->0000000000013ac5    4889c6              movq    %rax, %rsi
 * ->0000000000013ac8    49877640            xchgq   %rsi, 0x40(%r14)
 * 0000000000013acc    4885f6              testq   %rsi, %rsi
 */
void shell_dispatch_group_async_f()
{
	save_registers;
	uint64_t dq, item;
	asm volatile("movq %%rax, %0\n"
			"movq %%r14, %1"
			:"=m"(item), "=m"(dq):);
	kdebug_trace(DISPATCH_ENQUEUE, dq, item, 0x25, 0x25, 0);
	restore_registers;
	asm volatile("movq %%rax, %%rsi\n"
				"xchgq %%rsi, 0x40(%%r14)"
				::);
}

void detour_dispatch_group_async_f(struct hack_handler *hack_handler_ptr)
{
     detour_function(hack_handler_ptr, "dispatch_group_async_f",
         shell_dispatch_group_async_f, 0x189, 7);
}

/* 38
 * _dispatch_apply_f
 * 0000000000013b2e    55                  pushq   %rbp      
 * ->0000000000013ddf    49c7471000000000    movq    $0x0, 0x10(%r15)
 * 0000000000013de7    4d877d40            xchgq   %r15, 0x40(%r13)
 * 0000000000013deb    4d85ff              testq   %r15, %r15
 */
void shell_dispatch_apply_f()
{
	save_registers;
	uint64_t dq, item;
	asm volatile("movq %%r15, %0\n"
			"movq %%r13, %1"
			:"=m"(item), "=m"(dq):);
	kdebug_trace(DISPATCH_ENQUEUE, dq, item, 0x26, 0x26, 0);
	restore_registers;
	asm volatile("movq $0x0, 0x10(%%r15)"
				::);
}

void detour_dispatch_apply_f(struct hack_handler *hack_handler_ptr)
{
     detour_function(hack_handler_ptr, "dispatch_apply_f",
         shell_dispatch_apply_f, 0x2b1, 8);
}

/* 39
 * __dispatch_mach_msg_not_sent
 * 00000000000140ba    55                  pushq   %rbp      
 * ->00000000000141fd    4889d9              movq    %rbx, %rcx
 * ->0000000000014200    49874e40            xchgq   %rcx, 0x40(%r14)
 * 0000000000014204    4885c9              testq   %rcx, %rcx
 * 40
 * ->0000000000014294    4c89f9              movq    %r15, %rcx
 * ->0000000000014297    48870a              xchgq   %rcx, (%rdx)
 * 000000000001429a    4885c9              testq   %rcx, %rcx
 */
void shell__dispatch_mach_msg_not_sent_1()
{
	save_registers;
	uint64_t dq, item;
	asm volatile("movq %%rbx, %0\n"
			"movq %%r14, %1"
			:"=m"(item), "=m"(dq):);
	kdebug_trace(DISPATCH_ENQUEUE, dq, item, 0x27, 0x27, 0);
	restore_registers;
	asm volatile("movq %%rbx, %%rcx\n"
				"xchgq %%rcx, 0x40(%%r14)"
				::);
}

void shell__dispatch_mach_msg_not_sent_2()
{
	save_registers;
	uint64_t dq, item;
	asm volatile("movq %%r15, %0\n"
			"movq %%rdx, %%rax\n"
			"subq $0x40, %%rax\n"
			"movq %%rax, %1"
			:"=m"(item), "=m"(dq):);
	kdebug_trace(DISPATCH_ENQUEUE, dq, item, 0x28, 0x28, 0);
	restore_registers;
	asm volatile("movq %%r15, %%rcx\n"
				"xchgq %%rcx, (%%rdx)"
				::);
}

void detour__dispatch_mach_msg_not_sent(struct hack_handler *hack_handler_ptr)
{
     detour_function(hack_handler_ptr, "_dispatch_mach_msg_not_sent",
         shell__dispatch_mach_msg_not_sent_1, 0x143, 7);
     detour_function(hack_handler_ptr, "_dispatch_mach_msg_not_sent",
         shell__dispatch_mach_msg_not_sent_2, 0x1da, 6);
}

/* 41
 * _dispatch_mach_reconnect
 * 0000000000015276    55                  pushq   %rbp      
 * ->0000000000015328    4889c2              movq    %rax, %rdx
 * ->000000000001532b    48875140            xchgq   %rdx, 0x40(%rcx)
 * 000000000001532f    4885d2              testq   %rdx, %rdx
 *
void shell_dispatch_mach_reconnect()
{
	save_registers;
	uint64_t dq, item;
	asm volatile("movq %%rax, %0\n"
			"movq %%rcx, %1"
			:"=m"(item), "=m"(dq):);
	kdebug_trace(DISPATCH_ENQUEUE, dq, item, 0x29, 0x29, 0);
	restore_registers;
	asm volatile("movq %%rax, %%rdx\n"
				"xchgq %%rdx, 0x40(%%rcx)"
				::);
}

void detour_dispatch_mach_reconnect(struct hack_handler *hack_handler_ptr)
{
     detour_function(hack_handler_ptr, "dispatch_mach_reconnect",
         shell_dispatch_mach_reconnect, 0xb2, 7);
}
*/

/* 42
 * __dispatch_main_queue_callback_4CF
 * 0000000000015587    55                  pushq   %rbp      
 * ->00000000000155e0    4889d8              movq    %rbx, %rax
 * ->00000000000155e3    49874740            xchgq   %rax, 0x40(%r15)
 * 00000000000155e7    4885c0              testq   %rax, %rax
 *
 */
void shell__dispatch_main_queue_callback_4CF()
{
	save_registers;
	uint64_t dq, item;
	asm volatile("movq %%rbx, %0\n"
			"movq %%r15, %1"
			:"=m"(item), "=m"(dq):);
	kdebug_trace(DISPATCH_ENQUEUE, dq, item, 0x2a, 0x2a, 0);
	restore_registers;
	asm volatile("movq %%rbx, %%rax\n"
				"xchgq %%rax, 0x40(%%r15)"
				::);
}

void detour__dispatch_main_queue_callback_4CF(struct hack_handler *hack_handler_ptr)
{
     detour_function(hack_handler_ptr, "_dispatch_main_queue_callback_4CF",
         shell__dispatch_main_queue_callback_4CF, 0x59, 7);
}

/* 43
 * __dispatch_async_f2
 * 00000000000163b0    4989f0              movq    %rsi, %r8
 * ->000000000001640e    4c89c0              movq    %r8, %rax
 * ->0000000000016411    498701              xchgq   %rax, (%r9)
 * 0000000000016414    4885c0              testq   %rax, %rax
 */
void shell__dispatch_async_f2()
{
	save_registers;
	uint64_t dq, item;
	asm volatile("movq %%r8, %0\n"
			"movq %%r9, %%rax\n"
			"subq $0x40, %%rax\n"
			"movq %%rax, %1"
			:"=m"(item), "=m"(dq):);
	kdebug_trace(DISPATCH_ENQUEUE, dq, item, 0x2b, 0x2b, 0);
	restore_registers;
	asm volatile("movq %%r8, %%rax\n"
				"xchgq %%rax, (%%r9)"
				::);
}

void detour__dispatch_async_f2(struct hack_handler *hack_handler_ptr)
{
     detour_function(hack_handler_ptr, "_dispatch_async_f2",
         shell__dispatch_async_f2, 0x5e, 6);
}

/* 44
 * __dispatch_async_f_redirect
 * 0000000000016464    55                  pushq   %rbp      
 * ->00000000000165a9    4889c2              movq    %rax, %rdx
 * ->00000000000165ac    48875740            xchgq   %rdx, 0x40(%rdi)
 * 00000000000165b0    4885d2              testq   %rdx, %rdx
 * 45                                                        
 * ->00000000000165ef    4889c6              movq    %rax, %rsi                     
 * ->00000000000165f2    48877240            xchgq   %rsi, 0x40(%rdx)
 * 00000000000165f6    4885f6              testq   %rsi, %rsi
 */
void shell__dispatch_async_f_redirect_1()
{
	save_registers;
	uint64_t dq, item;
	asm volatile("movq %%rax, %0\n"
			"movq %%rdi, %1"
			:"=m"(item), "=m"(dq):);
	kdebug_trace(DISPATCH_ENQUEUE, dq, item, 0x2c, 0x2c, 0);
	restore_registers;
	asm volatile("movq %%rax, %%rdx\n"
				"xchgq %%rdx, 0x40(%%rdi)\n"
				::);
}

void shell__dispatch_async_f_redirect_2()
{
	save_registers;
	uint64_t dq, item;
	asm volatile("movq %%rax, %0\n"
			"movq %%rdx, %1"
			:"=m"(item), "=m"(dq):);
	kdebug_trace(DISPATCH_ENQUEUE, dq, item, 0x2d, 0x2d, 0);
	restore_registers;
	asm volatile("movq %%rax, %%rsi\n"
				"xchgq %%rsi, 0x40(%%rdx)"
				::);
}

void detour__dispatch_async_f_redirect(struct hack_handler *hack_handler_ptr)
{
     detour_function(hack_handler_ptr, "_dispatch_async_f_redirect",
         shell__dispatch_async_f_redirect_1, 0x145, 7);
     detour_function(hack_handler_ptr, "_dispatch_async_f_redirect",
         shell__dispatch_async_f_redirect_2, 0x18b, 7);
}

/* 46
 * __dispatch_sync_f_slow
 * 0000000000017f35    55                  pushq   %rbp      
 * ->000000000001804e    4889c1              movq    %rax, %rcx
 * ->0000000000018051    48874b40            xchgq   %rcx, 0x40(%rbx)
 * 0000000000018055    4885c9              testq   %rcx, %rcx
 */
void shell__dispatch_sync_f_slow()
{
	save_registers;
	uint64_t dq, item;
	asm volatile("movq %%rax, %0\n"
			"movq %%rbx, %1"
			:"=m"(item), "=m"(dq):);
	kdebug_trace(DISPATCH_ENQUEUE, dq, item, 0x2e, 0x2e, 0);
	restore_registers;
	asm volatile("movq %%rax, %%rcx\n"
				"xchgq %%rcx, 0x40(%%rbx)"
				::);
}

void detour__dispatch_sync_f_slow(struct hack_handler *hack_handler_ptr)
{
     detour_function(hack_handler_ptr, "_dispatch_sync_f_slow",
		shell__dispatch_sync_f_slow, 0x119 , 7);
}

/* 47
 * __dispatch_apply_redirect
 * 0000000000018293    55                  pushq   %rbp      
 * ->000000000001848c    49c7461000000000    movq    $0x0, 0x10(%r14)
 * 0000000000018494    4c877340            xchgq   %r14, 0x40(%rbx)
 * 0000000000018498    4d85f6              testq   %r14, %r14
 */
void shell__dispatch_apply_redirect()
{
	save_registers;
	uint64_t dq, item;
	asm volatile("movq %%r14, %0\n"
			"movq %%rbx, %1"
			:"=m"(item), "=m"(dq):);
	kdebug_trace(DISPATCH_ENQUEUE, dq, item, 0x2f, 0x2f, 0);
	restore_registers;
	asm volatile("movq $0x0, 0x10(%%r14)"::);
}

void detour__dispatch_apply_redirect(struct hack_handler *hack_handler_ptr)
{
     detour_function(hack_handler_ptr, "_dispatch_apply_redirect",
		shell__dispatch_apply_redirect, 0x1f9, 8);
}

/* 49
 * _dispatch_barrier_async_f
 * 0000000000019f68    4889d0              movq    %rdx, %rax
 * ->000000000001a090    4889d1              movq    %rdx, %rcx
 * ->000000000001a093    48874f40            xchgq   %rcx, 0x40(%rdi)
 * 000000000001a097    4885c9              testq   %rcx, %rcx
 */
void shell_dispatch_barrier_async_f()
{
	save_registers;
	uint64_t dq, item;
	asm volatile("movq %%rdx, %0\n"
			"movq %%rdi, %1"
			:"=m"(item), "=m"(dq):);
	kdebug_trace(DISPATCH_ENQUEUE, dq, item, 0x31, 0x31, 0);
	restore_registers;
	asm volatile("movq %%rdx, %%rcx\n"
				"xchgq %%rcx, 0x40(%%rdi)"
				::);
}

void detour_dispatch_barrier_async_f(struct hack_handler *hack_handler_ptr)
{
     detour_function(hack_handler_ptr, "dispatch_barrier_async_f",
		shell_dispatch_barrier_async_f, 0x128, 7);
}
#endif

/*
 * detour function, called by hooked function
 */
void detour_enqueue(struct hack_handler * hack_handler_ptr)
{
	#if defined(__LP64__)
	detour__dispatch_queue_wakeup_with_qos_slow(hack_handler_ptr);
	detour__dispatch_barrier_async_detached_f(hack_handler_ptr);
	detour__dispatch_barrier_async_f_slow(hack_handler_ptr);
	detour__dispatch_wakeup(hack_handler_ptr);
	detour__dispatch_queue_push_queue(hack_handler_ptr);
	detour__dispatch_kq_init(hack_handler_ptr);
	detour_dispatch_mach_receive_barrier_f(hack_handler_ptr);
	detour_dispatch_async_f(hack_handler_ptr);
	detour__dispatch_async_f_slow(hack_handler_ptr);
	//14 and 15 detour_dispatch_mach_send(hack_handler_ptr);
	detour__dispatch_mach_msg_send(hack_handler_ptr);
	detour__dispatch_mach_send_drain(hack_handler_ptr);
	detour__dispatch_mach_msg_recv(hack_handler_ptr);
	detour__dispatch_mach_reply_kevent_unregister(hack_handler_ptr);
	detour_dispatch_async_enforce_qos_class_f(hack_handler_ptr);
	detour_dispatch_async(hack_handler_ptr);
	detour__dispatch_barrier_async_f(hack_handler_ptr);
	detour_dispatch_mach_send_barrier_f(hack_handler_ptr);
	detour__dispatch_mach_cancel(hack_handler_ptr);
	detour__dispatch_mach_disconnect(hack_handler_ptr);
	detour_dispatch_group_async(hack_handler_ptr);
	//33 and 34 detour_dispatch_group_notify_f(hack_handler_ptr);
	detour__dispatch_barrier_sync_f_slow(hack_handler_ptr);
	detour_dispatch_group_async_f(hack_handler_ptr);
	detour_dispatch_apply_f(hack_handler_ptr);
	detour__dispatch_mach_msg_not_sent(hack_handler_ptr);
	// 41 detour_dispatch_mach_reconnect(hack_handler_ptr);
	detour__dispatch_main_queue_callback_4CF(hack_handler_ptr);
	detour__dispatch_async_f2(hack_handler_ptr);
	detour__dispatch_async_f_redirect(hack_handler_ptr);
	detour__dispatch_sync_f_slow(hack_handler_ptr);
	detour__dispatch_apply_redirect(hack_handler_ptr);
	detour_dispatch_barrier_async_f(hack_handler_ptr);
	#endif
}
