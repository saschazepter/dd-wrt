#
# Copyright (c) 2002-2006 Sam Leffler, Errno Consulting
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer,
#    without modification.
# 2. Redistributions in binary form must reproduce at minimum a disclaimer
#    similar to the "NO WARRANTY" disclaimer below ("Disclaimer") and any
#    redistribution must be conditioned upon including a substantially
#    similar Disclaimer requirement for further binary redistribution.
# 3. Neither the names of the above-listed copyright holders nor the names
#    of any contributors may be used to endorse or promote products derived
#    from this software without specific prior written permission.
#
# Alternatively, this software may be distributed under the terms of the
# GNU General Public License ("GPL") version 2 as published by the Free
# Software Foundation.
#
# NO WARRANTY
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF NONINFRINGEMENT, MERCHANTIBILITY
# AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
# THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR SPECIAL, EXEMPLARY,
# OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
# IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
# THE POSSIBILITY OF SUCH DAMAGES.
#
# $Id: //depot/sw/branches/sam_hal/public/x86_64-elf.inc#1 $
#

#
# Compilation configuration for building x86-64-elf.
#

#
ifndef TOOLPREFIX
#TOOLPREFIX=	/pub/gnu/bin/x86_64-linux-
endif
#
CC=	${TOOLPREFIX}gcc
LD=	${TOOLPREFIX}ld
STRIP=	${TOOLPREFIX}strip
OBJCOPY=${TOOLPREFIX}objcopy
NM=	${TOOLPREFIX}nm

COPTS+=	-DAH_BYTE_ORDER=AH_LITTLE_ENDIAN 
#
# NB: this should come from inttypes.h but can't until we cleanp
#     the definition of va_list on linux
#
COPTS+=	-DAH_WORDSIZE=64
COPTS+=	-mcmodel=kernel -mno-red-zone -Os -DBOESE=1
ifndef CONFIG_FRAME_POINTER
COPTS+=	-fomit-frame-pointer
endif
AH_SUPPORT_AR5210=1
AH_SUPPORT_AR5211=1
AH_SUPPORT_AR5212=1
AH_SUPPORT_AR5312=0
AH_SUPPORT_5111=1
AH_SUPPORT_5112=1
AH_SUPPORT_2413=1
AH_SUPPORT_5413=1
AH_SUPPORT_2316=0
AH_SUPPORT_2317=0
#AH_SUPPORT_DFS=1
AH_SUPPORT_XR=1