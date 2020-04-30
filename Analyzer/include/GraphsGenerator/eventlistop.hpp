#ifndef EVENTLIST_HPP
#define EVENTLIST_HPP

#include "parser.hpp"
class EventLists
{
	typedef std::list<EventBase *> event_list_t;
    std::map<uint64_t, std::list<EventBase *> > event_lists;
	std::map<uint64_t, EventBase*> tfl_to_event_map;
    int generate_event_lists(LoadData::meta_data_t);
    void clear_event_list();
	bool owner;
public:
    EventLists(LoadData::meta_data_t);
    EventLists(std::list<EventBase *> &eventlist);
    ~EventLists();
    std::map<uint64_t, std::list<EventBase *> > &get_event_lists();
    static void sort_event_list(std::list<EventBase*>&);
    void binary_search(std::list<EventBase *> list, std::string timestamp, std::list<EventBase *> &result);
    void filter_list_with_tid(std::list<EventBase *> &list, uint64_t tid);
    void filter_list_with_procname(std::list<EventBase *> &list, std::string procname);
	void label_tfl_index(std::list<EventBase *> &);
	EventBase *tfl_index2event(uint64_t index);
    void show_events(std::string timestamp, const std::string type, std::string procname, tid_t tid);
    void dump_all_event_to_file(std::string filepath);
    void streamout_all_event(std::string filepath);
	void tfl_peer_distance(std::string filepath);
    void tfl_all_event(std::string filepath);
	void tfl_by_thread(std::string filepath);
    void streamout_backtrace(std::string filepath);
};
#endif
