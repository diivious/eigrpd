#
# Donnie. V. Savage
# 16 December, 2004
#

if [[ "$EUID" = 0 ]]; then
    echo "Do not run as root"
    exit
fi

echo Install DEVTOOLS
sudo apt-get install git autoconf automake libtool make \
  libreadline-dev texinfo libjson-c-dev pkg-config bison flex \
  libc-ares-dev python3-dev python3-pytest python3-sphinx build-essential \
  libsnmp-dev libcap-dev libelf-dev

echo Install LIBYANG2
cd ~/Downloads

wget 'https://ci1.netdef.org/artifact/LIBYANG-LIBYANGV2/shared/build-5/Debian-10-x86_64-Packages/libyang-tools_2.0.7-1%7Edeb10u1_all.deb'
wget 'https://ci1.netdef.org/artifact/LIBYANG-LIBYANGV2/shared/build-5/Debian-10-x86_64-Packages/libyang2-dev_2.0.7-1%7Edeb10u1_amd64.deb'
wget 'https://ci1.netdef.org/artifact/LIBYANG-LIBYANGV2/shared/build-5/Debian-10-x86_64-Packages/libyang2_2.0.7-1%7Edeb10u1_amd64.deb'

sudo apt install ./libyang2-dev_2.0.7-1~deb10u1_amd64.deb
sudo apt install ./libyang-tools_2.0.7-1~deb10u1_all.deb 
sudo apt install ./libyang2_2.0.7-1~deb10u1_amd64.deb

echo Install GROUPS
sudo addgroup --system --gid 92 frr
sudo addgroup --system --gid 85 frrvty
sudo adduser --system --ingroup frr --home /var/opt/frr/ --gecos "FRR suite" --shell /bin/false frr
sudo usermod -a -G frrvty frr

echo Clone REPOS
cd ~/devel

git clone git@github.com:diivious/eigrpd.git
git clone https://github.com/frrouting/frr.git frr
git clone https://github.com/frrouting/frr.git frr-orig

echo Config FRR
cd ~/devel/frr
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

echo Build FRR
make -j 8

echo Check FRR
make check

echo install FRR
sudo make install

echo Create CONFIGS
sudo install -m 755 -o frr -g frr -d /var/log/frr
sudo install -m 755 -o frr -g frr -d /var/opt/frr
sudo install -m 775 -o frr -g frrvty -d /etc/frr
sudo install -m 640 -o frr -g frr /dev/null /etc/frr/zebra.conf
sudo install -m 640 -o frr -g frr /dev/null /etc/frr/bgpd.conf
sudo install -m 640 -o frr -g frr /dev/null /etc/frr/ospfd.conf
sudo install -m 640 -o frr -g frr /dev/null /etc/frr/ospf6d.conf
sudo install -m 640 -o frr -g frr /dev/null /etc/frr/isisd.conf
sudo install -m 640 -o frr -g frr /dev/null /etc/frr/ripd.conf
sudo install -m 640 -o frr -g frr /dev/null /etc/frr/ripngd.conf
sudo install -m 640 -o frr -g frr /dev/null /etc/frr/pimd.conf
sudo install -m 640 -o frr -g frr /dev/null /etc/frr/ldpd.conf
sudo install -m 640 -o frr -g frr /dev/null /etc/frr/nhrpd.conf
sudo install -m 640 -o frr -g frrvty /dev/null /etc/frr/vtysh.conf
sudo install -m 640 -o frr -g frr /dev/null /etc/frr/eigrpd.conf
