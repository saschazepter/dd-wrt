/*
 **************************************************************************
 * Copyright (c) 2015-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 **************************************************************************
 */
#include <linux/version.h>
#include <linux/types.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/icmp.h>
#include <linux/kthread.h>
#include <linux/debugfs.h>
#include <linux/pkt_sched.h>
#include <linux/string.h>
#include <net/route.h>
#include <net/ip.h>
#include <net/tcp.h>
#include <asm/unaligned.h>
#include <asm/uaccess.h>	/* for put_user */
#include <net/ipv6.h>
#include <linux/inet.h>
#include <linux/in.h>
#include <linux/udp.h>
#include <linux/tcp.h>
#include <linux/ppp_defs.h>
#include <linux/mroute.h>

#include <linux/inetdevice.h>
#include <linux/if_arp.h>
#include <linux/netfilter_ipv4.h>
#include <linux/netfilter_bridge.h>
#include <linux/if_bridge.h>
#include <net/arp.h>
#include <net/netfilter/nf_conntrack.h>
#include <net/netfilter/nf_conntrack_acct.h>
#include <net/netfilter/nf_conntrack_helper.h>
#include <net/netfilter/nf_conntrack_l4proto.h>
#include <linux/netfilter/nf_conntrack_zones_common.h>
#include <net/netfilter/nf_conntrack_core.h>
#include <net/netfilter/nf_conntrack_timeout.h>
#include <net/netfilter/ipv4/nf_conntrack_ipv4.h>
#include <net/netfilter/ipv4/nf_defrag_ipv4.h>
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
#define DEBUG_LEVEL ECM_SFE_IPV4_DEBUG_LEVEL

#include <sfe_api.h>

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
#ifdef ECM_CLASSIFIER_NL_ENABLE
#include "ecm_classifier_nl.h"
#endif
#ifdef ECM_NON_PORTED_SUPPORT_ENABLE
#include "ecm_sfe_non_ported_ipv4.h"
#endif
#ifdef ECM_MULTICAST_ENABLE
#ifndef ECM_FRONT_END_NSS_ENABLE
#include "ecm_sfe_multicast_ipv4.h"
#endif
#endif
#include "ecm_ipv4.h"
#include "ecm_interface.h"
#include "ecm_sfe_ported_ipv4.h"
#include "ecm_front_end_common.h"
#include "ecm_front_end_ipv4.h"

int ecm_sfe_ipv4_no_action_limit_default = 250;		/* Default no-action limit. */
int ecm_sfe_ipv4_driver_fail_limit_default = 250;		/* Default driver fail limit. */
int ecm_sfe_ipv4_nack_limit_default = 250;			/* Default nack limit. */
int ecm_sfe_ipv4_accelerated_count = 0;			/* Total offloads */
int ecm_sfe_ipv4_pending_accel_count = 0;			/* Total pending offloads issued to the SFE / awaiting completion */
int ecm_sfe_ipv4_pending_decel_count = 0;			/* Total pending deceleration requests issued to the SFE / awaiting completion */

#define ECM_SFE_IPV4_STATS_SYNC_PERIOD msecs_to_jiffies(1000)
#define ECM_SFE_IPV4_STATS_SYNC_UDELAY 4000     /* Delay for 4 ms */
/*
 * Workqueue for the connection sync
 */
struct workqueue_struct *ecm_sfe_ipv4_workqueue;
struct delayed_work ecm_sfe_ipv4_work;
struct sfe_ipv4_msg *ecm_sfe_ipv4_sync_req_msg;
static unsigned long int ecm_sfe_ipv4_next_req_time;
static unsigned long int ecm_sfe_ipv4_roll_check_jiffies;
static unsigned long int ecm_sfe_ipv4_stats_request_success = 0;	/* Number of success stats request */
static unsigned long int ecm_sfe_ipv4_stats_request_fail = 0;		/* Number of failed stats request */
static unsigned long int ecm_sfe_ipv4_stats_request_nack = 0;		/* Number of NACK'd stats request */

/*
 * Limiting the acceleration of connections.
 *
 * By default there is no acceleration limiting.
 * This means that when ECM has more connections (that can be accelerated) than the acceleration
 * engine will allow the ECM will continue to try to accelerate.
 * In this scenario the acceleration engine will begin removal of existing rules to make way for new ones.
 * When the accel_limit_mode is set to FIXED ECM will not permit more rules to be issued than the engine will allow.
 */
uint32_t ecm_sfe_ipv4_accel_limit_mode = ECM_FRONT_END_ACCEL_LIMIT_MODE_UNLIMITED;

/*
 * Locking of the classifier - concurrency control for file global parameters.
 * NOTE: It is safe to take this lock WHILE HOLDING a feci->lock.  The reverse is NOT SAFE.
 */
DEFINE_SPINLOCK(ecm_sfe_ipv4_lock);			/* Protect against SMP access between netfilter, events and private threaded function. */

/*
 * SFE driver linkage
 */
struct sfe_ctx_instance *ecm_sfe_ipv4_mgr = NULL;

static unsigned long ecm_sfe_ipv4_accel_cmd_time_avg_samples = 0;	/* Sum of time taken for the set of accel command samples, used to compute average time for an accel command to complete */
static unsigned long ecm_sfe_ipv4_accel_cmd_time_avg_set = 1;	/* How many samples in the set */
static unsigned long ecm_sfe_ipv4_decel_cmd_time_avg_samples = 0;	/* Sum of time taken for the set of accel command samples, used to compute average time for an accel command to complete */
static unsigned long ecm_sfe_ipv4_decel_cmd_time_avg_set = 1;	/* How many samples in the set */

#ifdef CONFIG_XFRM
static int ecm_sfe_ipv4_reject_acceleration_for_ipsec;		/* Don't accelerate IPSEC traffic */
#endif

/*
 * Debugfs dentry object.
 */
