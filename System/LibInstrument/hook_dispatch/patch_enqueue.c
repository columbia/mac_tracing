/* libdispatch.dylib
 * version 501.40.12
 * arch i386 x86_64
 ************************/
#include "lib_mach_info.h"
#include <dispatch/dispatch.h>

#define DISPATCH_ENQUEUE 0x210a0008
/* copy from libdispatch */
#define _OS_OBJECT_HEADER(isa, ref_cnt, xref_cnt) \
        isa; /* must be pointer-sized */ \
        int volatile ref_cnt; \
        int volatile xref_cnt

#define DISPATCH_STRUCT_HEADER() \
	_OS_OBJECT_HEADER( \
	const void *do_vtable, \
	do_ref_cnt, \
	do_xref_cnt); \
	void *volatile do_next; \
	void *do_targetq; \
	void *do_ctxt; \
	void *do_finalizer; \
	unsigned int volatile do_suspend_cnt;

#define DISPATCH_QUEUE_HEADER \
	uint32_t volatile dq_running; \
	void *volatile dq_items_head; \
	/* LP64 global queue cacheline boundary */ \
	void *volatile dq_items_tail; \
	/* do not care
	dispatch_queue_t dq_specific_q; \
	uint16_t dq_width; \
	uint16_t dq_is_thread_bound:1; \
	uint32_t volatile dq_override; \
	pthread_priority_t dq_priority; \
	mach_port_t dq_thread; \
	mach_port_t volatile dq_tqthread; \
	voucher_t dq_override_voucher; \
	unsigned long dq_serialnum; \
	const char *dq_label; \
	*/

struct patch_dispatch_queue_s {
	DISPATCH_STRUCT_HEADER();
	DISPATCH_QUEUE_HEADER;
	//DISPATCH_QUEUE_CACHELINE_PADDING; // for static queues only
};

//vtables are pointers far away from the low page in memory
#define DISPATCH_OBJ_IS_VTABLE(x) ((unsigned long)(x)->do_vtable > 0xffUL)

struct dispatch_patch_vtable_head {
	unsigned long const do_type;
	const char *const do_kind;
	size_t (*const do_debug)(void *, char *, size_t);
	void (*const do_invoke)(void *, void * dc, \
		void *);
	unsigned long (*const do_probe)(void *); \
	void (*const do_dispose)(void *);
};

// If dc_vtable is less than 127, then the object is a continuation.
// Otherwise, the object has a private layout and memory management rules. The
// layout until after 'do_next' must align with normal objects.
typedef unsigned long pthread_priority_t;
#if __x86_64__
//#define DISPATCH_CONTINUATION_HEADER()
struct dispatch_patch_item_head {
	const struct dispatch_patch_vtable_head *do_vtable; \
	union { \
		pthread_priority_t dc_priority; \
		int dc_cache_cnt; \
		uintptr_t dc_pad; \
	}; \
	void *volatile do_next; \
	void *dc_voucher; \
	void *dc_func; \
	void *dc_ctxt; \
	void *dc_data; \
	void *dc_other;
};
#define _DISPATCH_SIZEOF_PTR 8
#else
//#define DISPATCH_CONTINUATION_HEADER()
struct dispatch_patch_item_head {
	const struct dispatch_patch_vtable_head *do_vtable; \
	union { \
		pthread_priority_t dc_priority; \
		int dc_cache_cnt; \
		uintptr_t dc_pad; \
	}; \
	void *dc_voucher; \
	void *volatile do_next; \
	void *dc_func; \
	void *dc_ctxt; \
	void *dc_data; \
	void *dc_other;
};
#define _DISPATCH_SIZEOF_PTR 4
#endif
/* end of copy*/

#define dq_items_tail_offset\
	&(((struct patch_dispatch_queue_s *)(0))->dq_items_tail)

/************/
#if __x86_64__
/* 0x2
 * __dispatch_queue_wakeup_with_qos_slow:
 * 0x0L	0000000000003a88	55              	pushq	%rbp
 * 0x34fL	0000000000003dd7	4c897830        	movq	%r15, 0x30(%rax)
 * 0x353L	0000000000003ddb	48c7401000000000	movq	$0x0, 0x10(%rax)
 * 0x35bL	0000000000003de3	4889c1          	movq	%rax, %rcx
 * 0x35eL	0000000000003de6	49874c3c40      	xchgq	%rcx, 0x40(%r12,%rdi)
 * 0x363L	0000000000003deb	4885c9          	testq	%rcx, %rcx
 * 0x366L	0000000000003dee	7409            	je	0x3df9
 * 0x368L	0000000000003df0	48894110        	movq	%rax, 0x10(%rcx)
 * 0x36cL	0000000000003df4	e92cffffff      	jmp	0x3d25
 * target: 0000000000003de6	49874c3c40      	xchgq	%rcx, 0x40(%r12,%rdi)
 * verify: 0000000000003e03	e8b9130000      	callq	0x51c1
 */
void shell_enq__dispatch_queue_wakeup_with_qos_slow()
{
	save_registers
	uint64_t dq, invoke_ptr;
	struct dispatch_patch_item_head *item;
	asm volatile("movq %%rcx, %0\n"
		"leaq (%%r12, %%rdi), %%rax\n"
		"movq %%rax, %1"
		:"=m"(item), "=m"(dq):);
	if (DISPATCH_OBJ_IS_VTABLE(item))
		invoke_ptr = (uint64_t)(item->do_vtable->do_invoke);
	else
		invoke_ptr = 0;
	kdebug_trace(DISPATCH_ENQUEUE, 0x2ULL,
		dq, (uint64_t)item,
		(uint64_t)item->dc_ctxt, 0);
	kdebug_trace(DISPATCH_ENQUEUE, 1ULL << 32 | 0x2,
		(uint64_t)item->dc_func, invoke_ptr,
		(uint64_t)item->do_vtable, 0);
	restore_registers
	//simulation
	asm volatile("xchgq %%rcx, 0x40(%%r12, %%rdi)"::);
}
#endif
#if __i386__
/* 0x2
 * __dispatch_queue_wakeup_with_qos_slow:
 * 0x0L	000031bd	55 	pushl	%ebp
 * 0x339L	000034f6	89 70 18 	movl	%esi, 0x18(%eax)
 * 0x33cL	000034f9	c7 40 0c 00 00 00 00 	movl	$0x0, 0xc(%eax)
 * 0x343L	00003500	89 c2 	movl	%eax, %edx
 * 0x345L	00003502	87 94 0f 59 09 03 00 	xchgl	%edx, 0x30959(%edi,%ecx)
 * 0x34cL	00003509	85 d2 	testl	%edx, %edx
 * 0x34eL	0000350b	74 08 	je	0x3515
 * 0x350L	0000350d	89 42 0c 	movl	%eax, 0xc(%edx)
 * 0x353L	00003510	e9 25 ff ff ff 	jmp	0x343a
 * target: 00003502	87 94 0f 59 09 03 00 	xchgl	%edx, 0x30959(%edi,%ecx)
 * verify: 00003533	e8 a8 11 00 00 	calll	0x46e0
 */
void shell_enq__dispatch_queue_wakeup_with_qos_slow()
{
	save_registers
	uint64_t dq, invoke_ptr;
	struct dispatch_patch_item_head *item;
	asm volatile("movl %%edx, %0\n"
		"leal 0x30959(%%edi, %%ecx), %%eax\n"
		"subl $0x28, %%eax\n"
		"movl %%eax, %1"
		:"=m"(item), "=m"(dq):);
	if (DISPATCH_OBJ_IS_VTABLE(item))
		invoke_ptr = (uint64_t)(item->do_vtable->do_invoke);
	else
		invoke_ptr = 0;
	kdebug_trace(DISPATCH_ENQUEUE, 0x2ULL,
		dq, (uint64_t)item,
		(uint64_t)item->dc_ctxt, 0);
	kdebug_trace(DISPATCH_ENQUEUE, 1ULL << 32 | 0x2,
		(uint64_t)item->dc_func, invoke_ptr,
		(uint64_t)item->do_vtable, 0);
	restore_registers
	//simulation
	asm volatile("xchgl	%%edx, 0x30959(%%edi,%%ecx)"
		::);
}
#endif
void detour__dispatch_queue_wakeup_with_qos_slow(struct mach_o_handler *handler_ptr)
{
#if __x86_64__
	detour_function(handler_ptr, "_dispatch_queue_wakeup_with_qos_slow", 
		shell_enq__dispatch_queue_wakeup_with_qos_slow, 0x35e, 5);
#elif __i386__
	detour_function(handler_ptr, "_dispatch_queue_wakeup_with_qos_slow", 
		shell_enq__dispatch_queue_wakeup_with_qos_slow, 0x345, 7);
#endif
}
/************/
#if __x86_64__
/* 0x3
 * __dispatch_barrier_async_detached_f:
 * 0x0L	0000000000004f33	4889d0          	movq	%rdx, %rax
 * 0x8fL	0000000000004fc2	31c0            	xorl	%eax, %eax
 * 0x91L	0000000000004fc4	48c7421000000000	movq	$0x0, 0x10(%rdx)
 * 0x99L	0000000000004fcc	4889d1          	movq	%rdx, %rcx
 * 0x9cL	0000000000004fcf	48874f40        	xchgq	%rcx, 0x40(%rdi)
 * 0xa0L	0000000000004fd3	4885c9          	testq	%rcx, %rcx
 * 0xa3L	0000000000004fd6	741d            	je	0x4ff5
 * 0xa5L	0000000000004fd8	48895110        	movq	%rdx, 0x10(%rcx)
 * 0xa9L	0000000000004fdc	84c0            	testb	%al, %al
 * target: 0000000000004fcf	48874f40        	xchgq	%rcx, 0x40(%rdi)
 * verify: 0000000000004ff8	e9c4010000      	jmp	0x51c1
 */
void shell_enq__dispatch_barrier_async_detached_f()
{
	save_registers
	uint64_t dq, invoke_ptr;
	struct dispatch_patch_item_head *item;
	asm volatile("movq %%rdx, %0\n"
		"movq %%rdi, %1"
		:"=m"(item), "=m"(dq):);
	if (DISPATCH_OBJ_IS_VTABLE(item))
		invoke_ptr = (uint64_t)(item->do_vtable->do_invoke);
	else
		invoke_ptr = 0;
	kdebug_trace(DISPATCH_ENQUEUE, 0x3ULL,
		dq, (uint64_t)item,
		(uint64_t)item->dc_ctxt, 0);
	kdebug_trace(DISPATCH_ENQUEUE, 1ULL << 32 | 0x3,
		(uint64_t)item->dc_func, invoke_ptr,
		(uint64_t)item->do_vtable, 0);
	restore_registers
	//simulation
	asm volatile("movq	%%rdx, %%rcx\n"
		"xchgq	%%rcx, 0x40(%%rdi)"
		::);
}
#endif
#if __i386__
/* 0x3
 * __dispatch_barrier_async_detached_f:
 * 0x0L	00004444	55 	pushl	%ebp
 * 0xa7L	000044eb	31 db 	xorl	%ebx, %ebx
 * 0xa9L	000044ed	c7 40 0c 00 00 00 00 	movl	$0x0, 0xc(%eax)
 * 0xb0L	000044f4	89 c6 	movl	%eax, %esi
 * 0xb2L	000044f6	87 71 28 	xchgl	%esi, 0x28(%ecx)
 * 0xb5L	000044f9	85 f6 	testl	%esi, %esi
 * 0xb7L	000044fb	74 2f 	je	0x452c
 * 0xb9L	000044fd	89 46 0c 	movl	%eax, 0xc(%esi)
 * 0xbcL	00004500	84 db 	testb	%bl, %bl
 * target: 000044f6	87 71 28 	xchgl	%esi, 0x28(%ecx)
 * verify: 0000453e	e8 9d 01 00 00 	calll	0x46e0
 */
void shell_enq__dispatch_barrier_async_detached_f()
{
	save_registers
	uint64_t dq, invoke_ptr;
	struct dispatch_patch_item_head *item;
	//simulation
	asm volatile("movl %%eax, %%esi\n"
		"xchgl %%esi, 0x28(%%ecx)\n"
		"movl %%esi, -0xc(%%ebp)\n"
		"movl %%eax, %0\n"
		"movl %%ecx, %1"
		:"=m"(item), "=m"(dq):);
	if (DISPATCH_OBJ_IS_VTABLE(item))
		invoke_ptr = (uint64_t)(item->do_vtable->do_invoke);
	else
		invoke_ptr = 0;
	kdebug_trace(DISPATCH_ENQUEUE, 0x3ULL,
		dq, (uint64_t)item,
		(uint64_t)item->dc_ctxt, 0);
	kdebug_trace(DISPATCH_ENQUEUE, 1ULL << 32 | 0x3,
		(uint64_t)item->dc_func, invoke_ptr,
		(uint64_t)item->do_vtable, 0);
	restore_registers

	/*simulation
	asm volatile("movl	%%eax, %%esi\n"
		"xchgl	%%esi, 0x28(%%ecx)"
		::);
	*/
}
#endif
void detour__dispatch_barrier_async_detached_f(struct mach_o_handler *handler_ptr)
{
#if __x86_64__
	detour_function(handler_ptr, "_dispatch_barrier_async_detached_f", 
		shell_enq__dispatch_barrier_async_detached_f, 0x99, 7);
#elif __i386__
	detour_function(handler_ptr, "_dispatch_barrier_async_detached_f", 
		shell_enq__dispatch_barrier_async_detached_f, 0xb0, 5);
#endif
}
/************/
#if __x86_64__
/* 0x4
 * __dispatch_barrier_async_f_slow:
 * 0x0L	0000000000004ffe	55              	pushq	%rbp
 * 0x14eL	000000000000514c	31c9            	xorl	%ecx, %ecx
 * 0x150L	000000000000514e	48c7401000000000	movq	$0x0, 0x10(%rax)
 * 0x158L	0000000000005156	4889c2          	movq	%rax, %rdx
 * 0x15bL	0000000000005159	49875640        	xchgq	%rdx, 0x40(%r14)
 * 0x15fL	000000000000515d	4885d2          	testq	%rdx, %rdx
 * 0x162L	0000000000005160	7421            	je	0x5183
 * 0x164L	0000000000005162	48894210        	movq	%rax, 0x10(%rdx)
 * 0x168L	0000000000005166	84c9            	testb	%cl, %cl
 * target: 0000000000005159	49875640        	xchgq	%rdx, 0x40(%r14)
 * verify: 000000000000519d	e91f000000      	jmp	0x51c1
 */
void shell_enq__dispatch_barrier_async_f_slow()
{
	save_registers
	uint64_t dq, invoke_ptr;
	struct dispatch_patch_item_head *item;
	asm volatile("movq %%rax, %0\n"
		"movq %%r14, %1"
		:"=m"(item), "=m"(dq):);
	if (DISPATCH_OBJ_IS_VTABLE(item))
		invoke_ptr = (uint64_t)(item->do_vtable->do_invoke);
	else
		invoke_ptr = 0;
	kdebug_trace(DISPATCH_ENQUEUE, 0x4ULL,
		dq, (uint64_t)item,
		(uint64_t)item->dc_ctxt, 0);
	kdebug_trace(DISPATCH_ENQUEUE, 1ULL << 32 | 0x4,
		(uint64_t)item->dc_func, invoke_ptr,
		(uint64_t)item->do_vtable, 0);
	restore_registers
	//simulation
	asm volatile("movq	%%rax, %%rdx\n"
		"xchgq	%%rdx, 0x40(%%r14)"
		::);
}
#endif
#if __i386__
/* 0x4
 * __dispatch_barrier_async_f_slow:
 * 0x0L	0000454b	55 	pushl	%ebp
 * 0x134L	0000467f	31 d2 	xorl	%edx, %edx
 * 0x136L	00004681	c7 40 0c 00 00 00 00 	movl	$0x0, 0xc(%eax)
 * 0x13dL	00004688	89 c1 	movl	%eax, %ecx
 * 0x13fL	0000468a	87 4b 28 	xchgl	%ecx, 0x28(%ebx)
 * 0x142L	0000468d	85 c9 	testl	%ecx, %ecx
 * 0x144L	0000468f	74 15 	je	0x46a6
 * 0x146L	00004691	89 41 0c 	movl	%eax, 0xc(%ecx)
 * 0x149L	00004694	84 d2 	testb	%dl, %dl
 * target: 0000468a	87 4b 28 	xchgl	%ecx, 0x28(%ebx)
 * verify: 000046b8	e8 23 00 00 00 	calll	0x46e0
 */
void shell_enq__dispatch_barrier_async_f_slow()
{
	save_registers
	uint64_t dq, invoke_ptr;
	struct dispatch_patch_item_head *item;
	asm volatile("movl %%eax, %0\n"
		"movl %%ebx, %1"
		:"=m"(item), "=m"(dq):);
	if (DISPATCH_OBJ_IS_VTABLE(item))
		invoke_ptr = (uint64_t)(item->do_vtable->do_invoke);
	else
		invoke_ptr = 0;
	kdebug_trace(DISPATCH_ENQUEUE, 0x4ULL,
		dq, (uint64_t)item,
		(uint64_t)item->dc_ctxt, 0);
	kdebug_trace(DISPATCH_ENQUEUE, 1ULL << 32 | 0x4,
		(uint64_t)item->dc_func, invoke_ptr,
		(uint64_t)item->do_vtable, 0);
	restore_registers
	//simulation
	asm volatile("movl	%%eax, %%ecx\n"
		"xchgl	%%ecx, 0x28(%%ebx)"
		::);
}
#endif
void detour__dispatch_barrier_async_f_slow(struct mach_o_handler *handler_ptr)
{
#if __x86_64__
	detour_function(handler_ptr, "_dispatch_barrier_async_f_slow", 
		shell_enq__dispatch_barrier_async_f_slow, 0x158, 7);
#elif __i386__
	detour_function(handler_ptr, "_dispatch_barrier_async_f_slow", 
		shell_enq__dispatch_barrier_async_f_slow, 0x13d, 5);
#endif
}
/************/
#if __x86_64__
/* 0x5
 * __dispatch_wakeup:
 * 0x0L	0000000000005379	55              	pushq	%rbp
 * 0x7cL	00000000000053f5	4c8b7318        	movq	0x18(%rbx), %r14
 * 0x80L	00000000000053f9	48c7431000000000	movq	$0x0, 0x10(%rbx)
 * 0x88L	0000000000005401	4889d8          	movq	%rbx, %rax
 * 0x8bL	0000000000005404	49874640        	xchgq	%rax, 0x40(%r14)
 * 0x8fL	0000000000005408	4885c0          	testq	%rax, %rax
 * 0x92L	000000000000540b	7406            	je	0x5413
 * 0x94L	000000000000540d	48895810        	movq	%rbx, 0x10(%rax)
 * 0x98L	0000000000005411	eb0f            	jmp	0x5422
 * target: 0000000000005404	49874640        	xchgq	%rax, 0x40(%r14)
 * verify: 000000000000541d	e89ffdffff      	callq	0x51c1
 */
void shell_enq__dispatch_wakeup()
{
	save_registers
	uint64_t dq, invoke_ptr;
	struct dispatch_patch_item_head *item;
	asm volatile("movq %%rbx, %0\n"
		"movq %%r14, %1"
		:"=m"(item), "=m"(dq):);
	if (DISPATCH_OBJ_IS_VTABLE(item))
		invoke_ptr = (uint64_t)(item->do_vtable->do_invoke);
	else
		invoke_ptr = 0;
	kdebug_trace(DISPATCH_ENQUEUE, 0x5ULL,
		dq, (uint64_t)item,
		(uint64_t)item->dc_ctxt, 0);
	kdebug_trace(DISPATCH_ENQUEUE, 1ULL << 32 | 0x5,
		(uint64_t)item->dc_func, invoke_ptr,
		(uint64_t)item->do_vtable, 0);
	restore_registers
	//simulation
	asm volatile("movq	%%rbx, %%rax\n"
		"xchgq	%%rax, 0x40(%%r14)"
		::);
}
#endif
#if __i386__
/* 0x5
 * __dispatch_wakeup:
 * 0x0L	000048e5	55 	pushl	%ebp
 * 0x68L	0000494d	8b 77 10 	movl	0x10(%edi), %esi
 * 0x6bL	00004950	c7 47 0c 00 00 00 00 	movl	$0x0, 0xc(%edi)
 * 0x72L	00004957	89 f8 	movl	%edi, %eax
 * 0x74L	00004959	87 46 28 	xchgl	%eax, 0x28(%esi)
 * 0x77L	0000495c	85 c0 	testl	%eax, %eax
 * 0x79L	0000495e	74 05 	je	0x4965
 * 0x7bL	00004960	89 78 0c 	movl	%edi, 0xc(%eax)
 * 0x7eL	00004963	eb 1c 	jmp	0x4981
 * target: 00004959	87 46 28 	xchgl	%eax, 0x28(%esi)
 * verify: 0000497c	e8 5f fd ff ff 	calll	0x46e0
 */
