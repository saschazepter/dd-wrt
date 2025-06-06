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
cp .config_dir862 .config
echo "CONFIG_DAP3662=y" >> .config
echo "CONFIG_DAP2330=y" >> .config
echo "CONFIG_DAP2660=y" >> .config
sed -i 's/CONFIG_USB=y/# CONFIG_USB is not set/g' .config
sed -i 's/CONFIG_NTFS3G=y/# CONFIG_NTFS3G is not set/g' .config
sed -i 's/CONFIG_MINIDLNA=y/# CONFIG_MINIDLNA is not set/g' .config
sed -i 's/CONFIG_SAMBA3=y/# CONFIG_SAMBA3 is not set/g' .config
sed -i 's/CONFIG_UQMI=y/# CONFIG_UQMI is not set/g' .config
sed -i 's/CONFIG_NOMESSAGE=y/# CONFIG_NOMESSAGE is not set/g' .config
echo "CONFIG_OPTIMIZE_FOR_SPEED=y" >> .config

make -f Makefile.pb42 kernel clean all install
cd ../../../
mkdir -p ~/GruppenLW/releases/$DATE/dlink-dap2660
cp pb42/src/router/mips-uclibc/web-dap2660.img ~/GruppenLW/releases/$DATE/dlink-dap2660/factory-to-ddwrt.bin
cp pb42/src/router/mips-uclibc/web-dap2660-enc.img ~/GruppenLW/releases/$DATE/dlink-dap2660/factory-to-ddwrt-enc.bin
cp pb42/src/router/mips-uclibc/webflash-dap2660.trx ~/GruppenLW/releases/$DATE/dlink-dap2660/dap2660-webflash.bin
