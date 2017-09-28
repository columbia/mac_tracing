#ifndef VOUCHER_HPP
#define VOUCHER_HPP
#include "base.hpp"
//#include "mach_msg.hpp"

using namespace std;

#define BANK_TASK	0
#define BANK_ACCT	1

class BankEvent: public EventBase {
	pid_t bank_holder;
	pid_t bank_merchant;
	string bank_holder_name;
	string bank_merchant_name;
public:
	BankEvent(double timestamp, string op, uint64_t tid, uint64_t merchant, uint64_t holder, uint32_t coreid, string procname = "");
	pid_t get_bank_holder(void) { return bank_holder;}
	void set_bank_holder_name(string name) { bank_holder_name = name;}
	pid_t get_bank_merchant(void) { return bank_merchant;}
	void set_bank_merchant_name(string name) {bank_merchant_name = name;}
	void decode_event(bool is_verbose, ofstream &outfile);
	void streamout_event(ofstream &outfile);
};

class VoucherEvent: public EventBase {
	uint32_t bank_attr_type;
	msg_ev_t* carrier;
	uint64_t carrier_addr;
	uint64_t voucher_addr;
	pid_t bank_holder;
	pid_t bank_merchant;
	pid_t bank_orig;
	pid_t bank_prox;
	string bank_holder_name;
	string bank_merchant_name;
	string bank_orig_name;
	string bank_prox_name;
public:
	VoucherEvent(double timestamp, string op, uint64_t tid, uint64_t kmsg_addr, uint64_t _voucher_addr, uint64_t _bank_attr_type, uint32_t coreid, string procname ="");
	void set_carrier(msg_ev_t* cur_msg) {carrier = cur_msg;}
	msg_ev_t * get_carrier(void) {return carrier;}
	uint64_t get_carrier_addr(void) {return carrier_addr;}
	uint64_t get_voucher_addr(void) {return voucher_addr;}
	void set_bank_holder(pid_t pid) {bank_holder = pid;}
	pid_t get_bank_holder(void) { return bank_holder;}
	void set_bank_holder_name(string name) {bank_holder_name = name;}
	void set_bank_merchant(pid_t pid) {bank_merchant = pid;}
	pid_t get_bank_merchant(void) { return bank_merchant;}
	void set_bank_merchant_name(string name) {bank_merchant_name = name;}
	pid_t get_bank_orig(void) {return bank_orig;}
	void set_bank_orig_name(string name) {bank_orig_name = name;}
	pid_t get_bank_prox(void) {return bank_prox;}
	void set_bank_prox_name(string name) {bank_prox_name = name;}
	bool hook_msg(msg_ev_t * msg_event);
	void decode_event(bool is_verbose, ofstream &outfile);
	void streamout_event(ofstream &outfile);
};

class VoucherConnEvent: public EventBase {
	uint64_t voucher_ori;
	uint64_t voucher_new;
public:
	VoucherConnEvent(double timestamp, string op, uint64_t tid, uint64_t voucher_ori, uint64_t voucher_new, uint32_t coreid, string procname = "");
	uint64_t get_voucher_ori(void) {return voucher_ori;}
	uint64_t get_voucher_new(void) {return voucher_new;}
	void decode_event(bool is_verbose, ofstream &outfile);
	void streamout_event(ofstream &outfile);
};

#define CREATE	0
#define REUSE	1
class VoucherTransitEvent: public EventBase {
	uint64_t voucher_dst;
	uint64_t voucher_src;
	int type;
public:
	VoucherTransitEvent(double timestamp, string op, uint64_t tid, uint64_t voucher_dst, uint64_t voucher_src, uint32_t coreid, string procname = "");
	uint64_t get_voucher_dst(void) {return voucher_dst;}
	uint64_t get_voucher_src(void) {return voucher_src;}
	int get_type(void) {return type;}
	void decode_event(bool is_verbose, ofstream &outfile);
	void streamout_event(ofstream &outfile);
};

class VoucherDeallocEvent: public EventBase {
	uint64_t voucher;
public:
	VoucherDeallocEvent(double timestamp, string op, uint64_t tid, uint64_t voucher, uint32_t coreid, string procname = "");
	uint64_t get_voucher(void) {return voucher;}
	void decode_event(bool is_verbose, ofstream &outfile);
	void streamout_event(ofstream &outfile);
};

#endif
