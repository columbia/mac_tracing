#include "parser.hpp"
#include "cluster.hpp"

#define DEBUG_EXTRACT_CLUSTER 1

ClusterGen::ClusterGen(groups_t *groups, bool connect_mkrun)
{
	groups_ptr = groups;
	map<uint64_t, group_t *> main_groups = groups_ptr->get_main_groups();
	map<uint64_t, group_t *>::iterator it;

	cerr << "Sizeof main_groups " << main_groups.size() << endl;
	for (it = main_groups.begin(); it != main_groups.end(); it++) {
		nsapp_event_ev_t *init_event = dynamic_cast<nsapp_event_ev_t *>(it->second->contain_nsappevent());
		if (init_event && init_event->is_begin() == false) {
			if (it->second->get_cluster_idx() != -1) {
#if DEBUG_EXTRACT_CLUSTER
				//TODO: decode the user event for checking
				cerr << "Check: group with group id " << hex << it->second->get_group_id()\
					<< " has been added into cluster " << hex << it->second->get_cluster_idx() << endl;
#endif
				continue;
			}
			cluster_t *new_cluster = init_cluster(it->second);
			clusters[it->first] = new_cluster;

#ifdef DEBUG_EXTRACT_CLUSTER
			cerr << "Augment cluster from group " <<  it->second->get_group_id() << ", with root event at "\
				<< fixed << setprecision(1) << init_event->get_abstime() << endl;
#endif
			augment_cluster(new_cluster, connect_mkrun);
			add_waitsync(new_cluster);
		} else {

#if DEBUG_EXTRACT_CLUSTER
			if (it->second->get_cluster_idx() == -1) {
				cerr << "Check: group from main thread " << hex << it->second->get_group_id()\
					<< " was neither augmented as root nor added to clusters" << endl;
			}
#endif
		}
	}
}

ClusterGen::~ClusterGen()
{
	map<uint64_t, cluster_t *>::iterator cluster_it;
	for (cluster_it = clusters.begin(); cluster_it != clusters.end(); cluster_it++) {
		if (cluster_it->second)
			delete(cluster_it->second);
	}
	clusters.clear();
}

cluster_t *ClusterGen::init_cluster(group_t *group)
{
	cluster_t *ret = new Cluster(group);
	ret->push_connectors(group, NULL);
	return ret;
}

cluster_t *ClusterGen::cluster_of(group_t *g)
{
	if (!g || g->get_cluster_idx() == (uint64_t)-1)
		return NULL;

	if (clusters.find(g->get_cluster_idx()) != clusters.end())
		return clusters[g->get_cluster_idx()];

	mtx.lock();
	cerr << "Error: cluster with index " << g->get_cluster_idx() << " not found." << endl;
	mtx.unlock();

	return NULL;
}

