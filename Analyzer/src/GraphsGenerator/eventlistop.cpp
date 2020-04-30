#include "eventlistop.hpp"
#include "mach_msg.hpp"
#include <time.h>
//TODO: fix the op category, which is different from the event_id
EventLists::EventLists(LoadData::meta_data_t meta_data)
{
	owner = true;
    generate_event_lists(meta_data);
}

EventLists::EventLists(std::list<EventBase *> &eventlist)
{
	owner = false;
    event_lists[0] = eventlist;    
}

EventLists::~EventLists()
{
    //if (event_lists.size() > 1)
	if (owner == true)
        clear_event_list();
}

std::map<uint64_t, std::list<EventBase *>> &EventLists::get_event_lists()
{
    return event_lists;
}

void EventLists::label_tfl_index(std::list<EventBase *> &event_list)
{
	// prepare tfl index
    uint64_t index = 0;
	EventCategory event_checker;
	
	tfl_to_event_map.clear();
	for (auto cur_ev : event_list) {
		event_type_t id = cur_ev->get_event_type();
	//	if (!(event_checker.is_semantics_event(id) || event_checker.is_structure_event(id))
	//		 || event_checker.is_tfl_filtered(id))
		if (id == FAKED_WOKEN_EVENT || id == APPLOG_EVENT)
			continue;
        cur_ev->set_tfl_index(index);
		tfl_to_event_map[index] = cur_ev;
		index++;
	}
}

static bool lldb_initialized = false;

int EventLists::generate_event_lists(LoadData::meta_data_t meta_data)
{
    time_t time_begin, time_end;
    std::cout << "Begin parsing..." << std::endl;
    time(&time_begin);
    LoadData::preload();

#if defined(__APPLE__)
	if (!lldb_initialized) {
    	lldb::SBDebugger::Initialize();
		lldb_initialized = true;
	}
#endif
    event_lists = Parse::divide_and_parse();
#if defined(__APPLE__)
    lldb::SBDebugger::Terminate();
#endif
    //EventLists::sort_event_list(event_lists[0]);
	event_lists[0].sort(Parse::EventComparator::compare_time);
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
		return;
        //exit(EXIT_FAILURE);
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
		return;
        //exit(EXIT_FAILURE);
    }
    std::list<EventBase *> &evlist = event_lists[0];
    std::list<EventBase *>::iterator it;
    for(it = evlist.begin(); it != evlist.end(); it++) {
        EventBase *cur_ev = *it;
        cur_ev->streamout_event(dump);
    }
    dump.close();
}

void EventLists::tfl_peer_distance(std::string filepath)
{
	std::ofstream dump(filepath);
    if (dump.fail()) {
        std::cout << "unable to open file " << filepath << std::endl;
		return;
    }
	for (auto cur_ev : event_lists[0]) {
		event_type_t id = cur_ev->get_event_type();
		if (id == FAKED_WOKEN_EVENT || id == APPLOG_EVENT)
			continue;
		if (cur_ev->get_event_peer() != nullptr) {
			/*to do check tfl index*/
			dump << int64_t(cur_ev->get_tfl_index() - cur_ev->get_event_peer()->get_tfl_index())<< std::endl;
		}
		else
			dump << "0" << std::endl;
	}
	dump.close();
}

void EventLists::tfl_all_event(std::string filepath)
{
    std::ofstream dump(filepath);
    if (dump.fail()) {
        std::cout << "unable to open file " << filepath << std::endl;
		return;
    }

	EventCategory event_checker;
	for (auto cur_ev : event_lists[0]) {
		event_type_t id = cur_ev->get_event_type();
		//if (!(event_checker.is_semantics_event(id) || event_checker.is_structure_event(id))
		//	 || event_checker.is_tfl_filtered(id))
		if (id == FAKED_WOKEN_EVENT || id == APPLOG_EVENT)
			continue;
        cur_ev->tfl_event(dump);
		if (id != MSG_EVENT && id != SYSCALL_EVENT && id != BACKTRACE_EVENT)
			dump << " N/A";

		dump << std::endl;
    }
    dump.close();
}

void EventLists::tfl_by_thread(std::string filepath)
{
	std::ofstream dump(filepath);
	std::map<tid_t, event_list_t> tid_lists;

	for (auto event : event_lists[0]) {
		if (tid_lists.find(event->get_tid()) == tid_lists.end()) {
			event_list_t empty;
			empty.clear();
			tid_lists[event->get_tid()] = empty;
		}
		tid_lists[event->get_tid()].push_back(event);
	}
	
	for (auto element : tid_lists) {
		EventBase *prev = nullptr;
		for (auto cur_ev : element.second) {
			event_type_t id = cur_ev->get_event_type();
			if (id == FAKED_WOKEN_EVENT || id == APPLOG_EVENT)
				continue;
			cur_ev->EventBase::tfl_event(dump);
			if (prev == nullptr || prev->get_group_id() != cur_ev->get_group_id())
				dump << "\tbegin" << std::endl;
			else
				dump << "\tnone" << std::endl;
			prev = cur_ev;
		}
	}
	
	dump.close();
}

void EventLists::streamout_backtrace(std::string filepath)
{
    std::ofstream dump(filepath);
    if (dump.fail()) {
        std::cout << "unable to open file " << filepath << std::endl;
		return;
        //exit(EXIT_FAILURE);
    }

    std::list<EventBase *> &evlist = event_lists[BACKTRACE];
    std::list<EventBase *>::iterator it;
    for(it = evlist.begin(); it != evlist.end(); it++) {
        EventBase *cur_ev = *it;
#if defined(__APPLE__)
        if (dynamic_cast<BacktraceEvent *>(cur_ev))
            cur_ev->streamout_event(dump);
#endif
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
