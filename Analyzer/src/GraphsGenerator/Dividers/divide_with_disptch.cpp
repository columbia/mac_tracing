#include  "thread_divider.hpp"
#define DEBUG_THREAD_DIVIDER 1

//////////////////////////////////////////////
//event schemas for divider
/////////////////////////////////////////////
void ThreadDivider::add_disp_invoke_event_to_group(event_t *event)
{
	blockinvoke_ev_t *invoke_event = dynamic_cast<blockinvoke_ev_t *>(event);
	assert(invoke_event);

	if (invoke_event->is_begin()) {
		current_disp_invokers.push(invoke_event);
		/*1. if a new execution segment should initiate
		 * only isolate block when it is from dispatch queue
		 */
		if (cur_group && !cur_group->get_blockinvoke_level() && invoke_event->get_root())
			cur_group = NULL;
		/*2. processing dequeue event */
		if (dequeue_event && invoke_event->get_root() == dequeue_event) {
			add_general_event_to_group(dequeue_event);
			dequeue_event->set_nested_level(cur_group->get_blockinvoke_level());
			dequeue_event = NULL;
		}

#if DEBUG_THREAD_DIVIDER
		/*check dispatch_mig_server*/
		if (!dispatch_mig_servers.empty()) {
		    mtx.lock();
		    cerr << "Check: nest block invoke event inside dispatch_mig_server. ";
		    cerr << "InvokeBlock begins at " << fixed << setprecision(1) << event->get_abstime() << endl;
		    mtx.unlock();
		}
#endif
		/*3. processing the backtrace */
		if (backtrace_for_hook && backtrace_for_hook->hook_to_event(event, DISP_INV_EVENT)) {
			add_general_event_to_group(backtrace_for_hook);
			cur_group->add_group_tags(backtrace_for_hook->get_symbols());
			backtrace_for_hook = NULL;
		}
		/*add event to group*/
		add_general_event_to_group(event);
		cur_group->add_group_tags(invoke_event->get_desc());
		invoke_event->set_nested_level(cur_group->get_blockinvoke_level());
		cur_group->blockinvoke_level_inc();
	} else {
		/* sanity check for block invoke event
		 * and restore group if dispatch_mig_server invoked
		 */
		assert(invoke_event->get_root() && !current_disp_invokers.empty());
		current_disp_invokers.pop();
		blockinvoke_ev_t *begin_invoke = dynamic_cast<blockinvoke_ev_t *>(invoke_event->get_root());
		/*1. dispatch_mig_server inside block*/
		if (!dispatch_mig_servers.empty()) {
			disp_mig_ev_t *current_mig_server = dispatch_mig_servers.top();
			if (current_mig_server->get_mig_invoker() == begin_invoke) {
				cur_group = (group_t *)(current_mig_server->restore_owner());
				assert(cur_group && cur_group->get_blockinvoke_level() > 0);
				dispatch_mig_servers.pop();
			}
		}
		
#if DEBUG_THREAD_DIVIDER
		/*check if it is not a dispatch queue block*/
		if (gid2group(begin_invoke->get_group_id()) != cur_group) {
			mtx.lock();
			cerr << "Error: invoke_block(not_queued) end at "<< fixed << setprecision(1);
			cerr << invoke_event->get_abstime();
			cerr << "; block begins invoke_block(not_queued) at "<< fixed << setprecision(1);
			cerr << begin_invoke->get_abstime() << endl;
			mtx.unlock();
		}

		if (cur_group->get_blockinvoke_level() <= 0) {
			mtx.lock();
			cerr << "Error: unbalanced invoke at " << fixed << setprecision(1) << invoke_event->get_abstime() << endl;
			assert(cur_group->get_first_event());
			cerr << "group begins at " << fixed << setprecision(1) << cur_group->get_first_event()->get_abstime() << endl;
			mtx.unlock();
			cur_group = NULL;
			return;
		}
#else
		assert(gid2group(begin_invoke->get_group_id()) == cur_group);
		assert(cur_group && cur_group->get_blockinvoke_level() > 0);
#endif

		add_general_event_to_group(event);
		cur_group->add_group_tags(invoke_event->get_desc());
		cur_group->blockinvoke_level_dec();
		invoke_event->set_nested_level(cur_group->get_blockinvoke_level());
		if (cur_group->get_blockinvoke_level() == 0)
			cur_group = NULL;
	}
}

//dispatch_mig_server is usually called inside a block.
void ThreadDivider::add_disp_mig_event_to_group(event_t *event)
{
	disp_mig_ev_t *dispatch_mig_server = dynamic_cast<disp_mig_ev_t *>(event);
	add_general_event_to_group(event);

	dispatch_mig_servers.push(dispatch_mig_server);
	if (!current_disp_invokers.empty()) {
		blockinvoke_ev_t *mig_server_invoker = current_disp_invokers.top();
		dispatch_mig_server->set_mig_invoker(mig_server_invoker);
		assert(cur_group == gid2group(mig_server_invoker->get_group_id()));
		dispatch_mig_server->save_owner(cur_group);
		cur_group = NULL;
	} else {
#ifdef DEBUG_THREAD_DIVIDER
		mtx.lock();
		cerr << "Error: dispatch_mig_server called outside block invoke";
		cerr << " at " << fixed << setprecision(1) << event->get_abstime() << endl;
		cerr << "Unable to identify the end of dispatch_mig_server in such cases" << endl;
		mtx.unlock();
#endif
	}

}
