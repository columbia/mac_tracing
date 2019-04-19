#include "canonization.hpp"
NormNode::NormNode(Node *_node, map<event_id_t, bool> &key_events)
{
	node = _node;
	norm_group = new NormGroup(node->get_group(), key_events);
}

NormNode::~NormNode()
{
	if (norm_group != NULL)
		delete norm_group;
}

bool NormNode::is_empty()
{
	return norm_group->is_empty();
}

bool NormNode::operator==(NormNode &other)
{
	if (*norm_group == *(other.norm_group))
		return true;
	return false;
}

bool NormNode::operator!=(NormNode &other)
{
	return !((*this) == other);
}
