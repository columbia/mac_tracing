#!/bin/bash

command -v pidof > /dev/null 2>&1 || { echo >&2 "pidof is required, but it is not installed. Aborting,"; exit 1; }
if [ "$#" -ne 1 ]; then
	echo "Usage: $0 Process" >&2
	exit 1
fi

log="setbr.log"
windowserver_pid=`pidof WindowServer`
app_pids=`pidof $1`
app_pid=`echo $app_pids | awk '{print $1}'`

sudo ./set_breakpointer $windowserver_pid "/System/Library/Frameworks/CoreGraphics.framework/Versions/A/CoreGraphics" "_gOutMsgPending" 0 "WO" "DWord"
sudo ./set_breakpointer $app_pid "/System/Library/Frameworks/Carbon.framework/Versions/A/Frameworks/HIToolbox.framework/Versions/A/orig.HIToolbox" "__ZL28sCGEventIsMainThreadSpinning" 0 "WO" "Byte"
sudo ./set_breakpointer $app_pid "/System/Library/Frameworks/Carbon.framework/Versions/A/Frameworks/HIToolbox.framework/Versions/A/orig.HIToolbox" "__ZL32sCGEventIsDispatchedToMainThread" 1 "WO" "Byte"


echo -e "Hardware Breakpoints are set at: $(date)" >> setbr.log
echo -e "$windowserver_pid\tWindowServer\t_gOutMsgPending\t0\tWO\tDword" >> setbr.log
echo -e "$app_pid\t$1\t__ZL28sCGEventIsMainThreadSpinning\t0\tWO\tByte" >> setbr.log
echo -e "$app_pid\t$1\t__ZL32sCGEventIsDispatchedToMainThread\t0\tWO\tByte" >> setbr.log
echo -e "====================================================================================" >> setbr.log
