#include "search_graph.hpp"

GraphSearcher::GraphSearcher(Graph *_graph, map<event_id_t, bool> event_for_compare)
{
	if (event_for_compare.size() == 0) {
		key_events = load_default_key_events();
	} else {
		key_events = event_for_compare;
	}
	graph = _graph;
}

GraphSearcher::~GraphSearcher()
{
	//clear all normnodes
	map<tid_t, vector<NormNode *> >::iterator it;
	for (it = tid_normnodes_map.begin(); it != tid_normnodes_map.end(); it++) {
		vector<NormNode *> &norm_vector = it->second;
		vector<NormNode *>::iterator normnode_it;
		for (normnode_it = norm_vector.begin(); normnode_it != norm_vector.end(); normnode_it++) {
			delete *normnode_it;
		}
	}
	tid_normnodes_map.clear();
	key_events.clear();
}

map<event_id_t, bool> GraphSearcher::load_default_key_events()
{
	map<event_id_t, bool> key_events;
	key_events.insert(make_pair(MSG_EVENT, true));
	key_events.insert(make_pair(MR_EVENT, true));
	key_events.insert(make_pair(FAKED_WOKEN_EVENT, true));
	key_events.insert(make_pair(INTR_EVENT, false));
	key_events.insert(make_pair(WQNEXT_EVENT, false));
	key_events.insert(make_pair(TSM_EVENT, false));
	key_events.insert(make_pair(WAIT_EVENT, true));
	key_events.insert(make_pair(DISP_ENQ_EVENT, true));
	key_events.insert(make_pair(DISP_DEQ_EVENT, true));
	key_events.insert(make_pair(DISP_INV_EVENT, true));
	key_events.insert(make_pair(TMCALL_CREATE_EVENT, false));
	key_events.insert(make_pair(TMCALL_CANCEL_EVENT, false));
	key_events.insert(make_pair(TMCALL_CALLOUT_EVENT, false));
	key_events.insert(make_pair(BACKTRACE_EVENT, true));
	key_events.insert(make_pair(SYSCALL_EVENT, true));
	key_events.insert(make_pair(BREAKPOINT_TRAP_EVENT, true));
	key_events.insert(make_pair(RL_OBSERVER_EVENT, true));
	key_events.insert(make_pair(EVENTREF_EVENT, true));
	key_events.insert(make_pair(NSAPPEVENT_EVENT, true));
	key_events.insert(make_pair(DISP_MIG_EVENT, false));
	key_events.insert(make_pair(RL_BOUNDARY_EVENT, true));
	return key_events;
}

Node *GraphSearcher::spinning_node()
{
	return graph->get_spinning_node();
}

vector<NormNode *> GraphSearcher::update_normnodes_for_thread(tid_t tid)
{
	if (tid_normnodes_map.find(tid) != tid_normnodes_map.end())
		return tid_normnodes_map[tid];

	vector<NormNode *> result;
	vector<Node *> nodes_for_tid = graph->get_nodes_for_tid(tid);
	vector<Node *>::iterator it;
	for(it = nodes_for_tid.begin(); it != nodes_for_tid.end(); it++) {
		NormNode *node = new NormNode(*it, key_events);
		result.push_back(node);
	}
	tid_normnodes_map[tid] = result;
	return result;
}

NormNode *GraphSearcher::norm_node_fast(Node *node)
{
	tid_t thread = node->get_tid();
	if (tid_normnodes_map.find(thread) == tid_normnodes_map.end()) {
		return NULL;
	}

	vector<Node *> nodes_for_tid;
	vector<Node *>::iterator node_it;
	int index;

	nodes_for_tid = graph->get_nodes_for_tid(thread);
	node_it = find(nodes_for_tid.begin(), nodes_for_tid.end(), node);
	index = distance(nodes_for_tid.begin(), node_it);
	assert(index >= 0 && index < nodes_for_tid.size());
	assert(tid_normnodes_map[thread][index]->get_node() == node);

	return tid_normnodes_map[thread][index];
}

vector<Node *> GraphSearcher::get_similar_nodes_in_thread_before(Node *node, int number)
{
	vector<NormNode *> normnodes_for_thread = update_normnodes_for_thread(node->get_tid());
	vector<Node *> ret;
	NormNode *normnode = norm_node_fast(node);
	
	if(normnode->is_empty()) {
		cerr << "Search empty normed node for the given key events" << endl;
		cerr << "normnode id " << hex << node->get_group()->get_group_id() << endl;
		return ret;
	}
	
	vector<NormNode *>::iterator it
		= find(normnodes_for_thread.begin(), normnodes_for_thread.end(), normnode);
	int index = distance(normnodes_for_thread.begin(), it) - 1;

	if (index <= 0) {
		cerr << "No proceedings for search" << endl;
		return ret;
	}
	assert(index > 0);

	number = index > 2 * number ? number: index / 2;
	for (int i = 0; i < index - 2 * number; index++) {
		bool matched = true;
		for (int j = i; j < i + number; j++) {
			if (*(normnodes_for_thread[j]) != *(normnodes_for_thread[index - number + (j-i)])) {
				matched = false;
				break;			
			}
		}
		if (matched == true) {
			ret.push_back(normnodes_for_thread[i + number]->get_node());
		}
	}
	return ret;
}

