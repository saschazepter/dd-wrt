#!/bin/sh
OLDPATH=$PATH
DATE=$(date +%m-%d-%Y)
DATE+="-r"
DATE+=$(svnversion -n pb42/src/router/httpd)
export PATH=/xfs/toolchains/toolchain-mips_24kc_gcc-13.1.0_musl/bin:$OLDPATH
#export PATH=/xfs/toolchains/toolchain-mips_gcc-4.3.3+cs_uClibc-0.9.30.1/usr/bin:$OLDPATH
#export PATH=/xfs/toolchains/toolchain-mips_gcc-4.1.2-uClibc-0.9.30.1/usr/bin:$OLDPATH
#export PATH=/xfs/toolchains/staging_dir_mips_pb42/bin:$OLDPATH
cd pb42/src/router
[ -n "$DO_UPDATE" ] && svn update
cd opt/etc/config
[ -n "$DO_UPDATE" ] && svn update
cd ../../../
mkdir -p ~/GruppenLW/releases/$DATE/dlink-dap3310

cp .config_dap34xx .config
echo "CONFIG_UBNTXW=y" >> .config
echo "CONFIG_DAP3410=y" >> .config
echo "CONFIG_DAP3310=y" >> .config

make -f Makefile.pb42 kernel clean all install
cd ../../../
cp pb42/src/router/mips-uclibc/ar7420-firmware.bin ~/GruppenLW/releases/$DATE/dlink-dap3310/dap3310-dd-wrt-webupgrade.bin
cp pb42/src/router/mips-uclibc/dap3310.bin ~/GruppenLW/releases/$DATE/dlink-dap3310/factory-to-ddwrt.bin
cp notes/dap3310/* ~/GruppenLW/releases/$DATE/dlink-dap3310

