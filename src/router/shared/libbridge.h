/*
 * Copyright (C) 2000 Lennert Buytenhek
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef _LIBBRIDGE_H
#define _LIBBRIDGE_H
#ifndef __UCLIBC__
#include <netinet/in.h>
#include <sys/time.h>
#endif

struct bridge_id {
	unsigned char prio[2];
	unsigned char addr[6];
};

struct bridge_info {
	struct bridge_id designated_root;
	struct bridge_id bridge_id;
	unsigned root_path_cost;
	struct timeval max_age;
	struct timeval hello_time;
	struct timeval forward_delay;
	struct timeval bridge_max_age;
	struct timeval bridge_hello_time;
	struct timeval bridge_forward_delay;
	unsigned short root_port;
	unsigned char stp_enabled;
	unsigned char topology_change;
	unsigned char topology_change_detected;
	struct timeval ageing_time;
	struct timeval hello_timer_value;
	struct timeval tcn_timer_value;
	struct timeval topology_change_timer_value;
	struct timeval gc_timer_value;
};

struct fdb_entry {
	unsigned char mac_addr[6];
	unsigned short port_no;
	unsigned char is_local;
	struct timeval ageing_timer_value;
};

struct port_info {
	unsigned port_no;
	struct bridge_id designated_root;
	struct bridge_id designated_bridge;
	unsigned short port_id;
	unsigned short designated_port;
	unsigned char priority;
	unsigned char top_change_ack;
	unsigned char config_pending;
	unsigned char state;
	unsigned path_cost;
	unsigned designated_cost;
	struct timeval message_age_timer_value;
	struct timeval forward_delay_timer_value;
	struct timeval hold_timer_value;
	unsigned char hairpin_mode;
};

extern int br_init(void);
extern int br_refresh(void);
extern void br_shutdown(void);

extern const char *br_get_state_name(int state);
void apply_bridgeif(char *ifname, char *realport);
extern int br_add_bridge(const char *brname);
extern int br_del_bridge(const char *brname);
extern int br_add_interface(const char *br, const char *dev);
extern int br_del_interface(const char *br, const char *dev);
extern int br_set_bridge_forward_delay(const char *br, int delay);
extern int br_set_bridge_hello_time(const char *br, struct timeval *tv);
extern int br_set_bridge_max_age(const char *br, int age);
extern int br_set_ageing_time(const char *br, struct timeval *tv);
extern int br_set_stp_state(const char *br, int stp_state);
extern int br_set_bridge_priority(const char *br, int bridge_priority);
extern int br_set_port_priority(const char *br, const char *p, int port_priority);
extern int br_set_path_cost(const char *br, const char *p, int path_cost);
extern int br_read_fdb(const char *br, struct fdb_entry *fdbs, unsigned long skip, int num);
extern void set_multicast_to_unicast(const char *ifname);
extern int br_set_vlan_filtering(const char *br, int on);
#ifdef HAVE_VLAN_FILTERING
extern int br_has_vlan_filtering(void);
#else
static inline int br_has_vlan_filtering(void)
{
	return 0;
}
#endif
#ifndef HAVE_MICRO
extern void sync_multicast_to_unicast(void);
#else
static inline sync_multicast_to_unicast(void)
{
}
#endif
#endif
