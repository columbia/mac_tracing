#include "search_graph.hpp"
#include "js_graph.hpp"
#include "loguru.hpp"

GraphSearcher::GraphSearcher(Graph *_graph, 
	std::map<EventType::event_type_t, bool> event_for_compare)
{
	key_events = event_for_compare.size() == 0\
			? load_default_key_events()\
			: event_for_compare;
    graph = _graph;
	anomaly_node = nullptr;
    tid_normnodes_map.clear();
    similar_nodes.clear();
    path.clear();
    weak_edge_node_ids.clear();
    multi_edge_node_ids.clear();
	suspicious_nodes.clear();
	cached_selection.clear();
    stopping_node = nullptr;
}

GraphSearcher::GraphSearcher(Graph *_graph)
{
    graph = _graph;
	anomaly_node = nullptr;
    key_events = load_default_key_events();
    tid_normnodes_map.clear();
    similar_nodes.clear();
    path.clear();
    weak_edge_node_ids.clear();
    multi_edge_node_ids.clear();
	stopping_node = nullptr;
}

GraphSearcher::~GraphSearcher()
{
    //clear all normnodes
	for (auto element: tid_normnodes_map){
        std::vector<NormNode *> &norm_group_vector = element.second;
		for (auto normnode:norm_group_vector)
            delete normnode;
    }
    tid_normnodes_map.clear();
    similar_nodes.clear();
    path.clear();
    weak_edge_node_ids.clear();
    multi_edge_node_ids.clear();
    key_events.clear();
}

std::map<EventType::event_type_t, bool> GraphSearcher::load_default_key_events()
{
    std::map<EventType::event_type_t, bool> key_events;
    key_events.insert(std::make_pair(MSG_EVENT, true));
    key_events.insert(std::make_pair(MR_EVENT, false));
    key_events.insert(std::make_pair(FAKED_WOKEN_EVENT, false));
    key_events.insert(std::make_pair(INTR_EVENT, false));
    key_events.insert(std::make_pair(WQNEXT_EVENT, false));
    key_events.insert(std::make_pair(TSM_EVENT, false));
    key_events.insert(std::make_pair(WAIT_EVENT, true));
    key_events.insert(std::make_pair(DISP_ENQ_EVENT, true));
    key_events.insert(std::make_pair(DISP_DEQ_EVENT, true));
    key_events.insert(std::make_pair(DISP_INV_EVENT, true));
    key_events.insert(std::make_pair(TMCALL_CREATE_EVENT, false));
    key_events.insert(std::make_pair(TMCALL_CANCEL_EVENT, false));
    key_events.insert(std::make_pair(TMCALL_CALLOUT_EVENT, false));
    key_events.insert(std::make_pair(BACKTRACE_EVENT, true));
    key_events.insert(std::make_pair(SYSCALL_EVENT, true));
    key_events.insert(std::make_pair(BREAKPOINT_TRAP_EVENT, true));
    key_events.insert(std::make_pair(RL_OBSERVER_EVENT, true));
    key_events.insert(std::make_pair(EVENTREF_EVENT, true));
    key_events.insert(std::make_pair(NSAPPEVENT_EVENT, true));
    key_events.insert(std::make_pair(DISP_MIG_EVENT, true));
    key_events.insert(std::make_pair(RL_BOUNDARY_EVENT, true));
    return key_events;
}

std::vector<NormNode *> GraphSearcher::update_normnodes_for_thread(tid_t tid)
{
    if (tid_normnodes_map.find(tid) != tid_normnodes_map.end())
        return tid_normnodes_map[tid];

    std::vector<NormNode *> result;
    std::vector<Node *> nodes_for_tid = graph->get_nodes_for_tid(tid);
    std::vector<Node *>::iterator it;
    for(it = nodes_for_tid.begin(); it != nodes_for_tid.end(); it++) {
        assert(*it != nullptr);
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
        return nullptr;
    }

    std::vector<Node *> nodes_for_tid;
    std::vector<Node *>::iterator node_it;
    int index;

    nodes_for_tid = graph->get_nodes_for_tid(thread);
    node_it = find(nodes_for_tid.begin(), nodes_for_tid.end(), node);
    index = distance(nodes_for_tid.begin(), node_it);
    assert(index >= 0 && index < nodes_for_tid.size());
    assert(tid_normnodes_map[thread][index]->get_node() == node);

    return tid_normnodes_map[thread][index];
}

