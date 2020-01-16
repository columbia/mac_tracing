#include "voucher_bank_attrs.hpp"
#include "voucher.hpp"
#include "group.hpp"

#define DEBUG_VOUCHER 0

VoucherBankAttrs::VoucherBankAttrs(std::list<EventBase*> &_bank_list, std::list<EventBase*> &_voucher_list)
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

std::string VoucherBankAttrs::pid2comm(pid_t pid)
{
    return LoadData::pid2comm(pid);
}

void VoucherBankAttrs::update_voucher_list()
{
    std::list<EventBase *>::iterator it;
    VoucherEvent * voucher_info;

    for (it = voucher_list.begin(); it != voucher_list.end(); it++) {
        voucher_info = dynamic_cast<VoucherEvent *>(*it);

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
    std::list<EventBase*>::iterator it;
    BankEvent * bank_event;

    for (it = bank_list.begin(); it != bank_list.end(); it++) {
        bank_event = dynamic_cast<BankEvent*>(*it);

        pid_t bank_holder = bank_event->get_bank_holder();
        if (bank_holder != -1)
            bank_event->set_bank_holder_name(pid2comm(bank_holder));

        pid_t bank_merchant = bank_event->get_bank_merchant();
        if (bank_merchant != -1)
            bank_event->set_bank_merchant_name(pid2comm(bank_merchant));
    }
}
