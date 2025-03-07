USTEER_PKG_BUILD_DIR=$(TOP)/usteer
USTEER_STAGING_DIR=$(TOP)/_staging
USTEER_PKG_INSTALL:=1
USTEER_CMAKE_OPTIONS+=VERBOSE=0 -DBUILD_LUA=OFF \
		    -DCMAKE_BUILD_TYPE=release \
		    -DCMAKE_AR=${GCCAR} \
		    -DCMAKE_RANLIB=${GCCRANLIB}


USTEER_EXTRA_CFLAGS=-I$(TOP) -I$(STAGING_DIR)/usr/include -I$(TOP)/shared -L$(STAGING_DIR)/usr/lib -I$(TOP)/libnl-tiny/include  $(MIPS16_OPT) $(THUMB) -DNEED_PRINTF -ffunction-sections -fdata-sections -Wl,--gc-sections $(LTO)
USTEER_EXTRA_LDFLAGS=-ljson-c -L$(TOP)/libubox/ -L$(TOP)/libnl-tiny -lnl-tiny  -ffunction-sections -fdata-sections -Wl,--gc-sections $(LDLTO)

usteer-configure: libubox ubus
	$(call CMakeClean,$(USTEER_PKG_BUILD_DIR))
	$(call CMakeConfigure,$(USTEER_PKG_BUILD_DIR),$(USTEER_STAGING_DIR),$(USTEER_CMAKE_OPTIONS),$(USTEER_EXTRA_CFLAGS),$(USTEER_EXTRA_LDFLAGS),.) 

usteer: json-c libubox ubus
	$(MAKE) -C usteer
#	-install -D usteer/libusteer.so $(STAGING_DIR)/usr/lib/libusteer.so
#	-cp usteer/usteermsg.h $(STAGING_DIR)/usr/include/
#	-cp usteer/usteer_common.h $(STAGING_DIR)/usr/include/
#	-cp usteer/libusteer.h $(STAGING_DIR)/usr/include/

usteer-install:
#	install -D usteer/ap-monitor $(INSTALLDIR)/usteer/usr/sbin/ap-monitor
#	install -D usteer/fakeap $(INSTALLDIR)/usteer/usr/sbin/fakeap
	install -D usteer/usteerd $(INSTALLDIR)/usteer/usr/sbin/usteerd

usteer-clean:
	if [ -e "$(USTEER_PKG_BUILD_DIR)/Makefile" ]; then $(MAKE) -C usteer clean ; fi
