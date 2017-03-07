#include "dispatch.hpp"
DequeueEvent::DequeueEvent(double abstime, string op, uint64_t _tid, uint64_t _q_id, uint64_t _item, uint64_t _ctxt, uint32_t _ref, uint32_t _coreid, string procname)
	: EventBase(abstime, op, _tid, _coreid, procname)
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
	outfile << "\n*****" << endl;
    outfile << "\n group_id = " << std::right << hex << get_group_id();
	outfile << "\n [" << dec << get_pid() <<"] " << get_procname() << "(" << hex << get_tid() << ")" << get_coreid();
	outfile << "\n\t" << fixed << setprecision(2) << get_abstime();
	outfile << "\t" << get_op() << "_" << hex << ref;
	outfile << "\n\t_qid " << hex << q_id;
	outfile << "\n\t_item " << hex << item;
	outfile << "\n\t_vtable " << hex << vtable_ptr;
	outfile << "\n\t_invoke " << hex << invoke_ptr;
	outfile << "\n\t_func " << hex << func_ptr;
	if (desc.size())
		outfile << "\n\t_desc" << desc;

	if (root != NULL) 
		outfile << "\n\tenqueue by " << hex << root->get_tid() << " at " << fixed << setprecision(1) << root->get_abstime() << endl;
	if (invoke != NULL)
		outfile << "\n\tinvoked by" << hex << invoke->get_tid() << " at " << fixed << setprecision(1) << invoke->get_abstime() << endl;

	outfile << "\n\tnested " << nested_level;
	outfile << endl;
}

void DequeueEvent::streamout_event(ofstream &outfile)
{
	outfile << std::right << hex << get_group_id() << "\t" << fixed << setprecision(1) << get_abstime();
	outfile << "\t" << get_tid() << "\t" << get_procname();
	outfile << "\t" << get_op() << "_" << hex << ref <<  "_qid" << hex << q_id;
	outfile << "_item" << hex << item;
	if (func_ptr != 0)
		outfile << "_func" << hex << func_ptr;
	if (invoke_ptr != 0)
		outfile << "_invoke" << hex << invoke_ptr;
	if (desc.size())
		outfile << "_desc" << desc;

	if (root != NULL) 
		outfile << "\n(enqueue by " << hex << root->get_tid() << " at " << fixed << setprecision(1) << root->get_abstime();
	if (invoke != NULL) {
		if (root)
			outfile << "\t";
		outfile << "invoked by" << hex << invoke->get_tid() << " at " << fixed << setprecision(1) << invoke->get_abstime();
	}

	if (root || invoke)
		outfile << ")";

	outfile << endl;
}
