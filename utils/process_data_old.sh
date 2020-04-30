#!/bin/bash
#NSAPPLICATION_RECEIVED_KEVENT

events=("MACH_IPC_msg_" \
	"MACH_IPC_kmsg_free" \
	"MACH_IPC_voucher_info" \
	"MACH_IPC_voucher_"\
	"Bank_Account_redeem" \
	"MACH_MKRUNNABLE" \
	"MACH_WAKEUP_REASON" \
	"MACH_KNOTE" \
	"MACH_KQ_PROCESS" \
	"MACH_KEVENT_REGIS" \
	"MACH_KEVENT_PATH" \
	"MACH_KPATH_ERR" \
	"MACH_KN_ACT" \
	"INTERRUPT" \
	"wq__run_nextitem" \
	"MACH_TS_MAINTENANCE" \
	"MACH_WAIT" \
	"MACH_WAIT_REASON" \
	"dispatch_enqueue" \
	"dispatch_dequeue" \
	"dispatch_execute" \
	"dispatch_mig_server" \
	"MACH_CALLCREATE" \
	"MACH_CALLOUT" \
	"MACH_CALLCANCEL" \
	"MSG_Pathinfo" \
	"MSG_Backtrace" \
	"MSC_" \
	"BSC_" \
	"CALayerSet" \
	"NSViewSet" \
	"CALayerDisplay" \
	"HWBR_trap" \
	"RL_Observer" \
	"NSEvent" \
	"EventRef" \
	"NSAppGetEvent" \
	"RL_DoSource0" \
	"RL_DoSource1" \
	"RL_DoBlocks" \
	"RL_DoTimer" \
	"RL_DoObserver" \
	"MACH_SCHED_REMOTE_AST" \
	"TRACE_LOST_EVENTS" \
	"Begin the next line")

noise=("MACH_WAITQ" \
	"Begin the next line")

IFS='|'
keys="${events[*]}"
exps="${noise[*]}"

RTPATH=~/trace_logs/

read -s -p "Password for root:" PASS
echo ""

function parse_raw_trace_file()
{
	echo $PASS | sudo -S $RTPATH/ring_trace -R $1 -o tmpfile.tmp
	[[ $? == 0 ]] && echo "Finished reading trace data"

	if [[ -f "tmpfile.tmp.tail" ]]; then
		line=$(head -n 1 "tmpfile.tmp.tail")
		if [[ $line == Begin* ]]; then
			echo "Trace buffer get wrapped"
		fi
	fi

	echo "clear file"
	if [ -e $2 ]; then
		rm $2
	fi
	
	echo "store trace data tail part 1"
	[ -e "tmpfile.tmp.tail" ] && cat "tmpfile.tmp.tail" | egrep "$keys" | egrep -v "$exps" > $2 
	[[ $? == 0 ]] && echo $PASS | sudo -S rm "tmpfile.tmp.tail"

	echo "store trace data head part 2"
	[ -e "tmpfile.tmp" ] && cat "tmpfile.tmp" | egrep "$keys" | egrep -v "$exps" >> $2
	[[ $? == 0 ]] && echo $PASS | sudo -S rm "tmpfile.tmp"

	echo "check log file 3"
	[ -e $2 ] && echo "File processing done successfully"
}


function prepare()
{
	mkdir -p $1/output
	mkdir -p $1/input/raw_parse
	cp -r $2* $1/input/raw_parse
	cd $1/input/raw_parse
	cp -r ./$2_libs ../libs
	let err=$?
	if [[ "$err" == 0 ]]; then
		echo "get libs for backtrace successfully"
	else
		echo "fail to get libs for backtrace with err code $err"
	fi

	parse_raw_trace_file $2.trace $2.log

	cp $2.log ../
	let err=$?
	if [[ "$err" == 0 ]]; then
		echo "get trace data successfully"
	else
		echo "fail to get trace data with err code $err"
	fi

	if [ -f "$2.trace_tpcmaps" ]; then
		cp $2.trace_tpcmaps ../tpcmap.log
		let err=$?
		if [[ "$err" == 0 ]]; then
			echo "get thread process maps successfully"
		else
			echo "fail to get thread process maps with err code $err"
		fi
	fi
}

function analyze()
{
	cur_path=$PWD
	prepare $1 $2
	cd $cur_path/$1
	pwd
	cp -r $RTPATH/utils/ ./

	echo $3 > input/required_libs
	echo "WindowServer" >> input/required_libs

	if [ -e "analyzer" ]; then
		echo $PASS | sudo -S ./analyzer input/$2.log $3 $4 2>&1 | tee err.log
	fi
}

function usage()
{
	echo "$0 [dir_name] [datafile_prefix] [Process_name] [Live process pid]"
}

if [ "$#" -ne 4 ]; then
	usage
	exit 1
fi
analyze $1 $2 $3 $4
