#include "lib_mach_info.h"
#include <Carbon/Carbon.h>

#define EventRefDebug 0x2bd80104
#if __x86_64__
/*__ZL29CreateAndPostEventWithCGEventP9__CGEventjhP17__CFMachPortBoost:
*000000000003aa8a	55              	pushq	%rbp
*000000000003aa8b	4889e5          	movq	%rsp, %rbp
*000000000003aa8e	4157            	pushq	%r15
*000000000003aa90	4156            	pushq	%r14
*000000000003aab9	48c78590feffff00000000	movq	$0x0, -0x170(%rbp)
*000000000003aac4	488d8d90feffff  	leaq	-0x170(%rbp), %rcx
*000000000003aacb	4889fe          	movq	%rdi, %rsi
*000000000003aace	89c2            	movl	%eax, %edx
*000000000003aad0	e8ca030000      	callq	0x3ae9f
*000000000003aad5	85c0            	testl	%eax, %eax
*000000000003aad7	0f85a5030000    	jne	0x3ae82
*000000000003aadd	4c8bad90feffff  	movq	-0x170(%rbp), %r13
extern OSStatus 
CreateEventWithCGEvent(
  CFAllocatorRef    inAllocator,
  CGEventRef        inEvent,
  EventAttributes   inAttributes,
  EventRef *        outEvent)                                 AVAILABLE_MAC_OS_X_VERSION_10_5_AND_LATER;
*/
OSStatus shell_CreateAndPostEventWithCGEvent(CFAllocatorRef inAllocator, CGEventRef inEvent, EventAttributes inAttributes, EventRef *outEvent, OSStatus ret)
{
	save_registers
	ret = CreateEventWithCGEvent(inAllocator, inEvent, inAttributes, outEvent);
	uint32_t keycode;
	if (GetEventClass(*outEvent) == kEventClassKeyboard
		&& GetEventKind(*outEvent) == kEventRawKeyDown) {
		GetEventParameter(*outEvent, kEventParamKeyCode, typeUInt32,
			NULL, sizeof(keycode), NULL, &keycode);
		kdebug_trace(EventRefDebug, inEvent, (uint64_t)GetEventClass(*outEvent),
			(uint64_t)GetEventKind(*outEvent), (uint64_t)keycode, 0);
	} else
		kdebug_trace(EventRefDebug, inEvent, (uint64_t)GetEventClass(*outEvent),
			(uint64_t)GetEventKind(*outEvent), 0, 0);
	restore_registers
	return ret;
}
#elif __i386__
/*__ZL29CreateAndPostEventWithCGEventP9__CGEventmhP17__CFMachPortBoost:
*001c2207	55              	pushl	%ebp
*001c2208	89e5            	movl	%esp, %ebp
*001c220a	53              	pushl	%ebx
*001c220b	57              	pushl	%edi
*001c220c	56              	pushl	%esi
*001c220d	81ec3c010000    	subl	$0x13c, %esp            ## imm = 0x13C
*001c2213	e800000000      	calll	0x1c2218
*001c2218	5f              	popl	%edi
*001c2219	8b5d08          	movl	0x8(%ebp), %ebx
*001c221c	899de4feffff    	movl	%ebx, -0x11c(%ebp)
*001c2222	c785e0feffff00000000	movl	$0x0, -0x120(%ebp)
*001c222c	8d85e0feffff    	leal	-0x120(%ebp), %eax
*001c2232	8944240c        	movl	%eax, 0xc(%esp)
*001c2236	89542408        	movl	%edx, 0x8(%esp)
*001c223a	894c2404        	movl	%ecx, 0x4(%esp)
*001c223e	e85457e7ff      	calll	0x37997
*001c2243	85c0            	testl	%eax, %eax
*001c2245	0f8575040000    	jne	0x1c26c0
*001c224b	8bb5e0feffff    	movl	-0x120(%ebp), %esi
*/
void shell_CreateAndPostEventWithCGEvent(EventRef inEvent)
{
	save_registers
	asm volatile("movl (%%ebp), %%eax\n"
		"movl -0x120(%%eax), %%esi\n"
		"movl %%esi, %0":"=m"(inEvent):);
	kdebug_trace(EventRefDebug, (uint64_t)inEvent, (uint64_t)GetEventClass(inEvent), 0ULL, 0ULL, 0);
	restore_registers
	asm volatile("movl %0, %%esi"::"m"(inEvent));
}
#endif

#if __x86_64__
/* __ZL18PushToCGEventQueueP9__CGEventjhP17__CFMachPortBoost:
* 0000000000062cd6	55              	pushq	%rbp
* 0000000000062cd7	4889e5          	movq	%rsp, %rbp
* 0000000000062cda	4157            	pushq	%r15
* 0000000000062cdc	4156            	pushq	%r14
* 0000000000062cde	4155            	pushq	%r13
* 0000000000062ce0	4154            	pushq	%r12
* 0000000000062ce2	53              	pushq	%rbx
* 0000000000062ce3	4883ec28        	subq	$0x28, %rsp
* 0000000000062ce7	4989cc          	movq	%rcx, %r12
* 0000000000062cea	8955c8          	movl	%edx, -0x38(%rbp)
* 0000000000062ced	4189f6          	movl	%esi, %r14d
* 0000000000062cf0	4989fd          	movq	%rdi, %r13
* 0000000000062cf3	488d3dc6bc2d00  	leaq	0x2dbcc6(%rip), %rdi
* 0000000000062cfa	e823a22300      	callq	0x29cf22
* 0000000000062cff	4c89ef          	movq	%r13, %rdi
* 0000000000062d02	e8a5a52300      	callq	0x29d2ac
* 0000000000062d07	89c3            	movl	%eax, %ebx
*/
uint64_t shell_PushToCGEventQueue(EventRef inEvent)
{
	save_registers
	kdebug_trace(EventRefDebug, (uint64_t)inEvent, (uint64_t)GetEventClass(inEvent), 1ULL, 0ULL, 0);
	restore_registers
	return CGEventGetType((CGEventRef)inEvent);
}

#elif __i386__
#endif

void detour(struct mach_o_handler *handler_ptr)
{
#if __x86_64__
	detour_function(handler_ptr,
		"_ZL29CreateAndPostEventWithCGEventP9__CGEventjhP17__CFMachPortBoost",
		shell_CreateAndPostEventWithCGEvent, 0x46, 5);
	detour_function(handler_ptr,
		"_ZL18PushToCGEventQueueP9__CGEventjhP17__CFMachPortBoost",
		shell_PushToCGEventQueue, 0x2c, 5);
#elif __i386__
	detour_function(handler_ptr,
		"_ZL29CreateAndPostEventWithCGEventP9__CGEventmhP17__CFMachPortBoost",
		shell_CreateAndPostEventWithCGEvent, 0x44, 6);
#endif
}
