#ifndef CANONIZATION_HPP
#define CANONIZATION_HPP

#include "group.hpp"
#include <math.h>

class NormEvent {
	event_t *event;
public:
	string proc_name;
	event_id_t event_type;
	pid_t peer;

	NormEvent(event_t *ev);
	bool operator==(NormEvent & other);
	bool operator!=(NormEvent & other);
	event_t *get_real_event(void);
};
typedef NormEvent norm_ev_t;

//////////////////////////////////////////////
class NormGroup {
	group_t *group;
 // reference to original group
	map<event_id_t, bool> &key_events;
	list<norm_ev_t *> normalized_events;
	void normalize_events(void);
public:
	NormGroup(Group *g, map<event_id_t, bool> &key_events);
	~NormGroup();

	uint64_t original_size(void) {return group->get_container().size();}
	uint64_t normalized_size() {return normalized_events.size();}
	uint64_t get_group_id(void) {return group->get_group_id();}
	list<norm_ev_t *> &get_normalized_events(void) {return normalized_events;}
	map<string, uint32_t> &get_group_tags(void) {return group->get_group_tags();}

	bool is_subset_of(list<norm_ev_t *>);
	bool is_empty() {return normalized_events.size() == 0;}
	bool operator==(NormGroup &other);
	bool operator!=(NormGroup &other);
};
typedef NormGroup norm_group_t;
///////////////////////////////////////////
#include "graph.hpp"
class NormNode {
	Node *node;
	NormGroup *norm_group;
public:
	NormNode(Node *_node, map<event_id_t, bool> &key_events);
	~NormNode();
	bool is_empty();
	bool operator==(NormNode &other);
	bool operator!=(NormNode &other);
	Node *get_node(void) {return node;}
	NormGroup *get_norm_group() {return norm_group;}
};

#endif
