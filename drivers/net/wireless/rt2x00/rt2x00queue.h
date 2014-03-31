/*
	Copyright (C) 2004 - 2010 Ivo van Doorn <IvDoorn@gmail.com>
	<http://rt2x00.serialmonkey.com>

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the
	Free Software Foundation, Inc.,
	59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */


#ifndef RT2X00QUEUE_H
#define RT2X00QUEUE_H

#include <linux/prefetch.h>

#define DATA_FRAME_SIZE		2432
#define MGMT_FRAME_SIZE		256
#define AGGREGATION_SIZE	3840

enum data_queue_qid {
	QID_AC_VO = 0,
	QID_AC_VI = 1,
	QID_AC_BE = 2,
	QID_AC_BK = 3,
	QID_HCCA = 4,
	QID_MGMT = 13,
	QID_RX = 14,
	QID_OTHER = 15,
	QID_BEACON,
	QID_ATIM,
};

enum skb_frame_desc_flags {
	SKBDESC_DMA_MAPPED_RX = 1 << 0,
	SKBDESC_DMA_MAPPED_TX = 1 << 1,
	SKBDESC_IV_STRIPPED = 1 << 2,
	SKBDESC_NOT_MAC80211 = 1 << 3,
	SKBDESC_DESC_IN_SKB = 1 << 4,
};

struct skb_frame_desc {
	u8 flags;

	u8 desc_len;
	u8 tx_rate_idx;
	u8 tx_rate_flags;

	void *desc;

	__le32 iv[2];

	dma_addr_t skb_dma;

	struct queue_entry *entry;
};

static inline struct skb_frame_desc* get_skb_frame_desc(struct sk_buff *skb)
{
	BUILD_BUG_ON(sizeof(struct skb_frame_desc) >
		     IEEE80211_TX_INFO_DRIVER_DATA_SIZE);
	return (struct skb_frame_desc *)&IEEE80211_SKB_CB(skb)->driver_data;
}

enum rxdone_entry_desc_flags {
	RXDONE_SIGNAL_PLCP = BIT(0),
	RXDONE_SIGNAL_BITRATE = BIT(1),
	RXDONE_SIGNAL_MCS = BIT(2),
	RXDONE_MY_BSS = BIT(3),
	RXDONE_CRYPTO_IV = BIT(4),
	RXDONE_CRYPTO_ICV = BIT(5),
	RXDONE_L2PAD = BIT(6),
};

#define RXDONE_SIGNAL_MASK \
	( RXDONE_SIGNAL_PLCP | RXDONE_SIGNAL_BITRATE | RXDONE_SIGNAL_MCS )

struct rxdone_entry_desc {
	u64 timestamp;
	int signal;
	int rssi;
	int size;
	int flags;
	int dev_flags;
	u16 rate_mode;
	u8 cipher;
	u8 cipher_status;

	__le32 iv[2];
	__le32 icv;
};

enum txdone_entry_desc_flags {
	TXDONE_UNKNOWN,
	TXDONE_SUCCESS,
	TXDONE_FALLBACK,
	TXDONE_FAILURE,
	TXDONE_EXCESSIVE_RETRY,
	TXDONE_AMPDU,
};

struct txdone_entry_desc {
	unsigned long flags;
	int retry;
};

enum txentry_desc_flags {
	ENTRY_TXD_RTS_FRAME,
	ENTRY_TXD_CTS_FRAME,
	ENTRY_TXD_GENERATE_SEQ,
	ENTRY_TXD_FIRST_FRAGMENT,
	ENTRY_TXD_MORE_FRAG,
	ENTRY_TXD_REQ_TIMESTAMP,
	ENTRY_TXD_BURST,
	ENTRY_TXD_ACK,
	ENTRY_TXD_RETRY_MODE,
	ENTRY_TXD_ENCRYPT,
	ENTRY_TXD_ENCRYPT_PAIRWISE,
	ENTRY_TXD_ENCRYPT_IV,
	ENTRY_TXD_ENCRYPT_MMIC,
	ENTRY_TXD_HT_AMPDU,
	ENTRY_TXD_HT_BW_40,
	ENTRY_TXD_HT_SHORT_GI,
	ENTRY_TXD_HT_MIMO_PS,
};

struct txentry_desc {
	unsigned long flags;

	u16 length;
	u16 header_length;

	union {
		struct {
			u16 length_high;
			u16 length_low;
			u16 signal;
			u16 service;
			enum ifs ifs;
		} plcp;

		struct {
			u16 mcs;
			u8 stbc;
			u8 ba_size;
			u8 mpdu_density;
			enum txop txop;
			int wcid;
		} ht;
	} u;

	enum rate_modulation rate_mode;

	short retry_limit;

	enum cipher cipher;
	u16 key_idx;
	u16 iv_offset;
	u16 iv_len;
};

