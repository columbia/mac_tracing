/* libdispatch.dylib
 * version 501.40.12
 * arch i386 x86_64
 ************************/
#include "lib_mach_info.h"
#include <dispatch/dispatch.h>

#define DISPATCH_DEQUEUE 0x210a000c

/* copy from libdispatch */
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
/*end of copy*/

/************/
#if __x86_64__
/* 0x3
 * __dispatch_root_queue_drain:
 * 0x0L	0000000000005b39	55              	pushq	%rbp
 * 0x521L	000000000000605a	4c8b7b30        	movq	0x30(%rbx), %r15
 * 0x525L	000000000000605e	488b7320        	movq	0x20(%rbx), %rsi
 * 0x529L	0000000000006062	488b7b28        	movq	0x28(%rbx), %rdi
 * 0x52dL	0000000000006066	e898c3ffff      	callq	0x2403
 * 0x532L	000000000000606b	4d85ff          	testq	%r15, %r15
 * 0x535L	000000000000606e	7432            	je	0x60a2
 * 0x537L	0000000000006070	4c89ff          	movq	%r15, %rdi
 * 0x53aL	0000000000006073	e8965c0000      	callq	0xbd0e
 * target: 0000000000006066	e898c3ffff      	callq	0x2403
 * cmpxchg: 0000000000005cdc	0fb111          	cmpxchgl	%edx, (%rcx)
 * pause: 0000000000005cb1	f390            	pause
 */
/* 0x4
 * __dispatch_root_queue_drain:
 * 0x0L	0000000000005b39	55              	pushq	%rbp
 * 0x751L	000000000000628a	4c8b7b30        	movq	0x30(%rbx), %r15
 * 0x755L	000000000000628e	488b7320        	movq	0x20(%rbx), %rsi
 * 0x759L	0000000000006292	488b7b28        	movq	0x28(%rbx), %rdi
 * 0x75dL	0000000000006296	e868c1ffff      	callq	0x2403
 * 0x762L	000000000000629b	4d85ff          	testq	%r15, %r15
 * 0x765L	000000000000629e	7432            	je	0x62d2
 * 0x767L	00000000000062a0	4c89ff          	movq	%r15, %rdi
 * 0x76aL	00000000000062a3	e8665a0000      	callq	0xbd0e
 * target: 0000000000006296	e868c1ffff      	callq	0x2403
 * cmpxchg: 0000000000005cdc	0fb111          	cmpxchgl	%edx, (%rcx)
 * pause: 0000000000005cb1	f390            	pause
 */

/* __dispatch_root_queue_drain
 * 0x0 0000000000005b39    55                  pushq   %rbp
 * 0xccL	0000000000005c05	4d8d742438      	leaq	0x38(%r12), %r14
 * 0xd1L	0000000000005c0a	4d8d6c2440      	leaq	0x40(%r12), %r13
 * 0xd6L	0000000000005c0f	4531ff          	xorl	%r15d, %r15d
 * 0xd9L	0000000000005c12	eb0c            	jmp	0x5c20
 * 0xdbL	0000000000005c14	6548890425c8000000	movq	%rax, %gs:0xc8
 * 0xe4L	0000000000005c1d	41b701          	movb	$0x1, %r15b
 * start:
 * 0xe7L	0000000000005c20	48c7c3ffffffff  	movq	$-0x1, %rbx
 * 0xeeL	0000000000005c27	49871e          	xchgq	%rbx, (%r14)
 * 0xf1L	0000000000005c2a	4885db          	testq	%rbx, %rbx
 * 0xf4L	0000000000005c2d	7418            	je	0x5c47
 */

static void shell_deq__dispatch_root_queue_drain()
{
	uint64_t dq, invoke_ptr;
	struct dispatch_patch_item_head *item;
	save_registers
	//simulation
	asm volatile("movq $-0x1, %%rbx\n"
		"xchgq %%rbx, (%%r14)\n"
		"movq %%rbx, %0\n"
		"movq %%r12, %1"
		:"=m"(item), "=m"(dq):);

	if ((uintptr_t)item != (uintptr_t)-1 && item != 0) {
		if (DISPATCH_OBJ_IS_VTABLE(item))
			invoke_ptr = (uint64_t)(item->do_vtable->do_invoke);
		else
			invoke_ptr = 0;
		kdebug_trace(DISPATCH_DEQUEUE, 0x4,
				dq, (uint64_t)item,
				(uint64_t)item->dc_ctxt, 0);
		kdebug_trace(DISPATCH_DEQUEUE, 1ULL << 32 | 0x4, 
				(uint64_t)item->dc_func, invoke_ptr, 
				(uint64_t)item->do_vtable, 0);
	}
	restore_registers
	//simulation
	asm volatile("movq %0, %%rbx"::"m"(item));
}
#endif
#if __i386__
/* 0x3
 * __dispatch_root_queue_drain:
 * 0x0L	000050b1	55 	pushl	%ebp
 * 0x4dcL	0000558d	8b 4b 14 	movl	0x14(%ebx), %ecx
 * 0x4dfL	00005590	89 44 24 04 	movl	%eax, 0x4(%esp)
 * 0x4e3L	00005594	89 0c 24 	movl	%ecx, (%esp)
 * 0x4e6L	00005597	e8 71 c2 ff ff 	calll	0x180d
 * 0x4ebL	0000559c	85 f6 	testl	%esi, %esi
 * 0x4edL	0000559e	74 12 	je	0x55b2
 * 0x4efL	000055a0	89 34 24 	movl	%esi, (%esp)
 * 0x4f2L	000055a3	e8 47 5f 00 00 	calll	0xb4ef
 * target: 00005597	e8 71 c2 ff ff 	calll	0x180d
 * cmpxchg: 000053be	0f b1 0e 	cmpxchgl	%ecx, (%esi)
 * pause: 000053d5	f3 90 	pause
 */
