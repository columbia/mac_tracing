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

Cluster::Cluster(group_t * group)
{
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

void Cluster::add_edge(group_t *g1, group_t *g2, event_t *t1, event_t *t2, uint32_t type)
{
	struct rel_t edge = {g1, g2, t1, t2, type};
	edges.push_back(edge);
}

void Cluster::add_node(group_t *g)
{
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
}

bool Cluster::remove_edge(const rel_t edge)
{
	vector<rel_t>::iterator it = find_if(edges.begin(), edges.end(), 
					[&target_edge = edge]
					(const rel_t & m) -> bool{return target_edge == m;});
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

void Cluster::append_nodes(vector<group_t *> & n)
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

void Cluster::append_edges(vector<rel_t> & g)
{
	edges.insert(edges.end(), g.begin(), g.end());
}

bool Cluster::compare_time(group_t * node1, group_t * node2)
{
	double timestamp1 = node1->get_container().front()->get_abstime();
	double timestamp2 = node2->get_container().front()->get_abstime();
	if (timestamp1 - timestamp2 <= 10e-8 && timestamp1-timestamp2 >= -10e-8)
		return (node1->get_container().front()->get_tid() < node2->get_container().front()->get_tid());
	return (timestamp1 < timestamp2);
}

void Cluster::sort_nodes()
{
	sort(nodes.begin(), nodes.end(), &Cluster::compare_time);
}

void Cluster::decode_edges(ofstream &outfile)
{
	vector<rel_t>::iterator edge_it;
	for (edge_it = edges.begin(); edge_it != edges.end(); edge_it++) {
		//group_t * from = (*edge_it).g_from;
		//group_t * to = (*edge_it).g_to;
		uint32_t type = (*edge_it).rel_type;
		event_t * from_event = (*edge_it).e_from;
		event_t * to_event = (*edge_it).e_to;
		
		outfile << from_event->get_procname();
		if (from_event->check_infected())
			outfile << "#";
		outfile << "\t" << to_event->get_procname();
		if (to_event->check_infected())
			outfile << "*";
		outfile << "\t";
		switch (type) {
			case 0 : {
				string msg1_type = from_event->get_op();
				string msg2_type = to_event->get_op();
				if (msg1_type == "MACH_IPC_msg_send"
					&& msg2_type == "MACH_IPC_msg_recv") {
					outfile << "MACH_MSG_SEND"; 
				} else if (msg1_type == "MACH_IPC_msg_recv" && msg2_type == "MACH_IPC_msg_send") {
					outfile << "MACH_MSG_PASS";
				} else {
					outfile << "MACH_MSG_UNKNOWN";
				}
				break;
			}
			case 1 : outfile << "MK_RUN";
					break;
			case 2 : outfile << "DISPATCH_EXECUTE";
					break;
			case 3 : outfile << "TIMER_CALLOUT"; 
					break;
			case 4 : outfile << "CANCEL_CALLOUT";
					break;
			default: outfile << "UNKNOWN";
					break;
		}
		outfile << "\t" << fixed << setprecision(1) << from_event->get_abstime();
		outfile << "\t" << fixed << setprecision(1) << to_event->get_abstime();
		outfile << endl;
	}
}

void Cluster::decode_cluster(ofstream &outfile)
{
	list<event_t *> cluster_events;
	vector<group_t *>::iterator it;

	for (it = nodes.begin(); it != nodes.end(); it++) {
		list<event_t*> container = (*it)->get_container();
		cluster_events.insert(cluster_events.end(), container.begin(), container.end());
	}

	cluster_events.sort(Parse::EventComparator::compare_time);
	list<event_t *>::iterator jt;
	for (jt = cluster_events.begin(); jt != cluster_events.end(); jt++) {
		(*jt)->decode_event(0, outfile);
	}
}

void Cluster::streamout_cluster(ofstream &outfile)
{
	list<event_t *> cluster_events;
	vector<group_t *>::iterator it;
	
	for (it = nodes.begin(); it != nodes.end(); it++) {
		list<event_t*> container = (*it)->get_container();
		cluster_events.insert(cluster_events.end(), container.begin(), container.end());
	}
	outfile << "Size of cluster " << cluster_events.size() << endl;
	cluster_events.sort(Parse::EventComparator::compare_time);
	list<event_t *>::iterator jt;
	for (jt = cluster_events.begin(); jt != cluster_events.end(); jt++) {
		(*jt)->streamout_event(outfile);
	}
}

void Cluster::push_connectors(group_t * group, event_t * exclude)
{
	list<event_t*> events = group->get_container();
	list<event_t *>::iterator it;
	event_t * event;
	msg_ev_t * mach_msg_event;
	blockinvoke_ev_t * invoke_event;
	dequeue_ev_t * deq_event;
	timercallout_ev_t * timercallout_event;
	mkrun_ev_t *mr_event;

	for (it = events.begin(); it != events.end(); it++) {
		event = *it;
		if (event == exclude)
			continue;

		mach_msg_event = dynamic_cast<msg_ev_t *>(event);
		if (mach_msg_event) {
			connectors.push_back(mach_msg_event);
			continue;
		}

		invoke_event = dynamic_cast<blockinvoke_ev_t*>(event);
		if (invoke_event) {
			if (invoke_event->is_begin())
				connectors.push_back(invoke_event);
			continue;
		}

		deq_event = dynamic_cast<dequeue_ev_t*>(event);
		if (deq_event) {
			connectors.push_back(deq_event);
			continue;
		}

		timercallout_event = dynamic_cast<timercallout_ev_t*>(event);
		if (timercallout_event) {
			connectors.push_back(timercallout_event);
			continue;
		}
		
		mkrun_ev_t *mr_event = dynamic_cast<mkrun_ev_t *>(event);
		if (mr_event) {
			connectors.push_back(mr_event);
			continue;
		}
	}
}
