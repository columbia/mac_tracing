#ifndef VOUCHER_BANK_ATTRS_HPP
#define VOUCHER_BANK_ATTRS_HPP
#include "voucher.hpp"
class VoucherBankAttrs {
	list<event_t *> &bank_list;
	list<event_t *> &voucher_list;
	map<pid_t, string> &pid_comm;
public:
	VoucherBankAttrs(list<event_t *> &_bank_list, list<event_t*> & voucher_list, map<pid_t, string> &pid_comm);
	void update_event_info(void);
	void update_voucher_list(void);
	void update_bank_list(void);
};
typedef VoucherBankAttrs voucher_bank_attrs_t;
#endif
