#include "thread_divider.hpp"
//////////////////////////////////////////////
//event schemas for divider
/////////////////////////////////////////////
void ThreadDivider::add_tsm_event_to_group(event_t *event)
{
	/* isolate time share maintainance_makerunnable
	 * if called in csw, following the wait event
	 * if called in quntum timer expiration, following timer/??? interrupt
	 * both are followd by mkrunnable, close group after mkrunnable
	 */  
	tsm_ev_t *tsm_event = dynamic_cast<tsm_ev_t *>(event);
	tsm_event->save_gptr((void *)(cur_group));
	cur_group = create_group(gid_base + ret_map.size(), tsm_event);
	ret_map[cur_group->get_group_id()] = cur_group;
}

void ThreadDivider::add_mr_event_to_group(event_t *event)
{
	mkrun_ev_t *mr_event = dynamic_cast<mkrun_ev_t *>(event);
	event_t *last_event = cur_group ? cur_group->get_last_event() : NULL;
	uint64_t mr_type = mr_event->check_mr_type(last_event, potential_root);
	switch (mr_type) {
		case SCHED_TSM_MR: {
			/* processing sched_timeshare_maintainance_mkrun
			 * [delayed] close the cur_group and reload stored group
			 */
			add_general_event_to_group(event);
			tsm_ev_t *tsm = dynamic_cast<tsm_ev_t *>(last_event);
			cur_group = ((group_t *)(tsm->load_gptr()));
			break;
		}
		case INTR_MR: { 
			/* 1. Intr and Mr events will be in a new group
			 * save and restore the previous active group
			 * 2. Do not touch potental_root
			 * one intr may triger multiple mkrunnable
			 * 3. Check the case that an active thread being waken up again
			 */
			group_t *saved_cur_group = cur_group;
			cur_group = create_group(gid_base + ret_map.size(), potential_root);
			ret_map[cur_group->get_group_id()] = cur_group;
			add_general_event_to_group(event);
			cur_group = saved_cur_group;
			break;
		}
		default:
			add_general_event_to_group(event);
			break;
	}
}
