#include "lib_mach_info.h"
#include <pthread.h>

#if defined(__LP64__)
static int (*orig_pthread_cond_init)(pthread_cond_t *ocond, const pthread_condattr_t *attr) = NULL;

int pthread_cond_init(pthread_cond_t *ocond, const pthread_condattr_t *attr)
{
	if (orig_pthread_cond_init == NULL) {
		orig_pthread_cond_init = get_func_ptr_from_lib(pthread_getname_np, "pthread_cond_init");
	}

	if (orig_pthread_cond_init)
		return orig_pthread_cond_init(ocond, attr);

	else {
		perror("pthread_cond_init is not load\n");
		exit(EXIT_FAILURE);
	}
}
#endif