std::vector<Node *> GraphSearcher::get_prev_similar_nodes(Node *node, int number)
{
    if (number == 0)
        return get_prev_similar_nodes(node);

    std::vector<NormNode *> normnodes_for_thread = update_normnodes_for_thread(node->get_tid());
    //normnodes_for_thread = tid_normnodes_map[node->get_tid()];
    std::vector<Node *> ret;
    NormNode *normnode = norm_node_fast(node);
    assert(normnode != nullptr);
    
    if(normnode->is_empty()) {
		
        std::cerr << "Search empty normed node for the given key events" << std::endl;
        std::cerr << "normnode id " << std::hex << node->get_group()->get_group_id() << std::endl;
        return ret;
    }
    
    std::vector<NormNode *>::iterator it
        = std::find(normnodes_for_thread.begin(), normnodes_for_thread.end(), normnode);
    int index = distance(normnodes_for_thread.begin(), it);

    if (index <= 0) {
        std::cerr << "No proceedings for search" << std::endl;
        return ret;
    }
    assert(index > 0);

    number = index / 2 >  number ? number: index / 2;
    for (int i = 0; i <= index - 2 * number; i++) {
        bool matched = true;
        for (int j = i; j < i + number; j++) {
            if (*(normnodes_for_thread[j]) != *(normnodes_for_thread[index - number + (j-i)])) {
                matched = false;
                break;            
            }
        }
        if (matched == true)
            ret.push_back(normnodes_for_thread[i + number]->get_node());
    }
    similar_nodes = ret;
    return ret;
}

std::vector<Node *> GraphSearcher::get_prev_similar_nodes(Node *node)
{
    std::vector<NormNode *> normnodes_for_thread = update_normnodes_for_thread(node->get_tid());
    std::vector<NormNode *>::iterator it, end;

    std::vector<Node *> ret;
    NormNode *target_node = norm_node_fast(node);
    assert(target_node != nullptr);

    if(target_node->is_empty()) {
        std::cerr << "Search empty normed node for the given key events" << std::endl;
        std::cerr << "target node id " << std::hex << node->get_group()->get_group_id() << std::endl;
        return ret;
    }

    end = find(normnodes_for_thread.begin(), normnodes_for_thread.end(), target_node);
    assert(end != normnodes_for_thread.end() && *end == target_node);

    for (it = normnodes_for_thread.begin(); it != end; it++) {
        NormNode *cur_node = *it;
        if (cur_node->is_empty())
            continue;
        if (cur_node->get_node()->get_end_time() > node->get_begin_time())
            break; // only get similar group before current group
        if (*target_node == *cur_node)
            ret.push_back(cur_node->get_node());
    }
    return ret;
}

std::vector<Node *> GraphSearcher::get_post_similar_nodes(Node *node, double deadline)
{
    tid_t thread;
    std::vector<NormNode *> normnodes_for_thread;
    std::vector<NormNode *>::iterator it;
    std::vector<Node *> ret;
    ret.clear();
    
    thread = node->get_tid();
    normnodes_for_thread = update_normnodes_for_thread(thread);
    NormNode *normnode = norm_node_fast(node);
    if(normnode->is_empty()) {
        std::cerr << "Search empty normed node for the given key events" << std::endl;
        std::cerr << "normnode id " << std::hex << node->get_group()->get_group_id() << std::endl;
        return ret;
    }

    it = find(normnodes_for_thread.begin(), normnodes_for_thread.end(), normnode);
    assert(it != normnodes_for_thread.end() && *it == normnode);

    for (it++; it != normnodes_for_thread.end(); it++) {
        NormNode *cur_node = *it;
        if (deadline > 0.0 && cur_node->get_node()->get_begin_time() > deadline)
            break; 
        if (cur_node->is_empty())
            continue;
        if (*cur_node == *normnode)
            ret.push_back(cur_node->get_node());
    }
    return ret;
}

std::vector<Node *> GraphSearcher::get_baseline_on_syscall(Node *node)
{
    std::vector<Node *> return_nodes;
    std::vector<Node *>::iterator it;
    for (it = similar_nodes.begin(); it != similar_nodes.end(); it++) {
        Node *cur_node = *it;
#if DEBUG_SEARCH
        std::cerr << "Compare normal node with " << cur_node->get_gid() << std::endl;
#endif
        int ret = node->compare_syscall_ret(cur_node);
#if DEBUG_SEARCH
        std::cerr << "ret (no difference = 0, find minor difference = 1) " << ret << std::endl;
#endif
        if (ret != 0) 
            return_nodes.push_back(*it);
    }
    return return_nodes;
}

std::vector<Node *> GraphSearcher::get_baseline_on_timespan(Node *node)
{
    std::vector<Node *> return_nodes;
    std::vector<Node *>::iterator it;
    for (it = similar_nodes.begin(); it != similar_nodes.end(); it++) {
        int ret = node->compare_timespan(*it);
        if (ret != 0) 
            return_nodes.push_back(*it);
    }
    return return_nodes;
}

std::vector<Node *> GraphSearcher::get_baseline_on_waitcost(Node *node)
{
    std::vector<Node *> return_nodes;
    std::vector<Node *>::iterator it;
    for (it = similar_nodes.begin(); it != similar_nodes.end(); it++) {
        int ret = node->compare_wait_cost(*it);
        if (ret != 0) 
            return_nodes.push_back(*it);
    }
    return return_nodes;
    
}

