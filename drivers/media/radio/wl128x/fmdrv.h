/*
 *  FM Driver for Connectivity chip of Texas Instruments.
 *
 *  Common header for all FM driver sub-modules.
 *
 *  Copyright (C) 2011 Texas Instruments
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifndef _FM_DRV_H
#define _FM_DRV_H

#include <linux/skbuff.h>
#include <linux/interrupt.h>
#include <sound/core.h>
#include <sound/initval.h>
#include <linux/timer.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-common.h>
#include <media/v4l2-ctrls.h>

#define FM_DRV_VERSION            "0.1.1"
#define FM_DRV_NAME               "ti_fmdrv"
#define FM_DRV_CARD_SHORT_NAME    "TI FM Radio"
#define FM_DRV_CARD_LONG_NAME     "Texas Instruments FM Radio"

#define FM_INTTASK_RUNNING            0
#define FM_INTTASK_SCHEDULE_PENDING   1
#define FM_FW_DW_INPROGRESS     2
#define FM_CORE_READY                 3
#define FM_CORE_TRANSPORT_READY       4
#define FM_AF_SWITCH_INPROGRESS	      5
#define FM_CORE_TX_XMITING	      6

#define FM_TUNE_COMPLETE	      0x1
#define FM_BAND_LIMIT		      0x2

#define FM_DRV_TX_TIMEOUT      (5*HZ)	
#define FM_DRV_RX_SEEK_TIMEOUT (20*HZ)	

#define fmerr(format, ...) \
	printk(KERN_ERR "fmdrv: " format, ## __VA_ARGS__)
#define fmwarn(format, ...) \
	printk(KERN_WARNING "fmdrv: " format, ##__VA_ARGS__)
#ifdef DEBUG
#define fmdbg(format, ...) \
	printk(KERN_DEBUG "fmdrv: " format, ## __VA_ARGS__)
#else 
#define fmdbg(format, ...)
#endif
enum {
	FM_MODE_OFF,
	FM_MODE_TX,
	FM_MODE_RX,
	FM_MODE_ENTRY_MAX
};

#define FM_RX_RDS_INFO_FIELD_MAX	8	

struct fm_rdsdata_format {
	union {
		struct {
			u8 buff[FM_RX_RDS_INFO_FIELD_MAX];
		} groupdatabuff;
		struct {
			u16 pidata;
			u8 blk_b[2];
			u8 blk_c[2];
			u8 blk_d[2];
		} groupgeneral;
		struct {
			u16 pidata;
			u8 blk_b[2];
			u8 af[2];
			u8 ps[2];
		} group0A;
		struct {
			u16 pi[2];
			u8 blk_b[2];
			u8 ps[2];
		} group0B;
	} data;
};

struct region_info {
	u32 chanl_space;
	u32 bot_freq;
	u32 top_freq;
	u8 fm_band;
};
struct fmdev;
typedef void (*int_handler_prototype) (struct fmdev *);

struct fm_irq {
	u8 stage;
	u16 flag;	
	u16 mask;	
	
	struct timer_list timer;
	u8 retry;
	int_handler_prototype *handlers;
};

struct fm_rds {
	u8 flag;	
	u8 last_blk_idx;	

	
	wait_queue_head_t read_queue;
	u32 buf_size;	
	u32 wr_idx;
	u32 rd_idx;
	u8 *buff;
};

#define FM_RDS_MAX_AF_LIST		25

struct tuned_station_info {
	u16 picode;
	u32 af_cache[FM_RDS_MAX_AF_LIST];
	u8 afcache_size;
	u8 af_list_max;
};

struct fm_rx {
	struct region_info region;	
	u32 freq;	
	u8 mute_mode;	
	u8 deemphasis_mode; 
	
	u8 rf_depend_mute;
	u16 volume;	
	u16 rssi_threshold;	
	
	u8 afjump_idx;
	
	u32 freq_before_jump;
	u8 rds_mode;	
	u8 af_mode;	
	struct tuned_station_info stat_info;
	struct fm_rds rds;
};

#define FMTX_RDS_TXT_STR_SIZE	25
struct tx_rds {
	u8 text_type;
	u8 text[FMTX_RDS_TXT_STR_SIZE];
	u8 flag;
	u32 af_freq;
};
struct fmtx_data {
	u8 pwr_lvl;
	u8 xmit_state;
	u8 audio_io;
	u8 region;
	u16 aud_mode;
	u32 preemph;
	u32 tx_frq;
	struct tx_rds rds;
};

struct fmdev {
	struct video_device *radio_dev;	
	struct snd_card *card;	
	u16 asci_id;
	spinlock_t rds_buff_lock; 
	spinlock_t resp_skb_lock; 

	long flag;		
	u8 streg_cbdata; 

	struct sk_buff_head rx_q;	
	struct tasklet_struct rx_task;	

	struct sk_buff_head tx_q;	
	struct tasklet_struct tx_task;	
	unsigned long last_tx_jiffies;	
	atomic_t tx_cnt;	

	struct sk_buff *resp_skb;	
	
	struct completion maintask_comp;
	
	u8 pre_op;
	
	struct completion *resp_comp;
	struct fm_irq irq_info;
	u8 curr_fmmode; 
	struct fm_rx rx;	
	struct fmtx_data tx_data;

	
	struct v4l2_ctrl_handler ctrl_handler;

	
	struct mutex mutex;
};
#endif
