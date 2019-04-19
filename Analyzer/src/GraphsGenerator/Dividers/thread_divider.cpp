#include "thread_divider.hpp"

#define DEBUG_THREAD_DIVIDER 1
ThreadDivider::ThreadDivider(int _index, map<uint64_t, map<uint64_t, group_t *> >&sub_results, list<event_t *> ev_list)
:submit_result(sub_results)
{
	gid_base = ev_list.front()->get_tid() << GROUP_ID_BITS;
	cur_group = NULL;
	potential_root = NULL;
	backtrace_for_hook = NULL;
	voucher_for_hook = NULL;
	syscall_event = NULL;
	pending_msg_sent = NULL;
	dequeue_event = NULL;
	faked_wake_event = NULL;
	
	ret_map.clear();
	tid_list = ev_list;
	index = _index;
}

group_t *ThreadDivider::create_group(uint64_t id, event_t *root_event)
{
	group_t *new_gptr = new group_t(id, root_event);

	if (new_gptr == NULL) {
#if DEBUG_THREAD_DIVIDER
		mtx.lock();
		cerr << "Error: OOM can not create a new group." << endl;
		mtx.unlock();
#endif
		exit(EXIT_FAILURE);
	}

	if (root_event) 
		new_gptr->add_to_container(root_event);
	return new_gptr;
}

group_t *ThreadDivider::gid2group(uint64_t gid)
{
	if (ret_map.find(gid) != ret_map.end())
		return ret_map[gid];
	return NULL;
}

void ThreadDivider::delete_group(group_t *del_group)
{
	del_group->empty_container();
	delete(del_group);
}

void ThreadDivider::add_general_event_to_group(event_t *event)
{
	if (!cur_group) {
		cur_group = create_group(gid_base + ret_map.size(), NULL);
		ret_map[cur_group->get_group_id()] = cur_group;
	}

	if (faked_wake_event) {
		cur_group->add_to_container(faked_wake_event);
		faked_wake_event = NULL;
	}
	cur_group->add_to_container(event);
}

void ThreadDivider::store_event_to_group_handler(event_t *event)
{
	switch (event->get_event_id()) {
		case INTR_EVENT:
			potential_root = dynamic_cast<intr_ev_t *>(event);
			break;
		case BACKTRACE_EVENT:
			backtrace_for_hook = dynamic_cast<backtrace_ev_t *>(event);
			break;
		case VOUCHER_EVENT:
			voucher_for_hook = dynamic_cast<voucher_ev_t *>(event);
			break;
		case SYSCALL_EVENT:
			syscall_event = dynamic_cast<syscall_ev_t *>(event);
			if (event->get_op() == "MSC_mach_msg_overwrite_trap")
				pending_msg_sent = syscall_event;
			break;
		case DISP_DEQ_EVENT:
			dequeue_event = dynamic_cast<dequeue_ev_t *>(event);
			break;
		case FAKED_WOKEN_EVENT:
			faked_wake_event = dynamic_cast<fakedwoken_ev_t *>(event);
			break;
		default:
			break;
	}
}

/* general group generation per thread */
void ThreadDivider::divide()
{
	list<event_t *>::iterator it;
	event_t *event = NULL;
	for (it = tid_list.begin(); it != tid_list.end(); it++) {
		event = *it;
		switch (event->get_event_id()) {
			case VOUCHER_CONN_EVENT:
			case VOUCHER_DEALLOC_EVENT:
			case VOUCHER_TRANS_EVENT:
				break;
			case SYSCALL_EVENT:
				if (event->get_op() == "BSC_sigreturn")
					break;
			case INTR_EVENT:
				add_general_event_to_group(event);
			case BACKTRACE_EVENT:
			case VOUCHER_EVENT:
			case FAKED_WOKEN_EVENT:
			//case DISP_DEQ_EVENT:
				store_event_to_group_handler(event);
				break;
			case TSM_EVENT:
				add_tsm_event_to_group(event);
				break;
			case MR_EVENT:
				add_mr_event_to_group(event);
				break;
			case WAIT_EVENT:
				add_wait_event_to_group(event);
				break;
			case TMCALL_CALLOUT_EVENT:
				add_timercallout_event_to_group(event);
				break;
			case DISP_ENQ_EVENT: {
				add_general_event_to_group(event);
				enqueue_ev_t *enqueue_event = dynamic_cast<enqueue_ev_t *>(event);
				enqueue_event->set_nested_level(cur_group->get_blockinvoke_level());
				break;
			}
			case DISP_DEQ_EVENT: {
				dequeue_ev_t *dequeue_event = dynamic_cast<dequeue_ev_t *>(event);
				
				if (dequeue_event->is_executed()) {
					store_event_to_group_handler(event);
				} else {
					add_general_event_to_group(event);
					dequeue_event->set_nested_level(cur_group->get_blockinvoke_level());
				}
				
				//add_general_event_to_group(event);
				//dequeue_event->set_nested_level(cur_group->get_blockinvoke_level());
				break;
			}
			case DISP_INV_EVENT:
				add_disp_invoke_event_to_group(event);
				break;
			case DISP_MIG_EVENT: {
				add_disp_mig_event_to_group(event);
				break;
			}
			case MSG_EVENT:
				add_msg_event_into_group(event);
				store_event_to_group_handler(event);
				break;
			case BREAKPOINT_TRAP_EVENT:
				add_hwbr_event_into_group(event);
				break;
			default:
				add_general_event_to_group(event);
		}
	}
	submit_result[index] = ret_map;
}

void ThreadDivider::decode_groups(map<uint64_t, group_t *> & uievent_groups, string filepath)
{
	ofstream output(filepath);
	if (output.fail()) {
		mtx.lock();
		cerr << "Error: unable to open file " << filepath << " for write " << endl;
		mtx.unlock();
		return;
	}
	map<uint64_t, group_t *>::iterator it;
	group_t * cur_group;
	uint64_t index = 0;
	for (it = uievent_groups.begin(); it != uievent_groups.end(); it++) {
		output << "#Group " << hex << index << endl;
		index++;
		cur_group = it->second;
		cur_group->decode_group(output);
	}
	output.close();
}

void ThreadDivider::streamout_groups(map<uint64_t, group_t *> & uievent_groups, string filepath)
{
	ofstream output(filepath);
	if (output.fail()) {
		mtx.lock();
		cerr << "Error: unable to open file " << filepath << " for write " << endl;
		mtx.unlock();
		return;
	}
	map<uint64_t, group_t *>::iterator it;
	group_t * cur_group;
	uint64_t index = 0;
	for (it = uievent_groups.begin(); it != uievent_groups.end(); it++) {
		output << "#Group " << hex << index << endl;
		index++;
		cur_group = it->second;
		cur_group->streamout_group(output);
	}
	output.close();
}
