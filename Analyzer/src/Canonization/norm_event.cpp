#include "canonization.hpp"

NormEvent::NormEvent(event_t *_event)
{
	event = _event;
	peer = -1;
	proc_name = event->get_procname();
	event_type = event->get_event_id();

	if (event->get_event_peer())
		peer = event->get_event_peer()->get_pid();
}

bool NormEvent::operator==(NormEvent &other)
{
	if (proc_name != other.proc_name)
		return false;
	if (event_type != other.event_type)
		return false;
	
	if (peer != -1 || other.peer != -1){
		if (peer != other.peer)
			return false;
	}

	bool ret = true;
	event_t *peer = other.get_real_event();
	switch(event_type) {
		case SYSCALL_EVENT: {
			syscall_ev_t *cur_event = dynamic_cast<syscall_ev_t *>(event);
			syscall_ev_t *peer_event = dynamic_cast<syscall_ev_t *>(peer);
			if (cur_event->get_entry() != peer_event->get_entry())
				ret = false;	
			break;
		}
		case RL_BOUNDARY_EVENT:	{
			rl_boundary_ev_t *cur_event = dynamic_cast<rl_boundary_ev_t *>(event);
			rl_boundary_ev_t *peer_event = dynamic_cast<rl_boundary_ev_t *>(peer);
			if (cur_event->get_state() != peer_event->get_state())
				ret = false;
			if (cur_event->get_rls() != peer_event->get_rls())
				ret = false;
			if (cur_event->get_func_ptr() != peer_event->get_func_ptr())
				ret = false;
			break;
		} 

		case EVENTREF_EVENT: {
			event_ref_ev_t *cur_event = dynamic_cast<event_ref_ev_t *>(event);
			event_ref_ev_t *peer_event = dynamic_cast<event_ref_ev_t *>(peer);
			if (cur_event->get_class() != peer_event->get_class())
				ret = false;
			if (cur_event->get_kind() != peer_event->get_kind())
				ret = false;
			break;
		}

		case NSAPPEVENT_EVENT: {
			nsapp_event_ev_t *cur_event = dynamic_cast<nsapp_event_ev_t *>(event);
			nsapp_event_ev_t *peer_event = dynamic_cast<nsapp_event_ev_t *>(peer);
			if (cur_event->is_begin() != peer_event->is_begin())
				ret = false;
			if (cur_event->get_event_class() != peer_event->get_event_class())
				ret = false;
			break;
		}
		default:
			break;
	}	
	return ret;
}

bool NormEvent::operator!=(NormEvent & other)
{
	return !(*this == other);
}

event_t * NormEvent::get_real_event(void)
{
	return event;
}
