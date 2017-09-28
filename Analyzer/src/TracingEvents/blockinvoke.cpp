#include "dispatch.hpp"

BlockinvokeEvent::BlockinvokeEvent(double abstime, string op, uint64_t _tid, uint64_t _func, uint64_t _ctxt, bool _not_begin, uint32_t _core, string procname)
	: EventBase(abstime, DISP_INV_EVENT, op, _tid, _core, procname)
{
	func = _func;
	ctxt = _ctxt;
	begin = (!(_not_begin));
	rooted = false;
	root = NULL;
}

void BlockinvokeEvent::decode_event(bool is_verbose, ofstream &outfile)
{
	EventBase::decode_event(is_verbose, outfile);
	if (begin)
		outfile << "\n\t Begin_func_" << hex << func;
	else
		outfile << "\n\t End_func_" << hex << func;

	outfile << "\n\tctxt_" << hex << ctxt;

	if (rooted) {
		if (begin)
			outfile << "\n\tDequeue by " << hex << root->get_tid() << " at " << fixed << setprecision(1) << root->get_abstime() << endl;
		else
			outfile  << "\n\tBegin by " << hex << root->get_tid() << " at " << fixed << setprecision(1) << root->get_abstime() << endl;
	}

	if (desc.size())
		outfile <<"\n\tdesc " << desc << endl;

	outfile << "\n\tnested " << nested_level;
	outfile << endl;
}

void BlockinvokeEvent::streamout_event(ofstream &outfile)
{
	EventBase::streamout_event(outfile);
	outfile << "\t_func_" << hex << func;
	if (begin)
		outfile << "_ctxt_" << hex << ctxt << "_Begin";
	else 
		outfile << "_End";

	if (desc.size())
		outfile <<"\tdesc " << desc;
	
	if (rooted) {
		if (begin)
			outfile << "\nDequeue by " << hex << root->get_tid() << " at " << fixed << setprecision(1) << root->get_abstime();
		else
			outfile  << "\nBegin by " << hex << root->get_tid() << " at " << fixed << setprecision(1) << root->get_abstime();
	}

	outfile << endl;
}
