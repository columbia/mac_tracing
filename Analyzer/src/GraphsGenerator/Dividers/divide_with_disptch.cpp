#include  "thread_divider.hpp"
#define DEBUG_THREAD_DIVIDER 0

//////////////////////////////////////////////
//event schemas for divider
/////////////////////////////////////////////
void ThreadDivider::add_disp_invoke_begin_event(BlockInvokeEvent *invoke_event)
{
    current_disp_invokers.push(invoke_event);
    /*1. a new execution segment should initiate only when it is dequeued from dispatch queue */
    if (cur_group && invoke_event->get_root()) {
        if (cur_group->get_blockinvoke_level() > 0) {
            mtx.lock();
            std::cerr << "Nested block [currently it is not divided, check if it need isolated] ";
            std::cerr << "begins at " << std::fixed << std::setprecision(1) << invoke_event->get_abstime() << std::endl;
            std::cerr << "Parent at " << std::fixed << std::setprecision(1);
            std::cerr << cur_group->get_first_event()->get_abstime() << std::endl;
            mtx.unlock();
        } else {
            cur_group->set_divide_by_disp(DivideOldGroup);
            cur_group = nullptr;
        }
    }

    /*2. attach the dequeue event */
    if (dequeue_event) {
        if (invoke_event->get_root() == dequeue_event) {
            add_general_event_to_group(dequeue_event);
            dequeue_event->set_nested_level(cur_group->get_blockinvoke_level());
            dequeue_event = nullptr;
        } else {
            mtx.lock();
            std::cerr << "Event begins at " << std::fixed << std::setprecision(1) << invoke_event->get_abstime() << std::endl;
            std::cerr << "No matched to nearest dequeue event at " << std::fixed << std::setprecision(1);
            std::cerr << dequeue_event->get_abstime() << std::endl;
            mtx.unlock();
        }
    }

    /*3. check dispatch_mig_server*/
    if (!dispatch_mig_servers.empty()) {
        mtx.lock();
        std::cerr << std::hex << invoke_event->get_tid() << " ";
        std::cerr << ": nest block invoke event inside dispatch_mig_server. ";
        std::cerr << "InvokeBlock begins at " << std::fixed << std::setprecision(1);
        std::cerr << invoke_event->get_abstime() << std::endl;
        mtx.unlock();
    }

    /*4. processing the backtrace */
    if (backtrace_for_hook && backtrace_for_hook->hook_to_event(invoke_event, DISP_INV_EVENT)) {
        add_general_event_to_group(backtrace_for_hook);
        for (int i = 0; i < backtrace_for_hook->get_size(); i++)
            cur_group->add_group_tags(backtrace_for_hook->get_sym(i));
        backtrace_for_hook = nullptr;
    }

    /*5. add event to group*/
    add_general_event_to_group(invoke_event);
	if (invoke_event->get_desc().size() > 0)
    	cur_group->add_group_tags(invoke_event->get_desc());
    invoke_event->set_nested_level(cur_group->get_blockinvoke_level());
    cur_group->blockinvoke_level_inc();
}

