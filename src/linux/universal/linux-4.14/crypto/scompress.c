/*
 * Synchronous Compression operations
 *
 * Copyright 2015 LG Electronics Inc.
 * Copyright (c) 2016, Intel Corporation
 * Author: Giovanni Cabiddu <giovanni.cabiddu@intel.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 */
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/crypto.h>
#include <linux/compiler.h>
#include <linux/vmalloc.h>
#include <crypto/algapi.h>
#include <linux/cryptouser.h>
#include <net/netlink.h>
#include <linux/scatterlist.h>
#include <linux/locallock.h>
#include <crypto/scatterwalk.h>
#include <crypto/internal/acompress.h>
#include <crypto/internal/scompress.h>
#include "internal.h"

struct scomp_scratch {
	spinlock_t	lock;
	void		*src;
	void		*dst;
};

static DEFINE_PER_CPU(struct scomp_scratch, scomp_scratch) = {
	.lock = __SPIN_LOCK_UNLOCKED(scomp_scratch.lock),
};

static const struct crypto_type crypto_scomp_type;
static int scomp_scratch_users;
static DEFINE_MUTEX(scomp_lock);
static DEFINE_LOCAL_IRQ_LOCK(scomp_scratches_lock);

#ifdef CONFIG_NET
static int crypto_scomp_report(struct sk_buff *skb, struct crypto_alg *alg)
{
	struct crypto_report_comp rscomp;

	strncpy(rscomp.type, "scomp", sizeof(rscomp.type));

	if (nla_put(skb, CRYPTOCFGA_REPORT_COMPRESS,
		    sizeof(struct crypto_report_comp), &rscomp))
		goto nla_put_failure;
	return 0;

nla_put_failure:
	return -EMSGSIZE;
}
#else
static int crypto_scomp_report(struct sk_buff *skb, struct crypto_alg *alg)
{
	return -ENOSYS;
}
#endif

static void crypto_scomp_show(struct seq_file *m, struct crypto_alg *alg)
	__maybe_unused;

static void crypto_scomp_show(struct seq_file *m, struct crypto_alg *alg)
{
	seq_puts(m, "type         : scomp\n");
}

static void crypto_scomp_free_scratches(void)
{
	struct scomp_scratch *scratch;
	int i;

	for_each_possible_cpu(i) {
		scratch = per_cpu_ptr(&scomp_scratch, i);

		vfree(scratch->src);
		vfree(scratch->dst);
		scratch->src = NULL;
		scratch->dst = NULL;
	}
}

static int crypto_scomp_alloc_scratches(void)
{
	struct scomp_scratch *scratch;
	int i;

	for_each_possible_cpu(i) {
		void *mem;

		scratch = per_cpu_ptr(&scomp_scratch, i);

		mem = vmalloc_node(SCOMP_SCRATCH_SIZE, cpu_to_node(i));
		if (!mem)
			goto error;
		scratch->src = mem;
		mem = vmalloc_node(SCOMP_SCRATCH_SIZE, cpu_to_node(i));
		if (!mem)
			goto error;
		scratch->dst = mem;
	}
	return 0;
error:
	crypto_scomp_free_scratches();
	return -ENOMEM;
}

static int crypto_scomp_init_tfm(struct crypto_tfm *tfm)
{
	int ret = 0;

	mutex_lock(&scomp_lock);
	if (!scomp_scratch_users++)
		ret = crypto_scomp_alloc_scratches();
	mutex_unlock(&scomp_lock);

	return ret;
}

static void crypto_scomp_sg_free(struct scatterlist *sgl)
{
	int i, n;
	struct page *page;

	if (!sgl)
		return;

	n = sg_nents(sgl);
	for_each_sg(sgl, sgl, n, i) {
		page = sg_page(sgl);
		if (page)
			__free_page(page);
	}

	kfree(sgl);
}

static struct scatterlist *crypto_scomp_sg_alloc(size_t size, gfp_t gfp)
{
	struct scatterlist *sgl;
	struct page *page;
	int i, n;

	n = ((size - 1) >> PAGE_SHIFT) + 1;

	sgl = kmalloc_array(n, sizeof(struct scatterlist), gfp);
	if (!sgl)
		return NULL;

	sg_init_table(sgl, n);

	for (i = 0; i < n; i++) {
		page = alloc_page(gfp);
		if (!page)
			goto err;
		sg_set_page(sgl + i, page, PAGE_SIZE, 0);
	}

	return sgl;

err:
	sg_mark_end(sgl + i);
	crypto_scomp_sg_free(sgl);
	return NULL;
}

static int scomp_acomp_comp_decomp(struct acomp_req *req, int dir)
{
	struct crypto_acomp *tfm = crypto_acomp_reqtfm(req);
	void **tfm_ctx = acomp_tfm_ctx(tfm);
	struct crypto_scomp *scomp = *tfm_ctx;
	void **ctx = acomp_request_ctx(req);
	struct scomp_scratch *scratch;
	unsigned int dlen;
	int ret;

	if (!req->src || !req->slen || req->slen > SCOMP_SCRATCH_SIZE)
		return -EINVAL;

	if (req->dst && !req->dlen)
		return -EINVAL;

	if (!req->dlen || req->dlen > SCOMP_SCRATCH_SIZE)
		req->dlen = SCOMP_SCRATCH_SIZE;

	dlen = req->dlen;

	scratch = raw_cpu_ptr(&scomp_scratch);
	spin_lock(&scratch->lock);

	scatterwalk_map_and_copy(scratch->src, req->src, 0, req->slen, 0);
	if (dir)
		ret = crypto_scomp_compress(scomp, scratch->src, req->slen,
					    scratch->dst, &req->dlen, *ctx);
	else
		ret = crypto_scomp_decompress(scomp, scratch->src, req->slen,
					      scratch->dst, &req->dlen, *ctx);
	if (!ret) {
		if (!req->dst) {
			req->dst = crypto_scomp_sg_alloc(req->dlen, GFP_ATOMIC);
			if (!req->dst) {
				ret = -ENOMEM;
				goto out;
			}
		} else if (req->dlen > dlen) {
			ret = -ENOSPC;
			goto out;
		}
		scatterwalk_map_and_copy(scratch->dst, req->dst, 0, req->dlen,
					 1);
	}
out:
	spin_unlock(&scratch->lock);
	return ret;
}

