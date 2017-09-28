#include "lib_mach_info.h"
#include <AppKit/NSEvent.h>
#include <objc/objc-auto.h>

#define NSEventDebug	0x2bd80100
#define NSAppGetEvent	0x2bd80108
#define NSViewSet	 0x2bd8011C

#if __x86_64__
/*
*-[NSApplication run]:
*000000000003cad6	55              	pushq	%rbp
*000000000003cad7	4889e5          	movq	%rsp, %rbp
*000000000003cada	4157            	pushq	%r15
*000000000003cadc	4156            	pushq	%r14
*000000000003cade	4155            	pushq	%r13
*000000000003cae0	4154            	pushq	%r12
*000000000003cae2	53              	pushq	%rbx
*000000000003cae3	4883ec28        	subq	$0x28, %rsp
*
*000000000003cd5b	48c7c2ffffffff  	movq	$-0x1, %rdx
*000000000003cd62	41b901000000    	movl	$0x1, %r9d
*000000000003cd68	488b7dd0        	movq	-0x30(%rbp), %rdi
*000000000003cd6c	488b3595bae300  	movq	0xe3ba95(%rip), %rsi
*000000000003cd73	4889c1          	movq	%rax, %rcx
*000000000003cd76	4c8b45c0        	movq	-0x40(%rbp), %r8
*000000000003cd7a	ff15d0f1be00    	callq	*0xbef1d0(%rip)
*000000000003cd80	4d89f5          	movq	%r14, %r13
*000000000003cd83	31db            	xorl	%ebx, %ebx
*000000000003cd85	e814f79e00      	callq	0xa2c49e
*/
void shell_application_run_wait_event_begin()
{
	save_registers
	kdebug_trace(NSAppGetEvent, 0ULL, 0ULL, 0ULL, 0ULL, 0);
	restore_registers
	asm volatile("movl $0x1, %%r9d"::);
}

uint64_t shell_application_run_wait_event_end()
{
	save_registers
	kdebug_trace(NSAppGetEvent, 1ULL, 0ULL, 0ULL, 0ULL, 0);
	restore_registers
	return objc_collectingEnabled();
}
#elif __i386__

#endif 

/*
*-[NSApplication sendEvent:]:
*/
#if __x86_64__
/* X86_64
*-[NSApplication sendEvent:]:
*00000000001d504c	55              	pushq	%rbp
*00000000001d504d	4889e5          	movq	%rsp, %rbp
*00000000001d5050	4157            	pushq	%r15
*00000000001d5052	4156            	pushq	%r14
*00000000001d5054	4155            	pushq	%r13
*00000000001d5056	4154            	pushq	%r12
*00000000001d5058	53              	pushq	%rbx
*00000000001d5059	4881ec18020000  	subq	$0x218, %rsp            ## imm = 0x218
*00000000001d5060	4989ff          	movq	%rdi, %r15
*00000000001d5063	4c8b354e59a500  	movq	0xa5594e(%rip), %r14
*00000000001d506a	4d8b36          	movq	(%r14), %r14
*00000000001d506d	4c8975d0        	movq	%r14, -0x30(%rbp)
*00000000001d5071	488b05d86ecd00  	movq	0xcd6ed8(%rip), %rax
*00000000001d5078	4d8b2407        	movq	(%r15,%rax), %r12
*00000000001d507c	4889d7          	movq	%rdx, %rdi
*00000000001d507f	e86a220000      	callq	0x1d72ee
*00000000001d5084	4989c5          	movq	%rax, %r13
*00000000001d5087	4d85ed          	testq	%r13, %r13
*00000000001d508a	0f8418190000    	je	0x1d69a8
*00000000001d5090	4c89a5c8fdffff  	movq	%r12, -0x238(%rbp)
*/
void shell_sendEvent(void *arg1, void *arg2, NSEvent *nsevent)
{
	save_registers
	asm volatile("movq %%r13, %0\n"
		:"=m"(nsevent):);

	if ([nsevent type] == NSKeyDown)
		kdebug_trace(NSEventDebug, [nsevent type], [nsevent keyCode], 0, 0, 0);
	else if ([nsevent type] == NSAppKitDefined || [nsevent type] == NSSystemDefined)
		kdebug_trace(NSEventDebug, [nsevent type], [nsevent subtype], 0, 0, 0);
	else
		kdebug_trace(NSEventDebug, [nsevent type], 0, 0, 0, 0);
	
	restore_registers
	asm volatile("pushq %%rax\n"
		"movq (%%rbp), %%rax\n"
		"movq %%r12, -0x238(%%rax)\n"
		"popq %%rax"::);
}
#elif __i386__
/* i386
*001ecfde	55              	pushl	%ebp
*001ecfdf	89e5            	movl	%esp, %ebp
*001ecfe1	53              	pushl	%ebx
*001ecfe2	57              	pushl	%edi
*001ecfe3	56              	pushl	%esi
*001ecfe4	81ec9c010000    	subl	$0x19c, %esp            ## imm = 0x19C
*001ecfea	e800000000      	calll	0x1ecfef
*001ecfef	5e              	popl	%esi
*001ecff0	8b5508          	movl	0x8(%ebp), %edx
*001ecff3	8b4510          	movl	0x10(%ebp), %eax
*001ecff6	8bbe1950af00    	movl	0xaf5019(%esi), %edi
*001ecffc	8b0f            	movl	(%edi), %ecx
*001ecffe	894df0          	movl	%ecx, -0x10(%ebp)
*001ed001	8b5a10          	movl	0x10(%edx), %ebx
*001ed004	890424          	movl	%eax, (%esp)
*001ed007	e86b260000      	calll	0x1ef677
*001ed00c	85c0            	testl	%eax, %eax
*001ed00e	0f845f0b0000    	je	0x1edb73
*001ed014	899d84feffff    	movl	%ebx, -0x17c(%ebp)
*/
void shell_sendEvent(void *arg1, void *arg2, NSEvent *nsevent)
{
	save_registers
	kdebug_trace(NSEventDebug, (uint64_t)[nsevent type], 0ULL, 0ULL, 0ULL, 0);
	restore_registers
	asm volatile("pushl %%eax\n"
		"movl (%%ebp), %%eax\n"
		"movl %%ebx, -0x17c(%%eax)\n"
		"popl %%eax"::);
}
#endif