/* 0x4
 * __dispatch_root_queue_drain:
 * 0x0L	000050b1	55 	pushl	%ebp
 * 0x6dcL	0000578d	8b 4b 14 	movl	0x14(%ebx), %ecx
 * 0x6dfL	00005790	89 44 24 04 	movl	%eax, 0x4(%esp)
 * 0x6e3L	00005794	89 0c 24 	movl	%ecx, (%esp)
 * 0x6e6L	00005797	e8 71 c0 ff ff 	calll	0x180d
 * 0x6ebL	0000579c	8b 5d e4 	movl	-0x1c(%ebp), %ebx
 * 0x6eeL	0000579f	85 db 	testl	%ebx, %ebx
 * 0x6f0L	000057a1	74 12 	je	0x57b5
 * 0x6f2L	000057a3	89 1c 24 	movl	%ebx, (%esp)
 * target: 00005797	e8 71 c0 ff ff 	calll	0x180d
 * cmpxchg: 000053be	0f b1 0e 	cmpxchgl	%ecx, (%esi)
 * pause: 000053d5	f3 90 	pause
 */
/* __dispatch_root_queue_drain:
 * 000050b1	55 	pushl	%ebp
 * 0xa9L	0000515a	8b 45 e0 	movl	-0x20(%ebp), %eax
 * 0xacL	0000515d	8d 48 24 	leal	0x24(%eax), %ecx
 * 0xafL	00005160	89 4d f0 	movl	%ecx, -0x10(%ebp)
 * 0xb2L	00005163	8d 70 28 	leal	0x28(%eax), %esi
 * 0xb5L	00005166	89 75 dc 	movl	%esi, -0x24(%ebp)
 * start:
 * 0xb8L	00005169	31 d2 	xorl	%edx, %edx
 * 0xbaL	0000516b	eb 08 	jmp	0x5175
 * 0xbcL	0000516d	65 a3 64 00 00 00 	movl	%eax, %gs:0x64
 * 0xc2L	00005173	b2 01 	movb	$0x1, %dl
 * 0xc4L	00005175	89 d7 	movl	%edx, %edi
 * 0xc6L	00005177	8b 55 f0 	movl	-0x10(%ebp), %edx
 * 0xc9L	0000517a	bb ff ff ff ff 	movl	$0xffffffff, %ebx
 * 0xceL	0000517f	87 1a 	xchgl	%ebx, (%edx)
 * 0xd0L	00005181	85 db 	testl	%ebx, %ebx
 * 0xd2L	00005183	74 13 	je	0x5198
 * 0xd4L	00005185	83 fb ff 	cmpl	$-0x1, %ebx
 */
