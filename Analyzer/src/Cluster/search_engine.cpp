#include "search_engine.hpp"
#include <map>
using namespace std;

#define DEBUG_SEARCH_ENGINE 1

BugSearcher::BugSearcher(clusters_t *cluster_gen)
{
	cluster_map = cluster_gen->get_clusters();
	groups_ptr = cluster_gen->get_groups_ptr();
	ceiling_event = NULL;
	floor_event = NULL;
}

event_t *BugSearcher::search_source_event_in_main_thread(int event_type, event_t *event)
{
	event_t *cur_event, *target_event = NULL;
	list<event_t *> event_list = groups_ptr->get_list_of_tid(groups_ptr->get_main_thread());
	list<event_t *>::reverse_iterator rit;
	for (rit = event_list.rbegin(); rit != event_list.rend(); rit++) {
		cur_event = *rit;
		if (cur_event->get_abstime() < event->get_abstime()
				&& cur_event->get_event_id() == event_type) {
			switch (event_type) {
				case BREAKPOINT_TRAP_EVENT: {
					breakpoint_trap_ev_t *hwbr_event = dynamic_cast<breakpoint_trap_ev_t *>(cur_event);
					if (hwbr_event->get_trigger_var() == "_ZL32sCGEventIsDispatchedToMainThread" && hwbr_event->get_trigger_val() == 0)
						target_event = cur_event;
					break;
				}
				default:
					break;
			}
		}
		if (target_event)
			break;
	}
	return target_event;
}

cluster_t *BugSearcher::search_cluster_overlap(event_t *event)
{
	group_t *group = groups_ptr->group_of(event);

#if DEBUG_SEARCH_ENGINE
	mtx.lock();
	if (group == NULL)
		cerr << "Event at " << fixed << setprecision(1) << event->get_abstime() << "is not grouped" << endl;
	else if (group->get_cluster_idx() == -1)
		cerr << "Event at " << fixed << setprecision(1) << event->get_abstime() << "is not clustered" << endl;
	mtx.unlock();
#endif

	if (group && group->get_cluster_idx() != -1)
		return cluster_map[group->get_cluster_idx()];

	return NULL;
}

cluster_t *BugSearcher::report_spinning_cluster()
{
	list<event_t *> hwbr_list = groups_ptr->get_list_of_op(BREAKPOINT_TRAP);
	list<event_t *>::iterator it;
	for (it = hwbr_list.begin(); it != hwbr_list.end(); it++) {
		breakpoint_trap_ev_t *hwbr_event = dynamic_cast<breakpoint_trap_ev_t*>(*it);
		if (hwbr_event->get_trigger_var() == "_ZL28sCGEventIsMainThreadSpinning"
			&& hwbr_event->get_trigger_val() == 1) {
			floor_event = hwbr_event;
			ceiling_event = search_source_event_in_main_thread(BREAKPOINT_TRAP_EVENT, hwbr_event);
			break;
		}
	}
	if (floor_event && ceiling_event) {
#if DEBUG_SEARCH_ENGINE
	mtx.lock();
	cerr << "Beachball happens between " << fixed << setprecision(1) << ceiling_event->get_abstime() \
		<< " and " << fixed <<setprecision(1) << floor_event->get_abstime() << endl;
	mtx.unlock();
#endif
		return search_cluster_overlap(ceiling_event);
	}
#if DEBUG_SEARCH_ENGINE
	mtx.lock();
	cerr << "No beachball cluster reported" << endl; 
	mtx.unlock();
#endif
	return NULL;
}

template <typename T>
T BugSearcher::max_item_key(map<T, double> &maps, T comb)
{
	double max = 0.0;
	T max_key = comb;
	typename map<T, double>::iterator it;
	for (it = maps.begin(); it != maps.end(); it++) {
		if (it->second > max) {
			max = it->second;
			max_key = it->first;
		}
	}
	return max_key;
}

wait_ev_t *BugSearcher::suspicious_blocking(cluster_t *cluster)
{
	list<wait_ev_t *> &wait_events = cluster->get_wait_events();
 	list<wait_ev_t *>::iterator it;
	list<wait_ev_t *> unknown_blockings;
	map<wait_ev_t *, double> blocking_time_span;
	wait_ev_t *wait_event;

	for (it = wait_events.begin(); it != wait_events.end(); it++) {
		wait_event = *it;
		if (wait_event->get_tid() != groups_ptr->get_main_thread())
			continue;
		syscall_ev_t *syscall_event = wait_event->get_syscall();
		if (syscall_event) {
			if (syscall_event->get_ret_time() > 0)
				blocking_time_span[wait_event] = syscall_event->get_ret_time() - syscall_event->get_abstime();
			else
				unknown_blockings.push_back(wait_event);
		} else {
			unknown_blockings.push_back(wait_event);
		}
	}
#if DEBUG_SEARCH_ENGINE
	mtx.lock();
	for (it = unknown_blockings.begin(); it != unknown_blockings.end(); it++) {
		cerr << "unknown blocking time for wait at " << fixed << setprecision(1) << (*it)->get_abstime() << endl;
	}
	mtx.unlock();
#endif
	return max_item_key(blocking_time_span, (wait_ev_t *)NULL);
}

group_t *BugSearcher::suspicious_segment(cluster_t *cluster)
{
	vector<group_t *> &container = cluster->get_nodes();
	vector<group_t *>::iterator it;
	map<group_t *, double> group_time_span;

	for (it = container.begin(); it != container.end(); it++)
		group_time_span[(*it)] = (*it)->calculate_time_span();

	return max_item_key(group_time_span, (group_t *)NULL);
}
