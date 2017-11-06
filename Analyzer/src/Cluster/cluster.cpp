#include "cluster.hpp"
#include <algorithm>
Cluster::Cluster(void)
{
	cluster_id = -1;
	is_ground = is_infected = false;
	infected_groups.clear();
	gt_groups.clear();
	edges.clear();
	nodes.clear();
}

Cluster::Cluster(group_t *group)
{
	root = group;
	cluster_id = group->get_group_id();
	is_ground = is_infected = false;
	infected_groups.clear();
	gt_groups.clear();
	edges.clear();
	nodes.clear();
	add_node(group);
	connectors.clear();
}

Cluster::~Cluster(void)
{
	infected_groups.clear();
	gt_groups.clear();
	edges.clear();
	nodes.clear();
}

void Cluster::push_connectors(group_t *group, event_t *exclude)
{
	list<event_t *> events = group->get_container();
	list<event_t *>::iterator it;
	event_t *event;
	for (it = events.begin(); it != events.end(); it++) {
		event = *it;
		if (event == exclude)
			continue;

		switch (event->get_event_id()) {
			case MSG_EVENT:
				connectors.push_back(dynamic_cast<msg_ev_t *>(event));
				break;
			case DISP_ENQ_EVENT:
				connectors.push_back(dynamic_cast<enqueue_ev_t *>(event));
				break;
			case DISP_DEQ_EVENT:
				connectors.push_back(dynamic_cast<dequeue_ev_t *>(event));
				break;
			case TMCALL_CREATE_EVENT:
				connectors.push_back(dynamic_cast<timercreate_ev_t *>(event));
				break;
			case CA_SET_EVENT:
				connectors.push_back(dynamic_cast<ca_set_ev_t *>(event));
				break;
			case BREAKPOINT_TRAP_EVENT:
				connectors.push_back(dynamic_cast<breakpoint_trap_ev_t *>(event));
				break;
			case MR_EVENT:
				connectors.push_back(dynamic_cast<mkrun_ev_t *>(event));
				break;
			case WAIT_EVENT:
				wait_events.push_back(dynamic_cast<wait_ev_t *>(event));
				break;
			default:
				break;
		}
	}
}

list<event_t *> Cluster::pop_cur_connectors()
{
	list<event_t *> ret = connectors;
	connectors.clear();
	return ret;
}

list<wait_ev_t *> &Cluster::get_wait_events()
{
	return wait_events;
}

void Cluster::add_edge(group_t *g1, group_t *g2, event_t *t1, event_t *t2, uint32_t type)
{
	struct rel_t edge = {g1, g2, t1, t2, type};

	vector<rel_t>::iterator it = find_if(edges.begin(), edges.end(), 
		[&target_edge = edge]
		(const rel_t &m) -> bool{return target_edge == m;});

	if (it == edges.end())
		edges.push_back(edge);
}

bool Cluster::add_node(group_t *g)
{
	if (find(nodes.begin(), nodes.end(), g) != nodes.end())
		return false;
	
	if (g->check_ground() == true) {
		is_ground = true;
		gt_groups.push_back(g);
	}
	
	if (g->check_infected() == true) {
		is_infected = true;
		infected_groups.push_back(g);
	}

	g->set_cluster_idx(cluster_id);
	nodes.push_back(g);
	return true;
}

bool Cluster::remove_edge(const rel_t edge)
{
	vector<rel_t>::iterator it = find_if(edges.begin(), edges.end(), 
		[&target_edge = edge]
		(const rel_t &m) -> bool{return target_edge == m;});

	if (it != edges.end()) {
		edges.erase(it);
		return true;
	}

	return false;
}

bool Cluster::remove_node(group_t *node)
{
	vector<group_t *>::iterator it = find(nodes.begin(), nodes.end(), node);
	if (it != nodes.end()) {
		nodes.erase(it);
		return true;
	}
	return false;
}

void Cluster::append_nodes(vector<group_t *> &n)
{
	vector<group_t *>::iterator it;
	for (it = n.begin(); it != n.end(); it++) {
		(*it)->set_cluster_idx(cluster_id);

		if ((*it)->check_ground() == true) {
			is_ground = true;
			gt_groups.push_back(*it);
		}
	
		if ((*it)->check_infected() == true) {
			is_infected = true;
			infected_groups.push_back(*it);
		}
	}
	nodes.insert(nodes.end(), n.begin(), n.end());
}

void Cluster::append_edges(vector<rel_t> &g)
{
	edges.insert(edges.end(), g.begin(), g.end());
}

