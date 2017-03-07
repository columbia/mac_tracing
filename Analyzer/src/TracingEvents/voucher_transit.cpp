#include "voucher.hpp"
VoucherTransitEvent::VoucherTransitEvent(double timestamp, string op, uint64_t tid, uint64_t _voucher_dst, uint64_t _voucher_src, uint32_t coreid, string procname)
:EventBase(timestamp, op, tid, coreid, procname)
{
	voucher_dst = _voucher_dst;
	if (op.find("reuse") != string::npos) {
		voucher_src = _voucher_src;
		type = REUSE;
	}
	else {
		voucher_src = 0;
		type = CREATE;
	}
}

void VoucherTransitEvent::decode_event(bool is_verbose, ofstream &outfile)
{
	outfile << "\n*****" << endl;
    outfile << "\n group_id = " << std::right << hex << get_group_id();
	outfile << "\n\t" << get_procname() << "(" << hex << get_tid() << ")" << get_coreid();
	outfile << "\n\t" << fixed << setprecision(2) << get_abstime();
	outfile << "\n\t" << get_op() << endl;
	outfile << hex << voucher_dst << endl;
}

void VoucherTransitEvent::streamout_event(ofstream &outfile)
{
	outfile << std::right << hex << get_group_id() << "\t" << fixed << setprecision(1) << get_abstime();
	outfile << "\t" << get_tid() << "\t" << get_procname();
	outfile << "\t" << get_op() << "\t" << hex << voucher_dst << endl;
}