void ClusterGen::merge_by_mach_msg(cluster_t *cluster, msg_ev_t *curr_msg)
{
	msg_ev_t *next_msg = curr_msg->get_next();

	if (next_msg) {
		group_t *next_group = groups_ptr->group_of(next_msg);
		assert(next_group);

#if DEBUG_EXTRACT_CLUSTER
		{
		group_t *peer_group = next_group;
		map<uint64_t, group_t *> main_groups = groups_ptr->get_main_groups();
		if (peer_group->get_cluster_idx() != cluster->get_cluster_id()
				&& main_groups.find(peer_group->get_group_id()) != main_groups.end()) {
			nsapp_event_ev_t *init_event = dynamic_cast<nsapp_event_ev_t *>(peer_group->contain_nsappevent());
			mtx.lock();
			if (init_event && init_event->is_begin() == false)
				cerr << "Event at " << fixed << setprecision(1) << curr_msg->get_abstime()\
					<< " Tries to connect to Root Group " << hex << peer_group->get_group_id()\
					<< " with app event at " << fixed << setprecision(1) << init_event->get_abstime() << endl;
			mtx.unlock();
		}
		}
#endif

		if (next_group->get_cluster_idx() == cluster->get_cluster_id()) {
			cluster->add_edge(groups_ptr->group_of(curr_msg),
				next_group,
				curr_msg,
				next_msg,
				MSGP_REL);
		} else if (next_group->get_cluster_idx() != -1) {
#if DEBUG_EXTRACT_CLUSTER
			mtx.lock();
			cerr << __func__ <<": cluster " << hex << cluster->get_cluster_id() \
				<< " event at " << fixed << setprecision(1) << curr_msg->get_abstime() << endl;
			cerr << "group " << hex << next_group->get_group_id() \
				<< " with event at " << fixed << setprecision(1) << next_msg->get_abstime() \
				<< " was added into cluster " << hex << next_group->get_cluster_idx() << endl;
			mtx.unlock();
#endif
		} else {
			cluster->add_edge(groups_ptr->group_of(curr_msg),
				next_group,
				curr_msg,
				next_msg,
				MSGP_REL);
			if (cluster->add_node(next_group))
				cluster->push_connectors(next_group, next_msg);
		}
	}

	msg_ev_t *peer_msg = curr_msg->get_peer();
	if (peer_msg) {
		group_t *peer_group = groups_ptr->group_of(peer_msg);
		if (peer_group == NULL) {
#if DEBUG_EXTRACT_CLUSTER
			mtx.lock();
			cerr << __func__ <<": event at " << fixed << setprecision(1) \
				<< peer_msg->get_abstime() << " is not in group properly" << endl;
			mtx.unlock();
#endif
		}
		assert(peer_group);

#if DEBUG_EXTRACT_CLUSTER
		map<uint64_t, group_t *> main_groups = groups_ptr->get_main_groups();
		if (peer_group->get_cluster_idx() != cluster->get_cluster_id()
				&& main_groups.find(peer_group->get_group_id()) != main_groups.end()) {
			nsapp_event_ev_t *init_event = dynamic_cast<nsapp_event_ev_t *>(peer_group->contain_nsappevent());
			mtx.lock();
			if (init_event && init_event->is_begin() == false)
				cerr << "Event at " << fixed << setprecision(1) << curr_msg->get_abstime()\
					<< " Tries to connect to Root Group " << hex << peer_group->get_group_id()\
					<< " with app event at " << fixed << setprecision(1) << init_event->get_abstime() << endl;
			mtx.unlock();
		}
#endif
		if (peer_group->get_cluster_idx() == cluster->get_cluster_id()) {
			if (peer_msg->get_abstime() > curr_msg->get_abstime())
				cluster->add_edge(groups_ptr->group_of(curr_msg),
					peer_group,
					curr_msg,
					peer_msg,
					MSGP_REL);
			else
				cluster->add_edge(peer_group,
					groups_ptr->group_of(curr_msg),
					peer_msg,
					curr_msg,
					MSGP_REL);
		} else if (peer_group->get_cluster_idx() != -1) {
#if DEBUG_EXTRACT_CLUSTER
			mtx.lock();
			cerr << __func__ <<": cluster " << hex << cluster->get_cluster_id() \
				<< " event at " << fixed << setprecision(1) << curr_msg->get_abstime() << endl;
			cerr << "group " << hex << peer_group->get_group_id() \
				<< " with event at " << fixed << setprecision(1) << peer_msg->get_abstime() \
				<< " was added into cluster " << hex << peer_group->get_cluster_idx() << endl;
			mtx.unlock();
#endif
		} else {
			if (peer_msg->get_abstime() > curr_msg->get_abstime())
				cluster->add_edge(groups_ptr->group_of(curr_msg),
					peer_group,
					curr_msg,
					peer_msg,
					MSGP_REL);
			else
				cluster->add_edge(peer_group,
					groups_ptr->group_of(curr_msg),
					peer_msg,
					curr_msg,
					MSGP_REL);
			if (cluster->add_node(peer_group))
				cluster->push_connectors(peer_group, peer_msg);
		}
	}
}

