#
# Makefile for the LZMA compressed kernel loader for
# Atheros AR7XXX/AR9XXX based boards
#
# Copyright (C) 2011 Gabor Juhos <juhosg@openwrt.org>
#
# Some parts of this file was based on the OpenWrt specific lzma-loader
# for the BCM47xx and ADM5120 based boards:
#	Copyright (C) 2004 Manuel Novoa III (mjn3@codepoet.org)
#	Copyright (C) 2005 Mineharu Takahara <mtakahar@yahoo.com>
#	Copyright (C) 2005 by Oleg I. Vdovikin <oleg@cs.msu.su>
#
# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License version 2 as published
# by the Free Software Foundation.
#

LOADADDR	:= 0x80220000
LZMA_TEXT_START	:= 0x82000000
LOADER_DATA	:=
BOARD		:=
FLASH_START	:=
FLASH_OFFS	:=
FLASH_MAX	:=
PLATFORM	:=
SUBTARGET	:=
CACHE_FLAGS	:= -DCONFIG_CACHELINE_SIZE=32


CC		:= $(CROSS_COMPILE)gcc
LD		:= $(CROSS_COMPILE)ld
OBJCOPY		:= $(CROSS_COMPILE)objcopy
OBJDUMP		:= $(CROSS_COMPILE)objdump


BIN_FLAGS	:= -O binary -R .reginfo -R .note -R .comment -R .mdebug \
		   -R .MIPS.abiflags -S

CFLAGS		= -D__KERNEL__ -Wall -Wstrict-prototypes -Wno-trigraphs -Os \
		  -fno-strict-aliasing -fno-common -fomit-frame-pointer -G 0 \
		  -mno-abicalls -fno-pic -ffunction-sections -pipe -mlong-calls \
		  -fno-common -ffreestanding -fhonour-copts -nostartfiles \
		  -mabi=32 -march=mips32r2 \
		  -Wa,-32 -Wa,-march=mips32r2 -Wa,-mips32r2 -Wa,--trap
CFLAGS		+= -D_LZMA_PROB32
CFLAGS		+= $(LTO)
CFLAGS		+= $(CACHE_FLAGS)

ASFLAGS		= $(CFLAGS) -D__ASSEMBLY__

LDFLAGS		= -static -Wl,--gc-sections -Wl,-no-warn-mismatch $(LTO) $(LDLTO)
LDFLAGS		+= -Wl,-e,startup -T loader.lds -Wl,-Ttext,$(LZMA_TEXT_START)
LDFLAGS		+= $(LTO) $(LDLTO) -Wl,-z,max-page-size=4096 $(LTO)

O_FORMAT 	= $(shell $(OBJDUMP) -i | head -2 | grep elf32)

OBJECTS		:= head.o loader.o cache.o board.o LzmaDecodeSize.o

ifeq ($(strip $(SUBTARGET)),)
$(error "Please specify a SUBTARGET!")
endif

ifeq ($(strip $(SUBTARGET)),rtl838x)
CFLAGS	+= -DSOC_RTL838X
endif

ifeq ($(strip $(SUBTARGET)),rtl839x)
CFLAGS	+= -DSOC_RTL839X
endif

ifeq ($(strip $(SUBTARGET)),rtl930x)
CFLAGS	+= -DSOC_RTL930X
endif

ifeq ($(strip $(SUBTARGET)),rtl931x)
CFLAGS	+= -DSOC_RTL931X
endif

ifneq ($(strip $(LOADER_DATA)),)
OBJECTS		+= data.o
CFLAGS		+= -DLZMA_WRAPPER=1 -DLOADADDR=$(LOADADDR)
endif

ifneq ($(strip $(KERNEL_CMDLINE)),)
CFLAGS		+= -DCONFIG_KERNEL_CMDLINE='"$(KERNEL_CMDLINE)"'
endif

ifneq ($(strip $(FLASH_START)),)
CFLAGS		+= -DCONFIG_FLASH_START=$(FLASH_START)
endif

ifneq ($(strip $(FLASH_OFFS)),)
CFLAGS		+= -DCONFIG_FLASH_OFFS=$(FLASH_OFFS)
endif

ifneq ($(strip $(FLASH_MAX)),)
CFLAGS		+= -DCONFIG_FLASH_MAX=$(FLASH_MAX)
endif

all: loader.elf

# Don't build dependencies, this may die if $(CC) isn't gcc
dep:

install:

cache.o : cache.c
	$(CC) $(CFLAGS) -c -o $@ $<

%.o : %.c
	$(CC) $(CFLAGS) -c -o $@ $<

%.o : %.S
	$(CC) $(ASFLAGS) -c -o $@ $<

data.o: $(LOADER_DATA)
	$(LD) -r -b binary --oformat $(O_FORMAT) -T lzma-data.lds -o $@ $<

loader: $(OBJECTS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJECTS)

loader.bin: loader
	$(OBJCOPY) $(BIN_FLAGS) $< $@

loader2.o: loader.bin
	$(LD) -r -b binary --oformat $(O_FORMAT) -o $@ $<

loader.elf: loader2.o
	$(LD) -e startup -T loader2.lds -Ttext $(LOADADDR) -z max-page-size=4096 -o $@ $<

mrproper: clean

clean:
	rm -f loader *.elf *.bin *.o



