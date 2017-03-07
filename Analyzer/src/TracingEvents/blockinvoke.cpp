#include "dispatch.hpp"

BlockinvokeEvent::BlockinvokeEvent(double abstime, string op, uint64_t _tid, uint64_t _func, uint64_t _ctxt, bool _not_begin, uint32_t _core, string procname)
	: EventBase(abstime, op, _tid, _core, procname)
{
	func = _func;
	ctxt = _ctxt;
	begin = (!(_not_begin));
	rooted = false;
	root = NULL;
}

void BlockinvokeEvent::decode_event(bool is_verbose, ofstream &outfile)
{
	outfile << "\n*****" << endl;
    outfile << "\n group_id = " << std::right << hex << get_group_id();
	outfile << "\n [" << dec << get_pid() <<"] " << get_procname() << "(" << hex << get_tid() << ")" << get_coreid();
	outfile << "\n\t" << fixed << setprecision(2) << get_abstime();
	outfile << "\n\t" << get_op();
	if (begin)
		outfile << "\n\t Begin_func_" << hex << func;
	else
		outfile << "\n\t End_func_" << hex << func;

	outfile << "\n\tctxt_" << hex << ctxt;

	if (rooted == true) {
		if (begin)
			outfile << "\n\tdequeue by " << hex << root->get_tid() << " at " << fixed << setprecision(1) << root->get_abstime() << endl;
		else
			outfile  << "\n\tbegin by " << hex << root->get_tid() << " at " << fixed << setprecision(1) << root->get_abstime() << endl;
	}

	if (desc.size())
		outfile <<"\n\tdesc " << desc << endl;

	outfile << "\n\tnested " << nested_level;
	outfile << endl;
}

void BlockinvokeEvent::streamout_event(ofstream &outfile)
{
	outfile << std::right << hex << get_group_id() << "\t" << fixed << setprecision(1) << get_abstime();
	outfile << "\t" << hex << get_tid() << "\t" << get_procname();
	outfile << "\t" << get_op() << "_func" << hex << func;
	if (begin)
		outfile << "_ctxt" << hex << ctxt << "_Begin";
	else 
		outfile << "_End";

	if (desc.size())
		outfile <<"\tdesc " << desc;
	
	if (rooted == true) {
		outfile <<"\n(";
		if (begin)
			outfile << "dequeue by " << hex << root->get_tid() << " at " << fixed << setprecision(1) << root->get_abstime();
		else
			outfile  << "begin by " << hex << root->get_tid() << " at " << fixed << setprecision(1) << root->get_abstime();
		outfile << ")";
	}

	outfile << endl;
}
