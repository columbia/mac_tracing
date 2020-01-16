#include "canonization.hpp"
#define DEBUG_NORMGROUP 0

NormGroup::NormGroup(Group *g,std::map<EventType::event_type_t, bool> &_key_events)
:key_events(_key_events)
{
    group = g;
    normalized_events.clear();
    normalize_events();
}

NormGroup::~NormGroup(void)
{
    std::list<NormEvent *>::iterator it;
    for (it = normalized_events.begin(); it != normalized_events.end(); it++) {
        assert(*it != nullptr);
        delete(*it);
    }
    normalized_events.clear();
}

void NormGroup::normalize_events(void)
{
    std::list<EventBase *> &events = group->get_container();
    std::list<EventBase *>::iterator it;
#if DEBUG_NORMGROUP 
    std::cerr << "Normalize Group #" << std::hex << group->get_group_id() << std::endl;
#endif

            
    for (it = events.begin(); it != events.end(); it++) {
        if ((*it)->get_op() == "MSC_thread_switch")
            continue;
        if (key_events[(*it)->get_event_type()] == true) {
            NormEvent *NormGroupevent = new NormEvent((*it));
            if (!NormGroupevent) {
                std::cerr << "OOM, no space for NormGroupevent" << std::endl;
                exit(EXIT_FAILURE);
            }
#if DEBUG_NORMGROUP 
            std::cerr << std::hex << get_group_id() << " Event " << (*it)->get_op();
            std::cerr << " at " << std::fixed << std::setprecision(1) << (*it)->get_abstime() << std::endl;
#endif
            normalized_events.push_back(NormGroupevent);
        }
    }
}

bool NormGroup::is_subset_of(std::list<NormEvent *> peer_set)
{
    std::list<NormEvent *>::iterator this_it;
    std::list<NormEvent *>::iterator other_it;

    for(this_it = normalized_events.begin(), other_it = peer_set.begin();
        this_it != normalized_events.end() && other_it != peer_set.end(); ) {
        //if ((*other_it)->get_real_event()->get_op() == "MACH_wait") {
       //}
        while(other_it != peer_set.end() && (**this_it) != (**other_it))
            other_it++;
        if (other_it != peer_set.end()) {
#if DEBUG_NORMGROUP 
            std::cerr << "Event "<< std::fixed << std::setprecision(1) << (*this_it)->get_real_event()->get_abstime();
            std::cerr << " == "; 
            std::cerr << "Event "<< std::fixed << std::setprecision(1) << (*other_it)->get_real_event()->get_abstime();
            std::cerr << std::endl;
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
        std::map<std::string, uint32_t> peer_group_tags = other.get_group_tags();
        std::map<std::string, uint32_t>::iterator it;

        /* to fail quickly with comarision of tags, eg backtraces */
        if (get_group_tags().size() != peer_group_tags.size())
            return false;
        
        for (it = get_group_tags().begin(); it != get_group_tags().end(); it++) {
            std::string tag = it->first;
            if (peer_group_tags.find(tag) == peer_group_tags.end() || peer_group_tags[tag] != it->second)
                return false;
        }
    }

    std::list<NormEvent *> other_normalized_events = other.get_normalized_events();
#if DEBUG_NORMGROUP 
    std::cerr << "Group # " <<  std::hex << get_group_id() << " size = " << normalized_events.size() << std::endl;
    std::cerr << "Group # " <<  std::hex << other.get_group_id() << " size = " << other_normalized_events.size() << std::endl;
#endif

    if (!is_subset_of(other_normalized_events) || !other.is_subset_of(normalized_events)) {
#if DEBUG_NORMGROUP 
        std::cerr << "Group #"<< std::hex << get_group_id() << " != " << "Group #" << std::hex <<other.get_group_id() << std::endl;
#endif
        return false;
    }
    /*
    std::list<NormEvent *>::iterator this_it;
    std::list<NormEvent *>::iterator other_it;

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
    std::cerr << "Group #"<< std::hex << get_group_id() << " == " << "Group #" << std::hex <<other.get_group_id() << std::endl;
#endif
    return true;
}

bool NormGroup::operator!=(NormGroup &other)
{
    return !(*this == other);
}
