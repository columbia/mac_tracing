#include "dispatch.hpp"
EnqueueEvent::EnqueueEvent(double abstime, string op, uint64_t _tid, uint64_t _q_id, uint64_t _item, uint32_t _ref, uint32_t _coreid, string procname)
:EventBase(abstime, DISP_ENQ_EVENT, op, _tid, _coreid, procname)
{
	q_id = _q_id;
	item = _item;
	ref = _ref;
	consumed = false;
	consumer = NULL;
}

void EnqueueEvent::decode_event(bool is_verbose, ofstream &outfile)
{
	EventBase::decode_event(is_verbose, outfile);
	outfile << "\n\tref " << hex << ref;
	outfile << "\n\tqid " << hex << q_id;
	outfile << "\titem " << hex << item;
	outfile << "\n\tnested " << nested_level;
	outfile << endl;
}

void EnqueueEvent::streamout_event(ofstream &outfile)
{
	EventBase::streamout_event(outfile);

	outfile << "_ref" << hex << ref;
	outfile << "_qid" << hex << q_id;
	outfile << "_item" << hex << item;
	if (consumed)
		outfile << "\nDequeued by " << hex << consumer->get_tid() << " at " << fixed << setprecision(1) << consumer->get_abstime();

	outfile << endl;
}
