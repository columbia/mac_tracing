#ifndef INTERRUPT_HPP
#define INTERRUPT_HPP

#include "base.hpp"

#define LAPIC_DEFAULT_INTERRUPT_BASE 0xD0
#define LAPIC_REDUCED_INTERRUPT_BASE 0x50

#define LAPIC_PERFCNT_INTERRUPT		0xF
#define LAPIC_INTERPROCESSOR_INTERRUPT	0xE
#define LAPIC_TIMER_INTERRUPT		0xD
#define LAPIC_THERMAL_INTERRUPT		0xC
#define LAPIC_ERROR_INTERRUPT		0xB
#define LAPIC_SPURIOUS_INTERRUPT	0xA
#define LAPIC_CMCI_INTERRUPT		0x9
#define LAPIC_PMC_SW_INTERRUPT		0x8
#define LAPIC_PM_INTERRUPT			0x7
#define LAPIC_KICK_INTERRUPT		0x6
#define LAPIC_NMI_INTERRUPT			0x2

struct task_requested_policy {
	/* Task and thread policy (inherited) */
	uint64_t        int_darwinbg        :1,     /* marked as darwinbg via setpriority */
	                ext_darwinbg        :1,
	                int_iotier          :2,     /* IO throttle tier */
	                ext_iotier          :2,
	                int_iopassive       :1,     /* should IOs cause lower tiers to be throttled */
	                ext_iopassive       :1,
	                bg_iotier           :2,     /* what IO throttle tier should apply to me when I'm darwinbg? (pushed to threads) */
	                terminated          :1,     /* all throttles should be removed for quick exit or SIGTERM handling */

	/* Thread only policy */
	                th_pidbind_bg       :1,     /* thread only: task i'm bound to is marked 'watchbg' */
	                th_workq_bg         :1,     /* thread only: currently running a background priority workqueue */
	                thrp_qos            :3,     /* thread only: thread qos class */
	                thrp_qos_relprio    :4,     /* thread only: thread qos relative priority (store as inverse, -10 -> 0xA) */
	                thrp_qos_override   :3,     /* thread only: thread qos class override */

	/* Task only policy */
	                t_apptype           :3,     /* What apptype did launchd tell us this was (inherited) */
	                t_boosted           :1,     /* Has a non-zero importance assertion count */
	                t_int_gpu_deny      :1,     /* don't allow access to GPU */
	                t_ext_gpu_deny      :1,
	                t_role              :3,     /* task's system role */
	                t_tal_enabled       :1,     /* TAL mode is enabled */
	                t_base_latency_qos  :3,     /* Timer latency QoS */
	                t_over_latency_qos  :3,     /* Timer latency QoS override */
	                t_base_through_qos  :3,     /* Computation throughput QoS */
	                t_over_through_qos  :3,     /* Computation throughput QoS override */
	                t_sfi_managed       :1,     /* SFI Managed task */
	                t_qos_clamp         :3,     /* task qos clamp */

	/* Task only: suppression policies (non-embedded only) */
	                t_sup_active        :1,     /* Suppression is on */
	                t_sup_lowpri_cpu    :1,     /* Wants low priority CPU (MAXPRI_THROTTLE) */
	                t_sup_timer         :3,     /* Wanted timer throttling QoS tier */
	                t_sup_disk          :1,     /* Wants disk throttling */
	                t_sup_cpu_limit     :1,     /* Wants CPU limit (not hooked up yet)*/
	                t_sup_suspend       :1,     /* Wants to be suspended */
	                t_sup_throughput    :3,     /* Wants throughput QoS tier */
	                t_sup_cpu           :1,     /* Wants suppressed CPU priority (MAXPRI_SUPPRESSED) */
	                t_sup_bg_sockets    :1,     /* Wants background sockets */

	                reserved            :2;
};

struct task_effective_policy {
	/* Task and thread policy */
	uint64_t        darwinbg            :1,     /* marked as 'background', and sockets are marked bg when created */
	                lowpri_cpu          :1,     /* cpu priority == MAXPRI_THROTTLE */
	                io_tier             :2,     /* effective throttle tier */
	                io_passive          :1,     /* should IOs cause lower tiers to be throttled */
	                all_sockets_bg      :1,     /* All existing sockets in process are marked as bg (thread: all created by thread) */
	                new_sockets_bg      :1,     /* Newly created sockets should be marked as bg */
	                bg_iotier           :2,     /* What throttle tier should I be in when darwinbg is set? */
	                terminated          :1,     /* all throttles have been removed for quick exit or SIGTERM handling */
	                qos_ui_is_urgent    :1,     /* bump UI-Interactive QoS up to the urgent preemption band */

