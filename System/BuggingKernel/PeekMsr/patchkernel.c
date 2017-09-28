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

#define VM_KERNEL_ADDRPERM(_v) \
	(((vm_offset_t)(_v) == 0) ? \
	(vm_offset_t)(0) : (vm_offset_t)(_v) + vm_kernel_addrperm)

#define save_registers { \
	asm volatile("pushq %%rax\n"\
		"pushq %%rbx\n"\
		"pushq %%rdi\n"\
		"pushq %%rsi\n"\
		"pushq %%rdx\n"\
		"pushq %%rcx\n"\
		"pushq %%r8\n"\
		"pushq %%r9\n"\
		"pushq %%r10\n"\
		"pushq %%r12"\
		::);\
	}

#define restore_registers { \
	asm volatile("popq %%r12\n"\
		"popq %%r10\n"\
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

static struct handler hlr = {0};
int (*my_kdebug_trace64)(__unused struct proc *p, void *uap,
	__unused int32_t *retval) = NULL;

static uint64_t kdbg_thrmap_size = 0;
static uint64_t * RAW_file_offset_ptr = NULL;
static uint64_t my_vm_kernel_addrperm = 0;
static uint64_t RAW_file_limit = 512 * 1024 * 1024; //512M filesize default

void shell_kdbg_control()
{
	save_registers
	uint64_t *number_ptr = NULL;
	__asm__ volatile("movq (%%rbp), %%rax\n"
		"lea -0x88(%%rax), %%rbx\n"
		"movq %%rbx, %0"
		:"=m"(number_ptr):);

	if (number_ptr && RAW_file_offset_ptr) {
		if (kdbg_thrmap_size == 0)
			kdbg_thrmap_size = *RAW_file_offset_ptr;

#ifdef DEBUG
		uint64_t val = *RAW_file_offset_ptr;
		printf("RAW_file_offset = 0x%08x%08x\n", (uint32_t)(val >> 32), (uint32_t)val);
		printf("orig trying to write size 0x%08x%08x\n",
			(uint32_t)(*number_ptr >> 32),
			(uint32_t)(*number_ptr));
		printf("size of kdbg_thrmap_size 0x%08x%08x\n",
			(uint32_t)(kdbg_thrmap_size >> 32),
			(uint32_t)kdbg_thrmap_size);
#endif

		if (*RAW_file_offset_ptr >= RAW_file_limit) {
			RAW_file_limit = *RAW_file_offset_ptr;
			*RAW_file_offset_ptr = kdbg_thrmap_size;
		}

		if (*RAW_file_offset_ptr + *number_ptr > RAW_file_limit)
			*number_ptr = RAW_file_limit - *RAW_file_offset_ptr;

#ifdef DEBUG
		val = *RAW_file_offset_ptr;
		printf("RAW_file_offset 0x%08x%08x / 0x%08x%08x\n",
			(uint32_t)(val >> 32), (uint32_t)val,
			(uint32_t)(RAW_file_limit>> 32), (uint32_t)RAW_file_limit);
		printf("trying to write size 0x%08x%08x\n",
			(uint32_t)(*number_ptr >> 32),
			(uint32_t)(*number_ptr));
#endif
	}
	restore_registers
	//simulation "bf0d000207      movl    $0x702000d, %edi  ## imm = 0x702000D"
	__asm__ volatile("movl $0x702000d, %%edi"::);
}

void shell_waitq_assert_wait64_locked()
{
	save_registers
	uint64_t wait_event;
	uint64_t *rbp;
	__asm__ volatile("movq %%rsi, %0\n"
		"movq (%%rbp), %%rax\n"
		"movq %%rax, %1"
		:"=m"(wait_event), "=m"(rbp):);
	if ((int64_t)rbp > VM_MIN_KERNEL_AND_KEXT_ADDRESS) {
		struct {
			uint64_t code;
			uint64_t arg1;
			uint64_t arg2;
			uint64_t arg3;
			uint64_t arg4;
		} my_kd;
		my_kd.code = 0x1400104;
		my_kd.arg1 = VM_KERNEL_ADDRPERM(wait_event);
		rbp = rbp != NULL ? (uint64_t *)(*rbp) : NULL;
		my_kd.arg2 = rbp != NULL? *(rbp + 1) - hlr->seg_text_ptr : 0;
		rbp = rbp != NULL ? (uint64_t *)(*rbp) : NULL;
		my_kd.arg3 = rbp != NULL ? *(rbp + 1) - hlr->seg_text_ptr : 0;
		
		//rbp = rbp != NULL ? (uint64_t *)(*rbp) : NULL;
		//my_kd.arg4 = rbp != NULL ? *(rbp + 1) - hlr->seg_text_ptr : 0;
		my_kd.arg4 = ((struct proc *)(current_task()->bsd_info))->p_pid;
		my_kdebug_trace64(NULL, (void *)&my_kd, NULL);
	}
	restore_registers
	__asm__ volatile("movq 0x20(%%r12), %%rsi"::);
}

#if 0
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
#endif

static int init_code(uint64_t victim_func, uint64_t shell_func, char *invoke_hook)
{
	uint64_t ret_offset = shell_func - (victim_func + 5);
	*(uint32_t*)&invoke_hook[1] = (uint32_t)ret_offset;

	#if DEBUG
	printf("calculated offset: \n\t\tret_offset = 0x%08x%08x\n",
		(uint32_t)(ret_offset >> 32),
		(uint32_t)ret_offset);
	#endif

	return 0;
}

static void patch_func(uint64_t victim_intr, uint64_t shell_func, int32_t size)
{
	char invoke_hook[] =
		"\xe8\x00\x00\x00\x00" /*call shell code*/
		"\x90" /*nop*/
		"\x90" /*nop*/
		"\x90" /*nop*/
		"\x90" /*nop*/
		"\x90" /*nop*/
	;
	init_code(victim_intr, shell_func, invoke_hook);
	disable_wp();
	memcpy(victim_intr, invoke_hook, size);
	enable_wp();
}

struct proc *task_proc(struct task *tsk)
{
	return (struct proc *)(tsk->bsd_info);
}

void try_dump(unsigned long long syscall_entry_addr)
{
	uint64_t text_base = 0;
	void *sym_vm_addr = NULL;
#if 0
	printf("hi64_sysenter = 0x%08x%08x\n", syscall_entry_addr >> 32, syscall_entry_addr);
#endif

	if (!syscall_entry_addr)
		return;
	text_base = syscall_entry_addr - 0x144f90;

#if 0
	printf("dump opcode for hi64_sysenter:\n");
	uint8_t * code = (uint8_t *)(syscall_entry_addr);
	for (int i = 0; i < 16; i++) {
		printf("%02x ", *code);
		code++;
	}

	printf("\ndump opcode in text base:\n");
	code = (uint8_t *)(text_base);
	for (int i = 0; i < 16; i++) {
		printf("%02x ", *code);
		code++;
	}
	printf("\n");
#endif

	hlr.text_base = (void *)text_base;
	prepare_detour(&hlr);
	my_kdebug_trace64 = get_local_sym_in_vm(&hlr, "kdebug_trace64");
	RAW_file_offset_ptr = (uint64_t)get_local_sym_in_vm(&hlr, "RAW_file_offset"); 
	my_vm_kernel_addrperm = *(uint64_t *)get_local_sym_in_vm(&hlr, "vm_kernel_addrperm");

#ifdef DEBUG
	printf("vm_kernel_addrperm is 0x%08x%08x\n", (uint32_t)(my_vm_kernel_addrperm >> 32),
		(uint32_t)my_vm_kernel_addrperm);
#endif

#if 0
	/*sample code of kernel instrument  */
	sym_vm_addr = get_local_sym_in_vm(&hlr, "ipc_kmsg_copyin");
	if (!sym_vm_addr) {
#ifdef DEBUG
		printf("ipc_kmsg_copyin is not found\n");
#endif
		return;
	}
	/*
	init_code(sym_vm_addr + 0x17, shell_ipc_kmsg_copyin);
	disable_wp();
	memcpy(sym_vm_addr + 0x17, invoke_hook, 6);
	enable_wp();
	*/
#endif
	//ring buffer for trace collection
	sym_vm_addr = get_local_sym_in_vm(&hlr, "kdbg_control");
	if (!sym_vm_addr) {
#ifdef DEBUG
		printf("kdbg_control is not found\n");
#endif
		return;
	}
	patch_func(sym_vm_addr + 0xe09, shell_kdbg_control, 5);
	//add wait reason (kernel callstack) before wait event with tid-pid map
	sym_vm_addr = get_local_sym_in_vm(&hlr,"waitq_assert_wait64_locked");
	if (!sym_vm_addr) {
#ifdef DEBUG
		printf("waitq_assert_wait64_locked is not found\n");
#endif
		return;
	}
	patch_func(sym_vm_addr + 0x37, shell_waitq_assert_wait64_locked, 5);
	sym_vm_addr = 0;
}
