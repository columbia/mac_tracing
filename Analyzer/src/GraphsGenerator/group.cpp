#include "group.hpp"

Group::Group(uint64_t _group_id, event_t * _root)
{
	root = _root;
	group_id = _group_id;
	cluster_id = msg_peer = msg_bank_holder = -1;
	blockinvoke_level = 0;
	nsapp_event = NULL;
	time_span = 0.0;
	sorted = false;
	container.clear();
}

Group::~Group(void)
{
	root = nsapp_event = NULL;
	container.clear();
}

void Group::set_group_id(uint64_t _group_id)
{
	group_id = _group_id;

	if (root)
		root->set_group_id(group_id);

	list<event_t *>::iterator it;
	for (it = container.begin(); it != container.end(); it++)
		(*it)->set_group_id(group_id);
}

void Group::add_group_peer(string proc_comm)
{
	if (proc_comm.size())
		group_peer.insert(proc_comm);
}

void Group::add_group_tags(vector<string> &desc)
{
	vector<string>::iterator it;
	for (it = desc.begin(); it != desc.end(); it++) {
		group_tags[*it] = group_tags[*it] + 1;
	}
}

void Group::add_group_tags(string desc)
{
	group_tags[desc] = group_tags[desc] + 1;
}

void Group::set_root(event_t *r)
{
	mtx.lock();
	if (root != NULL)
		cerr << "Warning: change root for group " << hex << group_id << endl;
	mtx.unlock();
	root = r;
}

void Group::add_to_container(event_t *event)
{
	assert(event);
	//event->set_group_id(group_id);
	//container.push_back(event);

	if (event->get_event_id() == NSAPPEVENT_EVENT)
		nsapp_event = event;

	if (nsapp_event && event->get_event_id() == TMCALL_CREATE_EVENT) {
		timercreate_ev_t *timer_event = dynamic_cast<timercreate_ev_t *>(event);
		assert(timer_event);
		timer_event->set_is_event_processing();
	}

	event->set_group_id(group_id);
	container.push_back(event);
}

void Group::add_to_container(group_t *group)
{
	list<event_t*>::iterator it;
	list<event_t *> &peer_container = group->get_container();
	for (it = peer_container.begin(); it != peer_container.end(); it++) {
		add_to_container(*it);
	}
}

void Group::empty_container()
{
	list<event_t*>::iterator it;
	for (it = container.begin(); it != container.end(); it++) {
		(*it)->set_group_id(-1);
	}
	container.clear();
}

bool Group::operator==(Group &peer)
{
	sort_container();
	peer.sort_container();
	list<event_t *> &peer_container = peer.get_container();
	list<event_t *>::iterator it;
	list<event_t *>::iterator peer_it;
	for (it = container.begin(), peer_it = peer_container.begin();;) { //it != container.end() && peer_it != peer_container.end();) {
		while (it != container.end() && (*it)->get_event_id() == INTR_EVENT)
			it++;
		while(peer_it != peer_container.end() && (*peer_it)->get_event_id() == INTR_EVENT)
			peer_it++;

		if (it == container.end() && peer_it != peer_container.end())
			return false;
		if (it != container.end() && peer_it == peer_container.end())
			return false;
		if (it == container.end() && peer_it == peer_container.end())
			return true;
		if ((*peer_it)->get_event_id() == (*it)->get_event_id()) {
			it++;
			peer_it++;
		} else
			return false;
	}

	return true;
}

double Group::calculate_time_span()
{
	if (container.size()) {
		sort_container();
		time_span = get_last_event()->get_abstime() - get_first_event()->get_abstime();
	}
	return time_span;
}

int Group::compare_syscall_ret(group_t *peer)
{
	list<event_t *> &peer_container = peer->get_container();
	list<event_t *>::iterator cur_it, peer_it;
	int ret = 0;
	//get the desired event from the event list
	//compare
	for (cur_it = container.begin(), peer_it = peer_container.begin();
		cur_it != container.end() && peer_it != peer_container.end();) {
		while(cur_it != container.end() && (*cur_it)->get_event_id() != SYSCALL_EVENT)
			cur_it++;
		if (cur_it == container.end())
			return ret;

		while(peer_it != peer_container.end() && (*peer_it)->get_event_id() != SYSCALL_EVENT)
			peer_it++;

		if (peer_it == peer_container.end())
			return ret;

		assert(*cur_it != NULL && *peer_it != NULL);
		syscall_ev_t *cur_syscall = dynamic_cast<syscall_ev_t *>(*cur_it);
		syscall_ev_t *peer_syscall = dynamic_cast<syscall_ev_t *>(*peer_it);
		cerr << "Compare event " << fixed << setprecision(1) << cur_syscall->get_abstime();
		cerr << " with " << fixed << setprecision(1) << peer_syscall->get_abstime() << endl;
		if (cur_syscall->get_ret() != peer_syscall->get_ret())
			ret = 1;
		
		cerr << "result is (1 stands for have difference) : " << ret << endl;

		if (ret == 1)
			return ret;
		cur_it++;
		peer_it++;
	}
	return ret;
}

