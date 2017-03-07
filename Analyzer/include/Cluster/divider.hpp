#ifndef DIVIDER_HPP
#define DIVIDER_HPP
#include "group.hpp"

#define SUB_BASE_SHIFT 60
#define CANCELLED	0UL
#define FIRED		1UL
#define INDEX_MASK  ((1UL << SUB_BASE_SHIFT) - 1)

class Divider {
	list<event_t *> &ev_list;
	uint32_t divide_type;
	event_t* divide_event;
	typedef map<uint64_t, list<event_t *> > sub_lists_t;
	sub_lists_t sublists;
	uint64_t cancel_counter;
	uint64_t fired_counter;

	bool match_callevents(callcreate_ev_t* event,  callout_ev_t * divider);
	bool match_callevents(callout_ev_t* event,  callout_ev_t * divider);
	bool match_callevents(callcancel_ev_t* event,  callout_ev_t * divider);

public:
	Divider(list<event_t *>& ev_list, list<event_t *> &_bt_list, uint32_t divide_type, event_t *divide_event);
	callout_ev_t* get_divider(list<event_t *> &bt_list);
	void divide(void);
	void normalize_fired(void);
	void normalize_cancelled(void);
	void compare(void);
};

#endif