bool Cluster::compare_time(group_t *node1, group_t *node2)
{
	double timestamp1 = node1->get_container().front()->get_abstime();
	double timestamp2 = node2->get_container().front()->get_abstime();

	if (timestamp1 - timestamp2 <= 10e-8 && timestamp1-timestamp2 >= -10e-8)
		return (node1->get_container().front()->get_tid() \
			< node2->get_container().front()->get_tid());
	return (timestamp1 < timestamp2);
}

void Cluster::sort_nodes()
{
	sort(nodes.begin(), nodes.end(), &Cluster::compare_time);
}

bool Cluster::compare_edge_from(rel_t edge1, rel_t edge2)
{
	if (edge1.g_from->get_group_id() != edge2.g_from->get_group_id())
		return edge1.g_from->get_group_id() < edge2.g_from->get_group_id();
	return compare_time(edge1.g_from, edge2.g_from);
}

void Cluster::sort_edges()
{
	sort(edges.begin(), edges.end(), &Cluster::compare_edge_from);
}

void Cluster::classify_cluster_edges(node_edges_map_t &to_edges, node_edges_map_t &from_edges)
{
	vector<rel_t>::iterator it;
	to_edges.clear();
	from_edges.clear();
	for (it = edges.begin(); it != edges.end(); it++) {
		rel_t edge = *it;
		if (edge.g_to == edge.g_from)
			continue;
		to_edges.insert(make_pair(edge.g_to, edge));
		from_edges.insert(make_pair(edge.g_from, edge));
	}
}

rel_t Cluster::direct_edge(string from_proc, string to_proc)
{
	vector<rel_t>::iterator it;
	for (it = edges.begin(); it != edges.end(); it++) {
		rel_t edge = *it;
		if ((edge.e_from->get_procname() == from_proc && edge.e_to->get_procname() == to_proc)
			|| (edge.e_from->get_procname() == to_proc && edge.e_to->get_procname() == from_proc))
			return edge; 
	}
	rel_t empty = {NULL, NULL, NULL, NULL, 0};
	return empty;
}

int Cluster::get_node_idx_in_cluster(group_t *node)
{
	vector<group_t *>::iterator pos = find(nodes.begin(), nodes.end(), node);
	int ret = -1;
	if (pos != nodes.end())
		ret = pos - nodes.begin();
	return ret;
}

bool Cluster::traverse(string to_proc, group_t *cur_node, bool *visited,
		node_edges_map_t &from_edges, node_edges_map_t &to_edges, vector<rel_t> &paths,
		map<pair<group_t *, string>, vector<rel_t> > &sub_result)
{
	multimap_range_t from_range = from_edges.equal_range(cur_node);
	multimap_range_t to_range = to_edges.equal_range(cur_node);

	if (from_range.first == from_range.second && to_range.first == to_range.second) 
		return false;

	pair<group_t *, string> key = make_pair(cur_node, to_proc);
	if (sub_result.find(key) != sub_result.end()) {
		vector<rel_t> remaining = sub_result[key];
		paths.insert(paths.end(), remaining.begin(), remaining.end());
		return true;
	}

	visited[get_node_idx_in_cluster(cur_node)] = true;
	node_edges_map_t::iterator it;
	group_t *peer_node;
	string peer_proc;
	for (it = from_range.first; it != from_range.second; it++) {
		peer_node = it->second.g_to;
		peer_proc = it->second.e_to->get_procname();
		if (visited[get_node_idx_in_cluster(peer_node)] == true)
			continue;
		paths.push_back(it->second);
		if (it->second.e_to->get_procname() == to_proc
				|| traverse(to_proc, peer_node, visited, from_edges, to_edges, paths, sub_result) == true) {
			pair<group_t *, string> key = make_pair(peer_node, to_proc);
			sub_result[key] = paths;
			return true;
		}
		paths.pop_back();
	}

	for (it = to_range.first; it != to_range.second; it++) {
		peer_node = it->second.g_from;
		if (visited[get_node_idx_in_cluster(peer_node)] == true)
			continue;
		paths.push_back(it->second);
		if (it->second.e_from->get_procname() == to_proc
				|| traverse(to_proc, peer_node, visited, from_edges, to_edges, paths, sub_result) == true) {
			pair<group_t *, string> key = make_pair(peer_node, to_proc);
			sub_result[key] = paths;
			return true;
		}
		paths.pop_back();
	}
	visited[get_node_idx_in_cluster(cur_node)] = false;
	return false;
}

