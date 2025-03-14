/*
 * Copyright (c) 2012,2018, The Linux Foundation. All rights reserved.
 *
 * Copyright (c) 2022-2023, Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all copies.
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef _REF_FDB_H_
#define _REF_FDB_H_

#ifdef __cplusplus
extern "C" {
#endif                          /* __cplusplus */

#include <linux/version.h>
#if defined(IN_SWCONFIG)
#if defined(CONFIG_OF) && (LINUX_VERSION_CODE >= KERNEL_VERSION(3,14,0))
#include <linux/switch.h>
#else
#include <net/switch.h>
#endif
#endif

#include <linux/notifier.h>
#include "sw.h"
#include "fal/fal_type.h"
#include "fal/fal_fdb.h"
#include "init/ssdk_init.h"
#include "init/ssdk_plat.h"

#define MAX_PORT 6

#define REF_FDB_MAC_INVALID 0
#define REF_FDB_MAC_NEW 1
#define REF_FDB_MAC_OLD 2
#define REF_FDB_MAC_EXPIRE 3

typedef struct
{
	struct list_head list;
	a_uint8_t state;
	a_bool_t notified;
	fal_fdb_entry_t entry;
} ref_fdb_info_t;

#if defined(IN_SWCONFIG)
int
	qca_ar8327_sw_atu_flush(struct switch_dev *dev,
					const struct switch_attr *attr,
					struct switch_val *val);

int
qca_ar8327_sw_atu_dump(struct switch_dev *dev,
		       		const struct switch_attr *attr,
		       		struct switch_val *val);
#endif

fal_port_t
ref_fdb_get_port_by_mac(unsigned int vid, const char * addr);

sw_error_t
ref_fdb_sw_sync_task(struct qca_phy_priv *priv);
sw_error_t
ref_fdb_sw_sync_reset(struct qca_phy_priv *priv, fal_pbmp_t port_map);

#ifdef __cplusplus
}
#endif                          /* __cplusplus */
#endif                          /* _REF_FDB_H_ */