void shell_enq__dispatch_wakeup()
{
	save_registers
	uint64_t dq, invoke_ptr;
	struct dispatch_patch_item_head *item;
	asm volatile("movl %%edi, %0\n"
		"movl %%esi, %1"
		:"=m"(item), "=m"(dq):);
	if (DISPATCH_OBJ_IS_VTABLE(item))
		invoke_ptr = (uint64_t)(item->do_vtable->do_invoke);
	else
		invoke_ptr = 0;
	kdebug_trace(DISPATCH_ENQUEUE, 0x5ULL,
		dq, (uint64_t)item,
		(uint64_t)item->dc_ctxt, 0);
	kdebug_trace(DISPATCH_ENQUEUE, 1ULL << 32 | 0x5,
		(uint64_t)item->dc_func, invoke_ptr,
		(uint64_t)item->do_vtable, 0);
	restore_registers
	//simulation
	asm volatile("movl	%%edi, %%eax\n"
		"xchgl	%%eax, 0x28(%%esi)"
		::);
}
#endif
void detour__dispatch_wakeup(struct mach_o_handler *handler_ptr)
{
#if __x86_64__
	detour_function(handler_ptr, "_dispatch_wakeup", 
		shell_enq__dispatch_wakeup, 0x88, 7);
#elif __i386__
	detour_function(handler_ptr, "_dispatch_wakeup", 
		shell_enq__dispatch_wakeup, 0x72, 5);
#endif
}
/************/
#if __x86_64__
/* 0x6
 * __dispatch_queue_push_queue:
 * 0x0L	0000000000005508	55              	pushq	%rbp
 * 0x143L	000000000000564b	31c0            	xorl	%eax, %eax
 * 0x145L	000000000000564d	48c7431000000000	movq	$0x0, 0x10(%rbx)
 * 0x14dL	0000000000005655	4889d9          	movq	%rbx, %rcx
 * 0x150L	0000000000005658	49874e40        	xchgq	%rcx, 0x40(%r14)
 * 0x154L	000000000000565c	4885c9          	testq	%rcx, %rcx
 * 0x157L	000000000000565f	7418            	je	0x5679
 * 0x159L	0000000000005661	48895910        	movq	%rbx, 0x10(%rcx)
 * 0x15dL	0000000000005665	84c0            	testb	%al, %al
 * target: 0000000000005658	49874e40        	xchgq	%rcx, 0x40(%r14)
 * verify: 000000000000568a	e932fbffff      	jmp	0x51c1
 */
void shell_enq__dispatch_queue_push_queue_0()
{
	save_registers
	uint64_t dq, invoke_ptr;
	struct dispatch_patch_item_head *item;
	asm volatile("movq %%rbx, %0\n"
		"movq %%r14, %1"
		:"=m"(item), "=m"(dq):);
	if (DISPATCH_OBJ_IS_VTABLE(item))
		invoke_ptr = (uint64_t)(item->do_vtable->do_invoke);
	else
		invoke_ptr = 0;
	kdebug_trace(DISPATCH_ENQUEUE, 0x6ULL,
		dq, (uint64_t)item,
		(uint64_t)item->dc_ctxt, 0);
	kdebug_trace(DISPATCH_ENQUEUE, 1ULL << 32 | 0x6,
		(uint64_t)item->dc_func, invoke_ptr,
		(uint64_t)item->do_vtable, 0);
	restore_registers
	//simulation
	asm volatile("movq	%%rbx, %%rcx\n"
		"xchgq	%%rcx, 0x40(%%r14)"
		::);
}

/* 0x7
 * __dispatch_queue_push_queue:
 * 0x0L	0000000000005508	55              	pushq	%rbp
 * 0x1b6L	00000000000056be	48895830        	movq	%rbx, 0x30(%rax)
 * 0x1baL	00000000000056c2	48c7401000000000	movq	$0x0, 0x10(%rax)
 * 0x1c2L	00000000000056ca	4889c1          	movq	%rax, %rcx
 * 0x1c5L	00000000000056cd	4b874c3c40      	xchgq	%rcx, 0x40(%r12,%r15)
 * 0x1caL	00000000000056d2	4885c9          	testq	%rcx, %rcx
 * 0x1cdL	00000000000056d5	740d            	je	0x56e4
 * 0x1cfL	00000000000056d7	48894110        	movq	%rax, 0x10(%rcx)
 * 0x1d3L	00000000000056db	5b              	popq	%rbx
 * target: 00000000000056cd	4b874c3c40      	xchgq	%rcx, 0x40(%r12,%r15)
 */
void shell_enq__dispatch_queue_push_queue_1()
{
	save_registers
	uint64_t dq, invoke_ptr;
	struct dispatch_patch_item_head *item;
	asm volatile("movq %%rcx, %0\n"
		"leaq (%%r12, %%r15), %%rax\n"
		"movq %%rax, %1\n"
		:"=m"(item), "=m"(dq):);
	if (DISPATCH_OBJ_IS_VTABLE(item))
		invoke_ptr = (uint64_t)(item->do_vtable->do_invoke);
	else
		invoke_ptr = 0;
	kdebug_trace(DISPATCH_ENQUEUE, 0x7ULL,
		dq, (uint64_t)item,
		(uint64_t)item->dc_ctxt, 0);
	kdebug_trace(DISPATCH_ENQUEUE, 1ULL << 32 | 0x7,
		(uint64_t)item->dc_func, invoke_ptr,
		(uint64_t)item->do_vtable, 0);
	restore_registers
	//simulation
	asm volatile("xchgq	%%rcx, 0x40(%%r12,%%r15)"
		::);
}
#endif
#if __i386__
/* 0x6
 * __dispatch_queue_push_queue:
 * 0x0L	00004a40	55 	pushl	%ebp
 * 0x120L	00004b60	31 c0 	xorl	%eax, %eax
 * 0x122L	00004b62	c7 47 0c 00 00 00 00 	movl	$0x0, 0xc(%edi)
 * 0x129L	00004b69	89 f9 	movl	%edi, %ecx
 * 0x12bL	00004b6b	87 4e 28 	xchgl	%ecx, 0x28(%esi)
 * 0x12eL	00004b6e	85 c9 	testl	%ecx, %ecx
 * 0x130L	00004b70	74 15 	je	0x4b87
 * 0x132L	00004b72	89 79 0c 	movl	%edi, 0xc(%ecx)
 * 0x135L	00004b75	84 c0 	testb	%al, %al
 * target: 00004b6b	87 4e 28 	xchgl	%ecx, 0x28(%esi)
 * verify: 00004b99	e8 42 fb ff ff 	calll	0x46e0
 */
void shell_enq__dispatch_queue_push_queue_0()
{
	save_registers
	uint64_t dq, invoke_ptr;
	struct dispatch_patch_item_head *item;
	asm volatile("movl %%edi, %0\n"
		"movl %%esi, %1"
		:"=m"(item), "=m"(dq):);
	if (DISPATCH_OBJ_IS_VTABLE(item))
		invoke_ptr = (uint64_t)(item->do_vtable->do_invoke);
	else
		invoke_ptr = 0;
	kdebug_trace(DISPATCH_ENQUEUE, 0x6ULL,
		dq, (uint64_t)item,
		(uint64_t)item->dc_ctxt, 0);
	kdebug_trace(DISPATCH_ENQUEUE, 1ULL << 32 | 0x6,
		(uint64_t)item->dc_func, invoke_ptr,
		(uint64_t)item->do_vtable, 0);
	restore_registers
	//simulation
	asm volatile("movl	%%edi, %%ecx\n"
		"xchgl	%%ecx, 0x28(%%esi)"
		::);
}
/* 0x7
 * __dispatch_queue_push_queue:
 * 0x0L	00004a40	55 	pushl	%ebp
 * 0x18bL	00004bcb	89 78 18 	movl	%edi, 0x18(%eax)
 * 0x18eL	00004bce	c7 40 0c 00 00 00 00 	movl	$0x0, 0xc(%eax)
 * 0x195L	00004bd5	89 c1 	movl	%eax, %ecx
 * 0x197L	00004bd7	87 8c 1a da f0 02 00 	xchgl	%ecx, 0x2f0da(%edx,%ebx)
 * 0x19eL	00004bde	85 c9 	testl	%ecx, %ecx
 * 0x1a0L	00004be0	74 0b 	je	0x4bed
 * 0x1a2L	00004be2	89 41 0c 	movl	%eax, 0xc(%ecx)
 * 0x1a5L	00004be5	83 c4 1c 	addl	$0x1c, %esp
 * target: 00004bd7	87 8c 1a da f0 02 00 	xchgl	%ecx, 0x2f0da(%edx,%ebx)
 */
void shell_enq__dispatch_queue_push_queue_1()
{
	save_registers
	uint64_t dq, invoke_ptr;
	struct dispatch_patch_item_head *item;
	asm volatile("movl %%ecx, %0\n"
		"leal 0x2f0da(%%edx, %%ebx), %%eax\n"
		"subl $0x28, %%eax\n"
		"movl %%eax, %1"
		:"=m"(item), "=m"(dq):); 
	if (DISPATCH_OBJ_IS_VTABLE(item))
		invoke_ptr = (uint64_t)(item->do_vtable->do_invoke);
	else
		invoke_ptr = 0;
	kdebug_trace(DISPATCH_ENQUEUE, 0x7ULL,
		dq, (uint64_t)item,
		(uint64_t)item->dc_ctxt, 0);
	kdebug_trace(DISPATCH_ENQUEUE, 1ULL << 32 | 0x7,
		(uint64_t)item->dc_func, invoke_ptr,
		(uint64_t)item->do_vtable, 0);
	restore_registers
	//simulation
	asm volatile("xchgl	%%ecx, 0x2f0da(%%edx,%%ebx)"
		::);
}
#endif
void detour__dispatch_queue_push_queue(struct mach_o_handler *handler_ptr)
{
#if __x86_64__
	detour_function(handler_ptr, "_dispatch_queue_push_queue", 
		shell_enq__dispatch_queue_push_queue_0, 0x14d, 7);
	detour_function(handler_ptr, "_dispatch_queue_push_queue", 
		shell_enq__dispatch_queue_push_queue_1, 0x1c5, 5);
#elif __i386__
	detour_function(handler_ptr, "_dispatch_queue_push_queue", 
		shell_enq__dispatch_queue_push_queue_0, 0x129, 5);
	detour_function(handler_ptr, "_dispatch_queue_push_queue", 
		shell_enq__dispatch_queue_push_queue_1, 0x197, 7);
#endif
}
/************/
#if __x86_64__
/* 0xa
 * __dispatch_kq_init:
 * 0x0L	0000000000006594	55              	pushq	%rbp
 * 0x79L	000000000000660d	488b7818        	movq	0x18(%rax), %rdi
 * 0x7dL	0000000000006611	48c7401000000000	movq	$0x0, 0x10(%rax)
 * 0x85L	0000000000006619	4889c1          	movq	%rax, %rcx
 * 0x88L	000000000000661c	48874f40        	xchgq	%rcx, 0x40(%rdi)
 * 0x8cL	0000000000006620	4885c9          	testq	%rcx, %rcx
 * 0x8fL	0000000000006623	7406            	je	0x662b
 * 0x91L	0000000000006625	48894110        	movq	%rax, 0x10(%rcx)
 * 0x95L	0000000000006629	eb10            	jmp	0x663b
 * target: 000000000000661c	48874f40        	xchgq	%rcx, 0x40(%rdi)
 * verify: 0000000000006636	e886ebffff      	callq	0x51c1
 */
void shell_enq__dispatch_kq_init()
{
	save_registers
	uint64_t dq, invoke_ptr;
	struct dispatch_patch_item_head *item;
	asm volatile("movq %%rax, %0\n"
		"movq %%rdi, %1"
		:"=m"(item), "=m"(dq):);
	if (DISPATCH_OBJ_IS_VTABLE(item))
		invoke_ptr = (uint64_t)(item->do_vtable->do_invoke);
	else
		invoke_ptr = 0;
	kdebug_trace(DISPATCH_ENQUEUE, 0xaULL,
		dq, (uint64_t)item,
		(uint64_t)item->dc_ctxt, 0);
	kdebug_trace(DISPATCH_ENQUEUE, 1ULL << 32 | 0xa,
		(uint64_t)item->dc_func, invoke_ptr,
		(uint64_t)item->do_vtable, 0);
	restore_registers
	//simulation
	asm volatile("movq	%%rax, %%rcx\n"
		"xchgq	%%rcx, 0x40(%%rdi)"
		::);
}
#endif
#if __i386__
/* 0xa
 * __dispatch_kq_init:
 * 0x0L	00005b56	55 	pushl	%ebp
 * 0x8fL	00005be5	8b 48 10 	movl	0x10(%eax), %ecx
 * 0x92L	00005be8	c7 40 0c 00 00 00 00 	movl	$0x0, 0xc(%eax)
 * 0x99L	00005bef	89 c2 	movl	%eax, %edx
 * 0x9bL	00005bf1	87 51 28 	xchgl	%edx, 0x28(%ecx)
 * 0x9eL	00005bf4	85 d2 	testl	%edx, %edx
 * 0xa0L	00005bf6	74 05 	je	0x5bfd
 * 0xa2L	00005bf8	89 42 0c 	movl	%eax, 0xc(%edx)
 * 0xa5L	00005bfb	eb 1c 	jmp	0x5c19
 * target: 00005bf1	87 51 28 	xchgl	%edx, 0x28(%ecx)
 * verify: 00005c14	e8 c7 ea ff ff 	calll	0x46e0
 */
void shell_enq__dispatch_kq_init()
{
	save_registers
	uint64_t dq, invoke_ptr;
	struct dispatch_patch_item_head *item;
	asm volatile("movl %%eax, %0\n"
		"movl %%ecx, %1"
		:"=m"(item), "=m"(dq):);
	if (DISPATCH_OBJ_IS_VTABLE(item))
		invoke_ptr = (uint64_t)(item->do_vtable->do_invoke);
	else
		invoke_ptr = 0;
	kdebug_trace(DISPATCH_ENQUEUE, 0xaULL,
		dq, (uint64_t)item,
		(uint64_t)item->dc_ctxt, 0);
	kdebug_trace(DISPATCH_ENQUEUE, 1ULL << 32 | 0xa,
		(uint64_t)item->dc_func, invoke_ptr,
		(uint64_t)item->do_vtable, 0);
	restore_registers
	//simulation
	asm volatile("movl	%%eax, %%edx\n"
		"xchgl	%%edx, 0x28(%%ecx)"
		::);
}
#endif
void detour__dispatch_kq_init(struct mach_o_handler *handler_ptr)
{
#if __x86_64__
	detour_function(handler_ptr, "_dispatch_kq_init", 
		shell_enq__dispatch_kq_init, 0x85, 7);
#elif __i386__
	detour_function(handler_ptr, "_dispatch_kq_init", 
		shell_enq__dispatch_kq_init, 0x99, 5);
#endif
}
/************/
#if __x86_64__
/* 0xb
 * _dispatch_mach_receive_barrier_f:
 * 0x0L	00000000000089d2	55              	pushq	%rbp
 * 0xf4L	0000000000008ac6	31c9            	xorl	%ecx, %ecx
 * 0xf6L	0000000000008ac8	48c7401000000000	movq	$0x0, 0x10(%rax)
 * 0xfeL	0000000000008ad0	4889c2          	movq	%rax, %rdx
 * 0x101L	0000000000008ad3	49875640        	xchgq	%rdx, 0x40(%r14)
 * 0x105L	0000000000008ad7	4885d2          	testq	%rdx, %rdx
 * 0x108L	0000000000008ada	741a            	je	0x8af6
 * 0x10aL	0000000000008adc	48894210        	movq	%rax, 0x10(%rdx)
 * 0x10eL	0000000000008ae0	84c9            	testb	%cl, %cl
 * target: 0000000000008ad3	49875640        	xchgq	%rdx, 0x40(%r14)
 * verify: 0000000000008b09	e9b3c6ffff      	jmp	0x51c1
 */
void shell_enq_dispatch_mach_receive_barrier_f()
{
	save_registers
	uint64_t dq, invoke_ptr;
	struct dispatch_patch_item_head *item;
	asm volatile("movq %%rax, %0\n"
		"movq %%r14, %1"
		:"=m"(item), "=m"(dq):);
	if (DISPATCH_OBJ_IS_VTABLE(item))
		invoke_ptr = (uint64_t)(item->do_vtable->do_invoke);
	else
		invoke_ptr = 0;
	kdebug_trace(DISPATCH_ENQUEUE, 0xbULL,
		dq, (uint64_t)item,
		(uint64_t)item->dc_ctxt, 0);
	kdebug_trace(DISPATCH_ENQUEUE, 1ULL << 32 | 0xb,
		(uint64_t)item->dc_func, invoke_ptr,
		(uint64_t)item->do_vtable, 0);
	restore_registers
	//simulation
	asm volatile("movq	%%rax, %%rdx\n"
		"xchgq	%%rdx, 0x40(%%r14)"
		::);
}
#endif
#if __i386__
/* 0xe
 * _dispatch_mach_receive_barrier_f:
 * 0x0L	000080a4	55 	pushl	%ebp
 * 0xedL	00008191	31 db 	xorl	%ebx, %ebx
 * 0xefL	00008193	c7 40 0c 00 00 00 00 	movl	$0x0, 0xc(%eax)
 * 0xf6L	0000819a	89 c6 	movl	%eax, %esi
 * 0xf8L	0000819c	87 71 28 	xchgl	%esi, 0x28(%ecx)
 * 0xfbL	0000819f	85 f6 	testl	%esi, %esi
 * 0xfdL	000081a1	74 15 	je	0x81b8
 * 0xffL	000081a3	89 46 0c 	movl	%eax, 0xc(%esi)
 * 0x102L	000081a6	84 db 	testb	%bl, %bl
 * target: 0000819c	87 71 28 	xchgl	%esi, 0x28(%ecx)
 * verify: 000081ca	e8 11 c5 ff ff 	calll	0x46e0
 */
void shell_enq_dispatch_mach_receive_barrier_f()
{
	save_registers
	uint64_t dq, invoke_ptr;
	struct dispatch_patch_item_head *item;
	//simulation
	asm volatile("movl %%eax, %%esi\n"
		"xchgl %%esi, 0x28(%%ecx)\n"
		"movl %%esi, -0xc(%%ebp)\n"
		"movl %%eax, %0\n"
		"movl %%ecx, %1"
		:"=m"(item), "=m"(dq):);
	if (DISPATCH_OBJ_IS_VTABLE(item))
		invoke_ptr = (uint64_t)(item->do_vtable->do_invoke);
	else
		invoke_ptr = 0;
	kdebug_trace(DISPATCH_ENQUEUE, 0xeULL,
		dq, (uint64_t)item,
		(uint64_t)item->dc_ctxt, 0);
	kdebug_trace(DISPATCH_ENQUEUE, 1ULL << 32 | 0xe,
		(uint64_t)item->dc_func, invoke_ptr,
		(uint64_t)item->do_vtable, 0);
	restore_registers
	/*simulation
	asm volatile("movl	%%eax, %%esi\n"
		"xchgl	%%esi, 0x28(%%ecx)"
		::);
	*/
}
#endif
void detour_dispatch_mach_receive_barrier_f(struct mach_o_handler *handler_ptr)
{
#if __x86_64__
	detour_function(handler_ptr, "dispatch_mach_receive_barrier_f", 
		shell_enq_dispatch_mach_receive_barrier_f, 0xfe, 7);
#elif __i386__
	detour_function(handler_ptr, "dispatch_mach_receive_barrier_f", 
		shell_enq_dispatch_mach_receive_barrier_f, 0xf6, 5);
#endif
}
/************/
#if __x86_64__
/* 0xc
 * _dispatch_async_f:
 * 0x0L	0000000000008b29	0fb74750        	movzwl	0x50(%rdi), %eax
 * 0x140L	0000000000008c69	31c9            	xorl	%ecx, %ecx
 * 0x142L	0000000000008c6b	48c7401000000000	movq	$0x0, 0x10(%rax)
 * 0x14aL	0000000000008c73	4889c6          	movq	%rax, %rsi
 * 0x14dL	0000000000008c76	48877740        	xchgq	%rsi, 0x40(%rdi)
 * 0x151L	0000000000008c7a	4885f6          	testq	%rsi, %rsi
 * 0x154L	0000000000008c7d	7410            	je	0x8c8f
 * 0x156L	0000000000008c7f	48894610        	movq	%rax, 0x10(%rsi)
 * 0x15aL	0000000000008c83	84c9            	testb	%cl, %cl
 * target: 0000000000008c76	48877740        	xchgq	%rsi, 0x40(%rdi)
 * verify: 0000000000008c98	e924c5ffff      	jmp	0x51c1
 */
