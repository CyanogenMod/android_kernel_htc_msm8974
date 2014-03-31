/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * SGI UV Broadcast Assist Unit definitions
 *
 * Copyright (C) 2008-2011 Silicon Graphics, Inc. All rights reserved.
 */

#ifndef _ASM_X86_UV_UV_BAU_H
#define _ASM_X86_UV_UV_BAU_H

#include <linux/bitmap.h>
#define BITSPERBYTE 8

/*
 * Broadcast Assist Unit messaging structures
 *
 * Selective Broadcast activations are induced by software action
 * specifying a particular 8-descriptor "set" via a 6-bit index written
 * to an MMR.
 * Thus there are 64 unique 512-byte sets of SB descriptors - one set for
 * each 6-bit index value. These descriptor sets are mapped in sequence
 * starting with set 0 located at the address specified in the
 * BAU_SB_DESCRIPTOR_BASE register, set 1 is located at BASE + 512,
 * set 2 is at BASE + 2*512, set 3 at BASE + 3*512, and so on.
 *
 * We will use one set for sending BAU messages from each of the
 * cpu's on the uvhub.
 *
 * TLB shootdown will use the first of the 8 descriptors of each set.
 * Each of the descriptors is 64 bytes in size (8*64 = 512 bytes in a set).
 */

#define MAX_CPUS_PER_UVHUB		64
#define MAX_CPUS_PER_SOCKET		32
#define ADP_SZ				64 
#define UV_CPUS_PER_AS			32 
#define ITEMS_PER_DESC			8
#define MAX_BAU_CONCURRENT		3
#define UV_ACT_STATUS_MASK		0x3
#define UV_ACT_STATUS_SIZE		2
#define UV_DISTRIBUTION_SIZE		256
#define UV_SW_ACK_NPENDING		8
#define UV1_NET_ENDPOINT_INTD		0x38
#define UV2_NET_ENDPOINT_INTD		0x28
#define UV_NET_ENDPOINT_INTD		(is_uv1_hub() ?			\
			UV1_NET_ENDPOINT_INTD : UV2_NET_ENDPOINT_INTD)
#define UV_DESC_PSHIFT			49
#define UV_PAYLOADQ_PNODE_SHIFT		49
#define UV_PTC_BASENAME			"sgi_uv/ptc_statistics"
#define UV_BAU_BASENAME			"sgi_uv/bau_tunables"
#define UV_BAU_TUNABLES_DIR		"sgi_uv"
#define UV_BAU_TUNABLES_FILE		"bau_tunables"
#define WHITESPACE			" \t\n"
#define uv_mmask			((1UL << uv_hub_info->m_val) - 1)
#define uv_physnodeaddr(x)		((__pa((unsigned long)(x)) & uv_mmask))
#define cpubit_isset(cpu, bau_local_cpumask) \
	test_bit((cpu), (bau_local_cpumask).bits)

#define UV1_INTD_SOFT_ACK_TIMEOUT_PERIOD (9UL)
#define UV2_INTD_SOFT_ACK_TIMEOUT_PERIOD (15UL)

#define UV_INTD_SOFT_ACK_TIMEOUT_PERIOD	(is_uv1_hub() ?			\
		UV1_INTD_SOFT_ACK_TIMEOUT_PERIOD :			\
		UV2_INTD_SOFT_ACK_TIMEOUT_PERIOD)

#define BAU_MISC_CONTROL_MULT_MASK	3

#define UVH_AGING_PRESCALE_SEL		0x000000b000UL
#define BAU_URGENCY_7_SHIFT		28
#define BAU_URGENCY_7_MASK		7

#define UVH_TRANSACTION_TIMEOUT		0x000000b200UL
#define BAU_TRANS_SHIFT			40
#define BAU_TRANS_MASK			0x3f

