#ifndef __PACKET_INTERNAL_H__
#define __PACKET_INTERNAL_H__

struct packet_mclist {
	struct packet_mclist	*next;
	int			ifindex;
	int			count;
	unsigned short		type;
	unsigned short		alen;
	unsigned char		addr[MAX_ADDR_LEN];
};

/* kbdq - kernel block descriptor queue */
struct tpacket_kbdq_core {
	struct pgv	*pkbdq;
	unsigned int	feature_req_word;
	unsigned int	hdrlen;
	unsigned char	reset_pending_on_curr_blk;
	unsigned char   delete_blk_timer;
	unsigned short	kactive_blk_num;
	unsigned short	blk_sizeof_priv;

	/* last_kactive_blk_num:
	 * trick to see if user-space has caught up
	 * in order to avoid refreshing timer when every single pkt arrives.
	 */
	unsigned short	last_kactive_blk_num;

	char		*pkblk_start;
	char		*pkblk_end;
	int		kblk_size;
	unsigned int	max_frame_len;
	unsigned int	knum_blocks;
	uint64_t	knxt_seq_num;
	char		*prev;
	char		*nxt_offset;
	struct sk_buff	*skb;

	atomic_t	blk_fill_in_prog;

	/* Default is set to 8ms */
#define DEFAULT_PRB_RETIRE_TOV	(8)

	unsigned short  retire_blk_tov;
	unsigned short  version;
	unsigned long	tov_in_jiffies;

	/* timer to retire an outstanding block */
	struct timer_list retire_blk_timer;
};

struct pgv {
	char *buffer;
};

struct packet_ring_buffer {
	struct pgv		*pg_vec;

	unsigned int		head;
	unsigned int		frames_per_block;
	unsigned int		frame_size;
	unsigned int		frame_max;

	unsigned int		pg_vec_order;
	unsigned int		pg_vec_pages;
	unsigned int		pg_vec_len;

	unsigned int __percpu	*pending_refcnt;

	struct tpacket_kbdq_core	prb_bdqc;
};

extern struct mutex fanout_mutex;
#define PACKET_FANOUT_MAX	256

struct packet_fanout {
	possible_net_t		net;
	unsigned int		num_members;
	u16			id;
	u8			type;
	u8			flags;
	union {
		atomic_t		rr_cur;
		struct bpf_prog __rcu	*bpf_prog;
	};
	struct list_head	list;
	struct sock		*arr[PACKET_FANOUT_MAX];
	spinlock_t		lock;
	atomic_t		sk_ref;
	struct packet_type	prot_hook ____cacheline_aligned_in_smp;
};

struct packet_rollover {
	int			sock;
	atomic_long_t		num;
	atomic_long_t		num_huge;
	atomic_long_t		num_failed;
#define ROLLOVER_HLEN	(L1_CACHE_BYTES / sizeof(u32))
	u32			history[ROLLOVER_HLEN] ____cacheline_aligned;
} ____cacheline_aligned_in_smp;

struct packet_sock {
	/* struct sock has to be the first member of packet_sock */
	struct sock		sk;
	struct packet_fanout	*fanout;
	union  tpacket_stats_u	stats;
	struct packet_ring_buffer	rx_ring;
	struct packet_ring_buffer	tx_ring;
	int			copy_thresh;
	spinlock_t		bind_lock;
	struct mutex		pg_vec_lock;
	unsigned long		flags;
	unsigned int		running;	/* bind_lock must be held */
	unsigned int		has_vnet_hdr:1, /* writer must hold sock lock */
				tp_loss:1,
				tp_tx_has_off:1;
	int			pressure;
	int			ifindex;	/* bound device		*/
	__be16			num;
	struct packet_rollover	*rollover;
	struct packet_mclist	*mclist;
	atomic_long_t		mapped;
	enum tpacket_versions	tp_version;
	unsigned int		tp_hdrlen;
	unsigned int		tp_reserve;
	unsigned int		tp_tstamp;
	struct net_device __rcu	*cached_dev;
	int			(*xmit)(struct sk_buff *skb);
	struct packet_type	prot_hook ____cacheline_aligned_in_smp;
	unsigned int		pkt_type;
};

static struct packet_sock *pkt_sk(struct sock *sk)
{
	return (struct packet_sock *)sk;
}

enum packet_sock_flags {
	PACKET_SOCK_ORIGDEV,
	PACKET_SOCK_AUXDATA,
};

static inline void packet_sock_flag_set(struct packet_sock *po,
					enum packet_sock_flags flag,
					bool val)
{
	if (val)
		set_bit(flag, &po->flags);
	else
		clear_bit(flag, &po->flags);
}

static inline bool packet_sock_flag(const struct packet_sock *po,
				    enum packet_sock_flags flag)
{
	return test_bit(flag, &po->flags);
}

#endif
