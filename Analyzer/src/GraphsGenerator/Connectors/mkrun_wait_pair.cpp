#include "mkrun_wait_pair.hpp"
#include "eventlistop.hpp"

#define MKRUN_WAIT_DEBUG 0

MkrunWaitPair::MkrunWaitPair(list<event_t *> &_wait_list, list<event_t *> &_mkrun_list)
:wait_list(_wait_list), mkrun_list(_mkrun_list)
{
}

void MkrunWaitPair::pair_wait_mkrun(void)
{
	list<event_t*> mix_sorted_list;
	list<event_t*>::iterator it;
	map<uint64_t, wait_ev_t*> wait_map;
	wait_ev_t * wait;
	mkrun_ev_t * mr_event;

	mix_sorted_list.insert(mix_sorted_list.end(), wait_list.begin(), wait_list.end());
	mix_sorted_list.insert(mix_sorted_list.end(), mkrun_list.begin(), mkrun_list.end());
	EventLists::sort_event_list(mix_sorted_list);

#ifdef MKRUN_WAIT_DEBUG
	mtx.lock();
	cerr << "begin matching mkrun and wait ... " << endl;
	mtx.unlock();
#endif
	for (it = mix_sorted_list.begin(); it != mix_sorted_list.end(); it++) {
		wait = dynamic_cast<wait_ev_t*>(*it);
		if (wait) {
#if MKRUN_WAIT_DEBUG
			if (wait_map.find(wait->get_tid()) != wait_map.end()) {
				mtx.lock();
				cerr << "Warning: multiple waits " << fixed << setprecision(1) << wait->get_abstime() << endl;
				mtx.unlock();
			}
#endif
			wait_map[wait->get_tid()] = wait;
		} else {
			mr_event = dynamic_cast<mkrun_ev_t*>(*it);
			assert(mr_event);
			if (wait_map.find(mr_event->get_peer_tid()) != wait_map.end()) {
				mr_event->pair_wait(wait_map[mr_event->get_peer_tid()]);
				wait_map[mr_event->get_peer_tid()]->pair_mkrun(mr_event);
			} else {
#if MKRUN_WAIT_DEBUG
				mtx.lock();
				cerr << "Warning: no wait to make runnable " << fixed << setprecision(1) << mr_event->get_abstime() << endl;
				mtx.unlock();
#endif
			}
		}	
	}
#ifdef MKRUN_WAIT_DEBUG
	mtx.lock();
	cerr << "finish matching mkrun and wait." << endl;
	mtx.unlock();
#endif
}