std::vector<Node *> GraphSearcher::search_baseline_nodes(Node *cur_anomaly)
{
    std::vector<Node *> similar_nodes;
    similar_nodes.clear();

	if (cur_anomaly == nullptr)
		return similar_nodes;

    std::cout << "Spin vertex 0x" << std::hex << cur_anomaly->get_group()->get_group_id() << std::endl;
    int type = spinning_type(cur_anomaly);

    if (type == GraphSearcher::SPINNING_WAIT) {
		std::cout << "Long wait: try to get similar node" << std::endl;
        similar_nodes = get_prev_similar_nodes(cur_anomaly);   
        if (similar_nodes.size() > 0) {
            return get_baseline_on_waitcost(cur_anomaly);
        }
    }

    if (type == GraphSearcher::SPINNING_YIELD) {
		std::cout << "Long yield: try to get similar node" << std::endl;
        similar_nodes = get_prev_similar_nodes(cur_anomaly, 1);
        if (similar_nodes.size() > 0) {
            return get_baseline_on_timespan(cur_anomaly);
        }
    }

	std::cout << "No similar node found." << std::endl;
    return similar_nodes;
}

//true : equal -> normal execution
//false: not equal -> anomaly
/*
bool GraphSearcher::compare_node(Node *normal_node, Node *node)
{
    //compare the system call ret
    //compare the blocking time of wait
    return node->compare_syscall_ret(normal_node) == 0 &&
        node->compare_wait_cost(normal_node) == 0;
}
*/

int GraphSearcher::select_base_on_cached_node(Node *cur_node)
{
	//compare the incoming edges;
	//if the edges of the same type and same peer therad
	//return the index
	auto incoming_edges = get_incomming_edges(cur_node);
    LOG_S(INFO) << "search cache for node 0x"\
			 << std::hex << cur_node->get_gid() << std::endl;

	for (auto element: cached_selection) { //check every cache item
		auto cached_edges = get_incomming_edges(element.first);
		assert(cached_edges.size());
		if (cached_edges.size() != incoming_edges.size())
			continue;

		auto cache_it = cached_edges.begin();
		auto cur_it = incoming_edges.begin();
		bool found = true;
		for (; cache_it != cached_edges.end() && cur_it != incoming_edges.end();
			cache_it++, cur_it++) {
			if ((*cache_it)->rel_type != (*cur_it)->rel_type
				|| (*cache_it)->e_from->get_tid() != (*cur_it)->e_from->get_tid()
				|| (*cache_it)->e_to->get_tid() != (*cur_it)->e_to->get_tid()) {
				found = false;
				break;
			}
		}
		if (found == true) {
			LOG_S(INFO) << "cache hit: 0x" << std::hex << element.first->get_gid()\
				<< "\t select edge 0x" << std::hex << element.second << std::endl;
			return element.second;
		}
	}

	std::cout << "cache missed" << std::endl;
	return -1;
}

Node *GraphSearcher::get_anomaly_node()
{
    if (anomaly_node == nullptr)
        anomaly_node = graph->get_spinning_node();
    return anomaly_node;
}

bool GraphSearcher::set_anomaly_node(uint64_t gid)
{
	anomaly_node = graph->id_to_node(gid);
	return anomaly_node != nullptr;
}

int GraphSearcher::spinning_type(Node *spinning_node)
{
    if (spinning_node == nullptr)
        return SPINNING_NONE;

    Group *spinning_group = spinning_node->get_group();
    if (spinning_group == nullptr)
        return SPINNING_NONE;

    if (spinning_group->get_last_event()->get_event_type() == WAIT_EVENT)
        return SPINNING_WAIT;

    std::list<EventBase *> events = spinning_group->get_container();
    std::list<EventBase *>::iterator it;
    int yield_freq = 0;
    for (it = events.begin(); it != events.end(); it++) {
        if ((*it)->get_event_type() == SYSCALL_EVENT
            && (*it)->get_op().find("thread_switch") != std::string::npos)
            yield_freq++;
    }
    if (yield_freq > 0 && events.size() / yield_freq < 2)
        return SPINNING_YIELD;

    return SPINNING_BUSY;
}

bool GraphSearcher::is_wait_spinning()
{
	if (anomaly_node == nullptr)
        return false;
    return spinning_type(anomaly_node) == SPINNING_WAIT;
}

std::string GraphSearcher::decode_spinning_type()
{
    return decode_spinning_type(spinning_type(get_anomaly_node()));
}

std::string GraphSearcher::decode_spinning_type(int type)
{
    switch(type) {
        case SPINNING_NONE:
            return "not spinning";
        case SPINNING_WAIT:
            return "long waiting";
        case SPINNING_YIELD:
            return "long yield";
        case SPINNING_BUSY:
            return "cpu busy";
    }
    return "not spinning";
}

bool GraphSearcher::cmp_edge_by_intime(Edge *e1, Edge *e2)
{
    return e1->e_to->get_abstime() > e2->e_to->get_abstime();
}

std::vector<Edge *> GraphSearcher::get_incomming_edges_before(Node *cur_node, double deadline)
{
	std::vector<Edge *> ret;
    ret.clear();
	for (auto element: cur_node->get_in_edges()) {
        if (element.second->from != element.second->to
			&& element.first->get_abstime() < deadline)
            ret.push_back(element.second);
    }

	for (auto element: cur_node->get_in_weak_edges()) {
        if (element.second->from != element.second->to)
            ret.push_back(element.second);
	}

	sort(ret.begin(), ret.end(), GraphSearcher::cmp_edge_by_intime);
    return ret;
}

