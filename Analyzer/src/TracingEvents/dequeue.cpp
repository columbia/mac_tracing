#include "dispatch.hpp"
DequeueEvent::DequeueEvent(double abstime, string op, uint64_t _tid, uint64_t _q_id, uint64_t _item, uint64_t _ctxt, uint32_t _ref, uint32_t _coreid, string procname)
:EventBase(abstime, DISP_DEQ_EVENT, op, _tid, _coreid, procname)
{
	q_id = _q_id;
	item = _item;
	ref = _ref;
	ctxt = _ctxt;
	duplicate = false;
	executed = false;
	root = NULL;
	invoke = NULL;
}

void DequeueEvent::set_ptrs(uint64_t _func_ptr, uint64_t _invoke_ptr, uint64_t _vtable_ptr)
{
	func_ptr = _func_ptr;
	invoke_ptr = _invoke_ptr;
	vtable_ptr = _vtable_ptr;
}

void DequeueEvent::decode_event(bool is_verbose, ofstream &outfile)
{
	EventBase::decode_event(is_verbose, outfile);
	outfile << "\n\tref " << hex << ref;
	outfile << "\n\t_qid " << hex << q_id;
	outfile << "\n\t_item " << hex << item;
	outfile << "\n\t_vtable " << hex << vtable_ptr;
	outfile << "\n\t_invoke " << hex << invoke_ptr;
	outfile << "\n\t_func " << hex << func_ptr;

	if (desc.size())
		outfile << "\n\t_desc" << desc;

	if (root) 
		outfile << "\n\tenqueue by " << hex << root->get_tid() << " at " << fixed << setprecision(1) << root->get_abstime() << endl;
	if (invoke)
		outfile << "\n\tinvoked by" << hex << invoke->get_tid() << " at " << fixed << setprecision(1) << invoke->get_abstime() << endl;

	outfile << "\n\tnested " << nested_level;
	outfile << endl;
}

void DequeueEvent::streamout_event(ofstream &outfile)
{
	EventBase::streamout_event(outfile);
	outfile << "_ref" << hex << ref;
	outfile << "_qid" << hex << q_id;
	outfile << "_item" << hex << item;

	if (func_ptr)
		outfile << "_func" << hex << func_ptr;
	if (invoke_ptr)
		outfile << "_invoke" << hex << invoke_ptr;
	if (desc.size())
		outfile << "_desc" << desc;

	if (root) 
		outfile << "\n\tEnqueued by " << hex << root->get_tid() << " at " << fixed << setprecision(1) << root->get_abstime() << endl;;

	if (invoke)
		outfile << "\n\tInvoked by" << hex << invoke->get_tid() << " at " << fixed << setprecision(1) << invoke->get_abstime();

	outfile << endl;
}