void shell_enq_dispatch_async_f()
{
	save_registers
	uint64_t dq, invoke_ptr;
	struct dispatch_patch_item_head *item;
	asm volatile("movq %%rax, %0\n"
		"movq %%rdi, %1"
		:"=m"(item), "=m"(dq):);
	if (DISPATCH_OBJ_IS_VTABLE(item))
		invoke_ptr = (uint64_t)(item->do_vtable->do_invoke);
	else
		invoke_ptr = 0;
	kdebug_trace(DISPATCH_ENQUEUE, 0xcULL,
		dq, (uint64_t)item,
		(uint64_t)item->dc_ctxt, 0);
	kdebug_trace(DISPATCH_ENQUEUE, 1ULL << 32 | 0xc,
		(uint64_t)item->dc_func, invoke_ptr,
		(uint64_t)item->do_vtable, 0);
	restore_registers
	//simulation
	asm volatile("movq	%%rax, %%rsi\n"
		"xchgq	%%rsi, 0x40(%%rdi)"
		::);
}
#endif
#if __i386__
/* 0xf
 * _dispatch_async_f:
 * 0x0L	000081ef	55 	pushl	%ebp
 * 0x173L	00008362	31 db 	xorl	%ebx, %ebx
 * 0x175L	00008364	c7 42 0c 00 00 00 00 	movl	$0x0, 0xc(%edx)
 * 0x17cL	0000836b	89 d0 	movl	%edx, %eax
 * 0x17eL	0000836d	87 41 28 	xchgl	%eax, 0x28(%ecx)
 * 0x181L	00008370	85 c0 	testl	%eax, %eax
 * 0x183L	00008372	74 15 	je	0x8389
 * 0x185L	00008374	89 50 0c 	movl	%edx, 0xc(%eax)
 * 0x188L	00008377	84 db 	testb	%bl, %bl
 * target: 0000836d	87 41 28 	xchgl	%eax, 0x28(%ecx)
 * verify: 0000839b	e8 40 c3 ff ff 	calll	0x46e0
 */
void shell_enq_dispatch_async_f()
{
	save_registers
	uint64_t dq, invoke_ptr;
	struct dispatch_patch_item_head *item;
	asm volatile("movl %%edx, %0\n"
		"movl %%ecx, %1"
		:"=m"(item), "=m"(dq):);
	if (DISPATCH_OBJ_IS_VTABLE(item))
		invoke_ptr = (uint64_t)(item->do_vtable->do_invoke);
	else
		invoke_ptr = 0;
	kdebug_trace(DISPATCH_ENQUEUE, 0xfULL,
		dq, (uint64_t)item,
		(uint64_t)item->dc_ctxt, 0);
	kdebug_trace(DISPATCH_ENQUEUE, 1ULL << 32 | 0xf,
		(uint64_t)item->dc_func, invoke_ptr,
		(uint64_t)item->do_vtable, 0);
	restore_registers
	//simulation
	asm volatile("movl	%%edx, %%eax\n"
		"xchgl	%%eax, 0x28(%%ecx)"
		::);
}
#endif
void detour_dispatch_async_f(struct mach_o_handler *handler_ptr)
{
#if __x86_64__
	detour_function(handler_ptr, "dispatch_async_f", 
		shell_enq_dispatch_async_f, 0x14a, 7);
#elif __i386__
	detour_function(handler_ptr, "dispatch_async_f", 
		shell_enq_dispatch_async_f, 0x17c, 5);
#endif
}
/************/
#if __x86_64__
/* 0xd
 * __dispatch_async_f_slow:
 * 0x0L	0000000000008cae	55              	pushq	%rbp
 * 0x171L	0000000000008e1f	31c9            	xorl	%ecx, %ecx
 * 0x173L	0000000000008e21	48c7401000000000	movq	$0x0, 0x10(%rax)
 * 0x17bL	0000000000008e29	4889c2          	movq	%rax, %rdx
 * 0x17eL	0000000000008e2c	49875640        	xchgq	%rdx, 0x40(%r14)
 * 0x182L	0000000000008e30	4885d2          	testq	%rdx, %rdx
 * 0x185L	0000000000008e33	7421            	je	0x8e56
 * 0x187L	0000000000008e35	48894210        	movq	%rax, 0x10(%rdx)
 * 0x18bL	0000000000008e39	84c9            	testb	%cl, %cl
 * target: 0000000000008e2c	49875640        	xchgq	%rdx, 0x40(%r14)
 * verify: 0000000000008e70	e94cc3ffff      	jmp	0x51c1
 */
void shell_enq__dispatch_async_f_slow()
{
	save_registers
	uint64_t dq, invoke_ptr;
	struct dispatch_patch_item_head *item;
	asm volatile("movq %%rax, %0\n"
		"movq %%r14, %1"
		:"=m"(item), "=m"(dq):);
	if (DISPATCH_OBJ_IS_VTABLE(item))
		invoke_ptr = (uint64_t)(item->do_vtable->do_invoke);
	else
		invoke_ptr = 0;
	kdebug_trace(DISPATCH_ENQUEUE, 0xdULL,
		dq, (uint64_t)item,
		(uint64_t)item->dc_ctxt, 0);
	kdebug_trace(DISPATCH_ENQUEUE, 1ULL << 32 | 0xd,
		(uint64_t)item->dc_func, invoke_ptr,
		(uint64_t)item->do_vtable, 0);
	restore_registers
	//simulation
	asm volatile("movq	%%rax, %%rdx\n"
		"xchgq	%%rdx, 0x40(%%r14)"
		::);
}
#endif
#if __i386__
/* 0x10
 * __dispatch_async_f_slow:
 * 0x0L	000083c3	55 	pushl	%ebp
 * 0x14eL	00008511	31 d2 	xorl	%edx, %edx
 * 0x150L	00008513	c7 40 0c 00 00 00 00 	movl	$0x0, 0xc(%eax)
 * 0x157L	0000851a	89 c6 	movl	%eax, %esi
 * 0x159L	0000851c	87 71 28 	xchgl	%esi, 0x28(%ecx)
 * 0x15cL	0000851f	85 f6 	testl	%esi, %esi
 * 0x15eL	00008521	74 15 	je	0x8538
 * 0x160L	00008523	89 46 0c 	movl	%eax, 0xc(%esi)
 * 0x163L	00008526	84 d2 	testb	%dl, %dl
 * target: 0000851c	87 71 28 	xchgl	%esi, 0x28(%ecx)
 * verify: 0000854a	e8 91 c1 ff ff 	calll	0x46e0
 */
void shell_enq__dispatch_async_f_slow()
{
	save_registers
	uint64_t dq, invoke_ptr;
	struct dispatch_patch_item_head *item;
	//simulation
	asm volatile("movl %%eax, %%esi\n"
		"xchgl %%esi, 0x28(%%ecx)\n"
		"movl %%esi, -0xc(%%ebp)\n"
		"movl %%eax, %0\n"
		"movl %%ecx, %1"
		:"=m"(item), "=m"(dq):);
	if (DISPATCH_OBJ_IS_VTABLE(item))
		invoke_ptr = (uint64_t)(item->do_vtable->do_invoke);
	else
		invoke_ptr = 0;
	kdebug_trace(DISPATCH_ENQUEUE, 0x10ULL,
		dq, (uint64_t)item,
		(uint64_t)item->dc_ctxt, 0);
	kdebug_trace(DISPATCH_ENQUEUE, 1ULL << 32 | 0x10,
		(uint64_t)item->dc_func, invoke_ptr,
		(uint64_t)item->do_vtable, 0);
	restore_registers
	/*simulation
	asm volatile("movl	%%eax, %%esi\n"
		"xchgl	%%esi, 0x28(%%ecx)"
		::);
	*/
}
#endif
void detour__dispatch_async_f_slow(struct mach_o_handler *handler_ptr)
{
#if __x86_64__
	detour_function(handler_ptr, "_dispatch_async_f_slow", 
		shell_enq__dispatch_async_f_slow, 0x17b, 7);
#elif __i386__
	detour_function(handler_ptr, "_dispatch_async_f_slow", 
		shell_enq__dispatch_async_f_slow, 0x157, 5 );
#endif
}
/************/
#if __x86_64__
/* 0x10
 * __dispatch_mach_msg_send:
 * 0x0L	000000000000999f	55              	pushq	%rbp
 * 0x769L	000000000000a108	31c0            	xorl	%eax, %eax
 * 0x76bL	000000000000a10a	49c744241000000000	movq	$0x0, 0x10(%r12)
 * 0x774L	000000000000a113	4c89e1          	movq	%r12, %rcx
 * 0x777L	000000000000a116	49874d40        	xchgq	%rcx, 0x40(%r13)
 * 0x77bL	000000000000a11a	4885c9          	testq	%rcx, %rcx
 * 0x77eL	000000000000a11d	7412            	je	0xa131
 * 0x780L	000000000000a11f	4c896110        	movq	%r12, 0x10(%rcx)
 * 0x784L	000000000000a123	84c0            	testb	%al, %al
 * target: 000000000000a116	49874d40        	xchgq	%rcx, 0x40(%r13)
 * verify: 000000000000a13a	e882b0ffff      	callq	0x51c1
 */
void shell_enq__dispatch_mach_msg_send_0()
{
	save_registers
	uint64_t dq, invoke_ptr;
	struct dispatch_patch_item_head *item;
	asm volatile("movq %%r12, %0\n"
		"movq %%r13, %1"
		:"=m"(item), "=m"(dq):);
	if (DISPATCH_OBJ_IS_VTABLE(item))
		invoke_ptr = (uint64_t)(item->do_vtable->do_invoke);
	else
		invoke_ptr = 0;
	kdebug_trace(DISPATCH_ENQUEUE, 0x10ULL,
		dq, (uint64_t)item,
		(uint64_t)item->dc_ctxt, 0);
	kdebug_trace(DISPATCH_ENQUEUE, 1ULL << 32 | 0x10,
		(uint64_t)item->dc_func, invoke_ptr,
		(uint64_t)item->do_vtable, 0);
	restore_registers
	//simulation
	asm volatile("movq	%%r12, %%rcx\n"
		"xchgq	%%rcx, 0x40(%%r13)"
		::);
}
/* 0x11
 * __dispatch_mach_msg_send:
 * 0x0L	000000000000999f	55              	pushq	%rbp
 * 0x80aL	000000000000a1a9	488d5740        	leaq	0x40(%rdi), %rdx
 * 0x80eL	000000000000a1ad	48c7431000000000	movq	$0x0, 0x10(%rbx)
 * 0x816L	000000000000a1b5	4889d9          	movq	%rbx, %rcx
 * 0x819L	000000000000a1b8	48870a          	xchgq	%rcx, (%rdx)
 * 0x81cL	000000000000a1bb	4885c9          	testq	%rcx, %rcx
 * 0x81fL	000000000000a1be	7412            	je	0xa1d2
 * 0x821L	000000000000a1c0	48895910        	movq	%rbx, 0x10(%rcx)
 * 0x825L	000000000000a1c4	4531e4          	xorl	%r12d, %r12d
 * target: 000000000000a1b8	48870a          	xchgq	%rcx, (%rdx)
 * verify: 000000000000a1d8	e8e4afffff      	callq	0x51c1
 */
void shell_enq__dispatch_mach_msg_send_1()
{
	save_registers
	uint64_t dq, invoke_ptr;
	struct dispatch_patch_item_head *item;
	asm volatile("movq %%rbx, %0\n"
		"movq %%rdx, %%rax\n"
		"subq $0x40, %%rax\n"
		"movq %%rax, %1"
		:"=m"(item), "=m"(dq):);
	if (DISPATCH_OBJ_IS_VTABLE(item))
		invoke_ptr = (uint64_t)(item->do_vtable->do_invoke);
	else
		invoke_ptr = 0;
	kdebug_trace(DISPATCH_ENQUEUE, 0x11ULL,
		dq, (uint64_t)item,
		(uint64_t)item->dc_ctxt, 0);
	kdebug_trace(DISPATCH_ENQUEUE, 1ULL << 32 | 0x11,
		(uint64_t)item->dc_func, invoke_ptr,
		(uint64_t)item->do_vtable, 0);
	restore_registers
	//simulation
	asm volatile("movq	%%rbx, %%rcx\n"
		"xchgq	%%rcx, (%%rdx)"
		::);
}
#endif
#if __i386__
/* 0x15
 * __dispatch_mach_msg_send:
 * 0x0L	00008fe5	55 	pushl	%ebp
 * 0x612L	000095f7	8b 55 bc 	movl	-0x44(%ebp), %edx
 * 0x615L	000095fa	c7 42 0c 00 00 00 00 	movl	$0x0, 0xc(%edx)
 * 0x61cL	00009601	89 d3 	movl	%edx, %ebx
 * 0x61eL	00009603	87 56 28 	xchgl	%edx, 0x28(%esi)
 * 0x621L	00009606	85 d2 	testl	%edx, %edx
 * 0x623L	00009608	74 15 	je	0x961f
 * 0x625L	0000960a	89 5a 0c 	movl	%ebx, 0xc(%edx)
 * 0x628L	0000960d	84 c9 	testb	%cl, %cl
 * target: 00009603	87 56 28 	xchgl	%edx, 0x28(%esi)
 * verify: 00009631	e8 aa b0 ff ff 	calll	0x46e0
 */
void shell_enq__dispatch_mach_msg_send_0()
{
	save_registers
	uint64_t dq, invoke_ptr;
	struct dispatch_patch_item_head *item;
	//simulation
	asm volatile("movl %%edx, %0\n"
		"movl %%esi, %1\n"
		"movl %%edx, -0x4(%%ebp)"
		:"=m"(item), "=m"(dq):);
	if (DISPATCH_OBJ_IS_VTABLE(item))
		invoke_ptr = (uint64_t)(item->do_vtable->do_invoke);
	else
		invoke_ptr = 0;
	kdebug_trace(DISPATCH_ENQUEUE, 0x15ULL,
		dq, (uint64_t)item,
		(uint64_t)item->dc_ctxt, 0);
	kdebug_trace(DISPATCH_ENQUEUE, 1ULL << 32 | 0x15,
		(uint64_t)item->dc_func, invoke_ptr,
		(uint64_t)item->do_vtable, 0);
	restore_registers
	//simulation
	asm volatile(//"movl	%%edx, %%ebx\n"
		"xchgl	%%edx, 0x28(%%esi)"
		::);
}
/* 0x16
 * __dispatch_mach_msg_send:
 * 0x0L	00008fe5	55 	pushl	%ebp
 * 0x6bdL	000096a2	8d 77 28 	leal	0x28(%edi), %esi
 * 0x6c0L	000096a5	c7 43 0c 00 00 00 00 	movl	$0x0, 0xc(%ebx)
 * 0x6c7L	000096ac	89 da 	movl	%ebx, %edx
 * 0x6c9L	000096ae	87 16 	xchgl	%edx, (%esi)
 * 0x6cbL	000096b0	85 d2 	testl	%edx, %edx
 * 0x6cdL	000096b2	74 17 	je	0x96cb
 * 0x6cfL	000096b4	89 5a 0c 	movl	%ebx, 0xc(%edx)
 * 0x6d2L	000096b7	31 d2 	xorl	%edx, %edx
 * target: 000096ae	87 16 	xchgl	%edx, (%esi)
 * verify: 000096dd	e8 fe af ff ff 	calll	0x46e0
 */
void shell_enq__dispatch_mach_msg_send_1()
{
	save_registers
	uint64_t dq, invoke_ptr;
	struct dispatch_patch_item_head *item;
	asm volatile("movl %%ebx, %0\n"
		"subl 0x28, %%esi\n"
		"movl %%esi, %1"
		:"=m"(item), "=m"(dq):);
	if (DISPATCH_OBJ_IS_VTABLE(item))
		invoke_ptr = (uint64_t)(item->do_vtable->do_invoke);
	else
		invoke_ptr = 0;
	kdebug_trace(DISPATCH_ENQUEUE, 0x16ULL,
		dq, (uint64_t)item,
		(uint64_t)item->dc_ctxt, 0);
	kdebug_trace(DISPATCH_ENQUEUE, 1ULL << 32 | 0x16,
		(uint64_t)item->dc_func, invoke_ptr,
		(uint64_t)item->do_vtable, 0);
	restore_registers
	//simulation
	asm volatile("movl $0x0, 0xc(%%ebx)"
		::);
}
#endif
void detour__dispatch_mach_msg_send(struct mach_o_handler *handler_ptr)
{
#if __x86_64__
	detour_function(handler_ptr, "_dispatch_mach_msg_send", 
		shell_enq__dispatch_mach_msg_send_0,  0x774, 7);
	detour_function(handler_ptr, "_dispatch_mach_msg_send", 
		shell_enq__dispatch_mach_msg_send_1, 0x816, 6);
#elif __i386__
	detour_function(handler_ptr, "_dispatch_mach_msg_send", 
		shell_enq__dispatch_mach_msg_send_0, 0x61c, 5);
	detour_function(handler_ptr, "_dispatch_mach_msg_send", 
		shell_enq__dispatch_mach_msg_send_1, 0x6c0, 7);
#endif
}
/************/
#if __x86_64__
/* 0x12
 * __dispatch_mach_send_drain:
 * 0x0L	000000000000aa98	55              	pushq	%rbp
 * 0x162L	000000000000abfa	31c0            	xorl	%eax, %eax
 * 0x164L	000000000000abfc	49c7451000000000	movq	$0x0, 0x10(%r13)
 * 0x16cL	000000000000ac04	4c89e9          	movq	%r13, %rcx
 * 0x16fL	000000000000ac07	49874e40        	xchgq	%rcx, 0x40(%r14)
 * 0x173L	000000000000ac0b	4885c9          	testq	%rcx, %rcx
 * 0x176L	000000000000ac0e	741e            	je	0xac2e
 * 0x178L	000000000000ac10	4c896910        	movq	%r13, 0x10(%rcx)
 * 0x17cL	000000000000ac14	84c0            	testb	%al, %al
 * target: 000000000000ac07	49874e40        	xchgq	%rcx, 0x40(%r14)
 * verify: 000000000000ac45	e977a5ffff      	jmp	0x51c1
 */
void shell_enq__dispatch_mach_send_drain()
{
	save_registers
	uint64_t dq, invoke_ptr;
	struct dispatch_patch_item_head *item;
	asm volatile("movq %%r13, %0\n"
		"movq %%r14, %1"
		:"=m"(item), "=m"(dq):);
	if (DISPATCH_OBJ_IS_VTABLE(item))
		invoke_ptr = (uint64_t)(item->do_vtable->do_invoke);
	else
		invoke_ptr = 0;
	kdebug_trace(DISPATCH_ENQUEUE, 0x12ULL,
		dq, (uint64_t)item,
		(uint64_t)item->dc_ctxt, 0);
	kdebug_trace(DISPATCH_ENQUEUE, 1ULL << 32 | 0x12,
		(uint64_t)item->dc_func, invoke_ptr,
		(uint64_t)item->do_vtable, 0);
	restore_registers
	//simulation
	asm volatile("movq	%%r13, %%rcx\n"
		"xchgq	%%rcx, 0x40(%%r14)"
		::);
}
#endif
#if __i386__
/* 0x17
 * __dispatch_mach_send_drain:
 * 0x0L	0000a175	55 	pushl	%ebp
 * 0x150L	0000a2c5	c7 46 0c 00 00 00 00 	movl	$0x0, 0xc(%esi)
 * 0x157L	0000a2cc	89 f2 	movl	%esi, %edx
 * 0x159L	0000a2ce	8b 7d f0 	movl	-0x10(%ebp), %edi
 * 0x15cL	0000a2d1	87 57 28 	xchgl	%edx, 0x28(%edi)
 * 0x15fL	0000a2d4	85 d2 	testl	%edx, %edx
 * 0x161L	0000a2d6	74 1c 	je	0xa2f4
 * 0x163L	0000a2d8	89 72 0c 	movl	%esi, 0xc(%edx)
 * 0x166L	0000a2db	84 c9 	testb	%cl, %cl
 * target: 0000a2d1	87 57 28 	xchgl	%edx, 0x28(%edi)
 * verify: 0000a306	e8 d5 a3 ff ff 	calll	0x46e0
 */
