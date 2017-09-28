#include "voucher.hpp"
VoucherConnEvent::VoucherConnEvent(double timestamp, string op, uint64_t tid, uint64_t _voucher_ori, uint64_t _voucher_new, uint32_t coreid, string procname)
:EventBase(timestamp, VOUCHER_CONN_EVENT, op, tid, coreid, procname)
{
	voucher_ori = _voucher_ori;
	voucher_new = _voucher_new;
}

void VoucherConnEvent::decode_event(bool is_verbose, ofstream &outfile)
{
	EventBase::decode_event(is_verbose, outfile);
	outfile << "\n\tfrom_" << hex << voucher_ori;
	outfile << "\tto_" << hex << voucher_new << endl; 
	
}
void VoucherConnEvent::streamout_event(ofstream &outfile)
{
	EventBase::streamout_event(outfile);
	outfile << "\tfrom_" << hex << voucher_ori << "_to_" << hex << voucher_new << endl; 
}
