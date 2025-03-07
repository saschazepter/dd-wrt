/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 *   Copyright (C) 2020 Samsung Electronics Co., Ltd.
 *   Author(s): Namjae Jeon <linkinjeon@kernel.org>
 */
#ifndef NDR_H
#define NDR_H
struct ndr {
	char	*data;
	int	offset;
	int	length;
};

#define NDR_NTSD_OFFSETOF	0xA0

static int ndr_encode_dos_attr(struct ndr *n, struct xattr_dos_attrib *da);
static int ndr_decode_dos_attr(struct ndr *n, struct xattr_dos_attrib *da);
static int ndr_encode_posix_acl(struct ndr *n, struct user_namespace *user_ns,
			 struct inode *inode, struct xattr_smb_acl *acl,
			 struct xattr_smb_acl *def_acl);
static int ndr_encode_v4_ntacl(struct ndr *n, struct xattr_ntacl *acl);
static int ndr_encode_v3_ntacl(struct ndr *n, struct xattr_ntacl *acl);
static int ndr_decode_v4_ntacl(struct ndr *n, struct xattr_ntacl *acl);
#endif