void Cluster::search_paths(string from_proc, string to_proc, ofstream &outfile,
		map<pair<group_t *, string>, vector<rel_t> > &sub_result)
{
	set<group_t *> from_nodes;
	node_edges_map_t to_edges, from_edges;
	vector<group_t *>::iterator it;

	for (it = nodes.begin(); it != nodes.end(); it++)
		if ((*it)->get_first_event()->get_procname() == from_proc)
			from_nodes.insert(*it);
	classify_cluster_edges(to_edges, from_edges);

	//search in the cluseter and store the paths
	bool *visited = (bool *)malloc(sizeof(bool) * nodes.size());
	vector<rel_t> paths;
	if (visited == NULL) {
		mtx.lock();
		cerr << "Error: OOM " << __func__ << endl;
		mtx.unlock();
		return;
	}
	set<group_t *>::iterator node_it;
	for (node_it = from_nodes.begin(); node_it != from_nodes.end(); node_it++) {
		memset(visited, 0, nodes.size());
		paths.clear();
		if (traverse(to_proc, *node_it, visited, from_edges, to_edges, paths, sub_result)) {
			//flush_out_result	
			outfile << "Path from " << from_proc << " to " << to_proc << endl;
			vector<rel_t>::iterator edge_it;
			for (edge_it = paths.begin(); edge_it != paths.end(); edge_it ++) {
				outfile << "\trel " << (*edge_it).e_from->get_procname();
				outfile << " " << std::right << hex << (*edge_it).g_from->get_group_id();
				outfile << " at " << fixed << setprecision(1) << (*edge_it).e_from->get_abstime();
				outfile << " -> " << (*edge_it).e_to->get_procname();
				outfile << " " << std::right << hex << (*edge_it).g_to->get_group_id();
				outfile << " at " << fixed << setprecision(1) << (*edge_it).e_to->get_abstime();
				outfile << endl;
			}
			outfile << endl;

			pair<group_t *, string> key = make_pair(*node_it, to_proc);
			sub_result[key] = paths;
			break;
		}
	}
	free(visited);
}

void Cluster::inspect_procs_irrelevance(ofstream &outfile)
{
	set<string> procs;
	set<string>::iterator proc_it, proc_jt;
	vector<group_t *>::iterator node_it;
	map<pair<group_t *, string>, vector<rel_t> > sub_result;

	for (node_it = nodes.begin(); node_it != nodes.end(); node_it++)
		procs.insert((*node_it)->get_first_event()->get_procname());
	
	for (proc_it = procs.begin(); proc_it != procs.end(); proc_it++)
		for (proc_jt = proc_it, proc_jt++; proc_jt != procs.end(); proc_jt++) {
			outfile << "Inspect connection between " << *proc_it << " and " << *proc_jt << endl; 
			rel_t edge = direct_edge(*proc_it, *proc_jt);
			if (edge.g_from != NULL) {
				outfile << "Direct edge from " << *proc_it << " to " << *proc_jt << endl;

				pair<group_t *, string> key1 = make_pair(edge.g_from, edge.e_to->get_procname());
				pair<group_t *, string> key2 = make_pair(edge.g_to, edge.e_from->get_procname());
				vector<rel_t> paths;
				paths.clear();
				paths.push_back(edge);
				sub_result[key1] = paths;
				sub_result[key2] = paths;
			} else {
				search_paths(*proc_it, *proc_jt, outfile, sub_result);
			}
		}
}	

void Cluster::js_lanes(ofstream &outfile)
{
	set<string> lanes;
	vector<group_t *>::iterator it;
	event_t * first_event = NULL;

	for (it = nodes.begin(); it != nodes.end(); it++) {
		first_event = (*it)->get_first_event();
		stringstream ss;
		ss << hex << first_event->get_tid();
		lanes.insert(first_event->get_procname() + "_" + ss.str());
	}

	set<string>::iterator lane_it;
	outfile << "var lanes = [" << endl;
	for (lane_it = lanes.begin(); lane_it != lanes.end(); lane_it++)
		outfile << "\"" << *lane_it << "\"," << endl;
	outfile << "];" << endl;
}

void Cluster::js_groups(ofstream &outfile)
{
	vector<group_t *>::iterator it;
	event_t * first_event = NULL, * last_event = NULL;
	bool comma = false;

	outfile << "var groups = [" << endl;
	for (it = nodes.begin(); it != nodes.end(); it++) {
		if (comma)
			outfile << "," << endl;
		else
			comma = true;
		first_event = (*it)->get_first_event();
		last_event = (*it)->get_last_event();
		uint64_t time_begin, time_end;
		outfile <<"{lane: \"";
		outfile << first_event->get_procname();
		outfile << "_" << hex << first_event->get_tid();
		outfile <<"\", start: ";
		time_begin = static_cast<uint64_t>(first_event->get_abstime() * 10);
		outfile << dec << time_begin << ", end: ";
		time_end = static_cast<uint64_t>(last_event->get_abstime() * 10);
		outfile << dec << time_end << ", duration: ";
		outfile << dec << time_end - time_begin << ", name: \"";
		outfile << hex << (*it)->get_group_id() << "\"}";
	}
	outfile << "];" << endl;
}

