#include "parser.hpp"
#include "cluster.hpp"

typedef map<mkrun_ev_t *, list<event_t *>::iterator> mkrun_pos_t;

Clusters::Clusters(groups_t *_groups_ptr)
:groups_ptr(_groups_ptr)
{
	clusters.clear();
	index = 0;
	assert(groups_ptr != NULL);
}

Clusters::~Clusters(void)
{
	map<uint64_t, cluster_t *>::iterator it;
	for (it = clusters.begin(); it != clusters.end(); it++) {
		assert(it->second != NULL);
		delete(it->second);
	}
	clusters.clear();
	index = 0;
	groups_ptr = NULL;
}

cluster_t *Clusters::cluster_of(group_t *g)
{
	if (g->get_cluster_idx() == (uint64_t)-1)
		return NULL;

	if (clusters.find(g->get_cluster_idx()) != clusters.end())
		return clusters[g->get_cluster_idx()];

	cerr << "Error: cluster with index " << g->get_cluster_idx();
	cerr << " not found." << endl;

	return NULL;
}

cluster_t *Clusters::create_cluster()
{
	cluster_t *new_c = new cluster_t();

	if (new_c) {
		assert(clusters.find(index) == clusters.end());
		clusters[index] = new_c;
		new_c->set_cluster_id(index);
		index++;
	}

	return new_c;
}

bool Clusters::del_cluster(cluster_t *c)
{
	uint64_t index = c->get_cluster_id();
	map<uint64_t, cluster_t *>::iterator it = clusters.find(index);

	if (it == clusters.end())
		return false;

	assert(it->second == c);
	clusters.erase(it);
	delete c;
	assert(clusters.find(index) == clusters.end());
	return true;
}

cluster_t * Clusters::merge(cluster_t *c1, cluster_t *c2)
{
	c1->append_nodes(c2->get_nodes());
	c1->append_edges(c2->get_edges());
	return c1;
}

void Clusters::merge_clusters_of_events(event_t *prev_e, event_t *cur_e,
	uint32_t rel_type)
{
	group_t *prev_g = groups_ptr->group_of(prev_e);
	group_t *cur_g = groups_ptr->group_of(cur_e);

	if (!prev_g || !cur_g) {
		cerr << "Check: groups of events do not exist" << endl;
		return;
	}

	//assert(cur_g && prev_g);

	cluster_t *prev_c = cluster_of(prev_g);
	cluster_t *cur_c = cluster_of(cur_g);
	cluster_t *new_c;

	if (prev_c && prev_c == cur_c) {
		prev_c->add_edge(prev_g, cur_g, prev_e, cur_e, rel_type);
		return;
	}

	if (prev_c == NULL) {
		if (cur_c == NULL) {
			new_c = create_cluster();
			if (!new_c) {
				cerr << "OOM " << __func__ << endl;
				return;
			}

			new_c->add_edge(prev_g, cur_g, prev_e, cur_e, rel_type);
			new_c->add_node(prev_g);

			if (prev_g != cur_g)
				new_c->add_node(cur_g);

		} else {
			cur_c->add_edge(prev_g, cur_g, prev_e, cur_e, rel_type);
			cur_c->add_node(prev_g);
		}
	} else {
		if (cur_c == NULL) {
			prev_c->add_edge(prev_g, cur_g, prev_e, cur_e, rel_type);
			prev_c->add_node(cur_g);
		} else {
			assert(prev_c != cur_c);
			cluster_t *merged_c = merge(prev_c, cur_c);
			merged_c->add_edge(prev_g, cur_g, prev_e, cur_e, rel_type);
			assert(merged_c == prev_c);
			cur_c->get_nodes().clear();
			cur_c->get_edges().clear();
			del_cluster(cur_c);
		}
	}
	//TODO: set group root
}

void Clusters::merge_by_mach_msg()
{
	cerr << "merge cluster via mach_msg pattern\n";
	list<event_t *>::iterator it;
	list<event_t *> mach_msg_list = groups_ptr->get_list_of_op(MACH_IPC_MSG);
	msg_ev_t *curr_msg = NULL;
	msg_ev_t *next_msg = NULL;
	msg_ev_t *peer_msg = NULL;
	
	for (it = mach_msg_list.begin(); it != mach_msg_list.end(); it++) {
		curr_msg = dynamic_cast<msg_ev_t *>(*it);
		assert(curr_msg);
		next_msg = curr_msg->get_next();
		if (next_msg != NULL) {
			merge_clusters_of_events(curr_msg, next_msg, MSGP_REL);
		} else {
			peer_msg = curr_msg->get_peer();
			if (peer_msg) {
				if (peer_msg->get_abstime() >= curr_msg->get_abstime()) 
					merge_clusters_of_events(curr_msg, peer_msg, MSGP_REL);
			} else if (!curr_msg->is_freed_before_deliver()) {
				cerr << "Check: no peer msg found for : ";
				cerr << fixed << setprecision(2) \
					<< curr_msg->get_abstime() << endl;
			}
		}
	}
	cerr << "merge mach msg done\n";
}