void ClusterGen::merge_by_dispatch_ops(cluster_t *cluster, enqueue_ev_t *enqueue)
{
	if (!enqueue->is_consumed())
		return;
	event_t *deq = enqueue->get_consumer();
	if (!deq) {
#if DEBUG_EXTRACT_CLUSTER
		mtx.lock();
		cerr << "Error: no dequeue event for enqueue but with comsume flag set\n";
		mtx.unlock();
#endif
		return;
	}

	group_t *deq_group = groups_ptr->group_of(deq);
	if (deq_group == NULL) {
#if DEBUG_EXTRACT_CLUSTER
		mtx.lock();
		cerr << "No grouped deq event at " << fixed << setprecision(1) << deq->get_abstime() << endl;
		mtx.unlock();
#endif
		return;
	}

#if DEBUG_EXTRACT_CLUSTER
	group_t *peer_group = deq_group;
	map<uint64_t, group_t *> main_groups = groups_ptr->get_main_groups();
	if (peer_group->get_cluster_idx() != cluster->get_cluster_id()
			&& main_groups.find(peer_group->get_group_id()) != main_groups.end()) {
		nsapp_event_ev_t *init_event = dynamic_cast<nsapp_event_ev_t *>(peer_group->contain_nsappevent());
		mtx.lock();
		if (init_event && init_event->is_begin() == false)
			cerr << "Event at " << fixed << setprecision(1) << enqueue->get_abstime()\
				<< " Tries to connect to Root Group " << hex << peer_group->get_group_id()\
				<< " with app event at " << fixed << setprecision(1) << init_event->get_abstime() << endl;
		mtx.unlock();
	}
#endif

	if (deq_group->get_cluster_idx() == cluster->get_cluster_id()) {
		cluster->add_edge(groups_ptr->group_of(enqueue),
				deq_group,
				enqueue,
				deq,
				DISP_DEQ_REL);
	} else if (deq_group->get_cluster_idx() != -1) {
#if DEBUG_EXTRACT_CLUSTER
		mtx.lock();
		cerr << __func__ <<": cluster " << hex << cluster->get_cluster_id() \
			<< " event at " << fixed << setprecision(1) << enqueue->get_abstime() << endl;
		cerr << " group " << hex << deq_group->get_group_id() \
			<< " with event at " << fixed << setprecision(1) << deq->get_abstime() \
			<< " was added into cluster " << hex << deq_group->get_cluster_idx() << endl;
		mtx.unlock();
#endif
	} else {
		cluster->add_edge(groups_ptr->group_of(enqueue),
				deq_group,
				enqueue,
				deq,
				DISP_DEQ_REL);
		if (cluster->add_node(deq_group))
			cluster->push_connectors(deq_group, deq);
	}
}

void ClusterGen::merge_by_dispatch_ops(cluster_t *cluster, dequeue_ev_t *dequeue)
{
	event_t *invoke = dequeue->get_invoke();
	if (!invoke)
		return;
	group_t *invoke_group = groups_ptr->group_of(invoke);

#if DEBUG_EXTRACT_CLUSTER
	group_t *peer_group = invoke_group;
	map<uint64_t, group_t *> main_groups = groups_ptr->get_main_groups();
	if (peer_group->get_cluster_idx() != cluster->get_cluster_id()
			&& main_groups.find(peer_group->get_group_id()) != main_groups.end()) {
		nsapp_event_ev_t *init_event = dynamic_cast<nsapp_event_ev_t *>(peer_group->contain_nsappevent());
		mtx.lock();
		if (init_event && init_event->is_begin() == false)
			cerr << "Event at " << fixed << setprecision(1) << dequeue->get_abstime()\
				<< " Tries to connect to Root Group " << hex << peer_group->get_group_id()\
				<< " with app event at " << fixed << setprecision(1) << init_event->get_abstime() << endl;
		mtx.unlock();
	}
#endif
	if (invoke_group->get_cluster_idx() == cluster->get_cluster_id()) {
		cluster->add_edge(groups_ptr->group_of(dequeue),
				invoke_group,
				dequeue,
				invoke,
				DISP_EXE_REL);
	} else if (invoke_group->get_cluster_idx() != -1) {
#if DEBUG_EXTRACT_CLUSTER
		mtx.lock();
		cerr << __func__ <<": cluster " << hex << cluster->get_cluster_id() \
			<< " event at " << fixed << setprecision(1) << dequeue->get_abstime() << endl;
		cerr << " group " << hex << invoke_group->get_group_id() \
			<< " with event at " << fixed << setprecision(1) << invoke->get_abstime() \
			<< " was added into cluster " << hex << invoke_group->get_cluster_idx() << endl;
		mtx.unlock();
#endif
	} else {
		cluster->add_edge(groups_ptr->group_of(dequeue),
				invoke_group,
				dequeue,
				invoke,
				DISP_EXE_REL);
		if (cluster->add_node(invoke_group))
			cluster->push_connectors(invoke_group, invoke);
	}
}

