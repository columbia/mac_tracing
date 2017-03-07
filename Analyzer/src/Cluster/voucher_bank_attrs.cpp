#include "voucher_bank_attrs.hpp"
#include "voucher.hpp"

VoucherBankAttrs::VoucherBankAttrs(list<event_t*> &_bank_list, list<event_t*> &_voucher_list, map<pid_t, string> &_pid_comm)
:bank_list(_bank_list), voucher_list(_voucher_list), pid_comm(_pid_comm)
{
}

void VoucherBankAttrs::update_event_info()
{
	if (bank_list.size() > 0)
		update_bank_list();
	if (voucher_list.size() > 0)
		update_voucher_list();
}

//TODO : checking unknown proc name
void VoucherBankAttrs::update_voucher_list()
{
	list<event_t *>::iterator it;
	voucher_ev_t * voucher_info;
	string unknown("unknown");
	for (it = voucher_list.begin(); it != voucher_list.end(); it++) {
		voucher_info = dynamic_cast<voucher_ev_t *>(*it);
		pid_t bank_holder = voucher_info->get_bank_holder();
		if (bank_holder != -1) {
			if (pid_comm.find(bank_holder) != pid_comm.end())
				voucher_info->set_bank_holder_name(pid_comm[bank_holder]);
			else
				voucher_info->set_bank_holder_name(unknown);
		}
		pid_t bank_merchant = voucher_info->get_bank_merchant();
		if (bank_merchant != -1) {
			if (pid_comm.find(bank_merchant) != pid_comm.end())
				voucher_info->set_bank_merchant_name(pid_comm[bank_merchant]);
			else
				voucher_info->set_bank_merchant_name(unknown);
		}

		pid_t bank_orig = voucher_info->get_bank_orig();
		if (bank_orig != -1) {
			if (pid_comm.find(bank_orig) != pid_comm.end())
				voucher_info->set_bank_orig_name(pid_comm[bank_orig]);
			else
				voucher_info->set_bank_orig_name(unknown);
		}
		pid_t bank_prox = voucher_info->get_bank_prox();
		if (bank_prox!= -1) {
			if (pid_comm.find(bank_prox) != pid_comm.end())
				voucher_info->set_bank_prox_name(pid_comm[bank_prox]);
			else
				voucher_info->set_bank_prox_name(unknown);
		}
	}
}

void VoucherBankAttrs::update_bank_list()
{
	list<event_t*>::iterator it;
	bank_ev_t * bank_event;
	string unknown("unknown");
	for (it = bank_list.begin(); it != bank_list.end(); it++) {
		bank_event = dynamic_cast<bank_ev_t*>(*it);
		pid_t bank_holder = bank_event->get_bank_holder();
		if (bank_holder != -1) {
			if (pid_comm.find(bank_holder) != pid_comm.end())
				bank_event->set_bank_holder_name(pid_comm[bank_holder]);
			else
				bank_event->set_bank_holder_name(unknown);
		}
		pid_t bank_merchant = bank_event->get_bank_merchant();
		if (bank_merchant != -1) {
			if (pid_comm.find(bank_merchant) != pid_comm.end())
				bank_event->set_bank_merchant_name(pid_comm[bank_merchant]);
			else
				bank_event->set_bank_merchant_name(unknown);
		}
	}
}
