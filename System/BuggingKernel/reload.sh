#!/bin/sh -ex

FILE=PeekMsr.kext
sudo kextunload -b com.columbia.PeekMsr || true
sudo rm -rf ${FILE}
cp -R ./build/Release/${FILE} .
sudo chown -R root:wheel ${FILE}
sudo kextutil ${FILE}
cp ./build/Release/ClientPeekMsr ./msr
