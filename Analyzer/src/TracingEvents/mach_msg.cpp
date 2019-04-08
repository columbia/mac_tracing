#include "mach_msg.hpp"

MsgEvent::MsgEvent(double _timestamp, string _op, tid_t _tid, msgh_t * _header, uint32_t _core_id, string _procname)
: EventBase(_timestamp, MSG_EVENT, _op, _tid, _core_id, _procname)
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
	EventBase::decode_event(is_verbose, outfile);
	mach_msg_bits_t other = MACH_MSGH_BITS_OTHER(header->get_msgh_bits()) & MACH_MSGH_BITS_USED;
	outfile << "\t" << hex << tag;
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

	outfile << "\n\tmsgh_id: " << hex << header->get_msgh_id() << endl;
	outfile << endl;
}

void MsgEvent::streamout_event(ofstream &outfile)
{
	EventBase::streamout_event(outfile);
	outfile << "\t" << hex << tag;
	if (header->is_mig()) {
		assert(LoadData::mig_dictionary.find(header->get_msgh_id()) != 
				LoadData::mig_dictionary.end());
		outfile << "\tmig " <<  LoadData::mig_dictionary[header->get_msgh_id()].c_str();
	}
	else
		outfile <<"\tremote_port " << (uint32_t)(header->get_remote_port());
	
	outfile << "\tuser_addr " << hex << user_addr;

	if (peer)
		outfile << "\tpeer " << hex << peer->get_tid() << "\t" << fixed << setprecision(1) << peer->get_abstime() << "\t" << LoadData::tpc_maps[peer->get_tid()].second;

	if (is_freed_before_deliver())
		outfile << "\tfreed before deliver";

	outfile << "\tmsgh_id " << dec << header->get_msgh_id();
	outfile << endl;
}
