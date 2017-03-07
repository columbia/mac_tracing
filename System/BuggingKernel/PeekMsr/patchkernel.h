#ifndef patchkernel_h
#define patchkernel_h
#include "mach_info.h"
#include "include/task.h"
#include "include/proc_internal.h"
#include <mach/mach_types.h>
#include <mach/mach_vm.h>
#include <sys/systm.h>
#include <vm/vm_map.h>
#include <kern/task.h>


void try_dump(unsigned long long text_base);

#endif /* patchkernel_h */
