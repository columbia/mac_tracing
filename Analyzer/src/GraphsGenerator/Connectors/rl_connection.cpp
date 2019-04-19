#include "rl_connection.hpp"
#include "mach_msg.hpp"
#include "eventlistop.hpp"

#define RL_BOUNDARY_DEBUG 1

RLConnection::RLConnection(list<event_t *> &_rl_item_list, tid_evlist_t &_tid_lists)
:rl_item_list(_rl_item_list), tid_lists(_tid_lists)
{
}

void RLConnection::connect_for_source1(rl_boundary_ev_t *rl_boundary_event)
{
	
	/* Not need because of source1 will connect with port message
	list<event_t *> cur_tid_list = tid_lists[rl_boundary_event->get_tid()];
	list<event_t *>::reverse_iterator cur_rpos(find(cur_tid_list.begin(), cur_tid_list.end(), rl_boundary_event));
	for (; cur_rpos != cur_tid_list.rend(); cur_rpos++) {
		if ((*cur_rpos)->get_event_id() != MSG_EVENT)
			continue;
		msg_ev_t *msg_event = dynamic_cast<msg_ev_t *>(*cur_rpos);
		if (!msg_event->get_header()->is_mig() && msg_event->get_header()->check_recv()) {
			rl_boundary_event->set_owner(*cur_rpos);
			break;
		}
	}
#if RL_BOUNDARY_DEBUG
	if (cur_rpos == cur_tid_list.rend()) {
		mtx.lock();
		cerr << "No matching event found for Source1 " << fixed << setprecision(1) << rl_boundary_event->get_abstime() << endl;
		mtx.unlock();
	}
#endif
	*/
}

void RLConnection::connect_for_source0(rl_boundary_ev_t *rl_boundary_event, list<event_t *>::reverse_iterator rit)
{
	rl_boundary_ev_t *rl_event = NULL, *unset_sig_event = NULL, *set_sig_event = NULL;
	for (; rit != rl_item_list.rend(); rit++) {
		rl_event = dynamic_cast<rl_boundary_ev_t *>(*rit);
		if (!rl_event)
			continue;
		if (unset_sig_event == NULL) {
			if (rl_event->get_state() != UnsetSignalForSource0)
				continue;
			else
				unset_sig_event = rl_event;
		} else if (rl_event->get_state() == SetSignalForSource0 && unset_sig_event->get_rls() == rl_event->get_rls()) {
			rl_boundary_event->set_owner(*rit);
			assert(rl_event == *rit);
			rl_event->set_consumer(rl_boundary_event);
			break;
		}
	}
#if RL_BOUNDARY_DEBUG
	if (rit == rl_item_list.rend()) {
		mtx.lock();
		cerr << "No matching event found for Source0 " << fixed << setprecision(1) << rl_boundary_event->get_abstime() << endl;
		mtx.unlock();
	}
#endif
}

void RLConnection::connect_for_blocks(rl_boundary_ev_t *rl_boundary_event, list<event_t *>::reverse_iterator rit)
{
	rl_boundary_ev_t *block_orig = NULL;
	for (; rit != rl_item_list.rend(); rit++) {
		block_orig = dynamic_cast<rl_boundary_ev_t *>(*rit);
		if (!block_orig || block_orig->get_state() != BlockPerform)
			continue;
		if (block_orig->get_block() == rl_boundary_event->get_block()) {
			rl_boundary_event->set_owner(block_orig);
			block_orig->set_consumer(rl_boundary_event);
			break;
		}
	}
#if RL_BOUNDARY_DEBUG
	if (rit == rl_item_list.rend()) {
		mtx.lock();
		cerr << "No matching event found for Block " << fixed << setprecision(1) << rl_boundary_event->get_abstime() << endl;
		mtx.unlock();
	}
#endif
}

void RLConnection::rl_connection(void)
{
	list<event_t *>::iterator it;
	EventLists::sort_event_list(rl_item_list);
	for (it = rl_item_list.begin(); it != rl_item_list.end(); it++) {
		rl_boundary_ev_t *rl_boundary_event = dynamic_cast<rl_boundary_ev_t *>(*it);
		assert(rl_boundary_event);
		if (rl_boundary_event->get_state() == ItemEnd)
			continue;

		if (rl_boundary_event->get_op() == "RL_DoSource1") {
			// the last message recv(not mig) in the same thread and set_owner
			if (rl_boundary_event->get_state() == ItemBegin)
				connect_for_source1(rl_boundary_event);
		} else if (rl_boundary_event->get_op() == "RL_DoSource0") {
			// RunLoopWakeup event that wake up current thread
			if (rl_boundary_event->get_state() == ItemBegin) {
				list<event_t *>::reverse_iterator rit(it);
				connect_for_source0(rl_boundary_event, rit);
			}
		} else if (rl_boundary_event->get_op() == "RL_DoTimer") {
			// exclude timers added by the runloop, leaving the ones from application

		} else if (rl_boundary_event->get_op() == "RL_DoBlocks") {
			if (rl_boundary_event->get_state() == ItemBegin) {
				list<event_t *>::reverse_iterator rit(it);
				connect_for_blocks(rl_boundary_event, rit);
			}
		} 
	}
}
