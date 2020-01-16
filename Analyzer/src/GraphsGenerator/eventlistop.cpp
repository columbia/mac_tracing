#include "eventlistop.hpp"
#include "mach_msg.hpp"
#include <time.h>
EventLists::EventLists(LoadData::meta_data_t meta_data)
{
    generate_event_lists(meta_data);
}

EventLists::EventLists(std::list<EventBase *> &eventlist)
{
    //TODO: fix the op category, which is different from the event_id
    event_lists[0] = eventlist;    
}

EventLists::~EventLists()
{
    if (event_lists.size() > 1)
        clear_event_list();
}

std::map<uint64_t, std::list<EventBase *>> &EventLists::get_event_lists()
{
    return event_lists;
}

int EventLists::generate_event_lists(LoadData::meta_data_t meta_data)
{
    time_t time_begin, time_end;
    std::cout << "Begin parsing..." << std::endl;
    time(&time_begin);
    lldb::SBDebugger::Initialize();
    LoadData::preload();
    event_lists = Parse::divide_and_parse();
    EventLists::sort_event_list(event_lists[0]);
    lldb::SBDebugger::Terminate();

    std::list<EventBase *>::iterator it;
    uint64_t index = 0;
    for (it = event_lists[0].begin(); it != event_lists[0].end(); it++, index++) {
        EventBase *cur_ev = *it;
		if (cur_ev->get_event_type() == APPLOG_EVENT)
			continue;
        cur_ev->set_tfl_index(index);
		tfl_to_event_map[index] = cur_ev;
	}
    time(&time_end);
    std::cout << "Time cost for parsing: ";
    std::cout << std::fixed << std::setprecision(1) << difftime(time_end, time_begin) << " seconds"<< std::endl;
    return 0;
}

void EventLists::sort_event_list(std::list<EventBase *> &evlist)
{
    evlist.sort(Parse::EventComparator::compare_time);
}

void EventLists::dump_all_event_to_file(std::string filepath)
{
    std::ofstream dump(filepath);
    if (dump.fail()) {
        std::cout << "unable to open file " << filepath << std::endl;
        exit(EXIT_FAILURE);
    }

    std::list<EventBase *> &evlist = event_lists[0];
    std::list<EventBase *>::iterator it;
    for(it = evlist.begin(); it != evlist.end(); it++) {
        EventBase *cur_ev = *it;
        cur_ev->decode_event(1, dump);
    }
    dump.close();
}

void EventLists::streamout_all_event(std::string filepath)
{
    std::ofstream dump(filepath);
    if (dump.fail()) {
        std::cout << "unable to open file " << filepath << std::endl;
        exit(EXIT_FAILURE);
    }
    std::list<EventBase *> &evlist = event_lists[0];
    std::list<EventBase *>::iterator it;
    for(it = evlist.begin(); it != evlist.end(); it++) {
        EventBase *cur_ev = *it;
        cur_ev->streamout_event(dump);
    }
    dump.close();
}

void EventLists::tfl_all_event(std::string filepath)
{
    std::ofstream dump(filepath);
    if (dump.fail()) {
        std::cout << "unable to open file " << filepath << std::endl;
        exit(EXIT_FAILURE);
    }
    std::list<EventBase *> &evlist = event_lists[0];
    std::list<EventBase *>::iterator it;
    for(it = evlist.begin(); it != evlist.end(); it++) {
        EventBase *cur_ev = *it;
		if (cur_ev->get_event_type() == APPLOG_EVENT)
			continue;
        cur_ev->tfl_event(dump);
    }
    dump.close();
}

void EventLists::streamout_backtrace(std::string filepath)
{
    std::ofstream dump(filepath);
    if (dump.fail()) {
        std::cout << "unable to open file " << filepath << std::endl;
        exit(EXIT_FAILURE);
    }

    std::list<EventBase *> &evlist = event_lists[BACKTRACE];
    std::list<EventBase *>::iterator it;
    for(it = evlist.begin(); it != evlist.end(); it++) {
        EventBase *cur_ev = *it;
        if (dynamic_cast<BacktraceEvent *>(cur_ev))
            cur_ev->streamout_event(dump);
    }
    dump.close();
}

void EventLists::clear_event_list()
{
    std::list<EventBase *> &evlist = event_lists[0];
    std::list<EventBase *>::iterator it;
    for(it = evlist.begin(); it != evlist.end(); it++) {
        EventBase *cur_ev = *it;
        delete(cur_ev);
    }
    evlist.clear();
    event_lists.clear();
}

void EventLists::binary_search(std::list<EventBase *> list, std::string timestamp, std::list<EventBase *> &result)
{
}

void EventLists::filter_list_with_tid(std::list<EventBase *> &list, uint64_t tid)
{
}

void EventLists::filter_list_with_procname(std::list<EventBase *> &list, std::string procname)
{
}

EventBase *EventLists::tfl_index2event(uint64_t index)
{
	if (tfl_to_event_map.find(index) != tfl_to_event_map.end())
		return tfl_to_event_map[index];
	return nullptr;
}

void EventLists::show_events(std::string timestamp, const std::string type, std::string procname, uint64_t tid)
{
    
    std::list<EventBase *> result;
    if (timestamp != "0"){
        binary_search(event_lists[0], timestamp, result);   
        goto out;
    }

    if (type != "null" 
        && (LoadData::op_code_map.find(type) != LoadData::op_code_map.end()))
        result = event_lists[LoadData::op_code_map.at(type)];
    else
        result = event_lists[0];
    
    if (tid != 0)
        filter_list_with_tid(result, tid);

    if (procname != "null")
        filter_list_with_procname(result, procname);   
out:
 	return;      
}
