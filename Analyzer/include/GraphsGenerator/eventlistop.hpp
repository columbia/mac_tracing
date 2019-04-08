#ifndef EVENTLIST_HPP
#define EVENTLIST_HPP

#include "parser.hpp"
//namespace EventListOp
class EventLists
{
	map<uint64_t, list<event_t *>>event_lists;
	int generate_event_lists(LoadData::meta_data_t);
	void clear_event_list();
public:
	EventLists(LoadData::meta_data_t);
	~EventLists();
	map<uint64_t, list<event_t *> >&get_event_lists();
	static void sort_event_list(list<event_t*>&);
	void dump_all_event_to_file(string filepath);
	void streamout_all_event(string filepath);
	void streamout_backtrace(string filepath);
};
#endif