static struct dentry *ecm_sfe_ipv4_dentry;

/*
 * ecm_sfe_ipv4_accel_done_time_update()
 *	Record how long the command took to complete, updating average samples
 */
void ecm_sfe_ipv4_accel_done_time_update(struct ecm_front_end_connection_instance *feci)
{
	unsigned long delta;

	/*
	 * How long did it take the command to complete?
	 */
	spin_lock_bh(&feci->lock);
	feci->stats.cmd_time_completed = jiffies;
	delta = feci->stats.cmd_time_completed - feci->stats.cmd_time_begun;
	spin_unlock_bh(&feci->lock);

	spin_lock_bh(&ecm_sfe_ipv4_lock);
	ecm_sfe_ipv4_accel_cmd_time_avg_samples += delta;
	ecm_sfe_ipv4_accel_cmd_time_avg_set++;
	spin_unlock_bh(&ecm_sfe_ipv4_lock);
}

/*
 * ecm_sfe_ipv4_deccel_done_time_update()
 *	Record how long the command took to complete, updating average samples
 */
void ecm_sfe_ipv4_decel_done_time_update(struct ecm_front_end_connection_instance *feci)
{
	unsigned long delta;

	/*
	 * How long did it take the command to complete?
	 */
	spin_lock_bh(&feci->lock);
	feci->stats.cmd_time_completed = jiffies;
	delta = feci->stats.cmd_time_completed - feci->stats.cmd_time_begun;
	spin_unlock_bh(&feci->lock);

	spin_lock_bh(&ecm_sfe_ipv4_lock);
	ecm_sfe_ipv4_decel_cmd_time_avg_samples += delta;
	ecm_sfe_ipv4_decel_cmd_time_avg_set++;
	spin_unlock_bh(&ecm_sfe_ipv4_lock);
}

/*
 * ecm_sfe_ipv4_process_one_conn_sync_msg()
 *	handle one conn from the SFE.
 */
