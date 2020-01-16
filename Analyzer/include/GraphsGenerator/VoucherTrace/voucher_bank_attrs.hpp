#ifndef VOUCHER_BANK_ATTRS_HPP
#define VOUCHER_BANK_ATTRS_HPP
#include "voucher.hpp"
#include "loader.hpp"
class VoucherBankAttrs {
    std::list<EventBase *> &bank_list;
    std::list<EventBase *> &voucher_list;
public:
    VoucherBankAttrs(std::list<EventBase *> &_bank_list, std::list<EventBase*> & voucher_list);
    std::string pid2comm(pid_t);
    void update_event_info(void);
    void update_voucher_list(void);
    void update_bank_list(void);
};
typedef VoucherBankAttrs VoucherBankAttrs;
#endif