#define AS_PUSH_SHIFT UVH_LB_BAU_SB_ACTIVATION_CONTROL_PUSH_SHFT
#define SOFTACK_MSHIFT UVH_LB_BAU_MISC_CONTROL_ENABLE_INTD_SOFT_ACK_MODE_SHFT
#define SOFTACK_PSHIFT UVH_LB_BAU_MISC_CONTROL_INTD_SOFT_ACK_TIMEOUT_PERIOD_SHFT
#define SOFTACK_TIMEOUT_PERIOD UV_INTD_SOFT_ACK_TIMEOUT_PERIOD
#define write_gmmr	uv_write_global_mmr64
#define write_lmmr	uv_write_local_mmr
#define read_lmmr	uv_read_local_mmr
#define read_gmmr	uv_read_global_mmr64

#define DS_IDLE				0
#define DS_ACTIVE			1
#define DS_DESTINATION_TIMEOUT		2
#define DS_SOURCE_TIMEOUT		3
#define UV2H_DESC_IDLE			0
#define UV2H_DESC_BUSY			2
#define UV2H_DESC_DEST_TIMEOUT		4
#define UV2H_DESC_DEST_STRONG_NACK	5
#define UV2H_DESC_SOURCE_TIMEOUT	6
#define UV2H_DESC_DEST_PUT_ERR		7

#define PLUGGED_DELAY			10

#define PLUGSB4RESET			100
#define TIMEOUTSB4RESET			1
#define IPI_RESET_LIMIT			1
#define COMPLETE_THRESHOLD		5

#define UV_LB_SUBNODEID			0x10

#define UV_SA_SHFT UVH_LB_BAU_MISC_CONTROL_INTD_SOFT_ACK_TIMEOUT_PERIOD_SHFT
#define UV_SA_MASK UVH_LB_BAU_MISC_CONTROL_INTD_SOFT_ACK_TIMEOUT_PERIOD_MASK
#define UV2_ACK_MASK			0x7UL
#define UV2_ACK_UNITS_SHFT		3
#define UV2_LEG_SHFT UV2H_LB_BAU_MISC_CONTROL_USE_LEGACY_DESCRIPTOR_FORMATS_SHFT
#define UV2_EXT_SHFT UV2H_LB_BAU_MISC_CONTROL_ENABLE_EXTENDED_SB_STATUS_SHFT

#define DEST_Q_SIZE			20
#define DEST_NUM_RESOURCES		8
#define FLUSH_RETRY_PLUGGED		1
#define FLUSH_RETRY_TIMEOUT		2
#define FLUSH_GIVEUP			3
#define FLUSH_COMPLETE			4
#define FLUSH_RETRY_BUSYBUG		5

#define CONGESTED_RESPONSE_US		1000	
#define CONGESTED_REPS			10	
#define CONGESTED_PERIOD		30	
#define MSG_NOOP			0
#define MSG_REGULAR			1
#define MSG_RETRY			2

struct pnmask {
	unsigned long		bits[BITS_TO_LONGS(UV_DISTRIBUTION_SIZE)];
};

struct bau_local_cpumask {
	unsigned long		bits;
};

/*
 * Payload: 16 bytes (128 bits) (bytes 0x20-0x2f of descriptor)
 * only 12 bytes (96 bits) of the payload area are usable.
 * An additional 3 bytes (bits 27:4) of the header address are carried
 * to the next bytes of the destination payload queue.
 * And an additional 2 bytes of the header Suppl_A field are also
 * carried to the destination payload queue.
 * But the first byte of the Suppl_A becomes bits 127:120 (the 16th byte)
 * of the destination payload queue, which is written by the hardware
 * with the s/w ack resource bit vector.
 * [ effective message contents (16 bytes (128 bits) maximum), not counting
 *   the s/w ack bit vector  ]
 */

struct bau_msg_payload {
	unsigned long	address;		
	
	unsigned short	sending_cpu;		
	
	unsigned short	acknowledge_count;	
	
	unsigned int	reserved1:32;		
};


struct uv1_bau_msg_header {
	unsigned int	dest_subnodeid:6;	
	