static void ecm_sfe_ipv4_process_one_conn_sync_msg(struct sfe_ipv4_conn_sync *sync)
{
	struct nf_conntrack_tuple_hash *h;
	struct nf_conntrack_tuple tuple;
	struct nf_conn *ct;
	struct nf_conn_counter *acct;
	struct ecm_db_connection_instance *ci;
	struct ecm_front_end_connection_instance *feci;
	struct neighbour *neigh;
	ip_addr_t flow_ip;
	ip_addr_t return_ip_xlate;
	ip_addr_t return_ip;
	struct ecm_classifier_instance *assignments[ECM_CLASSIFIER_TYPES];
	int aci_index;
	int assignment_count;
	struct ecm_classifier_rule_sync class_sync;
	int flow_dir;
	int return_dir;
	int flow_ident;
	int return_ident_xlate;

	/*
	 * Look up ecm connection with a view to synchronising the connection, classifier and data tracker.
	 * Note that we use _xlate versions for destination - for egressing connections this would be the wan IP address,
	 * but for ingressing this would be the LAN side (non-nat'ed) address and is what we need for lookup of our connection.
	 */
	DEBUG_INFO("%px: SFE Sync, lookup connection using\n"
			"Protocol: %d\n" \
			"src_addr: %pI4n:%d\n" \
			"dest_addr: %pI4n:%d\n",
			sync,
			(int)sync->protocol,
			&sync->flow_ip, (int)sync->flow_ident,
			&sync->return_ip_xlate, (int)sync->return_ident_xlate);

	ECM_NIN4_ADDR_TO_IP_ADDR(flow_ip, sync->flow_ip);
	ECM_NIN4_ADDR_TO_IP_ADDR(return_ip_xlate, sync->return_ip_xlate);
	ECM_NIN4_ADDR_TO_IP_ADDR(return_ip, sync->return_ip);
	flow_ident = (int)sync->flow_ident;
	return_ident_xlate = (int)sync->return_ident_xlate;

	/*
	 * GRE connections such as PPTP-GRE are stored into the db using a 3 tuple based hash.
	 * L2TPv3 over IP connection also uses a 3 tuple based hash.
	 * So we ignore the port information here when trying to lookup the connection.
	 */
	if (sync->protocol == IPPROTO_GRE || sync->protocol == IPPROTO_L2TP) {
		flow_ident = 0;
		return_ident_xlate = 0;
	}

	ci = ecm_db_connection_find_and_ref(flow_ip, return_ip_xlate, sync->protocol, ntohs(flow_ident), ntohs(return_ident_xlate));

	if (!ci) {
		DEBUG_TRACE("%px: SFE Sync: no connection\n", sync);
		goto sync_conntrack;
	}
	DEBUG_TRACE("%px: Sync conn %px\n", sync, ci);

	/*
	 * Copy the sync data to the classifier sync structure to
	 * update the classifiers' stats.
	 */
	class_sync.tx_packet_count[ECM_CONN_DIR_FLOW] = sync->flow_tx_packet_count;
	class_sync.tx_byte_count[ECM_CONN_DIR_FLOW] = sync->flow_tx_byte_count;
	class_sync.rx_packet_count[ECM_CONN_DIR_FLOW] = sync->flow_rx_packet_count;
	class_sync.rx_byte_count[ECM_CONN_DIR_FLOW] = sync->flow_rx_byte_count;
	class_sync.tx_packet_count[ECM_CONN_DIR_RETURN] = sync->return_tx_packet_count;
	class_sync.tx_byte_count[ECM_CONN_DIR_RETURN] = sync->return_tx_byte_count;
	class_sync.rx_packet_count[ECM_CONN_DIR_RETURN] = sync->return_rx_packet_count;
	class_sync.rx_byte_count[ECM_CONN_DIR_RETURN] = sync->return_rx_byte_count;
	class_sync.reason = sync->reason;

	/*
	 * Sync assigned classifiers
	 */
	assignment_count = ecm_db_connection_classifier_assignments_get_and_ref(ci, assignments);
	for (aci_index = 0; aci_index < assignment_count; ++aci_index) {
		struct ecm_classifier_instance *aci;
		aci = assignments[aci_index];
		DEBUG_TRACE("%px: sync to: %px, type: %d\n", ci, aci, aci->type_get(aci));
		aci->sync_to_v4(aci, &class_sync);
	}
	ecm_db_connection_assignments_release(assignment_count, assignments);

	/*
	 * Keep connection alive and updated
	 */
	if (!ecm_db_connection_defunct_timer_touch(ci)) {
		ecm_db_connection_deref(ci);
		goto sync_conntrack;
	}

	/*
	 * Get the front end instance
	 */
	feci = ecm_db_connection_front_end_get_and_ref(ci);

	if (sync->flow_tx_packet_count || sync->return_tx_packet_count) {
		DEBUG_TRACE("%px: flow_rx_packet_count: %u, flow_rx_byte_count: %u, return_rx_packet_count: %u, return_rx_byte_count: %u\n",
				ci, sync->flow_rx_packet_count, sync->flow_rx_byte_count, sync->return_rx_packet_count, sync->return_rx_byte_count);
		DEBUG_TRACE("%px: flow_tx_packet_count: %u, flow_tx_byte_count: %u, return_tx_packet_count: %u, return_tx_byte_count: %u\n",
				ci, sync->flow_tx_packet_count, sync->flow_tx_byte_count, sync->return_tx_packet_count, sync->return_tx_byte_count);

#ifdef ECM_MULTICAST_ENABLE
		if (ecm_ip_addr_is_multicast(return_ip)) {
			/*
			 * The amount of data *sent* by the ECM multicast connection 'from' side is the amount the SFE has *received* in the 'flow' direction.
			 */
			ecm_db_multicast_connection_data_totals_update(ci, true, sync->flow_rx_byte_count, sync->flow_rx_packet_count);
			ecm_db_multicast_connection_data_totals_update(ci, false, sync->return_rx_byte_count, sync->return_rx_packet_count);
			ecm_db_multicast_connection_interface_heirarchy_stats_update(ci, sync->flow_rx_byte_count, sync->flow_rx_packet_count);

			/*
			 * Update interface statistics
			 */
			ecm_interface_multicast_stats_update(ci, sync->flow_tx_packet_count, sync->flow_tx_byte_count, sync->flow_rx_packet_count, sync->flow_rx_byte_count,
										sync->return_tx_packet_count, sync->return_tx_byte_count, sync->return_rx_packet_count, sync->return_rx_byte_count);
			/*
			 * Update IP multicast routing cache stats
			 */
			ipmr_mfc_stats_update(&init_net, htonl(flow_ip[0]), htonl(return_ip[0]), sync->flow_rx_packet_count,
										 sync->flow_rx_byte_count, sync->flow_rx_packet_count, sync->flow_rx_byte_count);
		} else
#endif
		{
			/*
			 * The amount of data *sent* by the ECM connection 'from' side is the amount the SFE has *received* in the 'flow' direction.
			 */
			ecm_db_connection_data_totals_update(ci, true, sync->flow_rx_byte_count, sync->flow_rx_packet_count);

			/*
			 * The amount of data *sent* by the ECM connection 'to' side is the amount the SFE has *received* in the 'return' direction.
			 */
			ecm_db_connection_data_totals_update(ci, false, sync->return_rx_byte_count, sync->return_rx_packet_count);

			/*
			 * Update interface statistics
			 */
			ecm_interface_stats_update(ci, sync->flow_tx_packet_count, sync->flow_tx_byte_count, sync->flow_rx_packet_count, sync->flow_rx_byte_count,
							sync->return_tx_packet_count, sync->return_tx_byte_count, sync->return_rx_packet_count, sync->return_rx_byte_count);
		}

		/*
		 * As packets have been accelerated we have seen some action.
		 */
		ecm_front_end_connection_action_seen(feci);
	}

	switch(sync->reason) {
	case SFE_RULE_SYNC_REASON_DESTROY:
		/*
		 * This is the final sync from the SFE for a connection whose acceleration was
		 * terminated by the ecm.
		 * NOTE: We take no action here since that is performed by the destroy message ack.
		 */
		DEBUG_INFO("%px: ECM initiated final sync seen: %d\n", ci, sync->reason);

		/*
		 * If there is no tx/rx packets to update the other linux subsystems, we shouldn't continue
		 * for the sync message which comes as a final sync for the ECM initiated destroy request.
		 * Because this means the connection is not active for sometime and adding this delta time
		 * to the conntrack timeout will update it eventhough there is no traffic for this connection.
		 * When the CT in destroy status, find ct could cause race
		 * conditions
		 */
		if ((!sync->flow_tx_packet_count && !sync->return_tx_packet_count)
				|| (ci->flags & ECM_DB_CONNECTION_FLAGS_DEFUNCT_CT_DESTROYED)) {
			ecm_front_end_connection_deref(feci);
			ecm_db_connection_deref(ci);
			return;
		}
		break;
	case SFE_RULE_SYNC_REASON_FLUSH:
	case SFE_RULE_SYNC_REASON_EVICT:
		/*
		 * SFE has ended acceleration without instruction from the ECM.
		 */
		DEBUG_INFO("%px: SFE Initiated final sync seen: %d cause:%d\n", ci, sync->reason, sync->cause);

		/*
		 * SFE Decelerated the connection
		 */
		feci->accel_ceased(feci);
		break;
	default:
		if (ecm_db_connection_is_routed_get(ci)) {
			/*
			 * Update the neighbour entry for source IP address
			 */
			neigh = ecm_interface_ipv4_neigh_get(flow_ip);
			if (!neigh) {
				DEBUG_WARN("Neighbour entry for %pI4n not found\n", &sync->flow_ip);
			} else {
				if (sync->flow_tx_packet_count) {
					DEBUG_TRACE("Neighbour entry event send for %pI4n: %px\n", &sync->flow_ip, neigh);
					neigh_event_send(neigh, NULL);
				}

				neigh_release(neigh);
			}

			/*
			 * Update the neighbour entry for destination IP address
			 */
			neigh = ecm_interface_ipv4_neigh_get(return_ip);
			if (!neigh) {
				DEBUG_WARN("Neighbour entry for %pI4n not found\n", &sync->return_ip);
			} else {
				if (sync->return_tx_packet_count) {
					DEBUG_TRACE("Neighbour entry event send for %pI4n: %px\n", &sync->return_ip, neigh);
					neigh_event_send(neigh, NULL);
				}

				neigh_release(neigh);
			}
		}
	}

	/*
	 * If connection should be re-generated then we need to force a deceleration
	 */
	if (unlikely(ecm_db_connection_regeneration_required_peek(ci))) {
		DEBUG_TRACE("%px: Connection generation changing, terminating acceleration", ci);
		feci->decelerate(feci);
	}

	ecm_front_end_connection_deref(feci);
	ecm_db_connection_deref(ci);

sync_conntrack:
	;

	/*
	 * Create a tuple so as to be able to look up a conntrack connection
	 */
	memset(&tuple, 0, sizeof(tuple));
	tuple.src.u3.ip = sync->flow_ip;
	tuple.src.u.all = sync->flow_ident;
	tuple.src.l3num = AF_INET;

	tuple.dst.u3.ip = sync->return_ip;
	tuple.dst.dir = IP_CT_DIR_ORIGINAL;
	tuple.dst.protonum = (uint8_t)sync->protocol;
	tuple.dst.u.all = sync->return_ident;

	DEBUG_TRACE("Conntrack sync, lookup conntrack connection using\n"
			"Protocol: %d\n"
			"src_addr: %pI4:%d\n"
			"dest_addr: %pI4:%d\n",
			(int)tuple.dst.protonum,
			&tuple.src.u3.ip, (int)(ntohs(tuple.src.u.all)),
			&tuple.dst.u3.ip, (int)(ntohs(tuple.dst.u.all)));

	/*
	 * Look up conntrack connection
	 */
	h = nf_conntrack_find_get(&init_net, &nf_ct_zone_dflt, &tuple);
	if (!h) {
		DEBUG_WARN("%px: SFE Sync: no conntrack connection\n", sync);
		return;
	}

	ct = nf_ct_tuplehash_to_ctrack(h);
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 9, 0))
	NF_CT_ASSERT(ct->timeout.data == (unsigned long)ct);