void shell_enq__dispatch_mach_send_drain()
{
	save_registers
	uint64_t dq, invoke_ptr;
	struct dispatch_patch_item_head *item;
	//simulation
	asm volatile("movl %%esi, %0\n"
		"movl (%%ebp), %%eax\n"
		"movl -0x10(%%eax), %%edi\n"
		"movl %%edi, -0x8(%%ebp)\n"
		"movl %%edi, %1"
		:"=m"(item), "=m"(dq):);
	if (DISPATCH_OBJ_IS_VTABLE(item))
		invoke_ptr = (uint64_t)(item->do_vtable->do_invoke);
	else
		invoke_ptr = 0;
	kdebug_trace(DISPATCH_ENQUEUE, 0x17ULL,
		dq, (uint64_t)item,
		(uint64_t)item->dc_ctxt, 0);
	kdebug_trace(DISPATCH_ENQUEUE, 1ULL << 32 | 0x17,
		(uint64_t)item->dc_func, invoke_ptr,
		(uint64_t)item->do_vtable, 0);
	restore_registers
	//simulation
	asm volatile("movl %0, %%edi\n"
		"xchgl	%%edx, 0x28(%%edi)"
		::"m"(dq));
}
#endif
void detour__dispatch_mach_send_drain(struct mach_o_handler *handler_ptr)
{
#if __x86_64__
	detour_function(handler_ptr, "_dispatch_mach_send_drain", 
		shell_enq__dispatch_mach_send_drain, 0x16c, 7);
#elif __i386__
	detour_function(handler_ptr, "_dispatch_mach_send_drain", 
		shell_enq__dispatch_mach_send_drain, 0x159, 6);
#endif
}
/************/
#if __x86_64__
/* 0x13
 * __dispatch_mach_msg_recv:
 * 0x0L	000000000000b7a9	55              	pushq	%rbp
 * 0x178L	000000000000b921	31db            	xorl	%ebx, %ebx
 * 0x17aL	000000000000b923	48c7401000000000	movq	$0x0, 0x10(%rax)
 * 0x182L	000000000000b92b	4889c1          	movq	%rax, %rcx
 * 0x185L	000000000000b92e	49874e40        	xchgq	%rcx, 0x40(%r14)
 * 0x189L	000000000000b932	4885c9          	testq	%rcx, %rcx
 * 0x18cL	000000000000b935	741e            	je	0xb955
 * 0x18eL	000000000000b937	48894110        	movq	%rax, 0x10(%rcx)
 * 0x192L	000000000000b93b	84db            	testb	%bl, %bl
 * target: 000000000000b92e	49874e40        	xchgq	%rcx, 0x40(%r14)
 * verify: 000000000000b96c	e95098ffff      	jmp	0x51c1
 */
void shell_enq__dispatch_mach_msg_recv()
{
	save_registers
	uint64_t dq, invoke_ptr;
	struct dispatch_patch_item_head *item;
	asm volatile("movq %%rax, %0\n"
		"movq %%r14, %1"
		:"=m"(item), "=m"(dq):);
	if (DISPATCH_OBJ_IS_VTABLE(item))
		invoke_ptr = (uint64_t)(item->do_vtable->do_invoke);
	else
		invoke_ptr = 0;
	kdebug_trace(DISPATCH_ENQUEUE, 0x13ULL,
		dq, (uint64_t)item,
		(uint64_t)item->dc_ctxt, 0);
	kdebug_trace(DISPATCH_ENQUEUE, 1ULL << 32 | 0x13,
		(uint64_t)item->dc_func, invoke_ptr,
		(uint64_t)item->do_vtable, 0);
	restore_registers
	//simulation
	asm volatile("movq	%%rax, %%rcx\n"
		"xchgq	%%rcx, 0x40(%%r14)"
		::);
}
#endif
#if __i386__
/* 0x18
 * __dispatch_mach_msg_recv:
 * 0x0L	0000af4b	55 	pushl	%ebp
 * 0x164L	0000b0af	8b 5d f0 	movl	-0x10(%ebp), %ebx
 * 0x167L	0000b0b2	c7 40 0c 00 00 00 00 	movl	$0x0, 0xc(%eax)
 * 0x16eL	0000b0b9	89 c6 	movl	%eax, %esi
 * 0x170L	0000b0bb	87 73 28 	xchgl	%esi, 0x28(%ebx)
 * 0x173L	0000b0be	85 f6 	testl	%esi, %esi
 * 0x175L	0000b0c0	74 15 	je	0xb0d7
 * 0x177L	0000b0c2	89 46 0c 	movl	%eax, 0xc(%esi)
 * 0x17aL	0000b0c5	84 d2 	testb	%dl, %dl
 * target: 0000b0bb	87 73 28 	xchgl	%esi, 0x28(%ebx)
 * verify: 0000b0e9	e8 f2 95 ff ff 	calll	0x46e0
 */
void shell_enq__dispatch_mach_msg_recv()
{
	save_registers
	uint64_t dq, invoke_ptr;
	struct dispatch_patch_item_head *item;
	//simulation
	asm volatile("movl %%eax, %%esi\n"
		"xchgl %%esi, 0x28(%%ebx)\n"
		"movl %%esi, -0xc(%%ebp)\n"
		"movl %%eax, %0\n"
		"movl %%ebx, %1"
		:"=m"(item), "=m"(dq):);
	if (DISPATCH_OBJ_IS_VTABLE(item))
		invoke_ptr = (uint64_t)(item->do_vtable->do_invoke);
	else
		invoke_ptr = 0;
	kdebug_trace(DISPATCH_ENQUEUE, 0x18ULL,
		dq, (uint64_t)item,
		(uint64_t)item->dc_ctxt, 0);
	kdebug_trace(DISPATCH_ENQUEUE, 1ULL << 32 | 0x18,
		(uint64_t)item->dc_func, invoke_ptr,
		(uint64_t)item->do_vtable, 0);
	restore_registers
	/*simulation
	asm volatile("movl	%%eax, %%esi\n"
		"xchgl	%%esi, 0x28(%%ebx)"
		::);
	*/
}
#endif
void detour__dispatch_mach_msg_recv(struct mach_o_handler *handler_ptr)
{
#if __x86_64__
	detour_function(handler_ptr, "_dispatch_mach_msg_recv", 
		shell_enq__dispatch_mach_msg_recv, 0x182, 7);
#elif __i386__
	detour_function(handler_ptr, "_dispatch_mach_msg_recv", 
		shell_enq__dispatch_mach_msg_recv, 0x16e, 5);
#endif
}
/************/
#if __x86_64__
/* 0x14
 * __dispatch_mach_reply_kevent_unregister:
 * 0x0L	000000000000b980	55              	pushq	%rbp
 * 0x15aL	000000000000bada	31c0            	xorl	%eax, %eax
 * 0x15cL	000000000000badc	49c7471000000000	movq	$0x0, 0x10(%r15)
 * 0x164L	000000000000bae4	4c89f9          	movq	%r15, %rcx
 * 0x167L	000000000000bae7	49874e40        	xchgq	%rcx, 0x40(%r14)
 * 0x16bL	000000000000baeb	4885c9          	testq	%rcx, %rcx
 * 0x16eL	000000000000baee	7425            	je	0xbb15
 * 0x170L	000000000000baf0	4c897910        	movq	%r15, 0x10(%rcx)
 * 0x174L	000000000000baf4	84c0            	testb	%al, %al
 * target: 000000000000bae7	49874e40        	xchgq	%rcx, 0x40(%r14)
 * verify: 000000000000bb28	e99496ffff      	jmp	0x51c1
 */
void shell_enq__dispatch_mach_reply_kevent_unregister()
{
	save_registers
	uint64_t dq, invoke_ptr;
	struct dispatch_patch_item_head *item;
	asm volatile("movq %%r15, %0\n"
		"movq %%r14, %1"
		:"=m"(item), "=m"(dq):);
	if (DISPATCH_OBJ_IS_VTABLE(item))
		invoke_ptr = (uint64_t)(item->do_vtable->do_invoke);
	else
		invoke_ptr = 0;
	kdebug_trace(DISPATCH_ENQUEUE, 0x14ULL,
		dq, (uint64_t)item,
		(uint64_t)item->dc_ctxt, 0);
	kdebug_trace(DISPATCH_ENQUEUE, 1ULL << 32 | 0x14,
		(uint64_t)item->dc_func, invoke_ptr,
		(uint64_t)item->do_vtable, 0);
	restore_registers
	//simulation
	asm volatile("movq	%%r15, %%rcx\n"
		"xchgq	%%rcx, 0x40(%%r14)"
		::);
}
#endif
#if __i386__
/* 0x19
 * __dispatch_mach_reply_kevent_unregister:
 * 0x0L	0000b104	55 	pushl	%ebp
 * 0x15aL	0000b25e	31 c9 	xorl	%ecx, %ecx
 * 0x15cL	0000b260	c7 47 0c 00 00 00 00 	movl	$0x0, 0xc(%edi)
 * 0x163L	0000b267	89 fa 	movl	%edi, %edx
 * 0x165L	0000b269	87 56 28 	xchgl	%edx, 0x28(%esi)
 * 0x168L	0000b26c	85 d2 	testl	%edx, %edx
 * 0x16aL	0000b26e	74 15 	je	0xb285
 * 0x16cL	0000b270	89 7a 0c 	movl	%edi, 0xc(%edx)
 * 0x16fL	0000b273	84 c9 	testb	%cl, %cl
 * target: 0000b269	87 56 28 	xchgl	%edx, 0x28(%esi)
 * verify: 0000b297	e8 44 94 ff ff 	calll	0x46e0
 */
void shell_enq__dispatch_mach_reply_kevent_unregister()
{
	save_registers
	uint64_t dq, invoke_ptr;
	struct dispatch_patch_item_head *item;
	asm volatile("movl %%edi, %0\n"
		"movl %%esi, %1"
		:"=m"(item), "=m"(dq):);
	if (DISPATCH_OBJ_IS_VTABLE(item))
		invoke_ptr = (uint64_t)(item->do_vtable->do_invoke);
	else
		invoke_ptr = 0;
	kdebug_trace(DISPATCH_ENQUEUE, 0x19ULL,
		dq, (uint64_t)item,
		(uint64_t)item->dc_ctxt, 0);
	kdebug_trace(DISPATCH_ENQUEUE, 1ULL << 32 | 0x19,
		(uint64_t)item->dc_func, invoke_ptr,
		(uint64_t)item->do_vtable, 0);
	restore_registers
	//simulation
	asm volatile("movl	%%edi, %%edx\n"
		"xchgl	%%edx, 0x28(%%esi)"
		::);
}
#endif
void detour__dispatch_mach_reply_kevent_unregister(struct mach_o_handler *handler_ptr)
{
#if __x86_64__
	detour_function(handler_ptr, "_dispatch_mach_reply_kevent_unregister", 
		shell_enq__dispatch_mach_reply_kevent_unregister, 0x164, 7);
#elif __i386__
	detour_function(handler_ptr, "_dispatch_mach_reply_kevent_unregister", 
		shell_enq__dispatch_mach_reply_kevent_unregister, 0x163, 5);
#endif
}
/************/
#if __x86_64__
/* 0x15
 * _dispatch_async_enforce_qos_class_f:
 * 0x0L	000000000000bbb6	0fb74750        	movzwl	0x50(%rdi), %eax
 * 0x113L	000000000000bcc9	31c9            	xorl	%ecx, %ecx
 * 0x115L	000000000000bccb	48c7401000000000	movq	$0x0, 0x10(%rax)
 * 0x11dL	000000000000bcd3	4889c6          	movq	%rax, %rsi
 * 0x120L	000000000000bcd6	48877740        	xchgq	%rsi, 0x40(%rdi)
 * 0x124L	000000000000bcda	4885f6          	testq	%rsi, %rsi
 * 0x127L	000000000000bcdd	7410            	je	0xbcef
 * 0x129L	000000000000bcdf	48894610        	movq	%rax, 0x10(%rsi)
 * 0x12dL	000000000000bce3	84c9            	testb	%cl, %cl
 * target: 000000000000bcd6	48877740        	xchgq	%rsi, 0x40(%rdi)
 * verify: 000000000000bcf8	e9c494ffff      	jmp	0x51c1
 */
void shell_enq_dispatch_async_enforce_qos_class_f()
{
	save_registers
	uint64_t dq, invoke_ptr;
	struct dispatch_patch_item_head *item;
	asm volatile("movq %%rax, %0\n"
		"movq %%rdi, %1"
		:"=m"(item), "=m"(dq):);
	if (DISPATCH_OBJ_IS_VTABLE(item))
		invoke_ptr = (uint64_t)(item->do_vtable->do_invoke);
	else
		invoke_ptr = 0;
	kdebug_trace(DISPATCH_ENQUEUE, 0x15ULL,
		dq, (uint64_t)item,
		(uint64_t)item->dc_ctxt, 0);
	kdebug_trace(DISPATCH_ENQUEUE, 1ULL << 32 | 0x15,
		(uint64_t)item->dc_func, invoke_ptr,
		(uint64_t)item->do_vtable, 0);
	restore_registers
	//simulation
	asm volatile("movq	%%rax, %%rsi\n"
		"xchgq	%%rsi, 0x40(%%rdi)"
		::);
}
#endif
#if __i386__
/* 0x1a
 * _dispatch_async_enforce_qos_class_f:
 * 0x0L	0000b34f	55 	pushl	%ebp
 * 0x142L	0000b491	31 db 	xorl	%ebx, %ebx
 * 0x144L	0000b493	c7 42 0c 00 00 00 00 	movl	$0x0, 0xc(%edx)
 * 0x14bL	0000b49a	89 d6 	movl	%edx, %esi
 * 0x14dL	0000b49c	87 71 28 	xchgl	%esi, 0x28(%ecx)
 * 0x150L	0000b49f	85 f6 	testl	%esi, %esi
 * 0x152L	0000b4a1	74 15 	je	0xb4b8
 * 0x154L	0000b4a3	89 56 0c 	movl	%edx, 0xc(%esi)
 * 0x157L	0000b4a6	84 db 	testb	%bl, %bl
 * target: 0000b49c	87 71 28 	xchgl	%esi, 0x28(%ecx)
 * verify: 0000b4ca	e8 11 92 ff ff 	calll	0x46e0
 */
void shell_enq_dispatch_async_enforce_qos_class_f()
{
	save_registers
	uint64_t dq, invoke_ptr;
	struct dispatch_patch_item_head *item;
	asm volatile("movl %%edx, %%esi\n"
		"xchgl %%esi, 0x28(%%ecx)\n"
		"movl %%esi, -0xc(%%ebp)\n"
		"movl %%edx, %0\n"
		"movl %%ecx, %1"
		:"=m"(item), "=m"(dq):);
	if (DISPATCH_OBJ_IS_VTABLE(item))
		invoke_ptr = (uint64_t)(item->do_vtable->do_invoke);
	else
		invoke_ptr = 0;
	kdebug_trace(DISPATCH_ENQUEUE, 0x1aULL,
		dq, (uint64_t)item,
		(uint64_t)item->dc_ctxt, 0);
	kdebug_trace(DISPATCH_ENQUEUE, 1ULL << 32 | 0x1a,
		(uint64_t)item->dc_func, invoke_ptr,
		(uint64_t)item->do_vtable, 0);
	restore_registers
	/* simulation
	asm volatile("movl	%%edx, %%esi\n"
		"xchgl	%%esi, 0x28(%%ecx)"
		::);
	*/
}
#endif
void detour_dispatch_async_enforce_qos_class_f(struct mach_o_handler *handler_ptr)
{
#if __x86_64__
	detour_function(handler_ptr, "dispatch_async_enforce_qos_class_f", 
		shell_enq_dispatch_async_enforce_qos_class_f, 0x11d, 7);
#elif __i386__
	detour_function(handler_ptr, "dispatch_async_enforce_qos_class_f", 
		shell_enq_dispatch_async_enforce_qos_class_f, 0x14b, 5);
#endif
}
/************/
#if __x86_64__
/* 0x19
 * _dispatch_async:
 * 0x0L	000000000000d050	55              	pushq	%rbp
 * 0x207L	000000000000d257	31c0            	xorl	%eax, %eax
 * 0x209L	000000000000d259	48c7421000000000	movq	$0x0, 0x10(%rdx)
 * 0x211L	000000000000d261	4889d1          	movq	%rdx, %rcx
 * 0x214L	000000000000d264	49874e40        	xchgq	%rcx, 0x40(%r14)
 * 0x218L	000000000000d268	4885c9          	testq	%rcx, %rcx
 * 0x21bL	000000000000d26b	0f8483000000    	je	0xd2f4
 * 0x221L	000000000000d271	48895110        	movq	%rdx, 0x10(%rcx)
 * 0x225L	000000000000d275	84c0            	testb	%al, %al
 * target: 000000000000d264	49874e40        	xchgq	%rcx, 0x40(%r14)
 * verify: 000000000000d30b	e9b17effff      	jmp	0x51c1
 */
void shell_enq_dispatch_async()
{
	save_registers
	uint64_t dq, invoke_ptr;
	struct dispatch_patch_item_head *item;
	asm volatile("movq %%rdx, %0\n"
		"movq %%r14, %1"
		:"=m"(item), "=m"(dq):);
	if (DISPATCH_OBJ_IS_VTABLE(item))
		invoke_ptr = (uint64_t)(item->do_vtable->do_invoke);
	else
		invoke_ptr = 0;
	kdebug_trace(DISPATCH_ENQUEUE, 0x19ULL,
		dq, (uint64_t)item,
		(uint64_t)item->dc_ctxt, 0);
	kdebug_trace(DISPATCH_ENQUEUE, 1ULL << 32 | 0x19,
		(uint64_t)item->dc_func, invoke_ptr,
		(uint64_t)item->do_vtable, 0);
	restore_registers
	//simulation
	asm volatile("movq	%%rdx, %%rcx\n"
		"xchgq	%%rcx, 0x40(%%r14)"
		::);
}
#endif
#if __i386__
/* 0x1e
 * _dispatch_async:
 * 0x0L	0000cc00	55 	pushl	%ebp
 * 0x1c4L	0000cdc4	31 c0 	xorl	%eax, %eax
 * 0x1c6L	0000cdc6	c7 42 0c 00 00 00 00 	movl	$0x0, 0xc(%edx)
 * 0x1cdL	0000cdcd	89 d6 	movl	%edx, %esi
 * 0x1cfL	0000cdcf	87 71 28 	xchgl	%esi, 0x28(%ecx)
 * 0x1d2L	0000cdd2	85 f6 	testl	%esi, %esi
 * 0x1d4L	0000cdd4	0f 84 88 00 00 00 	je	0xce62
 * 0x1daL	0000cdda	89 56 0c 	movl	%edx, 0xc(%esi)
 * 0x1ddL	0000cddd	84 c0 	testb	%al, %al
 * target: 0000cdcf	87 71 28 	xchgl	%esi, 0x28(%ecx)
 * verify: 0000ce74	e8 67 78 ff ff 	calll	0x46e0
 */
void shell_enq_dispatch_async()
{
	save_registers
	uint64_t dq, invoke_ptr;
	struct dispatch_patch_item_head *item;
	//simulation
	asm volatile("movl %%edx, %%esi\n"
		"xchgl %%esi, 0x28(%%ecx)\n"
		"movl %%esi, -0xc(%%ebp)\n"
		"movl %%edx, %0\n"
		"movl %%ecx, %1"
		:"=m"(item), "=m"(dq):);
	if (DISPATCH_OBJ_IS_VTABLE(item))
		invoke_ptr = (uint64_t)(item->do_vtable->do_invoke);
	else
		invoke_ptr = 0;
	kdebug_trace(DISPATCH_ENQUEUE, 0x1eULL,
		dq, (uint64_t)item,
		(uint64_t)item->dc_ctxt, 0);
	kdebug_trace(DISPATCH_ENQUEUE, 1ULL << 32 | 0x1e,
		(uint64_t)item->dc_func, invoke_ptr,
		(uint64_t)item->do_vtable, 0);
	restore_registers
	/*simulation
	asm volatile("movl	%%edx, %%esi\n"
		"xchgl	%%esi, 0x28(%%ecx)"
		::);
	*/
}
#endif
void detour_dispatch_async(struct mach_o_handler *handler_ptr)
{
#if __x86_64__
	detour_function(handler_ptr, "dispatch_async", 
		shell_enq_dispatch_async, 0x211, 7);
#elif __i386__
	detour_function(handler_ptr, "dispatch_async", 
		shell_enq_dispatch_async, 0x1cd, 5);
#endif
}
/************/
#if __x86_64__
/* 0x1a
 * __dispatch_barrier_async_f:
 * 0x0L	000000000000d33f	4989d1          	movq	%rdx, %r9
 * 0x15fL	000000000000d49e	31c0            	xorl	%eax, %eax
 * 0x161L	000000000000d4a0	48c7421000000000	movq	$0x0, 0x10(%rdx)
 * 0x169L	000000000000d4a8	4889d6          	movq	%rdx, %rsi
 * 0x16cL	000000000000d4ab	48877740        	xchgq	%rsi, 0x40(%rdi)
 * 0x170L	000000000000d4af	4885f6          	testq	%rsi, %rsi
 * 0x173L	000000000000d4b2	7410            	je	0xd4c4
 * 0x175L	000000000000d4b4	48895610        	movq	%rdx, 0x10(%rsi)
 * 0x179L	000000000000d4b8	84c0            	testb	%al, %al
 * target: 000000000000d4ab	48877740        	xchgq	%rsi, 0x40(%rdi)
 * verify: 000000000000d4cc	e9f07cffff      	jmp	0x51c1
 */
