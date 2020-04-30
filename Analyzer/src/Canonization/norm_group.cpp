#include "canonization.hpp"
#define DEBUG_NORMGROUP 1

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
    //std::list<EventBase *> &events = group->get_container();
    //std::list<EventBase *>::iterator it;
    //for (it = events.begin(); it != events.end(); it++) {
	for (auto event: group->get_container()) {
        if (event->get_op() == "MSC_thread_switch" 
			|| key_events[event->get_event_type()] == false)
            continue;

        //if (key_events[(*it)->get_event_type()] == true) {
		NormEvent *NormGroupEvent = new NormEvent(event);
		if (!NormGroupEvent) {
			LOG_S(INFO) << "OOM, no space for NormGroupevent" << std::endl;
			exit(EXIT_FAILURE);
		}
		normalized_events.push_back(NormGroupEvent);
        //}
    }
}

void NormGroup::print_events()
{
    LOG_S(INFO) << "Normalize Group #" << std::hex << group->get_group_id() << std::endl;
	for (auto event: normalized_events) {
		EventBase *real_event = event->get_real_event();
		LOG_S(INFO) << std::hex << get_group_id()\
			<< " Event " << real_event->get_op() \
			<< " at "\
			<< std::fixed << std::setprecision(1) << real_event->get_abstime()
			<< std::endl;
	}
}

bool NormGroup::is_subset_of(std::list<NormEvent *> peer_set)
{
    auto peer_rit = peer_set.rbegin();
    auto this_rit = normalized_events.rbegin();
    int len = normalized_events.size();
    int peer_len = peer_set.size();
    //len = len > 5 ? 5 : len;
	len = len > peer_len ? peer_len : len;
	

#if DEBUG_NORMGROUP 
	LOG_S(INFO) << "check last " << std::dec << len << " events" << std::endl;
#endif
	while (len > 0) {
		assert(this_rit != normalized_events.rend());
		bool equal = false;
		while (peer_len > 0) {
			assert(peer_rit != peer_set.rend());
			equal = (**this_rit) == (**peer_rit);

#if DEBUG_NORMGROUP 
			if (equal) {
            LOG_S(INFO) << "Event " << std::fixed << std::setprecision(1)\
				<< (*this_rit)->get_real_event()->get_abstime()\
            	<< " == "\
            	<< "Event "<< std::fixed << std::setprecision(1)\
				<< (*peer_rit)->get_real_event()->get_abstime()\
            	<< std::endl;
			} else {
            LOG_S(INFO) << "Event " << std::fixed << std::setprecision(1)\
				<< (*this_rit)->get_real_event()->get_abstime()\
            	<< " != "\
            	<< "Event "<< std::fixed << std::setprecision(1)\
				<< (*peer_rit)->get_real_event()->get_abstime()\
            	<< std::endl;
			}
#endif

			peer_len--;
			peer_rit++;
			if (equal) {
				//check next event
				break;
			}
		}

		if (equal == false)
			return false;
		this_rit++;
		len--;
	}

    if (len <= 0)
        return true;
    return false;
}


bool NormGroup::operator==(NormGroup &other)
{

#if 0
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
#endif

    std::list<NormEvent *> other_normalized_events = other.get_normalized_events();
#if DEBUG_NORMGROUP 
    LOG_S(INFO) << "Group # " <<  std::hex << get_group_id() << " size = " << normalized_events.size() << std::endl;
	print_events();
    LOG_S(INFO) << "Group # " <<  std::hex << other.get_group_id() << " size = " << other_normalized_events.size() << std::endl;
	other.print_events();
#endif

    if (!is_subset_of(other_normalized_events) || !other.is_subset_of(normalized_events)) {
#if DEBUG_NORMGROUP 
        LOG_S(INFO) << "Group #"<< std::hex << get_group_id() << " != " << "Group #" << std::hex <<other.get_group_id() << std::endl;
#endif
        return false;
    }

#if DEBUG_NORMGROUP 
    LOG_S(INFO) << "Group #"<< std::hex << get_group_id() << " == " << "Group #" << std::hex <<other.get_group_id() << std::endl;
#endif
    return true;
}

bool NormGroup::operator!=(NormGroup &other)
{
    return !(*this == other);
}
