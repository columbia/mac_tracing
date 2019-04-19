#include "graph.hpp"

static time_t time_begin, time_check, time_end;
Edge::Edge(event_t* e1, event_t*e2, uint32_t rel)
{
	e_from = e1;
	e_to = e2;
	rel_type = rel;
	from = to = NULL;
}

Edge::~Edge()
{
	from = to = NULL;
	e_from = e_to = NULL;
}
//////////////////////////////////
Node::Node(Graph *_p, group_t *_g)
:parent(_p), group(_g)
{
	out_edges.clear();
	in_edges.clear();
	//add_edges();
}

void Node::add_edges()
{
	list<event_t *> &lists = group->get_container();
	list<event_t *>::iterator it;

	for (it = lists.begin(); it != lists.end(); it++) {
		event_t *event = *it;
		switch (event->get_event_id()) {
			case MSG_EVENT:
				add_edge_for_msg(dynamic_cast<msg_ev_t *>(event));
				break;
			case DISP_ENQ_EVENT:
				add_edge_for_disp_enq(dynamic_cast<enqueue_ev_t *>(event));
				break;
			case DISP_DEQ_EVENT:
				add_edge_for_disp_deq(dynamic_cast<dequeue_ev_t *>(event));
				break;
			case TMCALL_CREATE_EVENT:
				add_edge_for_callcreate(dynamic_cast<timercreate_ev_t *>(event));
				break;
			case TMCALL_CALLOUT_EVENT:
				add_edge_for_callout(dynamic_cast<timercallout_ev_t *>(event));
				break;
			case TMCALL_CANCEL_EVENT:
				add_edge_for_callcancel(dynamic_cast<timercancel_ev_t *>(event));
				break;
			case CA_SET_EVENT:
				add_edge_for_caset(dynamic_cast<ca_set_ev_t *>(event));
				break;
			case CA_DISPLAY_EVENT:
				add_edge_for_cadisplay(dynamic_cast<ca_disp_ev_t *>(event));
				break;
			case RL_BOUNDARY_EVENT: {
				rl_boundary_ev_t *rlboundary_event = dynamic_cast<rl_boundary_ev_t *>(event);
				if (rlboundary_event->get_owner() != NULL)
					add_edge_for_rlitem(rlboundary_event);
				break;
			}
			case BREAKPOINT_TRAP_EVENT:
				add_edge_for_hwbr(dynamic_cast<breakpoint_trap_ev_t *>(event));
				break;
			case MR_EVENT:
				add_edge_for_mkrun(dynamic_cast<mkrun_ev_t *>(event));
				break;
			case WAIT_EVENT:
				add_edge_for_wait(dynamic_cast<wait_ev_t *>(event));
				break;
			case FAKED_WOKEN_EVENT:
				add_edge_for_fakedwoken(dynamic_cast<fakedwoken_ev_t*>(event));
				break;
			default:
				break;
		}
	}
}

Node::~Node()
{
	//remove_edges();
	parent = NULL;
	group  = NULL;
}


bool Node::add_in_edge(Edge *e)
{
	Edge *exist_edge = parent->check_and_add_edge(e);
	bool ret = exist_edge == NULL ? true : false;
	if (exist_edge == NULL) {
		in_edges[e->e_to] = e;
	} else {
		in_edges[e->e_to] = exist_edge;
	}
	return ret;
}

bool Node::add_out_edge(Edge *e)
{
	Edge *exist_edge = parent->check_and_add_edge(e);
	bool ret = exist_edge == NULL ? true : false;
	if (exist_edge == NULL) {
		out_edges[e->e_from] = e;
	} else {
		out_edges[e->e_from] = exist_edge;
	}
	return ret;
}

