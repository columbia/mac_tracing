#include "thread_divider.hpp"
ThreadDivider::ThreadDivider()
{
}

group_t * ThreadDivider::create_group(uint64_t id, event_t * root_event)
{
	group_t * new_gptr = new group_t(id, root_event);
	if (root_event) 
		new_gptr->add_to_container(root_event);
	if (new_gptr == NULL)
		cerr << "Error : OOM can not create a new group." << endl;
	return new_gptr;
}

void ThreadDivider::decode_groups(map<uint64_t, group_t *> & uievent_groups, string filepath)
{
	ofstream output(filepath);
	if (output.fail()) {
		cerr << "Err: unable to open file " << filepath << " for write " << endl;
		return;
	}
	map<uint64_t, group_t *>::iterator it;
	group_t * cur_group;
	uint64_t index = 0;
	for (it = uievent_groups.begin(); it != uievent_groups.end(); it++) {
		output << "#Group " << hex << index << endl;
		index++;
		cur_group = it->second;
		cur_group->decode_group(output);
	}
	output.close();
}

void ThreadDivider::streamout_groups(map<uint64_t, group_t *> & uievent_groups, string filepath)
{
	ofstream output(filepath);
	if (output.fail()) {
		cerr << "Err: unable to open file " << filepath << " for write " << endl;
		return;
	}
	map<uint64_t, group_t *>::iterator it;
	group_t * cur_group;
	uint64_t index = 0;
	for (it = uievent_groups.begin(); it != uievent_groups.end(); it++) {
		output << "#Group " << hex << index << endl;
		index++;
		cur_group = it->second;
		cur_group->streamout_group(output);
	}
	output.close();
}