void shell_enq__dispatch_barrier_async_f()
{
	save_registers
	uint64_t dq, invoke_ptr;
	struct dispatch_patch_item_head *item;
	asm volatile("movq %%rdx, %0\n"
		"movq %%rdi, %1"
		:"=m"(item), "=m"(dq):);
	if (DISPATCH_OBJ_IS_VTABLE(item))
		invoke_ptr = (uint64_t)(item->do_vtable->do_invoke);
	else
		invoke_ptr = 0;
	kdebug_trace(DISPATCH_ENQUEUE, 0x1aULL,
		dq, (uint64_t)item,
		(uint64_t)item->dc_ctxt, 0);
	kdebug_trace(DISPATCH_ENQUEUE, 1ULL << 32 | 0x1a,
		(uint64_t)item->dc_func, invoke_ptr,
		(uint64_t)item->do_vtable, 0);
	restore_registers
	//simulation
	asm volatile("movq	%%rdx, %%rsi\n"
		"xchgq	%%rsi, 0x40(%%rdi)"
		::);
}
#endif
#if __i386__
/* 0x1f
 * __dispatch_barrier_async_f:
 * 0x0L	0000ceb4	55 	pushl	%ebp
 * 0x181L	0000d035	31 d2 	xorl	%edx, %edx
 * 0x183L	0000d037	c7 46 0c 00 00 00 00 	movl	$0x0, 0xc(%esi)
 * 0x18aL	0000d03e	89 f0 	movl	%esi, %eax
 * 0x18cL	0000d040	87 41 28 	xchgl	%eax, 0x28(%ecx)
 * 0x18fL	0000d043	85 c0 	testl	%eax, %eax
 * 0x191L	0000d045	74 15 	je	0xd05c
 * 0x193L	0000d047	89 70 0c 	movl	%esi, 0xc(%eax)
 * 0x196L	0000d04a	84 d2 	testb	%dl, %dl
 * target: 0000d040	87 41 28 	xchgl	%eax, 0x28(%ecx)
 * verify: 0000d06e	e8 6d 76 ff ff 	calll	0x46e0
 */
void shell_enq__dispatch_barrier_async_f()
{
	save_registers
	uint64_t dq, invoke_ptr;
	struct dispatch_patch_item_head *item;
	asm volatile("movl %%esi, %0\n"
		"movl %%ecx, %1"
		:"=m"(item), "=m"(dq):);
	if (DISPATCH_OBJ_IS_VTABLE(item))
		invoke_ptr = (uint64_t)(item->do_vtable->do_invoke);
	else
		invoke_ptr = 0;
	kdebug_trace(DISPATCH_ENQUEUE, 0x1fULL,
		dq, (uint64_t)item,
		(uint64_t)item->dc_ctxt, 0);
	kdebug_trace(DISPATCH_ENQUEUE, 1ULL << 32 | 0x1f,
		(uint64_t)item->dc_func, invoke_ptr,
		(uint64_t)item->do_vtable, 0);
	restore_registers
	//simulation
	asm volatile("movl	%%esi, %%eax\n"
		"xchgl	%%eax, 0x28(%%ecx)"
		::);
}
#endif
void detour__dispatch_barrier_async_f(struct mach_o_handler *handler_ptr)
{
#if __x86_64__
	detour_function(handler_ptr, "_dispatch_barrier_async_f", 
		shell_enq__dispatch_barrier_async_f, 0x169, 7);
#elif __i386__
	detour_function(handler_ptr, "_dispatch_barrier_async_f", 
		shell_enq__dispatch_barrier_async_f, 0x18a, 5);
#endif
}
/************/
#if __x86_64__
/* 0x1b
 * _dispatch_mach_send_barrier_f:
 * 0x0L	000000000000d974	55              	pushq	%rbp
 * 0x119L	000000000000da8d	498b8ea8000000  	movq	0xa8(%r14), %rcx
 * 0x120L	000000000000da94	48c7421000000000	movq	$0x0, 0x10(%rdx)
 * 0x128L	000000000000da9c	4889d0          	movq	%rdx, %rax
 * 0x12bL	000000000000da9f	48874140        	xchgq	%rax, 0x40(%rcx)
 * 0x12fL	000000000000daa3	4885c0          	testq	%rax, %rax
 * 0x132L	000000000000daa6	740f            	je	0xdab7
 * 0x134L	000000000000daa8	48895010        	movq	%rdx, 0x10(%rax)
 * 0x138L	000000000000daac	4883c408        	addq	$0x8, %rsp
 * target: 000000000000da9f	48874140        	xchgq	%rax, 0x40(%rcx)
 * verify: 000000000000db0d	e9af76ffff      	jmp	0x51c1
 */
void shell_enq_dispatch_mach_send_barrier_f_0()
{
	save_registers
	uint64_t dq, invoke_ptr;
	struct dispatch_patch_item_head *item;
	asm volatile("movq %%rdx, %0\n"
		"movq %%rcx, %1"
		:"=m"(item), "=m"(dq):);
	if (DISPATCH_OBJ_IS_VTABLE(item))
		invoke_ptr = (uint64_t)(item->do_vtable->do_invoke);
	else
		invoke_ptr = 0;
	kdebug_trace(DISPATCH_ENQUEUE, 0x1bULL,
		dq, (uint64_t)item,
		(uint64_t)item->dc_ctxt, 0);
	kdebug_trace(DISPATCH_ENQUEUE, 1ULL << 32 | 0x1b,
		(uint64_t)item->dc_func, invoke_ptr,
		(uint64_t)item->do_vtable, 0);
	restore_registers
	//simulation
	asm volatile("movq	%%rdx, %%rax\n"
		"xchgq	%%rax, 0x40(%%rcx)"
		::);
}
/* 0x1c
 * _dispatch_mach_send_barrier_f:
 * 0x0L	000000000000d974	55              	pushq	%rbp
 * 0x159L	000000000000dacd	31c0            	xorl	%eax, %eax
 * 0x15bL	000000000000dacf	48c7421000000000	movq	$0x0, 0x10(%rdx)
 * 0x163L	000000000000dad7	4889d1          	movq	%rdx, %rcx
 * 0x166L	000000000000dada	49874e40        	xchgq	%rcx, 0x40(%r14)
 * 0x16aL	000000000000dade	4885c9          	testq	%rcx, %rcx
 * 0x16dL	000000000000dae1	741a            	je	0xdafd
 * 0x16fL	000000000000dae3	48895110        	movq	%rdx, 0x10(%rcx)
 * 0x173L	000000000000dae7	84c0            	testb	%al, %al
 * target: 000000000000dada	49874e40        	xchgq	%rcx, 0x40(%r14)
 * verify: 000000000000db0d	e9af76ffff      	jmp	0x51c1
 */
void shell_enq_dispatch_mach_send_barrier_f_1()
{
	save_registers
	uint64_t dq, invoke_ptr;
	struct dispatch_patch_item_head *item;
	asm volatile("movq %%rdx, %0\n"
		"movq %%r14, %1"
		:"=m"(item), "=m"(dq):);
	if (DISPATCH_OBJ_IS_VTABLE(item))
		invoke_ptr = (uint64_t)(item->do_vtable->do_invoke);
	else
		invoke_ptr = 0;
	kdebug_trace(DISPATCH_ENQUEUE, 0x1cULL,
		dq, (uint64_t)item,
		(uint64_t)item->dc_ctxt, 0);
	kdebug_trace(DISPATCH_ENQUEUE, 1ULL << 32 | 0x1c,
		(uint64_t)item->dc_func, invoke_ptr,
		(uint64_t)item->do_vtable, 0);
	restore_registers
	//simulation
	asm volatile("movq	%%rdx, %%rcx\n"
		"xchgq	%%rcx, 0x40(%%r14)"
		::);
}
#endif
#if __i386__
/* 0x22
 * _dispatch_mach_send_barrier_f:
 * 0x0L	0000d507	55 	pushl	%ebp
 * 0x10bL	0000d612	31 d2 	xorl	%edx, %edx
 * 0x10dL	0000d614	c7 41 0c 00 00 00 00 	movl	$0x0, 0xc(%ecx)
 * 0x114L	0000d61b	89 ce 	movl	%ecx, %esi
 * 0x116L	0000d61d	87 73 28 	xchgl	%esi, 0x28(%ebx)
 * 0x119L	0000d620	85 f6 	testl	%esi, %esi
 * 0x11bL	0000d622	74 15 	je	0xd639
 * 0x11dL	0000d624	89 4e 0c 	movl	%ecx, 0xc(%esi)
 * 0x120L	0000d627	84 d2 	testb	%dl, %dl
 * target: 0000d61d	87 73 28 	xchgl	%esi, 0x28(%ebx)
 * verify: 0000d64b	e8 90 70 ff ff 	calll	0x46e0
 */
void shell_enq_dispatch_mach_send_barrier_f_0()
{
	save_registers
	uint64_t dq, invoke_ptr;
	struct dispatch_patch_item_head *item;
	asm volatile("movl %%ecx, %%esi\n"
		"xchgl %%esi, 0x28(%%ebx)\n"
		"movl %%esi, -0xc(%%ebp)\n"
		"movl %%ecx, %0\n"
		"movl %%ebx, %1"
		:"=m"(item), "=m"(dq):);
	if (DISPATCH_OBJ_IS_VTABLE(item))
		invoke_ptr = (uint64_t)(item->do_vtable->do_invoke);
	else
		invoke_ptr = 0;
	kdebug_trace(DISPATCH_ENQUEUE, 0x22ULL,
		dq, (uint64_t)item,
		(uint64_t)item->dc_ctxt, 0);
	kdebug_trace(DISPATCH_ENQUEUE, 1ULL << 32 | 0x22,
		(uint64_t)item->dc_func, invoke_ptr,
		(uint64_t)item->do_vtable, 0);
	restore_registers
	/*simulation
	asm volatile("movl	%%ecx, %%esi\n"
		"xchgl	%%esi, 0x28(%%ebx)"
		::);
	*/
}
#endif
//TODO: Not verified
#if __i386__
/* 0x23
 * _dispatch_mach_send_barrier_f:
 * 0x0L	0000d507	55 	pushl	%ebp
 * 0x14bL	0000d652	8b 53 68 	movl	0x68(%ebx), %edx
 * 0x14eL	0000d655	c7 41 0c 00 00 00 00 	movl	$0x0, 0xc(%ecx)
 * 0x155L	0000d65c	89 c8 	movl	%ecx, %eax
 * 0x157L	0000d65e	87 42 24 	xchgl	%eax, 0x24(%edx)
 * 0x15aL	0000d661	85 c0 	testl	%eax, %eax
 * 0x15cL	0000d663	74 0b 	je	0xd670
 * 0x15eL	0000d665	89 48 0c 	movl	%ecx, 0xc(%eax)
 * 0x161L	0000d668	83 c4 1c 	addl	$0x1c, %esp
 * target: 0000d65e	87 42 24 	xchgl	%eax, 0x24(%edx)
 */
void shell_enq_dispatch_mach_send_barrier_f_1()
{
	save_registers
	uint64_t dq, invoke_ptr;
	struct dispatch_patch_item_head *item;
	asm volatile("movl %%ecx, %0\n"
		"movl %%edx, %1"
		:"=m"(item), "=m"(dq):);
	if (DISPATCH_OBJ_IS_VTABLE(item))
		invoke_ptr = (uint64_t)(item->do_vtable->do_invoke);
	else
		invoke_ptr = 0;
	kdebug_trace(DISPATCH_ENQUEUE, 0x23ULL,
		dq, (uint64_t)item,
		(uint64_t)item->dc_ctxt, 0);
	kdebug_trace(DISPATCH_ENQUEUE, 1ULL << 32 | 0x23,
		(uint64_t)item->dc_func, invoke_ptr,
		(uint64_t)item->do_vtable, 0);
	restore_registers
	//simulation
	asm volatile("movl	%%ecx, %%eax\n"
		"xchgl	%%eax, 0x24(%%edx)"
		::);
}
#endif
void detour_dispatch_mach_send_barrier_f(struct mach_o_handler *handler_ptr)
{
#if __x86_64__
	detour_function(handler_ptr, "dispatch_mach_send_barrier_f", 
		shell_enq_dispatch_mach_send_barrier_f_0, 0x128, 7);
	detour_function(handler_ptr, "dispatch_mach_send_barrier_f", 
		shell_enq_dispatch_mach_send_barrier_f_1, 0x163, 7);
#elif __i386__
	detour_function(handler_ptr, "dispatch_mach_send_barrier_f", 
		shell_enq_dispatch_mach_send_barrier_f_0, 0x114, 5);
	detour_function(handler_ptr, "dispatch_mach_send_barrier_f", 
		shell_enq_dispatch_mach_send_barrier_f_1, 0x155, 5);
#endif
}
/************/
#if __x86_64__
/* 0x1e
 * __dispatch_mach_cancel:
 * 0x0L	000000000000df55	55              	pushq	%rbp
 * 0xf1L	000000000000e046	31c9            	xorl	%ecx, %ecx
 * 0xf3L	000000000000e048	48c7401000000000	movq	$0x0, 0x10(%rax)
 * 0xfbL	000000000000e050	4889c2          	movq	%rax, %rdx
 * 0xfeL	000000000000e053	48875340        	xchgq	%rdx, 0x40(%rbx)
 * 0x102L	000000000000e057	4885d2          	testq	%rdx, %rdx
 * 0x105L	000000000000e05a	7412            	je	0xe06e
 * 0x107L	000000000000e05c	48894210        	movq	%rax, 0x10(%rdx)
 * 0x10bL	000000000000e060	84c9            	testb	%cl, %cl
 * target: 000000000000e053	48875340        	xchgq	%rdx, 0x40(%rbx)
 * verify: 000000000000e077	e84571ffff      	callq	0x51c1
 */
void shell_enq__dispatch_mach_cancel()
{
	save_registers
	uint64_t dq, invoke_ptr;
	struct dispatch_patch_item_head *item;
	asm volatile("movq %%rax, %0\n"
		"movq %%rbx, %1"
		:"=m"(item), "=m"(dq):);
	if (DISPATCH_OBJ_IS_VTABLE(item))
		invoke_ptr = (uint64_t)(item->do_vtable->do_invoke);
	else
		invoke_ptr = 0;
	kdebug_trace(DISPATCH_ENQUEUE, 0x1eULL,
		dq, (uint64_t)item,
		(uint64_t)item->dc_ctxt, 0);
	kdebug_trace(DISPATCH_ENQUEUE, 1ULL << 32 | 0x1e,
		(uint64_t)item->dc_func, invoke_ptr,
		(uint64_t)item->do_vtable, 0);
	restore_registers
	//simulation
	asm volatile("movq	%%rax, %%rdx\n"
		"xchgq	%%rdx, 0x40(%%rbx)"
		::);
}
#endif
#if __i386__
/* 0x25
 * __dispatch_mach_cancel:
 * 0x0L	0000dbf8	55 	pushl	%ebp
 * 0xfeL	0000dcf6	31 d2 	xorl	%edx, %edx
 * 0x100L	0000dcf8	c7 40 0c 00 00 00 00 	movl	$0x0, 0xc(%eax)
 * 0x107L	0000dcff	89 c3 	movl	%eax, %ebx
 * 0x109L	0000dd01	87 5e 28 	xchgl	%ebx, 0x28(%esi)
 * 0x10cL	0000dd04	85 db 	testl	%ebx, %ebx
 * 0x10eL	0000dd06	74 18 	je	0xdd20
 * 0x110L	0000dd08	89 43 0c 	movl	%eax, 0xc(%ebx)
 * 0x113L	0000dd0b	84 d2 	testb	%dl, %dl
 * target: 0000dd01	87 5e 28 	xchgl	%ebx, 0x28(%esi)
 * verify: 0000dd35	e8 a6 69 ff ff 	calll	0x46e0
 */
void shell_enq__dispatch_mach_cancel()
{
	save_registers
	uint64_t dq, invoke_ptr;
	struct dispatch_patch_item_head *item;
	asm volatile("movl %%eax, %%ebx\n"
		"xchgl %%ebx, 0x28(%%esi)\n"
		"movl %%ebx, -0x4(%%ebp)\n"
		"movl %%eax, %0\n"
		"movl %%esi, %1"
		:"=m"(item), "=m"(dq):);
	if (DISPATCH_OBJ_IS_VTABLE(item))
		invoke_ptr = (uint64_t)(item->do_vtable->do_invoke);
	else
		invoke_ptr = 0;
	kdebug_trace(DISPATCH_ENQUEUE, 0x25ULL,
		dq, (uint64_t)item,
		(uint64_t)item->dc_ctxt, 0);
	kdebug_trace(DISPATCH_ENQUEUE, 1ULL << 32 | 0x25,
		(uint64_t)item->dc_func, invoke_ptr,
		(uint64_t)item->do_vtable, 0);
	restore_registers
	/*simulation
	asm volatile("movl	%%eax, %%ebx\n"
		"xchgl	%%ebx, 0x28(%%esi)"
		::);
	*/
}
#endif
void detour__dispatch_mach_cancel(struct mach_o_handler *handler_ptr)
{
#if __x86_64__
	detour_function(handler_ptr, "_dispatch_mach_cancel", 
		shell_enq__dispatch_mach_cancel, 0xfb, 7);
#elif __i386__
	detour_function(handler_ptr, "_dispatch_mach_cancel", 
		shell_enq__dispatch_mach_cancel, 0x107, 5);
#endif
}
/************/
#if __x86_64__
/* 0x1f
 * __dispatch_mach_disconnect:
 * 0x0L	000000000000e08b	55              	pushq	%rbp
 * 0xd6L	000000000000e161	31c9            	xorl	%ecx, %ecx
 * 0xd8L	000000000000e163	48c7401000000000	movq	$0x0, 0x10(%rax)
 * 0xe0L	000000000000e16b	4889c2          	movq	%rax, %rdx
 * 0xe3L	000000000000e16e	4987542440      	xchgq	%rdx, 0x40(%r12)
 * 0xe8L	000000000000e173	4885d2          	testq	%rdx, %rdx
 * 0xebL	000000000000e176	7412            	je	0xe18a
 * 0xedL	000000000000e178	48894210        	movq	%rax, 0x10(%rdx)
 * 0xf1L	000000000000e17c	84c9            	testb	%cl, %cl
 * target: 000000000000e16e	4987542440      	xchgq	%rdx, 0x40(%r12)
 * verify: 000000000000e193	e82970ffff      	callq	0x51c1
 */
void shell_enq__dispatch_mach_disconnect()
{
	save_registers
	uint64_t dq, invoke_ptr;
	struct dispatch_patch_item_head *item;
	asm volatile("movq %%rax, %0\n"
		"movq %%r12, %1"
		:"=m"(item), "=m"(dq):);
	if (DISPATCH_OBJ_IS_VTABLE(item))
		invoke_ptr = (uint64_t)(item->do_vtable->do_invoke);
	else
		invoke_ptr = 0;
	kdebug_trace(DISPATCH_ENQUEUE, 0x1fULL,
		dq, (uint64_t)item,
		(uint64_t)item->dc_ctxt, 0);
	kdebug_trace(DISPATCH_ENQUEUE, 1ULL << 32 | 0x1f,
		(uint64_t)item->dc_func, invoke_ptr,
		(uint64_t)item->do_vtable, 0);
	restore_registers
	//simulation
	asm volatile("movq	%%rax, %%rdx\n"
		"xchgq	%%rdx, 0x40(%%r12)"
		::);
}
#endif
#if __i386__
/* 0x26
 * __dispatch_mach_disconnect:
 * 0x0L	0000dd47	55 	pushl	%ebp
 * 0xd5L	0000de1c	31 d2 	xorl	%edx, %edx
 * 0xd7L	0000de1e	c7 40 0c 00 00 00 00 	movl	$0x0, 0xc(%eax)
 * 0xdeL	0000de25	89 c3 	movl	%eax, %ebx
 * 0xe0L	0000de27	87 5e 28 	xchgl	%ebx, 0x28(%esi)
 * 0xe3L	0000de2a	85 db 	testl	%ebx, %ebx
 * 0xe5L	0000de2c	74 18 	je	0xde46
 * 0xe7L	0000de2e	89 43 0c 	movl	%eax, 0xc(%ebx)
 * 0xeaL	0000de31	84 d2 	testb	%dl, %dl
 * target: 0000de27	87 5e 28 	xchgl	%ebx, 0x28(%esi)
 * verify: 0000de58	e8 83 68 ff ff 	calll	0x46e0
 */
