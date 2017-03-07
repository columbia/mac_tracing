#include "dispatch.hpp"
EnqueueEvent::EnqueueEvent(double abstime, string op, uint64_t _tid, uint64_t _q_id, uint64_t _item, uint32_t _ref, uint32_t _coreid, string procname)
	: EventBase(abstime, op, _tid, _coreid, procname)
{
	q_id = _q_id;
	item = _item;
	ref = _ref;
	consumed = false;
	consumer = NULL;
}

void EnqueueEvent::decode_event(bool is_verbose, ofstream &outfile)
{
	outfile << "\n*****" << endl;
    outfile << "\n group_id = " << std::right << hex << get_group_id();
	outfile << "\n [" << dec << get_pid() <<"] " << get_procname() << "(" << hex << get_tid() << ")" << get_coreid();
	outfile << "\n\t" << fixed << setprecision(2) << get_abstime();
	outfile << "\n\t" << get_op() << "_" << hex << ref;
	outfile << "\tqid " << hex << q_id << "\titem " << hex << item;
	outfile << "\tnested " << nested_level;
	outfile << endl;
}

void EnqueueEvent::streamout_event(ofstream &outfile)
{
	outfile << std::right << hex << get_group_id() << "\t" << fixed << setprecision(1) << get_abstime();
	outfile << "\t" << get_tid() << "\t" << get_procname();
	outfile << "\t" << get_op() << "_" << hex << ref << "_qid" << hex << q_id;
	outfile << "_item" << hex << item;

	if (consumed == true) {
		outfile << "\n(";
		outfile << "deq by" << hex << consumer->get_tid() << " at " << fixed << setprecision(1) << consumer->get_abstime();
		outfile << ")";
	}

	outfile << endl;
}
