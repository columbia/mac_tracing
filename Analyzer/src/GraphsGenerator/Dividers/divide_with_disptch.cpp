#include  "thread_divider.hpp"
#define DEBUG_THREAD_DIVIDER 0

//////////////////////////////////////////////
//event schemas for divider
/////////////////////////////////////////////

void ThreadDivider::add_disp_invoke_begin_event(BlockInvokeEvent *invoke_event)
{
    current_disp_invokers.push(invoke_event);
    invoke_event->set_nested_level(current_disp_invokers.size());
    /*1. a new execution segment should initiate only when it is dequeued from dispatch queue */
    if (invoke_event->get_root()) {
		if (cur_group != nullptr && cur_group->get_blockinvoke_level() > 0) {
#if DEBUG_THREAD_DIVIDER 
            mtx.lock();
            LOG_S(INFO) << "Nested block [isolated items inside hueristically] \n" \
						<< "\tBlock begins at " << std::fixed << std::setprecision(1)\
						<< invoke_event->get_abstime() << std::endl\
						<< "\tParent at" << std::fixed << std::setprecision(1) \
          				<< cur_group->get_first_event()->get_abstime() << std::endl;
            mtx.unlock();
#endif
        	cur_group->set_divide_by_disp(DivideOldGroup);
		}
		//have a new group
        cur_group = nullptr;
    }

    /*2. attach the dequeue event */
    if (dequeue_event) {
        if (invoke_event->get_root() == dequeue_event) {
            dequeue_event->set_nested_level(invoke_event->get_nested_level());
            add_general_event_to_group(dequeue_event);
            dequeue_event = nullptr;
        } else {
            mtx.lock();
			LOG_S(INFO) << "Block beginning at " << std::fixed << std::setprecision(1) \
						<< invoke_event->get_abstime() \
            			<< " doesn't  match to nearest dequeue event at "\
						<< std::fixed << std::setprecision(1)\
            			<< dequeue_event->get_abstime() << std::endl;
            mtx.unlock();
        }
    }

#if DEBUG_THREAD_DIVIDER 
    /*3. check dispatch_mig_server*/
    if (!dispatch_mig_servers.empty()) {
        mtx.lock();
		LOG_S(INFO) << "Nested block [dispatch_mig_server] \n"\
					<< "\tBlock begins at " << std::fixed << std::setprecision(1)\
        			<< invoke_event->get_abstime() << std::endl\
					<< "\tParent at" << std::fixed << std::setprecision(1) \
					<< dispatch_mig_servers.top()->get_abstime() << std::endl;
        mtx.unlock();
    }
#endif

    /*4. processing the backtrace */
    if (backtrace_for_hook && backtrace_for_hook->hook_to_event(invoke_event, DISP_INV_EVENT)) {
        add_general_event_to_group(backtrace_for_hook);
        for (int i = 0; i < backtrace_for_hook->get_size(); i++)
            cur_group->add_group_tags(backtrace_for_hook->get_sym(i));
        backtrace_for_hook = nullptr;
    }

    /*5. add event to group*/
    add_general_event_to_group(invoke_event);
    cur_group->blockinvoke_level_inc();
	if (invoke_event->get_desc().size() > 0)
    	cur_group->add_group_tags(invoke_event->get_desc());
}

void ThreadDivider::add_disp_invoke_end_event(BlockInvokeEvent *invoke_event, BlockInvokeEvent *begin_invoke)
{
    /*1. check the stack of dispatch block stacks*/
    if (!current_disp_invokers.empty()) {
#if DEBUG_THREAD_DIVIDER
        mtx.lock();
		LOG_S(INFO) << "End of Block" << std::hex << invoke_event->get_tid() << " "\
         			<< " current_disp_invokers size " << current_disp_invokers.size() << std::endl\
					<< "\t invoke_block end at "<< std::fixed << std::setprecision(1)\
					<< invoke_event->get_abstime() << std::endl\
         			<< "\twhich begins from " << std::fixed << std::setprecision(1)\
					<< current_disp_invokers.top()->get_abstime() << std::endl;
        mtx.unlock();
#endif
    } else {
        //should not happened in the process of dividing, as it has checked in the process of parsing.
        mtx.lock(); 
       	LOG_S(INFO) << "[error] empty stack: no begin event for block " << std::fixed << std::setprecision(1)\
					<< invoke_event->get_abstime() << " tid = "<< std::hex << invoke_event->get_tid() << std::endl;
        mtx.unlock();
        //assert(!current_disp_invokers.empty());
        add_general_event_to_group(invoke_event);
        return;
    }

    if (!(begin_invoke == current_disp_invokers.top())) {
        //should not happened in the process of dividing, as it has checked in the process of parsing.
        mtx.lock();
        LOG_S(INFO) << "[error] matching at matching\t"\
				 	<< "\t block end " << std::hex << std::fixed << std::setprecision(1)\
        			<< invoke_event->get_abstime() << std::endl\
        			<< "\tTop of stack is at" << std::hex << std::fixed << std::setprecision(1)\
        			<< current_disp_invokers.top()->get_abstime() << std::endl;
        mtx.unlock();

        assert(begin_invoke == current_disp_invokers.top());
    }

    current_disp_invokers.pop();

    /*2. dispatch_mig_server inside block */
    if (!dispatch_mig_servers.empty()) {
        DispatchQueueMIGServiceEvent *current_mig_server = dispatch_mig_servers.top();
        if (current_mig_server->get_mig_invoker() == begin_invoke) {
            //cur_group = (Group *)(current_mig_server->restore_owner());
            //assert(cur_group && cur_group->get_blockinvoke_level() > 0);
            dispatch_mig_servers.pop();
        }
    }
    
    /*3. check the case dispatch block get broken*/
    if (cur_group == nullptr || gid2group(begin_invoke->get_group_id()) != cur_group) {
#if DEBUG_THREAD_DIVIDER
        mtx.lock();
        LOG_S(INFO) << "Invoke block is broken" << std::endl\
				<< "\tBlock ends at " << std::fixed << std::setprecision(1)\
				<< invoke_event->get_abstime() << std::endl\
        		<< "\tBlock begins at " << std::fixed << std::setprecision(1)\
				<< begin_invoke->get_abstime() << std::endl;
		if (cur_group != nullptr)
        	LOG_S(INFO) << "\tBlock begins in Group 0x"\
					<< std::hex << begin_invoke->get_group_id() << std::endl\
					<< "\tBlock ends in Group 0x"\
					<< std::hex << cur_group->get_group_id() << std::endl;
					
        mtx.unlock();
#endif
	} else { // (cur_group == gid2group(begin_invoke->get_group_id()))
		cur_group->blockinvoke_level_dec();
	}

    add_general_event_to_group(invoke_event);
    cur_group->add_group_tags(invoke_event->get_desc());

    if (begin_invoke->get_root()) {
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
		invoke_event->set_nested_level(begin_invoke->get_nested_level());
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
        LOG_S(INFO) << "Error: dispatch_mig_server called outside block invoke" \
        	<< " at " << std::fixed << std::setprecision(1) << event->get_abstime() \
        	<< ";\n\tunable to identify the end of dispatch_mig_server in such cases" << std::endl;
        mtx.unlock();
#endif
    }
}
