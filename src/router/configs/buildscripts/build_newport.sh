#!/bin/sh
OLDPATH=$PATH
DATE=$(date +%m-%d-%Y)
DATE+="-r"
DATE+=$(svnversion -n thunderx/src/router/libutils)
#export PATH=/xfs/toolchains/toolchain-laguna-new/bin:$OLDPATH
export PATH=/xfs/toolchains/toolchain-aarch64_generic_gcc-13.1.0_musl/bin:$OLDPATH
cd thunderx/src/router
[ -n "$DO_UPDATE" ] && svn update
cp configs/thunderx/.config_newport .config
echo "CONFIG_IRQBALANCE=y" >> .config
#echo "CONFIG_RSYNC=y" >> .config
echo "CONFIG_MAC80211_MESH=y" >> .config
echo "CONFIG_WPA3=y" >> .config
echo "CONFIG_CAKE=y" >> .config
#echo "CONFIG_SAMBA4=y" >> .config
echo "CONFIG_SMBD=y" >> .config
echo "CONFIG_SMARTDNS=y" >> .config
echo "CONFIG_NFS=y" >> .config
echo "CONFIG_MAC80211_RT2800=y" >> .config
echo "CONFIG_MAC80211_RTLWIFI=y" >> .config
echo "CONFIG_MT7615=y" >> .config
echo "CONFIG_MT7662=y" >> .config
echo "CONFIG_MT7921=y" >> .config
echo "CONFIG_MT7915=y" >> .config
echo "CONFIG_BRCMFMAC=y" >> .config
#echo "CONFIG_IWLWIFI=y" >> .config
echo "CONFIG_ATH11K=y" >> .config
echo "CONFIG_MRP=y" >> .config
echo "CONFIG_CFM=y" >> .config
echo "CONFIG_MDNS=y" >> .config
echo "CONFIG_MDNS_UTILS=y" >> .config
echo "CONFIG_WIREGUARD=y" >> .config
#echo "CONFIG_ZFS=y" >> .config
#echo "CONFIG_STRACE=y" >> .config
echo "CONFIG_WPA3=y" >> .config
echo "CONFIG_TDMA=y" >> .config
echo "CONFIG_RAID=y" >> .config
echo "CONFIG_EXFAT=y" >> .config
echo "CONFIG_NTFSPROGS=y" >> .config
#echo "CONFIG_RSYNC=y" >> .config
echo "CONFIG_DOSFSTOOLS=y" >> .config
echo "CONFIG_SMARTMONTOOLS=y" >> .config
#echo "CONFIG_FLASHROM=y" >> .config
echo "CONFIG_CAKE=y" >> .config
#echo "CONFIG_SAMBA4=y" >> .config
echo "CONFIG_SMBD=y" >> .config
echo "CONFIG_APFS=y" >> .config
echo "CONFIG_SMARTDNS=y" >> .config
echo "CONFIG_NGINX=y" >> .config
echo "CONFIG_MRP=y" >> .config
echo "CONFIG_CFM=y" >> .config
echo "CONFIG_HTOP=y" >> .config
echo "CONFIG_IPSET=y" >> .config
echo "CONFIG_NTFS3=y" >> .config
echo "CONFIG_QCA9984=y" >> .config
#echo "CONFIG_PLEX=y" >> .config
echo "KERNELVERSION=6.1" >> .config
#echo "CONFIG_PLEX=y" >> .config

#echo "CONFIG_LOCKDEBUG=y" >> .config
make -f Makefile.thunderx kernel clean all install

mkdir -p ~/GruppenLW/releases/$DATE/gateworks-newport-gw6xxx
cd ../../../
cp thunderx/src/router/aarch64-uclibc/newport.img.gz ~/GruppenLW/releases/$DATE/gateworks-newport-gw6xxx
cp thunderx/src/router/aarch64-uclibc/newport-webupgrade.bin ~/GruppenLW/releases/$DATE/gateworks-newport-gw6xxx/newport-webflash.bin



