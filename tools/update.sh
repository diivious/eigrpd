#!/usr/bin/bash
#
# Donnie. V. Savage
# 10 Nov, 2021
#

if [ "x$1" = "x-all" ]; then
    echo Updating FRR-ORIG
    cd ~/devel/frr-orig
    git fetch
    git pull
fi

echo Updating EIGRPD
cd ~/devel/eigrpd
    git fetch
    git pull

echo Updating FRR
cd ~/devel/frr
    rm -rf eigrpd
    git fetch
    git pull

echo Copying EIGRPD
cd ~/devel/frr
    rm -rf eigrpd
    cp -r ../eigrpd .

