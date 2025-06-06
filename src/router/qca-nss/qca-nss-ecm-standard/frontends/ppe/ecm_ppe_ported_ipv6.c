/*
 **************************************************************************
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 **************************************************************************
 */

#include <linux/version.h>
#include <linux/types.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/icmp.h>
#include <linux/debugfs.h>
#include <linux/kthread.h>
#include <linux/pkt_sched.h>
#include <linux/string.h>
#include <net/ip6_route.h>
#include <net/ip6_fib.h>
#include <net/addrconf.h>
#include <net/ipv6.h>
#include <net/tcp.h>
#include <asm/unaligned.h>
#include <asm/uaccess.h>	/* for put_user */
#include <net/ipv6.h>
#include <linux/inet.h>
#include <linux/in6.h>
#include <linux/udp.h>
#include <linux/tcp.h>
#include <linux/inetdevice.h>
#include <linux/if_arp.h>
#include <linux/netfilter_ipv6.h>
#include <linux/netfilter_bridge.h>
#include <linux/if_bridge.h>
#include <net/arp.h>
#include <net/netfilter/nf_conntrack.h>
#include <net/netfilter/nf_conntrack_acct.h>
#include <net/netfilter/nf_conntrack_helper.h>
#include <net/netfilter/nf_conntrack_l4proto.h>
#include <net/netfilter/nf_conntrack_zones.h>
#include <net/netfilter/nf_conntrack_core.h>
#include <net/netfilter/ipv6/nf_conntrack_ipv6.h>
#include <net/netfilter/ipv6/nf_defrag_ipv6.h>
#ifdef ECM_INTERFACE_VXLAN_ENABLE
#include <net/vxlan.h>
#endif
#include <linux/netfilter/nf_conntrack_tftp.h>
#ifdef ECM_INTERFACE_VLAN_ENABLE
#include <linux/../../net/8021q/vlan.h>
#include <linux/if_vlan.h>
#endif

/*
 * Debug output levels
 * 0 = OFF
 * 1 = ASSERTS / ERRORS
 * 2 = 1 + WARN
 * 3 = 2 + INFO
 * 4 = 3 + TRACE
 */
#define DEBUG_LEVEL ECM_PPE_PORTED_IPV6_DEBUG_LEVEL

#include "ecm_types.h"
#include "ecm_db_types.h"
#include "ecm_state.h"
#include "ecm_tracker.h"
#include "ecm_classifier.h"
#include "ecm_front_end_types.h"
#include "ecm_tracker_datagram.h"
#include "ecm_tracker_udp.h"
#include "ecm_tracker_tcp.h"
#include "ecm_db.h"
#include "ecm_classifier_default.h"
#ifdef ECM_CLASSIFIER_EMESH_ENABLE
#include "ecm_classifier_emesh.h"
#endif
#include "ecm_interface.h"
#include "ecm_ppe_ported_ipv6.h"
#include "ecm_ppe_ipv6.h"
#include "ecm_ppe_common.h"
#include "ecm_front_end_common.h"

static int ecm_ppe_ported_ipv6_accelerated_count[ECM_FRONT_END_PORTED_PROTO_MAX] = {0};
						/* Array of Number of TCP and UDP connections currently offloaded */

/*
 * ecm_ppe_ported_ipv6_handle_flush()
 *	Handle situation if destroy comes before setting up rule
 *	feci->lock is callers responsibility.
 */
static void ecm_ppe_ported_ipv6_handle_flush(struct ecm_front_end_connection_instance *feci)
{
	struct ecm_db_connection_instance *ci = feci->ci;

	DEBUG_ASSERT(feci->accel_mode == ECM_FRONT_END_ACCELERATION_MODE_ACCEL_PENDING, "%px: Unexpected mode: %d\n", ci, feci->accel_mode);

	/*
	 * If a flush occured before we got the ACK then our acceleration was effectively cancelled on us
	 */
	feci->stats.flush_happened = false;

	/*
	 * Increment the no-action counter. Our connection was decelerated on us with no action occurring.
	 */
	feci->stats.no_action_seen++;

	spin_lock_bh(&ecm_ppe_ipv6_lock);
	_ecm_ppe_ipv6_accel_pending_clear(feci, ECM_FRONT_END_ACCELERATION_MODE_DECEL);
	spin_unlock_bh(&ecm_ppe_ipv6_lock);
}

/*
 * ecm_ppe_ported_ipv6_accelerate_done()
 *	Acceleration rule pushed successfully
 *	feci->lock is caller's responsibility.
 */
static bool ecm_ppe_ported_ipv6_accelerate_done(struct ecm_front_end_connection_instance *feci)
{
	/*
	 * Clear any nack count
	 */
	feci->stats.ae_nack = 0;

	/*
	 * Clear the "accelerate pending" state and move to "accelerated" state bumping
	 * the accelerated counters to match our new state.
	 *
	 * Decelerate may have been attempted while we were "pending accel" and
	 * this function will return true if that was the case.
	 * If decelerate was pending then we need to begin deceleration :-(
	 */
	spin_lock_bh(&ecm_ppe_ipv6_lock);

	ecm_ppe_ported_ipv6_accelerated_count[feci->ported_accelerated_count_index]++;	/* Protocol specific counter */
	ecm_ppe_ipv6_accelerated_count++;		/* General running counter */

	if (!_ecm_ppe_ipv6_accel_pending_clear(feci, ECM_FRONT_END_ACCELERATION_MODE_ACCEL)) {
		/*
		 * Increment the no-action counter, this is reset if offload action is seen
		 */
		DEBUG_TRACE("%px: success(IPv6/ported): feci->accel_mode: %d\n", feci, feci->accel_mode);
		feci->stats.no_action_seen++;
		spin_unlock_bh(&ecm_ppe_ipv6_lock);
		return true;
	}

	spin_unlock_bh(&ecm_ppe_ipv6_lock);
	DEBUG_TRACE("%px: fail(IPv6/ported): feci->accel_mode: %d\n", feci, feci->accel_mode);

	return false;
}

/*
 * ecm_ppe_ported_ipv6_connection_accelerate()
 *	Accelerate a connection
 */
static void ecm_ppe_ported_ipv6_connection_accelerate(struct ecm_front_end_connection_instance *feci,
								struct ecm_classifier_process_response *pr, bool is_l2_encap,
								struct nf_conn *ct, struct sk_buff *skb)
{
	uint16_t regen_occurrances;
	int protocol;
	int32_t from_ifaces_first;
	int32_t to_ifaces_first;
	int32_t from_iface_id;
	int32_t to_iface_id;
	int32_t from_top_iface_id;
	int32_t to_top_iface_id;
	struct ecm_db_iface_instance *from_ifaces[ECM_DB_IFACE_HEIRARCHY_MAX];
	struct ecm_db_iface_instance *to_ifaces[ECM_DB_IFACE_HEIRARCHY_MAX];
	struct ecm_db_iface_instance *from_ppe_iface;
	struct ecm_db_iface_instance *to_ppe_iface;
	uint8_t from_ppe_iface_address[ETH_ALEN];
	uint8_t to_ppe_iface_address[ETH_ALEN];
	struct ppe_drv_v6_rule_create *pd6rc;
	struct ecm_classifier_instance *assignments[ECM_CLASSIFIER_TYPES];
	int aci_index;
	int assignment_count;
	ppe_drv_ret_t ppe_tx_status;
	int32_t list_index;
	int32_t interface_type_counts[ECM_DB_IFACE_TYPE_COUNT];
	bool rule_invalid;
	ip_addr_t src_ip;
	ip_addr_t dest_ip;
	bool is_defunct = false;
#ifdef ECM_FRONT_END_PPE_QOS_ENABLE
	bool is_ppeq = false;
#endif
	ecm_front_end_acceleration_mode_t result_mode;
	struct ecm_classifier_instance *aci;
	struct ecm_classifier_rule_create ecrc;

	DEBUG_CHECK_MAGIC(feci, ECM_FRONT_END_CONNECTION_INSTANCE_MAGIC, "%px: magic failed", feci);

	/*
	 * Get the re-generation occurrance counter of the connection.
	 * We compare it again at the end - to ensure that the rule construction has seen no generation
	 * changes during rule creation.
	 */
	regen_occurrances = ecm_db_connection_regeneration_occurrances_get(feci->ci);

	/*
	 * Test if acceleration is permitted
	 */
	if (!ecm_ppe_ipv6_accel_pending_set(feci)) {
		DEBUG_TRACE("%px: Acceleration not permitted: %px accel_mode=%d\n", feci, feci->ci, feci->accel_mode);
		return;
	}

	pd6rc = (struct ppe_drv_v6_rule_create *)kzalloc(sizeof(struct ppe_drv_v6_rule_create), GFP_ATOMIC | __GFP_NOWARN);
	if (!pd6rc) {
		DEBUG_WARN("%px: no memory for ppe ipv6 message structure instance: %px\n", feci, feci->ci);
		ecm_ppe_ipv6_accel_pending_clear(feci, ECM_FRONT_END_ACCELERATION_MODE_DECEL);
		return;
	}

	/*
	 * Okay construct an accel command.
	 * Initialise creation structure.
	 * NOTE: We leverage the app_data void pointer to be our 32 bit connection serial number.
	 * When we get it back we re-cast it to a uint32 and do a faster connection lookup.
	 */
	pd6rc->valid_flags = 0;
	pd6rc->rule_flags = 0;

	/*
	 * Initialize VLAN tag information
	 */
	pd6rc->vlan_rule.primary_vlan.ingress_vlan_tag = ECM_FRONT_END_VLAN_ID_NOT_CONFIGURED;
	pd6rc->vlan_rule.primary_vlan.egress_vlan_tag = ECM_FRONT_END_VLAN_ID_NOT_CONFIGURED;
	pd6rc->vlan_rule.secondary_vlan.ingress_vlan_tag = ECM_FRONT_END_VLAN_ID_NOT_CONFIGURED;
	pd6rc->vlan_rule.secondary_vlan.egress_vlan_tag = ECM_FRONT_END_VLAN_ID_NOT_CONFIGURED;

	/*
	 * Get the interface lists of the connection, we must have at least one interface in the list to continue
	 */
	from_ifaces_first = ecm_db_connection_interfaces_get_and_ref(feci->ci, from_ifaces, ECM_DB_OBJ_DIR_FROM);
	if (from_ifaces_first == ECM_DB_IFACE_HEIRARCHY_MAX) {
		DEBUG_WARN("%px: Accel attempt failed - no interfaces in from_interfaces list!\n", feci);
		goto ported_accel_bad_rule;
	}