void shell_enq__dispatch_mach_disconnect()
{
	save_registers
	uint64_t dq, invoke_ptr;
	struct dispatch_patch_item_head *item;
	asm volatile("movl %%eax, %0\n"
		"movl %%esi, %1\n"
		"movl %%eax, %%ebx\n"
		"xchgl %%ebx, 0x28(%%esi)\n"
		"movl %%ebx, -0x4(%%ebp)\n"
		:"=m"(item), "=m"(dq):);
	if (DISPATCH_OBJ_IS_VTABLE(item))
		invoke_ptr = (uint64_t)(item->do_vtable->do_invoke);
	else
		invoke_ptr = 0;
	kdebug_trace(DISPATCH_ENQUEUE, 0x26ULL,
		dq, (uint64_t)item,
		(uint64_t)item->dc_ctxt, 0);
	kdebug_trace(DISPATCH_ENQUEUE, 1ULL << 32 | 0x26,
		(uint64_t)item->dc_func, invoke_ptr,
		(uint64_t)item->do_vtable, 0);
	restore_registers
	/*simulation
	asm volatile("movl	%%eax, %%ebx\n"
		"xchgl	%%ebx, 0x28(%%esi)"
		::);
	*/
}
#endif
void detour__dispatch_mach_disconnect(struct mach_o_handler *handler_ptr)
{
#if __x86_64__
	detour_function(handler_ptr, "_dispatch_mach_disconnect", 
		shell_enq__dispatch_mach_disconnect, 0xe0, 7);
#elif __i386__
	detour_function(handler_ptr, "_dispatch_mach_disconnect", 
		shell_enq__dispatch_mach_disconnect, 0xde, 5);
#endif
}
/************/
#if __x86_64__
/* 0x20
 * _dispatch_group_async:
 * 0x0L	000000000000f1c7	55              	pushq	%rbp
 * 0x200L	000000000000f3c7	31c9            	xorl	%ecx, %ecx
 * 0x202L	000000000000f3c9	48c7401000000000	movq	$0x0, 0x10(%rax)
 * 0x20aL	000000000000f3d1	4889c2          	movq	%rax, %rdx
 * 0x20dL	000000000000f3d4	49875640        	xchgq	%rdx, 0x40(%r14)
 * 0x211L	000000000000f3d8	4885d2          	testq	%rdx, %rdx
 * 0x214L	000000000000f3db	7421            	je	0xf3fe
 * 0x216L	000000000000f3dd	48894210        	movq	%rax, 0x10(%rdx)
 * 0x21aL	000000000000f3e1	84c9            	testb	%cl, %cl
 * target: 000000000000f3d4	49875640        	xchgq	%rdx, 0x40(%r14)
 * verify: 000000000000f418	e9a45dffff      	jmp	0x51c1
 */
void shell_enq_dispatch_group_async()
{
	save_registers
	uint64_t dq, invoke_ptr;
	struct dispatch_patch_item_head *item;
	asm volatile("movq %%rax, %0\n"
		"movq %%r14, %1"
		:"=m"(item), "=m"(dq):);
	if (DISPATCH_OBJ_IS_VTABLE(item))
		invoke_ptr = (uint64_t)(item->do_vtable->do_invoke);
	else
		invoke_ptr = 0;
	kdebug_trace(DISPATCH_ENQUEUE, 0x20ULL,
		dq, (uint64_t)item,
		(uint64_t)item->dc_ctxt, 0);
	kdebug_trace(DISPATCH_ENQUEUE, 1ULL << 32 | 0x20,
		(uint64_t)item->dc_func, invoke_ptr,
		(uint64_t)item->do_vtable, 0);
	restore_registers
	//simulation
	asm volatile("movq	%%rax, %%rdx\n"
		"xchgq	%%rdx, 0x40(%%r14)"
		::);
}
#endif
#if __i386__
/* 0x27
 * _dispatch_group_async:
 * 0x0L	0000eff4	55 	pushl	%ebp
 * 0x1f5L	0000f1e9	31 db 	xorl	%ebx, %ebx
 * 0x1f7L	0000f1eb	c7 40 0c 00 00 00 00 	movl	$0x0, 0xc(%eax)
 * 0x1feL	0000f1f2	89 c2 	movl	%eax, %edx
 * 0x200L	0000f1f4	87 51 28 	xchgl	%edx, 0x28(%ecx)
 * 0x203L	0000f1f7	85 d2 	testl	%edx, %edx
 * 0x205L	0000f1f9	74 15 	je	0xf210
 * 0x207L	0000f1fb	89 42 0c 	movl	%eax, 0xc(%edx)
 * 0x20aL	0000f1fe	84 db 	testb	%bl, %bl
 * target: 0000f1f4	87 51 28 	xchgl	%edx, 0x28(%ecx)
 * verify: 0000f222	e8 b9 54 ff ff 	calll	0x46e0
 */
void shell_enq_dispatch_group_async()
{
	save_registers
	uint64_t dq, invoke_ptr;
	struct dispatch_patch_item_head *item;
	asm volatile("movl %%eax, %0\n"
		"movl %%ecx, %1"
		:"=m"(item), "=m"(dq):);
	if (DISPATCH_OBJ_IS_VTABLE(item))
		invoke_ptr = (uint64_t)(item->do_vtable->do_invoke);
	else
		invoke_ptr = 0;
	kdebug_trace(DISPATCH_ENQUEUE, 0x27ULL,
		dq, (uint64_t)item,
		(uint64_t)item->dc_ctxt, 0);
	kdebug_trace(DISPATCH_ENQUEUE, 1ULL << 32 | 0x27,
		(uint64_t)item->dc_func, invoke_ptr,
		(uint64_t)item->do_vtable, 0);
	restore_registers
	//simulation
	asm volatile("movl	%%eax, %%edx\n"
		"xchgl	%%edx, 0x28(%%ecx)"
		::);
}
#endif
void detour_dispatch_group_async(struct mach_o_handler *handler_ptr)
{
#if __x86_64__
	detour_function(handler_ptr, "dispatch_group_async", 
		shell_enq_dispatch_group_async, 0x20a, 7);
#elif __i386__
	detour_function(handler_ptr, "dispatch_group_async", 
		shell_enq_dispatch_group_async, 0x1fe, 5);
#endif
}
/************/
#if __x86_64__
/* 0x23
 * __dispatch_barrier_sync_f_slow:
 * 0x0L	0000000000010742	55              	pushq	%rbp
 * 0x208L	000000000001094a	48c78520ffffff00000000	movq	$0x0, -0xe0(%rbp)
 * 0x213L	0000000000010955	488d8d10ffffff  	leaq	-0xf0(%rbp), %rcx
 * 0x21aL	000000000001095c	4889ca          	movq	%rcx, %rdx
 * 0x21dL	000000000001095f	48875340        	xchgq	%rdx, 0x40(%rbx)
 * 0x221L	0000000000010963	4885d2          	testq	%rdx, %rdx
 * 0x224L	0000000000010966	7412            	je	0x1097a
 * 0x226L	0000000000010968	48894a10        	movq	%rcx, 0x10(%rdx)
 * 0x22aL	000000000001096c	84c0            	testb	%al, %al
 * target: 000000000001095f	48875340        	xchgq	%rdx, 0x40(%rbx)
 * verify: 0000000000010987	e83548ffff      	callq	0x51c1
 */
void shell_enq__dispatch_barrier_sync_f_slow()
{
	save_registers
	uint64_t dq, invoke_ptr;
	struct dispatch_patch_item_head *item;
	asm volatile("movq %%rcx, %0\n"
		"movq %%rbx, %1"
		:"=m"(item), "=m"(dq):);
	if (DISPATCH_OBJ_IS_VTABLE(item))
		invoke_ptr = (uint64_t)(item->do_vtable->do_invoke);
	else
		invoke_ptr = 0;
	kdebug_trace(DISPATCH_ENQUEUE, 0x23ULL,
		dq, (uint64_t)item,
		(uint64_t)item->dc_ctxt, 0);
	kdebug_trace(DISPATCH_ENQUEUE, 1ULL << 32 | 0x23,
		(uint64_t)item->dc_func, invoke_ptr,
		(uint64_t)item->do_vtable, 0);
	restore_registers
	//simulation
	asm volatile("movq	%%rcx, %%rdx\n"
		"xchgq	%%rdx, 0x40(%%rbx)"
		::);
}
#endif
#if __i386__
/* 0x2a
 * __dispatch_barrier_sync_f_slow:
 * 0x0L	000105d2	55 	pushl	%ebp
 * 0x183L	00010755	c7 45 9c 00 00 00 00 	movl	$0x0, -0x64(%ebp)
 * 0x18aL	0001075c	8d 45 90 	leal	-0x70(%ebp), %eax
 * 0x18dL	0001075f	89 c2 	movl	%eax, %edx
 * 0x18fL	00010761	87 56 28 	xchgl	%edx, 0x28(%esi)
 * 0x192L	00010764	85 d2 	testl	%edx, %edx
 * 0x194L	00010766	74 15 	je	0x1077d
 * 0x196L	00010768	89 42 0c 	movl	%eax, 0xc(%edx)
 * 0x199L	0001076b	84 c9 	testb	%cl, %cl
 * target: 00010761	87 56 28 	xchgl	%edx, 0x28(%esi)
 * verify: 0001078f	e8 4c 3f ff ff 	calll	0x46e0
 */
void shell_enq__dispatch_barrier_sync_f_slow()
{
	save_registers
	uint64_t dq, invoke_ptr;
	struct dispatch_patch_item_head *item;
	asm volatile("movl %%eax, %0\n"
		"movl %%esi, %1"
		:"=m"(item), "=m"(dq):);
	if (DISPATCH_OBJ_IS_VTABLE(item))
		invoke_ptr = (uint64_t)(item->do_vtable->do_invoke);
	else
		invoke_ptr = 0;
	kdebug_trace(DISPATCH_ENQUEUE, 0x2aULL,
		dq, (uint64_t)item,
		(uint64_t)item->dc_ctxt, 0);
	kdebug_trace(DISPATCH_ENQUEUE, 1ULL << 32 | 0x2a,
		(uint64_t)item->dc_func, invoke_ptr,
		(uint64_t)item->do_vtable, 0);
	restore_registers
	//simulation
	asm volatile("movl	%%eax, %%edx\n"
		"xchgl	%%edx, 0x28(%%esi)"
		::);
}
#endif
void detour__dispatch_barrier_sync_f_slow(struct mach_o_handler *handler_ptr)
{
#if __x86_64__
	detour_function(handler_ptr, "_dispatch_barrier_sync_f_slow", 
		shell_enq__dispatch_barrier_sync_f_slow, 0x21a, 7);
#elif __i386__
	detour_function(handler_ptr, "_dispatch_barrier_sync_f_slow", 
		shell_enq__dispatch_barrier_sync_f_slow, 0x18d, 5);
#endif
}
/************/
#if __x86_64__
/* 0x25
 * _dispatch_group_async_f:
 * 0x0L	000000000001393c	55              	pushq	%rbp
 * 0x17fL	0000000000013abb	31c9            	xorl	%ecx, %ecx
 * 0x181L	0000000000013abd	48c7401000000000	movq	$0x0, 0x10(%rax)
 * 0x189L	0000000000013ac5	4889c6          	movq	%rax, %rsi
 * 0x18cL	0000000000013ac8	49877640        	xchgq	%rsi, 0x40(%r14)
 * 0x190L	0000000000013acc	4885f6          	testq	%rsi, %rsi
 * 0x193L	0000000000013acf	741b            	je	0x13aec
 * 0x195L	0000000000013ad1	48894610        	movq	%rax, 0x10(%rsi)
 * 0x199L	0000000000013ad5	84c9            	testb	%cl, %cl
 * target: 0000000000013ac8	49877640        	xchgq	%rsi, 0x40(%r14)
 * verify: 0000000000013b00	e9bc16ffff      	jmp	0x51c1
 */
void shell_enq_dispatch_group_async_f()
{
	save_registers
	uint64_t dq, invoke_ptr;
	struct dispatch_patch_item_head *item;
	asm volatile("movq %%rax, %0\n"
		"movq %%r14, %1"
		:"=m"(item), "=m"(dq):);
	if (DISPATCH_OBJ_IS_VTABLE(item))
		invoke_ptr = (uint64_t)(item->do_vtable->do_invoke);
	else
		invoke_ptr = 0;
	kdebug_trace(DISPATCH_ENQUEUE, 0x25ULL,
		dq, (uint64_t)item,
		(uint64_t)item->dc_ctxt, 0);
	kdebug_trace(DISPATCH_ENQUEUE, 1ULL << 32 | 0x25,
		(uint64_t)item->dc_func, invoke_ptr,
		(uint64_t)item->do_vtable, 0);
	restore_registers
	//simulation
	asm volatile("movq	%%rax, %%rsi\n"
		"xchgq	%%rsi, 0x40(%%r14)"
		::);
}
#endif
#if __i386__
/* 0x2c
 * _dispatch_group_async_f:
 * 0x0L	00013ada	55 	pushl	%ebp
 * 0x156L	00013c30	31 db 	xorl	%ebx, %ebx
 * 0x158L	00013c32	c7 40 0c 00 00 00 00 	movl	$0x0, 0xc(%eax)
 * 0x15fL	00013c39	89 c2 	movl	%eax, %edx
 * 0x161L	00013c3b	87 51 28 	xchgl	%edx, 0x28(%ecx)
 * 0x164L	00013c3e	85 d2 	testl	%edx, %edx
 * 0x166L	00013c40	74 15 	je	0x13c57
 * 0x168L	00013c42	89 42 0c 	movl	%eax, 0xc(%edx)
 * 0x16bL	00013c45	84 db 	testb	%bl, %bl
 * target: 00013c3b	87 51 28 	xchgl	%edx, 0x28(%ecx)
 * verify: 00013c69	e8 72 0a ff ff 	calll	0x46e0
 */
void shell_enq_dispatch_group_async_f()
{
	save_registers
	uint64_t dq, invoke_ptr;
	struct dispatch_patch_item_head *item;
	asm volatile("movl %%eax, %0\n"
		"movl %%ecx, %1"
		:"=m"(item), "=m"(dq):);
	if (DISPATCH_OBJ_IS_VTABLE(item))
		invoke_ptr = (uint64_t)(item->do_vtable->do_invoke);
	else
		invoke_ptr = 0;
	kdebug_trace(DISPATCH_ENQUEUE, 0x2cULL,
		dq, (uint64_t)item,
		(uint64_t)item->dc_ctxt, 0);
	kdebug_trace(DISPATCH_ENQUEUE, 1ULL << 32 | 0x2c,
		(uint64_t)item->dc_func, invoke_ptr,
		(uint64_t)item->do_vtable, 0);
	restore_registers
	//simulation
	asm volatile("movl	%%eax, %%edx\n"
		"xchgl	%%edx, 0x28(%%ecx)"
		::);
}
#endif
void detour_dispatch_group_async_f(struct mach_o_handler *handler_ptr)
{
#if __x86_64__
	detour_function(handler_ptr, "dispatch_group_async_f", 
		shell_enq_dispatch_group_async_f, 0x189, 7);
#elif __i386__
	detour_function(handler_ptr, "dispatch_group_async_f", 
		shell_enq_dispatch_group_async_f, 0x15f, 5);
#endif
}
/************/
//TODO: Not verified
#if __x86_64__
/* 0x26
 * _dispatch_apply_f:
 * 0x0L	0000000000013b2e	55              	pushq	%rbp
 * 0x2acL	0000000000013dda	31c9            	xorl	%ecx, %ecx
 * 0x2aeL	0000000000013ddc	4989fe          	movq	%rdi, %r14
 * 0x2b1L	0000000000013ddf	49c7471000000000	movq	$0x0, 0x10(%r15)
 * 0x2b9L	0000000000013de7	4d877d40        	xchgq	%r15, 0x40(%r13)
 * 0x2bdL	0000000000013deb	4d85ff          	testq	%r15, %r15
 * 0x2c0L	0000000000013dee	7412            	je	0x13e02
 * 0x2c2L	0000000000013df0	49895f10        	movq	%rbx, 0x10(%r15)
 * 0x2c6L	0000000000013df4	84c9            	testb	%cl, %cl
 * target: 0000000000013de7	4d877d40        	xchgq	%r15, 0x40(%r13)
 */
void shell_enq_dispatch_apply_f()
{
	save_registers
	uint64_t dq, invoke_ptr;
	struct dispatch_patch_item_head *item;
	asm volatile("movq %%r15, %0\n"
		"movq %%r13, %1"
		:"=m"(item), "=m"(dq):);
	if (DISPATCH_OBJ_IS_VTABLE(item))
		invoke_ptr = (uint64_t)(item->do_vtable->do_invoke);
	else
		invoke_ptr = 0;
	kdebug_trace(DISPATCH_ENQUEUE, 0x26ULL,
		dq, (uint64_t)item,
		(uint64_t)item->dc_ctxt, 0);
	kdebug_trace(DISPATCH_ENQUEUE, 1ULL << 32 | 0x26,
		(uint64_t)item->dc_func, invoke_ptr,
		(uint64_t)item->do_vtable, 0);
	restore_registers
	//simulation
	asm volatile("movq	$0x0, 0x10(%%r15)"
		::);
}
#endif
//TODO: Not verified
#if __i386__
/* 0x2d
 * _dispatch_apply_f:
 * 0x0L	00013cc0	55 	pushl	%ebp
 * 0x2b3L	00013f73	31 c9 	xorl	%ecx, %ecx
 * 0x2b5L	00013f75	8b 55 c4 	movl	-0x3c(%ebp), %edx
 * 0x2b8L	00013f78	c7 42 0c 00 00 00 00 	movl	$0x0, 0xc(%edx)
 * 0x2bfL	00013f7f	87 56 28 	xchgl	%edx, 0x28(%esi)
 * 0x2c2L	00013f82	85 d2 	testl	%edx, %edx
 * 0x2c4L	00013f84	74 15 	je	0x13f9b
 * 0x2c6L	00013f86	89 7a 0c 	movl	%edi, 0xc(%edx)
 * 0x2c9L	00013f89	84 c9 	testb	%cl, %cl
 * target: 00013f7f	87 56 28 	xchgl	%edx, 0x28(%esi)
 */
void shell_enq_dispatch_apply_f()
{
	save_registers
	uint64_t dq, invoke_ptr;
	struct dispatch_patch_item_head *item;
	asm volatile("movl %%edx, %0\n"
		"movl %%esi, %1"
		:"=m"(item), "=m"(dq):);
	if (DISPATCH_OBJ_IS_VTABLE(item))
		invoke_ptr = (uint64_t)(item->do_vtable->do_invoke);
	else
		invoke_ptr = 0;
	kdebug_trace(DISPATCH_ENQUEUE, 0x2dULL,
		dq, (uint64_t)item,
		(uint64_t)item->dc_ctxt, 0);
	kdebug_trace(DISPATCH_ENQUEUE, 1ULL << 32 | 0x2d,
		(uint64_t)item->dc_func, invoke_ptr,
		(uint64_t)item->do_vtable, 0);
	restore_registers
	//simulation
	asm volatile("movl	$0x0, 0xc(%%edx)\n"
		::);
}
#endif
void detour_dispatch_apply_f(struct mach_o_handler *handler_ptr)
{
#if __x86_64__
	detour_function(handler_ptr, "dispatch_apply_f", 
		shell_enq_dispatch_apply_f, 0x2b1, 8);
#elif __i386__
	detour_function(handler_ptr, "dispatch_apply_f", 
		shell_enq_dispatch_apply_f, 0x2b8, 7);
#endif
}
/************/
#if __x86_64__
/* 0x27
 * __dispatch_mach_msg_not_sent:
 * 0x0L	00000000000140ba	55              	pushq	%rbp
 * 0x139L	00000000000141f3	31c0            	xorl	%eax, %eax
 * 0x13bL	00000000000141f5	48c7431000000000	movq	$0x0, 0x10(%rbx)
 * 0x143L	00000000000141fd	4889d9          	movq	%rbx, %rcx
 * 0x146L	0000000000014200	49874e40        	xchgq	%rcx, 0x40(%r14)
 * 0x14aL	0000000000014204	4885c9          	testq	%rcx, %rcx
 * 0x14dL	0000000000014207	7412            	je	0x1421b
 * 0x14fL	0000000000014209	48895910        	movq	%rbx, 0x10(%rcx)
 * 0x153L	000000000001420d	84c0            	testb	%al, %al
 * target: 0000000000014200	49874e40        	xchgq	%rcx, 0x40(%r14)
 * verify: 0000000000014224	e8980fffff      	callq	0x51c1
 */