void ClusterGen::merge_by_timercallout(cluster_t *cluster,
		timercreate_ev_t *timercreate_event)
{
	if (!timercreate_event->check_event_processing())
		return;

	timercallout_ev_t *timercallout_event = timercreate_event->get_called_peer();
	if (!timercallout_event)
		return;

	group_t *timercallout_group = groups_ptr->group_of(timercallout_event);

#if DEBUG_EXTRACT_CLUSTER
	group_t *peer_group = timercallout_group;
	map<uint64_t, group_t *> main_groups = groups_ptr->get_main_groups();
	if (peer_group->get_cluster_idx() != cluster->get_cluster_id()
			&& main_groups.find(peer_group->get_group_id()) != main_groups.end()) {
		nsapp_event_ev_t *init_event = dynamic_cast<nsapp_event_ev_t *>(peer_group->contain_nsappevent());
		mtx.lock();
		if (init_event && init_event->is_begin() == false)
			cerr << "Event at " << fixed << setprecision(1) << timercreate_event->get_abstime()\
				<< " Tries to connect to Root Group " << hex << peer_group->get_group_id()\
				<< " with app event at " << fixed << setprecision(1) << init_event->get_abstime() << endl;
		mtx.unlock();
	}
#endif

	if (timercallout_group->get_cluster_idx() == cluster->get_cluster_id()) {
		cluster->add_edge(groups_ptr->group_of(timercreate_event),
				timercallout_group,
				timercreate_event,
				timercallout_event,
				CALLOUT_REL);
	} else if (timercallout_group->get_cluster_idx() != -1) {
#if DEBUG_EXTRACT_CLUSTER
		mtx.lock();
		cerr << __func__ <<": cluster " << hex << cluster->get_cluster_id() \
			<< " event at " << fixed << setprecision(1) << timercreate_event->get_abstime() << endl;
		cerr << " group " << hex << timercallout_group->get_group_id() \
			<< " with event at " << fixed << setprecision(1) << timercallout_event->get_abstime() \
			<< " was added into cluster " << hex << timercallout_group->get_cluster_idx() << endl;
		mtx.unlock();
#endif
	} else {
		cluster->add_edge(groups_ptr->group_of(timercreate_event),
				timercallout_group,
				timercreate_event,
				timercallout_event,
				CALLOUT_REL);
		if (cluster->add_node(timercallout_group))
			cluster->push_connectors(timercallout_group, timercallout_event);
	}
}