static void shell_deq__dispatch_root_queue_drain()
{
	uint64_t dq, invoke_ptr;
	struct dispatch_patch_item_head *item;
	save_registers
	//simulation
	asm volatile("movl $-0x1, %%ebx\n"
		"xchgl %%ebx, (%%edx)\n"
		"movl %%ebx, -0x4(%%ebp)\n"
		"movl %%ebx, %0\n"
		"movl (%%ebp), %%eax\n"
		"movl -0x20(%%eax), %%ebx\n"
		"movl %%ebx, %1"
		:"=m"(item), "=m"(dq):);

	if ((uintptr_t)item != (uintptr_t)-1 && item != 0) {
		if (DISPATCH_OBJ_IS_VTABLE(item))
			invoke_ptr = (uint64_t)(item->do_vtable->do_invoke);
		else
			invoke_ptr = 0;
		kdebug_trace(DISPATCH_DEQUEUE, 0x4,
				dq, (uint64_t)item,
				(uint64_t)item->dc_ctxt, 0);
		kdebug_trace(DISPATCH_DEQUEUE, 1ULL << 32 | 0x4, 
				(uint64_t)item->dc_func, invoke_ptr, 
				(uint64_t)item->do_vtable, 0);
	}
	restore_registers
	//simulation
	//asm volatile("movl %0, %%ebx"::"m"(item));
}
#endif
static void detour__dispatch_root_queue_drain(struct mach_o_handler *handler_ptr)
{
#if __x86_64__
	detour_function(handler_ptr, "_dispatch_root_queue_drain", 
		shell_deq__dispatch_root_queue_drain, 0xe7, 10);
#elif __i386__
	detour_function(handler_ptr, "_dispatch_root_queue_drain", 
		shell_deq__dispatch_root_queue_drain, 0xc9, 7);
#endif
}
/************/
//TODO: No cmpxchg
#if __x86_64__
/* 0x5
 * __dispatch_queue_drain:
 * 0x0L	0000000000006d49	55              	pushq	%rbp
 * 0x2deL	0000000000007027	498b5c2430      	movq	0x30(%r12), %rbx
 * 0x2e3L	000000000000702c	498b742420      	movq	0x20(%r12), %rsi
 * 0x2e8L	0000000000007031	498b7c2428      	movq	0x28(%r12), %rdi
 * 0x2edL	0000000000007036	e8c8b3ffff      	callq	0x2403
 * 0x2f2L	000000000000703b	4885db          	testq	%rbx, %rbx
 * 0x2f5L	000000000000703e	7431            	je	0x7071
 * 0x2f7L	0000000000007040	4889df          	movq	%rbx, %rdi
 * 0x2faL	0000000000007043	e8c64c0000      	callq	0xbd0e
 * target: 0000000000007036	e8c8b3ffff      	callq	0x2403
 * pause: 0000000000006e76	f390            	pause
 */
/* 0x6
 * __dispatch_queue_drain:
 * 0x0L	0000000000006d49	55              	pushq	%rbp
 * 0x7feL	0000000000007547	4d8b6c2430      	movq	0x30(%r12), %r13
 * 0x803L	000000000000754c	498b742420      	movq	0x20(%r12), %rsi
 * 0x808L	0000000000007551	498b7c2428      	movq	0x28(%r12), %rdi
 * 0x80dL	0000000000007556	e8a8aeffff      	callq	0x2403
 * 0x812L	000000000000755b	4d85ed          	testq	%r13, %r13
 * 0x815L	000000000000755e	7432            	je	0x7592
 * 0x817L	0000000000007560	4c89ef          	movq	%r13, %rdi
 * 0x81aL	0000000000007563	e8a6470000      	callq	0xbd0e
 * target: 0000000000007556	e8a8aeffff      	callq	0x2403
 * cmpxchg: 0000000000007119	480fb132        	cmpxchgq	%rsi, (%rdx)
 * pause: 0000000000007127	f390            	pause
 */
static void shell_deq__dispatch_queue_drain()
{
	uint64_t dq, invoke_ptr;
	struct dispatch_patch_item_head *item;
	save_registers
	asm volatile("movq %%r12, %0\n"
		"movq %%r14, %1"
		:"=m"(item), "=m"(dq):);	

	if (DISPATCH_OBJ_IS_VTABLE(item))
		invoke_ptr = (uint64_t)(item->do_vtable->do_invoke);
	else
		invoke_ptr = 0;

	kdebug_trace(DISPATCH_DEQUEUE, 0x6,
		dq, (uint64_t)item,
		(uint64_t)item->dc_ctxt, 0);
	kdebug_trace(DISPATCH_DEQUEUE, 1ULL << 32 | 0x6, 
		(uint64_t)item->dc_func, invoke_ptr, 
		(uint64_t)item->do_vtable, 0);
	restore_registers
	//simulation
	asm volatile("movq 0x20(%%r12), %%rsi\n"::);
}
#endif
//TODO: No cmpxchg
#if __i386__
/* 0x5
 * __dispatch_queue_drain:
 * 0x0L	00006323	55 	pushl	%ebp
 * 0x2b7L	000065da	31 f6 	xorl	%esi, %esi
 * 0x2b9L	000065dc	f6 03 04 	testb	$0x4, (%ebx)
 * 0x2bcL	000065df	74 06 	je	0x65e7
 * 0x2beL	000065e1	8b 43 18 	movl	0x18(%ebx), %eax
 * 0x2c1L	000065e4	89 45 e0 	movl	%eax, -0x20(%ebp)
 * 0x2c4L	000065e7	8b 43 10 	movl	0x10(%ebx), %eax
 * 0x2c7L	000065ea	8b 4b 14 	movl	0x14(%ebx), %ecx
 * 0x2caL	000065ed	89 44 24 04 	movl	%eax, 0x4(%esp)
 * 0x2ceL	000065f1	89 0c 24 	movl	%ecx, (%esp)
 * 0x2d1L	000065f4	e8 14 b2 ff ff 	calll	0x180d
 * 0x2d6L	000065f9	8b 5d e0 	movl	-0x20(%ebp), %ebx
 * 0x2d9L	000065fc	85 db 	testl	%ebx, %ebx
 * 0x2dbL	000065fe	74 12 	je	0x6612
 * 0x2ddL	00006600	89 1c 24 	movl	%ebx, (%esp)
 * target: 000065f4	e8 14 b2 ff ff 	calll	0x180d
 * pause: 0000641c	f3 90 	pause
 */
