#include "canonization.hpp"

NormGroup::NormGroup(Group *g, uint64_t vtid)
{
	key_events.insert(make_pair(MACH_IPC_MSG, true));
	key_events.insert(make_pair(MACH_IPC_VOUCHER_INFO, false));
	key_events.insert(make_pair(MACH_IPC_VOUCHER_CONN, false));
	key_events.insert(make_pair(MACH_IPC_VOUCHER_TRANSIT, false));
	key_events.insert(make_pair(MACH_IPC_VOUCHER_DEALLOC, false));
	key_events.insert(make_pair(MACH_BANK_ACCOUNT, false));
	key_events.insert(make_pair(MACH_MK_RUN, false));
	key_events.insert(make_pair(INTR, false));
	key_events.insert(make_pair(WQ_NEXT, false));
	key_events.insert(make_pair(MACH_TS, false));
	key_events.insert(make_pair(MACH_WAIT, false));
	key_events.insert(make_pair(DISP_ENQ, true));
	key_events.insert(make_pair(DISP_DEQ, true));
	key_events.insert(make_pair(DISP_EXE, true));
	key_events.insert(make_pair(MACH_CALLCREATE, true));
	key_events.insert(make_pair(MACH_CALLOUT, true));
	key_events.insert(make_pair(MACH_CALLCANCEL, true));
	key_events.insert(make_pair(BACKTRACE, false));
	key_events.insert(make_pair(MACH_SYS, true));
	key_events.insert(make_pair(BSD_SYS, true));

	virtual_tid  = vtid;
	group = g;
	is_norm = false;
	delta = 0.0;
	compressed = 0;
	in = out = 0;
	container.clear();
	normalize_events();
	//compress_group();
}

NormGroup::~NormGroup(void)
{
	list<norm_ev_t *>::iterator it;
	for (it = container.begin(); it != container.end(); it++) {
		assert(*it != NULL);
		delete(*it);
	}
	container.clear();
	key_events.clear();
}

void NormGroup::normalize_events(void)
{
	list<event_t *> & events = group->get_container();
	list<event_t *>::iterator it;
	double begin, last;

	for (it = events.begin(); it != events.end(); it++) {
		last = (*it)->get_abstime();
		if (key_events[LoadData::map_op_code(0, (*it)->get_op())] == true) {
			norm_ev_t *norm_event = new norm_ev_t((*it), virtual_tid);
			if (norm_event == NULL)
				cerr << "OOM, no space for norm_event" << endl;
			else
				container.push_back(norm_event);
		}
	}
	begin = (*events.begin())->get_abstime();
	delta = last - begin;
}

uint32_t NormGroup::check_event_sequence(list<norm_ev_t *>::iterator begin, int step)
{
	list<norm_ev_t *>::iterator it = begin;
	vector<norm_ev_t> event_seq;

	for (int i = 0; i < step; i++, it++)
		event_seq.push_back(**it);

	for (int i = 0; it != container.end(); it++) {
		if (**it != event_seq[i % step])
			return 0;
	}

	return step;
}

/* TODO : checking backtrace in the group
 * if it contains [NSApplication sendEvent]
 * then this is a begin of user manipulation
 */


void NormGroup::compress_group(void)
{
	/* check the potential repeated patterns
	 * for example repated BSC / repeated timer_callouts
	 * remove redundant events, mark with compressed number
	 * in summaried field
	 */
	if (!container.size())
		return;

	list<norm_ev_t *>::iterator it, jt;

	for (it = container.begin(); it != container.end(); it++) {
		int steps;

		for (steps = 1; steps < distance(it, container.end()) / 2; steps++) {
			jt = it;
			advance(jt, steps);
			if ((**it == **jt) && (check_event_sequence(it, steps))) {
				compressed =  (uint64_t)distance(container.begin(), it) << 32| (uint64_t)steps;
				break;
			}
		}

		if (compressed)
			break;

	}
}

void NormGroup::check_compress(void)
{
	if (compressed) {
		cerr << "[Group compress] Group_id " << hex << get_group_id() << " compress with step " << (uint32_t)compressed;
		cerr << " begins at " << hex << (uint32_t)(compressed >> 32) << endl;
	}
}


bool NormGroup::operator==(NormGroup &other)
{
	
	map<string, uint32_t> peer_group_tags = other.get_group_tags();
	map<string, uint32_t>::iterator it;
	list<norm_ev_t *> other_container = other.get_container();
	list<norm_ev_t *>::iterator this_it;
	list<norm_ev_t *>::iterator other_it;
	bool ret = true;

	if (get_group_tags().size()) {
		/* to fail quickly with comarision of tags, eg backtraces */
		if (get_group_tags().size() != peer_group_tags.size()) {
			ret = false;
			goto out;
		}
		
		for (it = get_group_tags().begin(); it != get_group_tags().end(); it++) {
			string tag = it->first;
			if (peer_group_tags.find(tag) == peer_group_tags.end()) {
				ret = false;
				goto out;
			} else if (peer_group_tags[tag] != it->second) {
				ret = false;
				goto out;
			}
		}
	}

	/* TODO number of event may be misleading 
	 * events repeated periodically
	 * of different numbers of repeating
	 */
	if (other_container.size() != container.size()) {
		ret = false;
		goto out;
	}

	for (this_it = container.begin(), other_it = other_container.begin();
			this_it != container.end(); this_it++, other_it++) {
		if (**this_it != **other_it) {
			ret = false;
			break;
		}
	}

	if (ret == true)
		is_norm = true;
out:
	return ret;
}

bool NormGroup::operator!=(NormGroup &other)
{
	return !(*this == other);
}

void NormGroup::decode_group(ofstream &output)
{
	group->streamout_group(output);
}
