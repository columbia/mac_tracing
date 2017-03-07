#include "voucher.hpp"
VoucherConnEvent::VoucherConnEvent(double timestamp, string op, uint64_t tid, uint64_t _voucher_ori, uint64_t _voucher_new, uint32_t coreid, string procname)
:EventBase(timestamp, op, tid, coreid, procname)
{
	voucher_ori = _voucher_ori;
	voucher_new = _voucher_new;
}

void VoucherConnEvent::decode_event(bool is_verbose, ofstream &outfile)
{
	outfile << "\n*****" << endl;
    outfile << "\n group_id = " << std::right << hex << get_group_id();
	outfile << "\n\t" << get_procname() << "(" << hex << get_tid() << ")" << get_coreid();
	outfile << "\n\t" << fixed << setprecision(2) << get_abstime();
	outfile << "\n\t" << get_op() << endl;
	outfile << "\tfrom_" << hex << voucher_ori << "\tto_" << hex << voucher_new << endl; 
	
}
void VoucherConnEvent::streamout_event(ofstream &outfile)
{
	outfile << std::right << hex << get_group_id() << "\t" << fixed << setprecision(1) << get_abstime();
	outfile << "\t" << get_tid() << "\t" << get_procname();
	outfile << "\t" << get_op() << hex << voucher_ori << "_to_" << hex << voucher_new << endl; 
}
