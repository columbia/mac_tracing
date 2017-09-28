#include "parser.hpp"
#include "cluster.hpp"

ClusterGen::ClusterGen(groups_t *groups)
{
	spin_cluster = NULL;
	groups_ptr = groups;
	map<uint64_t, group_t *> main_groups = groups_ptr->get_main_groups();
	map<uint64_t, group_t *>::iterator it;

	/* first process cluster related to spinning beachball
	spin_cluster = NULL;
	for (it = main_groups.begin(); it != main_groups.end(); it++) {
		if (it->second->get_first_event()->get_abstime() < LoadData::meta_data.spin_timestamp
				&& it->second->get_last_event()->get_abstime() > LoadData::meta_data.spin_timestamp
				&&it->second->get_cluster_idx() == -1) {
			spin_cluster = init_cluster(it->second);
			augment_cluster(spin_cluster);
			add_waitsync(spin_cluster);

			cerr << "spin Cluster id = ";
			cerr << hex << spin_cluster->get_cluster_id() << endl;
		}
	}
	*/

	cluster_t *new_cluster;
	for (it = main_groups.begin(); it != main_groups.end(); it++) {
		if (it->second->get_cluster_idx() ==  -1) {
			new_cluster = init_cluster(it->second);
			clusters[it->first] = new_cluster;
			augment_cluster(new_cluster);
			add_waitsync(new_cluster);
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

void ClusterGen::merge_by_mach_msg(cluster_t *cluster, msg_ev_t *curr_msg)
{
	msg_ev_t *next_msg = curr_msg->get_next();
	if (next_msg) {
		group_t *next_group = groups_ptr->group_of(next_msg);
		assert(next_group);

		if (next_group->get_cluster_idx() == cluster->get_cluster_id()) {
			cluster->add_edge(groups_ptr->group_of(curr_msg),
				next_group,
				curr_msg,
				next_msg,
				MSGP_REL);
		} else if (next_group->get_cluster_idx() != -1) {
			cerr << "event at " << fixed << setprecision(1) \
				<< curr_msg->get_abstime() << endl;
			cerr << "group " << hex << next_group->get_group_id();
			cerr << " with event at " << fixed << setprecision(1) \
				 << next_msg->get_abstime();
			cerr << " was added into cluster " << hex \
				<< next_group->get_cluster_idx() << endl;
		} else {
			cluster->add_edge(groups_ptr->group_of(curr_msg),
				next_group,
				curr_msg,
				next_msg,
				MSGP_REL);
			cluster->add_node(next_group);
			cluster->push_connectors(next_group, next_msg);
		}
	}

	msg_ev_t *peer_msg = curr_msg->get_peer();
	if (peer_msg) {
		group_t *peer_group = groups_ptr->group_of(peer_msg);
		if (peer_group == NULL) {
			cerr << "event at " << fixed << setprecision(1) \
				<< peer_msg->get_abstime() << "is not in group properly" << endl;
		}
		assert(peer_group);
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
			cerr << "event at " << fixed << setprecision(1) \
				<< curr_msg->get_abstime() << endl;
			cerr << "group " << hex << peer_group->get_group_id();
			cerr << " with event at " << fixed << setprecision(1) \
				<< peer_msg->get_abstime();
			cerr << " was added into cluster " << hex \
				<< peer_group->get_cluster_idx() << endl;
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
			cluster->add_node(peer_group);
			cluster->push_connectors(peer_group, peer_msg);
		}
	}
}

void ClusterGen::merge_by_dispatch_ops(cluster_t *cluster,
	enqueue_ev_t *enqueue)
{
	if (!enqueue->is_consumed())
		return;

	event_t *deq = enqueue->get_consumer();
	if (deq) {
		group_t *deq_group = groups_ptr->group_of(deq);
		if (deq_group->get_cluster_idx() == cluster->get_cluster_id()) {
			cluster->add_edge(groups_ptr->group_of(enqueue),
				deq_group,
				enqueue,
				deq,
				DISP_DEQ_REL);
		} else if (deq_group->get_cluster_idx() != -1) {
			cerr << "event at " << fixed << setprecision(1) \
				<< enqueue->get_abstime() << endl;
			cerr << "group " << hex << deq_group->get_group_id();
			cerr << " with event at " << fixed << setprecision(1) \
				<< deq->get_abstime();
			cerr << " was added into cluster " << hex \
				<< deq_group->get_cluster_idx() << endl;
		} else {
			cluster->add_edge(groups_ptr->group_of(enqueue),
				deq_group,
				enqueue,
				deq,
				DISP_DEQ_REL);
			cluster->add_node(deq_group);
			cluster->push_connectors(deq_group, deq);
		}
	} else {
		cerr << "Error: no dequeue event for enqueue but with comsume flag set";
		cerr << endl;
	}
}

void ClusterGen::merge_by_dispatch_ops(cluster_t *cluster,
	dequeue_ev_t *dequeue)
{
	event_t *invoke = dequeue->get_invoke();
	if (invoke) {
		group_t *invoke_group = groups_ptr->group_of(invoke);
		if (invoke_group->get_cluster_idx() == cluster->get_cluster_id()) {
			cluster->add_edge(groups_ptr->group_of(dequeue),
				invoke_group,
				dequeue,
				invoke,
				DISP_EXE_REL);
		} else if (invoke_group->get_cluster_idx() != -1) {
			cerr << "event at " << fixed << setprecision(1) \
				<< dequeue->get_abstime() << endl;
			cerr << "group " << hex << invoke_group->get_group_id();
			cerr << " with event at " << fixed << setprecision(1) \
				<< invoke->get_abstime();
			cerr << " was added into cluster " << hex \
				<< invoke_group->get_cluster_idx() << endl;
		} else {
			cluster->add_edge(groups_ptr->group_of(dequeue),
				invoke_group,
				dequeue,
				invoke,
				DISP_EXE_REL);
			cluster->add_node(invoke_group);
			cluster->push_connectors(invoke_group, invoke);
		}
	}
}

void ClusterGen::merge_by_timercallout(cluster_t *cluster,
	timercreate_ev_t *timercreate_event)
{
	timercallout_ev_t *timercallout_event
		= timercreate_event->get_called_peer();

	if (timercallout_event) {
		group_t *timercallout_group = groups_ptr->group_of(timercallout_event);
		if (timercallout_group->get_cluster_idx() == cluster->get_cluster_id()) {
			cluster->add_edge(groups_ptr->group_of(timercreate_event),
				timercallout_group,
				timercreate_event,
				timercallout_event,
				CALLOUT_REL);
		} else if (timercallout_group->get_cluster_idx() != -1) {
			cerr << "event at " << fixed << setprecision(1) \
				<< timercreate_event->get_abstime() << endl;
			cerr << "group " << hex << timercallout_group->get_group_id();
			cerr << " with event at " << fixed << setprecision(1) \
				<< timercallout_event->get_abstime();
			cerr << " was added into cluster " << hex \
				<< timercallout_group->get_cluster_idx() << endl;
		} else {
			cluster->add_edge(groups_ptr->group_of(timercreate_event),
				timercallout_group,
				timercreate_event,
				timercallout_event,
				CALLOUT_REL);
			cluster->add_node(timercallout_group);
			cluster->push_connectors(timercallout_group, timercallout_event);
		}
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

	if (cadisplay_group->get_cluster_idx() == cluster->get_cluster_id()) {
		cluster->add_edge(caset_group,
				cadisplay_group,
				caset_event,
				cadisplay_event,
				CA_REL);
	} else if (cadisplay_group->get_cluster_idx() != -1) {
		cerr << "event at " << fixed << setprecision(1) \
			<< caset_event->get_abstime() << endl;
		cerr << "group " << hex << cadisplay_group->get_group_id();
		cerr << " with event at " << fixed << setprecision(1) \
			<< cadisplay_event->get_abstime();
		cerr << " was added into cluster " << hex \
			<< cadisplay_group->get_cluster_idx() << endl;
	} else {
		cluster->add_edge(caset_group,
				cadisplay_group,
				caset_event,
				cadisplay_event,
				CA_REL);
		cluster->add_node(cadisplay_group);
		cluster->push_connectors(cadisplay_group, cadisplay_event);
	}
}

void ClusterGen::merge_by_sharevariables(cluster_t *cluster, 
		breakpoint_trap_ev_t *breakpoint_event)
{
	breakpoint_trap_ev_t *peer_event = breakpoint_event->get_peer();

	if (peer_event && peer_event->check_read()) {
		group_t *read_group = groups_ptr->group_of(peer_event);
		group_t *write_group = groups_ptr->group_of(breakpoint_event);
		if (read_group->get_cluster_idx() == cluster->get_cluster_id()) {
			cluster->add_edge(write_group,
					read_group,
					breakpoint_event,
					peer_event,
					BRTRAP_REL);
		} else if (read_group->get_cluster_idx() != -1) {
			cerr << "event at " << fixed << setprecision(1) \
				<< peer_event->get_abstime() << endl;
			cerr << "group " << hex << write_group->get_group_id();
			cerr << " with event at " << fixed << setprecision(1) \
				<< breakpoint_event->get_abstime();
			cerr << " was added into cluster " << hex \
				<< write_group->get_cluster_idx() << endl;
		} else {
			cluster->add_edge(write_group,
					read_group,
					breakpoint_event,
					peer_event,
					BRTRAP_REL);
			cluster->add_node(read_group);
			cluster->push_connectors(read_group, peer_event);
		}
	}
}

void ClusterGen::augment_cluster(cluster_t *cur_cluster)
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
			default:
				cerr << "unknown connector " << event->get_event_id() << endl;
				break;
		}
	}
	augment_cluster(cur_cluster);
}

