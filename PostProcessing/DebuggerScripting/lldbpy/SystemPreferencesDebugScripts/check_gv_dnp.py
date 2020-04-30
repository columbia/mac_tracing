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

def get_value_from_expr(line):
	val = re.search('.*(0x[0-9a-fA-F]+?)$', line)
	ret = val.group(1)
	print "get_value_from_expr ", ret
	return int(ret, 16)

def run_commands(debugger, commands):
	return_obj = lldb.SBCommandReturnObject()
	command_interpreter = debugger.GetCommandInterpreter()
	ret = ''
	for command in commands:
		command_interpreter.HandleCommand(command, return_obj)
		if return_obj.Succeeded():
			ret = ret + return_obj.GetOutput() + '; '
		else:
			ret = ret + command + ' failed '
	return ret	

def run_thread_return(debugger):
	return run_commands(debugger, ['finish', 'register read rax'])

def run_step_in(debugger):
	thread_output = run_commands(debugger, ['thread step-in'])
	lines = thread_output.splitlines()
	symbol = extract_symbol_from_thread_info(lines[1])
	return symbol, lines[1]
	
def run_step_inst(debugger):
	thread_output = run_commands(debugger, ['thread step-inst'])
	lines = thread_output.splitlines()
	asm_code = lines[4]
	symbol = extract_symbol_from_thread_info(lines[1])
	return symbol, lines[1], asm_code

def step_and_record(process, debugger, options):
	Symbol_prev = None
	cur_sym_step_in_count = 0
	Begin_record = 0
	count = 0
	while True:
		count += 1
		try:
			top_frame, thread_info, asm_code = run_step_inst(debugger)

			if Symbol_prev is None or top_frame != Symbol_prev:
				#if Symbol_prev:
					#print 'finish ', str(cur_sym_step_in_count), ' insts in ', Symbol_prev
				Symbol_prev = top_frame
				print 'step inst ' + top_frame
				cur_sym_step_in_count = 1
			else:
				cur_sym_step_in_count += 1

			#print asm_code
			if Begin_record == 1 and '$0x1' in asm_code and '0x11ae27' in asm_code:
				print top_frame
				print asm_code
				#ret = run_commands(debugger, ['register read rax rdi rsi rdx rcx r8 r9'])
				ret = run_commands(debugger, ['register read'])
				print ret

			#if 'CGSDatagramReadStream::dispatch_datagrams' in top_frame:
			if 'display_notify_proc' in top_frame and Begin_record == 0:
				Begin_record = 1
			if 'retq' in asm_code and 'display_notify_proc' in top_frame:
				Begin_record = 0

			if 'dylib' in thread_info or 'CoreFoundation' in thread_info:
				ret = run_thread_return(debugger)

			if options.normal_endf in top_frame or options.spin_endf in top_frame:
				print 'Break after ', count, ' insts'
				break

		except (KeyboardInterrupt, SystemExit):
			print 'EXCPT Break after ', count, ' insts'
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
		#default = 'CGSDatagramReadStream::dispatch_datagrams')
		default = '_NSIsWindowOnOrExpectedToBeOnSpace')
		#default='activeDisplayNotificationHandler')
		#default = 'notify_datagram_handler')
	parser.add_option('-n', '--normal_end',
		dest = 'normal_endf',
		type = 'string',
		help = 'End function in normal execution',
		metavar = 'NORMEND',
		#default = 'CGSManagedDisplayCurrentSpaceAllowsWindow')
		#default ='_displayReconfigured')
		default = 'CGSDatagramReadStreamDispatchDatagramsWithData')
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
	
