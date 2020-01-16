#include "base.hpp"
#include <math.h>

EventBase::EventBase(double _timestamp, int _event_type, std::string _op, uint64_t _tid, uint32_t _core_id, std::string _procname)
:ProcessInfo(_tid, _procname),
TimeInfo(_timestamp),
EventType(_event_type, _op)
{
    core_id = _core_id;
    group_id = -1;
    complete = false;
    event_prev = nullptr;
    event_peer = nullptr;
    tfl_index = 0;
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
    out << "\t" << get_tid();
    out << "\t" << get_procname();
    out << "\t" << get_op();
    if (event_peer != nullptr)
     	out << "\t" << event_peer->get_tid()\
        	<< "\t" << event_peer->get_procname()\
    		<< "\t" << std::fixed << std::setprecision(1) << event_peer->get_abstime();
}

void EventBase::tfl_event(std::ofstream  &outfile)
{
    double prev = event_prev != nullptr? event_prev->get_abstime(): -1;
    outfile << get_procname() << "\t" << get_event_type();
    if (event_peer != nullptr)
        outfile << "\t" << event_peer->get_procname();
    else
        outfile << "\tN/A";

    if (event_prev != nullptr)
        outfile << "\t" << std::fixed << std::dec << int(log10(get_abstime() - prev));
    else
        outfile << "\tN/A";
    outfile << "\t" << get_group_id();
    outfile << std::endl;
}