	from_iface_id = ecm_db_iface_interface_identifier_get(from_ifaces[from_ifaces_first]);
	from_top_iface_id = ecm_db_iface_interface_identifier_get(from_ifaces[ECM_DB_IFACE_HEIRARCHY_MAX - 1]);
	DEBUG_TRACE("%px: from_iface_id:%d from_top_iface_id:%d", feci, from_iface_id, from_top_iface_id);

	to_ifaces_first = ecm_db_connection_interfaces_get_and_ref(feci->ci, to_ifaces, ECM_DB_OBJ_DIR_TO);
	if (to_ifaces_first == ECM_DB_IFACE_HEIRARCHY_MAX) {
		DEBUG_WARN("%px: Accel attempt failed - no interfaces in to_interfaces list!\n", feci);
		ecm_db_connection_interfaces_deref(from_ifaces, from_ifaces_first);
		goto ported_accel_bad_rule;
	}

	to_iface_id = ecm_db_iface_interface_identifier_get(to_ifaces[to_ifaces_first]);
	to_top_iface_id = ecm_db_iface_interface_identifier_get(to_ifaces[ECM_DB_IFACE_HEIRARCHY_MAX - 1]);

	DEBUG_TRACE("%px: to_iface_id:%d to_top_iface_id:%d", feci, to_iface_id, to_top_iface_id);

	/*
	 * Set the mtu values. These values will be overwritten if the flow is
	 * a specific tunnel type.
	 */
	pd6rc->conn_rule.flow_mtu = (uint32_t)ecm_db_connection_iface_mtu_get(feci->ci, ECM_DB_OBJ_DIR_FROM);
	pd6rc->conn_rule.return_mtu = (uint32_t)ecm_db_connection_iface_mtu_get(feci->ci, ECM_DB_OBJ_DIR_TO);

	/*
	 * Get the top interface in the hierarchy
	 */
	from_ppe_iface = from_ifaces[ECM_DB_IFACE_HEIRARCHY_MAX - 1];
	to_ppe_iface = to_ifaces[ECM_DB_IFACE_HEIRARCHY_MAX - 1];

	/*
	 * We know that each outward facing interface is known to the PPE and so this connection could be accelerated.
	 * However the lists may also specify other interesting details that must be included in the creation command,
	 * for example, ethernet MAC, VLAN tagging or PPPoE session information.
	 * We get this information by walking from the outer to the innermost interface for each list and examine the interface types.
	 *
	 * Start with the 'from' (src) side.
	 * NOTE: The lists may contain a complex heirarchy of similar type of interface e.g. multiple vlans or tunnels within tunnels.
	 * This PPE cannot handle that - there is no way to describe this in the rule - if we see multiple types that would conflict we have to abort.
	 */
	DEBUG_TRACE("%px: Examine from/src heirarchy list. skb=%px\n", feci, skb);
	memset(interface_type_counts, 0, sizeof(interface_type_counts));
	rule_invalid = false;
	for (list_index = from_ifaces_first; !rule_invalid && (list_index < ECM_DB_IFACE_HEIRARCHY_MAX); list_index++) {
		struct ecm_db_iface_instance *ii;
		uint32_t iface_id, ae_iface_id;
		ecm_db_iface_type_t ii_type;
		char *ii_name;
#ifdef ECM_INTERFACE_VLAN_ENABLE
		struct ecm_db_interface_info_vlan vlan_info;
		uint32_t vlan_value = 0;
		struct net_device *vlan_in_dev = NULL;
#endif

#ifdef ECM_INTERFACE_PPPOE_ENABLE
		struct ecm_db_interface_info_pppoe pppoe_info;
#endif
		ii = from_ifaces[list_index];
		ii_type = ecm_db_iface_type_get(ii);
		ii_name = ecm_db_interface_type_to_string(ii_type);
		iface_id = ecm_db_iface_interface_identifier_get(ii);
		ae_iface_id = ecm_ppe_common_get_ae_iface_id_by_netdev_id(iface_id);

		DEBUG_TRACE("%px: list_index: %d, ii: %px(%s %d), type: %d (%s), ae_iface_id(%d)\n",
				feci, list_index, ii, ii->name, iface_id, ii_type, ii_name, ae_iface_id);

		if (ecm_front_end_common_intf_ingress_qdisc_check(iface_id)) {
			DEBUG_TRACE("%px: PPE doesn't support ingress qdisc for this flow:(%d) type:%d(%s) interface",
					feci, iface_id, ii_type, ii_name);
			ecm_db_connection_interfaces_deref(from_ifaces, from_ifaces_first);
			ecm_db_connection_interfaces_deref(to_ifaces, to_ifaces_first);
			goto ported_accel_bad_rule;
		}

#ifdef ECM_FRONT_END_PPE_QOS_ENABLE
		if (ecm_front_end_common_intf_qdisc_check(iface_id, &is_ppeq) && !is_ppeq) {
			DEBUG_TRACE("%px: PPE doesn't support qdisc for this flow:(%d) type:%d(%s) interface",
					feci, iface_id, ii_type, ii_name);
			ecm_db_connection_interfaces_deref(from_ifaces, from_ifaces_first);
			ecm_db_connection_interfaces_deref(to_ifaces, to_ifaces_first);
			goto ported_accel_bad_rule;
		}
#endif

		/*
		 * Extract information from this interface type if it is applicable to the rule.
		 * Conflicting information may cause accel to be unsupported.
		 */
		switch (ii_type) {
		case ECM_DB_IFACE_TYPE_BRIDGE:
			DEBUG_TRACE("%px: Bridge\n", feci);
			if (interface_type_counts[ii_type] != 0) {
				/*
				 * Cannot cascade bridges
				 */
				rule_invalid = true;
				DEBUG_TRACE("%px: Bridge - ignore additional\n", feci);
				break;
			}

			ecm_db_iface_bridge_address_get(ii, from_ppe_iface_address);
#ifdef ECM_INTERFACE_VLAN_ENABLE
			if ((ecm_db_iface_type_get(from_ppe_iface) == ECM_DB_IFACE_TYPE_VLAN) &&
			    ecm_db_connection_is_routed_get(feci->ci)) {
				pd6rc->rule_flags |= PPE_DRV_V6_RULE_FROM_BRIDGE_VLAN_NETDEV;
				DEBUG_TRACE("%px VLAN over bridge %s from hierarchy\n", feci, from_ppe_iface->name);
			}
#endif
			DEBUG_TRACE("%px: Bridge - mac: %pM\n", feci, from_ppe_iface_address);
			break;

		case ECM_DB_IFACE_TYPE_OVS_BRIDGE:
#ifdef ECM_INTERFACE_OVS_BRIDGE_ENABLE
			DEBUG_TRACE("%px: OVS Bridge\n", feci);
			if (interface_type_counts[ii_type] != 0) {
				/*
				 * Cannot cascade bridges
				 */
				rule_invalid = true;
				DEBUG_TRACE("%px: OVS Bridge - ignore additional\n", feci);
				break;
			}

			ecm_db_iface_ovs_bridge_address_get(ii, from_ppe_iface_address);
			DEBUG_TRACE("%px: OVS Bridge - mac: %pM\n", feci, from_ppe_iface_address);
#else
			rule_invalid = true;
			DEBUG_TRACE("%px: OVS Bridge - not supported\n", feci);
#endif
			break;

		case ECM_DB_IFACE_TYPE_ETHERNET:
			DEBUG_TRACE("%px: Ethernet\n", feci);
			if (interface_type_counts[ii_type] != 0) {
				/*
				 * Ignore additional mac addresses, these are usually as a result of address propagation
				 * from bridges down to ports etc.
				 */
				DEBUG_TRACE("%px: Ethernet - ignore additional\n", feci);
				break;
			}

			/*
			 * Can only handle one MAC, the first outermost mac.
			 */
			ecm_db_iface_ethernet_address_get(ii, from_ppe_iface_address);
			DEBUG_TRACE("%px: Ethernet - mac: %pM\n", feci, from_ppe_iface_address);
			break;

		case ECM_DB_IFACE_TYPE_PPPOE:
#ifdef ECM_INTERFACE_PPPOE_ENABLE
			/*
			 * More than one PPPoE in the list is not valid!
			 */
			if (interface_type_counts[ii_type] != 0) {
				DEBUG_TRACE("%px: PPPoE - additional unsupported\n", feci);
				rule_invalid = true;
				break;
			}

			ecm_db_iface_pppoe_session_info_get(ii, &pppoe_info);

			pd6rc->pppoe_rule.flow_session.session_id = pppoe_info.pppoe_session_id;
			memcpy(pd6rc->pppoe_rule.flow_session.server_mac, pppoe_info.remote_mac, ETH_ALEN);
			pd6rc->valid_flags |= PPE_DRV_V6_VALID_FLAG_PPPOE_FLOW;

			DEBUG_TRACE("%px: PPPoE - session: %x, mac: %pM\n", feci,
					pd6rc->pppoe_rule.return_session.session_id,
					pd6rc->pppoe_rule.return_session.server_mac);

#else
			rule_invalid = true;
#endif
			break;

		case ECM_DB_IFACE_TYPE_VLAN:
		{
#ifdef ECM_INTERFACE_VLAN_ENABLE
			int32_t port_id;

			DEBUG_TRACE("%px: VLAN\n", feci);

			if (interface_type_counts[ii_type] > 1) {
				/*
				 * Can only support two vlans
				 */
				rule_invalid = true;
				DEBUG_TRACE("%px: VLAN - additional unsupported\n", feci);
				break;
			}
			ecm_db_iface_vlan_info_get(ii, &vlan_info);
			vlan_value = ((vlan_info.vlan_tpid << 16) | vlan_info.vlan_tag);

			/*
			 * Look up the vlan device and incorporate the vlan priority into the vlan_value
			 */
			vlan_in_dev = dev_get_by_index(&init_net, ecm_db_iface_interface_identifier_get(ii));
			if (vlan_in_dev) {
				vlan_value |= vlan_dev_get_egress_qos_mask(vlan_in_dev, pr->return_qos_tag);
				dev_put(vlan_in_dev);
				vlan_in_dev = NULL;
			}

			/*
			 * In case of vlan as VP port we need to specify port
			 * corresponding to vlan port and not the ultimate physical port.
			 */
			port_id = ecm_ppe_common_get_port_id_by_netdev_id(iface_id);

			if ((port_id != PPE_DRV_PORT_ID_INVALID) && ppe_vp_get_netdev_by_port_num(port_id)) {
				from_iface_id = iface_id;
			}

			/*
			 * Primary or secondary (QinQ) VLAN?
			 */
			if (interface_type_counts[ii_type] == 0) {
				pd6rc->vlan_rule.primary_vlan.ingress_vlan_tag = vlan_value;
			} else {
				pd6rc->vlan_rule.secondary_vlan.ingress_vlan_tag = vlan_value;
			}

			pd6rc->valid_flags |= PPE_DRV_V6_VALID_FLAG_VLAN;

			/*
			 * If we have not yet got an ethernet mac then take this one (very unlikely as mac should have been propagated to the slave (outer) device
			 */
			memcpy(from_ppe_iface_address, vlan_info.address, ETH_ALEN);
			DEBUG_TRACE("%px: vlan tag: %x\n", feci, vlan_value);
#else
			rule_invalid = true;
			DEBUG_TRACE("%px: VLAN - unsupported\n", feci);
#endif
			break;
		}

		case ECM_DB_IFACE_TYPE_IPSEC_TUNNEL:
#ifdef ECM_INTERFACE_IPSEC_ENABLE
			/*
			 * TODO: Add support for IPsec in PPE Frontend.
			 */
			DEBUG_WARN("%px: IPSEC is unsupported in PPE : %d (%s)\n", feci, ii_type, ii_name);
			rule_invalid = true;
#endif
			break;

		case ECM_DB_IFACE_TYPE_VXLAN:
		{
			/*
			 * For VxLAN device, a 5-tuple connection rule is added with the same src and dest ports in both the directions.
			 * Source interface is a VxLAN interface for the routed flow which is the case for VxLAN->IPsec or VxLAN->WAN rule.
			 * Note: These rules are always expected to be pushed only in tunnel to WAN direction.
			 */
#ifdef ECM_INTERFACE_VXLAN_ENABLE
			int32_t vxlan_ppe_dev_id;
			uint32_t vp_status;

			DEBUG_TRACE("%px: From VXLAN interface\n", feci);
			if (interface_type_counts[ii_type] != 0) {
				/*
				 * Can support only one VxLAN interface.
				 */
				DEBUG_WARN("%px: VxLAN - ignore additional\n", feci);
				rule_invalid = true;
				break;
			}

			vxlan_ppe_dev_id = ecm_ppe_ported_get_vxlan_ppe_dev_index(feci, ii, ECM_DB_OBJ_DIR_FROM, &vp_status);
			DEBUG_TRACE("%px: VXLAN: vxlan_ppe_dev_id:%d vp_status:%u", feci, vxlan_ppe_dev_id, vp_status);
			if (vp_status == NSS_PPE_VXLANMGR_VP_CREATION_IN_PROGRESS) {
				/* Retry with the subsequent packets */
				ecm_db_connection_interfaces_deref(from_ifaces, from_ifaces_first);
				ecm_db_connection_interfaces_deref(to_ifaces, to_ifaces_first);
				ecm_ppe_ipv6_accel_pending_clear(feci, ECM_FRONT_END_ACCELERATION_MODE_DECEL);
				kfree(pd6rc);
				return;
			}

			if ((vp_status != NSS_PPE_VXLANMGR_VP_CREATION_SUCCESS) || (vxlan_ppe_dev_id < 0)) {
				DEBUG_WARN("%px: VXLAN: Re-tried enough\n", feci);
				rule_invalid = true;
				break;
			}

			/* overwrite the first and top interface id */
			from_iface_id = vxlan_ppe_dev_id;
			from_top_iface_id = vxlan_ppe_dev_id;
			ae_iface_id = vxlan_ppe_dev_id;

			feci->set_stats_bitmap(feci, ECM_DB_OBJ_DIR_FROM, ECM_DB_IFACE_TYPE_VXLAN);
#else
			rule_invalid = true;
			DEBUG_TRACE("%px: VXLAN - unsupported\n", feci);
#endif
		}
			break;

		default:
			DEBUG_TRACE("%px: Ignoring: %d (%s)\n", feci, ii_type, ii_name);
		}

		if (ae_iface_id < 0) {
			DEBUG_TRACE("%px: PPE doesn't support iface_id:(%d) type:%d(%s) interface",
					feci, iface_id, ii_type, ii_name);
			ecm_db_connection_interfaces_deref(from_ifaces, from_ifaces_first);
			ecm_db_connection_interfaces_deref(to_ifaces, to_ifaces_first);
			goto ported_accel_bad_rule;
		}

		/*
		 * Seen an interface of this type
		 */
		interface_type_counts[ii_type]++;
	}

