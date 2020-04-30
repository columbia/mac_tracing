#include "parser.hpp"
#include "nsappevent.hpp"

namespace Parse
{
    NSAppEventParser::NSAppEventParser(std::string filename)
    :Parser(filename)
    {
        nsappevent_events.clear();
    }

    void NSAppEventParser::process()
    {
        std::string line, deltatime, opname, procname;
        double abstime;
        uint64_t tag, keycode, unused, tid, coreid;

        while (getline(infile, line)) {
            std::istringstream iss(line);

            if (!(iss >> abstime >> deltatime >> opname) ||
                !(iss >> std::hex >> tag >> keycode >> unused >> unused) ||
                !(iss >> tid >> coreid)) {
                outfile << line << std::endl;
                continue;
            }

            if (!getline(iss >> std::ws, procname) || !procname.size())
                procname = ""; 

            if (opname == "ARGUS_AppKit_SendEvent") {
                if (nsappevent_events.find(tid) != nsappevent_events.end()) {
                    nsappevent_events[tid]->set_event(tag, keycode);
                    nsappevent_events[tid]->set_complete();
                    nsappevent_events.erase(tid);
                } else {
                    outfile <<"No nsapplication event for matching" << std::endl;
                    outfile << line << std::endl;
                }
            }
            
            if (opname == "ARGUS_AppKit_GetEvent") {

                NSAppEventEvent *nsapp_event_event = new NSAppEventEvent(abstime, opname, tid, tag, coreid, procname);
                if (!nsapp_event_event) {
                    std::cerr << "OOM " << __func__ << std::endl;
                    exit(EXIT_FAILURE);
                }

                local_event_list.push_back(nsapp_event_event);
                nsappevent_events[tid] = nsapp_event_event;
            }
            //nsapp_event_event->set_complete();
        }
    } 
}