/* 0x6
 * __dispatch_queue_drain:
 * 0x0L	00006323	55 	pushl	%ebp
 * 0x7b3L	00006ad6	31 f6 	xorl	%esi, %esi
 * 0x7b5L	00006ad8	f6 03 04 	testb	$0x4, (%ebx)
 * 0x7b8L	00006adb	74 06 	je	0x6ae3
 * 0x7baL	00006add	8b 43 18 	movl	0x18(%ebx), %eax
 * 0x7bdL	00006ae0	89 45 c4 	movl	%eax, -0x3c(%ebp)
 * 0x7c0L	00006ae3	8b 43 10 	movl	0x10(%ebx), %eax
 * 0x7c3L	00006ae6	8b 4b 14 	movl	0x14(%ebx), %ecx
 * 0x7c6L	00006ae9	89 44 24 04 	movl	%eax, 0x4(%esp)
 * 0x7caL	00006aed	89 0c 24 	movl	%ecx, (%esp)
 * 0x7cdL	00006af0	e8 18 ad ff ff 	calll	0x180d
 * 0x7d2L	00006af5	8b 5d c4 	movl	-0x3c(%ebp), %ebx
 * 0x7d5L	00006af8	85 db 	testl	%ebx, %ebx
 * 0x7d7L	00006afa	74 16 	je	0x6b12
 * 0x7d9L	00006afc	89 1c 24 	movl	%ebx, (%esp)
 * target: 00006af0	e8 18 ad ff ff 	calll	0x180d
 * cmpxchg: 000066df	0f b1 3a 	cmpxchgl	%edi, (%edx)
 * pause: 000066e6	f3 90 	pause
 *
 * check %%esi == dq
 */
static void shell_deq__dispatch_queue_drain()
{
	uint64_t dq, invoke_ptr;
	struct dispatch_patch_item_head *item;
	save_registers
	asm volatile("movl %%ebx, %0\n"
		"movl (%%ebp), %%eax\n"
		"movl 0x8(%%eax), %%ecx\n"
		"movl %%ecx, %1"
		:"=m"(item), "=m"(dq):);
	if (DISPATCH_OBJ_IS_VTABLE(item))
		invoke_ptr = (uint64_t)(item->do_vtable->do_invoke);
	else
		invoke_ptr = 0;
	kdebug_trace(DISPATCH_DEQUEUE, 0x6,
		dq, (uint64_t)item,
		(uint64_t)item->dc_ctxt, 0);
	kdebug_trace(DISPATCH_DEQUEUE, 1ULL << 32 | 0x6, 
		(uint64_t)item->dc_func, invoke_ptr, 
		(uint64_t)item->do_vtable, 0);
	restore_registers
	//simulation
	asm volatile("movl 0x10(%%ebx), %%eax\n"
		"movl 0x14(%%ebx), %%ecx"::);
}
#endif
static void detour__dispatch_queue_drain(struct mach_o_handler *handler_ptr)
{
#if __x86_64__
	detour_function(handler_ptr, "_dispatch_queue_drain", 
		shell_deq__dispatch_queue_drain, 0x2e3, 5);
	detour_function(handler_ptr, "_dispatch_queue_drain", 
		shell_deq__dispatch_queue_drain, 0x803, 5);
#elif __i386__
	detour_function(handler_ptr, "_dispatch_queue_drain", 
		shell_deq__dispatch_queue_drain, 0x2c4, 6);
	detour_function(handler_ptr, "_dispatch_queue_drain", 
		shell_deq__dispatch_queue_drain, 0x7c0, 6);
#endif
}
/************/
#if __x86_64__
/* 0x15
 * __dispatch_main_queue_callback_4CF:
 * 0x0L	0000000000015587	55              	pushq	%rbp
 * 0x475L	00000000000159fc	498b5d30        	movq	0x30(%r13), %rbx
 * 0x479L	0000000000015a00	498b7520        	movq	0x20(%r13), %rsi
 * 0x47dL	0000000000015a04	498b7d28        	movq	0x28(%r13), %rdi
 * 0x481L	0000000000015a08	e8f6c9feff      	callq	0x2403
 * 0x486L	0000000000015a0d	4885db          	testq	%rbx, %rbx
 * 0x489L	0000000000015a10	7431            	je	0x15a43
 * 0x48bL	0000000000015a12	4889df          	movq	%rbx, %rdi
 * 0x48eL	0000000000015a15	e8f462ffff      	callq	0xbd0e
 * target: 0000000000015a08	e8f6c9feff      	callq	0x2403
 * pause: 00000000000156cb	f390            	pause
 * calculateing dispath mainq address:
 * #1 arg for _dispatch_queue_push_slow
 * (0x155f2 + 7 + 0x27447 - 0x15a05) (%%ret) = 0x2703b(%%ret)
 * or gs:0xa0
 */