	if (rule_invalid) {
		DEBUG_WARN("%px: from/src Rule invalid\n", feci);
		ecm_db_connection_interfaces_deref(from_ifaces, from_ifaces_first);
		ecm_db_connection_interfaces_deref(to_ifaces, to_ifaces_first);
		goto ported_accel_bad_rule;
	}

	/*
	 * First interface in each must be a known ppe interface
	 * Set interface numbers involved in accelerating this connection.
	 * These are the outer facing addresses from the heirarchy interface lists we got above.
	 */
	pd6rc->conn_rule.rx_if = ecm_ppe_common_get_ae_iface_id_by_netdev_id(from_iface_id);
	if (pd6rc->conn_rule.rx_if < 0) {
		ecm_db_connection_interfaces_deref(from_ifaces, from_ifaces_first);
		ecm_db_connection_interfaces_deref(to_ifaces, to_ifaces_first);
		DEBUG_TRACE("%px: Accel attempt failed - FIRST interfaces for 'from'(%d) interface list!\n", feci, pd6rc->conn_rule.rx_if);
		goto ported_accel_bad_rule;
	}

	/*
	 * Get PPE interface ID of the top interface in heirarchy
	 */
	pd6rc->top_rule.rx_if = ecm_ppe_common_get_ae_iface_id_by_netdev_id(from_top_iface_id);
	if (pd6rc->top_rule.rx_if < 0) {
		ecm_db_connection_interfaces_deref(from_ifaces, from_ifaces_first);
		ecm_db_connection_interfaces_deref(to_ifaces, to_ifaces_first);
		DEBUG_TRACE("%px: Accel attempt failed - TOP interfaces for 'from'(%d) interface list!\n", feci, pd6rc->top_rule.rx_if);
		goto ported_accel_bad_rule;
	}