void ClusterGen::merge_by_coreanimation(cluster_t *cluster,
		ca_set_ev_t *caset_event)
{
	group_t *caset_group = groups_ptr->group_of(caset_event);
	ca_disp_ev_t *cadisplay_event = caset_event->get_display_object();

	if (!cadisplay_event)
		return;
	group_t *cadisplay_group = groups_ptr->group_of(cadisplay_event);

#if DEBUG_EXTRACT_CLUSTER
	group_t * peer_group = cadisplay_group;
	map<uint64_t, group_t *> main_groups = groups_ptr->get_main_groups();
	if (peer_group->get_cluster_idx() != cluster->get_cluster_id()
			&& main_groups.find(peer_group->get_group_id()) != main_groups.end()) {
		nsapp_event_ev_t *init_event = dynamic_cast<nsapp_event_ev_t *>(peer_group->contain_nsappevent());
		mtx.lock();
		if (init_event && init_event->is_begin() == false)
			cerr << "Event at " << fixed << setprecision(1) << caset_event->get_abstime()\
				<< " Tries to connect to Root Group " << hex << peer_group->get_group_id()\
				<< " with app event at " << fixed << setprecision(1) << init_event->get_abstime() << endl;
		mtx.unlock();
	}
#endif

	if (cadisplay_group->get_cluster_idx() == cluster->get_cluster_id()) {
		cluster->add_edge(caset_group,
				cadisplay_group,
				caset_event,
				cadisplay_event,
				CA_REL);
	} else if (cadisplay_group->get_cluster_idx() != -1) {
#if DEBUG_EXTRACT_CLUSTER
		mtx.lock();
		cerr << __func__ <<": cluster " << hex << cluster->get_cluster_id() \
			<< " event at " << fixed << setprecision(1) << caset_event->get_abstime() << endl;
		cerr << " group " << hex << cadisplay_group->get_group_id() \
			<< " with event at " << fixed << setprecision(1) << cadisplay_event->get_abstime() \
			<< " was added into cluster " << hex << cadisplay_group->get_cluster_idx() << endl;
		mtx.unlock();
#endif
	} else {
		cluster->add_edge(caset_group,
				cadisplay_group,
				caset_event,
				cadisplay_event,
				CA_REL);
		if (cluster->add_node(cadisplay_group))
			cluster->push_connectors(cadisplay_group, cadisplay_event);
	}
}

void ClusterGen::merge_by_sharevariables(cluster_t *cluster, 
		breakpoint_trap_ev_t *breakpoint_event)
{
	breakpoint_trap_ev_t *peer_event = breakpoint_event->get_peer();
	group_t *read_group,  *write_group, *peer_group;

	if (!peer_event)
		return;
	
	if (peer_event->check_read()) {
		peer_group = read_group = groups_ptr->group_of(peer_event);
		write_group = groups_ptr->group_of(breakpoint_event);
	} else {
		peer_group = write_group = groups_ptr->group_of(peer_event);
		read_group = groups_ptr->group_of(breakpoint_event);
	}
	assert(read_group && write_group);

#if DEBUG_EXTRACT_CLUSTER
		map<uint64_t, group_t *> main_groups = groups_ptr->get_main_groups();
		if (peer_group->get_cluster_idx() != cluster->get_cluster_id()
			&& main_groups.find(peer_group->get_group_id()) != main_groups.end()) {
			nsapp_event_ev_t *init_event = dynamic_cast<nsapp_event_ev_t *>(peer_group->contain_nsappevent());
			mtx.lock();
			if (init_event && init_event->is_begin() == false)
				cerr << "Event at " << fixed << setprecision(1) << breakpoint_event->get_abstime()\
					<< " Tries to connect to Root Group " << hex << peer_group->get_group_id()\
					<< " with app event at " << fixed << setprecision(1) << init_event->get_abstime() << endl;
			mtx.unlock();
		}
#endif

	if (read_group->get_cluster_idx() == write_group->get_cluster_idx()) {
		cluster->add_edge(write_group,
				read_group,
				breakpoint_event,
				peer_event,
				BRTRAP_REL);
	} else if (write_group->get_cluster_idx() != -1 && read_group->get_cluster_idx() != -1) {
#if DEBUG_EXTRACT_CLUSTER
		mtx.lock();
		cerr << __func__ <<": cluster " << hex << cluster->get_cluster_id() \
			<< " event at " << fixed << setprecision(1) << breakpoint_event->get_abstime();
		cerr << " group " << hex << peer_group->get_group_id() \
			<< " with event at " << fixed << setprecision(1) << peer_event->get_abstime() \
			<< " was added into cluster " << hex << peer_group->get_cluster_idx() << endl;
		mtx.unlock();
#endif
	} else {
		cluster->add_edge(write_group,
				read_group,
				breakpoint_event,
				peer_event,
				BRTRAP_REL);
		if (cluster->add_node(peer_group))
			cluster->push_connectors(peer_group, peer_event);
	}
}

