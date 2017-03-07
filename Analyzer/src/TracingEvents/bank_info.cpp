#include "voucher.hpp"

BankEvent::BankEvent(double timestamp, string op, uint64_t tid, uint64_t merchant, uint64_t holder, uint32_t coreid, string procname)
:EventBase(timestamp, op, tid, coreid, procname)
{
	bank_holder = holder;
	bank_merchant = merchant;
	bank_holder_name = "-";
	bank_merchant_name = "-";
}

void BankEvent::decode_event(bool is_verbose, ofstream &outfile)
{
	outfile << "\n*****" << endl;
    outfile << "\n group_id = " << std::right << hex << get_group_id();
	outfile << "\n\t" << get_procname() << "(" << hex << get_tid() << ")" << get_coreid();
	outfile << "\n\t" << fixed << setprecision(2) << get_abstime();
	outfile << "\n\t" << get_op() << endl;

	if (bank_holder != -1) 
		outfile << "\t" << bank_holder_name;
	if (bank_merchant != -1)
		outfile << "\t" << bank_merchant_name;
	outfile << endl;
}

void BankEvent::streamout_event(ofstream &outfile)
{
	outfile << std::right << hex << get_group_id() << "\t" << fixed << setprecision(1) << get_abstime();
	outfile << "\t" << get_tid() << "\t" << get_procname();
	outfile << "\t voucher_" << get_op();
	if (bank_holder != -1) 
		outfile << "\t" << bank_holder_name;
	if (bank_merchant != -1)
		outfile << "\t" << bank_merchant_name;
	outfile << endl;
}
