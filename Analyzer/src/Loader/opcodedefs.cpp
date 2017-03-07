#include "loader.hpp"
namespace LoadData
{
	/*
	0 is reserved
	#define MACH_IPC_MSG				1
	#define MACH_IPC_VOUCHER_INFO		2
	#define MACH_IPC_VOUCHER_CONN		3
	#define MACH_IPC_VOUCHER_TRANSIT	5
	#define MACH_IPC_VOUCHER_DEALLOC	6
	#define MACH_BANK_ACCOUNT			7
	#define MACH_MK_RUN					8
	#define INTR						9
	#define WQ_NEXT						10
	#define MACH_TS						11
	#define MACH_WAIT					12
	#define DISP_ENQ					13
	#define DISP_DEQ					14
	#define DISP_EXE					15
	#define MACH_CALLCREATE				16
	#define MACH_CALLOUT				17
	#define MACH_CALLCANCEL				18
	#define BACKTRACE					19
	//#define BT_PATH					
	//#define BT_FRAME					
	#define MACH_SYS					20
	#define BSD_SYS						21
	*/

	const map<string, uint64_t> op_code_map = {
		make_pair("MACH_IPC_kmsg_free", MACH_IPC_MSG),
		make_pair("MACH_IPC_msg_trap", MACH_IPC_MSG),
		make_pair("MACH_IPC_msg_link", MACH_IPC_MSG),
		make_pair("MACH_IPC_msg_recv", MACH_IPC_MSG),
		make_pair("MACH_IPC_msg_recv_voucher_re", MACH_IPC_MSG),
		make_pair("MACH_IPC_msg_send", MACH_IPC_MSG),
		make_pair("MACH_IPC_msg_boarder", MACH_IPC_MSG),
		make_pair("MACH_IPC_msg_thread_voucher", MACH_IPC_MSG),
		make_pair("MACH_IPC_msg_dump", MACH_IPC_MSG),
		make_pair("MACH_IPC_msg_dsc", MACH_IPC_MSG),
		make_pair("MACH_IPC_msg_idsc", MACH_IPC_MSG),
		make_pair("MACH_IPC_voucher_info", MACH_IPC_VOUCHER_INFO),
		make_pair("MACH_IPC_voucher_conn", MACH_IPC_VOUCHER_CONN),
		make_pair("MACH_IPC_voucher_reuse", MACH_IPC_VOUCHER_TRANSIT),
		make_pair("MACH_IPC_voucher_create", MACH_IPC_VOUCHER_TRANSIT),
		make_pair("MACH_IPC_voucher_remove", MACH_IPC_VOUCHER_DEALLOC),
		make_pair("MACH_IPC_voucher_destroy", MACH_IPC_VOUCHER_DEALLOC),
		make_pair("Bank_Account_redeem", MACH_BANK_ACCOUNT),
	//	make_pair("Bank_Account_settle", MACH_BANK_ACCOUNT),
		make_pair("MACH_MKRUNNABLE", MACH_MK_RUN),
		make_pair("MACH_WAKEUP_REASON", MACH_MK_RUN),
		make_pair("INTERRUPT", INTR),
		make_pair("wq__run_nextitem", WQ_NEXT),
		make_pair("MACH_TS_MAINTENANCE", MACH_TS),
		make_pair("MACH_WAIT", MACH_WAIT),
		make_pair("MACH_WAIT_REASON", MACH_WAIT),
		make_pair("dispatch_enqueue", DISP_ENQ),
		make_pair("dispatch_dequeue", DISP_DEQ),
		make_pair("dispatch_execute", DISP_EXE),
		make_pair("MACH_CALLCREATE", MACH_CALLCREATE),
		make_pair("MACH_CALLOUT", MACH_CALLOUT),
		make_pair("MACH_CALLCANCEL", MACH_CALLCANCEL),
		//make_pair("MSG_Pathinfo", BT_PATH),
		//make_pair("MSG_Backtrace", BT_FRAME),
		make_pair("MSG_Pathinfo", BACKTRACE),
		make_pair("MSG_Backtrace", BACKTRACE),
		make_pair("MACH_SYSCALL", MACH_SYS),
		make_pair("BSD_SYSCALL", BSD_SYS)
	};
}
