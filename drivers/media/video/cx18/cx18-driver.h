/*
 *  cx18 driver internal defines and structures
 *
 *  Derived from ivtv-driver.h
 *
 *  Copyright (C) 2007  Hans Verkuil <hverkuil@xs4all.nl>
 *  Copyright (C) 2008  Andy Walls <awalls@md.metrocast.net>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 *  02111-1307  USA
 */

#ifndef CX18_DRIVER_H
#define CX18_DRIVER_H

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/fs.h>
#include <linux/pci.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/i2c.h>
#include <linux/i2c-algo-bit.h>
#include <linux/list.h>
#include <linux/unistd.h>
#include <linux/pagemap.h>
#include <linux/workqueue.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <asm/byteorder.h>

#include <media/v4l2-common.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-device.h>
#include <media/v4l2-fh.h>
#include <media/tuner.h>
#include <media/ir-kbd-i2c.h>
#include "cx18-mailbox.h"
#include "cx18-av-core.h"
#include "cx23418.h"

#include "demux.h"
#include "dmxdev.h"
#include "dvb_demux.h"
#include "dvb_frontend.h"
#include "dvb_net.h"
#include "dvbdev.h"

#include <media/videobuf-core.h>
#include <media/videobuf-vmalloc.h>

#ifndef CONFIG_PCI
#  error "This driver requires kernel PCI support."
#endif

#define CX18_MEM_OFFSET	0x00000000
#define CX18_MEM_SIZE	0x04000000
#define CX18_REG_OFFSET	0x02000000

#define CX18_MAX_CARDS 32

#define CX18_CARD_HVR_1600_ESMT	      0	
#define CX18_CARD_HVR_1600_SAMSUNG    1	
#define CX18_CARD_COMPRO_H900 	      2	
#define CX18_CARD_YUAN_MPC718 	      3	
#define CX18_CARD_CNXT_RAPTOR_PAL     4	
#define CX18_CARD_TOSHIBA_QOSMIO_DVBT 5 
#define CX18_CARD_LEADTEK_PVR2100     6 
#define CX18_CARD_LEADTEK_DVR3100H    7 
#define CX18_CARD_GOTVIEW_PCI_DVD3    8 
#define CX18_CARD_HVR_1600_S5H1411    9 
#define CX18_CARD_LAST		      9

#define CX18_ENC_STREAM_TYPE_MPG  0
#define CX18_ENC_STREAM_TYPE_TS   1
#define CX18_ENC_STREAM_TYPE_YUV  2
#define CX18_ENC_STREAM_TYPE_VBI  3
#define CX18_ENC_STREAM_TYPE_PCM  4
#define CX18_ENC_STREAM_TYPE_IDX  5
#define CX18_ENC_STREAM_TYPE_RAD  6
#define CX18_MAX_STREAMS	  7

#define PCI_VENDOR_ID_CX      0x14f1
#define PCI_DEVICE_ID_CX23418 0x5b7a

#define CX18_PCI_ID_HAUPPAUGE 		0x0070
#define CX18_PCI_ID_COMPRO 		0x185b
#define CX18_PCI_ID_YUAN 		0x12ab
#define CX18_PCI_ID_CONEXANT		0x14f1
#define CX18_PCI_ID_TOSHIBA		0x1179
#define CX18_PCI_ID_LEADTEK		0x107D
#define CX18_PCI_ID_GOTVIEW		0x5854


#define CX18_DEFAULT_ENC_TS_BUFFERS  1
#define CX18_DEFAULT_ENC_MPG_BUFFERS 2
#define CX18_DEFAULT_ENC_IDX_BUFFERS 1
#define CX18_DEFAULT_ENC_YUV_BUFFERS 2
#define CX18_DEFAULT_ENC_VBI_BUFFERS 1
#define CX18_DEFAULT_ENC_PCM_BUFFERS 1

#define CX18_MAX_FW_MDLS_PER_STREAM 63

