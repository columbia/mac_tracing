#include "voucher_bank_attrs.hpp"
#include "voucher.hpp"

#define DEBUG_VOUCHER 0

VoucherBankAttrs::VoucherBankAttrs(list<event_t*> &_bank_list, list<event_t*> &_voucher_list)
:bank_list(_bank_list), voucher_list(_voucher_list)
{
}

void VoucherBankAttrs::update_event_info()
{
	if (bank_list.size() > 0)
		update_bank_list();
	if (voucher_list.size() > 0)
		update_voucher_list();
}

string VoucherBankAttrs::pid2comm(pid_t pid)
{
	string proc_comm = "unknown";
	map<uint64_t, pair<pid_t, string> >::iterator it;
	for (it = LoadData::tpc_maps.begin(); it !=  LoadData::tpc_maps.end(); it++) {
		if ((it->second).first == pid) {
			return (it->second).second;
		}
	}
#if DEBUG_VOUCHER
	mtx.lock();
	cerr << "No comm found for pid " << hex << pid << endl;
	mtx.unlock();
#endif
	return proc_comm;
}

void VoucherBankAttrs::update_voucher_list()
{
	list<event_t *>::iterator it;
	voucher_ev_t * voucher_info;

	for (it = voucher_list.begin(); it != voucher_list.end(); it++) {
		voucher_info = dynamic_cast<voucher_ev_t *>(*it);

		pid_t bank_holder = voucher_info->get_bank_holder();
		if (bank_holder != -1)
			voucher_info->set_bank_holder_name(pid2comm(bank_holder));

		pid_t bank_merchant = voucher_info->get_bank_merchant();
		if (bank_merchant != -1) 
			voucher_info->set_bank_merchant_name(pid2comm(bank_merchant));

		pid_t bank_orig = voucher_info->get_bank_orig();
		if (bank_orig != -1)
			voucher_info->set_bank_orig_name(pid2comm(bank_orig));

		pid_t bank_prox = voucher_info->get_bank_prox();
		if (bank_prox != -1)
			voucher_info->set_bank_prox_name(pid2comm(bank_prox));
	}
}

void VoucherBankAttrs::update_bank_list()
{
	list<event_t*>::iterator it;
	bank_ev_t * bank_event;

	for (it = bank_list.begin(); it != bank_list.end(); it++) {
		bank_event = dynamic_cast<bank_ev_t*>(*it);

		pid_t bank_holder = bank_event->get_bank_holder();
		if (bank_holder != -1)
			bank_event->set_bank_holder_name(pid2comm(bank_holder));

		pid_t bank_merchant = bank_event->get_bank_merchant();
		if (bank_merchant != -1)
			bank_event->set_bank_merchant_name(pid2comm(bank_merchant));
	}
}