	unsigned int	base_dest_nasid:15;	
					
	unsigned int	command:8;		
	
	
	unsigned int	rsvd_1:3;		
	
	
	unsigned int	rsvd_2:9;		
	
	
	unsigned int	sequence:16;		
				

	unsigned int	rsvd_3:1;		
	
	
	
	
	unsigned int	replied_to:1;		
	
	unsigned int	msg_type:3;		
	
	unsigned int	canceled:1;		
	
	unsigned int	payload_1a:1;		
	
	unsigned int	payload_1b:2;		
	

	
	unsigned int	payload_1ca:6;		
	
	unsigned int	payload_1c:2;		
	

	
	unsigned int	payload_1d:6;		
	
	unsigned int	payload_1e:2;		
	

	unsigned int	rsvd_4:7;		
	
	unsigned int	swack_flag:1;		
	
	unsigned int	rsvd_5:6;		
	
	unsigned int	rsvd_6:5;		
	
	unsigned int	int_both:1;		
	
	unsigned int	fairness:3;		
	
	unsigned int	multilevel:1;		
	
	
	unsigned int	chaining:1;		
	
	unsigned int	rsvd_7:21;		
	
};

struct uv2_bau_msg_header {
	unsigned int	base_dest_nasid:15;	
					
	unsigned int	dest_subnodeid:5;	
	
	unsigned int	rsvd_1:1;		
	
	
	
	
	
	unsigned int	replied_to:1;		
	
	unsigned int	msg_type:3;		
	
	unsigned int	canceled:1;		
	
	unsigned int	payload_1:3;		
	

	
	unsigned int	payload_2a:3;		
	unsigned int	payload_2b:5;		
	

	
	unsigned int	payload_3:8;		
	

	unsigned int	rsvd_2:7;		
	
	unsigned int	swack_flag:1;		
	
	unsigned int	rsvd_3a:3;		
	unsigned int	rsvd_3b:8;		
	unsigned int	rsvd_3c:8;		
	unsigned int	rsvd_3d:3;		
	
	unsigned int	fairness:3;		
	

	unsigned int	sequence:16;		
	
	unsigned int	chaining:1;		
	
	unsigned int	multilevel:1;		
	
	unsigned int	rsvd_4:24;		
	
	unsigned int	command:8;		
	
};

struct bau_desc {
	struct pnmask				distribution;
	union bau_msg_header {
		struct uv1_bau_msg_header	uv1_hdr;
		struct uv2_bau_msg_header	uv2_hdr;
	} header;

	struct bau_msg_payload			payload;
};

struct bau_pq_entry {
	unsigned long	address;	
	
	unsigned short	sending_cpu;	
	
	unsigned short	acknowledge_count; 
	
	
	unsigned short	replied_to:1;	
	unsigned short	msg_type:3;	
	unsigned short	canceled:1;	
	unsigned short	unused1:3;	
	
	unsigned char	unused2a;	
	
	unsigned char	unused2;	
	
	unsigned char	swack_vec;	
	
	unsigned short	sequence;	
	
	unsigned char	unused4[2];	
	
	int		number_of_cpus;	
	
	unsigned char	unused5[8];	
	
};

struct msg_desc {
	struct bau_pq_entry	*msg;
	int			msg_slot;
	struct bau_pq_entry	*queue_first;
	struct bau_pq_entry	*queue_last;
};

struct reset_args {
	int			sender;
};

struct ptc_stats {
	
