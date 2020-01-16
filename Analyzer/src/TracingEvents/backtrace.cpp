#include "mach_msg.hpp"
#include "dispatch.hpp"
#include "backtraceinfo.hpp"

#define DEBUG_BT 0

BacktraceEvent::BacktraceEvent(double abstime, std::string _op, uint64_t _tid, 
    uint64_t _tag, uint64_t _max_frames, uint32_t _core_id, std::string _procname)
:EventBase(abstime, BACKTRACE_EVENT, _op, _tid, _core_id, _procname),
Frames(_tag, _max_frames, _tid)
{
    host_event = nullptr;
}

BacktraceEvent::~BacktraceEvent(void)
{
}

void BacktraceEvent::add_frames(uint64_t *frames, int size)
{
    for (int i = 0; i < size; i++)
        add_frame(frames[i]);
	assert(frames_info[frames_info.size()-1].addr != 0);
}

bool BacktraceEvent::hook_to_event(EventBase *event, event_type_t event_type) 
{
    bool ret = false;
    switch (event_type) {
        case MSG_EVENT: {
            MsgEvent *msg_event = dynamic_cast<MsgEvent*>(event);
            if (msg_event && host_event_tag == msg_event->get_user_addr()) {
                host_event = event;
                msg_event->set_bt(this);
#if DEBUG_BT
                mtx.lock();
                std::cerr << std::fixed << std::setprecision(1) << get_abstime() << " hooked to msg ";
                std::cerr << std::fixed << std::setprecision(1) << host_event->get_abstime() << std::endl;
                mtx.unlock();
#endif
                ret = true;
            }
            break;
        }
        case DISP_INV_EVENT: {
            BlockInvokeEvent *blockinvoke_event = dynamic_cast<BlockInvokeEvent *>(event);
            if (blockinvoke_event && host_event_tag == blockinvoke_event->get_func()) {
                host_event = event;
                blockinvoke_event->set_bt(this);
#if DEBUG_BT
                mtx.lock();
                std::cerr << std::fixed << std::setprecision(1) << get_abstime() << " hooked to dispatch_block ";
                std::cerr << std::fixed << std::setprecision(1) << host_event->get_abstime() << std::endl;
                mtx.unlock();
#endif
                ret = true;
            }
            break;
        }
        default:
            ret = false;
    }

    return ret;
}

/*
void BacktraceEvent::symbolize_frame(debug_data_t *debugger, 
    std::map<std::string,std::map<uint64_t, std::string> > &image_vmsymbol_map)
{
    symbolication(debugger, image_vmsymbol_map);
}
*/

void BacktraceEvent::decode_event(bool is_verbose, std::ofstream &outfile)
{
    EventBase::decode_event(is_verbose, outfile);

    if (host_event)
        outfile << "\thooked to " << std::fixed << std::setprecision(1) << host_event->get_abstime() << std::endl;
    else
        outfile << "\tnot hooked" << std::endl;

    outfile << "#frames = " << std::dec << max_frames << std::endl;
    decode_frames(outfile);
}

void BacktraceEvent::streamout_event(std::ostream &out) 
{
    EventBase::streamout_event(out);
    decode_frames(out);
}

void BacktraceEvent::streamout_event(std::ofstream &outfile) 
{
    EventBase::streamout_event(outfile);
    if (host_event)
        outfile << "\\n\thooked to " << std::fixed << std::setprecision(1) << host_event->get_abstime() << std::endl;
    else
        outfile << "\\n\tno hooked" << std::endl;
    decode_frames(outfile);
}