static int scomp_acomp_compress(struct acomp_req *req)
{
	return scomp_acomp_comp_decomp(req, 1);
}

static int scomp_acomp_decompress(struct acomp_req *req)
{
	return scomp_acomp_comp_decomp(req, 0);
}

static void crypto_exit_scomp_ops_async(struct crypto_tfm *tfm)
{
	struct crypto_scomp **ctx = crypto_tfm_ctx(tfm);

	crypto_free_scomp(*ctx);

	mutex_lock(&scomp_lock);
	if (!--scomp_scratch_users)
		crypto_scomp_free_scratches();
	mutex_unlock(&scomp_lock);
}

int crypto_init_scomp_ops_async(struct crypto_tfm *tfm)
{
	struct crypto_alg *calg = tfm->__crt_alg;
	struct crypto_acomp *crt = __crypto_acomp_tfm(tfm);
	struct crypto_scomp **ctx = crypto_tfm_ctx(tfm);
	struct crypto_scomp *scomp;

	if (!crypto_mod_get(calg))
		return -EAGAIN;

	scomp = crypto_create_tfm(calg, &crypto_scomp_type);
	if (IS_ERR(scomp)) {
		crypto_mod_put(calg);
		return PTR_ERR(scomp);
	}

	*ctx = scomp;
	tfm->exit = crypto_exit_scomp_ops_async;

	crt->compress = scomp_acomp_compress;
	crt->decompress = scomp_acomp_decompress;
	crt->dst_free = crypto_scomp_sg_free;
	crt->reqsize = sizeof(void *);

	return 0;
}

struct acomp_req *crypto_acomp_scomp_alloc_ctx(struct acomp_req *req)
{
	struct crypto_acomp *acomp = crypto_acomp_reqtfm(req);
	struct crypto_tfm *tfm = crypto_acomp_tfm(acomp);
	struct crypto_scomp **tfm_ctx = crypto_tfm_ctx(tfm);
	struct crypto_scomp *scomp = *tfm_ctx;
	void *ctx;

	ctx = crypto_scomp_alloc_ctx(scomp);
	if (IS_ERR(ctx)) {
		kfree(req);
		return NULL;
	}

	*req->__ctx = ctx;

	return req;
}

void crypto_acomp_scomp_free_ctx(struct acomp_req *req)
{
	struct crypto_acomp *acomp = crypto_acomp_reqtfm(req);
	struct crypto_tfm *tfm = crypto_acomp_tfm(acomp);
	struct crypto_scomp **tfm_ctx = crypto_tfm_ctx(tfm);
	struct crypto_scomp *scomp = *tfm_ctx;
	void *ctx = *req->__ctx;

	if (ctx)
		crypto_scomp_free_ctx(scomp, ctx);
}

static const struct crypto_type crypto_scomp_type = {
	.extsize = crypto_alg_extsize,
	.init_tfm = crypto_scomp_init_tfm,
#ifdef CONFIG_PROC_FS
	.show = crypto_scomp_show,
#endif
	.report = crypto_scomp_report,
	.maskclear = ~CRYPTO_ALG_TYPE_MASK,
	.maskset = CRYPTO_ALG_TYPE_MASK,
	.type = CRYPTO_ALG_TYPE_SCOMPRESS,
	.tfmsize = offsetof(struct crypto_scomp, base),
};

int crypto_register_scomp(struct scomp_alg *alg)
{
	struct crypto_alg *base = &alg->base;

	base->cra_type = &crypto_scomp_type;
	base->cra_flags &= ~CRYPTO_ALG_TYPE_MASK;
	base->cra_flags |= CRYPTO_ALG_TYPE_SCOMPRESS;

	return crypto_register_alg(base);
}
EXPORT_SYMBOL_GPL(crypto_register_scomp);

int crypto_unregister_scomp(struct scomp_alg *alg)
{
	return crypto_unregister_alg(&alg->base);
}
EXPORT_SYMBOL_GPL(crypto_unregister_scomp);

int crypto_register_scomps(struct scomp_alg *algs, int count)
{
	int i, ret;

	for (i = 0; i < count; i++) {
		ret = crypto_register_scomp(&algs[i]);
		if (ret)
			goto err;
	}

	return 0;

err:
	for (--i; i >= 0; --i)
		crypto_unregister_scomp(&algs[i]);

	return ret;
}
EXPORT_SYMBOL_GPL(crypto_register_scomps);

void crypto_unregister_scomps(struct scomp_alg *algs, int count)
{
	int i;

	for (i = count - 1; i >= 0; --i)
		crypto_unregister_scomp(&algs[i]);
}
EXPORT_SYMBOL_GPL(crypto_unregister_scomps);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Synchronous compression type");
