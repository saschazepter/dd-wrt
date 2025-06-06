#!/bin/sh

OLDPATH=$PATH
DATE=$(date +%m-%d-%Y)
DATE+="-r"
DATE+=$(svnversion -n ar531x/src/router/httpd)
export PATH=/xfs/toolchains/toolchain-mips_mips32_gcc-8.2.0_musl/bin:$OLDPATH
cd ar531x/src/router
[ -n "$DO_UPDATE" ] && svn update
cd opt/etc/config
[ -n "$DO_UPDATE" ] && svn update
cd ../../../

cp .config_dir300 .config
make -f Makefile.ar531x build_date
make -f Makefile.ar531x kernel clean all install
mkdir -p ~/GruppenLW/releases/$DATE/dlink-dir300
cd ../../../
cp ar531x/src/router/mips-uclibc/vmlinux.dir300 ~/GruppenLW/releases/$DATE/dlink-dir300/linux.bin
cp ar531x/src/router/mips-uclibc/dir300-firmware.bin ~/GruppenLW/releases/$DATE/dlink-dir300/dir300-firmware.bin

cd ar531x/src/router
cp .config_ar430w .config
make -f Makefile.ar531x build_date
make -f Makefile.ar531x libutils-clean shared-clean libutils shared install
mkdir -p ~/GruppenLW/releases/$DATE/airlink101-ar430w
cd ../../../

cp ar531x/src/router/mips-uclibc/vmlinux.dir300 ~/GruppenLW/releases/$DATE/airlink101-ar430w/linux.bin
cp ar531x/src/router/mips-uclibc/dir300-firmware.bin ~/GruppenLW/releases/$DATE/airlink101-ar430w/ar430w-firmware.bin