	/* Thread only policy */
	                thep_qos            :3,     /* thread only: thread qos class */
	                thep_qos_relprio    :4,     /* thread only: thread qos relative priority (store as inverse, -10 -> 0xA) */

	/* Task only policy */
	                t_gpu_deny          :1,     /* not allowed to access GPU */
	                t_tal_engaged       :1,     /* TAL mode is in effect */
	                t_suspended         :1,     /* task_suspend-ed due to suppression */
	                t_watchers_bg       :1,     /* watchers are BG-ed */
	                t_latency_qos       :3,     /* Timer latency QoS level */
	                t_through_qos       :3,     /* Computation throughput QoS level */
	                t_sup_active        :1,     /* suppression behaviors are in effect */
	                t_role              :3,     /* task's system role */
	                t_suppressed_cpu    :1,     /* cpu priority == MAXPRI_SUPPRESSED (trumped by lowpri_cpu) */
	                t_sfi_managed       :1,     /* SFI Managed task */
	                t_live_donor        :1,     /* task is a live importance boost donor */
	                t_qos_clamp         :3,     /* task qos clamp (applies to qos-disabled threads too) */
	                t_qos_ceiling       :3,     /* task qos ceiling (applies to only qos-participating threads) */
	                reserved            :23;
};

struct task_pended_policy {
	uint64_t        t_updating_policy   :1,     /* Busy bit for task to prevent concurrent 'complete' operations */

	/* Task and thread policy */
	                update_sockets      :1,

	/* Task only policy */
	                t_update_timers     :1,
	                t_update_watchers   :1,

	                reserved            :60;
};

#define AST_PREEMPT     0x01
#define AST_QUANTUM     0x02
#define AST_URGENT      0x04
#define AST_HANDOFF     0x08
#define AST_YIELD       0x10
#define AST_APC         0x20    /* migration APC hook */
#define AST_LEDGER      0x40
#define AST_BSD         0x80
#define AST_KPERF       0x100   /* kernel profiling */
#define AST_MACF        0x200   /* MACF user ret pending */
#define AST_CHUD        0x400 
#define AST_CHUD_URGENT     0x800
#define AST_GUARD       0x1000
#define AST_TELEMETRY_USER  0x2000  /* telemetry sample requested on interrupt from userspace */
#define AST_TELEMETRY_KERNEL    0x4000  /* telemetry sample requested on interrupt from kernel */
#define AST_TELEMETRY_WINDOWED  0x8000  /* telemetry sample meant for the window buffer */
#define AST_SFI         0x10000 /* Evaluate if SFI wait is needed before return to userspace */ 


class IntrEvent : public EventBase {
	double finish_time;
	uint32_t interrupt_num;
	int16_t sched_priority_pre;
	int16_t sched_priority_post;
	uint64_t rip;
	string rip_info;
	uint64_t user_mode;
	uint64_t ast;
	struct task_effective_policy effective_policy;
	struct task_requested_policy request_policy;
	vector<uint64_t> invoke_threads;
	const char* decode_interrupt_num(uint64_t);
	int32_t get_thep_qos_relprio() {return request_policy.thrp_qos_relprio;}
	int32_t get_thep_qos() {return request_policy.thrp_qos;}
	string ast_desc(uint32_t ast);
	string decode_ast(uint64_t ast);
	intr_ev_t * dup;

public:
	IntrEvent(double timestamp, string op, uint64_t _tid, uint64_t intr_no, uint64_t rip, uint64_t user_mode, uint32_t _coreid, string procname = "");
	IntrEvent(IntrEvent * intr);
	~IntrEvent();
	void add_dup(intr_ev_t * _dup) {dup = _dup;}
	void set_sched_priority_post(int16_t prio) {sched_priority_post = prio;}
	void set_ast(uint64_t _ast) {ast = _ast;}
	void set_effective_policy(uint64_t effect) {effective_policy = *(struct task_effective_policy *)(&effect);}
	void set_request_policy(uint64_t request) {request_policy = *(struct task_requested_policy*)(&request);}
	void set_finish_time(double timestamp) {finish_time = timestamp;}
	uint32_t get_interrupt_num(void) {return interrupt_num;}
	int16_t get_sched_priority_pre(void) {return sched_priority_pre;}
	int16_t get_sched_priority_post(void) {return sched_priority_post;}
	void set_rip_info(string & symbol) {rip_info = symbol;}
	string &get_rip_info(void) {return rip_info;}
	uint64_t get_rip(void) {return rip;}
	uint64_t get_user_mode(void) {return user_mode;}
	double get_finish_time(void) {return finish_time;}
	void add_invoke_thread(uint64_t tid) {invoke_threads.push_back(tid);}
	void decode_event(bool is_verbose, ofstream &outfile);
	void streamout_event(ofstream &outfile);
};

#endif