void Node::add_edge_for_msg(msg_ev_t *msg_event)
{

	msg_ev_t *next_msg = msg_event->get_next();
	msg_ev_t *prev_msg = msg_event->get_prev();
	msg_ev_t *peer_msg = msg_event->get_peer();

	if (next_msg != NULL) {
		Edge *e = new Edge(msg_event, next_msg, MSGP_REL);
		if (!add_out_edge(e))
			delete e;
	} else if (prev_msg != NULL) {
		Edge *e = new Edge(prev_msg, msg_event, MSGP_REL);
		if (!add_in_edge(e))
			delete e;
	}

	if (peer_msg == NULL)
		return;
	if (msg_event->get_header()->check_recv() == true) {
		Edge *e = new Edge(peer_msg, msg_event, MSGP_REL);
		if (!add_in_edge(e))
			delete e;
			
	} else {
		Edge *e = new Edge(msg_event, peer_msg, MSGP_REL);
		if (!add_out_edge(e))
			delete e;
	}
}

void Node::add_edge_for_disp_enq(enqueue_ev_t *enq_event)
{
	dequeue_ev_t *deq_event = enq_event->get_consumer();
	if (!deq_event)
		return;
	Edge *e = new Edge(enq_event, deq_event, DISP_EXE_REL);
	if (!add_out_edge(e))
		delete e;
}

void Node::add_edge_for_disp_deq(dequeue_ev_t *deq_event)
{
	enqueue_ev_t *enq_event = deq_event->get_root();
	if (!enq_event)
		return;
	Edge *e = new Edge(enq_event, deq_event, DISP_EXE_REL);
	if (!add_in_edge(e))
		delete e;
}

void Node::add_edge_for_callcreate(timercreate_ev_t *timer_create)
{
	timercallout_ev_t *timercallout_event = timer_create->get_called_peer();
	if (timercallout_event) {
		Edge *e = new Edge(timer_create, timercallout_event, TIMERCALLOUT_REL);
		if (!add_out_edge(e))
			delete e;
	}
	timercancel_ev_t *timercancel_event = timer_create->get_cancel_peer();
	if (timercancel_event) {
		Edge *e = new Edge(timer_create, timercancel_event, TIMERCANCEL_REL);
		if (!add_out_edge(e))
			delete e;
	}
}

void Node::add_edge_for_callout(timercallout_ev_t *timer_callout)
{
	timercreate_ev_t *timer_create = timer_callout->get_timercreate();
	if (timer_create) {
		Edge *e = new Edge(timer_create, timer_callout, TIMERCALLOUT_REL);
		if (!add_in_edge(e))
			delete e;
	}
}

void Node::add_edge_for_callcancel(timercancel_ev_t *timer_cancel)
{
	timercreate_ev_t *timer_create = timer_cancel->get_timercreate();
	if (timer_create) {
		Edge *e = new Edge(timer_create, timer_cancel, TIMERCANCEL_REL);
		if (!add_in_edge(e))
			delete e;
	}
}

void Node::add_edge_for_caset(ca_set_ev_t *ca_set_event)
{
	ca_disp_ev_t *ca_display_event = ca_set_event->get_display_event();
	if (ca_display_event) {
		Edge *e = new Edge(ca_set_event, ca_display_event, CA_REL);
		if (!add_out_edge(e))
			delete e;
	}
}

void Node::add_edge_for_cadisplay(ca_disp_ev_t *ca_display_event)
{
	vector<ca_set_ev_t*> ca_set_events = ca_display_event->get_ca_set_events();
	vector<ca_set_ev_t*>::iterator it;
	for (it = ca_set_events.begin(); it != ca_set_events.end(); it++) {
		ca_set_ev_t *ca_set_event = *it;
		Edge *e = new Edge(ca_set_event, ca_display_event, CA_REL);
		if (!add_in_edge(e))
			delete e;
	}
}

void Node::add_edge_for_rlitem(rl_boundary_ev_t *rl_event)
{
	event_t* owner = rl_event->get_owner();
	if (owner != NULL) {
		Edge *e = new Edge(owner, rl_event, RLITEM_REL);
		if (!add_in_edge(e))
			delete e;

	}
	event_t *consumer = rl_event->get_consumer();
	if (consumer != NULL) {
		Edge *e = new Edge(rl_event, consumer, RLITEM_REL);
		if (!add_out_edge(e))
			delete e;
	}
}

void Node::add_edge_for_hwbr(breakpoint_trap_ev_t *event)
{
//TODO: connectors for breakpoints thing
}