void ThreadDivider::add_disp_invoke_end_event(BlockInvokeEvent *invoke_event, BlockInvokeEvent *begin_invoke)
{
    /*1. check the stack of dispatch block stacks*/
    if (!current_disp_invokers.empty()) {
#if DEBUG_THREAD_DIVIDER
        mtx.lock();
        std::cerr << std::hex << invoke_event->get_tid() << " ";
        std::cerr << " pop current_disp_invokers size " << current_disp_invokers.size();
        std::cerr << " invoke_block end at "<< std::fixed << std::setprecision(1);
        std::cerr << invoke_event->get_abstime();
        std::cerr << " for " << std::fixed << std::setprecision(1) << current_disp_invokers.top()->get_abstime() << std::endl;
        mtx.unlock();
#endif
    } else {
        //should not happened in the process of dividing, as it has checked in the process of parsing.
        mtx.lock(); 
        std::cerr << std::hex << invoke_event->get_tid() << " ";
        std::cerr << "[error] empty stack of invoker_begin at " << std::fixed << std::setprecision(1);
        std::cerr << invoke_event->get_abstime();
        mtx.unlock();
        //assert(!current_disp_invokers.empty());
        add_general_event_to_group(invoke_event);
        return;
    }

    if (!(begin_invoke == current_disp_invokers.top())) {
        //should not happened in the process of dividing, as it has checked in the process of parsing.
        mtx.lock();
        std::cerr << std::hex << invoke_event->get_tid() << " ";
        std::cerr << "[error] matching at " << std::hex << std::fixed << std::setprecision(1);
        std::cerr << invoke_event->get_abstime();
        std::cerr << ". Top of stack is at" << std::hex << std::fixed << std::setprecision(1);
        std::cerr << current_disp_invokers.top()->get_abstime() << std::endl;
        mtx.unlock();
        assert(begin_invoke == current_disp_invokers.top());
    }

    current_disp_invokers.pop();

    /*2. dispatch_mig_server inside block */
    if (!dispatch_mig_servers.empty()) {
        DispatchQueueMIGServiceEvent *current_mig_server = dispatch_mig_servers.top();
        if (current_mig_server->get_mig_invoker() == begin_invoke) {
            cur_group = (Group *)(current_mig_server->restore_owner());
            assert(cur_group && cur_group->get_blockinvoke_level() > 0);
            dispatch_mig_servers.pop();
        }
    }
    
    /*3. check the case dispatch block get broken*/
    if (cur_group == nullptr) {
        mtx.lock();
        std::cerr << "invoke block is broken at " << std::fixed << std::setprecision(1) << invoke_event->get_abstime() << std::endl;
        std::cerr << "It begins at " << std::fixed << std::setprecision(1) << begin_invoke->get_abstime() << std::endl;
        mtx.unlock();
        cur_group = gid2group(begin_invoke->get_group_id());
    }

#if DEBUG_THREAD_DIVIDER
    if (gid2group(begin_invoke->get_group_id()) != cur_group) {
        mtx.lock();
        std::cerr << "Warn: invoke_block(not_queued) end at "<< std::fixed << std::setprecision(1);
        std::cerr << invoke_event->get_abstime();
        std::cerr << " in Group 0x" << std::hex << cur_group->get_group_id();
        std::cerr << "; block begins invoke_block(not_queued) at "<< std::fixed << std::setprecision(1);
        std::cerr << begin_invoke->get_abstime();
        std::cerr << " in Group 0x" << std::hex << begin_invoke->get_group_id();
        std::cerr << std::endl;
        mtx.unlock();
    }
#endif

    if (cur_group->get_blockinvoke_level() <= 0) {
        mtx.lock();
        std::cerr << "Error: unbalanced invoke at " << std::fixed << std::setprecision(1) << invoke_event->get_abstime() << std::endl;
        assert(cur_group->get_first_event());
        std::cerr << "group begins at " << std::fixed << std::setprecision(1) << cur_group->get_first_event()->get_abstime() << std::endl;
        mtx.unlock();
        assert(cur_group && cur_group->get_blockinvoke_level() > 0);
    }
    
    add_general_event_to_group(invoke_event);
    cur_group->add_group_tags(invoke_event->get_desc());
    cur_group->blockinvoke_level_dec();
    invoke_event->set_nested_level(cur_group->get_blockinvoke_level());

    if (begin_invoke->get_root() && cur_group->get_blockinvoke_level() == 0) {
        cur_group->set_divide_by_disp(DivideOldGroup);
        cur_group = nullptr;
    }
}

void ThreadDivider::add_disp_invoke_event_to_group(EventBase *event)
{
    BlockInvokeEvent *invoke_event = dynamic_cast<BlockInvokeEvent *>(event);
    assert(invoke_event);
    if (invoke_event->is_begin()) {
        add_disp_invoke_begin_event(invoke_event);
    } else {
        /* sanity check for block invoke event
         * and restore group if dispatch_mig_server invoked
         */
        BlockInvokeEvent *begin_invoke = dynamic_cast<BlockInvokeEvent *>(invoke_event->get_root());
        assert(begin_invoke);
        add_disp_invoke_end_event(invoke_event, begin_invoke);    
    }
}

//dispatch_mig_server is usually called inside a block.
void ThreadDivider::add_disp_mig_event_to_group(EventBase *event)
{
    DispatchQueueMIGServiceEvent *dispatch_mig_server = dynamic_cast<DispatchQueueMIGServiceEvent *>(event);
    add_general_event_to_group(event);

    dispatch_mig_servers.push(dispatch_mig_server);
    if (!current_disp_invokers.empty()) {
        BlockInvokeEvent *mig_server_invoker = current_disp_invokers.top();
        dispatch_mig_server->set_mig_invoker(mig_server_invoker);
        assert(cur_group == gid2group(mig_server_invoker->get_group_id()));
        dispatch_mig_server->save_owner(cur_group);
        cur_group->set_divide_by_disp(DivideOldGroup);
        cur_group = nullptr;
    } else {
#ifdef DEBUG_THREAD_DIVIDER
        mtx.lock();
        std::cerr << "Error: dispatch_mig_server called outside block invoke";
        std::cerr << " at " << std::fixed << std::setprecision(1) << event->get_abstime() << std::endl;
        std::cerr << "Unable to identify the end of dispatch_mig_server in such cases" << std::endl;
        mtx.unlock();
#endif
    }
}
