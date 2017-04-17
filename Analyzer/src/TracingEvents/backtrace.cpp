#include "backtraceinfo.hpp"
#include "mach_msg.hpp"
#include "dispatch.hpp"

BacktraceEvent::BacktraceEvent(double abstime, string _op, uint64_t _tid, frames_t * _frame_info, uint64_t _max_frames, uint64_t _host_event_tag, uint32_t _core_id, string _procname)
:EventBase(abstime, _op, _tid, _core_id, _procname)
{
	host_event = NULL;
	host_event_tag = _host_event_tag;
	frame_info = _frame_info;
	max_frames = _max_frames;
}

BacktraceEvent::~BacktraceEvent(void)
{
	if (frame_info != NULL)
		delete(frame_info);
	frame_info = NULL;
}

void BacktraceEvent::add_frames(uint64_t *frames, int size)
{
	for (int i = 0; i < size; i++)
		frame_info->add_frame(frames[i]);
}

bool BacktraceEvent::hook_to_event(event_t * event, uint32_t _event_type) 
{
	bool ret = false;
	switch (_event_type) {
		case MSG_EVENT: {
			msg_ev_t * msg_event = dynamic_cast<msg_ev_t*>(event);
			if (msg_event && host_event_tag == msg_event->get_user_addr()) {
				host_event = event;
				msg_event->set_bt(this);
				ret = true;
			}
			break;
		}
		case BLOCKINVOKE: {
			blockinvoke_ev_t * blockinvoke_event = dynamic_cast<blockinvoke_ev_t *>(event);
			if (blockinvoke_event && host_event_tag == blockinvoke_event->get_func()) {
				host_event = event;
				blockinvoke_event->set_bt(this);
				ret = true;
			}
			break;
		}
		default:
			ret = false;
	}

	if (ret && check_infected()) {
		event->set_infected();
	}

	return ret;
}

void BacktraceEvent::symbolize_frame(debug_data_t * debugger,  map<string, map<uint64_t, string> >&image_vmsymbol_map)
{
	frame_info->symbolication(debugger, image_vmsymbol_map);
	if (frame_info->check_infected() == true) {
		set_infected();
		cerr << "backtrace set spin " << endl;
		if (LoadData::meta_data.spin_timestamp < 10e-8) {
			LoadData::meta_data.spin_timestamp = get_abstime();
			cerr << "bakctrace host set spin timestamp " << fixed << setprecision(1) << LoadData::meta_data.spin_timestamp << endl;
		}
	}
}

void BacktraceEvent::decode_event(bool is_verbose, ofstream &outfile)
{
	outfile << "\n*****" << endl;
    outfile << "\n group_id = " << std::right << hex << get_group_id();
	outfile << "\n\t" << get_procname() << "(" << hex << get_tid() << ")" << get_coreid();
	outfile << "\n\t" << fixed << setprecision(2) << get_abstime();
	outfile << "\n\t" << get_op() << endl;
	if (host_event != NULL) {
		outfile << "\thooked to " << fixed << setprecision(1) << host_event->get_abstime() << endl;
	} else {
		outfile << "\tno hooked" << endl;
	}
	outfile << "#frames = " << dec << max_frames << endl;
	frame_info->decode_frames(outfile);
}

void BacktraceEvent::streamout_event(ofstream &outfile) 
{
	vector<string> bts = frame_info->get_symbols();
	if (bts.size() <= 0)
		return;
	outfile << std::right << hex << get_group_id() << "\t" << fixed << setprecision(1) << get_abstime();
	outfile << "\t" << get_tid() << "\t" << get_procname() << "\tbacktrace\\n";
	if (host_event != NULL) {
		outfile << "\thooked to " << fixed << setprecision(1) << host_event->get_abstime() << endl;
	} else {
		outfile << "\tno hooked" << endl;
	}
	//outfile << "#frames = " << dec << max_frames << endl;
	frame_info->streamout(outfile);
}
