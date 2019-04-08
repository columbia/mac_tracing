#include "group.hpp"

Group::Group(uint64_t _group_id, event_t * _root)
{
	root = _root;
	group_id = _group_id;
	cluster_id = msg_peer = msg_bank_holder = -1;
	blockinvoke_level = 0;
	nsapp_event = NULL;
	time_span = 0.0;
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

void Group::decode_group(ofstream & output)
{	
	sort_container();
	list<event_t*>::iterator it;
	for (it = container.begin(); it != container.end(); it++) {
		event_t * cur_ev = *it;
		cur_ev->decode_event(0, output);
	}
}

void Group::streamout_group(ofstream & output)
{	
	sort_container();
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
