#include "mach_msg.hpp"

MsgEvent::MsgEvent(double _timestamp, string _op, tid_t _tid, msgh_t * _header, uint32_t _core_id, string _procname)
: EventBase(_timestamp, _op, _tid, _core_id, _procname)
{
	header = _header;
	peer = next = NULL;
	bt = NULL;
	voucher = NULL;
	free_before_deliver = false;
}

MsgEvent::~MsgEvent()
{
	if (header != NULL)
		delete(header);
	header = NULL;
}

void MsgEvent::decode_event(bool is_verbose, ofstream &outfile)
{
	mach_msg_bits_t other = MACH_MSGH_BITS_OTHER(header->get_msgh_bits()) & MACH_MSGH_BITS_USED;
	outfile << "\n=====";
	outfile << "\n group_id = " << std::right << hex << get_group_id();
	outfile << "\n [" << dec << get_pid() <<"] " << get_procname() << "(" << hex << get_tid() << ")" << get_coreid();
	outfile << "\n\t" << fixed << setprecision(1) << get_abstime();
	outfile << "\n\t" << get_op() << "_" << hex << tag;

	header->decode_header(outfile);
	if (peer)
		outfile << "\n\tpeer " << hex << peer->get_tid() << "\t" << fixed << setprecision(1) << peer->get_abstime();
	if (next)
		outfile << "\n\tnext " << hex << next->get_tid() << "\t" <<  fixed << setprecision(1) << next->get_abstime();

	if (is_freed_before_deliver())
		outfile << "\n\tfreed before deliver";
	
	if (bt)
		outfile  << "\n\tbacktrace at: " << fixed << setprecision(2) << bt->get_abstime() << endl;
	if (voucher)
		outfile << "\n\tvoucher at: " << fixed << setprecision(2) << voucher->get_abstime() << endl;
	outfile << endl;
}

void MsgEvent::streamout_event(ofstream & outfile)
{
	outfile << std::right << hex << get_group_id() << "\t" << fixed << setprecision(1) << get_abstime();
	outfile << "\t" << hex << get_tid() << "\t" << get_procname();
	outfile << "\t" << get_op() << "_" << hex << tag << "_";

	if (header->is_mig()) {
		assert(LoadData::mig_dictionary.find(header->get_msgh_id()) != 
		LoadData::mig_dictionary.end());
		outfile << LoadData::mig_dictionary[header->get_msgh_id()].c_str();
	}
	else
		outfile << (uint32_t)(header->get_remote_port());

	if (peer)
		outfile << "\tpeer " << hex << peer->get_tid() << "\t" << fixed << setprecision(1) << peer->get_abstime();

	if (is_freed_before_deliver())
		outfile << "\tfreed before deliver";

	outfile << endl;
}