void Clusters::merge_by_dispatch_ops()
{
	cerr << "merge cluster via dispatch-execute\n";
	list<event_t *>::iterator it;
	list<event_t *> dispatch_exe_list = groups_ptr->get_list_of_op(DISP_EXE);

	for (it = dispatch_exe_list.begin(); it != dispatch_exe_list.end(); it++) {
		blockinvoke_ev_t *invoke_event = dynamic_cast<blockinvoke_ev_t *>(*it);
		assert(invoke_event);
		event_t *root = invoke_event->get_root();
		if (root)
			merge_clusters_of_events(root, invoke_event, DISP_EXE_REL);
	}

	list<event_t *>::iterator deq_it;
	list<event_t *> dispatch_deq_list = groups_ptr->get_list_of_op(DISP_DEQ); 

	for (deq_it = dispatch_deq_list.begin(); deq_it != dispatch_deq_list.end();
		deq_it++) {
		dequeue_ev_t *deq_event = dynamic_cast<dequeue_ev_t *>(*deq_it);
		assert(deq_event);
		event_t *root = deq_event->get_root();
		if (root)
			merge_clusters_of_events(root, deq_event, DISP_EXE_REL);
	}
	cerr << "merge dispatch-execute done\n";
}

void Clusters::merge_by_timercallout()
{
	cerr << "merge cluster via timercallout\n";
	list<event_t *>::iterator it;
	list<event_t *> timercallout_list
		= groups_ptr->get_list_of_op(MACH_CALLOUT);

	for (it = timercallout_list.begin(); it != timercallout_list.end(); it++) {
		timercallout_ev_t *timercallout_event
			= dynamic_cast<timercallout_ev_t *>(*it);

		assert(timercallout_event);
		timercreate_ev_t *timercreate_event
			= timercallout_event->get_timercreate();

		if (timercreate_event)
			merge_clusters_of_events(timercreate_event,
				timercallout_event,
				CALLOUT_REL);
	}

	list<event_t *>::iterator ct;
	list<event_t *> timercancel_list
		= groups_ptr->get_list_of_op(MACH_CALLCANCEL);

	for (ct = timercancel_list.begin(); ct != timercancel_list.end(); ct++) {
		timercancel_ev_t *timercancel_event
			= dynamic_cast<timercancel_ev_t*>(*ct);

		assert(timercancel_event);
		timercreate_ev_t * timercreate_event
			= timercancel_event->get_timercreate();

		if (timercreate_event)
			merge_clusters_of_events(timercreate_event,
				timercancel_event,
				CALLOUTCANCEL_REL);
	}

	cerr << "merge timercallouts done\n";
}

void Clusters::merge_by_ca()
{
	list<event_t *>::iterator it;
	list<event_t *> ca_display_list = groups_ptr->get_list_of_op(CA_DISPLAY);
	for (it = ca_display_list.begin(); it != ca_display_list.end(); it++) {
		ca_disp_ev_t *cadisplay_event = dynamic_cast<ca_disp_ev_t*>(*it);
		vector<ca_set_ev_t *> casets = cadisplay_event->get_ca_set_events();
		vector<ca_set_ev_t *>::iterator set_it;
		for (set_it = casets.begin(); set_it != casets.end(); set_it++) {	
			merge_clusters_of_events(cadisplay_event, *set_it, CA_REL);
		} 
	}
}

void Clusters::merge_by_breakpoint_trap()
{
	list<event_t *>::iterator it;
	list<event_t *> breakpoint_trap_list = groups_ptr->get_list_of_op(BREAKPOINT_TRAP);
	for (it = breakpoint_trap_list.begin(); it != breakpoint_trap_list.end(); it++) {
		breakpoint_trap_ev_t * breakpoint_trap_event = dynamic_cast<breakpoint_trap_ev_t *>(*it);
		if (breakpoint_trap_event->get_peer() && breakpoint_trap_event->check_read()) {
			merge_clusters_of_events(breakpoint_trap_event, breakpoint_trap_event->get_peer(), BRTRAP_REL);
		}
	}
}