#endif
	DEBUG_TRACE("%px: SFE Sync: conntrack connection\n", ct);

	ecm_front_end_flow_and_return_directions_get(ct, flow_ip, 4, &flow_dir, &return_dir);

	/*
	 * Only update if this is not a fixed timeout
	 */
	if (!test_bit(IPS_FIXED_TIMEOUT_BIT, &ct->status)) {
		unsigned long int delta_jiffies;

		/*
		 * Convert ms ticks from the SFE to jiffies. We know that inc_ticks is small
		 * and we expect HZ to be small too so we can multiply without worrying about
		 * wrap-around problems.  We add a rounding constant to ensure that the different
		 * time bases don't cause truncation errors.
		 */
		delta_jiffies = ((sync->inc_ticks * HZ) + (MSEC_PER_SEC / 2)) / MSEC_PER_SEC;

		spin_lock_bh(&ct->lock);
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 9, 0))
		ct->timeout.expires += delta_jiffies;
#else
		ct->timeout += delta_jiffies;
#endif
		spin_unlock_bh(&ct->lock);
	}

	acct = nf_conn_acct_find(ct)->counter;
	if (acct) {
		spin_lock_bh(&ct->lock);
		atomic64_add(sync->flow_rx_packet_count, &acct[flow_dir].packets);
		atomic64_add(sync->flow_rx_byte_count, &acct[flow_dir].bytes);

		atomic64_add(sync->return_rx_packet_count, &acct[return_dir].packets);
		atomic64_add(sync->return_rx_byte_count, &acct[return_dir].bytes);
		spin_unlock_bh(&ct->lock);
	}

	switch (sync->protocol) {
	case IPPROTO_TCP:
		spin_lock_bh(&ct->lock);
		if (ct->proto.tcp.seen[flow_dir].td_maxwin < sync->flow_max_window) {
			ct->proto.tcp.seen[flow_dir].td_maxwin = sync->flow_max_window;
		}
		if ((int32_t)(ct->proto.tcp.seen[flow_dir].td_end - sync->flow_end) < 0) {
			ct->proto.tcp.seen[flow_dir].td_end = sync->flow_end;
		}
		if ((int32_t)(ct->proto.tcp.seen[flow_dir].td_maxend - sync->flow_max_end) < 0) {
			ct->proto.tcp.seen[flow_dir].td_maxend = sync->flow_max_end;
		}
		if (ct->proto.tcp.seen[return_dir].td_maxwin < sync->return_max_window) {
			ct->proto.tcp.seen[return_dir].td_maxwin = sync->return_max_window;
		}
		if ((int32_t)(ct->proto.tcp.seen[return_dir].td_end - sync->return_end) < 0) {
			ct->proto.tcp.seen[return_dir].td_end = sync->return_end;
		}
		if ((int32_t)(ct->proto.tcp.seen[return_dir].td_maxend - sync->return_max_end) < 0) {
			ct->proto.tcp.seen[return_dir].td_maxend = sync->return_max_end;
		}
		spin_unlock_bh(&ct->lock);
		break;
	case IPPROTO_UDP:
		/*
		 * In Linux connection track, UDP flow has two timeout values:
		 * /proc/sys/net/netfilter/nf_conntrack_udp_timeout:
		 * 	this is for uni-direction UDP flow, normally its value is 60 seconds
		 * /proc/sys/net/netfilter/nf_conntrack_udp_timeout_stream:
		 * 	this is for bi-direction UDP flow, normally its value is 180 seconds
		 *
		 * Linux will update timer of UDP flow to stream timeout once it seen packets
		 * in reply direction. But if flow is accelerated by NSS or SFE, Linux won't
		 * see any packets. So we have to do the same thing in our stats sync message.
		 */
		if (!test_bit(IPS_ASSURED_BIT, &ct->status) && acct) {
			u_int64_t reply_pkts = atomic64_read(&acct[IP_CT_DIR_REPLY].packets);

			if (reply_pkts != 0) {
				struct nf_conntrack_l4proto *l4proto __maybe_unused;
				unsigned int *timeouts;

				set_bit(IPS_SEEN_REPLY_BIT, &ct->status);
				set_bit(IPS_ASSURED_BIT, &ct->status);
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 19, 0))
				l4proto = __nf_ct_l4proto_find(AF_INET, IPPROTO_UDP);
				timeouts = nf_ct_timeout_lookup(&init_net, ct, l4proto);

				spin_lock_bh(&ct->lock);
				ct->timeout.expires = jiffies + timeouts[UDP_CT_REPLIED];
				spin_unlock_bh(&ct->lock);
#else
				timeouts = nf_ct_timeout_lookup(ct);
				if (!timeouts) {
					struct nf_udp_net *un = nf_udp_pernet(nf_ct_net(ct));
					timeouts = un->timeouts;
				}

				spin_lock_bh(&ct->lock);
				ct->timeout = jiffies + timeouts[UDP_CT_REPLIED];
				spin_unlock_bh(&ct->lock);
#endif
			}
		}
		break;
	}

	/*
	 * Release connection
	 */
	nf_ct_put(ct);
}