void ClusterGen::merge_by_rlworks(cluster_t *cluster, rl_boundary_ev_t *rlboundary_event)
{
	event_t *peer_event = NULL;
	group_t *peer_group = NULL, *cur_group = groups_ptr->group_of(rlboundary_event);

	if (!(peer_event = rlboundary_event->get_owner()))
		return;
	
	if (!(peer_group = groups_ptr->group_of(peer_event)))
		return;

	if (peer_group->get_cluster_idx() == cluster->get_cluster_id()) {
		cluster->add_edge(cur_group,
			peer_group,
			rlboundary_event,
			peer_event,
			RLITEM_REL);
	} else if (peer_group->get_cluster_idx() != -1) {
#if DEBUG_EXTRACT_CLUSTER
			mtx.lock();
			cerr << __func__ <<": cluster " << hex << cluster->get_cluster_id() \
				<< " event at " << fixed << setprecision(1) << rlboundary_event->get_abstime() << endl;
			cerr << " group " << hex << peer_group->get_group_id() \
				<< " with event at " << fixed << setprecision(1) << peer_event->get_abstime() \
				<< " was added into cluster " << hex << peer_group->get_cluster_idx() << endl;
			mtx.unlock();
#endif
	} else {
		cluster->add_edge(cur_group,
			peer_group,
			rlboundary_event,
			peer_event,
			RLITEM_REL);
		if (cluster->add_node(peer_group))
			cluster->push_connectors(peer_group, peer_event);
	}
}

void ClusterGen::merge_by_mkrunnable(cluster_t *cluster, mkrun_ev_t *wakeup_event)
{
	if (wakeup_event->get_mr_type() == CLEAR_WAIT
		|| wakeup_event->get_mr_type() == WORKQ_MR)
		return;

	event_t *peer = NULL;

	if (wakeup_event && (peer = wakeup_event->get_peer_event())) {
		if (!groups_ptr->group_of(peer)) {
#if DEBUG_EXTRACT_CLUSTER
			mtx.lock();
			cerr << "Check: no event gets woken by mkrunnable event at" ;
			cerr << fixed << setprecision(1) << wakeup_event->get_abstime() << endl;
			mtx.unlock();
#endif
			return;
		}

		group_t *wakeup_group = groups_ptr->group_of(wakeup_event);
		group_t *peer_group = groups_ptr->group_of(peer);

#if DEBUG_EXTRACT_CLUSTER
		map<uint64_t, group_t *> main_groups = groups_ptr->get_main_groups();
		if (peer_group->get_cluster_idx() != cluster->get_cluster_id()
			&& main_groups.find(peer_group->get_group_id()) != main_groups.end()) {
			nsapp_event_ev_t *init_event = dynamic_cast<nsapp_event_ev_t *>(peer_group->contain_nsappevent());
			mtx.lock();
			if (init_event && init_event->is_begin() == false)
				cerr << "Event at " << fixed << setprecision(1) << wakeup_event->get_abstime()\
					<< " Tries to connect to Root Group " << hex << peer_group->get_group_id()\
					<< " with app event at " << fixed << setprecision(1) << init_event->get_abstime() << endl;
			mtx.unlock();
		}
#endif
		if (peer_group->get_cluster_idx() == cluster->get_cluster_id()) {
			cluster->add_edge(wakeup_group,
				peer_group,
				wakeup_event,
				peer,
				MKRUN_REL);
		} else if (peer_group->get_cluster_idx() != -1) {
#if DEBUG_EXTRACT_CLUSTER
			mtx.lock();
			cerr << __func__ <<": cluster " << hex << cluster->get_cluster_id() \
				<< " event at " << fixed << setprecision(1) << wakeup_event->get_abstime() << endl;
			cerr << " group " << hex << peer_group->get_group_id() \
				<< " with event at " << fixed << setprecision(1) << peer->get_abstime() \
				<< " was added into cluster " << hex << peer_group->get_cluster_idx() << endl;
			mtx.unlock();
#endif
		} else {
			cluster->add_edge(wakeup_group,
				peer_group,
				wakeup_event,
				peer,
				MKRUN_REL);
			if (cluster->add_node(peer_group))
				cluster->push_connectors(peer_group, peer);
		}
	}
}

