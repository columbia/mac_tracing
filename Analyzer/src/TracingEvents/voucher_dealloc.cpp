#include "voucher.hpp"
VoucherDeallocEvent::VoucherDeallocEvent(double timestamp, string op, uint64_t tid, uint64_t _voucher, uint32_t coreid, string procname)
:EventBase(timestamp, op, tid, coreid, procname)
{
	voucher = _voucher;
}

void VoucherDeallocEvent::decode_event(bool is_verbose, ofstream &outfile)
{
	outfile << "\n*****" << endl;
    outfile << "\n group_id = " << std::right << hex << get_group_id();
	outfile << "\n\t" << get_procname() << "(" << hex << get_tid() << ")" << get_coreid();
	outfile << "\n\t" << fixed << setprecision(2) << get_abstime();
	outfile << "\n\t" << get_op() << hex << voucher << endl;
	
}
void VoucherDeallocEvent::streamout_event(ofstream &outfile)
{
	outfile << std::right << hex << get_group_id() << "\t" << fixed << setprecision(1) << get_abstime();
	outfile << "\t" << get_tid() << "\t" << get_procname();
	outfile << "\t" << get_op() << "\t" << hex << voucher << endl;
}