/*
 * ecm_sfe_ipv4_stats_sync_callback()
 *	Callback handler from the SFE.
 */
static void ecm_sfe_ipv4_stats_sync_callback(void *app_data, struct sfe_ipv4_msg *nim)
{
	/*
	 * Only respond to sync messages
	 */
	if (nim->cm.type != SFE_RX_CONN_STATS_SYNC_MSG) {
		DEBUG_TRACE("Ignoring nim: %px - not sync: %d", nim, nim->cm.type);
		return;
	}
	ecm_sfe_ipv4_process_one_conn_sync_msg(&nim->msg.conn_stats);
}

/*
 * ecm_sfe_ipv4_get_accel_limit_mode()
 */
static int ecm_sfe_ipv4_get_accel_limit_mode(void *data, u64 *val)
{
	*val = ecm_sfe_ipv4_accel_limit_mode;

	return 0;
}

/*
 * ecm_sfe_ipv4_set_accel_limit_mode()
 */
static int ecm_sfe_ipv4_set_accel_limit_mode(void *data, u64 val)
{
	DEBUG_TRACE("ecm_sfe_ipv4_accel_limit_mode = %x\n", (int)val);

	/*
	 * Check that only valid bits are set.
	 * It's fine for no bits to be set as that suggests no modes are wanted.
	 */
	if (val && (val ^ (ECM_FRONT_END_ACCEL_LIMIT_MODE_FIXED | ECM_FRONT_END_ACCEL_LIMIT_MODE_UNLIMITED))) {
		DEBUG_WARN("ecm_sfe_ipv4_accel_limit_mode = %x bad\n", (int)val);
		return -EINVAL;
	}

	ecm_sfe_ipv4_accel_limit_mode = (int)val;

	return 0;
}

/*
 * Debugfs attribute for accel limit mode.
 */
DEFINE_SIMPLE_ATTRIBUTE(ecm_sfe_ipv4_accel_limit_mode_fops, ecm_sfe_ipv4_get_accel_limit_mode, ecm_sfe_ipv4_set_accel_limit_mode, "%llu\n");

/*
 * ecm_sfe_ipv4_get_accel_cmd_avg_millis()
 */
static ssize_t ecm_sfe_ipv4_get_accel_cmd_avg_millis(struct file *file,
								char __user *user_buf,
								size_t sz, loff_t *ppos)
{
	unsigned long set;
	unsigned long samples;
	unsigned long avg;
	char *buf;
	int ret;

	buf = kmalloc(PAGE_SIZE, GFP_KERNEL);
	if (!buf) {
		return -ENOMEM;
	}

	/*
	 * Operate under our locks.
	 * Compute the average of the samples taken and seed the next set of samples with the result of this one.
	 */
	spin_lock_bh(&ecm_sfe_ipv4_lock);
	samples = ecm_sfe_ipv4_accel_cmd_time_avg_samples;
	set = ecm_sfe_ipv4_accel_cmd_time_avg_set;
	ecm_sfe_ipv4_accel_cmd_time_avg_samples /= ecm_sfe_ipv4_accel_cmd_time_avg_set;
	ecm_sfe_ipv4_accel_cmd_time_avg_set = 1;
	avg = ecm_sfe_ipv4_accel_cmd_time_avg_samples;
	spin_unlock_bh(&ecm_sfe_ipv4_lock);

	/*
	 * Convert average jiffies to milliseconds
	 */
	avg *= 1000;
	avg /= HZ;

	ret = snprintf(buf, (ssize_t)PAGE_SIZE, "avg=%lu\tsamples=%lu\tset_size=%lu\n", avg, samples, set);
	if (ret < 0) {
		kfree(buf);
		return -EFAULT;
	}

	ret = simple_read_from_buffer(user_buf, sz, ppos, buf, ret);
	kfree(buf);
	return ret;
}

/*
 * File operations for accel command average time.
 */