void ClusterGen::augment_cluster(cluster_t *cur_cluster, bool connect_mkrun)
{
	list<event_t *> connectors = cur_cluster->pop_cur_connectors();
	if (connectors.size() == 0)
		return;

	list<event_t *>::iterator it;
	event_t *event;
	for (it = connectors.begin(); it != connectors.end(); it++) {
		event = *it;

		switch (event->get_event_id()) {
			case MSG_EVENT:
				merge_by_mach_msg(cur_cluster, dynamic_cast<msg_ev_t *>(event));
				break;
			case DISP_ENQ_EVENT:
				merge_by_dispatch_ops(cur_cluster, dynamic_cast<enqueue_ev_t *>(event));
				break;
			case DISP_DEQ_EVENT:
				merge_by_dispatch_ops(cur_cluster, dynamic_cast<dequeue_ev_t *>(event));
				break;
			case TMCALL_CREATE_EVENT:
				merge_by_timercallout(cur_cluster, dynamic_cast<timercreate_ev_t *>(event));
				break;
			case CA_SET_EVENT:
				merge_by_coreanimation(cur_cluster, dynamic_cast<ca_set_ev_t *>(event));
				break;
			case BREAKPOINT_TRAP_EVENT:
				merge_by_sharevariables(cur_cluster, dynamic_cast<breakpoint_trap_ev_t *>(event));
				break;
			case RL_BOUNDARY_EVENT:	
				merge_by_rlworks(cur_cluster, dynamic_cast<rl_boundary_ev_t *>(event));
				break;
			case MR_EVENT:
				if (connect_mkrun)
					merge_by_mkrunnable(cur_cluster, dynamic_cast<mkrun_ev_t *>(event));
				break;
			default:
#if DEBUG_EXTRACT_CLUSTER
				mtx.lock();
				cerr << "unknown connector event #" << event->get_event_id() << endl;
				mtx.unlock();
#endif
				break;
		}
	}
	augment_cluster(cur_cluster, connect_mkrun);
}

void ClusterGen::merge_by_waitsync(cluster_t *cluster, wait_ev_t *wait_event)
{
	mkrun_ev_t *wakeup_event = wait_event->get_mkrun();
	event_t *peer = NULL;

	if (wakeup_event && (peer = wakeup_event->get_peer_event())) {
		if (!groups_ptr->group_of(peer)) {
#if 0 //DEBUG_EXTRACT_CLUSTER
			mtx.lock();
			cerr << "No succesive execution gets grouped in the thread at" ;
			cerr << fixed << setprecision(1) << wait_event->get_abstime() << endl;
			mtx.unlock();
#endif
			return;
		}
		
		if (groups_ptr->group_of(peer)->get_cluster_idx() != cluster->get_cluster_id()) {
#if 0 //DEBUG_EXTRACT_CLUSTER
			mtx.lock();
			cerr << "No succesive execution inside cluster after wait at ";
			cerr << fixed << setprecision(1) << wait_event->get_abstime() << endl;
			mtx.unlock();
#endif
			return;
		}

		group_t *wakeup_group = groups_ptr->group_of(wakeup_event);
		if (wakeup_group->get_cluster_idx() == cluster->get_cluster_id()) {
			cluster->add_edge(wakeup_group,
				groups_ptr->group_of(peer),
				wakeup_event,
				peer,
				MKRUN_REL);
		} else if (wakeup_group->get_cluster_idx() != -1) {
#if DEBUG_EXTRACT_CLUSTER
			mtx.lock();
			cerr << __func__ <<": cluster " << hex << cluster->get_cluster_id() \
				<< " event at " << fixed << setprecision(1) << peer->get_abstime() << endl;
			cerr << "group " << hex << wakeup_group->get_group_id() \
				<< " with event at " << fixed << setprecision(1) << wakeup_event->get_abstime() \
				<< " was added into cluster " << hex << wakeup_group->get_cluster_idx() << endl;
			mtx.unlock();
#endif
		} else {
			cluster->add_edge(wakeup_group,
				groups_ptr->group_of(peer),
				wakeup_event,
				peer,
				MKRUN_REL);
			cluster->add_node(wakeup_group);
		}
	}
}

