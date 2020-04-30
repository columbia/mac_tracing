#include "base.hpp"
#include <math.h>

EventBase::EventBase(double _timestamp, int _event_type, std::string _op, uint64_t _tid, uint32_t _core_id, std::string _procname)
:ProcessInfo(_tid, _procname),
TimeInfo(_timestamp),
EventType(_event_type, _op)
{
	assert(event_type > 0 && event_type <= 28);
    core_id = _core_id;
    group_id = -1;
    complete = false;
    event_prev = nullptr;
    event_peer = nullptr;
    tfl_index = 0;
	propagated_frames.clear();
}


EventBase::EventBase(EventBase *base)
:ProcessInfo(base->get_tid(), base->get_procname()),
TimeInfo(base->get_abstime()),
EventType(base->get_event_type(), base->get_op())
{
    *this = *base;
}

void EventBase::decode_event(bool is_verbose, std::ofstream &outfile) 
{
    outfile << "\n*****" << std::endl;
    outfile << "\n group_id = " << std::right << std::hex << get_group_id();
    outfile << "\n [" << std::dec << get_pid() <<"] " << get_procname() << "(" << std::hex << get_tid() << ")" << get_coreid();
    outfile << "\n\t" << get_op();
    outfile << "\n\t" << std::fixed << std::setprecision(1) << get_abstime();
    outfile << std::endl;
}

void EventBase::streamout_event(std::ofstream &outfile)
{

    outfile << std::right << std::hex << get_group_id();
    outfile << "\t" << std::fixed << std::setprecision(1) << get_abstime();
    outfile << "\t" << get_tid();
    outfile << "\t" << get_procname();
    outfile << "\t" << get_op();

    if (event_peer != nullptr)
     	outfile << " <- / -> \t" << event_peer->get_tid()\
        	<< "\t" << event_peer->get_procname()\
    		<< "\t" << std::fixed << std::setprecision(1) << event_peer->get_abstime();
}

void EventBase::streamout_event(std::ostream &out)
{
    out << std::right << std::hex << get_group_id();
    out << "\t" << std::fixed << std::setprecision(1) << get_abstime();
    out << "\t" << std::hex << get_tid();
    out << "\t" << get_procname();
    out << "\t" << get_op();
	out << "\t 0x" << std::hex << get_group_id();
    if (event_peer != nullptr)
     	out << "\t" << std::hex << event_peer->get_tid()\
        	<< "\t" << event_peer->get_procname()\
    		<< "\t" << std::fixed << std::setprecision(1) << event_peer->get_abstime();
}

std::string EventBase::replace_blank(std::string s)
{
	std::string ret(s);
	assert(s.size() == ret.size());
	std::replace(ret.begin(), ret.end(), ' ', '#');
	return ret;
}

void EventBase::tfl_event(std::ofstream &outfile)
{
    double prev = event_prev != nullptr? event_prev->get_abstime(): -1;
    outfile << replace_blank(get_procname()) << " " << std::dec << get_event_type();
    if (event_peer != nullptr)
        outfile << " " << replace_blank(event_peer->get_procname());
    else
        outfile << " N/A";

    if (event_prev != nullptr)
        outfile << " " << std::fixed << std::dec << int(log10(get_abstime() - prev));
    else
        outfile << " N/A"; 
	for (auto frame: propagated_frames)
		outfile << " " << std::hex << get_pid() << "_" << std::hex << frame;

    //outfile << "\t" << std::hex << get_group_id();
}