void ClusterGen::merge_by_waitsync(cluster_t *cluster, wait_ev_t *wait_event)
{
	mkrun_ev_t *wakeup_event = wait_event->get_mkrun();
	event_t *peer = NULL;

	if (wakeup_event && (peer = wakeup_event->get_peer_event())) {

		if (!groups_ptr->group_of(peer)
			|| groups_ptr->group_of(peer)->get_cluster_idx() \
			!= cluster->get_cluster_id()) {
			cerr << "No succesive execution inside cluster after wait at ";
			cerr << fixed << setprecision(1) << wait_event->get_abstime();
			cerr << endl;
			return;
		}

		group_t *wakeup_group = groups_ptr->group_of(wakeup_event);
		if (wakeup_group->get_cluster_idx() == cluster->get_cluster_id()) {
			cluster->add_edge(wakeup_group,
				groups_ptr->group_of(wait_event),
				wakeup_event,
				wait_event,
				MKRUN_REL);
		} else if (wakeup_group->get_cluster_idx() != -1) {
			cerr << "event at " << fixed << setprecision(1) \
				<< wakeup_event->get_abstime() << endl;
			cerr << "group " << hex << wakeup_group->get_group_id();
			cerr << " with event at " << fixed << setprecision(1) \
				<< wakeup_event->get_abstime();
			cerr << " was added into cluster " << hex \
				<< wakeup_group->get_cluster_idx() << endl;
		} else {
			cluster->add_edge(wakeup_group,
				groups_ptr->group_of(wait_event),
				wakeup_event,
				wait_event,
				MKRUN_REL);
			cluster->add_node(wakeup_group);
		}
	}
}