void shell_enq__dispatch_mach_msg_not_sent_0()
{
	save_registers
	uint64_t dq, invoke_ptr;
	struct dispatch_patch_item_head *item;
	asm volatile("movq %%rbx, %0\n"
		"movq %%r14, %1"
		:"=m"(item), "=m"(dq):);
	if (DISPATCH_OBJ_IS_VTABLE(item))
		invoke_ptr = (uint64_t)(item->do_vtable->do_invoke);
	else
		invoke_ptr = 0;
	kdebug_trace(DISPATCH_ENQUEUE, 0x27ULL,
		dq, (uint64_t)item,
		(uint64_t)item->dc_ctxt, 0);
	kdebug_trace(DISPATCH_ENQUEUE, 1ULL << 32 | 0x27,
		(uint64_t)item->dc_func, invoke_ptr,
		(uint64_t)item->do_vtable, 0);
	restore_registers
	//simulation
	asm volatile("movq	%%rbx, %%rcx\n"
		"xchgq	%%rcx, 0x40(%%r14)"
		::);
}
/* 0x28
 * __dispatch_mach_msg_not_sent:
 * 0x0L	00000000000140ba	55              	pushq	%rbp
 * 0x1ceL	0000000000014288	498d5640        	leaq	0x40(%r14), %rdx
 * 0x1d2L	000000000001428c	49c7471000000000	movq	$0x0, 0x10(%r15)
 * 0x1daL	0000000000014294	4c89f9          	movq	%r15, %rcx
 * 0x1ddL	0000000000014297	48870a          	xchgq	%rcx, (%rdx)
 * 0x1e0L	000000000001429a	4885c9          	testq	%rcx, %rcx
 * 0x1e3L	000000000001429d	7425            	je	0x142c4
 * 0x1e5L	000000000001429f	4c897910        	movq	%r15, 0x10(%rcx)
 * 0x1e9L	00000000000142a3	84c0            	testb	%al, %al
 * target: 0000000000014297	48870a          	xchgq	%rcx, (%rdx)
 * verify: 00000000000142d7	e9e50effff      	jmp	0x51c1
 */
void shell_enq__dispatch_mach_msg_not_sent_1()
{
	save_registers
	uint64_t dq, invoke_ptr;
	struct dispatch_patch_item_head *item;
	asm volatile("movq %%r15, %0\n"
		"leaq -0x40(%%rdx), %%rax\n"
		"movq %%rax, %1"
		:"=m"(item), "=m"(dq):);
	if (DISPATCH_OBJ_IS_VTABLE(item))
		invoke_ptr = (uint64_t)(item->do_vtable->do_invoke);
	else
		invoke_ptr = 0;
	kdebug_trace(DISPATCH_ENQUEUE, 0x28ULL,
		dq, (uint64_t)item,
		(uint64_t)item->dc_ctxt, 0);
	kdebug_trace(DISPATCH_ENQUEUE, 1ULL << 32 | 0x28,
		(uint64_t)item->dc_func, invoke_ptr,
		(uint64_t)item->do_vtable, 0);
	restore_registers
	//simulation
	asm volatile("movq	%%r15, %%rcx\n"
		"xchgq	%%rcx, (%%rdx)"
		::);
}
#endif
#if __i386__
/* 0x2e
 * __dispatch_mach_msg_not_sent:
 * 0x0L	0001428e	55 	pushl	%ebp
 * 0x13dL	000143cb	31 c9 	xorl	%ecx, %ecx
 * 0x13fL	000143cd	c7 43 0c 00 00 00 00 	movl	$0x0, 0xc(%ebx)
 * 0x146L	000143d4	89 da 	movl	%ebx, %edx
 * 0x148L	000143d6	87 56 28 	xchgl	%edx, 0x28(%esi)
 * 0x14bL	000143d9	85 d2 	testl	%edx, %edx
 * 0x14dL	000143db	74 15 	je	0x143f2
 * 0x14fL	000143dd	89 5a 0c 	movl	%ebx, 0xc(%edx)
 * 0x152L	000143e0	84 c9 	testb	%cl, %cl
 * target: 000143d6	87 56 28 	xchgl	%edx, 0x28(%esi)
 * verify: 00014404	e8 d7 02 ff ff 	calll	0x46e0
 */
void shell_enq__dispatch_mach_msg_not_sent_0()
{
	save_registers
	uint64_t dq, invoke_ptr;
	struct dispatch_patch_item_head *item;
	asm volatile("movl %%ebx, %0\n"
		"movl %%esi, %1"
		:"=m"(item), "=m"(dq):);
	if (DISPATCH_OBJ_IS_VTABLE(item))
		invoke_ptr = (uint64_t)(item->do_vtable->do_invoke);
	else
		invoke_ptr = 0;
	kdebug_trace(DISPATCH_ENQUEUE, 0x2eULL,
		dq, (uint64_t)item,
		(uint64_t)item->dc_ctxt, 0);
	kdebug_trace(DISPATCH_ENQUEUE, 1ULL << 32 | 0x2e,
		(uint64_t)item->dc_func, invoke_ptr,
		(uint64_t)item->do_vtable, 0);
	restore_registers
	//simulation
	asm volatile("movl	%%ebx, %%edx\n"
		"xchgl	%%edx, 0x28(%%esi)"
		::);
}
/* 0x2f
 * __dispatch_mach_msg_not_sent:
 * 0x0L	0001428e	55 	pushl	%ebp
 * 0x1deL	0001446c	8d 5e 28 	leal	0x28(%esi), %ebx
 * 0x1e1L	0001446f	c7 47 0c 00 00 00 00 	movl	$0x0, 0xc(%edi)
 * 0x1e8L	00014476	89 fa 	movl	%edi, %edx
 * 0x1eaL	00014478	87 13 	xchgl	%edx, (%ebx)
 * 0x1ecL	0001447a	85 d2 	testl	%edx, %edx
 * 0x1eeL	0001447c	74 15 	je	0x14493
 * 0x1f0L	0001447e	89 7a 0c 	movl	%edi, 0xc(%edx)
 * 0x1f3L	00014481	84 c9 	testb	%cl, %cl
 * target: 00014478	87 13 	xchgl	%edx, (%ebx)
 * verify: 000144a5	e8 36 02 ff ff 	calll	0x46e0
 */
void shell_enq__dispatch_mach_msg_not_sent_1()
{
	save_registers
	uint64_t dq, invoke_ptr;
	struct dispatch_patch_item_head *item;
	asm volatile("movl %%edi, %0\n"
		"subl $0x28, %%ebx\n"
		"movl %%ebx, %1"
		:"=m"(item), "=m"(dq):);
	if (DISPATCH_OBJ_IS_VTABLE(item))
		invoke_ptr = (uint64_t)(item->do_vtable->do_invoke);
	else
		invoke_ptr = 0;
	kdebug_trace(DISPATCH_ENQUEUE, 0x2fULL,
		dq, (uint64_t)item,
		(uint64_t)item->dc_ctxt, 0);
	kdebug_trace(DISPATCH_ENQUEUE, 1ULL << 32 | 0x2f,
		(uint64_t)item->dc_func, invoke_ptr,
		(uint64_t)item->do_vtable, 0);
	restore_registers
	//simulation
	asm volatile("movl $0x0, 0xc(%%edi)"
		::);
}
#endif
void detour__dispatch_mach_msg_not_sent(struct mach_o_handler *handler_ptr)
{
#if __x86_64__
	detour_function(handler_ptr, "_dispatch_mach_msg_not_sent", 
		shell_enq__dispatch_mach_msg_not_sent_0, 0x143, 7);
	detour_function(handler_ptr, "_dispatch_mach_msg_not_sent", 
		shell_enq__dispatch_mach_msg_not_sent_1, 0x1da, 6);
#elif __i386__
	detour_function(handler_ptr, "_dispatch_mach_msg_not_sent", 
		shell_enq__dispatch_mach_msg_not_sent_0, 0x146, 5);
	detour_function(handler_ptr, "_dispatch_mach_msg_not_sent", 
		shell_enq__dispatch_mach_msg_not_sent_1, 0x1e1, 7);
#endif
}
/************/
#if __x86_64__
/* 0x2a
 * __dispatch_main_queue_callback_4CF:
 * 0x0L	0000000000015587	55              	pushq	%rbp
 * 0x4dL	00000000000155d4	48c745a000000000	movq	$0x0, -0x60(%rbp)
 * 0x55L	00000000000155dc	488d5d90        	leaq	-0x70(%rbp), %rbx
 * 0x59L	00000000000155e0	4889d8          	movq	%rbx, %rax
 * 0x5cL	00000000000155e3	49874740        	xchgq	%rax, 0x40(%r15)
 * 0x60L	00000000000155e7	4885c0          	testq	%rax, %rax
 * 0x63L	00000000000155ea	7406            	je	0x155f2
 * 0x65L	00000000000155ec	48895810        	movq	%rbx, 0x10(%rax)
 * 0x69L	00000000000155f0	eb14            	jmp	0x15606
 * target: 00000000000155e3	49874740        	xchgq	%rax, 0x40(%r15)
 * verify: 0000000000015601	e8bbfbfeff      	callq	0x51c1
 */
void shell_enq__dispatch_main_queue_callback_4CF()
{
	save_registers
	uint64_t dq, invoke_ptr;
	struct dispatch_patch_item_head *item;
	asm volatile("movq %%rbx, %0\n"
		"movq %%r15, %1"
		:"=m"(item), "=m"(dq):);
	if (DISPATCH_OBJ_IS_VTABLE(item))
		invoke_ptr = (uint64_t)(item->do_vtable->do_invoke);
	else
		invoke_ptr = 0;
	kdebug_trace(DISPATCH_ENQUEUE, 0x2aULL,
		dq, (uint64_t)item,
		(uint64_t)item->dc_ctxt, 0);
	kdebug_trace(DISPATCH_ENQUEUE, 1ULL << 32 | 0x2a,
		(uint64_t)item->dc_func, invoke_ptr,
		(uint64_t)item->do_vtable, 0);
	restore_registers
	//simulation
	asm volatile("movq	%%rbx, %%rax\n"
		"xchgq	%%rax, 0x40(%%r15)"
		::);
}
#endif
#if __i386__
/* 0x31
 * __dispatch_main_queue_callback_4CF:
 * 0x0L	00015911	55 	pushl	%ebp
 * 0x44L	00015955	c7 45 d4 00 00 00 00 	movl	$0x0, -0x2c(%ebp)
 * 0x4bL	0001595c	8d 4d c8 	leal	-0x38(%ebp), %ecx
 * 0x4eL	0001595f	89 c8 	movl	%ecx, %eax
 * 0x50L	00015961	87 43 28 	xchgl	%eax, 0x28(%ebx)
 * 0x53L	00015964	85 c0 	testl	%eax, %eax
 * 0x55L	00015966	74 05 	je	0x1596d
 * 0x57L	00015968	89 48 0c 	movl	%ecx, 0xc(%eax)
 * 0x5aL	0001596b	eb 1c 	jmp	0x15989
 * target: 00015961	87 43 28 	xchgl	%eax, 0x28(%ebx)
 * verify: 00015984	e8 57 ed fe ff 	calll	0x46e0
 */
void shell_enq__dispatch_main_queue_callback_4CF_0()
{
	save_registers
	uint64_t dq, invoke_ptr;
	struct dispatch_patch_item_head *item;
	asm volatile("movl %%ecx, %0\n"
		"movl %%ebx, %1"
		:"=m"(item), "=m"(dq):);
	if (DISPATCH_OBJ_IS_VTABLE(item))
		invoke_ptr = (uint64_t)(item->do_vtable->do_invoke);
	else
		invoke_ptr = 0;
	kdebug_trace(DISPATCH_ENQUEUE, 0x31ULL,
		dq, (uint64_t)item,
		(uint64_t)item->dc_ctxt, 0);
	kdebug_trace(DISPATCH_ENQUEUE, 1ULL << 32 | 0x31,
		(uint64_t)item->dc_func, invoke_ptr,
		(uint64_t)item->do_vtable, 0);
	restore_registers
	//simulation
	asm volatile("movl	%%ecx, %%eax\n"
		"xchgl	%%eax, 0x28(%%ebx)"
		::);
}
#endif
void detour__dispatch_main_queue_callback_4CF(struct mach_o_handler *handler_ptr)
{
#if __x86_64__
	detour_function(handler_ptr, "_dispatch_main_queue_callback_4CF", 
		shell_enq__dispatch_main_queue_callback_4CF, 0x59, 7);
#elif __i386__
	detour_function(handler_ptr, "_dispatch_main_queue_callback_4CF", 
		shell_enq__dispatch_main_queue_callback_4CF_0, 0x4e, 5);
#endif
}
/************/
#if __x86_64__
/* 0x2b
 * __dispatch_async_f2:
 * 0x0L	00000000000163b0	4989f0          	movq	%rsi, %r8
 * 0x53L	0000000000016403	83c1fe          	addl	$-0x2, %ecx
 * 0x56L	0000000000016406	49c7401000000000	movq	$0x0, 0x10(%r8)
 * 0x5eL	000000000001640e	4c89c0          	movq	%r8, %rax
 * 0x61L	0000000000016411	498701          	xchgq	%rax, (%r9)
 * 0x64L	0000000000016414	4885c0          	testq	%rax, %rax
 * 0x67L	0000000000016417	7431            	je	0x1644a
 * 0x69L	0000000000016419	4c894010        	movq	%r8, 0x10(%rax)
 * 0x6dL	000000000001641d	4885d2          	testq	%rdx, %rdx
 * target: 0000000000016411	498701          	xchgq	%rax, (%r9)
 * verify: 0000000000016452	e96aedfeff      	jmp	0x51c1
 */
void shell_enq__dispatch_async_f2()
{
	save_registers
	uint64_t dq, invoke_ptr;
	struct dispatch_patch_item_head *item;
	asm volatile("movq %%r8, %0\n"
		"leaq -0x40(%%r9), %%rax\n"
		"movq %%rax, %1"
		:"=m"(item), "=m"(dq):);
	if (DISPATCH_OBJ_IS_VTABLE(item))
		invoke_ptr = (uint64_t)(item->do_vtable->do_invoke);
	else
		invoke_ptr = 0;
	kdebug_trace(DISPATCH_ENQUEUE, 0x2bULL,
		dq, (uint64_t)item,
		(uint64_t)item->dc_ctxt, 0);
	kdebug_trace(DISPATCH_ENQUEUE, 1ULL << 32 | 0x2b,
		(uint64_t)item->dc_func, invoke_ptr,
		(uint64_t)item->do_vtable, 0);
	restore_registers
	//simulation
	asm volatile("movq	%%r8, %%rax\n"
		"xchgq	%%rax, (%%r9)"
		::);
}
#endif
#if __i386__
/* 0x33
 * __dispatch_async_f2:
 * 0x0L	00016866	55 	pushl	%ebp
 * 0x69L	000168cf	83 c3 fe 	addl	$-0x2, %ebx
 * 0x6cL	000168d2	c7 42 0c 00 00 00 00 	movl	$0x0, 0xc(%edx)
 * 0x73L	000168d9	89 d7 	movl	%edx, %edi
 * 0x75L	000168db	87 3e 	xchgl	%edi, (%esi)
 * 0x77L	000168dd	85 ff 	testl	%edi, %edi
 * 0x79L	000168df	74 3f 	je	0x16920
 * 0x7bL	000168e1	89 57 0c 	movl	%edx, 0xc(%edi)
 * 0x7eL	000168e4	8b 75 08 	movl	0x8(%ebp), %esi
 * target: 000168db	87 3e 	xchgl	%edi, (%esi)
 * verify: 00016936	e8 a5 dd fe ff 	calll	0x46e0
 */
void shell_enq__dispatch_async_f2()
{
	save_registers
	uint64_t dq, invoke_ptr;
	struct dispatch_patch_item_head *item;
	asm volatile("movl %%edx, %%edi\n"
		"xchgl %%edi, (%%esi)\n"
		"movl %%edi, -0x8(%%ebp)\n"
		"movl %%edx, %0\n"
		"subl $0x28, %%esi\n"
		"movl %%esi, %1"
		:"=m"(item), "=m"(dq):);
	if (DISPATCH_OBJ_IS_VTABLE(item))
		invoke_ptr = (uint64_t)(item->do_vtable->do_invoke);
	else
		invoke_ptr = 0;
	kdebug_trace(DISPATCH_ENQUEUE, 0x33ULL,
		dq, (uint64_t)item,
		(uint64_t)item->dc_ctxt, 0);
	kdebug_trace(DISPATCH_ENQUEUE, 1ULL << 32 | 0x33,
		(uint64_t)item->dc_func, invoke_ptr,
		(uint64_t)item->do_vtable, 0);
	restore_registers
	/*simulation
	asm volatile("movl	%%edx, %%edi\n"
		"xchgl	%%edi, (%%esi)"
		::);
	*/
}
#endif
void detour__dispatch_async_f2(struct mach_o_handler *handler_ptr)
{
#if __x86_64__
	detour_function(handler_ptr, "_dispatch_async_f2", 
		shell_enq__dispatch_async_f2, 0x5e, 6);
#elif __i386__
	detour_function(handler_ptr, "_dispatch_async_f2", 
		shell_enq__dispatch_async_f2, 0x6c, 7);
#endif
}
/************/
#if __x86_64__
/* 0x2c
 * __dispatch_async_f_redirect:
 * 0x0L	0000000000016464	55              	pushq	%rbp
 * 0x13bL	000000000001659f	31c9            	xorl	%ecx, %ecx
 * 0x13dL	00000000000165a1	48c7401000000000	movq	$0x0, 0x10(%rax)
 * 0x145L	00000000000165a9	4889c2          	movq	%rax, %rdx
 * 0x148L	00000000000165ac	48875740        	xchgq	%rdx, 0x40(%rdi)
 * 0x14cL	00000000000165b0	4885d2          	testq	%rdx, %rdx
 * 0x14fL	00000000000165b3	741a            	je	0x165cf
 * 0x151L	00000000000165b5	48894210        	movq	%rax, 0x10(%rdx)
 * 0x155L	00000000000165b9	84c9            	testb	%cl, %cl
 * target: 00000000000165ac	48875740        	xchgq	%rdx, 0x40(%rdi)
 * verify: 0000000000016659	e963ebfeff      	jmp	0x51c1
 */
void shell_enq__dispatch_async_f_redirect_0()
{
	save_registers
	uint64_t dq, invoke_ptr;
	struct dispatch_patch_item_head *item;
	asm volatile("movq %%rax, %0\n"
		"movq %%rdi, %1"
		:"=m"(item), "=m"(dq):);
	if (DISPATCH_OBJ_IS_VTABLE(item))
		invoke_ptr = (uint64_t)(item->do_vtable->do_invoke);
	else
		invoke_ptr = 0;
	kdebug_trace(DISPATCH_ENQUEUE, 0x2cULL,
		dq, (uint64_t)item,
		(uint64_t)item->dc_ctxt, 0);
	kdebug_trace(DISPATCH_ENQUEUE, 1ULL << 32 | 0x2c,
		(uint64_t)item->dc_func, invoke_ptr,
		(uint64_t)item->do_vtable, 0);
	restore_registers
	//simulation
	asm volatile("movq	%%rax, %%rdx\n"
		"xchgq	%%rdx, 0x40(%%rdi)"
		::);
}
/* 0x2d
 * __dispatch_async_f_redirect:
 * 0x0L	0000000000016464	55              	pushq	%rbp
 * 0x180L	00000000000165e4	4889fa          	movq	%rdi, %rdx
 * 0x183L	00000000000165e7	48c7401000000000	movq	$0x0, 0x10(%rax)
 * 0x18bL	00000000000165ef	4889c6          	movq	%rax, %rsi
 * 0x18eL	00000000000165f2	48877240        	xchgq	%rsi, 0x40(%rdx)
 * 0x192L	00000000000165f6	4885f6          	testq	%rsi, %rsi
 * 0x195L	00000000000165f9	7449            	je	0x16644
 * 0x197L	00000000000165fb	48894610        	movq	%rax, 0x10(%rsi)
 * 0x19bL	00000000000165ff	4d85f6          	testq	%r14, %r14
 * target: 00000000000165f2	48877240        	xchgq	%rsi, 0x40(%rdx)
 * verify: 0000000000016659	e963ebfeff      	jmp	0x51c1
 */
