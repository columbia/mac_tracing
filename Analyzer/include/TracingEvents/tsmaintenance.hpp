#ifndef TSMAINTENANCE_HPP
#define TSMAINTENANCE_HPP
 class TsmaintenanceEvent : public EventBase {
     void *preempted_group_ptr;
 public:
     TsmaintenanceEvent(double abstime, string op, uint64_t _tid, uint32_t event_core, string proc = "");
     void save_gptr(void * g_ptr) {preempted_group_ptr = g_ptr;}
     void *load_gptr(void); 
     void decode_event(bool is_verbose, ofstream &outfile);
     void streamout_event(ofstream &outfile);
}; 
#endif