std::vector<Edge *> GraphSearcher::get_strong_edges_before(Node *cur_node, double deadline)
{
	std::vector<Edge *> ret;
    ret.clear();
	for (auto element: cur_node->get_in_edges()) {
        if (element.second->from != element.second->to
			&& element.first->get_abstime() < deadline)
            ret.push_back(element.second);
    }

	sort(ret.begin(), ret.end(), GraphSearcher::cmp_edge_by_intime);
    return ret;
}

std::vector<Edge *> GraphSearcher::get_incomming_edges(Node *cur_node)
{
	std::vector<Edge *> ret;
    ret.clear();
	for (auto element: cur_node->get_in_edges()) {
        if (element.second->from != element.second->to)
            ret.push_back(element.second);
    }
	for (auto element: cur_node->get_in_weak_edges()) {
        if (element.second->from != element.second->to)
            ret.push_back(element.second);
	}

	sort(ret.begin(), ret.end(), GraphSearcher::cmp_edge_by_intime);
    return ret;
}

std::vector<Edge *> GraphSearcher::get_strong_edges(Node *cur_node)
{
	std::vector<Edge *> ret;
    ret.clear();
	for (auto element: cur_node->get_in_edges()) {
        if (element.second->from != element.second->to)
            ret.push_back(element.second);
    }
	sort(ret.begin(), ret.end(), GraphSearcher::cmp_edge_by_intime);
    return ret;
}

Node *GraphSearcher::get_prev_node_in_thread(Node *node, Node **vent_node_ptr)
{
    *vent_node_ptr = nullptr;
    std::vector<Node *> nodes_in_thread = graph->get_nodes_for_tid(node->get_tid());
    std::vector<Node *>::iterator it = find(nodes_in_thread.begin(), nodes_in_thread.end(), node);
    assert(it != nodes_in_thread.end());
    int index = distance(nodes_in_thread.begin(), it);

    //first check if the an weak edge for the known batch processing blocks or runloops
    for (int i = index - 1; i > 0 ; i--)  {
        if (nodes_in_thread[i]->get_end_time() > node->get_end_time())
            return nodes_in_thread[i];        
        if (*vent_node_ptr == nullptr && 
			((nodes_in_thread[i]->get_in_edges().size() > 0) 
				|| (nodes_in_thread[i]->get_in_weak_edges().size() > 1)))
            *vent_node_ptr = nodes_in_thread[i];
    }

    if (index > 0)
        return nodes_in_thread[index - 1];
    return nullptr;
}


Node *GraphSearcher::prev_node_from_weak_edge(Node *node)
{
    Node *edged_node_ptr = nullptr;
    Node *weak_node = get_prev_node_in_thread(node, &edged_node_ptr);
    if (edged_node_ptr != nullptr)
        return edged_node_ptr;
    return weak_node;
}

void GraphSearcher::print_nodes(std::vector<Node *> t_nodes)
{
    int i = 0;
    for (auto node : t_nodes) { 
        LOG_S(INFO)<< "\tNode [" << i << "] " << std::hex << node->get_gid() << std::endl;
        i++;
    }
    LOG_S(INFO) << std::endl;
}

void GraphSearcher::print_incoming_edges(std::vector<Edge *> incoming_edges)
{

    Group *peer_group; 
    EventBase *peer_event;
    std::list<EventBase *> lists;
    int pos;

	for (auto edge: incoming_edges) {
        peer_group = edge->from->get_group();
        peer_event = edge->e_from;
        lists = peer_group->get_container();
        pos = distance(lists.begin(), std::find(lists.begin(), lists.end(), peer_event));

        std::cout << "\tEdge " << std::dec << edge->rel_type\
				  << " from Node 0x" << std::hex << peer_group->get_group_id()\
				  << " " << peer_event->get_procname() << std::endl;

        std::cout << "\t\tfrom event [ 0x" << std::hex << pos << "]  at "\
           	 	<< std::fixed << std::setprecision(1) << edge->e_from->get_abstime() << std::endl;
		std::cout << "\t\t";
        peer_event->streamout_event(std::cout);
		std::cout << std::endl;

		std::cout << "\t\tto event" << std::endl;
        std::cout << "\t\t";
		edge->e_to->streamout_event(std::cout);
        std::cout << std::endl;
    }
}

std::map<uint64_t, EventBase *> GraphSearcher::collapse_prevs(std::vector<Edge *> incoming_edges)
{
    std::map<uint64_t, EventBase *> ret;
    ret.clear();

    if (incoming_edges.size() == 0)
        return ret;
    for (auto edge : incoming_edges) {
        if (ret.find(edge->from->get_gid()) == ret.end())
            ret[edge->from->get_gid()] = edge->e_from;
    }

    return ret;
}  