	unsigned long	s_giveup;		
	unsigned long	s_requestor;		
	unsigned long	s_stimeout;		
	unsigned long	s_dtimeout;		
	unsigned long	s_strongnacks;		
	unsigned long	s_time;			
	unsigned long	s_retriesok;		
	unsigned long	s_ntargcpu;		
	unsigned long	s_ntargself;		
	unsigned long	s_ntarglocals;		
	unsigned long	s_ntargremotes;		
	unsigned long	s_ntarglocaluvhub;	
	unsigned long	s_ntargremoteuvhub;	
	unsigned long	s_ntarguvhub;		
	unsigned long	s_ntarguvhub16;		
	unsigned long	s_ntarguvhub8;		
	unsigned long	s_ntarguvhub4;		
	unsigned long	s_ntarguvhub2;		
	unsigned long	s_ntarguvhub1;		
	unsigned long	s_resets_plug;		
	unsigned long	s_resets_timeout;	
	unsigned long	s_busy;			
	unsigned long	s_throttles;		
	unsigned long	s_retry_messages;	
	unsigned long	s_bau_reenabled;	
	unsigned long	s_bau_disabled;		
	unsigned long	s_uv2_wars;		
	unsigned long	s_uv2_wars_hw;		
	unsigned long	s_uv2_war_waits;	
	
	unsigned long	d_alltlb;		
	unsigned long	d_onetlb;		
	unsigned long	d_multmsg;		
	unsigned long	d_nomsg;		
	unsigned long	d_time;			
	unsigned long	d_requestee;		
	unsigned long	d_retries;		
	unsigned long	d_canceled;		
	unsigned long	d_nocanceled;		
	unsigned long	d_resets;		
	unsigned long	d_rcanceled;		
};

struct tunables {
	int			*tunp;
	int			deflt;
};

struct hub_and_pnode {
	short			uvhub;
	short			pnode;
};

struct socket_desc {
	short			num_cpus;
	short			cpu_number[MAX_CPUS_PER_SOCKET];
};

struct uvhub_desc {
	unsigned short		socket_mask;
	short			num_cpus;
	short			uvhub;
	short			pnode;
	struct socket_desc	socket[2];
};

struct bau_control {
	struct bau_desc		*descriptor_base;
	struct bau_pq_entry	*queue_first;
	struct bau_pq_entry	*queue_last;
	struct bau_pq_entry	*bau_msg_head;
	struct bau_control	*uvhub_master;
	struct bau_control	*socket_master;
	struct ptc_stats	*statp;
	cpumask_t		*cpumask;
	unsigned long		timeout_interval;
	unsigned long		set_bau_on_time;
	atomic_t		active_descriptor_count;
	int			plugged_tries;
	int			timeout_tries;
	int			ipi_attempts;
	int			conseccompletes;
	int			baudisabled;
	int			set_bau_off;
	short			cpu;
	short			osnode;
	short			uvhub_cpu;
	short			uvhub;
	short			uvhub_version;
	short			cpus_in_socket;
	short			cpus_in_uvhub;
	short			partition_base_pnode;
	short			using_desc; 
	unsigned int		inuse_map;
	unsigned short		message_number;
	unsigned short		uvhub_quiesce;
	short			socket_acknowledge_count[DEST_Q_SIZE];
	cycles_t		send_message;
	spinlock_t		uvhub_lock;
	spinlock_t		queue_lock;
	
	int			max_concurr;
	int			max_concurr_const;
	int			plugged_delay;
	int			plugsb4reset;
	int			timeoutsb4reset;
	int			ipi_reset_limit;
	int			complete_threshold;
	int			cong_response_us;
	int			cong_reps;
	int			cong_period;
	unsigned long		clocks_per_100_usec;
	cycles_t		period_time;
	long			period_requests;
	struct hub_and_pnode	*thp;
};

static inline unsigned long read_mmr_uv2_status(void)
{
	return read_lmmr(UV2H_LB_BAU_SB_ACTIVATION_STATUS_2);
}

static inline void write_mmr_data_broadcast(int pnode, unsigned long mmr_image)
{
	write_gmmr(pnode, UVH_BAU_DATA_BROADCAST, mmr_image);
}

static inline void write_mmr_descriptor_base(int pnode, unsigned long mmr_image)
{
	write_gmmr(pnode, UVH_LB_BAU_SB_DESCRIPTOR_BASE, mmr_image);
}