static struct file_operations ecm_sfe_ipv4_accel_cmd_avg_millis_fops = {
	.read = ecm_sfe_ipv4_get_accel_cmd_avg_millis,
};

/*
 * ecm_sfe_ipv4_get_decel_average_millis()
 */
static ssize_t ecm_sfe_ipv4_get_decel_cmd_avg_millis(struct file *file,
								char __user *user_buf,
								size_t sz, loff_t *ppos)
{
	unsigned long set;
	unsigned long samples;
	unsigned long avg;
	char *buf;
	int ret;

	buf = kmalloc(PAGE_SIZE, GFP_KERNEL);
	if (!buf) {
		return -ENOMEM;
	}

	/*
	 * Operate under our locks.
	 * Compute the average of the samples taken and seed the next set of samples with the result of this one.
	 */
	spin_lock_bh(&ecm_sfe_ipv4_lock);
	samples = ecm_sfe_ipv4_decel_cmd_time_avg_samples;
	set = ecm_sfe_ipv4_decel_cmd_time_avg_set;
	ecm_sfe_ipv4_decel_cmd_time_avg_samples /= ecm_sfe_ipv4_decel_cmd_time_avg_set;
	ecm_sfe_ipv4_decel_cmd_time_avg_set = 1;
	avg = ecm_sfe_ipv4_decel_cmd_time_avg_samples;
	spin_unlock_bh(&ecm_sfe_ipv4_lock);

	/*
	 * Convert average jiffies to milliseconds
	 */
	avg *= 1000;
	avg /= HZ;

	ret = snprintf(buf, (ssize_t)PAGE_SIZE, "avg=%lu\tsamples=%lu\tset_size=%lu\n", avg, samples, set);
	if (ret < 0) {
		kfree(buf);
		return -EFAULT;
	}

	ret = simple_read_from_buffer(user_buf, sz, ppos, buf, ret);
	kfree(buf);
	return ret;
}

/*
 * File operations for decel command average time.
 */
static struct file_operations ecm_sfe_ipv4_decel_cmd_avg_millis_fops = {
	.read = ecm_sfe_ipv4_get_decel_cmd_avg_millis,
};

/*
 * ecm_sfe_ipv4_get_stats_request_counter()
 */
static ssize_t ecm_sfe_ipv4_get_stats_request_counter(struct file *file,
								char __user *user_buf,
								size_t sz, loff_t *ppos)
{
	char *buf;
	int ret;

	buf = kmalloc(PAGE_SIZE, GFP_KERNEL);
	if (!buf) {
		return -ENOMEM;
	}

	ret = snprintf(buf, (ssize_t)PAGE_SIZE, "success=%lu\tfail=%lu\tnack=%lu\t\n",
			ecm_sfe_ipv4_stats_request_success, ecm_sfe_ipv4_stats_request_fail,
			ecm_sfe_ipv4_stats_request_nack);
	if (ret < 0) {
		kfree(buf);
		return -EFAULT;
	}

	ret = simple_read_from_buffer(user_buf, sz, ppos, buf, ret);
	kfree(buf);
	return ret;
}

/*
 * File operations for decel command average time.
 */
static struct file_operations ecm_sfe_ipv4_stats_request_counter_fops = {
	.read = ecm_sfe_ipv4_get_stats_request_counter,
};

/*
 * ecm_sfe_ipv4_connection_sync_many_callback()
 *	Callback for conn_sync_many request message
 */
static void ecm_sfe_ipv4_connection_sync_many_callback(void *app_data, struct sfe_ipv4_msg *nim)
{
	struct sfe_ipv4_conn_sync_many_msg *nicsm = &nim->msg.conn_stats_many;
	int i;

	/*
	 * If ECM is terminating, don't process this final stats
	 */
	if (ecm_ipv4_terminate_pending) {
		return;
	}

	if (nim->cm.response == SFE_CMN_RESPONSE_ACK) {
		for (i = 0; i < nicsm->count; i++) {
			ecm_sfe_ipv4_process_one_conn_sync_msg(&nicsm->conn_sync[i]);
		}
		ecm_sfe_ipv4_sync_req_msg->msg.conn_stats_many.index = nicsm->next;
	} else {
		/*
		 * We get a NACK from FW, which should not happen, restart the request
		 */
		DEBUG_WARN("IPv4 conn stats request failed, restarting\n");
		ecm_sfe_ipv4_stats_request_nack++;
		ecm_sfe_ipv4_sync_req_msg->msg.conn_stats_many.index = 0;
	}
	queue_delayed_work(ecm_sfe_ipv4_workqueue, &ecm_sfe_ipv4_work, 0);
}

/*
 * ecm_sfe_ipv4_stats_sync_req_work()
 *      Schedule delayed work to process connection stats and request next sync
 */