//return the stopped node
uint64_t GraphSearcher::path_slice_from_node(Node *node, double deadline)
{
    if (node != nullptr && deadline < 0)
        deadline = node->get_group()->get_last_event()->get_abstime();

    for (; node != nullptr;) {
        path.push_back(node);

        if (node->contains_nsapp_event()) {
            LOG_S(INFO) << "Contains UI event in Node 0x" << std::hex << node->get_gid() << std::endl;
            return 0;
        }

        std::vector<Edge *> incoming_edges = get_incomming_edges_before(node, deadline);
        std::vector<Edge *> strong_edges = get_strong_edges_before(node, deadline);
        std::map<uint64_t, EventBase *> predecessors = collapse_prevs(incoming_edges);
        std::map<uint64_t, EventBase *> strong_predecessors = collapse_prevs(strong_edges);

        if (incoming_edges.size() == 0) {
			LOG_S(INFO) << "No Edges for search and stopped at 0x"\
				 << std::hex << node->get_gid() << std::endl;
			node = nullptr;
		} else if (strong_predecessors.size() == 1 
				&& (predecessors.size() == 1 || refine_edge(node))) {
            LOG_S(INFO) << "Node 0x" << std::hex << node->get_gid()\
				<< " has incoming edges from same node :" << std::endl;
            print_incoming_edges(incoming_edges);
            node = strong_edges[0]->from;
            deadline = strong_edges[0]->e_from->get_abstime();
		} else {
			if (strong_predecessors.size() == 0) {
				LOG_S(INFO) << "Weak Edge to Node 0x" << std::hex << node->get_gid() << std::endl;
				weak_edge_node_ids.push_back(node->get_gid());
				print_incoming_edges(incoming_edges);

				Node *candidate = prev_node_from_weak_edge(node);
				if (candidate)
					LOG_S(INFO) << "Next node with edge incident 0x" << std::hex << candidate->get_gid() << std::endl;
				//TODO:refine weak edges in the graph
				return node->get_gid();
			} else {
				LOG_S(INFO) << "Multiple Incoming Edges to 0x" << std::hex << node->get_gid() << std::endl;
				Edge *opt_e = heuristic_edge(node, 3);
				if (opt_e) {
					LOG_S(INFO) << "Heuristics for node 0x" << std::hex << node->get_gid()\
						 << ": 0x" << std::hex << opt_e->from->get_gid()\
						 << " at " << std::fixed << std::setprecision(1)\
						 << opt_e->e_to->get_abstime() << std::endl;
				} else {
					LOG_S(INFO) << "Heuristics failed" << std::endl;
				}
				
				multi_edge_node_ids.push_back(node->get_gid());
				print_incoming_edges(incoming_edges);

				int index = select_base_on_cached_node(node);
				if (index != -1 && index < incoming_edges.size()) {
					//continue_backward_slice
					stopping_node = nullptr;
					LOG_S(INFO) << "Cache hit on the node" << std::endl;
					node = incoming_edges[index]->from;
					deadline = incoming_edges[index]->e_from->get_abstime();
					continue;
				}
				stopping_node = node;
				return node->get_gid();
			}
		}
    }
    return 0; //nothing to search anymore
}


Edge *GraphSearcher::refine_edge(Node *to)
{
	auto edges = get_incomming_edges(to);
	Edge *wakeup = nullptr;

	for (auto edge : edges) {
		if (edge->e_from->get_event_type() == MR_EVENT) {
			wakeup = edge;
			break;
		}
	}
	if (wakeup == nullptr)
		return nullptr;

	auto strong_edges = get_strong_edges(to);
	for (auto edge : strong_edges) {
		if (edge->from->get_gid() == wakeup->from->get_gid())
			return wakeup;
	}
	return nullptr;
}

Edge *GraphSearcher::heuristic_edge(Node *to, int n)
{
	if (path.size() == 0)
		return nullptr;

	std::map<Edge *, int> statistics;
	std::map<tid_t, Edge *> tid_map;
	std::map<pid_t, Edge *> pid_map;
	for (auto e : get_incomming_edges(to))
		statistics[e] = 0;
	
	auto rit = path.rbegin();
	for (int i = 0; i < n && rit != path.rend(); i++, rit++) {
		Node *cur_node = *rit;
		auto edges = cur_node->get_out_edges();
		for (auto element: edges) {
			Edge *e = element.second;
			tid_t tid = e->to->get_tid();
			if (tid_map.find(tid) != tid_map.end())
				statistics[tid_map[tid]]++;
			pid_t pid = e->to->get_pid();
			if (pid_map.find(pid) != pid_map.end())
				statistics[pid_map[pid]]++;
		}
	}
	
	Edge *ret = nullptr;
	int max = 0;
	for (auto element: statistics) {
		if (element.second > max) {
			max = element.second;
			ret = element.first;
			LOG_S(INFO) << "Edge from 0x" << std::hex <<  element.first->from->get_gid()\
				<< " wins with score " << std::dec << max << std::endl;
		}
		if (max > 0 && element.second == max) {
			LOG_S(INFO) << "Edge from 0x" << std::hex <<  element.first->from->get_gid()\
				<< " has same score " << std::dec << max << std::endl;
		}
	}

	if (max == 0)
		return nullptr;

	return ret;
}

