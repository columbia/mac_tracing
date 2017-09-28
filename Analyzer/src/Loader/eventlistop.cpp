#include "eventlistop.hpp"
#include "mach_msg.hpp"

namespace EventListOp
{
	void sort_event_list(list<event_t *> & evlist)
	{
		evlist.sort(Parse::EventComparator::compare_time);
	}
	
	void dump_all_event(list<event_t *> &evlist, const char *filepath)
	{
		ofstream dump(filepath);
		if (dump.fail()) {
			cout << "unable to open file " << filepath << endl;
			exit(EXIT_FAILURE);
		}
		
		list<event_t *>::iterator it;
		for(it = evlist.begin(); it != evlist.end(); it++) {
			event_t *cur_ev = *it;
			cur_ev->decode_event(1, dump);
		}
		dump.close();
	}
	
	void streamout_all_event(list<event_t *> &evlist, const char *filepath)
	{
		ofstream dump(filepath);
		if (dump.fail()) {
			cout << "unable to open file " << filepath << endl;
			exit(EXIT_FAILURE);
		}
		
		list<event_t *>::iterator it;
		for(it = evlist.begin(); it != evlist.end(); it++) {
			event_t *cur_ev = *it;
			cur_ev->streamout_event(dump);
		}
		dump.close();
	}

	void streamout_backtrace(list<event_t *> &evlist, const char *filepath)
	{
		ofstream dump(filepath);
		if (dump.fail()) {
			cout << "unable to open file " << filepath << endl;
			exit(EXIT_FAILURE);
		}

		list<event_t *>::iterator it;
		for(it = evlist.begin(); it != evlist.end(); it++) {
			event_t *cur_ev = *it;
			if (dynamic_cast<backtrace_ev_t *>(cur_ev))
				cur_ev->streamout_event(dump);
		}
		dump.close();
	}

	void clear_event_list(list<event_t *> &evlist)
	{
		list<event_t *>::iterator it;
		for(it = evlist.begin(); it != evlist.end(); it++) {
			event_t *cur_ev = *it;
			delete(cur_ev);
		}
		evlist.clear();
	}
}	