void Node::add_edge_for_mkrun(mkrun_ev_t *mkrun_event)
{
	event_t *peer_event = mkrun_event->get_peer_event();
	if (peer_event) {
		Edge *e = new Edge(mkrun_event, peer_event, MKRUN_REL);
		if (!add_out_edge(e))
			delete e;
	}
}

void Node::add_edge_for_fakedwoken(fakedwoken_ev_t *fakewoken)
{
	mkrun_ev_t *mkrun_event = fakewoken->get_peer();
	if (mkrun_event) {
		Edge *e = new Edge(mkrun_event, fakewoken, MKRUN_REL);
		if (!add_in_edge(e))
			delete e;
	}
}

void Node::add_edge_for_wait(wait_ev_t *wait_event)
{
	mkrun_ev_t *mkrun_event = wait_event->get_mkrun();
	if (mkrun_event) {
		Edge *e = new Edge(wait_event, mkrun_event, WAIT_REL);
		if (!add_out_edge(e))
			delete e;
	}
	
}

map<event_t *, Edge *> &Node::get_in_edges()
{
	return in_edges;
}

map<event_t *, Edge *> &Node::get_out_edges()
{
	return out_edges;
}

void Node::remove_edges()
{
	//cout << "Begin remove edges" << endl;
	while (in_edges.size() > 0) {
		assert(in_edges.begin() != in_edges.end());
		pair<event_t *, Edge*> edge = *(in_edges.begin());
		remove_edge(edge, false);
	}
	while (out_edges.size() > 0) {
		assert(out_edges.begin() != out_edges.end());
		pair<event_t *, Edge*> edge = *(out_edges.begin());
		remove_edge(edge, true);
	}
	assert(in_edges.size() == 0 && out_edges.size() == 0);
	//cout << "Finish remove edges" << endl;
}

bool Node::find_edge_in_map(pair<event_t *, Edge *> target, bool out_edge)
{
	map<event_t *, Edge *>& map = out_edge ? out_edges: in_edges;
	assert(map.size() == out_edges.size() || map.size() == in_edges.size());
	
	if (map.find(target.first) != map.end()) {
		return true;
	}
	return false;
}

void Node::remove_edge(pair<event_t *, Edge *> edge, bool out_edge)
{
	assert(edge.first != NULL && edge.second != NULL);
	assert(edge.second->e_from == edge.first || edge.second->e_to == edge.first);
	if (find_edge_in_map(edge, out_edge) == false) {
		//cout << "Edge has deleted" << endl;
		return;
	}
	//cout << "Begin remove edge" << endl;
	Edge *e = edge.second;
	
	if (e->from == e->to) {
		//circle edge
		in_edges.erase(e->e_to);
		out_edges.erase(e->e_from);
	}
	else if (out_edge) {
		Node *peer = e->to;
		if (peer) {
			out_edges.erase(e->e_from);
			peer->remove_edge(make_pair(e->e_to, e), false);
		}
	} else {
		Node *peer = e->from;
		if (peer) {
			in_edges.erase(e->e_to);
			peer->remove_edge(make_pair(e->e_from, e), true);
		}
	}
	//cout << "Remove edge from parent" << endl;
	parent->remove_edge(e);
	//cout << "Finish remove edge" << endl;
}

int Node::compare_syscall_ret(Node *peer)
{
	return get_group()->compare_syscall_ret(peer->get_group());
}

int Node::compare_timespan(Node *peer)
{
	return get_group()->compare_timespan(peer->get_group());
}

int Node::compare_wait_cost(Node *peer)
{
	return get_group()->compare_wait(peer->get_group());
}

double Node::get_begin_time()
{
	return group->get_first_event()->get_abstime();
}

double Node::get_end_time()
{
	return group->get_last_event()->get_abstime();
}

bool Node::contains_noncausual_mk_edge()
{
	return group->contains_noncausual_mk_edge();
}

bool Node::wait_over()
{
	return group->wait_over();
	
}

