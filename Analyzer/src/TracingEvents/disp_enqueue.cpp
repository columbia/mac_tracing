#include "dispatch.hpp"
BlockEnqueueEvent::BlockEnqueueEvent(double abstime, std::string op, uint64_t _tid, uint64_t _q_id, uint64_t _item, uint32_t _ref, uint32_t _coreid, std::string procname)
:EventBase(abstime, DISP_ENQ_EVENT, op, _tid, _coreid, procname),
BlockInfo(_q_id, _item, _ref)
{
    consumed = false;
}

void BlockEnqueueEvent::decode_event(bool is_verbose, std::ofstream &outfile)
{
    EventBase::decode_event(is_verbose, outfile);
    outfile << "\n\tref " << std::hex << ref;
    outfile << "\n\tqid " << std::hex << q_id;
    outfile << "\titem " << std::hex << item;
    outfile << "\n\tnested " << nested_level;
    outfile << std::endl;
}

void BlockEnqueueEvent::streamout_event(std::ofstream &outfile)
{
    EventBase::streamout_event(outfile);

    outfile << "_ref" << std::hex << ref;
    outfile << "_qid" << std::hex << q_id;
    outfile << "_item" << std::hex << item;
    EventBase *consumer = get_event_peer();
    if (consumed)
        outfile << "\nDequeued by " << std::hex << consumer->get_tid() << " at " << std::fixed << std::setprecision(1) << consumer->get_abstime();

    outfile << std::endl;
}