	/*
	 * Now examine the TO / DEST heirarchy list to construct the destination part of the rule
	 */
	DEBUG_TRACE("%px: Examine to/dest heirarchy list\n", feci);
	memset(interface_type_counts, 0, sizeof(interface_type_counts));
	rule_invalid = false;
	for (list_index = to_ifaces_first; !rule_invalid && (list_index < ECM_DB_IFACE_HEIRARCHY_MAX); list_index++) {
		struct ecm_db_iface_instance *ii;
		int32_t iface_id, ae_iface_id;
		ecm_db_iface_type_t ii_type;
		char *ii_name;
#ifdef ECM_INTERFACE_VLAN_ENABLE
		struct ecm_db_interface_info_vlan vlan_info;
		uint32_t vlan_value = 0;
		struct net_device *vlan_out_dev = NULL;
#endif
#ifdef ECM_INTERFACE_PPPOE_ENABLE
		struct ecm_db_interface_info_pppoe pppoe_info;
#endif
		ii = to_ifaces[list_index];
		ii_type = ecm_db_iface_type_get(ii);
		ii_name = ecm_db_interface_type_to_string(ii_type);
		iface_id = ecm_db_iface_interface_identifier_get(ii);
		ae_iface_id = ecm_ppe_common_get_ae_iface_id_by_netdev_id(iface_id);

		DEBUG_TRACE("%px: list_index: %d, ii: %px(%s %d), type: %d (%s), ae_iface_id(%d)\n",
				feci, list_index, ii, ii->name, iface_id, ii_type, ii_name, ae_iface_id);

		if (ecm_front_end_common_intf_ingress_qdisc_check(iface_id)) {
			DEBUG_TRACE("%px: PPE doesn't support ingress qdisc for this flow:(%d) type:%d(%s) interface",
					feci, iface_id, ii_type, ii_name);
			ecm_db_connection_interfaces_deref(from_ifaces, from_ifaces_first);
			ecm_db_connection_interfaces_deref(to_ifaces, to_ifaces_first);
			goto ported_accel_bad_rule;
		}

#ifdef ECM_FRONT_END_PPE_QOS_ENABLE
		if (ecm_front_end_common_intf_qdisc_check(iface_id, &is_ppeq) && !is_ppeq) {
			DEBUG_TRACE("%px: PPE doesn't support qdisc for this flow:(%d) type:%d(%s) interface",
					feci, iface_id, ii_type, ii_name);
			ecm_db_connection_interfaces_deref(from_ifaces, from_ifaces_first);
			ecm_db_connection_interfaces_deref(to_ifaces, to_ifaces_first);
			goto ported_accel_bad_rule;
		}
#endif

		/*
		 * Extract information from this interface type if it is applicable to the rule.
		 * Conflicting information may cause accel to be unsupported.
		 */
		switch (ii_type) {
		case ECM_DB_IFACE_TYPE_BRIDGE:
			DEBUG_TRACE("%px: Bridge\n", feci);
			if (interface_type_counts[ii_type] != 0) {
				/*
				 * Cannot cascade bridges
				 */
				rule_invalid = true;
				DEBUG_TRACE("%px: Bridge - ignore additional\n", feci);
				break;
			}

			ecm_db_iface_bridge_address_get(ii, to_ppe_iface_address);
#ifdef ECM_INTERFACE_VLAN_ENABLE
			if ((ecm_db_iface_type_get(to_ppe_iface) == ECM_DB_IFACE_TYPE_VLAN) &&
			    ecm_db_connection_is_routed_get(feci->ci)) {
				pd6rc->rule_flags |= PPE_DRV_V6_RULE_TO_BRIDGE_VLAN_NETDEV;
				DEBUG_TRACE("%px VLAN over bridge %s to hierarchy \n", feci, to_ppe_iface->name);
			}
#endif
			DEBUG_TRACE("%px: Bridge - mac: %pM\n", feci, to_ppe_iface_address);
			break;

		case ECM_DB_IFACE_TYPE_OVS_BRIDGE:
#ifdef ECM_INTERFACE_OVS_BRIDGE_ENABLE
			DEBUG_TRACE("%px: OVS Bridge\n", feci);
			if (interface_type_counts[ii_type] != 0) {
				/*
				 * Cannot cascade bridges
				 */
				rule_invalid = true;
				DEBUG_TRACE("%px: OVS Bridge - ignore additional\n", feci);
				break;
			}

			ecm_db_iface_ovs_bridge_address_get(ii, to_ppe_iface_address);
			DEBUG_TRACE("%px: OVS Bridge - mac: %pM\n", feci, to_ppe_iface_address);
#else
			rule_invalid = true;
			DEBUG_TRACE("%px: OVS Bridge - not supported\n", feci);
#endif
			break;

		case ECM_DB_IFACE_TYPE_ETHERNET:
			DEBUG_TRACE("%px: Ethernet\n", feci);
			if (interface_type_counts[ii_type] != 0) {
				/*
				 * Ignore additional mac addresses, these are usually as a result of address propagation
				 * from bridges down to ports etc.
				 */
				DEBUG_TRACE("%px: Ethernet - ignore additional\n", feci);
				break;
			}

			/*
			 * Can only handle one MAC, the first outermost mac.
			 */
			ecm_db_iface_ethernet_address_get(ii, to_ppe_iface_address);
			DEBUG_TRACE("%px: Ethernet - mac: %pM\n", feci, to_ppe_iface_address);
			break;

		case ECM_DB_IFACE_TYPE_PPPOE:
#ifdef ECM_INTERFACE_PPPOE_ENABLE
			/*
			 * More than one PPPoE in the list is not valid!
			 */
			if (interface_type_counts[ii_type] != 0) {
				DEBUG_TRACE("%px: PPPoE - additional unsupported\n", feci);
				rule_invalid = true;
				break;
			}

			/*
			 * Set the PPPoE rule creation structure.
			 */
			ecm_db_iface_pppoe_session_info_get(ii, &pppoe_info);
			pd6rc->pppoe_rule.return_session.session_id = pppoe_info.pppoe_session_id;
			memcpy(pd6rc->pppoe_rule.return_session.server_mac, pppoe_info.remote_mac, ETH_ALEN);
			pd6rc->valid_flags |= PPE_DRV_V6_VALID_FLAG_PPPOE_RETURN;

			DEBUG_TRACE("%px: PPPoE - session: %x, mac: %pM\n", feci,
					pd6rc->pppoe_rule.flow_session.session_id,
					pd6rc->pppoe_rule.flow_session.server_mac);
#else
			rule_invalid = true;
#endif
			break;

		case ECM_DB_IFACE_TYPE_VLAN:
		{
#ifdef ECM_INTERFACE_VLAN_ENABLE
			int32_t port_id;

			DEBUG_TRACE("%px: VLAN\n", feci);

			if (interface_type_counts[ii_type] > 1) {
				/*
				 * Can only support two vlans
				 */
				rule_invalid = true;
				DEBUG_TRACE("%px: VLAN - additional unsupported\n", feci);
				break;
			}
			ecm_db_iface_vlan_info_get(ii, &vlan_info);
			vlan_value = ((vlan_info.vlan_tpid << 16) | vlan_info.vlan_tag);

			/*
			 * Look up the vlan device and incorporate the vlan priority into the vlan_value
			 */
			vlan_out_dev = dev_get_by_index(&init_net, ecm_db_iface_interface_identifier_get(ii));
			if (vlan_out_dev) {
				vlan_value |= vlan_dev_get_egress_qos_mask(vlan_out_dev, pr->flow_qos_tag);
				dev_put(vlan_out_dev);
				vlan_out_dev = NULL;
			}

			/*
			 * In case of vlan as VP port we need to specify port
			 * corresponding to vlan port and not the ultimate physical port.
			 */
			port_id = ecm_ppe_common_get_port_id_by_netdev_id(iface_id);

			if ((port_id != PPE_DRV_PORT_ID_INVALID) && ppe_vp_get_netdev_by_port_num(port_id)) {
				to_iface_id = iface_id;
			}

			/*
			 * Primary or secondary (QinQ) VLAN?
			 */
			if (interface_type_counts[ii_type] == 0) {
				pd6rc->vlan_rule.primary_vlan.egress_vlan_tag = vlan_value;
			} else {
				pd6rc->vlan_rule.secondary_vlan.egress_vlan_tag = vlan_value;
			}

			pd6rc->valid_flags |= PPE_DRV_V6_VALID_FLAG_VLAN;

			/*
			 * If we have not yet got an ethernet mac then take this one (very unlikely as mac should have been propagated to the slave (outer) device
			 */
			memcpy(to_ppe_iface_address, vlan_info.address, ETH_ALEN);
			DEBUG_TRACE("%px: vlan tag: %x\n", feci, vlan_value);
#else
			rule_invalid = true;
			DEBUG_TRACE("%px: VLAN - unsupported\n", feci);
#endif
			break;
		}

		case ECM_DB_IFACE_TYPE_IPSEC_TUNNEL:
#ifdef ECM_INTERFACE_IPSEC_ENABLE
			/*
			 * TODO: Add support for IPsec in PPE Frontend.
			 */
			DEBUG_WARN("%px: IPSEC is unsupported in PPE : %d (%s)\n", feci, ii_type, ii_name);
			rule_invalid = true;
#endif
			break;

		case ECM_DB_IFACE_TYPE_VXLAN:
		{
#ifdef ECM_INTERFACE_VXLAN_ENABLE
			int32_t vxlan_ppe_dev_id;
			uint32_t vp_status;

			vxlan_ppe_dev_id = ecm_ppe_ported_get_vxlan_ppe_dev_index(feci, ii, ECM_DB_OBJ_DIR_TO, &vp_status);
			DEBUG_TRACE("%px: VXLAN: vxlan_ppe_dev_id:%d vp_status:%u", feci, vxlan_ppe_dev_id, vp_status);
			if (vp_status == NSS_PPE_VXLANMGR_VP_CREATION_IN_PROGRESS) {
				/* Retry with the subsequent packets */
				ecm_db_connection_interfaces_deref(from_ifaces, from_ifaces_first);
				ecm_db_connection_interfaces_deref(to_ifaces, to_ifaces_first);
				ecm_ppe_ipv6_accel_pending_clear(feci, ECM_FRONT_END_ACCELERATION_MODE_DECEL);
				kfree(pd6rc);
				return;
			}

			if ((vp_status != NSS_PPE_VXLANMGR_VP_CREATION_SUCCESS) || (vxlan_ppe_dev_id < 0)) {
				DEBUG_WARN("%px: VXLAN: Re-tried enough\n", feci);
				rule_invalid = true;
				break;
			}

			/* overwrite the first or top interface id */
			to_iface_id = vxlan_ppe_dev_id;
			to_top_iface_id = vxlan_ppe_dev_id;
			ae_iface_id = vxlan_ppe_dev_id;

			feci->set_stats_bitmap(feci, ECM_DB_OBJ_DIR_TO, ECM_DB_IFACE_TYPE_VXLAN);
#else
			rule_invalid = true;
			DEBUG_TRACE("%px: VXLAN - unsupported\n", feci);
#endif
		}
			break;

		default:
			DEBUG_TRACE("%px: Ignoring: %d (%s)\n", feci, ii_type, ii_name);
		}

		if (ae_iface_id < 0) {
			DEBUG_TRACE("%px: PPE doesn't support iface_id:(%d) type:%d(%s) interface",
					feci, iface_id, ii_type, ii_name);
			ecm_db_connection_interfaces_deref(from_ifaces, from_ifaces_first);
			ecm_db_connection_interfaces_deref(to_ifaces, to_ifaces_first);
			goto ported_accel_bad_rule;
		}

		/*
		 * Seen an interface of this type
		 */
		interface_type_counts[ii_type]++;
	}

	if (rule_invalid) {
		DEBUG_WARN("%px: to/dest Rule invalid\n", feci);
		ecm_db_connection_interfaces_deref(from_ifaces, from_ifaces_first);
		ecm_db_connection_interfaces_deref(to_ifaces, to_ifaces_first);
		goto ported_accel_bad_rule;
	}

	/*
	 * First interface in each must be a known ppe interface
	 * Set interface numbers involved in accelerating this connection.
	 * These are the outer facing addresses from the heirarchy interface lists we got above.
	 */
	pd6rc->conn_rule.tx_if = ecm_ppe_common_get_ae_iface_id_by_netdev_id(to_iface_id);
	if (pd6rc->conn_rule.tx_if < 0) {
		ecm_db_connection_interfaces_deref(from_ifaces, from_ifaces_first);
		ecm_db_connection_interfaces_deref(to_ifaces, to_ifaces_first);
		DEBUG_TRACE("%px: Accel attempt failed - FIRST interfaces for 'to'(%d) interface list!\n", feci, pd6rc->conn_rule.tx_if);
		goto ported_accel_bad_rule;
	}

	/*
	 * Get PPE interface ID of the top interface in heirarchy
	 */
	pd6rc->top_rule.tx_if = ecm_ppe_common_get_ae_iface_id_by_netdev_id(to_top_iface_id);
	if (pd6rc->top_rule.tx_if < 0) {
		ecm_db_connection_interfaces_deref(from_ifaces, from_ifaces_first);
		ecm_db_connection_interfaces_deref(to_ifaces, to_ifaces_first);
		DEBUG_TRACE("%px: Accel attempt failed - TOP interfaces for 'to'(%d) interfaces list!\n", feci, pd6rc->top_rule.tx_if);
		goto ported_accel_bad_rule;
	}

	/*
	 * Check if the PPE offload is enabled for the rule's Tx/Rx interfaces or not
	 */
	if (!ppe_drv_iface_check_flow_offload_enabled(pd6rc->conn_rule.rx_if,
						pd6rc->conn_rule.tx_if)) {
		DEBUG_TRACE("%px: PPE offload is disabled for rx if: %d, tx: %d\n",
				feci, pd6rc->conn_rule.rx_if, pd6rc->conn_rule.tx_if);
		ecm_db_connection_interfaces_deref(from_ifaces, from_ifaces_first);
		ecm_db_connection_interfaces_deref(to_ifaces, to_ifaces_first);
		goto ported_accel_bad_rule;
	}

	DEBUG_TRACE("%px: PPE offload is enabled for rx if: %d, tx: %d\n",
			feci, pd6rc->conn_rule.rx_if, pd6rc->conn_rule.tx_if);

	/*
	 * Routed or bridged?
	 */
	if (ecm_db_connection_is_routed_get(feci->ci)) {
		pd6rc->rule_flags |= PPE_DRV_V6_RULE_FLAG_ROUTED_FLOW;
	} else {
		pd6rc->rule_flags |= PPE_DRV_V6_RULE_FLAG_BRIDGE_FLOW;
	}