//if no wait event inside the group. return 0
int Group::compare_wait(group_t *peer)
{
	list<event_t *> &peer_container = peer->get_container();
	list<event_t *>::iterator cur_it, peer_it;
	int ret = 0;
	//get the desired event from the event list
	//compare
	for (cur_it = container.begin(), peer_it = peer_container.begin();
		cur_it != container.end() && peer_it != peer_container.end();) {
		while(cur_it != container.end() && (*cur_it)->get_event_id() != WAIT_EVENT)
			cur_it++;
		if (cur_it == container.end())
			return ret;

		while(peer_it != peer_container.end() && (*peer_it)->get_event_id() != WAIT_EVENT)
			peer_it++;
		if (peer_it == peer_container.end())
			return ret;
		double cur_wait = dynamic_cast<wait_ev_t*>(*cur_it)->get_time_cost();
		double peer_wait = dynamic_cast<wait_ev_t*>(*peer_it)->get_time_cost();
		if (cur_wait - peer_wait > 1000000)
			return 1;
		if (peer_wait - cur_wait > 1000000)
			return -1;
		cur_it++;
		peer_it++;
	}
	return ret;
}

int Group::compare_timespan(group_t *peer)
{
	double peer_timespan = peer->calculate_time_span();
	if (time_span - peer_timespan > 1000000)
		return 1;
 	if (peer_timespan - time_span > 1000000)
		return -1;
	return 0;
}

bool Group::contains_noncausual_mk_edge()
{
	sort_container();
	list<event_t*>::iterator it;
	for (it = container.begin(); it != container.end(); it++) {
		mkrun_ev_t *makerun = dynamic_cast<mkrun_ev_t *>(*it);
		if (makerun == NULL)
			continue;
		if (makerun->get_mr_type() == WORKQ_MR || makerun->get_mr_type() < 5) {
			mtx.lock();
			cout << "Group# " << hex << group_id << endl;
			mtx.unlock();
		}
	}
	return false;
}

bool Group::wait_over()
{
	sort_container();
	list<event_t*>::iterator it;
	for (it = container.begin(); it != container.end(); it++) {
		wait_ev_t *wait_event = dynamic_cast<wait_ev_t *>(*it);
		if (wait_event != NULL) {
			double wait_length = wait_event->get_time_cost();
			if (wait_length > 2000000)
				return true;
			if (wait_length < 10e-8 && wait_length > -10e-8)
				return true;
		}
	}
	return false;
}

void Group::decode_group(ofstream &output)
{	
	sort_container();
	list<event_t*>::iterator it;
	for (it = container.begin(); it != container.end(); it++) {
		event_t * cur_ev = *it;
		cur_ev->decode_event(0, output);
	}
}

void Group::streamout_group(ofstream &output)
{	
	sort_container();
	list<event_t*>::iterator it;
	for (it = container.begin(); it != container.end(); it++) {
		event_t * cur_ev = *it;
		cur_ev->streamout_event(output);
	}
}

void Group::streamout_group(ostream &output)
{	
	//sort_container();
	list<event_t*>::iterator it;
	for (it = container.begin(); it != container.end(); it++) {
		event_t * cur_ev = *it;
		cur_ev->streamout_event(output);
	}
}

void Group::pic_group(ostream &output)
{
	mtx.lock();
	if (container.size() == 0)
		cerr << "Group " << hex << group_id << " is of size zero" << endl;
	mtx.unlock();
	output << hex << group_id << "\t" << get_first_event()->get_procname() << "_" << get_first_event()->get_tid() << endl;
	output << "Time " << fixed << setprecision(1) << get_first_event()->get_abstime();
	output << " ~ " << fixed << setprecision(1) << get_last_event()->get_abstime() << endl;
}
