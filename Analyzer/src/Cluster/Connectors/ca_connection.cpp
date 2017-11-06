#include "ca_connection.hpp"
#include "eventlistop.hpp"

#define DEBUG_CA_CONN 0
CAConnection::CAConnection(list<event_t *> &_caset_list, list<event_t *> &_cadisplay_list)
:caset_list(_caset_list), cadisplay_list(_cadisplay_list)
{
}

void CAConnection::ca_connection(void)
{
	list<event_t *> mix_list;
	mix_list.insert(mix_list.end(), caset_list.begin(), caset_list.end());
	mix_list.insert(mix_list.end(), cadisplay_list.begin(), cadisplay_list.end());
	EventListOp::sort_event_list(mix_list);

	list<event_t *>::iterator it;
	list<event_t *>::reverse_iterator rit;
	
#ifdef DEBUG_CA_CONN
	mtx.lock();
	cerr << "begin core animation matching... " << endl;
	mtx.unlock();
#endif
	for (it = mix_list.begin(); it != mix_list.end(); it++) {
		ca_disp_ev_t * display_event = dynamic_cast<ca_disp_ev_t *>(*it);
		if (!display_event)
			continue;

		rit = find(mix_list.rbegin(), mix_list.rend(), display_event);
		uint64_t object_addr =  display_event->get_object_addr();

		for (; rit != mix_list.rend(); rit++) {
			ca_set_ev_t *set_event = dynamic_cast<ca_set_ev_t *>(*rit);
			if (!set_event)
				continue;
			assert(display_event->get_abstime() > set_event->get_abstime());
			if (set_event->get_object_addr() == object_addr) {
				// if the set_event has been matched,
				// all events on the layer before it should have been matched
				if (set_event->get_display_object() != NULL)
					break;
				display_event->push_set(set_event);
				set_event->set_display(display_event);
			}
		}
			
		if (display_event->ca_set_event_size() == 0) {
#if DEBUG_CA_CONN
			mtx.lock();
			cerr << "Unable to find corresponding set events for display CALayer "\
				<< hex << object_addr << " at "\
				<< fixed << setprecision(1) << display_event->get_abstime() << endl; 
			mtx.unlock();
#endif
		}

	}
	mix_list.clear();
#ifdef DEBUG_CA_CONN
	mtx.lock();
	cerr << "finish core animation matching." << endl;
	mtx.unlock();
#endif
}