	/*
	 * Set up the flow and return qos tags
	 */
	if (pr->process_actions & ECM_CLASSIFIER_PROCESS_ACTION_QOS_TAG) {

#ifdef ECM_FRONT_END_PPE_QOS_ENABLE
		int32_t to_ppe_qos_intf = ecm_db_iface_interface_identifier_get(to_ifaces[to_ifaces_first]);
		int32_t from_ppe_qos_intf = ecm_db_iface_interface_identifier_get(from_ifaces[from_ifaces_first]);

		if (ecm_front_end_common_intf_qdisc_check(to_ppe_qos_intf, &is_ppeq) && is_ppeq) {
			pd6rc->qos_rule.flow_int_pri = ppe_drv_qos_int_pri_get(dev_get_by_index(&init_net, to_ppe_qos_intf), pr->flow_qos_tag);
			pd6rc->qos_rule.qos_valid_flags |= PPE_DRV_VALID_FLAG_FLOW_PPE_QOS;
		}

		if (ecm_front_end_common_intf_qdisc_check(from_ppe_qos_intf, &is_ppeq) && is_ppeq) {
			pd6rc->qos_rule.return_int_pri = ppe_drv_qos_int_pri_get(dev_get_by_index(&init_net,from_ppe_qos_intf), pr->return_qos_tag);
			pd6rc->qos_rule.qos_valid_flags |= PPE_DRV_VALID_FLAG_RETURN_PPE_QOS;
		}
#endif

		pd6rc->qos_rule.flow_qos_tag = (uint32_t)pr->flow_qos_tag;
		pd6rc->qos_rule.return_qos_tag = (uint32_t)pr->return_qos_tag;
		pd6rc->valid_flags |= PPE_DRV_V6_VALID_FLAG_QOS;
	}

	/*
	 * DSCP information?
	 */
#if defined ECM_CLASSIFIER_DSCP_ENABLE || defined ECM_CLASSIFIER_EMESH_ENABLE
	if (pr->process_actions & ECM_CLASSIFIER_PROCESS_ACTION_DSCP) {
		pd6rc->dscp_rule.flow_dscp = pr->flow_dscp;
		pd6rc->dscp_rule.return_dscp = pr->return_dscp;
		pd6rc->rule_flags |= PPE_DRV_V6_RULE_FLAG_DSCP_MARKING;
		pd6rc->valid_flags |= PPE_DRV_V6_VALID_FLAG_DSCP_MARKING;
	}
#endif

	if (ecm_ppe_ipv6_vlan_passthrough_enable && !ecm_db_connection_is_routed_get(feci->ci) &&
		(pd6rc->vlan_rule.primary_vlan.ingress_vlan_tag == ECM_FRONT_END_VLAN_ID_NOT_CONFIGURED) &&
		(pd6rc->vlan_rule.primary_vlan.egress_vlan_tag == ECM_FRONT_END_VLAN_ID_NOT_CONFIGURED)) {
		int vlan_present = 0;
		vlan_present = skb_vlan_tag_present(skb);
		if (vlan_present) {
			uint32_t vlan_value;
			vlan_value = (ETH_P_8021Q << 16) | skb_vlan_tag_get(skb);
			pd6rc->vlan_rule.primary_vlan.ingress_vlan_tag = vlan_value;
			pd6rc->vlan_rule.primary_vlan.egress_vlan_tag = vlan_value;
			pd6rc->valid_flags |= PPE_DRV_V6_VALID_FLAG_VLAN;
		}
	}

#ifdef ECM_PPE_SOURCE_INTERFACE_CHECK_ENABLE
	if (ecm_interface_src_check) {
		pd6rc->rule_flags |= PPE_DRV_V6_RULE_FLAG_SRC_INTERFACE_CHECK;
		DEBUG_INFO("%px: Source interface check is enabled\n", feci);
	}
#endif

#ifdef ECM_CLASSIFIER_OVS_ENABLE
	/*
	 * Copy both primary and secondary (if exist) VLAN tags.
	 */
	if (pr->process_actions & ECM_CLASSIFIER_PROCESS_ACTION_OVS_VLAN_TAG) {
		pd6rc->vlan_rule.primary_vlan.ingress_vlan_tag = pr->ingress_vlan_tag[0];
		pd6rc->vlan_rule.primary_vlan.egress_vlan_tag = pr->egress_vlan_tag[0];
		pd6rc->valid_flags |= PPE_DRV_V6_VALID_FLAG_VLAN;
	}

	if (pr->process_actions & ECM_CLASSIFIER_PROCESS_ACTION_OVS_VLAN_QINQ_TAG) {
		pd6rc->vlan_rule.secondary_vlan.ingress_vlan_tag = pr->ingress_vlan_tag[1];
		pd6rc->vlan_rule.secondary_vlan.egress_vlan_tag = pr->egress_vlan_tag[1];
	}
#endif

#ifdef ECM_CLASSIFIER_EMESH_ENABLE

        /*
         * SAWF information
         */
        if (pr->process_actions & ECM_CLASSIFIER_PROCESS_ACTION_EMESH_SAWF_TAG) {
		bool sawf_rule_valid = true;
		pd6rc->sawf_rule.flow_mark = pr->flow_sawf_metadata;
		pd6rc->sawf_rule.flow_service_class = pr->flow_service_class;
		pd6rc->sawf_rule.return_mark = pr->return_sawf_metadata;
		pd6rc->sawf_rule.return_service_class = pr->return_service_class;
		pd6rc->valid_flags |= PPE_DRV_V6_VALID_FLAG_SAWF;

		/*
		 * In case of SAWF denying acceleration through PPE-DS
		 * Allowing acceleration only through PPE-VP
		 * For legacy scs and non-SPM rule case, do not deny acceleration through PPE-DS
		 * TODO: configure accel using DS for SAWF and SPM rule valid case as well.
		 */
		aci = ecm_db_connection_assigned_classifier_find_and_ref(feci->ci, ECM_CLASSIFIER_TYPE_EMESH);
		if (aci) {
			sawf_rule_valid = ecm_classifier_emesh_is_sawf_rule_valid((struct ecm_classifier_emesh_sawf_instance *)aci);
			aci->deref(aci);
		}
		if (!(pr->process_actions & ECM_CLASSIFIER_PROCESS_ACTION_EMESH_SAWF_LEGACY_SCS_TAG) && sawf_rule_valid) {
			spin_lock_bh(&feci->lock);
			feci->fe_info.front_end_flags &= (~ECM_FRONT_END_ENGINE_FLAG_PPE_DS);
			feci->fe_info.front_end_flags |= ECM_FRONT_END_ENGINE_FLAG_PPE_VP;
			spin_unlock_bh(&feci->lock);
		}
        }

        /*
         * VLAN pcp remark set in SAWF classifer, we modify the pcp value in VLAN tag
         * and send the update VLAN tag to PPE.
         */
        if (pr->process_actions & ECM_CLASSIFIER_PROCESS_ACTION_EMESH_SAWF_VLAN_PCP_REMARK) {
                if (pr->flow_vlan_pcp != ECM_FRONT_END_INVALID_VLAN_PCP &&
                                pd6rc->vlan_rule.primary_vlan.egress_vlan_tag != ECM_FRONT_END_VLAN_ID_NOT_CONFIGURED) {
                        pd6rc->vlan_rule.primary_vlan.egress_vlan_tag &= ~VLAN_PRIO_MASK;
                        pd6rc->vlan_rule.primary_vlan.egress_vlan_tag |= pr->flow_vlan_pcp << VLAN_PRIO_SHIFT;
                }

                if (pr->return_vlan_pcp != ECM_FRONT_END_INVALID_VLAN_PCP &&
                                pd6rc->vlan_rule.primary_vlan.ingress_vlan_tag != ECM_FRONT_END_VLAN_ID_NOT_CONFIGURED) {
                        pd6rc->vlan_rule.primary_vlan.ingress_vlan_tag &= ~VLAN_PRIO_MASK;
                        pd6rc->vlan_rule.primary_vlan.ingress_vlan_tag |= pr->return_vlan_pcp << VLAN_PRIO_SHIFT;
                }
        }

#endif

	/*
	 * Policer/ACL info
	 */
	if (pr->process_actions & ECM_CLASSIFIER_PROCESS_ACTION_ACL_ENABLED) {
			pd6rc->valid_flags |= PPE_DRV_V6_VALID_FLAG_ACL_POLICER;
			pd6rc->ap_rule.type = PPE_DRV_RULE_TYPE_FLOW_ACL;

			if (pr->rule_id.acl.flow_acl_id) {
				pd6rc->ap_rule.rule_id.acl.flow_acl_id = pr->rule_id.acl.flow_acl_id;
				pd6rc->ap_rule.rule_id.acl.flags |= PPE_DRV_VALID_FLAG_FLOW_ACL;
			}

			if (pr->rule_id.acl.return_acl_id) {
				pd6rc->ap_rule.rule_id.acl.return_acl_id = pr->rule_id.acl.return_acl_id;
				pd6rc->ap_rule.rule_id.acl.flags |= PPE_DRV_VALID_FLAG_RETURN_ACL;
			}
	} else if (pr->process_actions & ECM_CLASSIFIER_PROCESS_ACTION_POLICER_ENABLED) {
			pd6rc->valid_flags |= PPE_DRV_V6_VALID_FLAG_ACL_POLICER;
			pd6rc->ap_rule.type = PPE_DRV_RULE_TYPE_FLOW_POLICER;

			if (pr->rule_id.policer.flow_policer_id) {
				pd6rc->ap_rule.rule_id.policer.flow_policer_id = pr->rule_id.policer.flow_policer_id;
				pd6rc->ap_rule.rule_id.policer.flags |= PPE_DRV_VALID_FLAG_FLOW_POLICER;
			}

			if (pr->rule_id.policer.return_policer_id) {
				pd6rc->ap_rule.rule_id.policer.return_policer_id = pr->rule_id.policer.return_policer_id;
				pd6rc->ap_rule.rule_id.policer.flags |= PPE_DRV_VALID_FLAG_RETURN_POLICER;
			}
	}

	protocol = ecm_db_connection_protocol_get(feci->ci);

	/*
	 * Set protocol
	 */
	pd6rc->tuple.protocol = (int32_t)protocol;

	/*
	 * The flow_ip is where the connection established from
	 */
	ecm_db_connection_address_get(feci->ci, ECM_DB_OBJ_DIR_FROM, src_ip);
	ECM_IP_ADDR_TO_PPE_IPV6_ADDR(pd6rc->tuple.flow_ip, src_ip);

	/*
	 * The dest_ip is where the connection is established to
	 */
	ecm_db_connection_address_get(feci->ci, ECM_DB_OBJ_DIR_TO, dest_ip);
	ECM_IP_ADDR_TO_PPE_IPV6_ADDR(pd6rc->tuple.return_ip, dest_ip);

