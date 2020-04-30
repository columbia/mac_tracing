#!/bin/bash
#NSAPPLICATION_RECEIVED_KEVENT
noise=("Begin the next line")

invalid=("TRACE_LOST_EVENTS")

IFS='|'
exps="${noise[*]}"
invals="${invalid[*]}"

UTILS_PATH=~/trace_logs/utils/

read -s -p "Password for root:" PASS
echo ""

function parse_raw_trace_file()
{
	echo $PASS | sudo -S $UTILS_PATH/ring_trace -R $1 -o tmpfile.tmp
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
	#[ -e "tmpfile.tmp.tail" ] && cat "tmpfile.tmp.tail" | egrep "$keys" | egrep -v "$exps" > $2 
	[ -e "tmpfile.tmp.tail" ] && cat "tmpfile.tmp.tail" | egrep -v "$exps" > $2 
	[[ $? == 0 ]] && echo $PASS | sudo -S rm "tmpfile.tmp.tail"

	echo "store trace data head part 2"
	#[ -e "tmpfile.tmp" ] && cat "tmpfile.tmp" | egrep "$keys" | egrep -v "$exps" >> $2
	[ -e "tmpfile.tmp" ] && cat "tmpfile.tmp" | egrep -v "$exps" >> $2
	[[ $? == 0 ]] && echo $PASS | sudo -S rm "tmpfile.tmp"

	echo "check log file 3"
	[ -e $2 ] && echo "File processing done successfully" && egrep "$invals" $2
	echo "log checking done"
}


function prepare()
{
	mkdir -p $1/output
	mkdir -p $1/input/raw_parse
	mkdir -p $1/input/libs

	cp -r $2* $1/input/raw_parse
	cd $1/input/raw_parse
	cp -r ./$2_libs/* ../libs/
	
	let err=$?
	if [[ "$err" == 0 ]]; then
		echo "get libs for backtrace successfully"
	else
		echo "fail to get libs for backtrace with err code $err"
	fi

	parse_raw_trace_file $2.trace $2.log

	cp $2.log ../
	rm $2.log
	let err=$?
	if [[ "$err" == 0 ]]; then
		echo "get trace data successfully"
	else
		echo "fail to get trace data with err code $err"
	fi

	if [ -f "$2.trace_tpcmaps" ]; then
		cp $2.trace_tpcmaps ../tpcmap.log
		echo $PASS | sudo -S rm $2.trace_tpcmaps
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
	cp -r $UTILS_PATH/ ./

	echo $3 > input/required_libs

	if [ -e "check_graph" ]; then
		echo $PASS | sudo -S ./check_graph input/$2.log $3 $4 2>&1 | tee err.log
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
