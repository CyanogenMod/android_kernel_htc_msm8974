/****************************************************************************
 * Driver for Solarflare Solarstorm network controllers and boards
 * Copyright 2005-2006 Fen Systems Ltd.
 * Copyright 2005-2011 Solarflare Communications Inc.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation, incorporated herein by reference.
 */


#ifndef EFX_NET_DRIVER_H
#define EFX_NET_DRIVER_H

#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/ethtool.h>
#include <linux/if_vlan.h>
#include <linux/timer.h>
#include <linux/mdio.h>
#include <linux/list.h>
#include <linux/pci.h>
#include <linux/device.h>
#include <linux/highmem.h>
#include <linux/workqueue.h>
#include <linux/mutex.h>
#include <linux/vmalloc.h>
#include <linux/i2c.h>

#include "enum.h"
#include "bitfield.h"


#define EFX_DRIVER_VERSION	"3.1"

#ifdef DEBUG
#define EFX_BUG_ON_PARANOID(x) BUG_ON(x)
#define EFX_WARN_ON_PARANOID(x) WARN_ON(x)
#else
#define EFX_BUG_ON_PARANOID(x) do {} while (0)
#define EFX_WARN_ON_PARANOID(x) do {} while (0)
#endif


#define EFX_MAX_CHANNELS 32U
#define EFX_MAX_RX_QUEUES EFX_MAX_CHANNELS
#define EFX_EXTRA_CHANNEL_IOV	0
#define EFX_MAX_EXTRA_CHANNELS	1U

#define EFX_MAX_TX_TC		2
#define EFX_MAX_CORE_TX_QUEUES	(EFX_MAX_TX_TC * EFX_MAX_CHANNELS)
#define EFX_TXQ_TYPE_OFFLOAD	1	
#define EFX_TXQ_TYPE_HIGHPRI	2	
#define EFX_TXQ_TYPES		4
#define EFX_MAX_TX_QUEUES	(EFX_TXQ_TYPES * EFX_MAX_CHANNELS)

struct efx_special_buffer {
	void *addr;
	dma_addr_t dma_addr;
	unsigned int len;
	unsigned int index;
	unsigned int entries;
};

struct efx_tx_buffer {
	const struct sk_buff *skb;
	struct efx_tso_header *tsoh;
	dma_addr_t dma_addr;
	unsigned short len;
	bool continuation;
	bool unmap_single;
	unsigned short unmap_len;
};

/**
 * struct efx_tx_queue - An Efx TX queue
 *
 * This is a ring buffer of TX fragments.
 * Since the TX completion path always executes on the same
 * CPU and the xmit path can operate on different CPUs,
 * performance is increased by ensuring that the completion
 * path and the xmit path operate on different cache lines.
 * This is particularly important if the xmit path is always
 * executing on one CPU which is different from the completion
 * path.  There is also a cache line for members which are
 * read but not written on the fast path.
 *
 * @efx: The associated Efx NIC
 * @queue: DMA queue number
 * @channel: The associated channel
 * @core_txq: The networking core TX queue structure
 * @buffer: The software buffer ring
 * @txd: The hardware descriptor ring
 * @ptr_mask: The size of the ring minus 1.
 * @initialised: Has hardware queue been initialised?
 * @read_count: Current read pointer.
 *	This is the number of buffers that have been removed from both rings.
 * @old_write_count: The value of @write_count when last checked.
 *	This is here for performance reasons.  The xmit path will
 *	only get the up-to-date value of @write_count if this
 *	variable indicates that the queue is empty.  This is to
 *	avoid cache-line ping-pong between the xmit path and the
 *	completion path.
 * @insert_count: Current insert pointer
 *	This is the number of buffers that have been added to the
 *	software ring.
 * @write_count: Current write pointer
 *	This is the number of buffers that have been added to the
 *	hardware ring.
 * @old_read_count: The value of read_count when last checked.
 *	This is here for performance reasons.  The xmit path will
 *	only get the up-to-date value of read_count if this
 *	variable indicates that the queue is full.  This is to
 *	avoid cache-line ping-pong between the xmit path and the
 *	completion path.
 * @tso_headers_free: A list of TSO headers allocated for this TX queue
 *	that are not in use, and so available for new TSO sends. The list
 *	is protected by the TX queue lock.
 * @tso_bursts: Number of times TSO xmit invoked by kernel
 * @tso_long_headers: Number of packets with headers too long for standard
 *	blocks
 * @tso_packets: Number of packets via the TSO xmit path
 * @pushes: Number of times the TX push feature has been used
 * @empty_read_count: If the completion path has seen the queue as empty
 *	and the transmission path has not yet checked this, the value of
 *	@read_count bitwise-added to %EFX_EMPTY_COUNT_VALID; otherwise 0.
 */