static void ecm_sfe_ipv4_stats_sync_req_work(struct work_struct *work)
{
	/*
	 * Prepare a sfe_ipv4_msg with CONN_STATS_SYNC_MANY request
	 */
	struct sfe_ipv4_conn_sync_many_msg *nicsm_req = &ecm_sfe_ipv4_sync_req_msg->msg.conn_stats_many;
	sfe_tx_status_t sfe_tx_status;
	int retry = 3;
	unsigned long int current_jiffies;

	spin_lock_bh(&ecm_sfe_ipv4_lock);
	if (ecm_sfe_ipv4_accelerated_count == 0) {
		spin_unlock_bh(&ecm_sfe_ipv4_lock);
		DEBUG_TRACE("There is no accelerated IPv4 connection\n");
		goto reschedule;
	}
	spin_unlock_bh(&ecm_sfe_ipv4_lock);

	usleep_range(ECM_SFE_IPV4_STATS_SYNC_UDELAY - 100, ECM_SFE_IPV4_STATS_SYNC_UDELAY);

	/*
	 * If index is 0, we are starting a new round, but if we still have time remain
	 * in this round, sleep until it ends
	 */
	if (nicsm_req->index == 0) {
		current_jiffies = jiffies;

		if (time_is_after_jiffies(ecm_sfe_ipv4_roll_check_jiffies))  {
			ecm_sfe_ipv4_next_req_time = jiffies + ECM_SFE_IPV4_STATS_SYNC_PERIOD;
		}

		if (time_after(ecm_sfe_ipv4_next_req_time, current_jiffies)) {
			msleep(jiffies_to_msecs(ecm_sfe_ipv4_next_req_time - current_jiffies));
		}
		ecm_sfe_ipv4_roll_check_jiffies = jiffies;
		ecm_sfe_ipv4_next_req_time = ecm_sfe_ipv4_roll_check_jiffies + ECM_SFE_IPV4_STATS_SYNC_PERIOD;
	}

	while (retry) {
		if (ecm_ipv4_terminate_pending) {
			return;
		}
		sfe_tx_status = sfe_ipv4_tx(ecm_sfe_ipv4_mgr, ecm_sfe_ipv4_sync_req_msg);
		if (sfe_tx_status == SFE_TX_SUCCESS) {
			ecm_sfe_ipv4_stats_request_success++;
			return;
		}
		ecm_sfe_ipv4_stats_request_fail++;
		retry--;
		DEBUG_TRACE("TX_NOT_OKAY, try again later\n");
		usleep_range(100, 200);
	}

reschedule:
	/*
	 * TX failed after retries, reschedule ourselves with fresh start
	 */
	nicsm_req->count = 0;
	nicsm_req->index = 0;
	queue_delayed_work(ecm_sfe_ipv4_workqueue, &ecm_sfe_ipv4_work, ECM_SFE_IPV4_STATS_SYNC_PERIOD);
}

/*
 * ecm_sfe_ipv4_sync_queue_init
 *	Initialize the workqueue for ipv4 stats sync
 */
static bool ecm_sfe_ipv4_sync_queue_init(void)
{
	struct sfe_ipv4_conn_sync_many_msg *nicsm;

	/*
	 * Setup the connection sync msg/work/workqueue
	 */
	ecm_sfe_ipv4_sync_req_msg = kzalloc(PAGE_SIZE, GFP_KERNEL);
	if (!ecm_sfe_ipv4_sync_req_msg) {
		return false;
	}

	sfe_ipv4_msg_init(ecm_sfe_ipv4_sync_req_msg, SFE_SPECIAL_INTERFACE_IPV4,
		SFE_TX_CONN_STATS_SYNC_MANY_MSG,
		sizeof(struct sfe_ipv4_conn_sync_many_msg),
		NULL,
		NULL);

	nicsm = &ecm_sfe_ipv4_sync_req_msg->msg.conn_stats_many;

	/*
	 * Start with index 0 and calculate the size of the conn stats array
	 */
	nicsm->index = 0;
	nicsm->size = PAGE_SIZE;

	ecm_sfe_ipv4_workqueue = create_singlethread_workqueue("ecm_sfe_ipv4_workqueue");
	if (!ecm_sfe_ipv4_workqueue) {
		kfree(ecm_sfe_ipv4_sync_req_msg);
		return false;
	}
	INIT_DELAYED_WORK(&ecm_sfe_ipv4_work, ecm_sfe_ipv4_stats_sync_req_work);
	queue_delayed_work(ecm_sfe_ipv4_workqueue, &ecm_sfe_ipv4_work, ECM_SFE_IPV4_STATS_SYNC_PERIOD);

	return true;
}

/*
 * ecm_sfe_ipv4_sync_queue_exit
 *	 the workqueue for ipv4 stats sync
 */
static void ecm_sfe_ipv4_sync_queue_exit(void)
{
	/*
	 * Cancel the conn sync req work and destroy workqueue
	 */
	cancel_delayed_work_sync(&ecm_sfe_ipv4_work);
	destroy_workqueue(ecm_sfe_ipv4_workqueue);
	kfree(ecm_sfe_ipv4_sync_req_msg);
}

/*
 * ecm_sfe_ipv4_init()
 */
