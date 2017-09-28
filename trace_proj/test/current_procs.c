#include <unistd.h>
#include <stdlib.h>
#include <mach/mach.h>
#include <libproc.h>
#include <mach/mach_time.h>
#include <sys/sysctl.h>
#include <mach-o/ldsyms.h>
#include <stdio.h>

void procpid(pid_t pid)
{
	struct proc_bsdinfo proc;
	int st = proc_pidinfo(pid, PROC_PIDTBSDINFO, 0, &proc, PROC_PIDTBSDINFO_SIZE);
	if (st == PROC_PIDTBSDINFO_SIZE)
		printf("%d\t%s\n", pid, proc.pbi_comm);
}

void pidlist(void)
{
	int bufsize = proc_listpids(PROC_ALL_PIDS, 0, NULL, 0);
	pid_t * procs = (pid_t *)malloc(bufsize * 2);
	bufsize = proc_listpids(PROC_ALL_PIDS, 0, procs, bufsize << 1);

	int num_pids = bufsize / sizeof(pid_t);
	for (int i = 0; i < num_pids; i++) {
		procpid(procs[i]);
	}

	free(procs);
}

int main(void)
{ 
	pidlist();
	return 0;
}