static void shell_deq__dispatch_main_queue_callback_4CF_0()
{
	uint64_t dq, invoke_ptr;
	struct dispatch_patch_item_head *item;
	save_registers
	asm volatile("movq %%r13, %0\n"
		"movq 0x8(%%rbp), %%rax\n"
		"addq $0x2703b, %%rax\n"
		"movq %%rax, %1"
		:"=m"(item), "=m"(dq):);

	if (DISPATCH_OBJ_IS_VTABLE(item))
		invoke_ptr = (uint64_t)(item->do_vtable->do_invoke);
	else
		invoke_ptr = 0;
	kdebug_trace(DISPATCH_DEQUEUE, 0x15,
		dq, (uint64_t)item,
		(uint64_t)item->dc_ctxt, 0);
	kdebug_trace(DISPATCH_DEQUEUE, 1ULL << 32 | 0x15, 
		(uint64_t)item->dc_func, invoke_ptr, 
		(uint64_t)item->do_vtable, 0);
	restore_registers
	//simulation
	asm volatile("movq 0x20(%%r13), %%rsi\n"
		"movq 0x28(%%r13), %%rdi"::);
}

/* 0x16
 * __dispatch_main_queue_callback_4CF:
 * 0x0L	0000000000015587	55              	pushq	%rbp
 * 0x684L	0000000000015c0b	498b5d30        	movq	0x30(%r13), %rbx
 * 0x688L	0000000000015c0f	498b7520        	movq	0x20(%r13), %rsi
 * 0x68cL	0000000000015c13	498b7d28        	movq	0x28(%r13), %rdi
 * 0x690L	0000000000015c17	e8e7c7feff      	callq	0x2403
 * 0x695L	0000000000015c1c	4885db          	testq	%rbx, %rbx
 * 0x698L	0000000000015c1f	7431            	je	0x15c52
 * 0x69aL	0000000000015c21	4889df          	movq	%rbx, %rdi
 * 0x69dL	0000000000015c24	e8e560ffff      	callq	0xbd0e
 * target: 0000000000015c17	e8e7c7feff      	callq	0x2403
 * pause: 00000000000156cb	f390            	pause
 * calculateing dispath mainq address:
 * (0x155f2 + 7 + 0x27447 - 0x15c14) (%%ret) = 0x26e2c(%%ret)
 * or gs:0xa0
 * calculation mainq addr:
 * 00000000000155f2    48 8d 3d 47 74 02 00    leaq    0x27447(%rip), %rdi
 * 00000000000155af    4c 8d 3d 8a 74 02 00    leaq    0x2748a(%rip), %r15 
 * 00000000000155b6    49 83 7f 40 00  cmpq    $0x0, 0x40(%r15)
 */
