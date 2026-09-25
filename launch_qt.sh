#!/bin/sh
awk -F "calibrate=| laohua" '{print $2}' /proc/cmdline > /etc/cali

CALI=`cat /etc/cali`

if [ $CALI = "Y" ]
then
if [ ! -e /etc/pointercal ] ; then
        /usr/bin/ts_calibrate
        sync
fi
fi
export HOME=/forlinx/qt

cd /forlinx/web/lighttpd/sbin
./lighttpd -f ../config/lighttpd.conf

SEVNOCON=`cat /media/sda1/NoConn`
if [ $SEVNOCON = "Y" ]; then
cd /etc/
touch 7StampNoConnect
fi

SEVUSB=`cat /media/sda1/UpdateCont`

if [ $SEVUSB = "Y" ]; then
cd /forlinx/qt/bin
./AppUpdater -qws 2>/dev/null &
else

SEVDEF=`cat /etc/7Default`
SEVUPD=`cat /etc/7StampUpdate`

if [ $SEVUPD = "Y" ]; then
cd /forlinx/qt/bin
./AppUpdater -qws 2>/dev/null &
elif [ $SEVDEF = "Y" ]; then
cd /forlinx/qt/bin
./7-1-stamp -qws 2>/dev/null &
else
cd /stampit
./7-1-stamp -qws 2>/dev/null &
fi
fi

