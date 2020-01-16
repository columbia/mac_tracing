#include "search_graph.hpp"
#include "js_graph.hpp"


GraphSearcher::GraphSearcher(Graph *_graph, std::map<EventType::event_type_t, bool> event_for_compare)
{
    if (event_for_compare.size() == 0) {
        key_events = load_default_key_events();
    } else {
        key_events = event_for_compare;
    }
    graph = _graph;
    tid_normnodes_map.clear();
    similar_nodes.clear();
    path.clear();
    weak_edge_node_ids.clear();
    multi_edge_node_ids.clear();
    buggy_node = nullptr;
}

GraphSearcher::GraphSearcher(Graph *_graph)
{
    graph = _graph;
    key_events = load_default_key_events();
    tid_normnodes_map.clear();
    similar_nodes.clear();
    path.clear();
    weak_edge_node_ids.clear();
    multi_edge_node_ids.clear();
    buggy_node = nullptr;
    buggy_node = nullptr;
}

GraphSearcher::~GraphSearcher()
{
    //clear all normnodes
    std::map<tid_t, std::vector<NormNode *> >::iterator it;
    for (it = tid_normnodes_map.begin(); it != tid_normnodes_map.end(); it++) {
        std::vector<NormNode *> &norm_group_vector = it->second;
        std::vector<NormNode *>::iterator normnode_it;
        for (normnode_it = norm_group_vector.begin(); normnode_it != norm_group_vector.end(); normnode_it++) {
            delete *normnode_it;
        }
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

std::vector<Node *> GraphSearcher::get_similar_nodes_in_thread_before(Node *node, int number)
{
    if (number == 0)
        return get_similar_nodes_in_thread_before(node);

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

std::vector<Node *> GraphSearcher::get_similar_nodes_in_thread_before(Node *node)
{
    std::vector<NormNode *> normnodes_for_thread = update_normnodes_for_thread(node->get_tid());
    std::vector<NormNode *>::iterator it, end;

    std::vector<Node *> ret;
    NormNode *normnode = norm_node_fast(node);
    assert(normnode != nullptr);

    if(normnode->is_empty()) {
        std::cerr << "Search empty normed node for the given key events" << std::endl;
        std::cerr << "normnode id " << std::hex << node->get_group()->get_group_id() << std::endl;
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
        if (*cur_node == *normnode)
            ret.push_back(cur_node->get_node());
    }
    similar_nodes = ret;
    return ret;
}

std::vector<Node *> GraphSearcher::get_similar_nodes_in_thread_after(Node *node, double deadline)
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
        if (cur_node->get_node()->get_begin_time() > deadline)
            break; // only get similar group before current group
        if (cur_node->is_empty())
            continue;
        if (*cur_node == *normnode)
            ret.push_back(cur_node->get_node());
    }
    similar_nodes = ret;
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

std::vector<Node *> GraphSearcher::search_baseline_nodes(Node *spinning_node)
{
    std::vector<Node *> similar_nodes;
    std::vector<Node *>::iterator it;

    similar_nodes.clear();
    assert(spinning_node != nullptr);
    std::cout << "Spinning data " << spinning_node->get_group()->get_group_id() << std::endl;
    int type = spinning_type(spinning_node);
    if (type == GraphSearcher::SPINNING_WAIT) {
        similar_nodes = get_similar_nodes_in_thread_before(spinning_node);   
        if (similar_nodes.size() > 0) {
            return get_baseline_on_waitcost(spinning_node);
        }
    }

    if (type == GraphSearcher::SPINNING_YIELD) {
        similar_nodes = get_similar_nodes_in_thread_before(spinning_node, 1);
        if (similar_nodes.size() > 0) {
            return get_baseline_on_timespan(spinning_node);
        }
    }
    return similar_nodes;
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

Node *GraphSearcher::spinning_node()
{
    if (buggy_node == nullptr)
        buggy_node = graph->get_spinning_node();
    return buggy_node;
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
    buggy_node = spinning_node();
    if (buggy_node == nullptr)
        return false;
    return spinning_type(buggy_node) == SPINNING_WAIT;
}

std::string GraphSearcher::decode_spinning_type()
{
    return decode_spinning_type(spinning_type(spinning_node()));
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

//except for the blocing wait, other cases do path slicing from buggy node
//Node *GraphSearcher::get_begin_node_for_slicing(Node *node)
Node *GraphSearcher::get_begin_node_for_causal_path_slicing(Node *node)
{
    Node *buggy_node = spinning_node();
    if (buggy_node == nullptr)
        return node;

    if (spinning_type(buggy_node) == SPINNING_WAIT) {
        std::map<EventBase *, Edge *> out_edges = node->get_out_edges();
        std::map<EventBase *, Edge *>::iterator it;
        for (it = out_edges.begin(); it != out_edges.end(); it++) {
            WaitEvent *wait = dynamic_cast<WaitEvent *>(it->first);
            if (wait != nullptr)
                return it->second->to;
        }
    }
    return buggy_node;
}

bool GraphSearcher::cmp_edge_by_intime(Edge *e1, Edge *e2)
{
    return e1->e_to->get_abstime() > e2->e_to->get_abstime();
}

std::list<Edge *> GraphSearcher::get_incomming_edges_before(std::map<EventBase *, Edge*> &in_edges, double deadline)
{
    std::list<Edge *> ret;
    std::map<EventBase *, Edge *>::iterator it;

    ret.clear();
    for (it = in_edges.begin(); it != in_edges.end(); it++) {
        if (it->second->from != it->second->to && it->first->get_abstime() < deadline)
            ret.push_back(it->second);
    }
    ret.sort(GraphSearcher::cmp_edge_by_intime);
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
        if (*vent_node_ptr == nullptr && nodes_in_thread[i]->get_in_edges().size() > 0)
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
    std::vector<Node *>::iterator it;
    int i = 0;
    for (it = t_nodes.begin(); it != t_nodes.end(); it++)
        std::cout << "\tNode [" << i << "] "<< (*it)->get_group()->get_group_id() << std::endl;
    std::cout << std::endl;
}

void GraphSearcher::print_incoming_edges(std::list<Edge *> incoming_edges)
{
    std::list<Edge *>::iterator it;
    Group *peer_group; 
    EventBase *peer_event;
    std::list<EventBase *> lists;
    int pos;

    for (it = incoming_edges.begin(); it != incoming_edges.end(); it++) {
        peer_group =  (*it)->from->get_group();
        peer_event = (*it)->e_from;
        lists = peer_group->get_container();
        pos = distance(lists.begin(), std::find(lists.begin(), lists.end(), peer_event));

        std::cout << "\tNode 0x" << std::hex << peer_group->get_group_id() << " " << peer_event->get_procname() << std::endl;
        std::cout << "\tEdge from event [" << std::hex << pos << "]"   \
            <<" at " << std::fixed << std::setprecision(1) << (*it)->e_from->get_abstime() << std::endl;
        std::cout << "\t";
        peer_event->streamout_event(std::cout);
        std::cout << std::endl;
    }
}

std::map<uint64_t, EventBase *> GraphSearcher::collapse_prevs(std::list<Edge *> incoming_edges)
{
    std::map<uint64_t, EventBase *> ret;
    ret.clear();
    if (incoming_edges.size() == 0)
        return ret;
    
    std::list<Edge *>::iterator it;
    for (it = incoming_edges.begin(); it != incoming_edges.end(); it++) {
        if (ret.find((*it)->from->get_gid()) == ret.end())
            ret[(*it)->from->get_gid()] = (*it)->e_from;
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
            std::cout << "Contains UI event in Node 0x" << std::hex << node->get_gid() << std::endl;
            return 0;
        }

        std::list<Edge *> incoming_edges = get_incomming_edges_before(node->get_in_edges(), deadline);
        std::map<uint64_t, EventBase *> predecessors = collapse_prevs(incoming_edges);

        if (incoming_edges.size() == 0) {
            uint64_t stopped_gid = node->get_gid();
            std::cout << "Weak Edge to Node 0x" << std::hex << stopped_gid << std::endl;
            weak_edge_node_ids.push_back(stopped_gid);
            node = prev_node_from_weak_edge(node);
            if (node != nullptr)
                std::cout << "\tSuggest choose along weak edge Node 0x" << std::hex << node->get_gid() << std::endl; 
            else
                std::cout << "\tNo more node can be searched from weak edge 0x" << std::hex << stopped_gid << std::endl;
            return stopped_gid;
        }

        if (predecessors.size() == 1) { 
            std::cout << "Node 0x" << std::hex <<  " has incoming edges from same node :" << std::endl;
            print_incoming_edges(incoming_edges);
            node = incoming_edges.front()->from;
            deadline = incoming_edges.front()->e_from->get_abstime();
        } else {
            assert(predecessors.size() > 1);
            std::cout << "Multiple Incoming Edges to 0x" << std::hex << node->get_gid() << std::endl;
            multi_edge_node_ids.push_back(node->get_gid());
            print_incoming_edges(incoming_edges);
            return node->get_gid();
        }
    }
    return 0; //nothing to search anymore
}


void GraphSearcher::check_and_remove_from_weak_edge_node_ids(uint64_t gid)
{
    std::vector<uint64_t>::reverse_iterator rit;
    rit = std::find_if(weak_edge_node_ids.rbegin(), weak_edge_node_ids.rend(), [gid](uint64_t &i){return i == gid;});
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


uint64_t GraphSearcher::continue_backward_slice(uint64_t gid, int event_index)
{
    Node *node = id_to_node(gid);
    double deadline = -1.0;
    if (node == nullptr)
        return 0;

	/*
    std::list<EventBase *> lists = node->get_group()->get_container();
    if (lists.size() > event_index) {
        std::list<EventBase *>::iterator it = lists.begin();
        std::advance(it, event_index);
        deadline = (*it)->get_abstime();
    }
	*/
	EventBase *event = node->get_group()->event_at(event_index);
	if (event != nullptr)
		deadline = event->get_abstime();

    return path_slice_from_node(node, deadline);
}

uint64_t GraphSearcher::init_diagnose()
{
    Node *buggy_node = spinning_node();
    if (buggy_node == nullptr) {
        std::cout << "No spinning node found" << std::endl;
        return 0;
    }

    std::vector<Node *> similar_nodes;
    std::vector<Node *>::iterator it;
    Node *baseline_node = nullptr;
    uint64_t ret = 0;

    path.clear();
    weak_edge_node_ids.clear();
    multi_edge_node_ids.clear();
    similar_nodes.clear();
    similar_nodes = search_baseline_nodes(buggy_node);
    std::cout << "Baseline nodes: " << std::endl;
    print_nodes(similar_nodes);

    if (similar_nodes.size()  > 0) {
        baseline_node = similar_nodes.back();
    }

    if (baseline_node) {
        std::cout << "Path slice from normal node 0x" << std::hex << baseline_node->get_gid() << std::endl;
        ret = path_slice_from_node(get_begin_node_for_causal_path_slicing(baseline_node));
    } else {
        std::cerr << "Path slice from spinning node 0x" << std::hex << buggy_node->get_gid() << std::endl;
        ret = path_slice_from_node(buggy_node);
    }
    return ret;
}


int GraphSearcher::path_comparison()
{
    if (path.size() == 0 || spinning_type(spinning_node()) != SPINNING_WAIT)
        return -1;

    std::vector<Node *>::iterator it = path.begin();
    Node *culprit = nullptr;

    suspicious_nodes.clear();
    for (it++; it != path.end(); it++) {
        Node *cur_normal_node = *it;
        std::vector<Node *> buggy_nodes = graph->get_buggy_node_candidates(cur_normal_node);
        
        std::cout << "Check Node 0x" << std::hex << cur_normal_node->get_gid()\
            << std::dec << " with buggy node candidates size = "\
            << buggy_nodes.size() << std::endl;

        if (buggy_nodes.size() == 0)
        /*check next thread*/
            continue;

        //check nodes
        std::vector<Node *>::reverse_iterator rit;
        int count = 5;
        for (rit = buggy_nodes.rbegin(); rit != buggy_nodes.rend() && count > 0; rit++) {
            count--;
            if ((*rit)->wait_over() == true) {
                culprit = *rit;
                suspicious_nodes.push_back(culprit);
                break;
            }
        }
        
        if (culprit != nullptr)
            break;
        /*else check next thread*/
    }
    
    if (culprit) {
        buggy_node = culprit;
        std::cout << "Get culprit node 0x" << buggy_node->get_gid() << std::endl;
        /* recursively find the culprit of culprit to root cause
         * or user interaction to recurse the process*/
        return 0;
        //return init_diagnose();
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
    sstream << std::hex << buggy_node->get_gid();
    std::string file_path = "path_from_" + sstream.str();
    js_paths(path, buggy_node, file_path);

    std::vector<Node *>::iterator it;
    std::cout << "Path to 0x" << std::hex << buggy_node->get_gid() << std::endl;
    for (it = path.begin(); it != path.end(); it++)
        std::cout << "\t0x" << std::hex << (*it)->get_gid();
    std::cout << std::endl;
    
    if (weak_edge_node_ids.size() != 0) {
        std::cout << "Weak Edges in Path :" << std::endl;
        std::cout << std::hex << std::setw(20);
        std::copy(weak_edge_node_ids.begin(),
                weak_edge_node_ids.end(),
                std::ostream_iterator<uint64_t> (std::cout, "\t"));
        std::cout << std::endl;
    }

    if (multi_edge_node_ids.size() != 0) {
        std::cout << "Multi Edges in Path :" << std::endl;
        std::cout << std::hex << std::setw(20);
        std::copy(multi_edge_node_ids.begin(),
                multi_edge_node_ids.end(),
                std::ostream_iterator<uint64_t> (std::cout, "\t"));
        std::cout << std::endl;
    }
}

void GraphSearcher::get_culprits()
{
} 
