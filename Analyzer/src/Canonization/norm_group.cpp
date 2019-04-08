#include "canonization.hpp"

NormGroup::NormGroup(Group *g, map<uint64_t, bool> &_key_events)
:key_events(_key_events)
{
	group = g;
	virtual_tid  = -1;
	in = out = 0;
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

	for (it = events.begin(); it != events.end(); it++) {
		if (key_events[LoadData::map_op_code(0, (*it)->get_op())] == true) {
			norm_ev_t *norm_event = new norm_ev_t((*it), virtual_tid);
			if (!norm_event) {
				cerr << "OOM, no space for norm_event" << endl;
				exit(EXIT_FAILURE);
			}
			normalized_events.push_back(norm_event);
		}
	}
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
	list<norm_ev_t *>::iterator this_it;
	list<norm_ev_t *>::iterator other_it;
	/* TODO: number of event may be misleading, for events repeated periodically */
	if (other_normalized_events.size() != normalized_events.size())
		return false;

	for (this_it = normalized_events.begin(), other_it = other_normalized_events.begin();
			this_it != normalized_events.end(); this_it++, other_it++) {
		if (**this_it != **other_it)
			return false;
	}
	return true;
}

bool NormGroup::operator!=(NormGroup &other)
{
	return !(*this == other);
}

void NormGroup::decode_group(ofstream &output)
{
	list<norm_ev_t *>::iterator it;
	for (it = normalized_events.begin(); it != normalized_events.end(); it++) {
		(*it)->decode_event(output);
	}
}
