#!/bin/bash
import optparse
import os
import platform
import sys
import commands
import time
#import parse
import re

#----------------------------------------------------------------------
# Code that auto imports LLDB
# credit to llvm project examples
#----------------------------------------------------------------------
try:
    # Just try for LLDB in case PYTHONPATH is already correctly setup
    import lldb
except ImportError:
    lldb_python_dirs = list()
    # lldb is not in the PYTHONPATH, try some defaults for the current platform
    platform_system = platform.system()
    if platform_system == 'Darwin':
        # On Darwin, try the currently selected Xcode directory
        xcode_dir = commands.getoutput("xcode-select --print-path")
        if xcode_dir:
            lldb_python_dirs.append(
                os.path.realpath(
                    xcode_dir +
                    '/../SharedFrameworks/LLDB.framework/Resources/Python'))
            lldb_python_dirs.append(
                xcode_dir + '/Library/PrivateFrameworks/LLDB.framework/Resources/Python')
        lldb_python_dirs.append(
            '/System/Library/PrivateFrameworks/LLDB.framework/Resources/Python')
    success = False
    for lldb_python_dir in lldb_python_dirs:
        if os.path.exists(lldb_python_dir):
            if not (sys.path.__contains__(lldb_python_dir)):
                sys.path.append(lldb_python_dir)
                try:
                    import lldb
                except ImportError:
                    pass
                else:
                    print('imported lldb from: "%s"' % (lldb_python_dir))
                    success = True
                    break
    if not success:
        print("error: couldn't locate the 'lldb' module, please set PYTHONPATH correctly")
        sys.exit(1)

#----------------------------------------------
#
#----------------------------------------------

def extract_symbol_from_thread_info(line):
	##
	#extrace symbol from thread_info line lines[1]
	#* thread #1: tid = 0x6b39b, 0x00000001040ddd56 orig.AppKit`-[NSApplication run] + 640,\
	#   queue = 'com.apple.main-thread', stop reason = step out
	#* thread #1: tid = 0x8f0ea, 0x000000010aff623c orig.AppKit`_NSIsWindowOnOrExpectedToBeOnSpace + 1,\
	#	queue = 'com.apple.main-thread', stop reason = instruction step into
	##
	try:
		thread_symbol = re.search('.*[`](.+?)[+].*', line)
		if thread_symbol:
			return thread_symbol.group(1).strip()
		else:
			thread_symbol = re.search('.*[`](.+?), queue \=.*', line)
			if thread_symbol:
				return thread_symbol.group(1).strip()
			else:
				return 'UNKNOWN_SYM'
	except:
		return 'Fail to search symbol in ' + line

def extract_module_from_thread_info(line):
	pass

def run_commands(debugger, commands):
	return_obj = lldb.SBCommandReturnObject()
	command_interpreter = debugger.GetCommandInterpreter()
	for command in commands:
		command_interpreter.HandleCommand(command, return_obj)
		if return_obj.Succeeded():
			#print "command : ", command
			return return_obj.GetOutput()
		else:
			return commands + ' failed'

def run_thread_return(debugger):
	return_obj = lldb.SBCommandReturnObject()
	command_interpreter = debugger.GetCommandInterpreter()
	command_interpreter.HandleCommand('finish', return_obj)
	#command_interpreter.HandleCommand('thread return', return_obj)
	#thread_output = return_obj.GetOutput()
	#lines = thread_output.splitlines()
	#return lines[1]
	command_interpreter.HandleCommand('register read rax', return_obj)
	return return_obj.GetOutput()

def run_read_rax(debugger):
	return_obj = lldb.SBCommandReturnObject()
	command_interpreter = debugger.GetCommandInterpreter()
	command_interperter.HandleCommand('register read rax', return_obj)
	return return_obj.GetOutput()

def run_step_in(debugger):
	return_obj = lldb.SBCommandReturnObject()
	command_interpreter = debugger.GetCommandInterpreter()
	command_interpreter.HandleCommand('thread step-in', return_obj)
	thread_output = return_obj.GetOutput()
	lines = thread_output.splitlines()
	symbol = extract_symbol_from_thread_info(lines[1])
	return symbol, lines[1]
	
def run_step_inst(debugger):
	return_obj = lldb.SBCommandReturnObject()
	command_interpreter = debugger.GetCommandInterpreter()
	command_interpreter.HandleCommand('thread step-inst', return_obj)
	thread_output = return_obj.GetOutput()
	lines = thread_output.splitlines()
	asm_code = lines[4]
	symbol = extract_symbol_from_thread_info(lines[1])
	return symbol, lines[1], asm_code

