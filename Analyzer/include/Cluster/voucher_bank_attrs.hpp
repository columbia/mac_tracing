#ifndef VOUCHER_BANK_ATTRS_HPP
#define VOUCHER_BANK_ATTRS_HPP
#include "voucher.hpp"
#include "loader.hpp"
class VoucherBankAttrs {
	list<event_t *> &bank_list;
	list<event_t *> &voucher_list;
public:
	VoucherBankAttrs(list<event_t *> &_bank_list, list<event_t*> & voucher_list);
	string pid2comm(pid_t);
	void update_event_info(void);
	void update_voucher_list(void);
	void update_bank_list(void);
};
typedef VoucherBankAttrs voucher_bank_attrs_t;
#endif
