TCOBJ= tc.o tc_qdisc.o tc_class.o tc_filter.o tc_util.o \
       m_police.o m_estimator.o m_action.o

include ../Config

TCMODULES :=
TCMODULES += q_fifo.o
TCMODULES += q_sfq.o
TCMODULES += q_red.o
TCMODULES += q_prio.o
TCMODULES += q_tbf.o
ifneq ($(KERNELVERSION),6.12)
TCMODULES += q_cbq.o
TCMODULES += f_rsvp.o
TCMODULES += f_tcindex.o
TCMODULES += q_dsmark.o
endif
#TCMODULES += q_wrr.o
TCMODULES += f_u32.o
TCMODULES += f_route.o
TCMODULES += f_fw.o
TCMODULES += q_gred.o
TCMODULES += q_ingress.o
TCMODULES += q_hfsc.o
TCMODULES += q_htb.o
ifeq ($(KERNELVERSION),3.10)
TCMODULES += q_multiq.o
TCMODULES += q_cake.o
TCMODULES += q_codel.o
TCMODULES += q_fq_codel.o
TCMODULES += q_fq_codel_fast.o
TCMODULES += q_pie.o
endif
ifeq ($(KERNELVERSION),3.18)
TCMODULES += q_multiq.o
TCMODULES += q_cake.o
TCMODULES += q_codel.o
TCMODULES += q_fq_codel.o
TCMODULES += q_fq_codel_fast.o
TCMODULES += q_fq.o
TCMODULES += q_pie.o
endif

ifeq ($(KERNELVERSION),4.4)
CFLAGS += -DCE_THRESHOLD
TCMODULES += q_multiq.o
TCMODULES += q_cake.o
TCMODULES += q_codel.o
TCMODULES += q_fq_codel.o
TCMODULES += q_fq_codel_fast.o
TCMODULES += q_fq.o
TCMODULES += q_pie.o
endif

ifeq ($(KERNELVERSION),4.9)
CFLAGS += -DLOW_RATE_THRESHOLD
CFLAGS += -DCE_THRESHOLD
TCMODULES += q_multiq.o
TCMODULES += q_cake.o
TCMODULES += q_codel.o
TCMODULES += q_fq_codel.o
TCMODULES += q_fq_codel_fast.o
TCMODULES += q_fq.o
TCMODULES += q_pie.o
endif

ifeq ($(KERNELVERSION),4.14)
CFLAGS += -DLOW_RATE_THRESHOLD
CFLAGS += -DCE_THRESHOLD
TCMODULES += q_multiq.o
TCMODULES += q_cake.o
TCMODULES += q_codel.o
TCMODULES += q_fq_codel.o
TCMODULES += q_fq_codel_fast.o
TCMODULES += q_fq.o
TCMODULES += q_pie.o
endif

ifeq ($(KERNELVERSION),6.1)
CFLAGS += -DLOW_RATE_THRESHOLD
CFLAGS += -DCE_THRESHOLD
TCMODULES += q_multiq.o
TCMODULES += q_cake.o
TCMODULES += q_codel.o
TCMODULES += q_fq_codel.o
TCMODULES += q_fq_codel_fast.o
TCMODULES += q_fq.o
TCMODULES += q_pie.o
endif
ifeq ($(KERNELVERSION),6.1-nss)
CFLAGS += -DLOW_RATE_THRESHOLD
CFLAGS += -DCE_THRESHOLD
TCMODULES += q_multiq.o
TCMODULES += q_cake.o
TCMODULES += q_codel.o
TCMODULES += q_fq_codel.o
TCMODULES += q_fq_codel_fast.o
TCMODULES += q_fq.o
TCMODULES += q_pie.o
endif
ifeq ($(KERNELVERSION),6.6)
CFLAGS += -DLOW_RATE_THRESHOLD
CFLAGS += -DCE_THRESHOLD
TCMODULES += q_multiq.o
TCMODULES += q_cake.o
TCMODULES += q_codel.o
TCMODULES += q_fq_codel.o
TCMODULES += q_fq_codel_fast.o
TCMODULES += q_fq.o
TCMODULES += q_pie.o
endif
ifeq ($(KERNELVERSION),6.6-nss)
CFLAGS += -DLOW_RATE_THRESHOLD
CFLAGS += -DCE_THRESHOLD
TCMODULES += q_multiq.o
TCMODULES += q_cake.o
TCMODULES += q_codel.o
TCMODULES += q_fq_codel.o
TCMODULES += q_fq_codel_fast.o
TCMODULES += q_fq.o
TCMODULES += q_pie.o
endif
ifeq ($(KERNELVERSION),6.12)
CFLAGS += -DLOW_RATE_THRESHOLD
CFLAGS += -DCE_THRESHOLD
TCMODULES += q_multiq.o
TCMODULES += q_cake.o
TCMODULES += q_codel.o
TCMODULES += q_fq_codel.o
TCMODULES += q_fq_codel_fast.o
TCMODULES += q_fq.o
TCMODULES += q_pie.o
endif


TCMODULES += m_gact.o
TCMODULES += m_mirred.o
TCMODULES += m_ipt.o
TCMODULES += m_pedit.o
TCMODULES += p_ip.o
TCMODULES += p_icmp.o
TCMODULES += p_tcp.o
TCMODULES += p_udp.o

TCOBJ += $(TCMODULES)

TCLIB := tc_core.o
TCLIB += tc_red.o
TCLIB += tc_cbq.o
TCLIB += tc_estimator.o

CFLAGS += -DCONFIG_GACT -DCONFIG_GACT_PROB

TCSO :=
TCSO += q_netem.so
ifeq ($(TC_CONFIG_ATM),y)
  TCSO += q_atm.so
endif

LDLIBS += -L. -ltc -lm -ldl

LDFLAGS += -Wl,-export-dynamic

%.so: %.c
	$(CC) $(CFLAGS) $(LDLTO) -shared -fpic $< -o $@


all: libtc.a tc $(TCSO)

tc: $(TCOBJ) $(LIBNETLINK) $(LIBUTIL) $(TCLIB)

libtc.a: $(TCLIB)
	$(AR) rcs $@ $(TCLIB) $(LTOPLUGIN)

install: all
	mkdir -p $(DESTDIR)/usr/lib/tc
	install -m 0755 -s tc $(DESTDIR)$(SBINDIR)
	for i in $(TCSO); \
	do install -m 755 -s $$i $(DESTDIR)/usr/lib/tc; \
	done

clean:
	rm -f $(TCOBJ) $(TCLIB) libtc.a tc *.so *.o

q_atm.so: q_atm.c
	$(CC) $(CFLAGS) -shared -fpic -o q_atm.so q_atm.c -latm

