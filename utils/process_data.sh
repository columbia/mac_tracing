#!/bin/bash
#NSAPPLICATION_RECEIVED_KEVENT

events=("MACH_IPC_msg_" \
	"MACH_IPC_kmsg_free" \
	"MACH_IPC_voucher_info" \
	"MACH_IPC_voucher_"\
	"Bank_Account_redeem" \
	"MACH_MKRUNNABLE" \
	"MACH_WAKEUP_REASON" \
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
	"MACH_SCHED_REMOTE_AST" \
	"TRACE_LOST_EVENTS" \
	"Begin the next line")

noise=("MACH_WAITQ" \
	"Begin the next line")

IFS='|'
keys="${events[*]}"
exps="${noise[*]}"

RTPATH=~/trace_logs/

function parse_raw_trace_file()
{
	sudo $RTPATH/ring_trace -R $1 -o tmpfile.tmp
	echo "Read trace data " $?

	if [[ -f "tmpfile.tmp.tail" ]]; then
		line=$(head -n 1 "tmpfile.tmp.tail")
		if [[ $line == Begin* ]]; then
			echo $line
			echo "Trace buffer get wrapped"
		fi
	fi
		
	[ -e $2 ] && rm $2
	[ -e "tmpfile.tmp.tail" ] && cat "tmpfile.tmp.tail" | egrep "$keys" | egrep -v "$exps" > $2  && sudo rm "tmpfile.tmp.tail"
	[ -e "tmpfile.tmp" ] && cat "tmpfile.tmp" | egrep "$keys" | egrep -v "$exps" >> $2 && sudo rm "tmpfile.tmp"
	[ -e $2 ] && echo "File processing done successfully"
}

function prepare()
{
	mkdir -p $1/input/raw_parse
	cp -r $2* $1/input/raw_parse

	cd $1/input/raw_parse
	cp -r ./$2_libs ../libs
	echo "get libs for backtrace" $?
#	pwd
#	wslibfile=`grep -srn "DumpProcess" ./$2_libs | awk -F ":" '{if ($3 ~ /DumpProcess.*WindowServer/) {print $1; exit} }'`
#	if [ x$wslibfile != x ]; then
#		cat $wslibfile > ../libinfo.log
#		echo "write WindowServer vmmap from" $wslibfile " to libinfo " $?
#	else
#		echo "no windowserver lib"
#	fi
#
#	echo "proc is " $3
#	proc=$3$
#	grep -srn "DumpProcess" ./$2_libs | awk -F ":" -v myproc="DumpProcess.*$proc" '{if ($3 ~ myproc) {print $1} }' | while read -r line; do
#		echo "lib " $line
#		cat $line >> ../libinfo.log
#		echo "write " $3 " vmmap from " $line " to libinfo " $?
#	done

	parse_raw_trace_file $2.trace $2.log
	cp $2.log ../
	echo "get trace data" $?

	if [ -f "$2.trace_tpcmaps" ]; then
		cp $2.trace_tpcmaps ../tpcmap.log
		echo "get thread_process maps" $?
	fi
}

function analyze()
{
	cur_path=$PWD
	echo $cur_path
	prepare $1 $2
	cd $cur_path/$1
	pwd
	cp -r $RTPATH/utils/ ./
	set -x
	[ -e "analyzer" ] && sudo ./analyzer input/$2.log $3 $4 > err.log 2>&1
	echo "analyze " $?
	set +x
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
