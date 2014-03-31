/*
 * Copyright(c) 2007 Intel Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Maintained at www.Open-FCoE.org
 */

#ifndef _FC_FRAME_H_
#define _FC_FRAME_H_

#include <linux/scatterlist.h>
#include <linux/skbuff.h>
#include <scsi/scsi_cmnd.h>

#include <scsi/fc/fc_fs.h>
#include <scsi/fc/fc_fcp.h>
#include <scsi/fc/fc_encaps.h>

#include <linux/if_ether.h>


#define ntohll(x) be64_to_cpu(x)
#define htonll(x) cpu_to_be64(x)

static inline u32 ntoh24(const u8 *p)
{
	return (p[0] << 16) | (p[1] << 8) | p[2];
}

static inline void hton24(u8 *p, u32 v)
{
	p[0] = (v >> 16) & 0xff;
	p[1] = (v >> 8) & 0xff;
	p[2] = v & 0xff;
}


#define	FC_FRAME_HEADROOM	32	
#define	FC_FRAME_TAILROOM	8	

#define FC_FRAME_SG_LEN		(MAX_SKB_FRAGS - 1)

#define fp_skb(fp)	(&((fp)->skb))
#define fr_hdr(fp)	((fp)->skb.data)
#define fr_len(fp)	((fp)->skb.len)
#define fr_cb(fp)	((struct fcoe_rcv_info *)&((fp)->skb.cb[0]))
#define fr_dev(fp)	(fr_cb(fp)->fr_dev)
#define fr_seq(fp)	(fr_cb(fp)->fr_seq)
#define fr_sof(fp)	(fr_cb(fp)->fr_sof)
#define fr_eof(fp)	(fr_cb(fp)->fr_eof)
#define fr_flags(fp)	(fr_cb(fp)->fr_flags)
#define fr_encaps(fp)	(fr_cb(fp)->fr_encaps)
#define fr_max_payload(fp)	(fr_cb(fp)->fr_max_payload)
#define fr_fsp(fp)	(fr_cb(fp)->fr_fsp)
#define fr_crc(fp)	(fr_cb(fp)->fr_crc)

struct fc_frame {
	struct sk_buff skb;
};

struct fcoe_rcv_info {
	struct fc_lport	*fr_dev;	
	struct fc_seq	*fr_seq;	
	struct fc_fcp_pkt *fr_fsp;	
	u32		fr_crc;
	u16		fr_max_payload;	
	u8		fr_sof;		
	u8		fr_eof;		
	u8		fr_flags;	
	u8		fr_encaps;	
	u8		granted_mac[ETH_ALEN]; 
};


static inline struct fcoe_rcv_info *fcoe_dev_from_skb(const struct sk_buff *skb)
{
	BUILD_BUG_ON(sizeof(struct fcoe_rcv_info) > sizeof(skb->cb));
	return (struct fcoe_rcv_info *) skb->cb;
}

#define	FCPHF_CRC_UNCHECKED	0x01	

static inline void fc_frame_init(struct fc_frame *fp)
{
	fr_dev(fp) = NULL;
	fr_seq(fp) = NULL;
	fr_flags(fp) = 0;
	fr_encaps(fp) = 0;
}

struct fc_frame *fc_frame_alloc_fill(struct fc_lport *, size_t payload_len);
struct fc_frame *_fc_frame_alloc(size_t payload_len);

static inline struct fc_frame *fc_frame_alloc(struct fc_lport *dev, size_t len)
{
	struct fc_frame *fp;

	if (len && len % 4)
		fp = fc_frame_alloc_fill(dev, len);
	else
		fp = _fc_frame_alloc(len);
	return fp;
}

static inline void fc_frame_free(struct fc_frame *fp)
{
	kfree_skb(fp_skb(fp));
}

static inline int fc_frame_is_linear(struct fc_frame *fp)
{
	return !skb_is_nonlinear(fp_skb(fp));
}

static inline
struct fc_frame_header *__fc_frame_header_get(const struct fc_frame *fp)
{
	return (struct fc_frame_header *)fr_hdr(fp);
}

static inline
struct fc_frame_header *fc_frame_header_get(const struct fc_frame *fp)
{
	WARN_ON(fr_len(fp) < sizeof(struct fc_frame_header));
	return __fc_frame_header_get(fp);
}

static inline u32 fc_frame_sid(const struct fc_frame *fp)
{
	return ntoh24(__fc_frame_header_get(fp)->fh_s_id);
}

static inline u32 fc_frame_did(const struct fc_frame *fp)
{
	return ntoh24(__fc_frame_header_get(fp)->fh_d_id);
}

static inline void *fc_frame_payload_get(const struct fc_frame *fp,
					 size_t len)
{
	void *pp = NULL;

	if (fr_len(fp) >= sizeof(struct fc_frame_header) + len)
		pp = fc_frame_header_get(fp) + 1;
	return pp;
}

static inline u8 fc_frame_payload_op(const struct fc_frame *fp)
{
	u8 *cp;

	cp = fc_frame_payload_get(fp, sizeof(u8));
	if (!cp)
		return 0;
	return *cp;

}

static inline enum fc_class fc_frame_class(const struct fc_frame *fp)
{
	return fc_sof_class(fr_sof(fp));
}

u32 fc_frame_crc_check(struct fc_frame *);

static inline u8 fc_frame_rctl(const struct fc_frame *fp)
{
	return fc_frame_header_get(fp)->fh_r_ctl;
}

static inline bool fc_frame_is_cmd(const struct fc_frame *fp)
{
	return fc_frame_rctl(fp) == FC_RCTL_DD_UNSOL_CMD;
}

void fc_frame_leak_check(void);

#endif 