enum queue_entry_flags {
	ENTRY_BCN_ASSIGNED,
	ENTRY_OWNER_DEVICE_DATA,
	ENTRY_DATA_PENDING,
	ENTRY_DATA_IO_FAILED,
	ENTRY_DATA_STATUS_PENDING,
};

struct queue_entry {
	unsigned long flags;
	unsigned long last_action;

	struct data_queue *queue;

	struct sk_buff *skb;

	unsigned int entry_idx;

	void *priv_data;
};

enum queue_index {
	Q_INDEX,
	Q_INDEX_DMA_DONE,
	Q_INDEX_DONE,
	Q_INDEX_MAX,
};

enum data_queue_flags {
	QUEUE_STARTED,
	QUEUE_PAUSED,
};

struct data_queue {
	struct rt2x00_dev *rt2x00dev;
	struct queue_entry *entries;

	enum data_queue_qid qid;
	unsigned long flags;

	struct mutex status_lock;
	spinlock_t tx_lock;
	spinlock_t index_lock;

	unsigned int count;
	unsigned short limit;
	unsigned short threshold;
	unsigned short length;
	unsigned short index[Q_INDEX_MAX];

	unsigned short txop;
	unsigned short aifs;
	unsigned short cw_min;
	unsigned short cw_max;

	unsigned short data_size;
	unsigned short desc_size;

	unsigned short usb_endpoint;
	unsigned short usb_maxpacket;
};

struct data_queue_desc {
	unsigned short entry_num;
	unsigned short data_size;
	unsigned short desc_size;
	unsigned short priv_size;
};

#define queue_end(__dev) \
	&(__dev)->rx[(__dev)->data_queues]

#define tx_queue_end(__dev) \
	&(__dev)->tx[(__dev)->ops->tx_queues]

#define queue_next(__queue) \
	&(__queue)[1]

#define queue_loop(__entry, __start, __end)			\
	for ((__entry) = (__start);				\
	     prefetch(queue_next(__entry)), (__entry) != (__end);\
	     (__entry) = queue_next(__entry))

#define queue_for_each(__dev, __entry) \
	queue_loop(__entry, (__dev)->rx, queue_end(__dev))

#define tx_queue_for_each(__dev, __entry) \
	queue_loop(__entry, (__dev)->tx, tx_queue_end(__dev))

#define txall_queue_for_each(__dev, __entry) \
	queue_loop(__entry, (__dev)->tx, queue_end(__dev))

bool rt2x00queue_for_each_entry(struct data_queue *queue,
				enum queue_index start,
				enum queue_index end,
				void *data,
				bool (*fn)(struct queue_entry *entry,
					   void *data));

static inline int rt2x00queue_empty(struct data_queue *queue)
{
	return queue->length == 0;
}

static inline int rt2x00queue_full(struct data_queue *queue)
{
	return queue->length == queue->limit;
}

static inline int rt2x00queue_available(struct data_queue *queue)
{
	return queue->limit - queue->length;
}

static inline int rt2x00queue_threshold(struct data_queue *queue)
{
	return rt2x00queue_available(queue) < queue->threshold;
}
static inline int rt2x00queue_dma_timeout(struct queue_entry *entry)
{
	if (!test_bit(ENTRY_OWNER_DEVICE_DATA, &entry->flags))
		return false;
	return time_after(jiffies, entry->last_action + msecs_to_jiffies(100));
}

/**
 * _rt2x00_desc_read - Read a word from the hardware descriptor.
 * @desc: Base descriptor address
 * @word: Word index from where the descriptor should be read.
 * @value: Address where the descriptor value should be written into.
 */
static inline void _rt2x00_desc_read(__le32 *desc, const u8 word, __le32 *value)
{
	*value = desc[word];
}

/**
 * rt2x00_desc_read - Read a word from the hardware descriptor, this
 * function will take care of the byte ordering.
 * @desc: Base descriptor address
 * @word: Word index from where the descriptor should be read.
 * @value: Address where the descriptor value should be written into.
 */
static inline void rt2x00_desc_read(__le32 *desc, const u8 word, u32 *value)
{
	__le32 tmp;
	_rt2x00_desc_read(desc, word, &tmp);
	*value = le32_to_cpu(tmp);
}

/**
 * rt2x00_desc_write - write a word to the hardware descriptor, this
 * function will take care of the byte ordering.
 * @desc: Base descriptor address
 * @word: Word index from where the descriptor should be written.
 * @value: Value that should be written into the descriptor.
 */
static inline void _rt2x00_desc_write(__le32 *desc, const u8 word, __le32 value)
{
	desc[word] = value;
}

/**
 * rt2x00_desc_write - write a word to the hardware descriptor.
 * @desc: Base descriptor address
 * @word: Word index from where the descriptor should be written.
 * @value: Value that should be written into the descriptor.
 */
static inline void rt2x00_desc_write(__le32 *desc, const u8 word, u32 value)
{
	_rt2x00_desc_write(desc, word, cpu_to_le32(value));
}

#endif 