void Cluster::js_edge(ofstream &outfile, event_t *host, const char *action, event_t *peer, bool *comma)
{
	uint64_t time;
	peer = peer ? peer : host;
	if (*comma == true)
		outfile << "," << endl;
	else 
		*comma = true;

	outfile << "{laneFrom: \"" << host->get_procname();
	outfile << "_" << hex << host->get_tid() << "\",";
	outfile << "laneTo: \"" << peer->get_procname();
	outfile << "_" << hex << peer->get_tid() << "\",";
	time = static_cast<uint64_t>(host->get_abstime() * 10);
	outfile << "timeFrom: " << dec << time << ",";
	time = static_cast<uint64_t>(peer->get_abstime() * 10);
	outfile << "timeTo: " << dec << time << ",";
	outfile << "label:\"" << action << "\"}";
}

void Cluster::message_edge(ofstream &outfile, event_t *event, bool *comma)
{
	msg_ev_t *mach_msg_event = dynamic_cast<msg_ev_t *>(event);
	event_t *peer = NULL;
	if (!mach_msg_event)
		return;
	if ((peer = mach_msg_event->get_peer())) {
		if (mach_msg_event->get_abstime() < peer->get_abstime())
			js_edge(outfile, mach_msg_event, "send", peer, comma);
		else
			js_edge(outfile, mach_msg_event, "recv", NULL, comma);
	}

	if ((peer = mach_msg_event->get_next()))
		js_edge(outfile, mach_msg_event, "pass_msg", peer, comma);
}

void Cluster::dispatch_edge(ofstream &outfile, event_t *event, bool *comma)
{
	event_t *peer = NULL;
	enqueue_ev_t *enqueue_event = dynamic_cast<enqueue_ev_t *>(event);
	if (enqueue_event && ((peer = enqueue_event->get_consumer()))) {
		js_edge(outfile, enqueue_event, "dispatch", peer, comma);
		return;
	}

	dequeue_ev_t *deq_event = dynamic_cast<dequeue_ev_t *>(event);
	if (deq_event && ((peer = deq_event->get_invoke())))
		js_edge(outfile, deq_event, "deq_exec", peer, comma);
}

void Cluster::timercall_edge(ofstream &outfile, event_t *event, bool *comma)
{
	event_t *peer = NULL;
	timercreate_ev_t *timercreate_event = dynamic_cast<timercreate_ev_t *>(event);
	if (timercreate_event && ((peer = timercreate_event->get_called_peer())))
		js_edge(outfile, timercreate_event, "timer_callout", peer, comma);
}

void Cluster::mkrun_edge(ofstream &outfile, event_t *event, bool *comma)
{
	mkrun_ev_t *mr_event = dynamic_cast<mkrun_ev_t *>(event);
	event_t *peer = NULL;

	if (mr_event && (peer = mr_event->get_peer_event())) {
		string wakeup_resource("wakeup_");
		if (mr_event->get_wait() != NULL)
			wakeup_resource.append(mr_event->get_wait()->get_wait_resource());
		js_edge(outfile, mr_event, wakeup_resource.c_str(), peer, comma);
	}
}

void Cluster::wait_label(ofstream &outfile, event_t *event, bool *comma)
{
	wait_ev_t *wait_event = dynamic_cast<wait_ev_t *>(event);
	if (!wait_event)
		return;
	string wait_resource("wait_");
	wait_resource.append(wait_event->get_wait_resource());
	js_edge(outfile, wait_event, wait_resource.c_str(), NULL, comma);
}

void Cluster::coreannimation_edge(ofstream &outfile, event_t *event, bool *comma)
{
	ca_set_ev_t *caset_event = dynamic_cast<ca_set_ev_t *>(event);
	event_t *peer = NULL;
	if (caset_event && (peer = caset_event->get_display_object()))
		js_edge(outfile, caset_event, "CoreAnimationUpdate", peer, comma);
}