///////////////////////////////////
Graph::Graph(groups_t *_groups_ptr)
{
	groups_ptr = _groups_ptr;
	init_graph();
}

Graph::~Graph(void)
{
	//while(nodes.size() > 0) {
		//remove_node(*(nodes.begin()));
	//}
	nodes_map.clear();
	tid_glist_map.clear();
	edge_map.clear();
	while(nodes.size() > 0) {
		delete(*(nodes.begin()));
	}

	while(edges.size() > 0) {
		delete(*(edges.begin()));
	}
}

typedef std::unique_ptr<boost::asio::io_service::work> asio_worker;

void Graph::init_graph()
{
	time(&time_begin);
	cerr << "Begin generate graph..." << endl;

	boost::asio::io_service ioService;
	boost::thread_group threadpool;
	asio_worker work(new asio_worker::element_type(ioService));
	
	for (int i = 0 ; i < LoadData::meta_data.nthreads; i++)
		threadpool.create_thread(boost::bind(&boost::asio::io_service::run, &ioService));

	map<uint64_t, group_t *> groups = groups_ptr->get_groups();
	map<uint64_t, group_t *>::iterator it;
	for (it = groups.begin(); it != groups.end(); it++) {
		group_t* cur_group = it->second;
		Node *node = new Node(this, cur_group);
		nodes.push_back(node);
		nodes_map[cur_group] = node;
		//cout << " Generate node for Node " << hex << cur_group->get_group_id() << endl;
		ioService.post(boost::bind(&Node::add_edges, node));
	}

	work.reset();
	threadpool.join_all();

	time(&time_check);
	cerr << "graph generate cost "  << fixed << setprecision(1) << difftime(time_check, time_begin) << " seconds" << endl;

	//update the Node pointers in edges
	vector<Edge *>::iterator e_it;
	for (e_it = edges.begin(); e_it != edges.end(); e_it++) {
		Edge *cur_edge = *e_it;
		assert(cur_edge);
		assert(cur_edge->e_from && cur_edge->e_to);

		group_t *cur_edge_from_group = groups_ptr->group_of(cur_edge->e_from);
		group_t *cur_edge_to_group = groups_ptr->group_of(cur_edge->e_to);

		//exclude the edge introduced by hardware beakpoint
		if (cur_edge_from_group == NULL || cur_edge_to_group == NULL)
			continue;
#if 0
		if (cur_edge_from_group == NULL) {
			if (cur_edge->e_to->get_procname() == "kernel_task") {
				continue;
			} else {
				cerr << "Error event at " << fixed << setprecision(1) << cur_edge->e_from->get_abstime() << endl;
				cerr << "Peer process is " << cur_edge->e_to->get_procname();
				cerr << " at " << fixed << setprecision(1) << cur_edge->e_to->get_abstime() << endl;
				assert(cur_edge_from_group);
			}
		}

		if (cur_edge_to_group == NULL) {
			if (cur_edge->e_from->get_procname() == "kernel_task") {
				continue;
			} else {
					cerr << "Error event at " << fixed << setprecision(1) << cur_edge->e_to->get_abstime() << endl;
					cerr << "Peer process is " << cur_edge->e_from->get_procname();
					cerr << " at " << fixed << setprecision(1) << cur_edge->e_from->get_abstime() << endl;
					assert(cur_edge_to_group);
			}
		}
#endif

		Node *cur_edge_from = nodes_map[cur_edge_from_group];
		Node *cur_edge_to = nodes_map[cur_edge_to_group];
		assert(cur_edge_from && cur_edge_to);

		cur_edge->from = cur_edge_from;
		cur_edge->to = cur_edge_to;
	}
	time(&time_end);
	cerr << "Time cost for graph_init is "  << fixed << setprecision(1) << difftime(time_end, time_begin) << " seconds" << endl;
}

void Graph::remove_node(Node *node)
{
	//cout << "Begin remove node" << endl;
	assert(node != NULL);
	//remove node as well as the connecting edges
	node->remove_edges();
	vector<Node *>::iterator it = find(nodes.begin(), nodes.end(), node);
	assert(it != nodes.end() && *it == node);
	nodes.erase(it);
	delete node;
	//cout << "Finish remove node" << endl;
}