static void shell_deq__dispatch_main_queue_callback_4CF_1()
{
	uint64_t dq, invoke_ptr;
	struct dispatch_patch_item_head *item;
	save_registers
	asm volatile("movq %%r13, %0\n"
		"movq 0x8(%%rbp), %%rax\n"
		"addq $0x26e2c, %%rax\n"
		"movq %%rax, %1"
		:"=m"(item), "=m"(dq):);
	if (DISPATCH_OBJ_IS_VTABLE(item))
		invoke_ptr = (uint64_t)(item->do_vtable->do_invoke);
	else
		invoke_ptr = 0;
	kdebug_trace(DISPATCH_DEQUEUE, 0x16,
		dq, (uint64_t)item,
		(uint64_t)item->dc_ctxt, 0);
	kdebug_trace(DISPATCH_DEQUEUE, 1ULL << 32 | 0x16, 
		(uint64_t)item->dc_func, invoke_ptr, 
		(uint64_t)item->do_vtable, 0);
	restore_registers
	//simulation
	asm volatile("movq 0x20(%%r13), %%rsi\n"
		"movq 0x28(%%r13), %%rdi"::);
}
#endif
//TODO: No cmpxchg
#if __i386__
/* 0x15
 * __dispatch_main_queue_callback_4CF:
 * 0x0L	00015911	55 	pushl	%ebp
 * 0x465L	00015d76	8b 47 18 	movl	0x18(%edi), %eax
 * 0x468L	00015d79	89 45 b4 	movl	%eax, -0x4c(%ebp)
 * 0x46bL	00015d7c	8b 47 10 	movl	0x10(%edi), %eax
 * 0x46eL	00015d7f	8b 4f 14 	movl	0x14(%edi), %ecx
 * 0x471L	00015d82	89 44 24 04 	movl	%eax, 0x4(%esp)
 * 0x475L	00015d86	89 0c 24 	movl	%ecx, (%esp)
 * 0x478L	00015d89	e8 7f ba fe ff 	calll	0x180d
 * 0x47dL	00015d8e	8b 7d b4 	movl	-0x4c(%ebp), %edi
 * 0x480L	00015d91	85 ff 	testl	%edi, %edi
 * 0x482L	00015d93	74 12 	je	0x15da7
 * 0x484L	00015d95	89 3c 24 	movl	%edi, (%esp)
 * target: 00015d89	e8 7f ba fe ff 	calll	0x180d
 * pause: 00015a24	f3 90 	pause
 *
 * (0x15711 + 0x1591f - 0x15d81)(%%ret) = 152af(%%ret)
 *
 * calculate mainq:
 * 0001591a	e8 00 00 00 00 	calll	0x1591f
 * 0001591f    5e  popl    %esi
 * 00015937	8b 9e 11 57 01 00 	movl	0x15711(%esi), %ebx
 * 0001593d    89 5d bc    movl    %ebx, -0x44(%ebp)
 * 00015940    83 7b 28 00     cmpl    $0x0, 0x28(%ebx)
 */
static void shell_deq__dispatch_main_queue_callback_4CF_0()
{
	uint64_t dq, invoke_ptr;
	struct dispatch_patch_item_head *item;
	save_registers
	asm volatile("movl %%edi, %0\n"
		"movl 0x4(%%ebp), %%eax\n"
		"movl 0x152af(%%eax), %%ecx\n"
		"movl %%ecx, %1"
		:"=m"(item), "=m"(dq):);
	if (DISPATCH_OBJ_IS_VTABLE(item))
		invoke_ptr = (uint64_t)(item->do_vtable->do_invoke);
	else
		invoke_ptr = 0;
	kdebug_trace(DISPATCH_DEQUEUE, 0x15,
		dq, (uint64_t)item,
		(uint64_t)item->dc_ctxt, 0);
	kdebug_trace(DISPATCH_DEQUEUE, 1ULL << 32 | 0x15, 
		(uint64_t)item->dc_func, invoke_ptr, 
		(uint64_t)item->do_vtable, 0);
	restore_registers
	//simulation
	asm volatile("movl 0x10(%%edi), %%eax\n"
		"movl 0x14(%%edi), %%ecx"::);
}
/* 0x16
 * __dispatch_main_queue_callback_4CF:
 * 0x0L	00015911	55 	pushl	%ebp
 * 0x670L	00015f81	8b 47 18 	movl	0x18(%edi), %eax
 * 0x673L	00015f84	89 45 c0 	movl	%eax, -0x40(%ebp)
 * 0x676L	00015f87	8b 47 10 	movl	0x10(%edi), %eax
 * 0x679L	00015f8a	8b 4f 14 	movl	0x14(%edi), %ecx
 * 0x67cL	00015f8d	89 44 24 04 	movl	%eax, 0x4(%esp)
 * 0x680L	00015f91	89 0c 24 	movl	%ecx, (%esp)
 * 0x683L	00015f94	e8 74 b8 fe ff 	calll	0x180d
 * 0x688L	00015f99	8b 7d c0 	movl	-0x40(%ebp), %edi
 * 0x68bL	00015f9c	85 ff 	testl	%edi, %edi
 * 0x68dL	00015f9e	74 12 	je	0x15fb2
 * 0x68fL	00015fa0	89 3c 24 	movl	%edi, (%esp)
 * target: 00015f94	e8 74 b8 fe ff 	calll	0x180d
 * pause: 00015a24	f3 90 	pause
 * (0x15711 + 0x1591f - 0x15f8c)(%%ret) = 150a4(%%ret)
 *
 */