	/*
	 * Same approach as above for port information
	 */
	pd6rc->tuple.flow_ident = ecm_db_connection_port_get(feci->ci, ECM_DB_OBJ_DIR_FROM);
	pd6rc->tuple.return_ident = ecm_db_connection_port_get(feci->ci, ECM_DB_OBJ_DIR_TO);

	/*
	 * Get mac addresses.
	 * The src_mac is the mac address of the node that established the connection.
	 * This will work whether the from_node is LAN (egress) or WAN (ingress).
	 */
	ecm_db_connection_node_address_get(feci->ci, ECM_DB_OBJ_DIR_FROM, (uint8_t *)pd6rc->conn_rule.flow_mac);

	/*
	 * The dest_mac is more complex. For egress it is the node address of the 'to' side of the connection.
	 * For ingress it is the node adress of the NAT'ed 'to' IP.
	 * Essentially it is the MAC of node associated with create.dest_ip and this is "to nat" side.
	 */
	ecm_db_connection_node_address_get(feci->ci, ECM_DB_OBJ_DIR_TO, (uint8_t *)pd6rc->conn_rule.return_mac);

	/*
	 * Sync our creation command from the assigned classifiers to get specific additional creation rules.
	 * NOTE: These are called in ascending order of priority and so the last classifier (highest) shall
	 * override any preceding classifiers.
	 * This also gives the classifiers a chance to see that acceleration is being attempted.
	 *
	 * sync_from_v6 is avoided for EMESH classifier as it has callbacks registered with WLAN driver.
	 * Since the default accel mode is "auto" where PPE and SFE both are enabled, it may happen that ppe fails
	 * to accelerate and it may fall back on SFE. In that case these WLAN callbacks will be triggered twice
	 * from both PPE and SFE even if PPE rule create fails. So we sync from
	 * emesh classifier only when acceleration is successfull either from PPE or SFE.
	 */
	assignment_count = ecm_db_connection_classifier_assignments_get_and_ref(feci->ci, assignments);
	for (aci_index = 0; aci_index < assignment_count; ++aci_index) {
		/*
		 * NOTE: The current classifiers do not sync anything to the underlying accel engines.
		 * In the future, if any of the classifiers wants to pass any parameter, these parameters
		 * should be received via this object and copied to the accel engine's create object (nircm).
		*/
		aci = assignments[aci_index];
#ifdef ECM_CLASSIFIER_EMESH_ENABLE
		if ((aci->type_get(aci)) != ECM_CLASSIFIER_TYPE_EMESH) {
			DEBUG_TRACE("%px: sync from: %px, type: %d\n", feci, aci, aci->type_get(aci));
			aci->sync_from_v6(aci, &ecrc);
		}
#else
		DEBUG_TRACE("%px: sync from: %px, type: %d\n", feci, aci, aci->type_get(aci));
		aci->sync_from_v6(aci, &ecrc);
#endif
	}

	/*
	 * Release the interface lists
	 */
	ecm_db_connection_interfaces_deref(from_ifaces, from_ifaces_first);
	ecm_db_connection_interfaces_deref(to_ifaces, to_ifaces_first);

	DEBUG_INFO("%px: Ported Accelerate connection %px\n"
			"Protocol: %d\n"
			"from_mtu: %u\n"
			"to_mtu: %u\n"
			"from_ip: " ECM_IP_ADDR_OCTAL_FMT ":%d\n"
			"to_ip: " ECM_IP_ADDR_OCTAL_FMT ":%d\n"
			"from_mac: %pM\n"
			"to_mac: %pM\n"
			"src_iface_num: %u\n"
			"dest_iface_num: %u\n"
			"src_nexthop_num: %u\n"
			"dest_nexthop_num: %u\n"
			"ingress_inner_vlan_tag: %x\n"
			"egress_inner_vlan_tag: %x\n"
			"ingress_outer_vlan_tag: %x\n"
			"egress_outer_vlan_tag: %x\n"
			"rule_flags: %x\n"
			"valid_flags: %x\n"
			"flow_qos_tag: %x (%u)\n"
			"return_qos_tag: %x (%u)\n"
			"flow_dscp: %x\n"
			"return_dscp: %x\n"
			"flow_sawf mark: %x\n"
			"return_sawf mark: %x\n"
			"conn_rule.rx_if: %d (from iface first:%s)\n"
			"conn_rule.tx_if: %d (to iface first:%s)\n",
			feci,
			feci->ci,
			pd6rc->tuple.protocol,
			pd6rc->conn_rule.flow_mtu,
			pd6rc->conn_rule.return_mtu,
			ECM_IP_ADDR_TO_OCTAL(src_ip), pd6rc->tuple.flow_ident,
			ECM_IP_ADDR_TO_OCTAL(dest_ip), pd6rc->tuple.return_ident,
			pd6rc->conn_rule.flow_mac,
			pd6rc->conn_rule.return_mac,
			pd6rc->conn_rule.rx_if,
			pd6rc->conn_rule.tx_if,
			pd6rc->top_rule.rx_if,
			pd6rc->top_rule.tx_if,
			pd6rc->vlan_rule.primary_vlan.ingress_vlan_tag,
			pd6rc->vlan_rule.primary_vlan.egress_vlan_tag,
			pd6rc->vlan_rule.secondary_vlan.ingress_vlan_tag,
			pd6rc->vlan_rule.secondary_vlan.egress_vlan_tag,
			pd6rc->rule_flags,
			pd6rc->valid_flags,
			pd6rc->qos_rule.flow_qos_tag, pd6rc->qos_rule.flow_qos_tag,
			pd6rc->qos_rule.return_qos_tag, pd6rc->qos_rule.return_qos_tag,
			pd6rc->dscp_rule.flow_dscp,
			pd6rc->dscp_rule.return_dscp,
			pd6rc->sawf_rule.flow_mark,
			pd6rc->sawf_rule.return_mark,
			pd6rc->conn_rule.rx_if, (from_ifaces[from_ifaces_first])->name,
			pd6rc->conn_rule.tx_if, (to_ifaces[to_ifaces_first])->name);

	/*
	 * Now that the rule has been constructed we re-compare the generation occurrance counter.
	 * If there has been a change then we abort because the rule may have been created using
	 * unstable data - especially if another thread has begun regeneration of the connection state.
	 * NOTE: This does not prevent a regen from being flagged immediately after this line of code either,
	 * or while the acceleration rule is in flight to the ppe.
	 * This is only to check for consistency of rule state - not that the state is stale.
	 * Remember that the connection is marked as "accel pending state" so if a regen is flagged immediately
	 * after this check passes, the connection will be decelerated and refreshed very quickly.
	 */
	if (regen_occurrances != ecm_db_connection_regeneration_occurrances_get(feci->ci)) {
		DEBUG_INFO("%px: connection:%px regen occurred - aborting accel rule.\n", feci, feci->ci);
		ecm_ppe_ipv6_accel_pending_clear(feci, ECM_FRONT_END_ACCELERATION_MODE_DECEL);
		ecm_db_connection_assignments_release(assignment_count, assignments);
		kfree(pd6rc);
		return;
	}

	/*
	 * Ref the connection before issuing an PPE rule
	 * NOTE: remember that this will also implicitly hold the feci.
	 */
	ecm_db_connection_ref(feci->ci);

	/*
	 * We are about to issue the command, record the time of transmission
	 */
	spin_lock_bh(&feci->lock);
	feci->stats.cmd_time_begun = jiffies;
	spin_unlock_bh(&feci->lock);

	pd6rc->rule_flags |= PPE_DRV_V6_RULE_FLAG_RETURN_VALID | PPE_DRV_V6_RULE_FLAG_FLOW_VALID;

	if (feci->fe_info.front_end_flags & ECM_FRONT_END_ENGINE_FLAG_PPE_DS) {
		pd6rc->rule_flags |= PPE_DRV_V6_RULE_FLAG_DS_FLOW;
	}

	if (feci->fe_info.front_end_flags & ECM_FRONT_END_ENGINE_FLAG_PPE_VP) {
		pd6rc->rule_flags |= PPE_DRV_V6_RULE_FLAG_VP_FLOW;
	}

	DEBUG_TRACE("%px: ECM IPv6 Ported Rule ready to be pushed in PPE:%px\n", feci, feci->ci);

	/*
	 * Call the rule create function
	 */
	ppe_tx_status = ppe_drv_v6_create(pd6rc);
	if (ppe_tx_status == PPE_DRV_RET_SUCCESS) {
		ecm_ppe_ipv6_accel_done_time_update(feci);

		/*
		 * Take feci->lock till completion of rule create in ECM.
		 */
		spin_lock_bh(&feci->lock);

		/*
		 * Reset the driver_fail count - transmission was okay here.
		 */
		feci->stats.driver_fail = 0;

		if (feci->stats.flush_happened) {
			DEBUG_TRACE("%px: flush happened just after ppe IPv6 rule push success and before calling accelerate_done.\n", feci);
			ecm_ppe_ported_ipv6_handle_flush(feci);

			/*
			 * Release feci->lock taken above.
			 */
			spin_unlock_bh(&feci->lock);

			ecm_db_connection_assignments_release(assignment_count, assignments);
			ecm_db_connection_deref(feci->ci);
			kfree(pd6rc);
			return;
		}

		if (!ecm_ppe_ported_ipv6_accelerate_done(feci)) {
			/*
			 * Connection couldn't be accelerated successfully, as decelerate was pending.
			 */

			DEBUG_INFO("%px: Decelerate was pending(IPv6/ported) %p\n", feci, feci->ci);

			/*
			 * Check if the pending decelerate was done with the defunct process.
			 * If it was, set the is_defunct flag of the feci to false for re-try.
			 */
			if (feci->is_defunct) {
				is_defunct = feci->is_defunct;
				feci->is_defunct = false;
			}

			/*
			 * Release feci->lock taken above.
			 */
			spin_unlock_bh(&feci->lock);

			/*
			 * If the pending decelerate was done through defunct process, we should
			 * re-try it here with the same defunct function, because the purpose of that
			 * process is to remove the connection from the database as well after decelerating it.
			 */
			if (is_defunct) {
				ecm_db_connection_make_defunct(feci->ci);
			} else {
				feci->decelerate(feci);
			}

			ecm_db_connection_assignments_release(assignment_count, assignments);
			ecm_db_connection_deref(feci->ci);
			kfree(pd6rc);
			return;
		}

		/*
		 * Release feci->lock taken above.
		 */
		spin_unlock_bh(&feci->lock);

		ecm_db_connection_deref(feci->ci);
		DEBUG_TRACE("%px: ppe_drv_v6_create() success with ret=%d\n", feci, ppe_tx_status);
		kfree(pd6rc);

		/*
		 * Get the assigned classifiers and call their create notify callbacks. If they are interested in this type of
		 * create, they will handle the event.
		 */
		for (aci_index = 0; aci_index < assignment_count; ++aci_index) {
			struct ecm_classifier_instance *aci;
			aci = assignments[aci_index];
			if (aci->notify_create) {
				aci->notify_create(aci, NULL);
			}
		}
		ecm_db_connection_assignments_release(assignment_count, assignments);

		/*
		 * For emesh classifier sync_from_v6 to be called after rule is successfully created.
		 */
#ifdef ECM_CLASSIFIER_EMESH_ENABLE
		aci = ecm_db_connection_assigned_classifier_find_and_ref(feci->ci, ECM_CLASSIFIER_TYPE_EMESH);
		if (aci) {
			DEBUG_TRACE("%px: sync from: %px, type: %d\n", feci, aci, aci->type_get(aci));
			aci->sync_from_v6(aci, &ecrc);
			aci->deref(aci);
		}
#endif
		return;
	}