void Clusters::merge_by_mkrun()
{
	cerr << "merge cluster via mkrun : ";
	cerr << groups_ptr->get_all_mkrun().size() << "\n";

	mkrun_pos_t all_mkrun = groups_ptr->get_all_mkrun();
	mkrun_pos_t::iterator mkrun_it;

	for (mkrun_it = all_mkrun.begin(); mkrun_it != all_mkrun.end();
		mkrun_it++) {
		mkrun_ev_t *mr_event = mkrun_it->first;
		list<event_t *> &tidlist
			= groups_ptr->get_list_of_tid(mr_event->get_peer_tid());
		list<event_t *>::iterator pos = mkrun_it->second;

		if (pos == tidlist.end()) {
			cerr << "invalid mkrun pair record for mkrunnable at ";
			cerr << fixed << setprecision(1) << mr_event->get_abstime();
			continue;
		}

		while (pos != tidlist.end()
			&& (*pos)->get_group_id() == (uint64_t) -1) {
			//TODO : check events that are not grouped
			pos++; 
		} 
			
		if (pos == tidlist.end()) {
			cerr << "invalid mkrun pair record after skip ungrouped event;";
			cerr << fixed << setprecision(1) << mr_event->get_abstime() << endl;
			continue;
		}

		// timer timercallout
		if (dynamic_cast<timercallout_ev_t *>(*pos))
			continue;

		// clear wait && run_nextreq
		if (mr_event->get_mr_type() == WORKQ_MR)
			continue;
		
		merge_clusters_of_events(mr_event, (*pos), MKRUN_REL);
	}	
	cerr << "merge make_run done\n";
}

void Clusters::decode_clusters(string & output_path)
{
	ofstream output(output_path);
	map<uint64_t, cluster_t *>::iterator it;
	uint64_t index = 0;
	int ground = 0;
	cluster_t *cur_cluster;

	cerr << "Total number of Clusters = " << hex << clusters.size() << endl;
	for (it = clusters.begin(); it != clusters.end(); it++) {
		cur_cluster = it->second;
		assert(cur_cluster);
		index = cur_cluster->get_cluster_id();
		output << "#Cluster " << hex << index << "(num of groups = ";
		output << hex << (cur_cluster->get_nodes()).size() << ")\n";

		if (cur_cluster->check_ground()) {
			ground++;
			cur_cluster->decode_edges(output);
			//cur_cluster->decode_cluster(output);
			//cur_cluster->streamout_cluster(output);
		}
	}
	output.close();
	cerr << "number of ground truth Clusters = ";
	cerr << hex << ground << " over ";
	cerr << hex << clusters.size() << "in total "<< endl;
}

void Clusters::streamout_clusters(string & output_path)
{
	ofstream outfile(output_path);
	map<uint64_t, cluster_t*>::iterator it;
	cluster_t * cur_cluster;
	uint64_t index;

	for (it = clusters.begin(); it != clusters.end(); it++) {
		cur_cluster = it->second;		
		index = cur_cluster->get_cluster_id();
		outfile << "#Cluster " << hex << index;
		outfile << "(num_of_groups = ";
		outfile << hex << (cur_cluster->get_nodes()).size() << ")\n";
		cur_cluster->streamout_cluster(outfile);
	}
	outfile.close();
}

void Clusters::decode_dangle_groups(string & output_path)
{
	ofstream output(output_path);
	map<uint64_t, group_t *> &gs = groups_ptr->get_groups();
	map<uint64_t, group_t *>::iterator it;
	uint64_t total_groups = gs.size();
	int dangle_groups = 0;
	int single_groups = 0;

	for (it = gs.begin(); it != gs.end(); it++) {
		group_t *g = it->second;
		if (g->get_cluster_idx() == (uint64_t)-1) {
			dangle_groups++;
			if (g->get_container().size() == 1)
				single_groups++;
			output << "Group: " << hex <<  g->get_group_id() << endl;
			g->decode_group(output);
		}
	}
	output.close();
	cerr << "dangle group num = " << hex << dangle_groups;
	cerr << "(out of " << hex << total_groups << " total groups)\n";
	cerr << "single group num = " << hex << single_groups;
	cerr << "(out of " << hex << dangle_groups << " dangle groups)\n";
}