void GraphSearcher::check_and_remove_from_weak_edge_node_ids(uint64_t gid)
{
    std::vector<uint64_t>::reverse_iterator rit;
    rit = std::find_if(weak_edge_node_ids.rbegin(), weak_edge_node_ids.rend(),
			 [gid](uint64_t &i){return i == gid;});
    if (rit != weak_edge_node_ids.rend())
        weak_edge_node_ids.erase(next(rit).base());
}

void GraphSearcher::check_and_remove_from_multi_edge_node_ids(uint64_t gid)
{
    std::vector<uint64_t>::reverse_iterator rit;
    rit = std::find_if(multi_edge_node_ids.rbegin(), multi_edge_node_ids.rend(), [gid](uint64_t &i) {return i == gid;});
    if (rit != multi_edge_node_ids.rend())
        multi_edge_node_ids.erase(next(rit).base());
}

uint64_t GraphSearcher::check_converge(uint64_t gid_1, uint64_t gid_2, int thresh_hold)
{
	if (gid_1 == gid_2)
		return gid_1;
	Node *node_1 = id_to_node(gid_1);
	Node *node_2 = id_to_node(gid_2);
	if (!node_1 || !node_2)
		return 0;
	std::cout << "Begin check converge of 0x" << std::hex << gid_1 << " and " << std::hex << gid_2 << std::endl;
	return check_converge(node_1, node_2, 2 * thresh_hold);
}

uint64_t GraphSearcher::check_converge(Node *node_1, Node *node_2, int thresh_hold)
{
	if (node_1 == node_2)
		return node_1->get_gid();
	if (thresh_hold == 0)
		return 0;

	std::vector<Edge *> edges_1 = get_incomming_edges(node_1);
    std::map<uint64_t, EventBase *> predecessors_1 = collapse_prevs(edges_1);

	std::vector<Edge *> edges_2 = get_incomming_edges(node_2);
    std::map<uint64_t, EventBase *> predecessors_2 = collapse_prevs(edges_2);

	if (collapse_prevs(edges_1).size() == 0 && collapse_prevs(edges_2).size() == 0)
		return 0;

	//for (auto edge_1 : edges_1) {
		//Node *from_1 = edge_1->from;
	for (auto element: predecessors_1) {
		Node *from_1 = id_to_node(element.first);
		assert(from_1);
		if (from_1 == node_1)
			continue;
		uint64_t ret = check_converge(from_1, node_2, thresh_hold - 1);
		if (ret > 0)
			return ret;
	}

	//for (auto edge_2 : edges_2) {
		//Node *from_2 = edge_2->from;
	for (auto element: predecessors_2) {
		Node *from_2 = id_to_node(element.first);
		if (from_2 == node_2)
			continue;
		uint64_t ret = check_converge(node_1, from_2, thresh_hold - 1);
		if (ret > 0)
			return ret;
	}

	return 0;
}

bool GraphSearcher::clear_path_from_node(uint64_t gid)
{
    Node *node = id_to_node(gid);
    if (node == nullptr)
        return false;    
    std::vector<Node *>::iterator pos, it;
    pos = std::find_if(path.begin(), path.end(), [node](Node *&cur) {return cur == node;});
    if (pos == path.end())
        return false;
    while ((it = pos) != path.end()) {
        check_and_remove_from_weak_edge_node_ids((*it)->get_gid());
        check_and_remove_from_multi_edge_node_ids((*it)->get_gid());
        it++;
    }
    path.erase(pos, path.end());
    return true;
}

void GraphSearcher::clear_path()
{
	path.clear();
	weak_edge_node_ids.clear();
	multi_edge_node_ids.clear();
}

uint64_t GraphSearcher::continue_backward_slice(uint64_t gid, int event_index)
{
    Node *node = id_to_node(gid);
    if (node == nullptr)
        return 0;

    double deadline = -1.0;
	EventBase *event = node->get_group()->event_at(event_index);
	if (event != nullptr)
		deadline = event->get_abstime();
	
	if (stopping_node != nullptr) {
		auto incoming_edges = get_incomming_edges(stopping_node);
		int index = 0;
		for (auto edge : incoming_edges) {
			if (edge->from == node) {
				cached_selection[stopping_node] = index;
				std::cout << "store selection on 0x"\
					<< std::hex << stopping_node->get_gid()\
					<< " in the cache." << std::endl;
				break;
			}
			index++;
		}
		stopping_node = nullptr;
	}

    return path_slice_from_node(node, deadline);
}

