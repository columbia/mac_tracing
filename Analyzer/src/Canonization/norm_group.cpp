#include "canonization.hpp"
#define DEBUG_NORMGROUP 0

NormGroup::NormGroup(Group *g, map<event_id_t, bool> &_key_events)
:key_events(_key_events)
{
	group = g;
	normalized_events.clear();
	normalize_events();
}

NormGroup::~NormGroup(void)
{
	list<norm_ev_t *>::iterator it;
	for (it = normalized_events.begin(); it != normalized_events.end(); it++) {
		assert(*it != NULL);
		delete(*it);
	}
	normalized_events.clear();
}

void NormGroup::normalize_events(void)
{
	list<event_t *> &events = group->get_container();
	list<event_t *>::iterator it;
#if DEBUG_NORMGROUP 
	cerr << "Normalize Group #" << hex << group->get_group_id() << endl;
#endif
	for (it = events.begin(); it != events.end(); it++) {
		if (key_events[(*it)->get_event_id()] == true) {
			norm_ev_t *norm_event = new norm_ev_t((*it));
			if (!norm_event) {
				cerr << "OOM, no space for norm_event" << endl;
				exit(EXIT_FAILURE);
			}
#if DEBUG_NORMGROUP 
			cerr << "Event " << (*it)->get_op() << endl;
#endif
			normalized_events.push_back(norm_event);
		}
	}
}

bool NormGroup::is_subset_of(list<norm_ev_t *> peer_set)
{
	list<norm_ev_t *>::iterator this_it;
	list<norm_ev_t *>::iterator other_it;

	for(this_it = normalized_events.begin(), other_it = peer_set.begin();
		this_it != normalized_events.end() && other_it != peer_set.end(); ) {
		while(other_it != peer_set.end() && (**this_it) != (**other_it))
			other_it++;
		if (other_it != peer_set.end()) {
#if DEBUG_NORMGROUP 
			cerr << "Event "<< fixed << setprecision(1) << (*this_it)->get_real_event()->get_abstime();
			cerr << " == "; 
			cerr << "Event "<< fixed << setprecision(1) << (*other_it)->get_real_event()->get_abstime();
			cerr << endl;
#endif
		} else {
			return false;
		}	
		assert(other_it != peer_set.end() && this_it != normalized_events.end());
		this_it++;
		//other_it++; comment out this line to exclude the repeating calls due to failure
	}

	if (this_it == normalized_events.end())
		return true;
	return false;
}

bool NormGroup::operator==(NormGroup &other)
{

	if (get_group_tags().size()) {
		map<string, uint32_t> peer_group_tags = other.get_group_tags();
		map<string, uint32_t>::iterator it;

		/* to fail quickly with comarision of tags, eg backtraces */
		if (get_group_tags().size() != peer_group_tags.size())
			return false;
		
		for (it = get_group_tags().begin(); it != get_group_tags().end(); it++) {
			string tag = it->first;
			if (peer_group_tags.find(tag) == peer_group_tags.end() || peer_group_tags[tag] != it->second)
				return false;
		}
	}

	list<norm_ev_t *> other_normalized_events = other.get_normalized_events();
#if DEBUG_NORMGROUP 
	cerr << "Group # " <<  hex << get_group_id() << " size = " << normalized_events.size() << endl;
	cerr << "Group # " <<  hex << other.get_group_id() << " size = " << other_normalized_events.size() << endl;
#endif

	if (!is_subset_of(other_normalized_events) || !other.is_subset_of(normalized_events)) {
#if DEBUG_NORMGROUP 
		cerr << "Group #"<< hex << get_group_id() << " != " << "Group #" << hex <<other.get_group_id() << endl;
#endif
		return false;
	}
	/*
	list<norm_ev_t *>::iterator this_it;
	list<norm_ev_t *>::iterator other_it;

	// TODO: number of event may be misleading, for events repeated periodically 
	//if (other_normalized_events.size() != normalized_events.size())
		//return false;
	this_it = normalized_events.begin();
	other_it = other_normalized_events.begin();
	

	for(; this_it != normalized_events.end() && other_it != other_normalized_event.end(); ) {
		
		if (**this_it != **other_it)
			return false;
		this_it++;
		other_it++;
	}
	*/
#if DEBUG_NORMGROUP 
	cerr << "Group #"<< hex << get_group_id() << " == " << "Group #" << hex <<other.get_group_id() << endl;
#endif
	return true;
}

bool NormGroup::operator!=(NormGroup &other)
{
	return !(*this == other);
}
