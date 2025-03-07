#!/bin/sh
OLDPATH=$PATH
DATE=$(date +%m-%d-%Y)
DATE+="-r"
DATE+=$(svnversion -n pb42/src/router/httpd)
export PATH=/xfs/toolchains/toolchain-mips_24kc_gcc-13.1.0_musl/bin:$OLDPATH
#export PATH=/xfs/toolchains/toolchain-mips_gcc-4.3.3+cs_uClibc-0.9.30.1/usr/bin:$OLDPATH
cd pb42/src/router
[ -n "$DO_UPDATE" ] && svn update
cd opt/etc/config
[ -n "$DO_UPDATE" ] && svn update
cd ../../../
mkdir -p ~/GruppenLW/releases/$DATE/netgear-wndr3700v4
mkdir -p ~/GruppenLW/releases/$DATE/netgear-wndr4300

cp .config_wndr3700v4 .config
echo "CONFIG_WPA3=y" >> .config
echo "CONFIG_MAC80211_MESH=y" >> .config
echo "CONFIG_CAKE=y" >> .config
echo "CONFIG_RAID=y" >> .config
echo "CONFIG_SMBD=y" >> .config
echo "CONFIG_SMARTDNS=y" >> .config
echo "CONFIG_MDNS=y" >> .config
make -f Makefile.pb42 kernel clean all install
make -f Makefile.pb42 wndr
make -f Makefile.pb42 wndr
cd ../../../
cp pb42/src/router/mips-uclibc/wndr3700v4-webflash.bin ~/GruppenLW/releases/$DATE/netgear-wndr3700v4/wndr3700v4-webflash.bin
cp pb42/src/router/mips-uclibc/wndr3700v4-factory.img ~/GruppenLW/releases/$DATE/netgear-wndr3700v4

cd pb42/src/router
cp .config_wndr3700v4 .config
echo "CONFIG_WNDR4300=y" >> .config
echo "CONFIG_MAC80211_MESH=y" >> .config
echo "CONFIG_WPA3=y" >> .config
echo "CONFIG_RAID=y" >> .config
echo "CONFIG_CAKE=y" >> .config
echo "CONFIG_SMBD=y" >> .config
echo "CONFIG_SMARTDNS=y" >> .config
echo "CONFIG_MDNS=y" >> .config

make -f Makefile.pb42 libutils-clean libutils install
make -f Makefile.pb42 wndr
cd ../../../

cp pb42/src/router/mips-uclibc/wndr3700v4-webflash.bin ~/GruppenLW/releases/$DATE/netgear-wndr4300/wndr4300-webflash.bin
cp pb42/src/router/mips-uclibc/wndr4300-factory.img ~/GruppenLW/releases/$DATE/netgear-wndr4300