void Cluster::js_arrows(ofstream &outfile)
{
	vector<group_t *>::iterator it;
	list<event_t *>::iterator event_it;
	bool comma = false;

	outfile <<"var arrows = [" << endl;
	for (it = nodes.begin(); it != nodes.end(); it++) {
		list<event_t *> container = (*it)->get_container();
		if (container.size() == 0)
			continue;

		for (event_it = container.begin(); event_it != container.end(); event_it++) {
			switch((*event_it)->get_event_id()) {
				case MSG_EVENT:
					message_edge(outfile, *event_it, &comma);
					break;
				case DISP_ENQ_EVENT: 
				case DISP_DEQ_EVENT: 
					dispatch_edge(outfile, *event_it, &comma);
					break;
				case TMCALL_CREATE_EVENT:
					timercall_edge(outfile, *event_it, &comma);
					break;
				case MR_EVENT:
					mkrun_edge(outfile, *event_it, &comma);
					break;
				case WAIT_EVENT:
					wait_label(outfile, *event_it, &comma);
					break;
				case CA_SET_EVENT:
					coreannimation_edge(outfile, *event_it, &comma);
					break;
				default:
					break;
			}
		}
		//js_edge(outfile, last_event, "deactivate", NULL);
	}
	outfile << "];" << endl;
}

void Cluster::js_cluster(ofstream &outfile)
{
	js_lanes(outfile);
	js_groups(outfile);
	js_arrows(outfile);
}

void Cluster::decode_edges(ofstream &outfile)
{
	vector<rel_t>::iterator edge_it;
	sort_edges();
	for (edge_it = edges.begin(); edge_it != edges.end(); edge_it++) {
		uint32_t type = edge_it->rel_type;
		event_t *from_event = edge_it->e_from;
		event_t *to_event = edge_it->e_to;

		outfile << hex << edge_it->g_from->get_group_id();
		outfile << "\t" << from_event->get_procname() << ", ";
		outfile << hex << edge_it->g_to->get_group_id();
		outfile << "\t" << to_event->get_procname() <<  ", ";

		switch (type) {
			case MSGP_REL: {
				string msg1_type = from_event->get_op();
				string msg2_type = to_event->get_op();
				if (msg1_type == "MACH_IPC_msg_send" && msg2_type == "MACH_IPC_msg_recv")
					outfile << "MACH_MSG_SEND";
				else if (msg1_type == "MACH_IPC_msg_recv" && msg2_type == "MACH_IPC_msg_send")
					outfile << "MACH_MSG_PASS";
				else
					outfile << "MACH_MSG_UNKNOWN";
				break;
			}
			case MKRUN_REL:
				outfile << "MK_RUN";
				break;
			case DISP_EXE_REL:
				outfile << "DISPATCH_EXECUTE";
				break;
			case DISP_DEQ_REL:
				outfile << "DISPATCH_DEQUEUE";
				break;
			case CALLOUT_REL:
				outfile << "TIMER_CALLOUT"; 
				break;
			case CALLOUTCANCEL_REL:
				outfile << "CANCEL_CALLOUT";
				break;
			case CA_REL:
				outfile << "COREANNIMATION";
				break;
			case BRTRAP_REL:
				outfile << "HWBR_SHARED_VAR";
				break;
			default:
				outfile << "UNKNOWN " << type;
				break;
		}

		outfile << ", " << fixed << setprecision(1) << from_event->get_abstime();
		outfile << ", " << fixed << setprecision(1) << to_event->get_abstime();
		outfile << endl;
	}
}

void Cluster::decode_cluster(ofstream &outfile)
{
	list<event_t *> cluster_events;
	vector<group_t *>::iterator it;

	for (it = nodes.begin(); it != nodes.end(); it++) {
		list<event_t *> container = (*it)->get_container();
		cluster_events.insert(cluster_events.end(), container.begin(), container.end());
	}

	cluster_events.sort(Parse::EventComparator::compare_time);
	list<event_t *>::iterator jt;
	for (jt = cluster_events.begin(); jt != cluster_events.end(); jt++)
		(*jt)->decode_event(0, outfile);
}

void Cluster::streamout_cluster(ofstream &outfile)
{
	list<event_t *> cluster_events;
	vector<group_t *>::iterator it;
	
	for (it = nodes.begin(); it != nodes.end(); it++) {
		list<event_t*> container = (*it)->get_container();
		cluster_events.insert(cluster_events.end(), container.begin(), container.end());
	}

	outfile << "#Events in cluster" << cluster_events.size() << endl;
	cluster_events.sort(Parse::EventComparator::compare_time);
	list<event_t *>::iterator jt;
	for (jt = cluster_events.begin(); jt != cluster_events.end(); jt++)
		(*jt)->streamout_event(outfile);
}
