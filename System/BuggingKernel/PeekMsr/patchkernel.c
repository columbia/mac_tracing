#include "patchkernel.h"
#define DEBUG 1

#define CR0_WP  0x0000000000010000  /* i486: Write-protect kernel access */
static inline uintptr_t get_cr0(void)
{                        
	uintptr_t cr0;       
	__asm__ volatile("mov %%cr0, %0" : "=r" (cr0));
	return(cr0);         
}

static inline void set_cr0(uintptr_t value)
{                       
	__asm__ volatile("mov %0, %%cr0" : : "r" (value));
}

void disable_wp()
{
	__asm__ volatile("cli");
	set_cr0(get_cr0() & (~CR0_WP));
}

void enable_wp()
{
	set_cr0(get_cr0() | CR0_WP);
	__asm__ volatile("sti");
}

#define save_registers { \
	asm volatile("pushq %%rax\n"\
		"pushq %%rbx\n"\
		"pushq %%rdi\n"\
		"pushq %%rsi\n"\
		"pushq %%rdx\n"\
		"pushq %%rcx\n"\
		"pushq %%r8\n"\
		"pushq %%r9\n"\
		"pushq %%r10"\
		::);\
	}

#define restore_registers { \
	asm volatile("popq %%r10\n"\
		"popq %%r9\n"\
		"popq %%r8\n"\
		"popq %%rcx\n"\
		"popq %%rdx\n"\
		"popq %%rsi\n"\
		"popq %%rdi\n"\
		"popq %%rbx\n"\
		"popq %%rax"\
		::);\
	}

int (*my_kdebug_trace64)(__unused struct proc *p, void *uap, __unused int32_t *retval) = NULL;

void shell_ipc_kmsg_copyin()
{
	__asm__ volatile("andl $0x801f1f1f, (%%rax)"::);
	save_registers
	struct {
		uint64_t code;
		uint64_t arg1;
		uint64_t arg2;
		uint64_t arg3;
		uint64_t arg4;
	} my_kd = {0x121364, 0xf, 0xf, 0xf, 0xf};
	if (my_kdebug_trace64 != NULL) {
		my_kdebug_trace64(NULL, (void *)&my_kd, NULL);
		printf("check code ipc_kmsg_copyin %d\n",
			((struct proc *)(current_task()->bsd_info))->p_pid);
	}
	restore_registers
}

static char invoke_hook[] =
		"\xe8\xcc\x01\x00\x00" /*call shell code*/
		"\x90" /*nop*/
		"\x90" /*nop*/
		"\x90" /*nop*/
		"\x90" /*nop*/
		"\x90" /*nop*/
;

static int init_code(uint64_t victim_func, uint64_t shell_func)
{
	uint64_t ret_offset = shell_func - (victim_func + 5);
	*(uint32_t*)&invoke_hook[1] = (uint32_t)ret_offset;

	#if DEBUG
	printf("calculated offset: \n");
	printf("\t\tret_offset = %llx\n", ret_offset);
	#endif

	return 0;
}

struct handler hlr = {0};

struct proc * task_proc(struct task * tsk)
{
	return (struct proc *)(tsk->bsd_info);
}

void try_dump(unsigned long long syscall_entry_addr)
{
	uint64_t text_base = 0;
	#ifdef DEBUG
	printf("hi64_sysenter = 0x%08x%08x\n", syscall_entry_addr >> 32, syscall_entry_addr);
	#endif

	if (syscall_entry_addr != 0) {
		text_base = syscall_entry_addr - 0x144f90;

	#ifdef DEBUG
		printf("dump opcode for hi64_sysenter:\n");
		uint8_t * code = (uint8_t *)(syscall_entry_addr);
		for (int i = 0; i < 16; i++) {
			printf("%02x ", *code);
			code++;
		}
		printf("\n");

		printf("dump opcode in text base:\n");
		code = (uint8_t *)(text_base);
		for (int i = 0; i < 16; i++) {
			printf("%02x ", *code);
			code++;
		}
		printf("\n");
	#endif
	}

	hlr.text_base = (void *)text_base;
	prepare_detour(&hlr);
	my_kdebug_trace64 = get_local_sym_in_vm(&hlr, "kdebug_trace64");

	/*sample code of kernel instrument*/
	void * sym_vm_addr = get_local_sym_in_vm(&hlr, "ipc_kmsg_copyin");
	init_code(sym_vm_addr + 0x17, shell_ipc_kmsg_copyin);
	disable_wp();
	memcpy(sym_vm_addr + 0x17, invoke_hook, 6);
	enable_wp();
}
