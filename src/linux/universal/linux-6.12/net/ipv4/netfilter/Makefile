# SPDX-License-Identifier: GPL-2.0
#
# Makefile for the netfilter modules on top of IPv4.
#

# defrag
obj-$(CONFIG_NF_DEFRAG_IPV4) += nf_defrag_ipv4.o

obj-$(CONFIG_NF_SOCKET_IPV4) += nf_socket_ipv4.o
obj-$(CONFIG_NF_TPROXY_IPV4) += nf_tproxy_ipv4.o

# reject
obj-$(CONFIG_NF_REJECT_IPV4) += nf_reject_ipv4.o

# NAT helpers (nf_conntrack)
obj-$(CONFIG_NF_NAT_H323) += nf_nat_h323.o
obj-$(CONFIG_NF_NAT_PPTP) += nf_nat_pptp.o
obj-$(CONFIG_NF_NAT_RTSP) += nf_nat_rtsp.o

nf_nat_snmp_basic-y := nf_nat_snmp_basic.asn1.o nf_nat_snmp_basic_main.o
$(obj)/nf_nat_snmp_basic_main.o: $(obj)/nf_nat_snmp_basic.asn1.h
obj-$(CONFIG_NF_NAT_SNMP_BASIC) += nf_nat_snmp_basic.o

obj-$(CONFIG_NFT_REJECT_IPV4) += nft_reject_ipv4.o
obj-$(CONFIG_NFT_FIB_IPV4) += nft_fib_ipv4.o
obj-$(CONFIG_NFT_DUP_IPV4) += nft_dup_ipv4.o

# generic IP tables
obj-$(CONFIG_IP_NF_IPTABLES_LEGACY) += ip_tables.o

# the three instances of ip_tables
obj-$(CONFIG_IP_NF_FILTER) += iptable_filter.o
obj-$(CONFIG_IP_NF_MANGLE) += iptable_mangle.o
obj-$(CONFIG_IP_NF_NAT) += iptable_nat.o
obj-$(CONFIG_IP_NF_RAW) += iptable_raw.o
obj-$(CONFIG_IP_NF_SECURITY) += iptable_security.o

# matches
obj-$(CONFIG_IP_NF_MATCH_AH) += ipt_ah.o
obj-$(CONFIG_IP_NF_MATCH_RPFILTER) += ipt_rpfilter.o
obj-$(CONFIG_IP_NF_MATCH_WEBSTR) += ipt_webstr.o
obj-$(CONFIG_IP_NF_MATCH_IPP2P) += xt_ipp2p.o

# targets
obj-$(CONFIG_IP_NF_TARGET_ECN) += ipt_ECN.o
obj-$(CONFIG_IP_NF_TARGET_REJECT) += ipt_REJECT.o
obj-$(CONFIG_IP_NF_TARGET_SYNPROXY) += ipt_SYNPROXY.o
obj-$(CONFIG_IP_NF_TARGET_TRIGGER) += ipt_TRIGGER.o

# generic ARP tables
obj-$(CONFIG_IP_NF_ARPTABLES) += arp_tables.o
obj-$(CONFIG_IP_NF_ARP_MANGLE) += arpt_mangle.o

# just filtering instance of ARP tables for now
obj-$(CONFIG_IP_NF_ARPFILTER) += arptable_filter.o

obj-$(CONFIG_NF_DUP_IPV4) += nf_dup_ipv4.o