struct efx_tx_queue {
	
	struct efx_nic *efx ____cacheline_aligned_in_smp;
	unsigned queue;
	struct efx_channel *channel;
	struct netdev_queue *core_txq;
	struct efx_tx_buffer *buffer;
	struct efx_special_buffer txd;
	unsigned int ptr_mask;
	bool initialised;

	
	unsigned int read_count ____cacheline_aligned_in_smp;
	unsigned int old_write_count;

	
	unsigned int insert_count ____cacheline_aligned_in_smp;
	unsigned int write_count;
	unsigned int old_read_count;
	struct efx_tso_header *tso_headers_free;
	unsigned int tso_bursts;
	unsigned int tso_long_headers;
	unsigned int tso_packets;
	unsigned int pushes;

	
	unsigned int empty_read_count ____cacheline_aligned_in_smp;
#define EFX_EMPTY_COUNT_VALID 0x80000000
};

struct efx_rx_buffer {
	dma_addr_t dma_addr;
	union {
		struct sk_buff *skb;
		struct page *page;
	} u;
	unsigned int len;
	u16 flags;
};
#define EFX_RX_BUF_PAGE		0x0001
#define EFX_RX_PKT_CSUMMED	0x0002
#define EFX_RX_PKT_DISCARD	0x0004

struct efx_rx_page_state {
	unsigned refcnt;
	dma_addr_t dma_addr;

	unsigned int __pad[0] ____cacheline_aligned;
};

struct efx_rx_queue {
	struct efx_nic *efx;
	struct efx_rx_buffer *buffer;
	struct efx_special_buffer rxd;
	unsigned int ptr_mask;
	bool enabled;
	bool flush_pending;

	int added_count;
	int notified_count;
	int removed_count;
	unsigned int max_fill;
	unsigned int fast_fill_trigger;
	unsigned int fast_fill_limit;
	unsigned int min_fill;
	unsigned int min_overfill;
	unsigned int alloc_page_count;
	unsigned int alloc_skb_count;
	struct timer_list slow_fill;
	unsigned int slow_fill_count;
};

struct efx_buffer {
	void *addr;
	dma_addr_t dma_addr;
	unsigned int len;
};


enum efx_rx_alloc_method {
	RX_ALLOC_METHOD_AUTO = 0,
	RX_ALLOC_METHOD_SKB = 1,
	RX_ALLOC_METHOD_PAGE = 2,
};

struct efx_channel {
	struct efx_nic *efx;
	int channel;
	const struct efx_channel_type *type;
	bool enabled;
	int irq;
	unsigned int irq_moderation;
	struct net_device *napi_dev;
	struct napi_struct napi_str;
	bool work_pending;
	struct efx_special_buffer eventq;
	unsigned int eventq_mask;
	unsigned int eventq_read_ptr;
	int event_test_cpu;

	unsigned int irq_count;
	unsigned int irq_mod_score;
#ifdef CONFIG_RFS_ACCEL
	unsigned int rfs_filters_added;
#endif

	int rx_alloc_level;
	int rx_alloc_push_pages;

	unsigned n_rx_tobe_disc;
	unsigned n_rx_ip_hdr_chksum_err;
	unsigned n_rx_tcp_udp_chksum_err;
	unsigned n_rx_mcast_mismatch;
	unsigned n_rx_frm_trunc;
	unsigned n_rx_overlength;
	unsigned n_skbuff_leaks;

	struct efx_rx_buffer *rx_pkt;

	struct efx_rx_queue rx_queue;
	struct efx_tx_queue tx_queue[EFX_TXQ_TYPES];
};

