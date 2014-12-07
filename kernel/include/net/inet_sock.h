/*
 * INET		An implementation of the TCP/IP protocol suite for the LINUX
 *		operating system.  INET is implemented using the  BSD Socket
 *		interface as the means of communication with the user level.
 *
 *		Definitions for inet_sock
 *
 * Authors:	Many, reorganised here by
 * 		Arnaldo Carvalho de Melo <acme@mandriva.com>
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 */
#ifndef _INET_SOCK_H
#define _INET_SOCK_H


#include <linux/kmemcheck.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/jhash.h>
#include <linux/netdevice.h>

#include <net/flow.h>
#include <net/sock.h>
#include <net/request_sock.h>
#include <net/netns/hash.h>

/** struct ip_options - IP Options
 *
 * @faddr - Saved first hop address
 * @is_data - Options in __data, rather than skb
 * @is_strictroute - Strict source route
 * @srr_is_hit - Packet destination addr was our one
 * @is_changed - IP checksum more not valid
 * @rr_needaddr - Need to record addr of outgoing dev
 * @ts_needtime - Need to record timestamp
 * @ts_needaddr - Need to record addr of outgoing dev
 */
struct ip_options {
	__be32		faddr;
	unsigned char	optlen;
	unsigned char	srr;
	unsigned char	rr;
	unsigned char	ts;
	unsigned char	is_strictroute:1,
			srr_is_hit:1,
			is_changed:1,
			rr_needaddr:1,
			ts_needtime:1,
			ts_needaddr:1;
	unsigned char	router_alert;
	unsigned char	cipso;

#ifndef __GENKSYMS__
	unsigned char	rhel_alloc_flag:1;
#else
	unsigned char	__pad2;
#endif
	unsigned char	__data[0];
};

#define optlength(opt) (sizeof(struct ip_options) + opt->optlen)

struct ip_options_rcu {
	struct rcu_head rcu;
	struct ip_options opt;
};

struct ip_options_data {
	struct ip_options	opt;
	char			data[40];
};

static inline struct ip_options_rcu *get_ip_options_rcu(struct ip_options *opt)
{
	if (!opt)
		return NULL;
	/*
	 * Warn if someone allocated opt directly without using
	 * rhel_ip_options_set_alloc_flag().
	 */
	WARN_ON(!opt->rhel_alloc_flag);

	return container_of(opt, struct ip_options_rcu, opt);
}

static inline void rhel_ip_options_set_alloc_flag(struct ip_options *opt)
{
	opt->rhel_alloc_flag = 1;
}

static inline struct ip_options *kmalloc_ip_options(size_t opt_len, gfp_t flags)
{
	struct ip_options_rcu *opt_rcu;

	opt_rcu = kmalloc(sizeof(struct ip_options_rcu) + opt_len, flags);
	if (!opt_rcu)
		return NULL;
	rhel_ip_options_set_alloc_flag(&opt_rcu->opt);
	return &opt_rcu->opt;
}

static inline struct ip_options *kzalloc_ip_options(size_t opt_len, gfp_t flags)
{
	return kmalloc_ip_options(opt_len, flags | __GFP_ZERO);
}

static inline void kfree_ip_options(struct ip_options *opt)
{
	kfree(get_ip_options_rcu(opt));
}

struct inet_request_sock {
	struct request_sock	req;
#if defined(CONFIG_IPV6) || defined(CONFIG_IPV6_MODULE)
	u16			inet6_rsk_offset;
#endif
	__be16			loc_port;
	__be32			loc_addr;
	__be32			rmt_addr;
	__be16			rmt_port;
	kmemcheck_bitfield_begin(flags);
	u16			snd_wscale : 4,
				rcv_wscale : 4,
				tstamp_ok  : 1,
				sack_ok	   : 1,
				wscale_ok  : 1,
				ecn_ok	   : 1,
				acked	   : 1,
				no_srccheck: 1;
	kmemcheck_bitfield_end(flags);
	struct ip_options	*opt;
};

static inline struct inet_request_sock *inet_rsk(const struct request_sock *sk)
{
	return (struct inet_request_sock *)sk;
}

struct ip_mc_socklist;
struct ipv6_pinfo;
struct rtable;

/** struct inet_sock - representation of INET sockets
 *
 * @sk - ancestor class
 * @pinet6 - pointer to IPv6 control block
 * @daddr - Foreign IPv4 addr
 * @rcv_saddr - Bound local IPv4 addr
 * @dport - Destination port
 * @num - Local port
 * @saddr - Sending source
 * @uc_ttl - Unicast TTL
 * @sport - Source port
 * @id - ID counter for DF pkts
 * @tos - TOS
 * @mc_ttl - Multicasting TTL
 * @is_icsk - is this an inet_connection_sock?
 * @mc_index - Multicast device index
 * @mc_list - Group array
 * @cork - info to build ip hdr on each ip frag while socket is corked
 */