vector<Node *> GraphSearcher::get_similar_nodes_in_thread_before(Node *node)
{
	vector<NormNode *> normnodes_for_thread = update_normnodes_for_thread(node->get_tid());
	vector<NormNode *>::iterator it, end;

	vector<Node *> ret;
	NormNode *normnode = norm_node_fast(node);

	if(normnode->is_empty()) {
		cerr << "Search empty normed node for the given key events" << endl;
		cerr << "normnode id " << hex << node->get_group()->get_group_id() << endl;
		return ret;
	}

	end = find(normnodes_for_thread.begin(), normnodes_for_thread.end(), normnode);
	assert(end != normnodes_for_thread.end() && *end == normnode);

	for (it = normnodes_for_thread.begin(); it != end; it++) {
		NormNode *cur_node = *it;
		if (cur_node->is_empty())
			continue;

		if (cur_node->get_node()->get_end_time() > node->get_begin_time())
			break; // only get similar group before current group
		
		assert(cur_node != normnode);
		if (*cur_node == *normnode)
			ret.push_back(cur_node->get_node());
	}
	return ret;
}

vector<Node *> GraphSearcher::get_similar_nodes_in_thread_after(Node *node, double deadline)
{
	tid_t thread;
	vector<NormNode *> normnodes_for_thread;
	vector<NormNode *>::iterator it;
	vector<Node *> ret;
	ret.clear();
	
	thread = node->get_tid();
	normnodes_for_thread = update_normnodes_for_thread(thread);
	NormNode *normnode = norm_node_fast(node);
	if(normnode->is_empty()) {
		cerr << "Search empty normed node for the given key events" << endl;
		cerr << "normnode id " << hex << node->get_group()->get_group_id() << endl;
		return ret;
	}

	it = find(normnodes_for_thread.begin(), normnodes_for_thread.end(), normnode);
	assert(it != normnodes_for_thread.end() && *it == normnode);

	for (it++; it != normnodes_for_thread.end(); it++) {
		NormNode *cur_node = *it;

		if (cur_node->get_node()->get_begin_time() > deadline)
			break; // only get similar group before current group
		
		assert(cur_node != normnode);
		if (cur_node->is_empty())
			continue;
		if (*cur_node == *normnode)
			ret.push_back(cur_node->get_node());
	}
	return ret;
}

vector<Node *> GraphSearcher::get_counterpart_on_syscall(Node *node, vector<Node *> similar_nodes)
{
	vector<Node *> return_nodes;
	vector<Node *>::iterator it;
	for (it = similar_nodes.begin(); it != similar_nodes.end(); it++) {
		Node *cur_node = *it;
		cerr << "Compare normal node with " << cur_node->get_gid() << endl;
		int ret = node->compare_syscall_ret(cur_node);
		cerr << "ret (no difference = 0, find minor difference = 1) " << ret << endl;
		if (ret != 0) 
			return_nodes.push_back(*it);
	}
	return return_nodes;
}

vector<Node *> GraphSearcher::get_counterpart_on_timespan(Node *node, vector<Node *> similar_nodes)
{
	vector<Node *> return_nodes;
	vector<Node *>::iterator it;
	for (it = similar_nodes.begin(); it != similar_nodes.end(); it++) {
		int ret = node->compare_timespan(*it);
		if (ret != 0) 
			return_nodes.push_back(*it);
	}
	return return_nodes;
}

vector<Node *> GraphSearcher::get_counterpart_on_waitcost(Node *node, vector<Node *>similar_nodes)
{
	vector<Node *> return_nodes;
	vector<Node *>::iterator it;
	for (it = similar_nodes.begin(); it != similar_nodes.end(); it++) {
		int ret = node->compare_wait_cost(*it);
		if (ret != 0) 
			return_nodes.push_back(*it);
	}
	return return_nodes;
	
}

vector<Node *> GraphSearcher::wakeup_node(Node *counter_node)
{
	map<event_t *, Edge *>out_edges = counter_node->get_out_edges();
	map<event_t *, Edge *>::iterator it;
	vector<Node *> ret;
	
	for (it = out_edges.begin(); it != out_edges.end(); it++) {
		wait_ev_t *wait = dynamic_cast<wait_ev_t *>(it->first);
		if (wait != NULL) {
			cout << "Wait event at " << fixed << setprecision(1) << wait->get_abstime() << endl;
			ret.push_back(it->second->to);
		}
	}
	return ret;
}