int ecm_sfe_ipv4_init(struct dentry *dentry)
{
	int result = -1;

	if (!ecm_front_end_is_feature_supported(ECM_FE_FEATURE_SFE)) {
		DEBUG_INFO("SFE IPv4 is disabled\n");
		return 0;
	}

	DEBUG_INFO("ECM SFE IPv4 init\n");

	ecm_sfe_ipv4_dentry = debugfs_create_dir("ecm_sfe_ipv4", dentry);
	if (!ecm_sfe_ipv4_dentry) {
		DEBUG_ERROR("Failed to create ecm sfe ipv4 directory in debugfs\n");
		return result;
	}

#ifdef CONFIG_XFRM
	if (!ecm_debugfs_create_u32("reject_acceleration_for_ipsec", S_IRUGO | S_IWUSR, ecm_sfe_ipv4_dentry,
					(u32 *)&ecm_sfe_ipv4_reject_acceleration_for_ipsec)) {
		DEBUG_ERROR("Failed to create ecm sfe ipv4 reject_acceleration_for_ipsec file in debugfs\n");
		goto task_cleanup;
	}
#endif

	if (!ecm_debugfs_create_u32("no_action_limit_default", S_IRUGO | S_IWUSR, ecm_sfe_ipv4_dentry,
					(u32 *)&ecm_sfe_ipv4_no_action_limit_default)) {
		DEBUG_ERROR("Failed to create ecm sfe ipv4 no_action_limit_default file in debugfs\n");
		goto task_cleanup;
	}

	if (!ecm_debugfs_create_u32("driver_fail_limit_default", S_IRUGO | S_IWUSR, ecm_sfe_ipv4_dentry,
					(u32 *)&ecm_sfe_ipv4_driver_fail_limit_default)) {
		DEBUG_ERROR("Failed to create ecm sfe ipv4 driver_fail_limit_default file in debugfs\n");
		goto task_cleanup;
	}

	if (!ecm_debugfs_create_u32("nack_limit_default", S_IRUGO | S_IWUSR, ecm_sfe_ipv4_dentry,
					(u32 *)&ecm_sfe_ipv4_nack_limit_default)) {
		DEBUG_ERROR("Failed to create ecm sfe ipv4 nack_limit_default file in debugfs\n");
		goto task_cleanup;
	}

	if (!ecm_debugfs_create_u32("accelerated_count", S_IRUGO, ecm_sfe_ipv4_dentry,
					(u32 *)&ecm_sfe_ipv4_accelerated_count)) {
		DEBUG_ERROR("Failed to create ecm sfe ipv4 accelerated_count file in debugfs\n");
		goto task_cleanup;
	}

	if (!ecm_debugfs_create_u32("pending_accel_count", S_IRUGO, ecm_sfe_ipv4_dentry,
					(u32 *)&ecm_sfe_ipv4_pending_accel_count)) {
		DEBUG_ERROR("Failed to create ecm sfe ipv4 pending_accel_count file in debugfs\n");
		goto task_cleanup;
	}

	if (!ecm_debugfs_create_u32("pending_decel_count", S_IRUGO, ecm_sfe_ipv4_dentry,
					(u32 *)&ecm_sfe_ipv4_pending_decel_count)) {
		DEBUG_ERROR("Failed to create ecm sfe ipv4 pending_decel_count file in debugfs\n");
		goto task_cleanup;
	}

	if (!debugfs_create_file("accel_limit_mode", S_IRUGO | S_IWUSR, ecm_sfe_ipv4_dentry,
					NULL, &ecm_sfe_ipv4_accel_limit_mode_fops)) {
		DEBUG_ERROR("Failed to create ecm sfe ipv4 accel_limit_mode file in debugfs\n");
		goto task_cleanup;
	}

	if (!debugfs_create_file("accel_cmd_avg_millis", S_IRUGO, ecm_sfe_ipv4_dentry,
					NULL, &ecm_sfe_ipv4_accel_cmd_avg_millis_fops)) {
		DEBUG_ERROR("Failed to create ecm sfe ipv4 accel_cmd_avg_millis file in debugfs\n");
		goto task_cleanup;
	}

	if (!debugfs_create_file("decel_cmd_avg_millis", S_IRUGO, ecm_sfe_ipv4_dentry,
					NULL, &ecm_sfe_ipv4_decel_cmd_avg_millis_fops)) {
		DEBUG_ERROR("Failed to create ecm sfe ipv4 decel_cmd_avg_millis file in debugfs\n");
		goto task_cleanup;
	}

	if (!ecm_sfe_ported_ipv4_debugfs_init(ecm_sfe_ipv4_dentry)) {
		DEBUG_ERROR("Failed to create ecm ported files in debugfs\n");
		goto task_cleanup;
	}

	if (!debugfs_create_file("stats_request_counter", S_IRUGO, ecm_sfe_ipv4_dentry,
					NULL, &ecm_sfe_ipv4_stats_request_counter_fops))
	{
		DEBUG_ERROR("Failed to create ecm sfe ipv4 stats request ounter file in debugfs\n");
		goto task_cleanup;
	}

#ifdef ECM_NON_PORTED_SUPPORT_ENABLE
	if (!ecm_sfe_non_ported_ipv4_debugfs_init(ecm_sfe_ipv4_dentry)) {
		DEBUG_ERROR("Failed to create ecm non-ported files in debugfs\n");
		goto task_cleanup;
	}
#endif
	/*
	 * Register this module with SFE.
	 * Notify manager should be registered before the netfilter hooks. Because there
	 * is a possibility that the ECM can try to send acceleration messages to the
	 * acceleration engine without having an acceleration engine manager.
	 */
	if (!ecm_sfe_ipv4_sync_queue_init()) {
		DEBUG_ERROR("Failed to init queue\n");
		goto task_cleanup;
	}
	ecm_sfe_ipv4_mgr = sfe_ipv4_notify_register(ecm_sfe_ipv4_stats_sync_callback, ecm_sfe_ipv4_connection_sync_many_callback, NULL);

#ifdef ECM_MULTICAST_ENABLE
#ifndef ECM_FRONT_END_NSS_ENABLE
	if (ecm_sfe_multicast_ipv4_init(ecm_sfe_ipv4_dentry)) {
		DEBUG_ERROR("Failed to init sfe multicast\n");
		goto task_cleanup;
	}
#endif
#endif
	return 0;

task_cleanup:

	debugfs_remove_recursive(ecm_sfe_ipv4_dentry);
	return result;
}
EXPORT_SYMBOL(ecm_sfe_ipv4_init);

/*
 * ecm_sfe_ipv4_exit()
 */
void ecm_sfe_ipv4_exit(void)
{
	if (!ecm_front_end_is_feature_supported(ECM_FE_FEATURE_SFE)) {
		DEBUG_INFO("SFE IPv4 is disabled\n");
		return;
	}

#ifdef ECM_MULTICAST_ENABLE
#ifndef ECM_FRONT_END_NSS_ENABLE
	ecm_sfe_multicast_ipv4_exit();
#endif
#endif
	DEBUG_INFO("ECM SFE IPv4 Module exit\n");
	sfe_ipv4_notify_unregister();
	ecm_sfe_ipv4_sync_queue_exit();

	/*
	 * Remove the debugfs files recursively.
	 */
	if (ecm_sfe_ipv4_dentry) {
		debugfs_remove_recursive(ecm_sfe_ipv4_dentry);
	}
}
EXPORT_SYMBOL(ecm_sfe_ipv4_exit);
