#include "voucher.hpp"
VoucherTransitEvent::VoucherTransitEvent(double timestamp, string op, uint64_t tid, uint64_t _voucher_dst, uint64_t _voucher_src, uint32_t coreid, string procname)
:EventBase(timestamp, VOUCHER_TRANS_EVENT, op, tid, coreid, procname)
{
	voucher_dst = _voucher_dst;
	if (op.find("reuse") != string::npos) {
		voucher_src = _voucher_src;
		type = REUSE;
	} else {
		voucher_src = 0;
		type = CREATE;
	}
}

void VoucherTransitEvent::decode_event(bool is_verbose, ofstream &outfile)
{
	EventBase::decode_event(is_verbose, outfile);
	outfile << "\tvoucher dst " << hex << voucher_dst << endl;
}

void VoucherTransitEvent::streamout_event(ofstream &outfile)
{
	EventBase::streamout_event(outfile);
	outfile << "\tvoucher dst " << hex << voucher_dst << endl;
}