#define CX18_UNIT_ENC_YUV_BUFSIZE	(720 *  32 * 3 / 2) 
#define CX18_625_LINE_ENC_YUV_BUFSIZE	(CX18_UNIT_ENC_YUV_BUFSIZE * 576/32)
#define CX18_525_LINE_ENC_YUV_BUFSIZE	(CX18_UNIT_ENC_YUV_BUFSIZE * 480/32)

struct cx18_enc_idx_entry {
	__le32 length;
	__le32 offset_low;
	__le32 offset_high;
	__le32 flags;
	__le32 pts_low;
	__le32 pts_high;
} __attribute__ ((packed));
#define CX18_UNIT_ENC_IDX_BUFSIZE \
	(sizeof(struct cx18_enc_idx_entry) * V4L2_ENC_IDX_ENTRIES)

#define CX18_DEFAULT_ENC_TS_BUFSIZE   32
#define CX18_DEFAULT_ENC_MPG_BUFSIZE  32
#define CX18_DEFAULT_ENC_IDX_BUFSIZE  (CX18_UNIT_ENC_IDX_BUFSIZE * 1 / 1024 + 1)
#define CX18_DEFAULT_ENC_YUV_BUFSIZE  (CX18_UNIT_ENC_YUV_BUFSIZE * 3 / 1024 + 1)
#define CX18_DEFAULT_ENC_PCM_BUFSIZE   4

#define I2C_CLIENTS_MAX 16


#define CX18_DBGFLG_WARN  (1 << 0)
#define CX18_DBGFLG_INFO  (1 << 1)
#define CX18_DBGFLG_API   (1 << 2)
#define CX18_DBGFLG_DMA   (1 << 3)
#define CX18_DBGFLG_IOCTL (1 << 4)
#define CX18_DBGFLG_FILE  (1 << 5)
#define CX18_DBGFLG_I2C   (1 << 6)
#define CX18_DBGFLG_IRQ   (1 << 7)
#define CX18_DBGFLG_HIGHVOL (1 << 8)

