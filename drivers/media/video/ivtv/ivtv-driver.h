/*
    ivtv driver internal defines and structures
    Copyright (C) 2003-2004  Kevin Thayer <nufan_wfk at yahoo.com>
    Copyright (C) 2004  Chris Kennedy <c@groovy.org>
    Copyright (C) 2005-2007  Hans Verkuil <hverkuil@xs4all.nl>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef IVTV_DRIVER_H
#define IVTV_DRIVER_H

/* Internal header for ivtv project:
 * Driver for the cx23415/6 chip.
 * Author: Kevin Thayer (nufan_wfk at yahoo.com)
 * License: GPL
 * http://www.ivtvdriver.org
 *
 * -----
 * MPG600/MPG160 support by  T.Adachi <tadachi@tadachi-net.com>
 *                      and Takeru KOMORIYA<komoriya@paken.org>
 *
 * AVerMedia M179 GPIO info by Chris Pinkham <cpinkham@bc2va.org>
 *                using information provided by Jiun-Kuei Jung @ AVerMedia.
 */

#include <linux/module.h>
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
#include <linux/scatterlist.h>
#include <linux/kthread.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <asm/uaccess.h>
#include <asm/byteorder.h>

#include <linux/dvb/video.h>
#include <linux/dvb/audio.h>
#include <media/v4l2-common.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-fh.h>
#include <media/tuner.h>
#include <media/cx2341x.h>
#include <media/ir-kbd-i2c.h>

#include <linux/ivtv.h>

#define IVTV_ENCODER_OFFSET	0x00000000
#define IVTV_ENCODER_SIZE	0x00800000	
#define IVTV_DECODER_OFFSET	0x01000000
#define IVTV_DECODER_SIZE	0x00800000	
#define IVTV_REG_OFFSET 	0x02000000
#define IVTV_REG_SIZE		0x00010000

#define IVTV_MAX_CARDS 32

#define IVTV_ENC_STREAM_TYPE_MPG  0
#define IVTV_ENC_STREAM_TYPE_YUV  1
#define IVTV_ENC_STREAM_TYPE_VBI  2
#define IVTV_ENC_STREAM_TYPE_PCM  3
#define IVTV_ENC_STREAM_TYPE_RAD  4
#define IVTV_DEC_STREAM_TYPE_MPG  5
#define IVTV_DEC_STREAM_TYPE_VBI  6
#define IVTV_DEC_STREAM_TYPE_VOUT 7
#define IVTV_DEC_STREAM_TYPE_YUV  8
#define IVTV_MAX_STREAMS	  9

#define IVTV_DMA_SG_OSD_ENT	(2883584/PAGE_SIZE)	

#define IVTV_REG_DMAXFER 	(0x0000)
#define IVTV_REG_DMASTATUS 	(0x0004)
#define IVTV_REG_DECDMAADDR 	(0x0008)
#define IVTV_REG_ENCDMAADDR 	(0x000c)
#define IVTV_REG_DMACONTROL 	(0x0010)
#define IVTV_REG_IRQSTATUS 	(0x0040)
#define IVTV_REG_IRQMASK 	(0x0048)

#define IVTV_REG_ENC_SDRAM_REFRESH 	(0x07F8)
#define IVTV_REG_ENC_SDRAM_PRECHARGE 	(0x07FC)
#define IVTV_REG_DEC_SDRAM_REFRESH 	(0x08F8)
#define IVTV_REG_DEC_SDRAM_PRECHARGE 	(0x08FC)
#define IVTV_REG_VDM 			(0x2800)
#define IVTV_REG_AO 			(0x2D00)
#define IVTV_REG_BYTEFLUSH 		(0x2D24)
#define IVTV_REG_SPU 			(0x9050)
#define IVTV_REG_HW_BLOCKS 		(0x9054)
#define IVTV_REG_VPU 			(0x9058)
#define IVTV_REG_APU 			(0xA064)

#define IVTV_REG_DEC_LINE_FIELD		(0x28C0)

