#!/bin/sh
OLDPATH=$PATH
DATE=$(date +%m-%d-%Y)
DATE+="-r"
DATE+=$(svnversion -n pb42/src/router/httpd)
export PATH=/xfs/toolchains/toolchain-mips_24kc_gcc-13.1.0_musl/bin:$OLDPATH
#export PATH=/xfs/toolchains/toolchain-mips_gcc-4.3.3+cs_uClibc-0.9.30.1/usr/bin:$OLDPATH
#export PATH=/xfs/toolchains/staging_dir_mips_pb42/bin:$OLDPATH
cd pb42/src/router
[ -n "$DO_UPDATE" ] && svn update
cd opt/etc/config
[ -n "$DO_UPDATE" ] && svn update
cd ../../../
cp .config_tg2521 .config
make -f Makefile.pb42 kernel clean all install
mkdir -p ~/GruppenLW/releases/$DATE/zcom-tg2521
cd ../../../
cp pb42/src/router/mips-uclibc/zcom-firmware.img ~/GruppenLW/releases/$DATE/zcom-tg2521/tg2521-factory-to-ddwrt-firmware.img
cp pb42/src/router/mips-uclibc/zcom-firmware.bin ~/GruppenLW/releases/$DATE/zcom-tg2521/tg2521-firmware.bin