#define CX18_DEBUG(x, type, fmt, args...) \
	do { \
		if ((x) & cx18_debug) \
			v4l2_info(&cx->v4l2_dev, " " type ": " fmt , ## args); \
	} while (0)
#define CX18_DEBUG_WARN(fmt, args...)  CX18_DEBUG(CX18_DBGFLG_WARN, "warning", fmt , ## args)
#define CX18_DEBUG_INFO(fmt, args...)  CX18_DEBUG(CX18_DBGFLG_INFO, "info", fmt , ## args)
#define CX18_DEBUG_API(fmt, args...)   CX18_DEBUG(CX18_DBGFLG_API, "api", fmt , ## args)
#define CX18_DEBUG_DMA(fmt, args...)   CX18_DEBUG(CX18_DBGFLG_DMA, "dma", fmt , ## args)
#define CX18_DEBUG_IOCTL(fmt, args...) CX18_DEBUG(CX18_DBGFLG_IOCTL, "ioctl", fmt , ## args)
#define CX18_DEBUG_FILE(fmt, args...)  CX18_DEBUG(CX18_DBGFLG_FILE, "file", fmt , ## args)
#define CX18_DEBUG_I2C(fmt, args...)   CX18_DEBUG(CX18_DBGFLG_I2C, "i2c", fmt , ## args)
#define CX18_DEBUG_IRQ(fmt, args...)   CX18_DEBUG(CX18_DBGFLG_IRQ, "irq", fmt , ## args)

#define CX18_DEBUG_HIGH_VOL(x, type, fmt, args...) \
	do { \
		if (((x) & cx18_debug) && (cx18_debug & CX18_DBGFLG_HIGHVOL)) \
			v4l2_info(&cx->v4l2_dev, " " type ": " fmt , ## args); \
	} while (0)
#define CX18_DEBUG_HI_WARN(fmt, args...)  CX18_DEBUG_HIGH_VOL(CX18_DBGFLG_WARN, "warning", fmt , ## args)
#define CX18_DEBUG_HI_INFO(fmt, args...)  CX18_DEBUG_HIGH_VOL(CX18_DBGFLG_INFO, "info", fmt , ## args)
#define CX18_DEBUG_HI_API(fmt, args...)   CX18_DEBUG_HIGH_VOL(CX18_DBGFLG_API, "api", fmt , ## args)
#define CX18_DEBUG_HI_DMA(fmt, args...)   CX18_DEBUG_HIGH_VOL(CX18_DBGFLG_DMA, "dma", fmt , ## args)
#define CX18_DEBUG_HI_IOCTL(fmt, args...) CX18_DEBUG_HIGH_VOL(CX18_DBGFLG_IOCTL, "ioctl", fmt , ## args)
#define CX18_DEBUG_HI_FILE(fmt, args...)  CX18_DEBUG_HIGH_VOL(CX18_DBGFLG_FILE, "file", fmt , ## args)
#define CX18_DEBUG_HI_I2C(fmt, args...)   CX18_DEBUG_HIGH_VOL(CX18_DBGFLG_I2C, "i2c", fmt , ## args)
#define CX18_DEBUG_HI_IRQ(fmt, args...)   CX18_DEBUG_HIGH_VOL(CX18_DBGFLG_IRQ, "irq", fmt , ## args)

#define CX18_ERR(fmt, args...)      v4l2_err(&cx->v4l2_dev, fmt , ## args)
#define CX18_WARN(fmt, args...)     v4l2_warn(&cx->v4l2_dev, fmt , ## args)
#define CX18_INFO(fmt, args...)     v4l2_info(&cx->v4l2_dev, fmt , ## args)

#define CX18_DEBUG_DEV(x, dev, type, fmt, args...) \
	do { \
		if ((x) & cx18_debug) \
			v4l2_info(dev, " " type ": " fmt , ## args); \
	} while (0)
#define CX18_DEBUG_WARN_DEV(dev, fmt, args...) \
		CX18_DEBUG_DEV(CX18_DBGFLG_WARN, dev, "warning", fmt , ## args)
#define CX18_DEBUG_INFO_DEV(dev, fmt, args...) \
		CX18_DEBUG_DEV(CX18_DBGFLG_INFO, dev, "info", fmt , ## args)
#define CX18_DEBUG_API_DEV(dev, fmt, args...) \
		CX18_DEBUG_DEV(CX18_DBGFLG_API, dev, "api", fmt , ## args)
#define CX18_DEBUG_DMA_DEV(dev, fmt, args...) \
		CX18_DEBUG_DEV(CX18_DBGFLG_DMA, dev, "dma", fmt , ## args)
#define CX18_DEBUG_IOCTL_DEV(dev, fmt, args...) \
		CX18_DEBUG_DEV(CX18_DBGFLG_IOCTL, dev, "ioctl", fmt , ## args)
#define CX18_DEBUG_FILE_DEV(dev, fmt, args...) \
		CX18_DEBUG_DEV(CX18_DBGFLG_FILE, dev, "file", fmt , ## args)
#define CX18_DEBUG_I2C_DEV(dev, fmt, args...) \
		CX18_DEBUG_DEV(CX18_DBGFLG_I2C, dev, "i2c", fmt , ## args)
#define CX18_DEBUG_IRQ_DEV(dev, fmt, args...) \
		CX18_DEBUG_DEV(CX18_DBGFLG_IRQ, dev, "irq", fmt , ## args)

#define CX18_DEBUG_HIGH_VOL_DEV(x, dev, type, fmt, args...) \
	do { \
		if (((x) & cx18_debug) && (cx18_debug & CX18_DBGFLG_HIGHVOL)) \
			v4l2_info(dev, " " type ": " fmt , ## args); \
	} while (0)
#define CX18_DEBUG_HI_WARN_DEV(dev, fmt, args...) \
	CX18_DEBUG_HIGH_VOL_DEV(CX18_DBGFLG_WARN, dev, "warning", fmt , ## args)
#define CX18_DEBUG_HI_INFO_DEV(dev, fmt, args...) \
	CX18_DEBUG_HIGH_VOL_DEV(CX18_DBGFLG_INFO, dev, "info", fmt , ## args)
#define CX18_DEBUG_HI_API_DEV(dev, fmt, args...) \
	CX18_DEBUG_HIGH_VOL_DEV(CX18_DBGFLG_API, dev, "api", fmt , ## args)
#define CX18_DEBUG_HI_DMA_DEV(dev, fmt, args...) \
	CX18_DEBUG_HIGH_VOL_DEV(CX18_DBGFLG_DMA, dev, "dma", fmt , ## args)
#define CX18_DEBUG_HI_IOCTL_DEV(dev, fmt, args...) \
	CX18_DEBUG_HIGH_VOL_DEV(CX18_DBGFLG_IOCTL, dev, "ioctl", fmt , ## args)
#define CX18_DEBUG_HI_FILE_DEV(dev, fmt, args...) \
	CX18_DEBUG_HIGH_VOL_DEV(CX18_DBGFLG_FILE, dev, "file", fmt , ## args)
#define CX18_DEBUG_HI_I2C_DEV(dev, fmt, args...) \
	CX18_DEBUG_HIGH_VOL_DEV(CX18_DBGFLG_I2C, dev, "i2c", fmt , ## args)
#define CX18_DEBUG_HI_IRQ_DEV(dev, fmt, args...) \
	CX18_DEBUG_HIGH_VOL_DEV(CX18_DBGFLG_IRQ, dev, "irq", fmt , ## args)

#define CX18_ERR_DEV(dev, fmt, args...)      v4l2_err(dev, fmt , ## args)
#define CX18_WARN_DEV(dev, fmt, args...)     v4l2_warn(dev, fmt , ## args)
#define CX18_INFO_DEV(dev, fmt, args...)     v4l2_info(dev, fmt , ## args)

extern int cx18_debug;

struct cx18_options {
	int megabytes[CX18_MAX_STREAMS]; 
	int cardtype;		
	int tuner;		
	int radio;		
};

#define CX18_F_M_NEED_SWAP  0	

#define CX18_F_S_CLAIMED 	3	
#define CX18_F_S_STREAMING      4	
#define CX18_F_S_INTERNAL_USE	5	
#define CX18_F_S_STREAMOFF	7	
#define CX18_F_S_APPL_IO        8	/* this stream is used read/written by an application */
#define CX18_F_S_STOPPING	9	

#define CX18_F_I_LOADED_FW		0 	
#define CX18_F_I_EOS			4 	
#define CX18_F_I_RADIO_USER		5 	
#define CX18_F_I_ENC_PAUSED		13 	
#define CX18_F_I_INITED			21 	
#define CX18_F_I_FAILED			22 	

#define CX18_SLICED_TYPE_TELETEXT_B     (1)
#define CX18_SLICED_TYPE_CAPTION_525    (4)
#define CX18_SLICED_TYPE_WSS_625        (5)
#define CX18_SLICED_TYPE_VPS            (7)

#define list_entry_is_past_end(pos, head, member) \
	(&pos->member == (head))

struct cx18_buffer {
	struct list_head list;
	dma_addr_t dma_handle;
	char *buf;

	u32 bytesused;
	u32 readpos;
};

struct cx18_mdl {
	struct list_head list;
	u32 id;		

	unsigned int skipped;
	unsigned long m_flags;

	struct list_head buf_list;
	struct cx18_buffer *curr_buf; 

	u32 bytesused;
	u32 readpos;
};

struct cx18_queue {
	struct list_head list;
	atomic_t depth;
	u32 bytesused;
	spinlock_t lock;
};

struct cx18_stream; 

struct cx18_dvb {
	struct cx18_stream *stream;
	struct dmx_frontend hw_frontend;
	struct dmx_frontend mem_frontend;
	struct dmxdev dmxdev;
	struct dvb_adapter dvb_adapter;
	struct dvb_demux demux;
	struct dvb_frontend *fe;
	struct dvb_net dvbnet;
	int enabled;
	int feeding;
	struct mutex feedlock;
};

struct cx18;	 
struct cx18_scb; 


#define CX18_MAX_MDL_ACKS 2
#define CX18_MAX_IN_WORK_ORDERS (CX18_MAX_FW_MDLS_PER_STREAM + 7)

#define CX18_F_EWO_MB_STALE_UPON_RECEIPT 0x1
#define CX18_F_EWO_MB_STALE_WHILE_PROC   0x2
#define CX18_F_EWO_MB_STALE \
	     (CX18_F_EWO_MB_STALE_UPON_RECEIPT | CX18_F_EWO_MB_STALE_WHILE_PROC)

struct cx18_in_work_order {
	struct work_struct work;
	atomic_t pending;
	struct cx18 *cx;
	unsigned long flags;
	int rpu;
	struct cx18_mailbox mb;
	struct cx18_mdl_ack mdl_ack[CX18_MAX_MDL_ACKS];
	char *str;
};

#define CX18_INVALID_TASK_HANDLE 0xffffffff

struct cx18_stream {
	struct video_device *video_dev;	
	struct cx18_dvb *dvb;		
	struct cx18 *cx; 		
	const char *name;		
	int type;			
	u32 handle;			
	unsigned int mdl_base_idx;

	u32 id;
	unsigned long s_flags;	
	int dma;		
	wait_queue_head_t waitq;

	
	struct list_head buf_pool;	
	u32 buffers;			
	u32 buf_size;			

	
	u32 bufs_per_mdl;
	u32 mdl_size;		

	
	struct cx18_queue q_free;	
	struct cx18_queue q_busy;	
	struct cx18_queue q_full;	
	struct cx18_queue q_idle;	

	struct work_struct out_work_order;

	
	u32 pixelformat;
	u32 vb_bytes_per_frame;
	struct list_head vb_capture;    
	spinlock_t vb_lock;
	struct timer_list vb_timeout;

	struct videobuf_queue vbuf_q;
	spinlock_t vbuf_q_lock; 
	enum v4l2_buf_type vb_type;
};

struct cx18_videobuf_buffer {
	
	struct videobuf_buffer vb;
	v4l2_std_id tvnorm; 
	u32 bytes_used;
};

struct cx18_open_id {
	struct v4l2_fh fh;
	u32 open_id;
	int type;
	struct cx18 *cx;
};

static inline struct cx18_open_id *fh2id(struct v4l2_fh *fh)
{
	return container_of(fh, struct cx18_open_id, fh);
}

static inline struct cx18_open_id *file2id(struct file *file)
{
	return fh2id(file->private_data);
}

struct cx18_card;


static const u32 vbi_active_samples = 1444; 
static const u32 vbi_hblank_samples_60Hz = 272; 
static const u32 vbi_hblank_samples_50Hz = 284; 

#define CX18_VBI_FRAMES 32

struct vbi_info {
	
	struct v4l2_format in;
	struct v4l2_sliced_vbi_format *sliced_in; 
	u32 count;    
	u32 start[2]; 

	u32 frame; 


	
	int insert_mpeg;

	struct v4l2_sliced_vbi_data sliced_data[36];

#define CX18_SLICED_MPEG_DATA_MAXSZ	1584
	
#define CX18_SLICED_MPEG_DATA_BUFSZ	(CX18_SLICED_MPEG_DATA_MAXSZ+8)
	u8 *sliced_mpeg_data[CX18_VBI_FRAMES];
	u32 sliced_mpeg_size[CX18_VBI_FRAMES];

	
	u32 inserted_frame;

	struct cx18_mdl sliced_mpeg_mdl;
	struct cx18_buffer sliced_mpeg_buf;
};

struct cx18_i2c_algo_callback_data {
	struct cx18 *cx;
	int bus_index;   
};

#define CX18_MAX_MMIO_WR_RETRIES 10

struct cx18 {
	int instance;
	struct pci_dev *pci_dev;
	struct v4l2_device v4l2_dev;
	struct v4l2_subdev *sd_av;     
	struct v4l2_subdev *sd_extmux; 

	const struct cx18_card *card;	
	const char *card_name;  
	const struct cx18_card_tuner_i2c *card_i2c; 
	u8 is_50hz;
	u8 is_60hz;
	u8 nof_inputs;		
	u8 nof_audio_inputs;	
	u32 v4l2_cap;		
	u32 hw_flags; 		
	unsigned int free_mdl_idx;
	struct cx18_scb __iomem *scb; 
	struct mutex epu2apu_mb_lock; 
	struct mutex epu2cpu_mb_lock; 

	struct cx18_av_state av_state;

	
	struct cx2341x_handler cxhdl;
	u32 filter_mode;
	u32 temporal_strength;
	u32 spatial_strength;

	
	unsigned long dualwatch_jiffies;
	u32 dualwatch_stereo_mode;

	struct mutex serialize_lock;    
	struct cx18_options options; 	
	int stream_buffers[CX18_MAX_STREAMS]; 
	int stream_buf_size[CX18_MAX_STREAMS]; 
	struct cx18_stream streams[CX18_MAX_STREAMS]; 	
	struct snd_cx18_card *alsa; 
	void (*pcm_announce_callback)(struct snd_cx18_card *card, u8 *pcm_data,
				      size_t num_bytes);

	unsigned long i_flags;  
	atomic_t ana_capturing;	
	atomic_t tot_capturing;	
	int search_pack_header;

	int open_id;		

	u32 base_addr;

	u8 card_rev;
	void __iomem *enc_mem, *reg_mem;

	struct vbi_info vbi;

	u64 mpg_data_received;
	u64 vbi_data_inserted;

	wait_queue_head_t mb_apu_waitq;
	wait_queue_head_t mb_cpu_waitq;
	wait_queue_head_t cap_w;
	
	wait_queue_head_t dma_waitq;

	u32 sw1_irq_mask;
	u32 sw2_irq_mask;
	u32 hw2_irq_mask;

	struct workqueue_struct *in_work_queue;
	char in_workq_name[11]; 
	struct cx18_in_work_order in_work_order[CX18_MAX_IN_WORK_ORDERS];
	char epu_debug_str[256]; 

	
	struct i2c_adapter i2c_adap[2];
	struct i2c_algo_bit_data i2c_algo[2];
	struct cx18_i2c_algo_callback_data i2c_algo_cb_data[2];

	struct IR_i2c_init_data ir_i2c_init_data;

	
	u32 gpio_dir;
	u32 gpio_val;
	struct mutex gpio_lock;
	struct v4l2_subdev sd_gpiomux;
	struct v4l2_subdev sd_resetctrl;

	

	
	u32 audio_input;
	u32 active_input;
	v4l2_std_id std;
	v4l2_std_id tuner_std;	

	
	struct work_struct request_module_wk;
};

static inline struct cx18 *to_cx18(struct v4l2_device *v4l2_dev)
{
	return container_of(v4l2_dev, struct cx18, v4l2_dev);
}

extern int (*cx18_ext_init)(struct cx18 *);

extern int cx18_first_minor;


int cx18_msleep_timeout(unsigned int msecs, int intr);

struct tveeprom; 
void cx18_read_eeprom(struct cx18 *cx, struct tveeprom *tv);

int cx18_init_on_first_open(struct cx18 *cx);

static inline int cx18_raw_vbi(const struct cx18 *cx)
{
	return cx->vbi.in.type == V4L2_BUF_TYPE_VBI_CAPTURE;
}

#define cx18_call_hw(cx, hw, o, f, args...)				\
	do {								\
		struct v4l2_subdev *__sd;				\
		__v4l2_device_call_subdevs_p(&(cx)->v4l2_dev, __sd,	\
			!(hw) || (__sd->grp_id & (hw)), o, f , ##args);	\
	} while (0)

#define cx18_call_all(cx, o, f, args...) cx18_call_hw(cx, 0, o, f , ##args)

#define cx18_call_hw_err(cx, hw, o, f, args...)				\
({									\
	struct v4l2_subdev *__sd;					\
	__v4l2_device_call_subdevs_until_err_p(&(cx)->v4l2_dev,		\
			__sd, !(hw) || (__sd->grp_id & (hw)), o, f,	\
			##args);					\
})

#define cx18_call_all_err(cx, o, f, args...) \
	cx18_call_hw_err(cx, 0, o, f , ##args)

#endif 
