#include "voucher.hpp"
VoucherDeallocEvent::VoucherDeallocEvent(double timestamp, string op, uint64_t tid, uint64_t _voucher, uint32_t coreid, string procname)
:EventBase(timestamp, VOUCHER_DEALLOC_EVENT, op, tid, coreid, procname)
{
	voucher = _voucher;
}

void VoucherDeallocEvent::decode_event(bool is_verbose, ofstream &outfile)
{
	EventBase::decode_event(is_verbose, outfile);
	outfile << "\n\tvoucher " << hex << voucher << endl;
}

void VoucherDeallocEvent::streamout_event(ofstream &outfile)
{
	EventBase::streamout_event(outfile);
	outfile << "\t" << hex << voucher << endl;
}