uint64_t GraphSearcher::init_diagnose()
{
	if (get_anomaly_node() == nullptr) {
        std::cout << "No spinning node found" << std::endl;
        return 0;
    }
	assert(anomaly_node != nullptr);

    std::vector<Node *> similar_nodes;
    Node *baseline_node = nullptr;

    path.clear();
    weak_edge_node_ids.clear();
    multi_edge_node_ids.clear();
    similar_nodes.clear();
    similar_nodes = search_baseline_nodes(anomaly_node);

    if (similar_nodes.size()  > 0) {
		std::cout << "Baseline nodes: " << std::endl;
		print_nodes(similar_nodes);
        baseline_node = similar_nodes.back();
    }

    if (baseline_node) {
		return search_from_baseline(baseline_node->get_gid());
    } else {
        std::cerr << "Path slice from spinning node 0x" << std::hex << anomaly_node->get_gid() << std::endl;
        return path_slice_from_node(anomaly_node);
    }
}

uint64_t GraphSearcher::search_from_baseline(uint64_t gid)
{
		Node *wakeup_node = graph->id_to_node(gid + 1);
		if (wakeup_node == nullptr) {
			std::cout << "Wakeup node does not exist" << std::endl;
			return 0;
		}
		assert(wakeup_node->index_to_event(0)->get_event_type() == FAKED_WOKEN_EVENT);
        std::cout << "Path slice for normal node 0x" << std::hex << gid\
			 << " from 0x" << std::hex << wakeup_node->get_gid() << std::endl;
		return path_slice_from_node(wakeup_node);
}

int GraphSearcher::path_comparison()
{
    if (path.size() == 0 || spinning_type(get_anomaly_node()) != SPINNING_WAIT)
        return -1;

	std::map<tid_t, Node *> extract_path;

	extract_path.clear();
	suspicious_nodes.clear();

	for (auto node : path) {
		if (extract_path.find(node->get_tid()) != extract_path.end())
			continue;
		extract_path[node->get_tid()] = node;
	}

	std::cout << "Examine paths with " << std::dec << extract_path.size() << " threads." <<  std::endl;

    Node *culprit = nullptr, *cur_normal_node = nullptr;
	for (auto element: extract_path) {
		cur_normal_node = element.second;
		double anomaly_timestamp = anomaly_node->get_begin_time();
        std::vector<Node *> buggy_nodes
			= graph->nodes_between(cur_normal_node, anomaly_timestamp);
        std::cout << "Check Node 0x" << std::hex << cur_normal_node->get_gid()\
            << std::dec << " with buggy node candidates size = "\
            << buggy_nodes.size() << std::endl;

        if (buggy_nodes.size() == 0)
        /*check next thread*/
            continue;
        //check nodes
        std::vector<Node *>::reverse_iterator rit;
        int count = 32;
        for (rit = buggy_nodes.rbegin(); rit != buggy_nodes.rend() && count > 0; rit++) {
            count--;
            if ((*rit)->wait_over() || (*rit)->execute_over()) {
                culprit = *rit;
                suspicious_nodes.push_back(culprit);
				std::cout << "Push culprit " << std::hex << culprit->get_gid() << " base on "\
						 << std::hex <<  cur_normal_node->get_gid() << std::endl;
                break;
            }
        }
		/*
        if (culprit != nullptr)
            break;
        check next thread*/
    }
	
	if (suspicious_nodes.size() > 0) {
		for (auto node : suspicious_nodes)
        	std::cout << "Get culprit node 0x" << std::hex << node->get_gid() << std::endl;
        /* recursively find the culprit of culprit to root cause
         * or user interaction to recurse the process
		 */
        //return init_diagnose();
        return 0;
	} else {
		std::cout << "No culprit node found" << std::endl;
	}

    return -2;
}

void GraphSearcher::js_paths(std::vector<Node *>& paths, Node *node, std::string file_path)
{
    std::ofstream outfile(file_path);
    outfile << "var main_lane = \"" << node->get_group()->get_first_event()->get_procname() << "_"\
            << std::hex << node->get_group()->get_first_event()->get_tid() << "\";" << std::endl;
    JSGraph jsgraph(graph);
    jsgraph.js_cluster(paths, outfile);
    outfile.close();
}

void GraphSearcher::show_path()
{
    std::stringstream sstream;
    sstream << std::hex << anomaly_node->get_gid();

    std::string file_path = "path_from_" + sstream.str();
    js_paths(path, anomaly_node, file_path);

    LOG_S(INFO) << "Path to 0x" << std::hex << anomaly_node->get_gid() << std::endl;
	for (auto node : path)
        LOG_S(INFO) << "\t0x" << std::hex << node->get_gid() << "\t" << node->get_procname() << std::endl;
    LOG_S(INFO) << std::endl;
    
    if (weak_edge_node_ids.size() != 0) {
        LOG_S(INFO) << "Weak Edges in Path :" << std::endl;
        LOG_S(INFO) << std::hex << std::setw(20);
		for (auto id : weak_edge_node_ids)
			LOG_S(INFO) << std::hex << id << "\t";
        LOG_S(INFO) << std::endl;
    }

    if (multi_edge_node_ids.size() != 0) {
        LOG_S(INFO) << "Multi Edges in Path :" << std::endl;
        LOG_S(INFO) << std::hex << std::setw(20);
		for (auto id: multi_edge_node_ids)
			LOG_S(INFO) << std::hex << id << "\t";
        LOG_S(INFO) << std::endl;
    }
}

