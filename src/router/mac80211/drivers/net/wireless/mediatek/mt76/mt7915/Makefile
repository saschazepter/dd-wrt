# SPDX-License-Identifier: ISC

EXTRA_CFLAGS += -DCONFIG_MT76_LEDS
obj-$(CPTCFG_MT7915E) += mt7915e.o

mt7915e-y := pci.o init.o dma.o eeprom.o main.o mcu.o mac.o \
	     debugfs.o mmio.o

mt7915e-$(CPTCFG_NL80211_TESTMODE) += testmode.o
mt7915e-$(CPTCFG_MT7986_WMAC) += soc.o
mt7915e-$(CONFIG_DEV_COREDUMP) += coredump.o
