#ifndef BASE_EVENT_HPP
#define BASE_EVENT_HPP

#include <fstream>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <string>
#include <cassert>
#include <xpc/xpc.h>
#include <mach/message.h>
#include <queue>
#include <list>
#include <map>
#include <set>
#include <stack>
using namespace std;

#define MSG_EVENT	 0
#define MR_EVENT	 1
#define WAIT_EVENT	 2
#define INTR_EVENT	 3
#define WQNEXT_EVENT 4
#define BLOCKINVOKE  5

class EventBase;
class MsgHeader;
class MsgEvent;
class WaitEvent;
class IntrEvent;
class WqnextEvent;
class TsmaintenanceEvent;
class MkrunEvent;
class CallCreateEvent;
class CalloutEvent;
class CallCancelEvent;
class SyscallEvent;
class BacktraceEvent;
class EnqueueEvent;
class DequeueEvent;
class BlockinvokeEvent;
class VoucherEvent;
class VoucherConnEvent;
class VoucherTransitEvent;
class VoucherDeallocEvent;
class BankEvent;

typedef EventBase event_t;
typedef MsgHeader msgh_t;
typedef	MsgEvent msg_ev_t;
typedef WaitEvent wait_ev_t;
typedef IntrEvent intr_ev_t;
typedef WqnextEvent	wqnext_ev_t;
typedef TsmaintenanceEvent tsm_ev_t;
typedef MkrunEvent mkrun_ev_t;
typedef CallCreateEvent callcreate_ev_t;
typedef CalloutEvent callout_ev_t;
typedef CallCancelEvent callcancel_ev_t;
typedef SyscallEvent syscall_ev_t;
typedef BacktraceEvent backtrace_ev_t;
typedef EnqueueEvent enqueue_ev_t;
typedef DequeueEvent dequeue_ev_t;
typedef BlockinvokeEvent blockinvoke_ev_t;
typedef VoucherEvent voucher_ev_t;
typedef VoucherConnEvent voucher_conn_ev_t;
typedef VoucherTransitEvent voucher_transit_ev_t;
typedef VoucherDeallocEvent voucher_dealloc_ev_t;
typedef BankEvent bank_ev_t;

typedef uint64_t tid_t;
class EventBase {
	double timestamp;
	string op;
	tid_t tid;
	pid_t pid;
	uint32_t core_id;
	string procname;
	uint64_t group_id;
	bool complete;
	bool ground;
	bool infected;

public:
	EventBase(double _timestamp, string _op, tid_t _tid, uint32_t _core_id, string _procname= "");
	EventBase(EventBase *);
	virtual ~EventBase() {}
	void override_timestamp(double new_timestamp) {timestamp = new_timestamp;}
	double get_abstime(void) {return timestamp;}
	string get_op(void) {return op;}
	tid_t get_tid(void) {return tid;}
	pid_t get_pid(void) {return pid;}
	void set_pid(pid_t _pid) {pid = _pid;}
	void override_procname(string _procname) {procname = _procname;}
	string& get_procname(void) {return procname;}
	uint32_t get_coreid(void) {return core_id;}
	void set_group_id(uint64_t _gid) {group_id = _gid;}
	uint64_t get_group_id(void) {return group_id;}

	void set_complete(void) {complete = true;}
	bool is_complete(void) {return complete;}
	void set_ground(bool _ground) {ground = _ground;}
	bool check_ground(void) {return ground;}
	void set_infected(void) {infected = true;}
	bool check_infected(void) {return infected;}

	virtual void decode_event(bool is_verbose, ofstream &outfile);
	virtual void streamout_event(ofstream &outfile) {};
};
#endif