extern int ivtv_debug;
#ifdef CONFIG_VIDEO_ADV_DEBUG
extern int ivtv_fw_debug;
#endif

#define IVTV_DBGFLG_WARN    (1 << 0)
#define IVTV_DBGFLG_INFO    (1 << 1)
#define IVTV_DBGFLG_MB      (1 << 2)
#define IVTV_DBGFLG_IOCTL   (1 << 3)
#define IVTV_DBGFLG_FILE    (1 << 4)
#define IVTV_DBGFLG_DMA     (1 << 5)
#define IVTV_DBGFLG_IRQ     (1 << 6)
#define IVTV_DBGFLG_DEC     (1 << 7)
#define IVTV_DBGFLG_YUV     (1 << 8)
#define IVTV_DBGFLG_I2C     (1 << 9)
#define IVTV_DBGFLG_HIGHVOL (1 << 10)

#define IVTV_DEBUG(x, type, fmt, args...) \
	do { \
		if ((x) & ivtv_debug) \
			v4l2_info(&itv->v4l2_dev, " " type ": " fmt , ##args);	\
	} while (0)
#define IVTV_DEBUG_WARN(fmt, args...)  IVTV_DEBUG(IVTV_DBGFLG_WARN,  "warn",  fmt , ## args)
#define IVTV_DEBUG_INFO(fmt, args...)  IVTV_DEBUG(IVTV_DBGFLG_INFO,  "info",  fmt , ## args)
#define IVTV_DEBUG_MB(fmt, args...)    IVTV_DEBUG(IVTV_DBGFLG_MB,    "mb",    fmt , ## args)
#define IVTV_DEBUG_DMA(fmt, args...)   IVTV_DEBUG(IVTV_DBGFLG_DMA,   "dma",   fmt , ## args)
#define IVTV_DEBUG_IOCTL(fmt, args...) IVTV_DEBUG(IVTV_DBGFLG_IOCTL, "ioctl", fmt , ## args)
#define IVTV_DEBUG_FILE(fmt, args...)  IVTV_DEBUG(IVTV_DBGFLG_FILE,  "file",  fmt , ## args)
#define IVTV_DEBUG_I2C(fmt, args...)   IVTV_DEBUG(IVTV_DBGFLG_I2C,   "i2c",   fmt , ## args)
#define IVTV_DEBUG_IRQ(fmt, args...)   IVTV_DEBUG(IVTV_DBGFLG_IRQ,   "irq",   fmt , ## args)
#define IVTV_DEBUG_DEC(fmt, args...)   IVTV_DEBUG(IVTV_DBGFLG_DEC,   "dec",   fmt , ## args)
#define IVTV_DEBUG_YUV(fmt, args...)   IVTV_DEBUG(IVTV_DBGFLG_YUV,   "yuv",   fmt , ## args)

#define IVTV_DEBUG_HIGH_VOL(x, type, fmt, args...) \
	do { \
		if (((x) & ivtv_debug) && (ivtv_debug & IVTV_DBGFLG_HIGHVOL)) 	\
			v4l2_info(&itv->v4l2_dev, " " type ": " fmt , ##args);	\
	} while (0)
#define IVTV_DEBUG_HI_WARN(fmt, args...)  IVTV_DEBUG_HIGH_VOL(IVTV_DBGFLG_WARN,  "warn",  fmt , ## args)
#define IVTV_DEBUG_HI_INFO(fmt, args...)  IVTV_DEBUG_HIGH_VOL(IVTV_DBGFLG_INFO,  "info",  fmt , ## args)
#define IVTV_DEBUG_HI_MB(fmt, args...)    IVTV_DEBUG_HIGH_VOL(IVTV_DBGFLG_MB,    "mb",    fmt , ## args)
#define IVTV_DEBUG_HI_DMA(fmt, args...)   IVTV_DEBUG_HIGH_VOL(IVTV_DBGFLG_DMA,   "dma",   fmt , ## args)
#define IVTV_DEBUG_HI_IOCTL(fmt, args...) IVTV_DEBUG_HIGH_VOL(IVTV_DBGFLG_IOCTL, "ioctl", fmt , ## args)
#define IVTV_DEBUG_HI_FILE(fmt, args...)  IVTV_DEBUG_HIGH_VOL(IVTV_DBGFLG_FILE,  "file",  fmt , ## args)
#define IVTV_DEBUG_HI_I2C(fmt, args...)   IVTV_DEBUG_HIGH_VOL(IVTV_DBGFLG_I2C,   "i2c",   fmt , ## args)
#define IVTV_DEBUG_HI_IRQ(fmt, args...)   IVTV_DEBUG_HIGH_VOL(IVTV_DBGFLG_IRQ,   "irq",   fmt , ## args)
#define IVTV_DEBUG_HI_DEC(fmt, args...)   IVTV_DEBUG_HIGH_VOL(IVTV_DBGFLG_DEC,   "dec",   fmt , ## args)
#define IVTV_DEBUG_HI_YUV(fmt, args...)   IVTV_DEBUG_HIGH_VOL(IVTV_DBGFLG_YUV,   "yuv",   fmt , ## args)

#define IVTV_ERR(fmt, args...)      v4l2_err(&itv->v4l2_dev, fmt , ## args)
#define IVTV_WARN(fmt, args...)     v4l2_warn(&itv->v4l2_dev, fmt , ## args)
#define IVTV_INFO(fmt, args...)     v4l2_info(&itv->v4l2_dev, fmt , ## args)

#define OUT_NONE        0
#define OUT_MPG         1
#define OUT_YUV         2
#define OUT_UDMA_YUV    3
#define OUT_PASSTHROUGH 4

#define IVTV_MAX_PGM_INDEX (400)

#define IVTV_DEFAULT_I2C_CLOCK_PERIOD	20

struct ivtv_options {
	int kilobytes[IVTV_MAX_STREAMS];        
	int cardtype;				
	int tuner;				
	int radio;				
	int newi2c;				
	int i2c_clock_period;			
};

struct ivtv_mailbox {
	u32 flags;
	u32 cmd;
	u32 retval;
	u32 timeout;
	u32 data[CX2341X_MBOX_MAX_DATA];
};

struct ivtv_api_cache {
	unsigned long last_jiffies;		
	u32 data[CX2341X_MBOX_MAX_DATA];	
};

struct ivtv_mailbox_data {
	volatile struct ivtv_mailbox __iomem *mbox;
	unsigned long busy;
	u8 max_mbox;
};

#define IVTV_F_B_NEED_BUF_SWAP  (1 << 0)	

#define IVTV_F_S_DMA_PENDING	0	
#define IVTV_F_S_DMA_HAS_VBI	1       
#define IVTV_F_S_NEEDS_DATA	2 	

#define IVTV_F_S_CLAIMED 	3	
#define IVTV_F_S_STREAMING      4	
#define IVTV_F_S_INTERNAL_USE	5	
#define IVTV_F_S_PASSTHROUGH	6	
#define IVTV_F_S_STREAMOFF	7	
#define IVTV_F_S_APPL_IO        8	/* this stream is used read/written by an application */

#define IVTV_F_S_PIO_PENDING	9	
#define IVTV_F_S_PIO_HAS_VBI	1       

#define IVTV_F_I_DMA		   0 	
#define IVTV_F_I_UDMA		   1 	
#define IVTV_F_I_UDMA_PENDING	   2 	
#define IVTV_F_I_SPEED_CHANGE	   3 	
#define IVTV_F_I_EOS		   4 	
#define IVTV_F_I_RADIO_USER	   5 	
#define IVTV_F_I_DIG_RST	   6 	
#define IVTV_F_I_DEC_YUV	   7 	
#define IVTV_F_I_UPDATE_CC	   9  	
#define IVTV_F_I_UPDATE_WSS	   10 	
#define IVTV_F_I_UPDATE_VPS	   11 	
#define IVTV_F_I_DECODING_YUV	   12 	
#define IVTV_F_I_ENC_PAUSED	   13 	
#define IVTV_F_I_VALID_DEC_TIMINGS 14 	
#define IVTV_F_I_HAVE_WORK  	   15	
#define IVTV_F_I_WORK_HANDLER_VBI  16	
#define IVTV_F_I_WORK_HANDLER_YUV  17	
#define IVTV_F_I_WORK_HANDLER_PIO  18	
#define IVTV_F_I_PIO		   19	
#define IVTV_F_I_DEC_PAUSED	   20 	
#define IVTV_F_I_INITED		   21 	
#define IVTV_F_I_FAILED		   22 	

#define IVTV_F_I_EV_DEC_STOPPED	   28	
#define IVTV_F_I_EV_VSYNC	   29 	
#define IVTV_F_I_EV_VSYNC_FIELD    30 	
#define IVTV_F_I_EV_VSYNC_ENABLED  31 	

struct ivtv_sg_element {
	__le32 src;
	__le32 dst;
	__le32 size;
};

struct ivtv_sg_host_element {
	u32 src;
	u32 dst;
	u32 size;
};

struct ivtv_user_dma {
	struct mutex lock;
	int page_count;
	struct page *map[IVTV_DMA_SG_OSD_ENT];
	
	struct page *bouncemap[IVTV_DMA_SG_OSD_ENT];

	
	struct ivtv_sg_element SGarray[IVTV_DMA_SG_OSD_ENT];
	dma_addr_t SG_handle;
	int SG_length;

	
	struct scatterlist SGlist[IVTV_DMA_SG_OSD_ENT];
};

struct ivtv_dma_page_info {
	unsigned long uaddr;
	unsigned long first;
	unsigned long last;
	unsigned int offset;
	unsigned int tail;
	int page_count;
};

struct ivtv_buffer {
	struct list_head list;
	dma_addr_t dma_handle;
	unsigned short b_flags;
	unsigned short dma_xfer_cnt;
	char *buf;
	u32 bytesused;
	u32 readpos;
};

struct ivtv_queue {
	struct list_head list;          
	u32 buffers;                    
	u32 length;                     
	u32 bytesused;                  
};

struct ivtv;				

struct ivtv_stream {
	struct video_device *vdev;	
	struct ivtv *itv; 		
	const char *name;		
	int type;			
	u32 caps;			

	struct v4l2_fh *fh;		
	spinlock_t qlock; 		
	unsigned long s_flags;		
	int dma;			
	u32 pending_offset;
	u32 pending_backup;
	u64 pending_pts;

	u32 dma_offset;
	u32 dma_backup;
	u64 dma_pts;

	int subtype;
	wait_queue_head_t waitq;
	u32 dma_last_offset;

	
	u32 buffers;
	u32 buf_size;
	u32 buffers_stolen;

	
	struct ivtv_queue q_free;	
	struct ivtv_queue q_full;	
	struct ivtv_queue q_io;		
	struct ivtv_queue q_dma;	
	struct ivtv_queue q_predma;	

	u16 dma_xfer_cnt;

	
	struct ivtv_sg_host_element *sg_pending;
	struct ivtv_sg_host_element *sg_processing;
	struct ivtv_sg_element *sg_dma;
	dma_addr_t sg_handle;
	int sg_pending_size;
	int sg_processing_size;
	int sg_processed;

	
	struct scatterlist *SGlist;
};

struct ivtv_open_id {
	struct v4l2_fh fh;
	int type;                       
	int yuv_frames;                 
	struct ivtv *itv;
};

static inline struct ivtv_open_id *fh2id(struct v4l2_fh *fh)
{
	return container_of(fh, struct ivtv_open_id, fh);
}

struct yuv_frame_info
{
	u32 update;
	s32 src_x;
	s32 src_y;
	u32 src_w;
	u32 src_h;
	s32 dst_x;
	s32 dst_y;
	u32 dst_w;
	u32 dst_h;
	s32 pan_x;
	s32 pan_y;
	u32 vis_w;
	u32 vis_h;
	u32 interlaced_y;
	u32 interlaced_uv;
	s32 tru_x;
	u32 tru_w;
	u32 tru_h;
	u32 offset_y;
	s32 lace_mode;
	u32 sync_field;
	u32 delay;
	u32 interlaced;
};

#define IVTV_YUV_MODE_INTERLACED	0x00
#define IVTV_YUV_MODE_PROGRESSIVE	0x01
#define IVTV_YUV_MODE_AUTO		0x02
#define IVTV_YUV_MODE_MASK		0x03

#define IVTV_YUV_SYNC_EVEN		0x00
#define IVTV_YUV_SYNC_ODD		0x04
#define IVTV_YUV_SYNC_MASK		0x04

#define IVTV_YUV_BUFFERS 8

struct yuv_playback_info
{
	u32 reg_2834;
	u32 reg_2838;
	u32 reg_283c;
	u32 reg_2840;
	u32 reg_2844;
	u32 reg_2848;
	u32 reg_2854;
	u32 reg_285c;
	u32 reg_2864;

	u32 reg_2870;
	u32 reg_2874;
	u32 reg_2890;
	u32 reg_2898;
	u32 reg_289c;

	u32 reg_2918;
	u32 reg_291c;
	u32 reg_2920;
	u32 reg_2924;
	u32 reg_2928;
	u32 reg_292c;
	u32 reg_2930;

	u32 reg_2934;

	u32 reg_2938;
	u32 reg_293c;
	u32 reg_2940;
	u32 reg_2944;
	u32 reg_2948;
	u32 reg_294c;
	u32 reg_2950;
	u32 reg_2954;
	u32 reg_2958;
	u32 reg_295c;
	u32 reg_2960;
	u32 reg_2964;
	u32 reg_2968;
	u32 reg_296c;

	u32 reg_2970;

	int v_filter_1;
	int v_filter_2;
	int h_filter;

	u8 track_osd; 

	u32 osd_x_offset;
	u32 osd_y_offset;

	u32 osd_x_pan;
	u32 osd_y_pan;

	u32 osd_vis_w;
	u32 osd_vis_h;

	u32 osd_full_w;
	u32 osd_full_h;

	int decode_height;

	int lace_mode;
	int lace_threshold;
	int lace_sync_field;

	atomic_t next_dma_frame;
	atomic_t next_fill_frame;

	u32 yuv_forced_update;
	int update_frame;

	u8 fields_lapsed;   

	struct yuv_frame_info new_frame_info[IVTV_YUV_BUFFERS];
	struct yuv_frame_info old_frame_info;
	struct yuv_frame_info old_frame_info_args;

	void *blanking_ptr;
	dma_addr_t blanking_dmaptr;

	int stream_size;

	u8 draw_frame; 
	u8 max_frames_buffered; 

	struct v4l2_rect main_rect;
	u32 v4l2_src_w;
	u32 v4l2_src_h;

	u8 running; 
};

#define IVTV_VBI_FRAMES 32

struct vbi_cc {
	u8 odd[2];	
	u8 even[2];	;
};

struct vbi_vps {
	u8 data[5];	
};

struct vbi_info {
	

	u32 raw_decoder_line_size;              
	u8 raw_decoder_sav_odd_field;           
	u8 raw_decoder_sav_even_field;          
	u32 sliced_decoder_line_size;           
	u8 sliced_decoder_sav_odd_field;        
	u8 sliced_decoder_sav_even_field;       

	u32 start[2];				
	u32 count;				
	u32 raw_size;				
	u32 sliced_size;			

	u32 dec_start;				
	u32 enc_start;				
	u32 enc_size;				
	int fpi;				

	struct v4l2_format in;			
	struct v4l2_sliced_vbi_format *sliced_in; 
	int insert_mpeg;			

	

	u32 frame; 				

	

	struct vbi_cc cc_payload[256];		
	int cc_payload_idx;			
	u8 cc_missing_cnt;			
	int wss_payload;			
	u8 wss_missing_cnt;			
	struct vbi_vps vps_payload;		

	

	struct v4l2_sliced_vbi_data sliced_data[36];	
	struct v4l2_sliced_vbi_data sliced_dec_data[36];

	

	u8 *sliced_mpeg_data[IVTV_VBI_FRAMES];
	u32 sliced_mpeg_size[IVTV_VBI_FRAMES];
	struct ivtv_buffer sliced_mpeg_buf;	
	u32 inserted_frame;			
};

struct ivtv_card;

struct ivtv {
	
	struct pci_dev *pdev;		
	const struct ivtv_card *card;	
	const char *card_name;          
	const struct ivtv_card_tuner_i2c *card_i2c; 
	u8 has_cx23415;			
	u8 pvr150_workaround;           
	u8 nof_inputs;			
	u8 nof_audio_inputs;		
	u32 v4l2_cap;			
	u32 hw_flags; 			
	v4l2_std_id tuner_std;		
	struct v4l2_subdev *sd_video;	
	struct v4l2_subdev *sd_audio;	
	struct v4l2_subdev *sd_muxer;	
	u32 base_addr;                  
	volatile void __iomem *enc_mem; 
	volatile void __iomem *dec_mem; 
	volatile void __iomem *reg_mem; 
	struct ivtv_options options; 	

	struct v4l2_device v4l2_dev;
	struct cx2341x_handler cxhdl;
	struct {
		
		struct v4l2_ctrl *ctrl_pts;
		struct v4l2_ctrl *ctrl_frame;
	};
	struct {
		
		struct v4l2_ctrl *ctrl_audio_playback;
		struct v4l2_ctrl *ctrl_audio_multilingual_playback;
	};
	struct v4l2_ctrl_handler hdl_gpio;
	struct v4l2_subdev sd_gpio;	
	u16 instance;

	
	unsigned long i_flags;          
	u8 is_50hz;                     
	u8 is_60hz                      ;
	u8 is_out_50hz                  ;
	u8 is_out_60hz                  ;
	int output_mode;                
	u32 audio_input;                
	u32 active_input;               
	u32 active_output;              
	v4l2_std_id std;                
	v4l2_std_id std_out;            
	u8 audio_stereo_mode;           
	u8 audio_bilingual_mode;        

	
	spinlock_t lock;                
	struct mutex serialize_lock;    

	
	int stream_buf_size[IVTV_MAX_STREAMS];          
	struct ivtv_stream streams[IVTV_MAX_STREAMS]; 	
	atomic_t capturing;		
	atomic_t decoding;		


	
	u32 irqmask;                    
	u32 irq_rr_idx;                 
	struct kthread_worker irq_worker;		
	struct task_struct *irq_worker_task;		
	struct kthread_work irq_work;	
	spinlock_t dma_reg_lock;        
	int cur_dma_stream;		
	int cur_pio_stream;		
	u32 dma_data_req_offset;        
	u32 dma_data_req_size;          
	int dma_retries;                
	struct ivtv_user_dma udma;      
	struct timer_list dma_timer;    
	u32 last_vsync_field;           
	wait_queue_head_t dma_waitq;    
	wait_queue_head_t eos_waitq;    
	wait_queue_head_t event_waitq;  
	wait_queue_head_t vsync_waitq;  


	
	struct ivtv_mailbox_data enc_mbox;              
	struct ivtv_mailbox_data dec_mbox;              
	struct ivtv_api_cache api_cache[256]; 		


	
	struct i2c_adapter i2c_adap;
	struct i2c_algo_bit_data i2c_algo;
	struct i2c_client i2c_client;
	int i2c_state;                  
	struct mutex i2c_bus_lock;      

	struct IR_i2c_init_data ir_i2c_init_data;

	
	u32 pgm_info_offset;            
	u32 pgm_info_num;               
	u32 pgm_info_write_idx;         /* last index written by the card that was transferred to pgm_info[] */
	u32 pgm_info_read_idx;          
	struct v4l2_enc_idx_entry pgm_info[IVTV_MAX_PGM_INDEX]; 


	
	u32 open_id;			
	int search_pack_header;         
	int speed;                      
	u8 speed_mute_audio;            
	u64 mpg_data_received;          
	u64 vbi_data_inserted;          
	u32 last_dec_timing[3];         
	unsigned long dualwatch_jiffies;
	u32 dualwatch_stereo_mode;      


	
	struct vbi_info vbi;            


	
	struct yuv_playback_info yuv_info;              


	
	unsigned long osd_video_pbase;
	int osd_global_alpha_state;     
	int osd_local_alpha_state;      
	int osd_chroma_key_state;       
	u8  osd_global_alpha;           
	u32 osd_chroma_key;             
	struct v4l2_rect osd_rect;      
	struct v4l2_rect main_rect;     
	struct osd_info *osd_info;      
	void (*ivtvfb_restore)(struct ivtv *itv); 
};

static inline struct ivtv *to_ivtv(struct v4l2_device *v4l2_dev)
{
	return container_of(v4l2_dev, struct ivtv, v4l2_dev);
}

extern int ivtv_first_minor;


void ivtv_set_irq_mask(struct ivtv *itv, u32 mask);
void ivtv_clear_irq_mask(struct ivtv *itv, u32 mask);

int ivtv_set_output_mode(struct ivtv *itv, int mode);

struct ivtv_stream *ivtv_get_output_stream(struct ivtv *itv);

int ivtv_msleep_timeout(unsigned int msecs, int intr);

int ivtv_waitq(wait_queue_head_t *waitq);

struct tveeprom; 
void ivtv_read_eeprom(struct ivtv *itv, struct tveeprom *tv);

int ivtv_init_on_first_open(struct ivtv *itv);

static inline int ivtv_raw_vbi(const struct ivtv *itv)
{
	return itv->vbi.in.type == V4L2_BUF_TYPE_VBI_CAPTURE;
}

#define write_sync(val, reg) \
	do { writel(val, reg); readl(reg); } while (0)

#define read_reg(reg) readl(itv->reg_mem + (reg))
#define write_reg(val, reg) writel(val, itv->reg_mem + (reg))
#define write_reg_sync(val, reg) \
	do { write_reg(val, reg); read_reg(reg); } while (0)

#define read_enc(addr) readl(itv->enc_mem + (u32)(addr))
#define write_enc(val, addr) writel(val, itv->enc_mem + (u32)(addr))
#define write_enc_sync(val, addr) \
	do { write_enc(val, addr); read_enc(addr); } while (0)

#define read_dec(addr) readl(itv->dec_mem + (u32)(addr))
#define write_dec(val, addr) writel(val, itv->dec_mem + (u32)(addr))
#define write_dec_sync(val, addr) \
	do { write_dec(val, addr); read_dec(addr); } while (0)

#define ivtv_call_hw(itv, hw, o, f, args...) 				\
	do {								\
		struct v4l2_subdev *__sd;				\
		__v4l2_device_call_subdevs_p(&(itv)->v4l2_dev, __sd,	\
			!(hw) || (__sd->grp_id & (hw)), o, f , ##args);	\
	} while (0)

#define ivtv_call_all(itv, o, f, args...) ivtv_call_hw(itv, 0, o, f , ##args)

#define ivtv_call_hw_err(itv, hw, o, f, args...)			\
({									\
	struct v4l2_subdev *__sd;					\
	__v4l2_device_call_subdevs_until_err_p(&(itv)->v4l2_dev, __sd,	\
		!(hw) || (__sd->grp_id & (hw)), o, f , ##args);		\
})

#define ivtv_call_all_err(itv, o, f, args...) ivtv_call_hw_err(itv, 0, o, f , ##args)

#endif
