/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 *   Copyright (C) 2018 Samsung Electronics Co., Ltd.
 */

#ifndef __TREE_CONNECT_MANAGEMENT_H__
#define __TREE_CONNECT_MANAGEMENT_H__

#include <linux/hashtable.h>

#include "../ksmbd_netlink.h"

struct ksmbd_share_config;
struct ksmbd_conn;
struct ksmbd_user;

struct ksmbd_tree_connect {
	int				id;

	unsigned int			flags;
	struct ksmbd_share_config	*share_conf;
	struct ksmbd_user		*user;

	struct list_head		list;

	int				maximal_access;
	bool				posix_extensions;
};

struct ksmbd_tree_conn_status {
	unsigned int			ret;
	struct ksmbd_tree_connect	*tree_conn;
};

static inline int test_tree_conn_flag(struct ksmbd_tree_connect *tree_conn,
				      int flag)
{
	return tree_conn->flags & flag;
}

struct ksmbd_session;

static struct ksmbd_tree_conn_status
ksmbd_tree_conn_connect(struct ksmbd_conn *conn, struct ksmbd_session *sess,
			const char *share_name);

static int ksmbd_tree_conn_disconnect(struct ksmbd_session *sess,
			       struct ksmbd_tree_connect *tree_conn);

static struct ksmbd_tree_connect *ksmbd_tree_conn_lookup(struct ksmbd_session *sess,
						  unsigned int id);

static struct ksmbd_share_config *ksmbd_tree_conn_share(struct ksmbd_session *sess,
						 unsigned int id);

static int ksmbd_tree_conn_session_logoff(struct ksmbd_session *sess);

#endif /* __TREE_CONNECT_MANAGEMENT_H__ */