/*
 * [NSView setNeedsDisplay:]
 */
#if __x86_64__
/*[NSView setNeedsDisplay:]
* 000000000002429c	55              	pushq	%rbp
* 000000000002429d	4889e5          	movq	%rsp, %rbp
* 00000000000242a0	4157            	pushq	%r15
* 00000000000242a2	4156            	pushq	%r14
* 00000000000242a4	53              	pushq	%rbx
* 00000000000242a5	4883ec28        	subq	$0x28, %rsp
* 00000000000242a9	4889fb          	movq	%rdi, %rbx
* 00000000000242ac	84d2            	testb	%dl, %dl
* 00000000000242ae	7442            	je	0x242f2
* 00000000000242b0	488b0561efe800  	movq	0xe8ef61(%rip), %rax
* 00000000000242b7	488b357289e500  	movq	0xe58972(%rip), %rsi
* 00000000000242be	488b4c0318      	movq	0x18(%rbx,%rax), %rcx
*/
void shell_NSView_setNeedsDisplay(void *arg1, void *arg2, void *arg3)
{
	save_registers
	kdebug_trace(NSViewSet, arg1, arg3, 0ULL, 0ULL, 0);
	restore_registers
	asm volatile("movq 0x18(%%rbx, %%rax), %%rcx\n"::);
}
#elif __i386__
/* -[NSView setNeedsDisplay:]:
* 000279fa	55              	pushl	%ebp
* 000279fb	89e5            	movl	%esp, %ebp
* 000279fd	53              	pushl	%ebx
* 000279fe	57              	pushl	%edi
* 000279ff	56              	pushl	%esi
* 00027a00	81ec9c000000    	subl	$0x9c, %esp
* 00027a06	e800000000      	calll	0x27a0b
* 00027a0b	5e              	popl	%esi
* 00027a0c	8b4508          	movl	0x8(%ebp), %eax
* 00027a0f	8a4d10          	movb	0x10(%ebp), %cl
* 00027a12	8b550c          	movl	0xc(%ebp), %edx
* 00027a15	8945f0          	movl	%eax, -0x10(%ebp)
* 00027a18	8955ec          	movl	%edx, -0x14(%ebp)
* 00027a1b	884deb          	movb	%cl, -0x15(%ebp)
* 00027a1e	84c9            	testb	%cl, %cl
* 00027a20	742d            	je	0x27a4f
* 00027a22	8b8e856dd200    	movl	0xd26d85(%esi), %ecx
*/
void shell_NSView_setNeedsDisplay(void *arg1, void *arg2, void *arg3)
{
	save_registers
	kdebug_trace(NSViewSet, arg1, arg3, 0ULL, 0ULL, 0);
	restore_registers
	asm volatile("movl 0xd26d85(%%esi), %%ecx" ::);
}
#endif


void detour(struct mach_o_handler *handler_ptr)
{
#if __x86_64__
	detour_function(handler_ptr, "[NSApplication run]", shell_application_run_wait_event_begin, 0x28c, 6);
	detour_function(handler_ptr, "[NSApplication run]", shell_application_run_wait_event_end, 0x2af, 5);
	detour_function(handler_ptr, "[NSApplication sendEvent:]", shell_sendEvent, 0x44, 7);
	detour_function(handler_ptr, "[NSView setNeedsDisplay:]", shell_NSView_setNeedsDisplay, 0x22, 5);
#elif __i386__
	detour_function(handler_ptr, "[NSApplication sendEvent:]", shell_sendEvent, 0x1e, 6);
	detour_function(handler_ptr, "[NSView setNeedsDisplay:]", shell_NSView_setNeedsDisplay, 0x28, 6);
#endif
}