struct efx_channel_type {
	void (*handle_no_channel)(struct efx_nic *);
	int (*pre_probe)(struct efx_channel *);
	void (*get_name)(struct efx_channel *, char *buf, size_t len);
	struct efx_channel *(*copy)(const struct efx_channel *);
	bool keep_eventq;
};

enum efx_led_mode {
	EFX_LED_OFF	= 0,
	EFX_LED_ON	= 1,
	EFX_LED_DEFAULT	= 2
};

#define STRING_TABLE_LOOKUP(val, member) \
	((val) < member ## _max) ? member ## _names[val] : "(invalid)"

extern const char *const efx_loopback_mode_names[];
extern const unsigned int efx_loopback_mode_max;
#define LOOPBACK_MODE(efx) \
	STRING_TABLE_LOOKUP((efx)->loopback_mode, efx_loopback_mode)

extern const char *const efx_reset_type_names[];
extern const unsigned int efx_reset_type_max;
#define RESET_TYPE(type) \
	STRING_TABLE_LOOKUP(type, efx_reset_type)

enum efx_int_mode {
	
	EFX_INT_MODE_MSIX = 0,
	EFX_INT_MODE_MSI = 1,
	EFX_INT_MODE_LEGACY = 2,
	EFX_INT_MODE_MAX	
};
#define EFX_INT_MODE_USE_MSI(x) (((x)->interrupt_mode) <= EFX_INT_MODE_MSI)

enum nic_state {
	STATE_INIT = 0,
	STATE_RUNNING = 1,
	STATE_FINI = 2,
	STATE_DISABLED = 3,
	STATE_MAX,
};

#ifdef CONFIG_HAVE_EFFICIENT_UNALIGNED_ACCESS
#define EFX_PAGE_IP_ALIGN 0
#else
#define EFX_PAGE_IP_ALIGN NET_IP_ALIGN
#endif

#define EFX_PAGE_SKB_ALIGN 2

struct efx_nic;

#define EFX_FC_RX	FLOW_CTRL_RX
#define EFX_FC_TX	FLOW_CTRL_TX
#define EFX_FC_AUTO	4

struct efx_link_state {
	bool up;
	bool fd;
	u8 fc;
	unsigned int speed;
};

static inline bool efx_link_state_equal(const struct efx_link_state *left,
					const struct efx_link_state *right)
{
	return left->up == right->up && left->fd == right->fd &&
		left->fc == right->fc && left->speed == right->speed;
}

struct efx_phy_operations {
	int (*probe) (struct efx_nic *efx);
	int (*init) (struct efx_nic *efx);
	void (*fini) (struct efx_nic *efx);
	void (*remove) (struct efx_nic *efx);
	int (*reconfigure) (struct efx_nic *efx);
	bool (*poll) (struct efx_nic *efx);
	void (*get_settings) (struct efx_nic *efx,
			      struct ethtool_cmd *ecmd);
	int (*set_settings) (struct efx_nic *efx,
			     struct ethtool_cmd *ecmd);
	void (*set_npage_adv) (struct efx_nic *efx, u32);
	int (*test_alive) (struct efx_nic *efx);
	const char *(*test_name) (struct efx_nic *efx, unsigned int index);
	int (*run_tests) (struct efx_nic *efx, int *results, unsigned flags);
};

enum efx_phy_mode {
	PHY_MODE_NORMAL		= 0,
	PHY_MODE_TX_DISABLED	= 1,
	PHY_MODE_LOW_POWER	= 2,
	PHY_MODE_OFF		= 4,
	PHY_MODE_SPECIAL	= 8,
};

static inline bool efx_phy_mode_disabled(enum efx_phy_mode mode)
{
	return !!(mode & ~PHY_MODE_TX_DISABLED);
}

struct efx_mac_stats {
	u64 tx_bytes;
	u64 tx_good_bytes;
	u64 tx_bad_bytes;
	u64 tx_packets;
	u64 tx_bad;
	u64 tx_pause;
	u64 tx_control;
	u64 tx_unicast;
	u64 tx_multicast;
	u64 tx_broadcast;
	u64 tx_lt64;
	u64 tx_64;
	u64 tx_65_to_127;
	u64 tx_128_to_255;
	u64 tx_256_to_511;
	u64 tx_512_to_1023;
	u64 tx_1024_to_15xx;
	u64 tx_15xx_to_jumbo;
	u64 tx_gtjumbo;
	u64 tx_collision;
	u64 tx_single_collision;
	u64 tx_multiple_collision;
	u64 tx_excessive_collision;
	u64 tx_deferred;
	u64 tx_late_collision;
	u64 tx_excessive_deferred;
	u64 tx_non_tcpudp;
	u64 tx_mac_src_error;
	u64 tx_ip_src_error;
	u64 rx_bytes;
	u64 rx_good_bytes;
	u64 rx_bad_bytes;
	u64 rx_packets;
	u64 rx_good;
	u64 rx_bad;
	u64 rx_pause;
	u64 rx_control;
	u64 rx_unicast;
	u64 rx_multicast;
	u64 rx_broadcast;
	u64 rx_lt64;
	u64 rx_64;
	u64 rx_65_to_127;
	u64 rx_128_to_255;
	u64 rx_256_to_511;
	u64 rx_512_to_1023;
	u64 rx_1024_to_15xx;
	u64 rx_15xx_to_jumbo;
	u64 rx_gtjumbo;
	u64 rx_bad_lt64;
	u64 rx_bad_64_to_15xx;
	u64 rx_bad_15xx_to_jumbo;
	u64 rx_bad_gtjumbo;
	u64 rx_overflow;
	u64 rx_missed;
	u64 rx_false_carrier;
	u64 rx_symbol_error;
	u64 rx_align_error;
	u64 rx_length_error;
	u64 rx_internal_error;
	u64 rx_good_lt64;
};

#define EFX_MCAST_HASH_BITS 8

#define EFX_MCAST_HASH_ENTRIES (1 << EFX_MCAST_HASH_BITS)

union efx_multicast_hash {
	u8 byte[EFX_MCAST_HASH_ENTRIES / 8];
	efx_oword_t oword[EFX_MCAST_HASH_ENTRIES / sizeof(efx_oword_t) / 8];
};

struct efx_filter_state;
struct efx_vf;
struct vfdi_status;

struct efx_nic {
	/* The following fields should be written very rarely */

	char name[IFNAMSIZ];
	struct pci_dev *pci_dev;
	const struct efx_nic_type *type;
	int legacy_irq;
	bool legacy_irq_enabled;
	struct workqueue_struct *workqueue;
	char workqueue_name[16];
	struct work_struct reset_work;
	resource_size_t membase_phys;
	void __iomem *membase;

	enum efx_int_mode interrupt_mode;
	unsigned int timer_quantum_ns;
	bool irq_rx_adaptive;
	unsigned int irq_rx_moderation;
	u32 msg_enable;

	enum nic_state state;
	unsigned long reset_pending;

	struct efx_channel *channel[EFX_MAX_CHANNELS];
	char channel_name[EFX_MAX_CHANNELS][IFNAMSIZ + 6];
	const struct efx_channel_type *
	extra_channel_type[EFX_MAX_EXTRA_CHANNELS];

	unsigned rxq_entries;
	unsigned txq_entries;
	unsigned tx_dc_base;
	unsigned rx_dc_base;
	unsigned sram_lim_qw;
	unsigned next_buffer_table;
	unsigned n_channels;
	unsigned n_rx_channels;
	unsigned rss_spread;
	unsigned tx_channel_offset;
	unsigned n_tx_channels;
	unsigned int rx_buffer_len;
	unsigned int rx_buffer_order;
	u8 rx_hash_key[40];
	u32 rx_indir_table[128];

	unsigned int_error_count;
	unsigned long int_error_expire;

	struct efx_buffer irq_status;
	unsigned irq_zero_count;
	unsigned irq_level;
	struct delayed_work selftest_work;

#ifdef CONFIG_SFC_MTD
	struct list_head mtd_list;
#endif

	void *nic_data;

	struct mutex mac_lock;
	struct work_struct mac_work;
	bool port_enabled;

	bool port_initialized;
	struct net_device *net_dev;

	struct efx_buffer stats_buffer;

	unsigned int phy_type;
	const struct efx_phy_operations *phy_op;
	void *phy_data;
	struct mdio_if_info mdio;
	unsigned int mdio_bus;
	enum efx_phy_mode phy_mode;

	u32 link_advertising;
	struct efx_link_state link_state;
	unsigned int n_link_state_changes;

	bool promiscuous;
	union efx_multicast_hash multicast_hash;
	u8 wanted_fc;
	unsigned fc_disable;

	atomic_t rx_reset;
	enum efx_loopback_mode loopback_mode;
	u64 loopback_modes;

	void *loopback_selftest;

	struct efx_filter_state *filter_state;

	atomic_t drain_pending;
	atomic_t rxq_flush_pending;
	atomic_t rxq_flush_outstanding;
	wait_queue_head_t flush_wq;

#ifdef CONFIG_SFC_SRIOV
	struct efx_channel *vfdi_channel;
	struct efx_vf *vf;
	unsigned vf_count;
	unsigned vf_init_count;
	unsigned vi_scale;
	unsigned vf_buftbl_base;
	struct efx_buffer vfdi_status;
	struct list_head local_addr_list;
	struct list_head local_page_list;
	struct mutex local_lock;
	struct work_struct peer_work;
#endif

	/* The following fields may be written more often */

	struct delayed_work monitor_work ____cacheline_aligned_in_smp;
	spinlock_t biu_lock;
	int last_irq_cpu;
	unsigned n_rx_nodesc_drop_cnt;
	struct efx_mac_stats mac_stats;
	spinlock_t stats_lock;
};

static inline int efx_dev_registered(struct efx_nic *efx)
{
	return efx->net_dev->reg_state == NETREG_REGISTERED;
}

static inline unsigned int efx_port_num(struct efx_nic *efx)
{
	return efx->net_dev->dev_id;
}

struct efx_nic_type {
	int (*probe)(struct efx_nic *efx);
	void (*remove)(struct efx_nic *efx);
	int (*init)(struct efx_nic *efx);
	void (*dimension_resources)(struct efx_nic *efx);
	void (*fini)(struct efx_nic *efx);
	void (*monitor)(struct efx_nic *efx);
	enum reset_type (*map_reset_reason)(enum reset_type reason);
	int (*map_reset_flags)(u32 *flags);
	int (*reset)(struct efx_nic *efx, enum reset_type method);
	int (*probe_port)(struct efx_nic *efx);
	void (*remove_port)(struct efx_nic *efx);
	bool (*handle_global_event)(struct efx_channel *channel, efx_qword_t *);
	void (*prepare_flush)(struct efx_nic *efx);
	void (*update_stats)(struct efx_nic *efx);
	void (*start_stats)(struct efx_nic *efx);
	void (*stop_stats)(struct efx_nic *efx);
	void (*set_id_led)(struct efx_nic *efx, enum efx_led_mode mode);
	void (*push_irq_moderation)(struct efx_channel *channel);
	int (*reconfigure_port)(struct efx_nic *efx);
	int (*reconfigure_mac)(struct efx_nic *efx);
	bool (*check_mac_fault)(struct efx_nic *efx);
	void (*get_wol)(struct efx_nic *efx, struct ethtool_wolinfo *wol);
	int (*set_wol)(struct efx_nic *efx, u32 type);
	void (*resume_wol)(struct efx_nic *efx);
	int (*test_registers)(struct efx_nic *efx);
	int (*test_nvram)(struct efx_nic *efx);

	int revision;
	unsigned int mem_map_size;
	unsigned int txd_ptr_tbl_base;
	unsigned int rxd_ptr_tbl_base;
	unsigned int buf_tbl_base;
	unsigned int evq_ptr_tbl_base;
	unsigned int evq_rptr_tbl_base;
	u64 max_dma_mask;
	unsigned int rx_buffer_hash_size;
	unsigned int rx_buffer_padding;
	unsigned int max_interrupt_mode;
	unsigned int phys_addr_channels;
	unsigned int timer_period_max;
	netdev_features_t offload_features;
};


static inline struct efx_channel *
efx_get_channel(struct efx_nic *efx, unsigned index)
{
	EFX_BUG_ON_PARANOID(index >= efx->n_channels);
	return efx->channel[index];
}

#define efx_for_each_channel(_channel, _efx)				\
	for (_channel = (_efx)->channel[0];				\
	     _channel;							\
	     _channel = (_channel->channel + 1 < (_efx)->n_channels) ?	\
		     (_efx)->channel[_channel->channel + 1] : NULL)

#define efx_for_each_channel_rev(_channel, _efx)			\
	for (_channel = (_efx)->channel[(_efx)->n_channels - 1];	\
	     _channel;							\
	     _channel = _channel->channel ?				\
		     (_efx)->channel[_channel->channel - 1] : NULL)

static inline struct efx_tx_queue *
efx_get_tx_queue(struct efx_nic *efx, unsigned index, unsigned type)
{
	EFX_BUG_ON_PARANOID(index >= efx->n_tx_channels ||
			    type >= EFX_TXQ_TYPES);
	return &efx->channel[efx->tx_channel_offset + index]->tx_queue[type];
}

static inline bool efx_channel_has_tx_queues(struct efx_channel *channel)
{
	return channel->channel - channel->efx->tx_channel_offset <
		channel->efx->n_tx_channels;
}

static inline struct efx_tx_queue *
efx_channel_get_tx_queue(struct efx_channel *channel, unsigned type)
{
	EFX_BUG_ON_PARANOID(!efx_channel_has_tx_queues(channel) ||
			    type >= EFX_TXQ_TYPES);
	return &channel->tx_queue[type];
}

static inline bool efx_tx_queue_used(struct efx_tx_queue *tx_queue)
{
	return !(tx_queue->efx->net_dev->num_tc < 2 &&
		 tx_queue->queue & EFX_TXQ_TYPE_HIGHPRI);
}

#define efx_for_each_channel_tx_queue(_tx_queue, _channel)		\
	if (!efx_channel_has_tx_queues(_channel))			\
		;							\
	else								\
		for (_tx_queue = (_channel)->tx_queue;			\
		     _tx_queue < (_channel)->tx_queue + EFX_TXQ_TYPES && \
			     efx_tx_queue_used(_tx_queue);		\
		     _tx_queue++)

#define efx_for_each_possible_channel_tx_queue(_tx_queue, _channel)	\
	if (!efx_channel_has_tx_queues(_channel))			\
		;							\
	else								\
		for (_tx_queue = (_channel)->tx_queue;			\
		     _tx_queue < (_channel)->tx_queue + EFX_TXQ_TYPES;	\
		     _tx_queue++)

static inline bool efx_channel_has_rx_queue(struct efx_channel *channel)
{
	return channel->channel < channel->efx->n_rx_channels;
}

static inline struct efx_rx_queue *
efx_channel_get_rx_queue(struct efx_channel *channel)
{
	EFX_BUG_ON_PARANOID(!efx_channel_has_rx_queue(channel));
	return &channel->rx_queue;
}

#define efx_for_each_channel_rx_queue(_rx_queue, _channel)		\
	if (!efx_channel_has_rx_queue(_channel))			\
		;							\
	else								\
		for (_rx_queue = &(_channel)->rx_queue;			\
		     _rx_queue;						\
		     _rx_queue = NULL)

static inline struct efx_channel *
efx_rx_queue_channel(struct efx_rx_queue *rx_queue)
{
	return container_of(rx_queue, struct efx_channel, rx_queue);
}

static inline int efx_rx_queue_index(struct efx_rx_queue *rx_queue)
{
	return efx_rx_queue_channel(rx_queue)->channel;
}

static inline struct efx_rx_buffer *efx_rx_buffer(struct efx_rx_queue *rx_queue,
						  unsigned int index)
{
	return &rx_queue->buffer[index];
}

static inline void set_bit_le(unsigned nr, unsigned char *addr)
{
	addr[nr / 8] |= (1 << (nr % 8));
}

static inline void clear_bit_le(unsigned nr, unsigned char *addr)
{
	addr[nr / 8] &= ~(1 << (nr % 8));
}


#define EFX_MAX_FRAME_LEN(mtu) \
	((((mtu) + ETH_HLEN + VLAN_HLEN + 4 + 7) & ~7) + 16)


#endif 