	/*
	 * Creation command failed (specific reason ignored).
	 */
	ecm_ppe_ipv6_accel_done_time_update(feci);

	/*
	 * Take feci->lock till completion of rule create in ECM.
	 */
	spin_lock_bh(&feci->lock);

	/*
	 * Creation command failed (specific reason ignored).
	 */
	DEBUG_ASSERT(feci->accel_mode == ECM_FRONT_END_ACCELERATION_MODE_ACCEL_PENDING, "%px: Unexpected mode: %d\n", feci->ci, feci->accel_mode);
	feci->stats.ae_nack++;
	feci->stats.ae_nack_total++;
	if (feci->stats.ae_nack >= feci->stats.ae_nack_limit) {
		/*
		 * Too many PPE rejections
		 */
		result_mode = ECM_FRONT_END_ACCELERATION_MODE_FAIL_ACCEL_ENGINE;
	} else {
		/*
		 * Revert to decelerated
		 */
		result_mode = ECM_FRONT_END_ACCELERATION_MODE_DECEL;
	}

	/*
	 * If connection is now defunct then set mode to ensure no further accel attempts occur
	 */
	if (feci->is_defunct) {
		result_mode = ECM_FRONT_END_ACCELERATION_MODE_FAIL_DEFUNCT;
	}

	spin_lock_bh(&ecm_ppe_ipv6_lock);
	_ecm_ppe_ipv6_accel_pending_clear(feci, result_mode);
	spin_unlock_bh(&ecm_ppe_ipv6_lock);

	/*
	 * Release feci->lock taken above.
	 */
	spin_unlock_bh(&feci->lock);

	/*
	 * Release the connection.
	 */
	ecm_db_connection_assignments_release(assignment_count, assignments);
	ecm_db_connection_deref(feci->ci);
	DEBUG_TRACE("%px: ppe_drv_v6_create() failed with ret=%d\n", feci, ppe_tx_status);
	kfree(pd6rc);
	return;

ported_accel_bad_rule:

	kfree(pd6rc);

	/*
	 * Jump to here when rule data is bad and an offload command cannot be constructed
	 */
	DEBUG_WARN("%px: Accel failed for ported IPv6 flow - bad rule\n", feci);
	ecm_ppe_ipv6_accel_pending_clear(feci, ECM_FRONT_END_ACCELERATION_MODE_FAIL_RULE);
}

/*
 * ecm_ppe_ported_ipv6_connection_destroy_done()
 *	Callback for handling destroy ack/nack calls.
 *	feci->lock is caller's responsibility.
 */
static void ecm_ppe_ported_ipv6_connection_destroy_done(struct ecm_front_end_connection_instance *feci)
{
	/*
	 * Drop decel pending counter
	 */
	spin_lock_bh(&ecm_ppe_ipv6_lock);
	ecm_ppe_ipv6_pending_decel_count--;
	DEBUG_ASSERT(ecm_ppe_ipv6_pending_decel_count >= 0, "Bad decel pending counter\n");
	spin_unlock_bh(&ecm_ppe_ipv6_lock);

	/*
	 * If decel is not still pending then it's possible that the PPE ended acceleration by some other reason e.g. flush
	 * In which case we cannot rely on the response we get here.
	 */
	if (feci->accel_mode != ECM_FRONT_END_ACCELERATION_MODE_DECEL_PENDING) {
		return;
	}

	feci->accel_mode = ECM_FRONT_END_ACCELERATION_MODE_DECEL;

	/*
	 * If connection became defunct then set mode so that no further accel/decel attempts occur.
	 */
	if (feci->is_defunct) {
		feci->accel_mode = ECM_FRONT_END_ACCELERATION_MODE_FAIL_DEFUNCT;
	}

	/*
	 * Ported acceleration ends
	 */
	spin_lock_bh(&ecm_ppe_ipv6_lock);
	ecm_ppe_ported_ipv6_accelerated_count[feci->ported_accelerated_count_index]--;	/* Protocol specific counter */
	DEBUG_ASSERT(ecm_ppe_ported_ipv6_accelerated_count[feci->ported_accelerated_count_index] >= 0, "Bad udp accel counter\n");
	ecm_ppe_ipv6_accelerated_count--;		/* General running counter */
	DEBUG_ASSERT(ecm_ppe_ipv6_accelerated_count >= 0, "Bad accel counter\n");
	spin_unlock_bh(&ecm_ppe_ipv6_lock);
}

/*
 * ecm_ppe_ported_ipv6_connection_decelerate_send()
 *	Call PPE driver to decelerate a connection
 */
static bool ecm_ppe_ported_ipv6_connection_decelerate_send(struct ecm_front_end_connection_instance *feci)
{
	struct ppe_drv_v6_rule_destroy pd6rd;
	ip_addr_t src_ip;
	ip_addr_t dest_ip;
	ppe_drv_ret_t status;
	bool ret;

	/*
	 * Increment the decel pending counter
	 */
	spin_lock_bh(&ecm_ppe_ipv6_lock);
	ecm_ppe_ipv6_pending_decel_count++;
	spin_unlock_bh(&ecm_ppe_ipv6_lock);

	pd6rd.tuple.protocol = (int32_t)ecm_db_connection_protocol_get(feci->ci);

	/*
	 * Get addressing information
	 */
	ecm_db_connection_address_get(feci->ci, ECM_DB_OBJ_DIR_FROM, src_ip);
	ECM_IP_ADDR_TO_PPE_IPV6_ADDR(pd6rd.tuple.flow_ip, src_ip);
	ecm_db_connection_address_get(feci->ci, ECM_DB_OBJ_DIR_TO, dest_ip);
	ECM_IP_ADDR_TO_PPE_IPV6_ADDR(pd6rd.tuple.return_ip, dest_ip);
	pd6rd.tuple.flow_ident = ecm_db_connection_port_get(feci->ci, ECM_DB_OBJ_DIR_FROM);
	pd6rd.tuple.return_ident = ecm_db_connection_port_get(feci->ci, ECM_DB_OBJ_DIR_TO);

	/*
	 * Take a ref to the feci->ci so that it will persist until we get a response from the PPE.
	 * NOTE: This will implicitly hold the feci too.
	 */
	ecm_db_connection_ref(feci->ci);

	/*
	 * We are about to issue the command, record the time of transmission
	 */
	spin_lock_bh(&feci->lock);
	feci->stats.cmd_time_begun = jiffies;
	spin_unlock_bh(&feci->lock);

	/*
	 * Destroy the PPE connection cache entry.
	 */
	status = ppe_drv_v6_destroy(&pd6rd);
	if (status == PPE_DRV_RET_SUCCESS) {
		/*
		 * Take feci->lock till completion of rule destroy in ECM.
		 */
		spin_lock_bh(&feci->lock);

		/*
		 * Reset the driver_fail count - transmission was okay here.
		 */
		feci->stats.driver_fail = 0;
		ecm_ppe_ported_ipv6_connection_destroy_done(feci);

		/*
		 * Release the feci->lock taken above.
		 */
		spin_unlock_bh(&feci->lock);

		/*
		 * Record time duration
		 */
		ecm_ppe_ipv6_decel_done_time_update(feci);

		ecm_db_connection_deref(feci->ci);
		return true;
	}

	/*
	 * PPE Deceleration failed and may need retry.
	 */
	ret = ecm_front_end_destroy_failure_handle(feci);

	/*
	 * Record command duration
	 */
	ecm_ppe_ipv6_decel_done_time_update(feci);

	spin_lock_bh(&ecm_ppe_ipv6_lock);
	ecm_ppe_ipv6_pending_decel_count--;
	DEBUG_ASSERT(ecm_ppe_ipv6_pending_decel_count >= 0, "Bad decel pending counter\n");
	spin_unlock_bh(&ecm_ppe_ipv6_lock);

	ecm_db_connection_deref(feci->ci);
	return ret;
}

/*
 * ecm_ppe_ported_ipv6_connection_decelerate()
 *	Decelerate a connection
 */
static bool ecm_ppe_ported_ipv6_connection_decelerate(struct ecm_front_end_connection_instance *feci)
{
	/*
	 * Check if accel mode is OK for the deceleration.
	 */
	spin_lock_bh(&feci->lock);
	if (!ecm_front_end_common_connection_decelerate_accel_mode_check(feci)) {
		spin_unlock_bh(&feci->lock);
		return false;
	}
	feci->accel_mode = ECM_FRONT_END_ACCELERATION_MODE_DECEL_PENDING;
	spin_unlock_bh(&feci->lock);

	return ecm_ppe_ported_ipv6_connection_decelerate_send(feci);
}

/*
 * ecm_ppe_ported_ipv6_connection_defunct_callback()
 *	Callback to be called when a ported connection has become defunct.
 */