void GraphSearcher::get_culprits()
{
	if (suspicious_nodes.size() > 0)
		for (auto node : suspicious_nodes)
        	std::cout << "Get culprit node 0x"\
				 << std::hex << node->get_gid() << std::endl;
	else
		std::cout << "No suspicious node found" << std::endl;
} 

void GraphSearcher::tfl_pair(Node *from_node, Node *to_node, std::ofstream &out)
{
	if (from_node == nullptr || to_node == nullptr) {
		std::cout << "Invalid input nodes" << std::endl;
		return;
	}
	Group *from = from_node->get_group();
	Group *to = to_node->get_group();	
	for (auto event : from->get_container()) {
		event->EventBase::tfl_event(out);
		out << std::endl;
	}

	out << "-" << std::endl;
	for (auto event : to->get_container()) {
		event->EventBase::tfl_event(out);
		out << std::endl;
	}
	out << "=" << std::endl;
}

void GraphSearcher::record_positive(uint64_t from, uint64_t to)
{
    std::stringstream sstream;
    sstream << std::hex << anomaly_node->get_gid();

    std::string file_path = "output/sel_path_from_" + sstream.str() + ".positive";
	std::ofstream pos_out(file_path, std::ios::app);
	tfl_pair(graph->id_to_node(from), graph->id_to_node(to), pos_out);
	pos_out.close();
}

void GraphSearcher::record_na(uint64_t from, uint64_t to)
{
    std::stringstream sstream;
    sstream << std::hex << anomaly_node->get_gid();

    std::string file_path = "output/sel_path_from_" + sstream.str() + ".na";
	std::ofstream na_out(file_path, std::ios::app);
	tfl_pair(graph->id_to_node(from), graph->id_to_node(to), na_out);
	na_out.close();
}

void GraphSearcher::record_negtive(uint64_t from, uint64_t to)
{
    std::stringstream sstream;
    sstream << std::hex << anomaly_node->get_gid();

    std::string file_path = "output/sel_path_from_" + sstream.str() + ".negtive";
	std::ofstream neg_out(file_path, std::ios::app);
	tfl_pair(graph->id_to_node(from), graph->id_to_node(to), neg_out);
	neg_out.close();
}

void GraphSearcher::tfl_selection(uint64_t from, uint64_t to, int type)
{
	switch (type) {
		case 1: record_positive(from, to);
				break;
		case 0: record_na(from, to);
				break;
		case -1: record_negtive(from, to);
				break;
		default:
			std::cout << "Invalid input type" << std::endl;
	}
}

void GraphSearcher::record_selection()
{
    std::stringstream sstream;
    sstream << std::hex << anomaly_node->get_gid();

    std::string file_path = "output/sel_path_from_" + sstream.str() + ".positive";
	std::ofstream pos_out(file_path, std::ios::app);
	pos_out << "=" << std::endl;
	pos_out.close();

    file_path = "output/sel_path_from_" + sstream.str() + ".negtive";
	std::ofstream neg_out(file_path, std::ios::app);
	neg_out << "=" << std::endl;
	neg_out.close();

    file_path = "output/sel_path_from_" + sstream.str() + ".na";
	std::ofstream na_out(file_path, std::ios::app);
	na_out << "=" << std::endl;
	na_out.close();

	for (auto to: multi_edge_node_ids) {
		Node *to_node = graph->id_to_node(to);
		assert(to_node != nullptr); 

		auto it = find(path.begin(), path.end(), to_node);
		assert(it != path.end());
		it++;
		if (it == path.end())
			continue;
		Node *from_node = *(it);

		auto incoming_edges = get_incomming_edges(to_node);

		std::map<uint64_t, bool> visited_map;
		visited_map.clear();
		uint64_t from = 0;

		for (auto edge : incoming_edges) {
			from = edge->from->get_gid();
			assert(to  == edge->to->get_gid());
			if (edge->from == from_node ) {
				tfl_selection(from, to, 1);
				visited_map[from] = true;
			} else {
				if (visited_map.find(from) != visited_map.end())
					continue;
				tfl_selection(from, to, 0);
			}
		}

		for (auto it = visited_map.begin(); it != visited_map.end(); it++) {
			auto j = it;
			for (j++; j != visited_map.end(); j++)
				tfl_selection(it->first, j->first, 0);
		}
	}
}

void GraphSearcher::show_similar_nodes(uint64_t gid)
{
	Node *basenode = id_to_node(gid);
	if (basenode == nullptr)
		return;
	std::vector<Node *> ret = get_prev_similar_nodes(basenode);
	int i = 0;
	LOG_S(INFO) << "Similar nodes to " << std::hex << gid << std::endl;
	for (auto node : ret)
		LOG_S(INFO) << "[" << i++ << "]\t" << std::hex << node->get_gid() << std::endl;
	ret = get_post_similar_nodes(basenode, 0.0);
	for (auto node : ret)
		LOG_S(INFO) << "[" << i++ << "]\t" << std::hex << node->get_gid() << std::endl;
}
