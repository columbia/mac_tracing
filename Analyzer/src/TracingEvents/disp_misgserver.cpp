#include "dispatch.hpp"

DispMigEvent::DispMigEvent(double abstime, string op, uint64_t tid, uint32_t core_id, string procname)
:EventBase(abstime, DISP_MIG_EVENT, op, tid, core_id, procname)
{
}
void DispMigEvent::save_owner(void *_owner)
{
	owner = _owner;
}

void *DispMigEvent::restore_owner(void)
{
	return owner;
}