static void shell_deq__dispatch_main_queue_callback_4CF_1()
{
	uint64_t dq, invoke_ptr;
	struct dispatch_patch_item_head *item;
	save_registers
	asm volatile("movl %%edi, %0\n"
		"movl 0x4(%%ebp), %%eax\n"
		"movl 0x150a4(%%eax), %%ecx\n"
		"movl %%ecx, %1"
		:"=m"(item), "=m"(dq):);
	if (DISPATCH_OBJ_IS_VTABLE(item))
		invoke_ptr = (uint64_t)(item->do_vtable->do_invoke);
	else
		invoke_ptr = 0;
	kdebug_trace(DISPATCH_DEQUEUE, 0x16,
		dq, (uint64_t)item,
		(uint64_t)item->dc_ctxt, 0);
	kdebug_trace(DISPATCH_DEQUEUE, 1ULL << 32 | 0x16, 
		(uint64_t)item->dc_func, invoke_ptr, 
		(uint64_t)item->do_vtable, 0);
	restore_registers
	//simulation
	asm volatile("movl 0x10(%%edi), %%eax\n"
		"movl 0x14(%%edi), %%ecx"::);
}
#endif
static void detour__dispatch_main_queue_callback_4CF(struct mach_o_handler *handler_ptr)
{
#if __x86_64__
	detour_function(handler_ptr, "_dispatch_main_queue_callback_4CF", 
		shell_deq__dispatch_main_queue_callback_4CF_0, 0x479, 8);
	detour_function(handler_ptr, "_dispatch_main_queue_callback_4CF", 
		shell_deq__dispatch_main_queue_callback_4CF_1, 0x688, 8);
#elif __i386__
	detour_function(handler_ptr, "_dispatch_main_queue_callback_4CF", 
		shell_deq__dispatch_main_queue_callback_4CF_0, 0x46b, 6);
	detour_function(handler_ptr, "_dispatch_main_queue_callback_4CF", 
		shell_deq__dispatch_main_queue_callback_4CF_1, 0x676, 6);
#endif
}
/************/
#if __x86_64__
/* 0x17
 * __dispatch_runloop_root_queue_perform_4CF:
 * 0x0L	000000000001766c	55              	pushq	%rbp
 * 0x46eL	0000000000017ada	4c8b6330        	movq	0x30(%rbx), %r12
 * 0x472L	0000000000017ade	488b7320        	movq	0x20(%rbx), %rsi
 * 0x476L	0000000000017ae2	488b7b28        	movq	0x28(%rbx), %rdi
 * 0x47aL	0000000000017ae6	e818a9feff      	callq	0x2403
 * 0x47fL	0000000000017aeb	4d85e4          	testq	%r12, %r12
 * 0x482L	0000000000017aee	7434            	je	0x17b24
 * 0x484L	0000000000017af0	4c89e7          	movq	%r12, %rdi
 * 0x487L	0000000000017af3	e81642ffff      	callq	0xbd0e
 * target: 0000000000017ae6	e818a9feff      	callq	0x2403
 * cmpxchg: 0000000000017778	4c0fb131        	cmpxchgq	%r14, (%rcx)
 * pause: 0000000000017780	f390            	pause

 */
/* 0x18
 * __dispatch_runloop_root_queue_perform_4CF:
 * 0x0L	000000000001766c	55              	pushq	%rbp
 * 0x7b7L	0000000000017e23	4c8b6b30        	movq	0x30(%rbx), %r13
 * 0x7bbL	0000000000017e27	488b7320        	movq	0x20(%rbx), %rsi
 * 0x7bfL	0000000000017e2b	488b7b28        	movq	0x28(%rbx), %rdi
 * 0x7c3L	0000000000017e2f	e8cfa5feff      	callq	0x2403
 * 0x7c8L	0000000000017e34	4d85ed          	testq	%r13, %r13
 * 0x7cbL	0000000000017e37	7432            	je	0x17e6b
 * 0x7cdL	0000000000017e39	4c89ef          	movq	%r13, %rdi
 * 0x7d0L	0000000000017e3c	e8cd3effff      	callq	0xbd0e
 * target: 0000000000017e2f	e8cfa5feff      	callq	0x2403
 * cmpxchg: 0000000000017778	4c0fb131        	cmpxchgq	%r14, (%rcx)
 * pause: 0000000000017780	f390            	pause
 */