void ClusterGen::add_waitsync(cluster_t *cluster)
{
	list<wait_ev_t *>::iterator it;
	list<wait_ev_t *> wait_events = cluster->get_wait_events();
	for (it = wait_events.begin(); it != wait_events.end(); it++) {
		merge_by_waitsync(cluster, *it);
	}
}

void ClusterGen::js_clusters(string &output_path)
{
	ofstream output(output_path);
	map<uint64_t, cluster_t*>::iterator it;
	uint64_t index = 0;
	cluster_t * cur_cluster;

	mtx.lock();
	cerr << "Total number of Clusters = " << hex << clusters.size() << endl;
	mtx.unlock();

	for (it = clusters.begin(); it != clusters.end(); it++) {
		cur_cluster = it->second;
		assert(cur_cluster != NULL);
		index = cur_cluster->get_cluster_id();
		output << "#Cluster " << hex << cur_cluster->get_cluster_id();
		output << "(num of groups = " << hex << (cur_cluster->get_nodes()).size() << ")\n";
		cur_cluster->js_cluster(output);
	}

	output.close();
}

void ClusterGen::streamout_clusters(string &output_path)
{
	ofstream output(output_path);
	map<uint64_t, cluster_t*>::iterator it;
	cluster_t * cur_cluster;

	mtx.lock();
	cerr << "Total number of Clusters = " << hex << clusters.size() << endl;
	mtx.unlock();

	for (it = clusters.begin(); it != clusters.end(); it++) {
		cur_cluster = it->second;
		assert(cur_cluster != NULL);
		if (cur_cluster->get_nodes().size() < 10)
			continue;
		output << "#Cluster " << hex << cur_cluster->get_cluster_id();
		output << "(num of groups = " << hex << (cur_cluster->get_nodes()).size() << ")\n";

		cur_cluster->decode_edges(output);
		cur_cluster->streamout_cluster(output);
	}

	output.close();
}

void ClusterGen::inspect_clusters(string &output_path)
{
	ofstream output(output_path);
	map<uint64_t, cluster_t*>::iterator it;
	cluster_t * cur_cluster;

	mtx.lock();
	cerr << "Total number of Clusters = " << hex << clusters.size() << endl;
	mtx.unlock();

	for (it = clusters.begin(); it != clusters.end(); it++) {
		cur_cluster = it->second;
		if (cur_cluster->get_nodes().size() < 10)
			continue;
		output << "#Cluster " << hex << cur_cluster->get_cluster_id() << endl;
		cur_cluster->inspect_procs_irrelevance(output);
	}
	output.close();
}

void ClusterGen::compare(ClusterGen *peer, string &output_path)
{
	ofstream output(output_path);
	map<uint64_t, cluster_t *> peer_clusters = peer->get_clusters();
	map<uint64_t, cluster_t *>::iterator it1, it2;

	for (it1 = clusters.begin(); it1 != clusters.end(); it1++) {
		cluster_t *cur_cluster = it1->second;
		for (it2 = peer_clusters.begin(); it2 != peer_clusters.end(); it2++) {
			if (it1->first == it2->first) {
				cur_cluster->compare(it2->second, output);
			}
		}
	}
	output.close();
}

void ClusterGen::check_connection(uint64_t gid_1, uint64_t gid_2, string &output_path)
{
	ofstream output;
	output.open(output_path, ofstream::out | ofstream::app);
	//ofstream output(output_path, ofstream::app);
	group_t *group_1, *group_2;
	cluster_t *cluster;

	group_1 = groups_ptr->get_groups()[gid_1];
	group_2 = groups_ptr->get_groups()[gid_2];
	
	output << "+++++\ncheck_connection of group " << hex << gid_1 << " and group " << hex << gid_2 << endl;
	cluster = cluster_of(group_1);
	if (!cluster || cluster_of(group_2) != cluster)  {
		output << "group " << hex << gid_1 << " and group " << hex << gid_2 << " are not in the same cluster" << endl;
	} else {
		cluster->check_connection(group_1, group_2, output);
	}
	output.close();
}