void ClusterGen::add_waitsync(cluster_t *cluster)
{
	cerr << "Begin add wait sync into cluster" << endl;
	list<wait_ev_t *>::iterator it;
	list<wait_ev_t *> wait_events = cluster->get_wait_events();
	for (it = wait_events.begin(); it != wait_events.end(); it++) {
		merge_by_waitsync(cluster, *it);
	}
	cerr << "End add wait sync into cluster" << endl;
}

void ClusterGen::pic_clusters(string & output_path)
{
	ofstream output(output_path);
	/*
	if (spin_cluster)
		spin_cluster->pic_cluster(output);
	*/

	map<uint64_t, cluster_t*>::iterator it;
	uint64_t index = 0;
	cluster_t * cur_cluster;

	cerr << "Total number of Clusters = " << hex << clusters.size() << endl;
	for (it = clusters.begin(); it != clusters.end(); it++) {
		cur_cluster = it->second;
		assert(cur_cluster != NULL);
		index = cur_cluster->get_cluster_id();
		output << "#Cluster " << hex << cur_cluster->get_cluster_id();
		output << "(num of groups = " << hex << (cur_cluster->get_nodes()).size() << ")\n";
		cur_cluster->pic_cluster(output);
	}

	output.close();
}

void ClusterGen::streamout_clusters(string &output_path)
{
	ofstream output(output_path);
	
	/*
	if (spin_cluster)
		spin_cluster->streamout_cluster(output);
	*/
	map<uint64_t, cluster_t*>::iterator it;
	cluster_t * cur_cluster;

	cerr << "Total number of Clusters = " << hex << clusters.size() << endl;
	for (it = clusters.begin(); it != clusters.end(); it++) {
		cur_cluster = it->second;
		assert(cur_cluster != NULL);

		output << "#Cluster " << hex << cur_cluster->get_cluster_id();
		output << "(num of groups = " << hex << (cur_cluster->get_nodes()).size() << ")\n";

		cur_cluster->decode_edges(output);
		cur_cluster->streamout_cluster(output);
	}

	output.close();
}