def step_and_record(process, debugger, options):
	Symbol_prev = None
	cur_sym_step_in_count = 0
	#count = 1000
	count = 0
	while True:#count != 0:
		count += 1
		try:
			top_frame, thread_info, asm_code = run_step_inst(debugger)
			if '->  0x10558501b' in asm_code:
				
				print "WLM begin try to writ pc"
				ret = run_commands(debugger, ['register read'])
				print ret
				ret = run_commands(debugger, ['register write pc 0x105584fee'])
				print "WLM end try to writ pc ", ret
				ret = run_commands(debugger, ['register read'])
				print ret
				if ret :
					run_step_inst(debugger)
					run_step_inst(debugger)
					break;
			if Symbol_prev is None or top_frame != Symbol_prev:
				#if Symbol_prev:
					#print 'finish ', str(cur_sym_step_in_count), ' insts in ', Symbol_prev

				Symbol_prev = top_frame
				ret = run_commands(debugger, ['bt'])
				cur_sym_step_in_count = 1
				ret = run_commands(debugger, ['frame variable'])
			else:
				cur_sym_step_in_count += 1
			#thread_plan = lldb.SBThreadPlan()
			#start_frame = thread_plan.GetThread().GetFrameAtIndex(0)
			#stop_reason = thread_plan.GetThread().GetStopReason()
	
			if 'dylib' in thread_info or 'CoreFoundation' in thread_info:
				ret = run_thread_return(debugger)
				#print 'thread finish ' + ret
			if options.normal_endf in top_frame or options.spin_endf in top_frame:
				#print 'Break after ', count, ' insts'
				break

		except (KeyboardInterrupt, SystemExit):
			#process.Continue()
			#print 'EXCPT Break after ', count, ' insts'
			break;

		#except Exception as e:
			#print 'Error in loop:', str(e)
			#break
	
def set_breakpoint(debugger, options):
	debugger.HandleCommand("_regexp-break %s" % (options.breakpoint))
	ret = run_commands(debugger, ['breakpoint list'])
	print ret

def main(argv):
##
#--breakpoint begin_funcion --normal_end [end_function] --spin_end [end_function] 
#--pid attached_process
##
	parser = optparse.OptionParser()
	parser.add_option('-b', '--breakpoint',
		dest = 'breakpoint',
		type = 'string',
		help = 'Trace calls from the breakpoint',
		metavar = 'BPEXPR',
		#default = None)
		default = '_NSIsWindowOnOrExpectedToBeOnSpace')
	parser.add_option('-n', '--normal_end',
		dest = 'normal_endf',
		type = 'string',
		help = 'End function in normal execution',
		metavar = 'NORMEND',
		#default = 'CGSManagedDisplayCurrentSpaceAllowsWindow')
		default ='_displayReconfigured')
	parser.add_option('-s', '--spin_end',
		dest = 'spin_endf',
		type = 'string',
		help = 'End function in spinning execution',
		metavar = 'SPINEND',
		#default = 'CGSManagedDisplayCurrentSpaceAllowsWindow')
		default = 'CGSDatagramReadStreamDispatchDatagramsWithData')
	parser.add_option('-p', '--pid',
		dest = 'pid',
		type = 'int',
		help = 'Process to attach',
		metavar = 'PID',
		default = -1)
	(options, args) = parser.parse_args(argv)
	if options.pid == -1:
		sys.exit(1)

	debugger = lldb.SBDebugger.Create()
	debugger.SetAsync(False)
	command_interpreter = debugger.GetCommandInterpreter()

	#attach to target process
	attach_info = lldb.SBAttachInfo(options.pid)
	error = lldb.SBError()
	target = debugger.CreateTarget(None, 'x86_64', None, True, error)
	print 'Create target: ', error
	process = target.Attach(attach_info, error)
	print 'Attach to process: ', error

	if process and process.GetProcessID() != lldb.LLDB_INVALID_PROCESS_ID:
		pid = process.GetProcessID()
		print 'Process is ', pid
	else :
		print 'Fail to attach lldb to ', options.pid

	set_breakpoint(debugger, options)
	process.Continue()

	step_and_record(process, debugger, options)

	lldb.SBDebugger.Terminate()

if __name__ == '__main__':
	main(sys.argv[1:])
	
