#include "group.hpp"

Group::Group(uint64_t _group_id, event_t * _root)
{
	root = _root;
	group_id = _group_id;
	cluster_id = msg_peer = msg_bank_holder = -1;
	blockinvoke_level = 0;
	is_ground = is_infected = false;
	nsapp_event = NULL;
	infected_event = NULL;
	time_span = 0.0;
	container.clear();
}

Group::~Group(void)
{
	root = infected_event = NULL;
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
	event->set_group_id(group_id);
	container.push_back(event);

	if (event->get_event_id() == NSAPPEVENT_EVENT)
		nsapp_event = event;

	if (is_ground == false && event->check_ground() == true)
		is_ground = true;

	backtrace_ev_t * bt_event = dynamic_cast<backtrace_ev_t *>(event);
	if (bt_event != NULL && bt_event->check_infected() == true) {
		is_infected = true;
		infected_event = bt_event;
	}
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
	list<event_t*>::iterator it;
	for (it = container.begin(); it != container.end(); it++) {
		event_t * cur_ev = *it;
		cur_ev->decode_event(0, output);
	}
}

void Group::streamout_group(ofstream & output)
{	
	list<event_t*>::iterator it;
	for (it = container.begin(); it != container.end(); it++) {
		event_t * cur_ev = *it;
		cur_ev->streamout_event(output);
	}
}

void Group::pic_group(ofstream & output)
{
	mtx.lock();
	if (container.size() == 0)
		cerr << "Group " << hex << group_id << " is of size zero" << endl;
	mtx.unlock();
	output << hex << group_id << "\t" << get_first_event()->get_procname() << "_" << get_first_event()->get_tid() << endl;
	output << "Time " << fixed << setprecision(1) << get_first_event()->get_abstime();
	output << " ~ " << fixed << setprecision(1) << get_last_event()->get_abstime() << endl;
}