void Graph::remove_edge(Edge *edge)
{
	assert(edge != NULL);
	vector<Edge *>::iterator it = find(edges.begin(), edges.end(), edge);
	if (it != edges.end()) {
		edges.erase(it);
		delete edge;
	}
}

Edge *Graph::check_and_add_edge(Edge *e)
{
	Edge *ret = NULL;
	graph_edge_mtx.lock();
	/*
	vector<Edge *>::iterator it;
	for (it = edges.begin(); it != edges.end(); it++)
		if (**it == *e) {
			assert((*it)->e_from == e->e_from);
			assert((*it)->e_to == e->e_to);
			graph_edge_mtx.unlock();
			return *it;
		}
	*/
	if (edge_map.find(e->e_from) != edge_map.end()) {
		if (edge_map[e->e_from]->e_to == e->e_to) {
			ret = edge_map[e->e_from];
			graph_edge_mtx.unlock();
			return ret;
		}
	}
	edge_map[e->e_from] = e;
	edges.push_back(e);
	graph_edge_mtx.unlock();
	return NULL;
} 

void Graph::add_edge(Edge *e)
{
	edges.push_back(e);
	//data structure subjects to modification if performance is not satisfied.
}

vector<Node *> Graph::get_nodes_for_tid(tid_t tid)
{
	vector<Node *> result;
	result.clear();
	// the version constructed with groups
	gid_group_map_t tgroups = groups_ptr->get_groups_by_tid(tid);
	gid_group_map_t::iterator it;
	for (it = tgroups.begin(); it != tgroups.end(); it++){
		assert(nodes_map.find(it->second) != nodes_map.end());
		result.push_back(nodes_map[it->second]);
	}
	//add versions that constructed with nodes
	return result;
}

vector<Node *> Graph::get_nodes_in_thread_after(Node *cur_node)
{
	vector<Node *> ret = get_nodes_for_tid(cur_node->get_tid());
	vector<Node *>::iterator it = find(ret.begin(), ret.end(), cur_node);
	ret.erase(ret.begin(), it);
	return ret;
}

Node *Graph::id_to_node(group_id_t gid)
{
	group_t * id_to_group = groups_ptr->get_group_by_gid(gid);
	if (nodes_map.find(id_to_group) != nodes_map.end())
		return nodes_map[id_to_group];
	cerr << "Unknown group id " << hex << gid << endl;
	return NULL;
}

Node *Graph::get_spinning_node()
{
	group_t *spinning_group = groups_ptr->spinning_group();
	if (spinning_group == NULL)
		return NULL;
	if (nodes_map.find(spinning_group) != nodes_map.end()) {
		assert(spinning_group);
		assert(nodes_map[spinning_group]);
		return nodes_map[spinning_group];
	}
	return NULL;
}

void Graph::streamout_nodes_and_edges(string path)
{
	ofstream output(path);
	vector<Node *>::iterator it;
	for (it = nodes.begin(); it != nodes.end(); it++) {
		Node *node = *it;
		map<event_t *, Edge *> edges = node->get_out_edges();
		if (edges.size() > 3) {
			output << "Randomly check node with group " << hex << node->get_gid() << endl; 
			map<event_t *, Edge *>::iterator e_it;
			for (e_it = edges.begin(); e_it != edges.end(); e_it++) {
				Edge *cur_edge = e_it->second;
				assert(node == cur_edge->from);
				output << "Edge " << fixed << setprecision(1) << cur_edge->e_from->get_abstime();
				output << "->" << fixed << setprecision(1) << cur_edge->e_to->get_abstime() << endl;
			}
			break;
		}
	}
	output.close();
}

void Graph::check_pattern_for_mkrun(string path)
{
	ofstream output(path);
	vector<Node *>::iterator it;
	for (it = nodes.begin(); it != nodes.end(); it++) {
		Node *node = *it;
		if(node->contains_noncausual_mk_edge() == true)
			output << "Group id " << hex << node->get_group()->get_group_id() << endl;
	}
	output.close();
}
