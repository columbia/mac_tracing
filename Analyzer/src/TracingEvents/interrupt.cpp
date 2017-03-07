#include "interrupt.hpp"

IntrEvent::IntrEvent(double timestamp, string op, uint64_t _tid, uint64_t intr_no, uint64_t _rip, uint64_t _user_mode, uint32_t _core_id, string procname)
:EventBase(timestamp, op, _tid, _core_id, procname)
{
	invoke_threads.clear();
	finish_time = 0;
	dup = NULL;
	interrupt_num = (uint32_t)intr_no;
	sched_priority_pre = (int16_t)(intr_no >> 32);
	rip = _rip;
	user_mode = _user_mode;
}

IntrEvent::IntrEvent(IntrEvent * intr)
:EventBase(intr)
{
	*this = *intr;
	dup = NULL;
	// or do memcopy?
}

IntrEvent::~IntrEvent()
{
	if (!dup)
		delete(dup);
	dup = NULL;
}

const char * IntrEvent::decode_interrupt_num(uint64_t interrupt_num)
{
	int n = interrupt_num - LAPIC_DEFAULT_INTERRUPT_BASE;
	switch(n) {
		case LAPIC_PERFCNT_INTERRUPT:
			return "PERFCNT";
		case LAPIC_INTERPROCESSOR_INTERRUPT:
			return "IPI";
		case LAPIC_TIMER_INTERRUPT:
			return "TIMER";
		case LAPIC_THERMAL_INTERRUPT:
			return "THERMAL";
		case LAPIC_ERROR_INTERRUPT:
			return "ERROR";
		case LAPIC_SPURIOUS_INTERRUPT:
			return "SPURIOUS";
		case LAPIC_CMCI_INTERRUPT:
			return "CMCI";
		case LAPIC_PMC_SW_INTERRUPT:
			return "PMC_SW";
		case LAPIC_PM_INTERRUPT:
			return "PM";
		case LAPIC_KICK_INTERRUPT:
			return "KICK";		
		default:
			return "unknown";
	}
}

string IntrEvent::ast_desc(uint32_t ast)
{
	string desc;
	if (ast & AST_PREEMPT)
		desc.append("preempt ");
	if (ast & AST_QUANTUM)
		desc.append("quantum ");
	if (ast & AST_URGENT)
		desc.append("urget ");
	if (ast & AST_HANDOFF)
		desc.append("handoff ");
	if (ast & AST_YIELD)
		desc.append("yield ");
	if (ast & AST_APC)
		desc.append("apc ");
	if (ast & AST_LEDGER)
		desc.append("ledger ");
	if (ast & AST_BSD)
		desc.append("bsd ");
	if (ast & AST_KPERF)
		desc.append("kperf ");
	if (ast & AST_MACF)
		desc.append("macf ");
	if (ast & AST_CHUD)
		desc.append("chud ");
	if (ast & AST_CHUD_URGENT)
		desc.append("ast_chud_urgent ");
	if (ast & AST_GUARD)
		desc.append("ast_guard ");
	if (ast & AST_TELEMETRY_USER)
		desc.append("ast_telmetry_user ");
	if (ast & AST_TELEMETRY_KERNEL)
		desc.append("ast_telemetry_kern ");
	if (ast & AST_TELEMETRY_WINDOWED)
		desc.append("ast_telemetry_windowed ");
	if (ast & AST_SFI)
		desc.append("ast_sfi ");
	if (!desc.size())
		desc = "ast_none";
	return desc;
}

string IntrEvent::decode_ast(uint64_t ast)
{
	uint32_t thr_ast = uint32_t(ast >> 32);
	uint32_t thr_reason = uint32_t(ast);
	return "thr_ast: " + ast_desc(thr_ast) + "\tblock_reason: " + ast_desc(thr_reason);
}

void IntrEvent::decode_event(bool is_verbose, ofstream & outfile)
{
	outfile << "\n*****" << endl;
	outfile << "\n group_id = " << std::right << hex << get_group_id();
	outfile << "\n\t" << get_procname() << "(" << hex << get_tid() << ")" << get_coreid();
	outfile << "\n\t" << fixed << setprecision(1) << get_abstime();
	outfile << " - " << fixed << setprecision(1) << finish_time;
	outfile << "\n\t" << get_op();
	const char * intr_desc = decode_interrupt_num(interrupt_num);
	if (strcmp(intr_desc, "unknown"))
		outfile << "\n\t" << intr_desc;
	else
		outfile << "\n\t" << interrupt_num;

	outfile << "\n\t" << "priority: " << hex << sched_priority_pre << " -> " << hex << sched_priority_post;
	outfile << "\n\t" << "thread_qos: " << hex << get_thep_qos();
	if (rip_info.size())
		outfile << "\n\t" << rip_info;
	outfile << "\n\t" << "thread_ast: " << ast_desc(ast >> 32);
	outfile << "\n\t" << "block_reason: " << ast_desc((uint32_t)ast);

	outfile << endl;
}

void IntrEvent::streamout_event(ofstream & outfile)
{
	outfile << std::right << hex << get_group_id();
	outfile << "\t" << fixed << setprecision(1) << get_abstime();
	outfile << "\t" << hex << get_tid();
	outfile << "\t" << get_procname();

	const char * intr_desc = decode_interrupt_num(interrupt_num);
	if (strcmp(intr_desc, "unknown"))
		outfile << "\t" << intr_desc << "_INTR";
	else
		outfile << "\t" << interrupt_num << "_INTR";

	if (rip_info.size())
		outfile << "\t" << rip_info;
	/*
	outfile << "\n(";
	outfile  << "priority " << hex << sched_priority_pre << "->" << hex << sched_priority_post;
	outfile << "\t" << "thread_qos: " << hex << get_thep_qos();
	outfile << "\t" << "thread_ast: " << ast_desc(ast >> 32);
	outfile << "\t" << "block_reason: " << ast_desc((uint32_t)ast);
	outfile << ")";
	*/
	
	outfile << endl;
}