void shell_enq__dispatch_async_f_redirect_1()
{
	save_registers
	uint64_t dq, invoke_ptr;
	struct dispatch_patch_item_head *item;
	asm volatile("movq %%rax, %0\n"
		"movq %%rdx, %1"
		:"=m"(item), "=m"(dq):);
	if (DISPATCH_OBJ_IS_VTABLE(item))
		invoke_ptr = (uint64_t)(item->do_vtable->do_invoke);
	else
		invoke_ptr = 0;
	kdebug_trace(DISPATCH_ENQUEUE, 0x2dULL,
		dq, (uint64_t)item,
		(uint64_t)item->dc_ctxt, 0);
	kdebug_trace(DISPATCH_ENQUEUE, 1ULL << 32 | 0x2d,
		(uint64_t)item->dc_func, invoke_ptr,
		(uint64_t)item->do_vtable, 0);
	restore_registers
	//simulation
	asm volatile("movq	%%rax, %%rsi\n"
		"xchgq	%%rsi, 0x40(%%rdx)"
		::);
}
#endif
#if __i386__
/* 0x34
 * __dispatch_async_f_redirect:
 * 0x0L	0001693d	55 	pushl	%ebp
 * 0x122L	00016a5f	31 c9 	xorl	%ecx, %ecx
 * 0x124L	00016a61	c7 40 0c 00 00 00 00 	movl	$0x0, 0xc(%eax)
 * 0x12bL	00016a68	89 c6 	movl	%eax, %esi
 * 0x12dL	00016a6a	87 72 28 	xchgl	%esi, 0x28(%edx)
 * 0x130L	00016a6d	85 f6 	testl	%esi, %esi
 * 0x132L	00016a6f	0f 84 8e 00 00 00 	je	0x16b03
 * 0x138L	00016a75	89 46 0c 	movl	%eax, 0xc(%esi)
 * 0x13bL	00016a78	84 c9 	testb	%cl, %cl
 * target: 00016a6a	87 72 28 	xchgl	%esi, 0x28(%edx)
 * verify: 00016b15	e8 c6 db fe ff 	calll	0x46e0
 */
void shell_enq__dispatch_async_f_redirect_0()
{
	save_registers
	uint64_t dq, invoke_ptr;
	struct dispatch_patch_item_head *item;
	asm volatile("movl %%eax, %0\n"
		"movl %%edx, %1\n"
		"movl %%eax, %%esi\n"
		"xchgl %%esi, 0x28(%%edx)\n"
		"movl %%esi, -0xc(%%ebp)"
		:"=m"(item), "=m"(dq):);
	if (DISPATCH_OBJ_IS_VTABLE(item))
		invoke_ptr = (uint64_t)(item->do_vtable->do_invoke);
	else
		invoke_ptr = 0;
	kdebug_trace(DISPATCH_ENQUEUE, 0x34ULL,
		dq, (uint64_t)item,
		(uint64_t)item->dc_ctxt, 0);
	kdebug_trace(DISPATCH_ENQUEUE, 1ULL << 32 | 0x34,
		(uint64_t)item->dc_func, invoke_ptr,
		(uint64_t)item->do_vtable, 0);
	restore_registers
	/*simulation
	asm volatile("movl	%%eax, %%esi\n"
		"xchgl	%%esi, 0x28(%%edx)"
		::);
	*/
}
/* 0x35
 * __dispatch_async_f_redirect:
 * 0x0L	0001693d	55 	pushl	%ebp
 * 0x163L	00016aa0	89 d6 	movl	%edx, %esi
 * 0x165L	00016aa2	c7 40 0c 00 00 00 00 	movl	$0x0, 0xc(%eax)
 * 0x16cL	00016aa9	89 c1 	movl	%eax, %ecx
 * 0x16eL	00016aab	87 4e 28 	xchgl	%ecx, 0x28(%esi)
 * 0x171L	00016aae	85 c9 	testl	%ecx, %ecx
 * 0x173L	00016ab0	74 3c 	je	0x16aee
 * 0x175L	00016ab2	89 41 0c 	movl	%eax, 0xc(%ecx)
 * 0x178L	00016ab5	85 ff 	testl	%edi, %edi
 * target: 00016aab	87 4e 28 	xchgl	%ecx, 0x28(%esi)
 * verify: 00016b15	e8 c6 db fe ff 	calll	0x46e0
 */
void shell_enq__dispatch_async_f_redirect_1()
{
	save_registers
	uint64_t dq, invoke_ptr;
	struct dispatch_patch_item_head *item;
	asm volatile("movl %%eax, %0\n"
		"movl %%esi, %1"
		:"=m"(item), "=m"(dq):);
	if (DISPATCH_OBJ_IS_VTABLE(item))
		invoke_ptr = (uint64_t)(item->do_vtable->do_invoke);
	else
		invoke_ptr = 0;
	kdebug_trace(DISPATCH_ENQUEUE, 0x35ULL,
		dq, (uint64_t)item,
		(uint64_t)item->dc_ctxt, 0);
	kdebug_trace(DISPATCH_ENQUEUE, 1ULL << 32 | 0x35,
		(uint64_t)item->dc_func, invoke_ptr,
		(uint64_t)item->do_vtable, 0);
	restore_registers
	//simulation
	asm volatile("movl	%%eax, %%ecx\n"
		"xchgl	%%ecx, 0x28(%%esi)"
		::);
}
#endif
void detour__dispatch_async_f_redirect(struct mach_o_handler *handler_ptr)
{
#if __x86_64__
	detour_function(handler_ptr, "_dispatch_async_f_redirect", 
		shell_enq__dispatch_async_f_redirect_0, 0x145, 7);
	detour_function(handler_ptr, "_dispatch_async_f_redirect", 
		shell_enq__dispatch_async_f_redirect_1, 0x18b, 7);
#elif __i386__
	detour_function(handler_ptr, "_dispatch_async_f_redirect", 
		shell_enq__dispatch_async_f_redirect_0, 0x12b, 5);
	detour_function(handler_ptr, "_dispatch_async_f_redirect", 
		shell_enq__dispatch_async_f_redirect_1, 0x16c, 5);
#endif
}
/************/
#if __x86_64__
/* 0x2e
 * __dispatch_sync_f_slow:
 * 0x0L	0000000000017f35	55              	pushq	%rbp
 * 0x107L	000000000001803c	48c78560ffffff00000000	movq	$0x0, -0xa0(%rbp)
 * 0x112L	0000000000018047	488d8550ffffff  	leaq	-0xb0(%rbp), %rax
 * 0x119L	000000000001804e	4889c1          	movq	%rax, %rcx
 * 0x11cL	0000000000018051	48874b40        	xchgq	%rcx, 0x40(%rbx)
 * 0x120L	0000000000018055	4885c9          	testq	%rcx, %rcx
 * 0x123L	0000000000018058	743b            	je	0x18095
 * 0x125L	000000000001805a	48894110        	movq	%rax, 0x10(%rcx)
 * 0x129L	000000000001805e	4885f6          	testq	%rsi, %rsi
 * target: 0000000000018051	48874b40        	xchgq	%rcx, 0x40(%rbx)
 * verify: 00000000000180a1	e81bd1feff      	callq	0x51c1
 */
void shell_enq__dispatch_sync_f_slow()
{
	save_registers
	uint64_t dq, invoke_ptr;
	struct dispatch_patch_item_head *item;
	asm volatile("movq %%rax, %0\n"
		"movq %%rbx, %1"
		:"=m"(item), "=m"(dq):);
	if (DISPATCH_OBJ_IS_VTABLE(item))
		invoke_ptr = (uint64_t)(item->do_vtable->do_invoke);
	else
		invoke_ptr = 0;
	kdebug_trace(DISPATCH_ENQUEUE, 0x2eULL,
		dq, (uint64_t)item,
		(uint64_t)item->dc_ctxt, 0);
	kdebug_trace(DISPATCH_ENQUEUE, 1ULL << 32 | 0x2e,
		(uint64_t)item->dc_func, invoke_ptr,
		(uint64_t)item->do_vtable, 0);
	restore_registers
	//simulation
	asm volatile("movq	%%rax, %%rcx\n"
		"xchgq	%%rcx, 0x40(%%rbx)"
		::);
}
#endif
#if __i386__
/* 0x37
 * __dispatch_sync_f_slow:
 * 0x0L	000184b3	55 	pushl	%ebp
 * 0xc0L	00018573	c7 45 bc 00 00 00 00 	movl	$0x0, -0x44(%ebp)
 * 0xc7L	0001857a	8d 45 b0 	leal	-0x50(%ebp), %eax
 * 0xcaL	0001857d	89 c1 	movl	%eax, %ecx
 * 0xccL	0001857f	87 4e 28 	xchgl	%ecx, 0x28(%esi)
 * 0xcfL	00018582	85 c9 	testl	%ecx, %ecx
 * 0xd1L	00018584	74 3e 	je	0x185c4
 * 0xd3L	00018586	89 41 0c 	movl	%eax, 0xc(%ecx)
 * 0xd6L	00018589	85 db 	testl	%ebx, %ebx
 * target: 0001857f	87 4e 28 	xchgl	%ecx, 0x28(%esi)
 * verify: 000185d7	e8 04 c1 fe ff 	calll	0x46e0
 */
void shell_enq__dispatch_sync_f_slow()
{
	save_registers
	uint64_t dq, invoke_ptr;
	struct dispatch_patch_item_head *item;
	asm volatile("movl %%eax, %0\n"
		"movl %%esi, %1"
		:"=m"(item), "=m"(dq):);
	if (DISPATCH_OBJ_IS_VTABLE(item))
		invoke_ptr = (uint64_t)(item->do_vtable->do_invoke);
	else
		invoke_ptr = 0;
	kdebug_trace(DISPATCH_ENQUEUE, 0x37ULL,
		dq, (uint64_t)item,
		(uint64_t)item->dc_ctxt, 0);
	kdebug_trace(DISPATCH_ENQUEUE, 1ULL << 32 | 0x37,
		(uint64_t)item->dc_func, invoke_ptr,
		(uint64_t)item->do_vtable, 0);
	restore_registers
	//simulation
	asm volatile("movl	%%eax, %%ecx\n"
		"xchgl	%%ecx, 0x28(%%esi)"
		::);
}
#endif
void detour__dispatch_sync_f_slow(struct mach_o_handler *handler_ptr)
{
#if __x86_64__
	detour_function(handler_ptr, "_dispatch_sync_f_slow", 
		shell_enq__dispatch_sync_f_slow, 0x119, 7);
#elif __i386__
	detour_function(handler_ptr, "_dispatch_sync_f_slow", 
		shell_enq__dispatch_sync_f_slow, 0xca, 5);
#endif
}
/************/
//TODO: Not verified
#if __x86_64__
/* 0x2f
 * __dispatch_apply_redirect:
 * 0x0L	0000000000018293	55              	pushq	%rbp
 * 0x1f4L	0000000000018487	31c9            	xorl	%ecx, %ecx
 * 0x1f6L	0000000000018489	4989ff          	movq	%rdi, %r15
 * 0x1f9L	000000000001848c	49c7461000000000	movq	$0x0, 0x10(%r14)
 * 0x201L	0000000000018494	4c877340        	xchgq	%r14, 0x40(%rbx)
 * 0x205L	0000000000018498	4d85f6          	testq	%r14, %r14
 * 0x208L	000000000001849b	7416            	je	0x184b3
 * 0x20aL	000000000001849d	4d896610        	movq	%r12, 0x10(%r14)
 * 0x20eL	00000000000184a1	84c9            	testb	%cl, %cl
 * target: 0000000000018494	4c877340        	xchgq	%r14, 0x40(%rbx)
 */
void shell_enq__dispatch_apply_redirect()
{
	save_registers
	uint64_t dq, invoke_ptr;
	struct dispatch_patch_item_head *item;
	asm volatile("movq %%r14, %0\n"
		"movq %%rbx, %1"
		:"=m"(item), "=m"(dq):);
	if (DISPATCH_OBJ_IS_VTABLE(item))
		invoke_ptr = (uint64_t)(item->do_vtable->do_invoke);
	else
		invoke_ptr = 0;
	kdebug_trace(DISPATCH_ENQUEUE, 0x2fULL,
		dq, (uint64_t)item,
		(uint64_t)item->dc_ctxt, 0);
	kdebug_trace(DISPATCH_ENQUEUE, 1ULL << 32 | 0x2f,
		(uint64_t)item->dc_func, invoke_ptr,
		(uint64_t)item->do_vtable, 0);
	restore_registers
	//simulation
	asm volatile("movq	$0x0, 0x10(%%r14)\n"
		::);
}
#endif
//TODO: Not verified
#if __i386__
/* 0x38
 * __dispatch_apply_redirect:
 * 0x0L	00018811	55 	pushl	%ebp
 * 0x1a7L	000189b8	31 c9 	xorl	%ecx, %ecx
 * 0x1a9L	000189ba	8b 55 f0 	movl	-0x10(%ebp), %edx
 * 0x1acL	000189bd	c7 42 0c 00 00 00 00 	movl	$0x0, 0xc(%edx)
 * 0x1b3L	000189c4	87 57 28 	xchgl	%edx, 0x28(%edi)
 * 0x1b6L	000189c7	85 d2 	testl	%edx, %edx
 * 0x1b8L	000189c9	74 15 	je	0x189e0
 * 0x1baL	000189cb	89 72 0c 	movl	%esi, 0xc(%edx)
 * 0x1bdL	000189ce	84 c9 	testb	%cl, %cl
 * target: 000189c4	87 57 28 	xchgl	%edx, 0x28(%edi)
 */
void shell_enq__dispatch_apply_redirect()
{
	save_registers
	uint64_t dq, invoke_ptr;
	struct dispatch_patch_item_head *item;
	asm volatile("movl %%edx, %0\n"
		"movl %%edi, %1"
		:"=m"(item), "=m"(dq):);
	if (DISPATCH_OBJ_IS_VTABLE(item))
		invoke_ptr = (uint64_t)(item->do_vtable->do_invoke);
	else
		invoke_ptr = 0;
	kdebug_trace(DISPATCH_ENQUEUE, 0x38ULL,
		dq, (uint64_t)item,
		(uint64_t)item->dc_ctxt, 0);
	kdebug_trace(DISPATCH_ENQUEUE, 1ULL << 32 | 0x38,
		(uint64_t)item->dc_func, invoke_ptr,
		(uint64_t)item->do_vtable, 0);
	restore_registers
	//simulation
	asm volatile("movl	$0x0, 0xc(%%edx)\n"
		::);
}
#endif
void detour__dispatch_apply_redirect(struct mach_o_handler *handler_ptr)
{
#if __x86_64__
	detour_function(handler_ptr, "_dispatch_apply_redirect", 
		shell_enq__dispatch_apply_redirect, 0x1f9, 8);
#elif __i386__
	detour_function(handler_ptr, "_dispatch_apply_redirect", 
		shell_enq__dispatch_apply_redirect, 0x1ac, 7);
#endif
}
/************/
#if __x86_64__
/* 0x31
 * _dispatch_barrier_async_f:
 * 0x0L	0000000000019f68	4889d0          	movq	%rdx, %rax
 * 0x11eL	000000000001a086	31c0            	xorl	%eax, %eax
 * 0x120L	000000000001a088	48c7421000000000	movq	$0x0, 0x10(%rdx)
 * 0x128L	000000000001a090	4889d1          	movq	%rdx, %rcx
 * 0x12bL	000000000001a093	48874f40        	xchgq	%rcx, 0x40(%rdi)
 * 0x12fL	000000000001a097	4885c9          	testq	%rcx, %rcx
 * 0x132L	000000000001a09a	740d            	je	0x1a0a9
 * 0x134L	000000000001a09c	48895110        	movq	%rdx, 0x10(%rcx)
 * 0x138L	000000000001a0a0	84c0            	testb	%al, %al
 * target: 000000000001a093	48874f40        	xchgq	%rcx, 0x40(%rdi)
 * verify: 000000000001a0ac	e910b1feff      	jmp	0x51c1
 */
void shell_enq_dispatch_barrier_async_f()
{
	save_registers
	uint64_t dq, invoke_ptr;
	struct dispatch_patch_item_head *item;
	asm volatile("movq %%rdx, %0\n"
		"movq %%rdi, %1"
		:"=m"(item), "=m"(dq):);
	if (DISPATCH_OBJ_IS_VTABLE(item))
		invoke_ptr = (uint64_t)(item->do_vtable->do_invoke);
	else
		invoke_ptr = 0;
	kdebug_trace(DISPATCH_ENQUEUE, 0x31ULL,
		dq, (uint64_t)item,
		(uint64_t)item->dc_ctxt, 0);
	kdebug_trace(DISPATCH_ENQUEUE, 1ULL << 32 | 0x31,
		(uint64_t)item->dc_func, invoke_ptr,
		(uint64_t)item->do_vtable, 0);
	restore_registers
	//simulation
	asm volatile("movq	%%rdx, %%rcx\n"
		"xchgq	%%rcx, 0x40(%%rdi)"
		::);
}
#endif
#if __i386__
/* 0x3a
 * _dispatch_barrier_async_f:
 * 0x0L	0001a715	55 	pushl	%ebp
 * 0x13cL	0001a851	8b 75 08 	movl	0x8(%ebp), %esi
 * 0x13fL	0001a854	c7 40 0c 00 00 00 00 	movl	$0x0, 0xc(%eax)
 * 0x146L	0001a85b	89 c1 	movl	%eax, %ecx
 * 0x148L	0001a85d	87 4e 28 	xchgl	%ecx, 0x28(%esi)
 * 0x14bL	0001a860	85 c9 	testl	%ecx, %ecx
 * 0x14dL	0001a862	74 15 	je	0x1a879
 * 0x14fL	0001a864	89 41 0c 	movl	%eax, 0xc(%ecx)
 * 0x152L	0001a867	84 db 	testb	%bl, %bl
 * target: 0001a85d	87 4e 28 	xchgl	%ecx, 0x28(%esi)
 * verify: 0001a88b	e8 50 9e fe ff 	calll	0x46e0
 */
void shell_enq_dispatch_barrier_async_f()
{
	save_registers
	uint64_t dq, invoke_ptr;
	struct dispatch_patch_item_head *item;
	asm volatile("movl %%eax, %0\n"
		"movl %%esi, %1"
		:"=m"(item), "=m"(dq):);
	if (DISPATCH_OBJ_IS_VTABLE(item))
		invoke_ptr = (uint64_t)(item->do_vtable->do_invoke);
	else
		invoke_ptr = 0;
	kdebug_trace(DISPATCH_ENQUEUE, 0x3aULL,
		dq, (uint64_t)item,
		(uint64_t)item->dc_ctxt, 0);
	kdebug_trace(DISPATCH_ENQUEUE, 1ULL << 32 | 0x3a,
		(uint64_t)item->dc_func, invoke_ptr,
		(uint64_t)item->do_vtable, 0);
	restore_registers
	//simulation
	asm volatile("movl	%%eax, %%ecx\n"
		"xchgl	%%ecx, 0x28(%%esi)"
		::);
}
#endif
void detour_dispatch_barrier_async_f(struct mach_o_handler *handler_ptr)
{
#if __x86_64__
	detour_function(handler_ptr, "dispatch_barrier_async_f", 
		shell_enq_dispatch_barrier_async_f, 0x128, 7);
#elif __i386__
	detour_function(handler_ptr, "dispatch_barrier_async_f", 
		shell_enq_dispatch_barrier_async_f, 0x146, 5);
#endif
}
/************/
void detour_enqueue(struct mach_o_handler * handler_ptr)
{
	detour__dispatch_queue_wakeup_with_qos_slow(handler_ptr);
	detour__dispatch_barrier_async_detached_f(handler_ptr);
	detour__dispatch_barrier_async_f_slow(handler_ptr);
	detour__dispatch_wakeup(handler_ptr);
	detour__dispatch_queue_push_queue(handler_ptr);
	detour__dispatch_kq_init(handler_ptr);
	detour_dispatch_mach_receive_barrier_f(handler_ptr);
	detour_dispatch_async_f(handler_ptr);
	detour__dispatch_async_f_slow(handler_ptr);
	detour__dispatch_mach_msg_send(handler_ptr);
	detour__dispatch_mach_send_drain(handler_ptr);
	detour__dispatch_mach_msg_recv(handler_ptr);
	detour__dispatch_mach_reply_kevent_unregister(handler_ptr);
	detour_dispatch_async_enforce_qos_class_f(handler_ptr);
	detour_dispatch_async(handler_ptr);
	detour__dispatch_barrier_async_f(handler_ptr);
	detour_dispatch_mach_send_barrier_f(handler_ptr);
	detour__dispatch_mach_cancel(handler_ptr);
	detour__dispatch_mach_disconnect(handler_ptr);
	detour_dispatch_group_async(handler_ptr);
	detour__dispatch_barrier_sync_f_slow(handler_ptr);
	detour_dispatch_group_async_f(handler_ptr);
	detour_dispatch_apply_f(handler_ptr);
	detour__dispatch_mach_msg_not_sent(handler_ptr);
	detour__dispatch_main_queue_callback_4CF(handler_ptr);
	detour__dispatch_async_f2(handler_ptr);
	detour__dispatch_async_f_redirect(handler_ptr);
	detour__dispatch_sync_f_slow(handler_ptr);
	detour__dispatch_apply_redirect(handler_ptr);
	detour_dispatch_barrier_async_f(handler_ptr);
}
