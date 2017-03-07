#include "voucher.hpp"
#include "mach_msg.hpp"

VoucherEvent::VoucherEvent(double timestamp, string op, uint64_t tid, uint64_t kmsg_addr, uint64_t _voucher_addr, uint64_t _bank_attr_type, uint32_t coreid, string procname)
:EventBase(timestamp, op, tid, coreid, procname)
{
	if (_bank_attr_type == BANK_TASK) {
		bank_attr_type = (uint32_t)_bank_attr_type;
		bank_orig = bank_prox = -1;
	} else {
		bank_orig = pid_t(_bank_attr_type >> 32);
		bank_orig = bank_orig == 0 ? -1 : bank_orig;
		bank_prox = pid_t(_bank_attr_type);
		bank_prox = bank_prox == 0 ? -1 : bank_prox;
		bank_attr_type = BANK_ACCT;
	}
	bank_holder = bank_merchant = -1;

	carrier = NULL;
	carrier_addr = kmsg_addr;
	voucher_addr = _voucher_addr;

	bank_holder_name = "-";
	bank_merchant_name = "-";
	bank_orig_name = "-";
	bank_prox_name = "-";
}

bool VoucherEvent::hook_msg(msg_ev_t * msg_event)
{
	if (msg_event->get_header()->get_carried_vport() == voucher_addr
		&& msg_event->get_tag() == carrier_addr) {
		carrier = msg_event;
		msg_event->set_voucher(this);
		return true;
	}
	return false;
}

void VoucherEvent::decode_event(bool is_verbose, ofstream &outfile)
{
	outfile << "\n*****" << endl;
    outfile << "\n group_id = " << std::right << hex << get_group_id();
	outfile << "\n\t" << get_procname() << "(" << hex << get_tid() << ")" << get_coreid();
	outfile << "\n\t" << fixed << setprecision(2) << get_abstime();
	outfile << "\n\t" << get_op() << endl;

	outfile << "\t";
	if (bank_attr_type == BANK_TASK)
		outfile << "bank_context";
	else if (bank_attr_type == BANK_ACCT)
		outfile << "bank_account";
	else
		outfile << "bank_unknown";

	if (bank_holder != -1) 
		outfile << "\n\tbank_holder : " << bank_holder_name;
	if (bank_merchant != -1)
		outfile << "\n\tbank_merchant : " << bank_merchant_name;
	if (bank_orig != -1)
		outfile << "\n\tbank_originator : " << bank_orig_name;
	if (bank_prox != -1)
		outfile << "\n\tbank_proximate : " << bank_prox_name;

	outfile << endl;
}

void VoucherEvent::streamout_event(ofstream &outfile)
{
	outfile << std::right << hex << get_group_id() << "\t" << fixed << setprecision(1) << get_abstime();
	outfile << "\t" << get_tid() << "\t" << get_procname();
	/*
	if (bank_attr_type == BANK_TASK)
		outfile << "\tbank_context(";
	else if (bank_attr_type == BANK_ACCT)
		outfile << "\tbank_account(";
	*/
	outfile << "\t" <<  get_op();
	outfile << "\t(voucher_" << hex << voucher_addr << "_msg_" << hex << carrier_addr << ")";

	if (bank_holder != -1) 
		outfile << "\t" << bank_holder_name;
	if (bank_merchant != -1)
		outfile << "\t" << bank_merchant_name;
	if (bank_orig != -1)
		outfile << "\t" << bank_orig_name;
	if (bank_prox != -1)
		outfile << "\t" << bank_prox_name;
	outfile << endl;
}
