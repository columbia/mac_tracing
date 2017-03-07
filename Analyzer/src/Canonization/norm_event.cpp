#include "canonization.hpp"

NormEvent::NormEvent(event_t * e, uint64_t v_tid)
{
	event = e;
	proc_name = e->get_procname();
	optype = e->get_op();
	virtual_tid = v_tid;
}

bool NormEvent::operator==(NormEvent & other)
{
	if (proc_name == other.proc_name
		&& optype == other.optype) {
	//	&& virtual_tid == other.virtual_tid) {
		return true;
	}
	return false;
}

bool NormEvent::operator!=(NormEvent & other)
{
	return !(*this == other);
}

event_t * NormEvent::get_real_event(void)
{
	return event;
}
