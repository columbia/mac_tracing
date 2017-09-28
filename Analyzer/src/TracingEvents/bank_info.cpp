#include "voucher.hpp"

BankEvent::BankEvent(double timestamp, string op, uint64_t tid, uint64_t merchant, uint64_t holder, uint32_t coreid, string procname)
:EventBase(timestamp, VOUCHER_EVENT, op, tid, coreid, procname)
{
	bank_holder = holder;
	bank_merchant = merchant;
	bank_holder_name = "-";
	bank_merchant_name = "-";
}

void BankEvent::decode_event(bool is_verbose, ofstream &outfile)
{
	EventBase::decode_event(is_verbose, outfile);

	if (bank_holder != -1) 
		outfile << "\tholder " << bank_holder_name;
	if (bank_merchant != -1)
		outfile << "\tmerchant " << bank_merchant_name;
	outfile << endl;
}

void BankEvent::streamout_event(ofstream &outfile)
{
	EventBase::streamout_event(outfile);

	if (bank_holder != -1) 
		outfile << "\tholder " << bank_holder_name;
	if (bank_merchant != -1)
		outfile << "\tmerchant " << bank_merchant_name;
	outfile << endl;
}
