#!/usr/bin/bash
#
# Donnie. V. Savage
# 10 Nov, 2021
#

cd ~/devel/frr

if [ "x$1" = "x-config" ]; then
    shift
   ./bootstrap.sh
   ./configure \
       --localstatedir=/var/opt/frr \
       --sbindir=/usr/lib/frr \
       --sysconfdir=/etc/frr \
       --enable-multipath=64 \
       --enable-user=frr \
       --enable-group=frr \
       --enable-vty-group=frrvty \
       --enable-configfile-mask=0640 \
       --enable-logfile-mask=0640 \
       --enable-fpm \
       --with-pkg-git-version \
       --with-pkg-extra-version=

elif
    make -j 8
fi