static void shell_deq__dispatch_runloop_root_queue_perform_4CF()
{
	uint64_t dq, invoke_ptr;
	struct dispatch_patch_item_head *item;
	save_registers
	asm volatile("movq %%rbx, %0\n"
		"movq %%r15, %1\n"
		:"=m"(item), "=m"(dq):);
	if (DISPATCH_OBJ_IS_VTABLE(item))
		invoke_ptr = (uint64_t)(item->do_vtable->do_invoke);
	else
		invoke_ptr = 0;
	kdebug_trace(DISPATCH_DEQUEUE, 0x17,
		dq, (uint64_t)item,
		(uint64_t)item->dc_ctxt, 0);
	kdebug_trace(DISPATCH_DEQUEUE, 1ULL << 32 | 0x17, 
		(uint64_t)item->dc_func, invoke_ptr, 
		(uint64_t)item->do_vtable, 0);
	restore_registers
	//simulation
	asm volatile("movq 0x20(%%rbx), %%rsi\n"
		"movq 0x28(%%rbx), %%rdi" ::);
}
#endif
#if __i386__
/* 0x17
 * __dispatch_runloop_root_queue_perform_4CF:
 * 0x0L	00017b4c	55 	pushl	%ebp
 * 0x459L	00017fa5	8b 46 18 	movl	0x18(%esi), %eax
 * 0x45cL	00017fa8	89 45 d0 	movl	%eax, -0x30(%ebp)
 * 0x45fL	00017fab	8b 46 10 	movl	0x10(%esi), %eax
 * 0x462L	00017fae	8b 4e 14 	movl	0x14(%esi), %ecx
 * 0x465L	00017fb1	89 44 24 04 	movl	%eax, 0x4(%esp)
 * 0x469L	00017fb5	89 0c 24 	movl	%ecx, (%esp)
 * 0x46cL	00017fb8	e8 50 98 fe ff 	calll	0x180d
 * 0x471L	00017fbd	8b 75 d0 	movl	-0x30(%ebp), %esi
 * 0x474L	00017fc0	85 f6 	testl	%esi, %esi
 * 0x476L	00017fc2	74 16 	je	0x17fda
 * 0x478L	00017fc4	89 34 24 	movl	%esi, (%esp)
 * target: 00017fb8	e8 50 98 fe ff 	calll	0x180d
 * cmpxchg: 00017c38	0f b1 19 	cmpxchgl	%ebx, (%ecx)
 * pause: 00017c3f	f3 90 	pause

 */
/* 0x18
 * __dispatch_runloop_root_queue_perform_4CF:
 * 0x0L	00017b4c	55 	pushl	%ebp
 * 0x7abL	000182f7	8b 46 18 	movl	0x18(%esi), %eax
 * 0x7aeL	000182fa	89 45 d8 	movl	%eax, -0x28(%ebp)
 * 0x7b1L	000182fd	8b 46 10 	movl	0x10(%esi), %eax
 * 0x7b4L	00018300	8b 4e 14 	movl	0x14(%esi), %ecx
 * 0x7b7L	00018303	89 44 24 04 	movl	%eax, 0x4(%esp)
 * 0x7bbL	00018307	89 0c 24 	movl	%ecx, (%esp)
 * 0x7beL	0001830a	e8 fe 94 fe ff 	calll	0x180d
 */
static void shell_deq__dispatch_runloop_root_queue_perform_4CF()
{
	uint64_t dq, invoke_ptr;
	struct dispatch_patch_item_head *item;
	save_registers
	asm volatile("movl %%esi, %0\n"
		"movl (%%ebp), %%eax\n"
		"movl 0x8(%%eax), %%ebx\n"
		"movl %%ebx, %1\n"
		:"=m"(item), "=m"(dq):);
	if (DISPATCH_OBJ_IS_VTABLE(item))
		invoke_ptr = (uint64_t)(item->do_vtable->do_invoke);
	else
		invoke_ptr = 0;
	kdebug_trace(DISPATCH_DEQUEUE, 0x18,
		dq, (uint64_t)item,
		(uint64_t)item->dc_ctxt, 0);
	kdebug_trace(DISPATCH_DEQUEUE, 1ULL << 32 | 0x18, 
		(uint64_t)item->dc_func, invoke_ptr, 
		(uint64_t)item->do_vtable, 0);
	restore_registers
	//simulation
	asm volatile("movl 0x10(%%esi), %%eax\n"
		"movl 0x14(%%esi), %%ecx"::);
}
#endif
static void detour__dispatch_runloop_root_queue_perform_4CF(
	struct mach_o_handler *handler_ptr)
{
#if __x86_64__
	detour_function(handler_ptr, "_dispatch_runloop_root_queue_perform_4CF", 
		shell_deq__dispatch_runloop_root_queue_perform_4CF, 0x472, 8);
	detour_function(handler_ptr, "_dispatch_runloop_root_queue_perform_4CF", 
		shell_deq__dispatch_runloop_root_queue_perform_4CF, 0x7bb, 8);
#elif __i386__
	detour_function(handler_ptr, "_dispatch_runloop_root_queue_perform_4CF", 
		shell_deq__dispatch_runloop_root_queue_perform_4CF, 0x45f, 6);
	detour_function(handler_ptr, "_dispatch_runloop_root_queue_perform_4CF", 
		shell_deq__dispatch_runloop_root_queue_perform_4CF, 0x7b1, 6);
#endif
}
/************/
void detour_dequeue(struct mach_o_handler * handler_ptr)
{
	detour__dispatch_root_queue_drain(handler_ptr);
	detour__dispatch_queue_drain(handler_ptr);
	detour__dispatch_main_queue_callback_4CF(handler_ptr);
	detour__dispatch_runloop_root_queue_perform_4CF(handler_ptr);
}