bool GraphSearcher::compare_prev_step_for_nodes(Node *target_node, Node *peer_node)
{
	map<event_t *, Edge *> in_edges_target = target_node->get_in_edges();
	map<event_t *, Edge *> in_edges_peer = peer_node->get_in_edges();
	if (in_edges_target.size() != in_edges_peer.size())
		return false;

	map<event_t *, Edge *>::iterator target_it, peer_it;
	for (target_it = in_edges_target.begin(), peer_it = in_edges_peer.begin();
		target_it != in_edges_target.end() && peer_it != in_edges_peer.end();
		target_it++, peer_it++) {
		bool ret = true;
		Edge *target_edge = target_it->second;
		Edge *peer_edge = peer_it->second;

		NormNode *target_from = new NormNode(target_edge->from, key_events);
		NormNode *peer_from = new NormNode(peer_edge->from, key_events);
		if (target_from != peer_from)
			ret = false;
		delete target_from;
		delete peer_from;
		if (ret == false)
			return ret;
	}
	return true;
}

vector<Node *> GraphSearcher::get_peers(map<event_t *, Edge*> &in_edges)
{
	vector<Node *>ret;
	ret.clear();
	map<event_t *, Edge *>::iterator it;
	for (it = in_edges.begin(); it != in_edges.end(); it++) {
		Node *peer = it->second->from;
		if (peer != it->second->to)
			ret.push_back(peer);
	}
	return ret;
}

Node * GraphSearcher::get_prev_node_in_thread(Node *node)
{
	vector<Node *> nodes_for_tid = graph->get_nodes_for_tid(node->get_tid());
	vector<Node *>::iterator it = find(nodes_for_tid.begin(), nodes_for_tid.end(), node);
	assert(it != nodes_for_tid.end());
	int index = distance(nodes_for_tid.begin(), it);
	for (int i = index - 1; i > 0 ; i--)  {
		if (nodes_for_tid[i]->get_end_time() > node->get_end_time())
			return nodes_for_tid[i];		
	}
	return NULL;
}

vector<Node *> GraphSearcher::slice_path_for_node(Node *node, bool interference)
{
	vector<Node *> path;
	for (;;) {
		path.push_back(node);
		group_id_t group_id = -1;
		vector<Node *> peer_nodes = get_peers(node->get_in_edges());
		if (peer_nodes.size() == 0) {
			if (interference == true) {
				//gnu plot current node
				cout << "Input the node id [exit with 0]: " << endl;
				cin >> hex >> group_id;
				node = graph->id_to_node(group_id);
			} else {
				Node *p = get_prev_node_in_thread(node);			
				if (p)
					peer_nodes.push_back(p);
			}
		}

		if (peer_nodes.size() > 0) {
			if (interference == true) {
				//printout all th nodes in peer_nodes
				//gnuplot current node and the possible prededent;
				cout << "Input the node id [exit with 0]: " << endl;
				cin >> hex >> group_id;
				node = graph->id_to_node(group_id);
			} else {
				node = peer_nodes.back();
			}
		} else {
			cout << "No further nodes can be searched" << endl;
			break;
		}
	}
	return path;
}

//true : equal
//false: not equal
bool GraphSearcher::compare_node(Node *cur_normal_node, Node *potential_buggy_node)
{
	//compare the system call ret
	//compare the blocking time of wait
	return potential_buggy_node->compare_syscall_ret(cur_normal_node) == 0 &&
		potential_buggy_node->compare_wait_cost(cur_normal_node) == 0;
}

void GraphSearcher::examine_path_backward(Node *buggy_node, vector<Node *> normal_path_for_compare, bool interference)
{
	vector<Node *>::iterator it = normal_path_for_compare.begin();

	for (it++; it != normal_path_for_compare.end(); it++) {
		Node *cur_normal_node = *it;
		//vector<Node *> buggy_nodes = get_similar_nodes_in_thread_after(cur_normal_node, cur_normal_node->get_end_time());
		vector<Node *> buggy_nodes = graph->get_nodes_in_thread_after(cur_normal_node);

		if (buggy_nodes.size() == 0) {
			cout << "probably blocking in the thread " << hex << cur_normal_node->get_tid() << endl;
		} else {
		//check the nodes
			vector<Node *>::iterator it;
			int count = 10;
			for (it = buggy_nodes.begin(); it != buggy_nodes.end() && count > 0; it++) {
				count--;
				if ((*it)->wait_over() == true)
					cout << "probably blocking in the thread " << hex << cur_normal_node->get_tid() << endl;
			}
		}
		
		int if_continue = -1;
		if (interference == true) {
		//plot buggy nodes and cur_normal_node
			cout << "to continue check next node in the path , please enter 1; others to stop" << endl;
			cin >> if_continue;
		} else {
			//vector<Node *> counterparts = get_counterpart_on_syscall(cur_normal_node, buggy_nodes);
			vector<Node *> counterparts = get_counterpart_on_waitcost(cur_normal_node, buggy_nodes);
			//get_counterpart_on_waitcost
			if (counterparts.size() > 0)
				if_continue = compare_node(cur_normal_node, counterparts.back())? 1 : 0;
			else
				if_continue = compare_node(cur_normal_node, buggy_nodes.back())? 1 : 0;
		}
		if (if_continue == 1)
			continue;
		//plot the buggy node
	}
}