static inline void write_mmr_activation(unsigned long index)
{
	write_lmmr(UVH_LB_BAU_SB_ACTIVATION_CONTROL, index);
}

static inline void write_gmmr_activation(int pnode, unsigned long mmr_image)
{
	write_gmmr(pnode, UVH_LB_BAU_SB_ACTIVATION_CONTROL, mmr_image);
}

static inline void write_mmr_payload_first(int pnode, unsigned long mmr_image)
{
	write_gmmr(pnode, UVH_LB_BAU_INTD_PAYLOAD_QUEUE_FIRST, mmr_image);
}

static inline void write_mmr_payload_tail(int pnode, unsigned long mmr_image)
{
	write_gmmr(pnode, UVH_LB_BAU_INTD_PAYLOAD_QUEUE_TAIL, mmr_image);
}

static inline void write_mmr_payload_last(int pnode, unsigned long mmr_image)
{
	write_gmmr(pnode, UVH_LB_BAU_INTD_PAYLOAD_QUEUE_LAST, mmr_image);
}

static inline void write_mmr_misc_control(int pnode, unsigned long mmr_image)
{
	write_gmmr(pnode, UVH_LB_BAU_MISC_CONTROL, mmr_image);
}

static inline unsigned long read_mmr_misc_control(int pnode)
{
	return read_gmmr(pnode, UVH_LB_BAU_MISC_CONTROL);
}

static inline void write_mmr_sw_ack(unsigned long mr)
{
	uv_write_local_mmr(UVH_LB_BAU_INTD_SOFTWARE_ACKNOWLEDGE_ALIAS, mr);
}

static inline void write_gmmr_sw_ack(int pnode, unsigned long mr)
{
	write_gmmr(pnode, UVH_LB_BAU_INTD_SOFTWARE_ACKNOWLEDGE_ALIAS, mr);
}

static inline unsigned long read_mmr_sw_ack(void)
{
	return read_lmmr(UVH_LB_BAU_INTD_SOFTWARE_ACKNOWLEDGE);
}

static inline unsigned long read_gmmr_sw_ack(int pnode)
{
	return read_gmmr(pnode, UVH_LB_BAU_INTD_SOFTWARE_ACKNOWLEDGE);
}

static inline void write_mmr_data_config(int pnode, unsigned long mr)
{
	uv_write_global_mmr64(pnode, UVH_BAU_DATA_CONFIG, mr);
}

static inline int bau_uvhub_isset(int uvhub, struct pnmask *dstp)
{
	return constant_test_bit(uvhub, &dstp->bits[0]);
}
static inline void bau_uvhub_set(int pnode, struct pnmask *dstp)
{
	__set_bit(pnode, &dstp->bits[0]);
}
static inline void bau_uvhubs_clear(struct pnmask *dstp,
				    int nbits)
{
	bitmap_zero(&dstp->bits[0], nbits);
}
static inline int bau_uvhub_weight(struct pnmask *dstp)
{
	return bitmap_weight((unsigned long *)&dstp->bits[0],
				UV_DISTRIBUTION_SIZE);
}

static inline void bau_cpubits_clear(struct bau_local_cpumask *dstp, int nbits)
{
	bitmap_zero(&dstp->bits, nbits);
}

extern void uv_bau_message_intr1(void);
extern void uv_bau_timeout_intr1(void);

struct atomic_short {
	short counter;
};

static inline int atomic_read_short(const struct atomic_short *v)
{
	return v->counter;
}

static inline int atom_asr(short i, struct atomic_short *v)
{
	return i + xadd(&v->counter, i);
}

static inline int atomic_inc_unless_ge(spinlock_t *lock, atomic_t *v, int u)
{
	spin_lock(lock);
	if (atomic_read(v) >= u) {
		spin_unlock(lock);
		return 0;
	}
	atomic_inc(v);
	spin_unlock(lock);
	return 1;
}

#endif 
