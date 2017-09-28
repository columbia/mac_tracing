#ifndef CA_SET_HPP
#define CA_SET_HPP
#include "base.hpp"
#include "cadisplay.hpp"

using namespace std;
class CASetEvent : public EventBase {
	uint64_t object_addr;
	ca_disp_ev_t *display;
public:
	CASetEvent(double timestamp, string op, uint64_t tid, uint64_t object, uint32_t coreid, string procname = "");
	uint64_t get_object_addr(void) {return object_addr;}
	void set_display(ca_disp_ev_t *disp) {display = disp;}
	ca_disp_ev_t *get_display_object(void) {return display;}
	void decode_event(bool is_verbose, ofstream &outfile);
	void streamout_event(ofstream &outfile);
};
#endif
