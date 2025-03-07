#!/bin/sh
OLDPATH=$PATH
DATE=$(date +%m-%d-%Y)
DATE+="-r"
DATE+=$(svnversion -n rt2880/src/router/httpd)
export PATH=/xfs/toolchains/toolchain-mipsel_24kc_gcc-13.1.0_musl/bin:$OLDPATH
#export PATH=/xfs/toolchains/toolchain-mipsel_mips32_gcc-8.2.0_musl/bin:$OLDPATH
cd rt2880/src/router
[ -n "$DO_UPDATE" ] && svn update
cd opt/etc/config
[ -n "$DO_UPDATE" ] && svn update
cd ../../../
cp .config_dir600 .config
echo "CONFIG_DIR615=y" >> .config
#echo "CONFIG_WIREGUARD=y" >> .config
mkdir -p ~/GruppenLW/releases/$DATE/dlink-dir615d
make -f Makefile.rt2880 kernel clean all install
cd ../../../
cp rt2880/src/router/mipsel-uclibc/rt2880-webflash.bin ~/GruppenLW/releases/$DATE/dlink-dir615d/dir615d-ddwrt-webflash.bin
cp rt2880/src/router/mipsel-uclibc/dir615d-web.bin ~/GruppenLW/releases/$DATE/dlink-dir615d/dlink-dir615d-factory-webflash.bin
