#include "parser.hpp"
#include "cluster.hpp"

ClusterGen::ClusterGen(groups_t * groups)
{
	groups_ptr = groups;
	map<uint64_t, group_t *> main_groups = groups_ptr->get_main_groups();
	map<uint64_t, group_t *>::iterator it;
	cluster_t * new_cluster;
	for (it = main_groups.begin(); it != main_groups.end(); it++) {
		new_cluster = init_cluster(it->second);
		clusters[it->first] = new_cluster;
	}
	map<uint64_t, cluster_t *>::iterator cluster_it;
	for (cluster_it = clusters.begin(); cluster_it != clusters.end(); cluster_it++) {
		augment_cluster(cluster_it->second);
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

cluster_t * ClusterGen::init_cluster(group_t * group)
{
	cluster_t * ret = new Cluster(group);
	ret->push_connectors(group, NULL);
	return ret;
}

void ClusterGen::merge_by_mach_msg(cluster_t * cluster, msg_ev_t * curr_msg)
{
	msg_ev_t * next_msg = curr_msg->get_next();
	if (next_msg != NULL) {
		group_t * next_group = groups_ptr->group_of(next_msg);

		if (next_group->get_cluster_idx() == cluster->get_cluster_id())
			cluster->add_edge(groups_ptr->group_of(curr_msg), next_group, curr_msg, next_msg, MSGP);
		else if (next_group->get_cluster_idx() != -1) {
			cerr << "cluster of " << fixed << setprecision(1) << next_msg->get_abstime() << " was added into cluster " << hex << next_group->get_cluster_idx() << endl;
		} else {
			cluster->add_edge(groups_ptr->group_of(curr_msg), next_group, curr_msg, next_msg, MSGP);
			cluster->add_node(next_group);
			cluster->push_connectors(next_group, next_msg);
		}
	}

	msg_ev_t * peer_msg = curr_msg->get_peer();
	if (peer_msg && peer_msg->get_abstime() > curr_msg->get_abstime()) {
		group_t * peer_group = groups_ptr->group_of(peer_msg);
		
		if (peer_group->get_cluster_idx() == cluster->get_cluster_id())
			cluster->add_edge(groups_ptr->group_of(curr_msg), peer_group, curr_msg, peer_msg, MSGP);
		else if (peer_group->get_cluster_idx() != -1)
			cerr << "cluster of " << fixed << setprecision(1) << peer_msg->get_abstime() << " was added into cluster " << hex << peer_group->get_cluster_idx() << endl;
		else {
			cluster->add_edge(groups_ptr->group_of(curr_msg), peer_group, curr_msg, peer_msg, MSGP);
			cluster->add_node(peer_group);
			cluster->push_connectors(peer_group, peer_msg);
		}
	}
}

void ClusterGen::merge_by_dispatch_ops(cluster_t * cluster, blockinvoke_ev_t *invoke_event)
{
	event_t * root = invoke_event->get_root();
	if (root != NULL) {
		group_t * root_group = groups_ptr->group_of(root);
		if (root_group->get_cluster_idx() == cluster->get_cluster_id())
			cluster->add_edge(root_group, groups_ptr->group_of(invoke_event), root, invoke_event, DISP_EXE);
		else if (root_group->get_cluster_idx() != -1) {
			cerr << "cluster of " << fixed << setprecision(1) << root->get_abstime() << " was added into cluster " << hex << root_group->get_cluster_idx() << endl;
		} else {
			cluster->add_edge(root_group, groups_ptr->group_of(invoke_event), root, invoke_event, DISP_EXE);
			cluster->add_node(root_group);
			cluster->push_connectors(root_group, root);
		}
	}
}

void ClusterGen::merge_by_dispatch_ops(cluster_t * cluster, dequeue_ev_t * dequeue)
{
	event_t * root = dequeue->get_root();
	if (root != NULL) {
		group_t * root_group = groups_ptr->group_of(root);
		if (root_group->get_cluster_idx() == cluster->get_cluster_id())
			cluster->add_edge(root_group, groups_ptr->group_of(dequeue), root, dequeue, DISP_DEQ);
		else if (root_group->get_cluster_idx() != -1) {
			cerr << "cluster of " << fixed << setprecision(1) << root->get_abstime() << " was added into cluster " << hex << root_group->get_cluster_idx() << endl;
		} else {
			cluster->add_edge(root_group, groups_ptr->group_of(dequeue), root, dequeue, DISP_DEQ);
			cluster->add_node(root_group);
			cluster->push_connectors(root_group, root);
		}
	}
}

void ClusterGen::merge_by_timercallout(cluster_t * cluster, timercallout_ev_t * timercallout_event)
{
	timercreate_ev_t * timercreate_event = timercallout_event->get_timercreate();
	if (timercreate_event != NULL) {
		group_t * timercreate_group = groups_ptr->group_of(timercreate_event);
		if (timercreate_group->get_cluster_idx() == cluster->get_cluster_id())
			cluster->add_edge(timercreate_group, groups_ptr->group_of(timercallout_event), timercreate_event, timercallout_event, CALLOUT);
		else if (timercreate_group->get_cluster_idx() != -1) {
			cerr << "cluster of " << fixed << setprecision(1) <<timercreate_event->get_abstime() << " was added into cluster " << hex << timercreate_group->get_cluster_idx() << endl;
		} else {
			cluster->add_edge(timercreate_group, groups_ptr->group_of(timercallout_event), timercreate_event, timercallout_event, CALLOUT);
			cluster->add_node(timercreate_group);
			cluster->push_connectors(timercreate_group, timercreate_event);
		}
	}
}

void ClusterGen::merge_by_mkrun(cluster_t * cluster, mkrun_ev_t *mr_event)
{
}

void ClusterGen::augment_cluster(cluster_t * cur_cluster)
{
	list<event_t *> connectors = cur_cluster->pop_cur_connectors();
	if (connectors.size() == 0)
		return;

	list<event_t *>::iterator it;
	event_t * event;
	msg_ev_t * mach_msg_event;
	blockinvoke_ev_t * invoke_event;
	dequeue_ev_t * deq_event;
	timercallout_ev_t * timercallout_event;
	mkrun_ev_t *mr_event;
	
	for (it = connectors.begin(); it != connectors.end(); it++) {
		event = *it;
		mach_msg_event = dynamic_cast<msg_ev_t *>(event);
		if (mach_msg_event) {
			merge_by_mach_msg(cur_cluster, mach_msg_event);
			continue;
		}

		invoke_event = dynamic_cast<blockinvoke_ev_t*>(event);
		if (invoke_event) {
			merge_by_dispatch_ops(cur_cluster, invoke_event);
			continue;
		}

		deq_event = dynamic_cast<dequeue_ev_t*>(event);
		if (deq_event) {
			merge_by_dispatch_ops(cur_cluster, deq_event);
			continue;
		}

		timercallout_event = dynamic_cast<timercallout_ev_t*>(event);
		if (timercallout_event) {
			merge_by_timercallout(cur_cluster, timercallout_event);
			continue;
		}
		
		mkrun_ev_t *mr_event = dynamic_cast<mkrun_ev_t *>(event);
		if (mr_event) {
			merge_by_mkrun(cur_cluster, mr_event);
			continue;
		}
	}

	augment_cluster(cur_cluster);
}

void ClusterGen::streamout_clusters(string & output_path)
{
	ofstream output(output_path);
	map<uint64_t, cluster_t*>::iterator it;
	uint64_t index = 0;
	cluster_t * cur_cluster;

	cerr << "Total number of Clusters = " << hex << clusters.size() << endl;
	for (it = clusters.begin(); it != clusters.end(); it++) {
		cur_cluster = it->second;
		assert(cur_cluster != NULL);
		index = cur_cluster->get_cluster_id();
		output << "#Cluster " << hex << index << "(num of groups = " << hex << (cur_cluster->get_nodes()).size() << ")\n";
		//cur_cluster->decode_edges(output);
		//cur_cluster->decode_cluster(output);
		cur_cluster->streamout_cluster(output);
	}
	output.close();
}
