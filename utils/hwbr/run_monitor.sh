#!/bin/bash

#command -v pidof > /dev/null 2>&1 || { echo >&2 "pidof is required, but it is not installed. Aborting,"; exit 1; }

sp_pid=$1
dm_pid=$2

#sudo ./set_breakpointer $windowserver_pid "/System/Library/Frameworks/CoreGraphics.framework/Versions/A/CoreGraphics" "_gOutMsgPending" 0 "WO" "DWord"
#sudo ./set_breakpointer $app_pid "/System/Library/Frameworks/Carbon.framework/Versions/A/Frameworks/HIToolbox.framework/Versions/A/orig.HIToolbox" "__ZL28sCGEventIsMainThreadSpinning" 0 "WO" "Byte"
#sudo ./set_breakpointer $app_pid "/System/Library/Frameworks/Carbon.framework/Versions/A/Frameworks/HIToolbox.framework/Versions/A/orig.HIToolbox" "__ZL28sCGEventIsMainThreadSpinning" 0 "WO" "Byte"
sudo ./set_breakpointer $sp_pid "/System/Library/Frameworks/CoreGraphics.framework/Versions/A/CoreGraphics" "_gCGSWillReconfigureSeen" 1 "WO" "Byte"
sudo ./set_breakpointer $dm_pid "/System/Library/Frameworks/CoreGraphics.framework/Versions/A/CoreGraphics" "_gCGSWillReconfigureSeen" 1 "WO" "Byte"