bool ecm_ppe_ported_ipv6_connection_defunct_callback(void *arg, int *accel_mode)
{
	bool ret;
	struct ecm_front_end_connection_instance *feci = (struct ecm_front_end_connection_instance *)arg;
	DEBUG_CHECK_MAGIC(feci, ECM_FRONT_END_CONNECTION_INSTANCE_MAGIC, "%px: magic failed", feci);

	/*
	 * Check if the connection can be defuncted.
	 */
	spin_lock_bh(&feci->lock);
	if (!ecm_front_end_common_connection_defunct_check(feci)) {
		*accel_mode = feci->accel_mode;
		spin_unlock_bh(&feci->lock);
		return false;
	}

	if (!ecm_front_end_common_connection_decelerate_accel_mode_check(feci)) {
		*accel_mode = feci->accel_mode;
		spin_unlock_bh(&feci->lock);
		return false;
	}
	feci->accel_mode = ECM_FRONT_END_ACCELERATION_MODE_DECEL_PENDING;
	spin_unlock_bh(&feci->lock);

	/*
	 * If none of the cases matched above, this means the connection is in the
	 * accel mode, so we force a deceleration.
	 * NOTE: If the mode is accel pending then the decel will be actioned when that is completed.
	 */
	ret = ecm_ppe_ported_ipv6_connection_decelerate_send(feci);

	/*
	 * Copy the accel_mode which is returned from the decelerate message function. This value
	 * will be used in the caller to decide releasing the final reference of the connection.
	 * But if this function reaches to here, the caller care about the ret. If ret is true,
	 * the reference will be released regardless of the accel_mode. If ret is false, accel_mode
	 * will be in the ACCEL state (for destroy re-try) and this state will not be used in the
	 * caller's decision. It looks for ACCEL_FAIL states.
	 */
	spin_lock_bh(&feci->lock);
	*accel_mode = feci->accel_mode;
	spin_unlock_bh(&feci->lock);

	return ret;
}

/*
 * ecm_ppe_ported_ipv6_connection_accel_ceased()
 *	PPE has indicated that acceleration has stopped.
 *
 * NOTE: This is called in response to an PPE self-initiated termination of acceleration.
 * This must NOT be called because the ECM terminated the acceleration.
 */
static void ecm_ppe_ported_ipv6_connection_accel_ceased(struct ecm_front_end_connection_instance *feci)
{
	DEBUG_CHECK_MAGIC(feci, ECM_FRONT_END_CONNECTION_INSTANCE_MAGIC, "%px: magic failed", feci);
	DEBUG_INFO("%px: accel ceased\n", feci);

	spin_lock_bh(&feci->lock);

	/*
	 * If we are in accel-pending state then the PPE has issued a flush out-of-order
	 * with the ACK/NACK we are actually waiting for.
	 * To work around this we record a "flush has already happened" and will action it when we finally get that ACK/NACK.
	 */
	if (feci->accel_mode == ECM_FRONT_END_ACCELERATION_MODE_ACCEL_PENDING) {
		feci->stats.flush_happened = true;
		feci->stats.flush_happened_total++;
		spin_unlock_bh(&feci->lock);
		return;
	}

	/*
	 * If connection is no longer accelerated by the time we get here just ignore the command
	 */
	if (feci->accel_mode != ECM_FRONT_END_ACCELERATION_MODE_ACCEL) {
		spin_unlock_bh(&feci->lock);
		return;
	}

	/*
	 * If the no_action_seen counter was not reset then acceleration ended without any offload action
	 */
	if (feci->stats.no_action_seen) {
		feci->stats.no_action_seen_total++;
	}

	/*
	 * If the no_action_seen indicates successive cessations of acceleration without any offload action occuring
	 * then we fail out this connection
	 */
	if (feci->stats.no_action_seen >= feci->stats.no_action_seen_limit) {
		feci->accel_mode = ECM_FRONT_END_ACCELERATION_MODE_FAIL_NO_ACTION;
	} else {
		feci->accel_mode = ECM_FRONT_END_ACCELERATION_MODE_DECEL;
	}
	spin_unlock_bh(&feci->lock);

	/*
	 * Ported acceleration ends
	 */
	spin_lock_bh(&ecm_ppe_ipv6_lock);
	ecm_ppe_ported_ipv6_accelerated_count[feci->ported_accelerated_count_index]--;	/* Protocol specific counter */
	DEBUG_ASSERT(ecm_ppe_ported_ipv6_accelerated_count[feci->ported_accelerated_count_index] >= 0, "Bad ported accel counter\n");
	ecm_ppe_ipv6_accelerated_count--;		/* General running counter */
	DEBUG_ASSERT(ecm_ppe_ipv6_accelerated_count >= 0, "Bad accel counter\n");
	spin_unlock_bh(&ecm_ppe_ipv6_lock);
}

#ifdef ECM_STATE_OUTPUT_ENABLE
/*
 * ecm_ppe_ported_ipv6_connection_state_get()
 *	Return the state of this ported front end instance
 */
static int ecm_ppe_ported_ipv6_connection_state_get(struct ecm_front_end_connection_instance *feci, struct ecm_state_file_instance *sfi)
{
	DEBUG_CHECK_MAGIC(feci, ECM_FRONT_END_CONNECTION_INSTANCE_MAGIC, "%px: magic failed", feci);

	return ecm_front_end_common_connection_state_get(feci, sfi, "ppe_v6.ported");
}
#endif

/*
 * ecm_ppe_ported_ipv6_connection_set();
 *	Sets the PPE IPv6 ported connection's fields.
 */
void ecm_ppe_ported_ipv6_connection_set(struct ecm_front_end_connection_instance *feci, uint32_t flags)
{
	feci->accel_engine = ECM_FRONT_END_ENGINE_PPE;
	spin_lock_bh(&ecm_ppe_ipv6_lock);
	feci->stats.no_action_seen_limit = ecm_ppe_ipv6_no_action_limit_default;
	feci->stats.driver_fail_limit = ecm_ppe_ipv6_driver_fail_limit_default;
	feci->stats.ae_nack_limit = ecm_ppe_ipv6_nack_limit_default;
	spin_unlock_bh(&ecm_ppe_ipv6_lock);
	feci->accelerate = ecm_ppe_ported_ipv6_connection_accelerate;
	feci->decelerate = ecm_ppe_ported_ipv6_connection_decelerate;
	feci->accel_ceased = ecm_ppe_ported_ipv6_connection_accel_ceased;
#ifdef ECM_STATE_OUTPUT_ENABLE
	feci->state_get = ecm_ppe_ported_ipv6_connection_state_get;
#endif
	feci->ae_interface_number_by_dev_get = ecm_ppe_common_get_interface_number_by_dev;
	feci->ae_interface_number_by_dev_type_get = ecm_ppe_common_get_interface_number_by_dev_type;
	feci->ae_interface_type_get = ecm_ppe_common_get_interface_type;
	feci->regenerate = ecm_ppe_common_connection_regenerate;
	feci->defunct = ecm_ppe_ported_ipv6_connection_defunct_callback;

	feci->get_stats_bitmap = ecm_front_end_common_get_stats_bitmap;
	feci->set_stats_bitmap = ecm_front_end_common_set_stats_bitmap;
	feci->fe_info.front_end_flags = flags;
	feci->next_accel_engine = ECM_FRONT_END_ENGINE_PPE;

	/*
	 * Just in case this function is called while switching AE to PPE
	 * let's reset the failure stats which was increased by the old AE.
	 */
	feci->stats.driver_fail_total = 0;
	feci->stats.driver_fail = 0;
	feci->stats.ae_nack = 0;
	feci->stats.ae_nack_total = 0;
}

/*
 * ecm_ppe_ported_ipv6_connection_instance_alloc()
 *	Create a front end instance specific for ported connection
 */
struct ecm_front_end_connection_instance *ecm_ppe_ported_ipv6_connection_instance_alloc(
								uint32_t accel_flags,
								int protocol,
								struct ecm_db_connection_instance **nci)
{
	struct ecm_front_end_connection_instance *feci;
	struct ecm_db_connection_instance *ci;
	bool can_accel = (accel_flags & ECM_FRONT_END_ENGINE_FLAG_CAN_ACCEL);

	if (ecm_ppe_ipv6_is_conn_limit_reached()) {
		DEBUG_TRACE("Reached connection limit\n");
		return NULL;
	}

	/*
	 * Now allocate the new connection
	 */
	*nci = ecm_db_connection_alloc();
	if (!*nci) {
		DEBUG_WARN("Failed to allocate connection\n");
		return NULL;
	}

	ci = *nci;

	feci = (struct ecm_front_end_connection_instance *)kzalloc(sizeof(struct ecm_front_end_connection_instance), GFP_ATOMIC | __GFP_NOWARN);
	if (!feci) {
		ecm_db_connection_deref(ci);
		DEBUG_WARN("Ported Front end alloc failed\n");
		return NULL;
	}

	/*
	 * Refs is 1 for the creator of the connection
	 */
	feci->refs = 1;
	DEBUG_SET_MAGIC(feci, ECM_FRONT_END_CONNECTION_INSTANCE_MAGIC);
	spin_lock_init(&feci->lock);

	feci->can_accel = can_accel;
	feci->accel_mode = (can_accel) ? ECM_FRONT_END_ACCELERATION_MODE_DECEL : ECM_FRONT_END_ACCELERATION_MODE_FAIL_DENIED;

	/*
	 * Copy reference to connection - no need to ref ci as ci maintains a ref to this instance instead (this instance persists for as long as ci does)
	 */
	feci->ci = ci;
	feci->ip_version = 6;
	feci->protocol = protocol;

	ecm_ppe_ported_ipv6_connection_set(feci, accel_flags);

	if (protocol == IPPROTO_TCP) {
		feci->ported_accelerated_count_index = ECM_FRONT_END_PORTED_PROTO_TCP;
	} else if (protocol == IPPROTO_UDP) {
		feci->ported_accelerated_count_index = ECM_FRONT_END_PORTED_PROTO_UDP;
	} else {
		DEBUG_WARN("%px: Wrong protocol: %d\n", feci, protocol);
		DEBUG_CLEAR_MAGIC(feci);
		kfree(feci);
		ecm_db_connection_deref(ci);
		return NULL;
	}

	return feci;
}

/*
 * ecm_ppe_ported_ipv6_debugfs_init()
 */
bool ecm_ppe_ported_ipv6_debugfs_init(struct dentry *dentry)
{
	if (!ecm_debugfs_create_u32("udp_accelerated_count", S_IRUGO, dentry,
				    &ecm_ppe_ported_ipv6_accelerated_count[ECM_FRONT_END_PORTED_PROTO_UDP])) {
		DEBUG_ERROR("Failed to create ecm ppe ipv6 udp_accelerated_count file in debugfs\n");
		return false;
	}

	if (!ecm_debugfs_create_u32("tcp_accelerated_count", S_IRUGO, dentry,
					&ecm_ppe_ported_ipv6_accelerated_count[ECM_FRONT_END_PORTED_PROTO_TCP])) {
		DEBUG_ERROR("Failed to create ecm ppe ipv6 tcp_accelerated_count file in debugfs\n");
		return false;
	}

	return true;
}