struct inet_sock {
	/* sk and pinet6 has to be the first two members of inet_sock */
	struct sock		sk;
#if defined(CONFIG_IPV6) || defined(CONFIG_IPV6_MODULE)
	struct ipv6_pinfo	*pinet6;
#endif
	/* Socket demultiplex comparisons on incoming packets. */
	__be32			daddr;
	__be32			rcv_saddr;
	__be16			dport;
	__u16			num;
	__be32			saddr;
	__s16			uc_ttl;
	__u16			cmsg_flags;
	struct ip_options	*opt;
	__be16			sport;
	__u16			id;
	__u8			tos;
	__u8			mc_ttl;
	__u8			pmtudisc;
	__u8			recverr:1,
				is_icsk:1,
				freebind:1,
				hdrincl:1,
				mc_loop:1,
				transparent:1,
				mc_all:1;
	int			mc_index;
	__be32			mc_addr;
	struct ip_mc_socklist	*mc_list;
	struct {
		unsigned int		flags;
		unsigned int		fragsize;
		struct ip_options	*opt;
		struct dst_entry	*dst;
		int			length; /* Total length of all frames */
		__be32			addr;
		struct flowi		fl;
	} cork;
};

static inline __u32 inet_sk_rxhash(struct sock *sk)
{
	struct sock_extended *ske = sk_extended(sk);

	return ske->inet_sock_extended.rxhash;
}

#define IPCORK_OPT	1	/* ip-options has been held in ipcork.opt */
#define IPCORK_ALLFRAG	2	/* always fragment (for ipv6 for now) */

static inline struct inet_sock *inet_sk(const struct sock *sk)
{
	return (struct inet_sock *)sk;
}

static inline void __inet_sk_copy_descendant(struct sock *sk_to,
					     const struct sock *sk_from,
					     const int ancestor_size)
{
	memcpy(inet_sk(sk_to) + 1, inet_sk(sk_from) + 1,
	       sk_from->sk_prot->obj_size - ancestor_size);
}
#if !(defined(CONFIG_IPV6) || defined(CONFIG_IPV6_MODULE))
static inline void inet_sk_copy_descendant(struct sock *sk_to,
					   const struct sock *sk_from)
{
	__inet_sk_copy_descendant(sk_to, sk_from, sizeof(struct inet_sock));
}
#endif

extern int inet_sk_rebuild_header(struct sock *sk);

extern u32 inet_ehash_secret;
extern void build_ehash_secret(void);

static inline unsigned int inet_ehashfn(struct net *net,
					const __be32 laddr, const __u16 lport,
					const __be32 faddr, const __be16 fport)
{
	return jhash_3words((__force __u32) laddr,
			    (__force __u32) faddr,
			    ((__u32) lport) << 16 | (__force __u32)fport,
			    inet_ehash_secret + net_hash_mix(net));
}

static inline int inet_sk_ehashfn(const struct sock *sk)
{
	const struct inet_sock *inet = inet_sk(sk);
	const __be32 laddr = inet->rcv_saddr;
	const __u16 lport = inet->num;
	const __be32 faddr = inet->daddr;
	const __be16 fport = inet->dport;
	struct net *net = sock_net(sk);

	return inet_ehashfn(net, laddr, lport, faddr, fport);
}

static inline struct request_sock *inet_reqsk_alloc(struct request_sock_ops *ops)
{
	struct request_sock *req = reqsk_alloc(ops);
	struct inet_request_sock *ireq = inet_rsk(req);

	if (req != NULL) {
		kmemcheck_annotate_bitfield(ireq, flags);
		ireq->opt = NULL;
	}

	return req;
}

static inline __u8 inet_sk_flowi_flags(const struct sock *sk)
{
	return inet_sk(sk)->transparent ? FLOWI_FLAG_ANYSRC : 0;
}

static inline void inet_rps_record_flow(struct sock *sk)
{
	struct rps_sock_flow_table *sock_flow_table;

	rcu_read_lock();
	sock_flow_table = rcu_dereference(rps_sock_flow_table);
	rps_record_sock_flow(sock_flow_table, inet_sk_rxhash(sk));
	rcu_read_unlock();
}

static inline void inet_rcs_record_cpu(struct sock *sk) 
{
	sk->sk_affinity = smp_processor_id();
}

static inline void inet_sock_cpu_or_flow_record(struct sock *sk)
{
    //if (enable_receive_cpu_selection)
    if (sock_flag(sk, SOCK_AFFINITY))
        inet_rcs_record_cpu(sk);
    else
        inet_rps_record_flow(sk);
}

static inline void inet_rps_reset_flow(struct sock *sk)
{
	struct rps_sock_flow_table *sock_flow_table;

	rcu_read_lock();
	sock_flow_table = rcu_dereference(rps_sock_flow_table);
	rps_reset_sock_flow(sock_flow_table, inet_sk_rxhash(sk));
	rcu_read_unlock();
}

static inline void inet_rps_save_rxhash(struct sock *sk, u32 rxhash)
{
	struct sock_extended *ske = sk_extended(sk);

	if (unlikely(ske->inet_sock_extended.rxhash != rxhash)) {
		inet_rps_reset_flow(sk);
		ske->inet_sock_extended.rxhash = rxhash;
	}
}
#endif	/* _INET_SOCK_H */
