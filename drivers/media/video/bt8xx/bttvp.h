/*

    bttv - Bt848 frame grabber driver

    bttv's *private* header file  --  nobody other than bttv itself
    should ever include this file.

    (c) 2000-2002 Gerd Knorr <kraxel@bytesex.org>

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
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#ifndef _BTTVP_H_
#define _BTTVP_H_

#include <linux/types.h>
#include <linux/wait.h>
#include <linux/i2c.h>
#include <linux/i2c-algo-bit.h>
#include <linux/pci.h>
#include <linux/input.h>
#include <linux/mutex.h>
#include <linux/scatterlist.h>
#include <asm/io.h>
#include <media/v4l2-common.h>
#include <linux/device.h>
#include <media/videobuf-dma-sg.h>
#include <media/tveeprom.h>
#include <media/rc-core.h>
#include <media/ir-kbd-i2c.h>

#include "bt848.h"
#include "bttv.h"
#include "btcx-risc.h"

#ifdef __KERNEL__

#define FORMAT_FLAGS_DITHER       0x01
#define FORMAT_FLAGS_PACKED       0x02
#define FORMAT_FLAGS_PLANAR       0x04
#define FORMAT_FLAGS_RAW          0x08
#define FORMAT_FLAGS_CrCb         0x10

#define RISC_SLOT_O_VBI        4
#define RISC_SLOT_O_FIELD      6
#define RISC_SLOT_E_VBI       10
#define RISC_SLOT_E_FIELD     12
#define RISC_SLOT_LOOP        14

#define RESOURCE_OVERLAY       1
#define RESOURCE_VIDEO_STREAM  2
#define RESOURCE_VBI           4
#define RESOURCE_VIDEO_READ    8

#define RAW_LINES            640
#define RAW_BPL             1024

#define UNSET (-1U)

#define MIN_VDELAY 2
#define MAX_HDELAY (0x3FF & -2)
#define MAX_HACTIVE (0x3FF & -4)

#define BTTV_NORMS    (\
		V4L2_STD_PAL    | V4L2_STD_PAL_N | \
		V4L2_STD_PAL_Nc | V4L2_STD_SECAM | \
		V4L2_STD_NTSC   | V4L2_STD_PAL_M | \
		V4L2_STD_PAL_60)

struct bttv_tvnorm {
	int   v4l2_id;
	char  *name;
	u32   Fsc;
	u16   swidth, sheight; 
	u16   totalwidth;
	u8    adelay, bdelay, iform;
	u32   scaledtwidth;
	u16   hdelayx1, hactivex1;
	u16   vdelay;
	u8    vbipack;
	u16   vtotal;
	int   sram;
	u16   vbistart[2];
	struct v4l2_cropcap cropcap;
};
extern const struct bttv_tvnorm bttv_tvnorms[];

struct bttv_format {
	char *name;
	int  fourcc;          
	int  btformat;        
	int  btswap;          
	int  depth;           
	int  flags;
	int  hshift,vshift;   
};

struct bttv_ir {
	struct rc_dev           *dev;
	struct timer_list       timer;

	char                    name[32];
	char                    phys[32];

	
	u32                     mask_keycode;
	u32                     mask_keydown;
	u32                     mask_keyup;
	u32                     polling;
	u32                     last_gpio;
	int                     shift_by;
	int                     start; 
	int                     addr; 
	int                     rc5_remote_gap;

	
	bool			rc5_gpio;   
	u32                     last_bit;   
	u32                     code;       
	struct timeval          base_time;  
	bool                    active;     
};



struct bttv_geometry {
	u8  vtc,crop,comb;
	u16 width,hscale,hdelay;
	u16 sheight,vscale,vdelay,vtotal;
};

struct bttv_buffer {
	
	struct videobuf_buffer     vb;

	
	const struct bttv_format   *fmt;
	unsigned int               tvnorm;
	int                        btformat;
	int                        btswap;
	struct bttv_geometry       geo;
	struct btcx_riscmem        top;
	struct btcx_riscmem        bottom;
	struct v4l2_rect           crop;
	unsigned int               vbi_skip[2];
	unsigned int               vbi_count[2];
};

struct bttv_buffer_set {
	struct bttv_buffer     *top;       
	struct bttv_buffer     *bottom;    
	unsigned int           top_irq;
	unsigned int           frame_irq;
};

struct bttv_overlay {
	unsigned int           tvnorm;
	struct v4l2_rect       w;
	enum v4l2_field        field;
	struct v4l2_clip       *clips;
	int                    nclips;
	int                    setup_ok;
};

struct bttv_vbi_fmt {
	struct v4l2_vbi_format fmt;

	
	const struct bttv_tvnorm *tvnorm;

	__s32                  end;
};

void bttv_vbi_fmt_reset(struct bttv_vbi_fmt *f, unsigned int norm);

struct bttv_crop {
	
	struct v4l2_rect       rect;

	__s32                  min_scaled_width;
	__s32                  min_scaled_height;
	__s32                  max_scaled_width;
	__s32                  max_scaled_height;
};

struct bttv_fh {
	struct bttv              *btv;
	int resources;
#ifdef VIDIOC_G_PRIORITY
	enum v4l2_priority       prio;
#endif
	enum v4l2_buf_type       type;

	
	struct videobuf_queue    cap;
	const struct bttv_format *fmt;
	int                      width;
	int                      height;

	
	const struct bttv_format *ovfmt;
	struct bttv_overlay      ov;

	
	int                      do_crop;

	
	struct videobuf_queue    vbi;
	struct bttv_vbi_fmt      vbi_fmt;
};


int bttv_risc_packed(struct bttv *btv, struct btcx_riscmem *risc,
		     struct scatterlist *sglist,
		     unsigned int offset, unsigned int bpl,
		     unsigned int pitch, unsigned int skip_lines,
		     unsigned int store_lines);

void bttv_set_dma(struct bttv *btv, int override);
int bttv_risc_init_main(struct bttv *btv);
int bttv_risc_hook(struct bttv *btv, int slot, struct btcx_riscmem *risc,
		   int irqflags);

int bttv_buffer_risc(struct bttv *btv, struct bttv_buffer *buf);
int bttv_buffer_activate_video(struct bttv *btv,
			       struct bttv_buffer_set *set);
int bttv_buffer_activate_vbi(struct bttv *btv,
			     struct bttv_buffer *vbi);
void bttv_dma_free(struct videobuf_queue *q, struct bttv *btv,
		   struct bttv_buffer *buf);

int bttv_overlay_risc(struct bttv *btv, struct bttv_overlay *ov,
		      const struct bttv_format *fmt,
		      struct bttv_buffer *buf);



int bttv_try_fmt_vbi_cap(struct file *file, void *fh, struct v4l2_format *f);
int bttv_g_fmt_vbi_cap(struct file *file, void *fh, struct v4l2_format *f);
int bttv_s_fmt_vbi_cap(struct file *file, void *fh, struct v4l2_format *f);

extern struct videobuf_queue_ops bttv_vbi_qops;


extern struct bus_type bttv_sub_bus_type;
int bttv_sub_add_device(struct bttv_core *core, char *name);
int bttv_sub_del_devices(struct bttv_core *core);


extern int no_overlay;


extern void init_bttv_i2c_ir(struct bttv *btv);
extern int fini_bttv_i2c(struct bttv *btv);


extern unsigned int bttv_verbose;
extern unsigned int bttv_debug;
extern unsigned int bttv_gpio;
extern void bttv_gpio_tracking(struct bttv *btv, char *comment);
extern int init_bttv_i2c(struct bttv *btv);

#define dprintk(fmt, ...)			\
do {						\
	if (bttv_debug >= 1)			\
		pr_debug(fmt, ##__VA_ARGS__);	\
} while (0)
#define dprintk_cont(fmt, ...)			\
do {						\
	if (bttv_debug >= 1)			\
		pr_cont(fmt, ##__VA_ARGS__);	\
} while (0)
#define d2printk(fmt, ...)			\
do {						\
	if (bttv_debug >= 2)			\
		printk(fmt, ##__VA_ARGS__);	\
} while (0)

#define BTTV_MAX_FBUF   0x208000
#define BTTV_TIMEOUT    msecs_to_jiffies(500)    
#define BTTV_FREE_IDLE  msecs_to_jiffies(1000)   


struct bttv_pll_info {
	unsigned int pll_ifreq;    
	unsigned int pll_ofreq;    
	unsigned int pll_crystal;  
	unsigned int pll_current;  
};

struct bttv_input {
	struct input_dev      *dev;
	char                  name[32];
	char                  phys[32];
	u32                   mask_keycode;
	u32                   mask_keydown;
};

struct bttv_suspend_state {
	u32  gpio_enable;
	u32  gpio_data;
	int  disabled;
	int  loop_irq;
	struct bttv_buffer_set video;
	struct bttv_buffer     *vbi;
};

struct bttv {
	struct bttv_core c;

	
	unsigned short id;
	unsigned char revision;
	unsigned char __iomem *bt848_mmio;   

	
	unsigned int cardid;   
	unsigned int tuner_type;  
	unsigned int tda9887_conf;
	unsigned int svhs, dig;
	unsigned int has_saa6588:1;
	struct bttv_pll_info pll;
	int triton1;
	int gpioirq;

	int use_i2c_hw;

	
	int shutdown;

	void (*volume_gpio)(struct bttv *btv, __u16 volume);
	void (*audio_mode_gpio)(struct bttv *btv, struct v4l2_tuner *tuner, int set);

	
	spinlock_t gpio_lock;

	
	struct i2c_algo_bit_data   i2c_algo;
	struct i2c_client          i2c_client;
	int                        i2c_state, i2c_rc;
	int                        i2c_done;
	wait_queue_head_t          i2c_queue;
	struct v4l2_subdev 	  *sd_msp34xx;
	struct v4l2_subdev 	  *sd_tvaudio;

	
	struct video_device *video_dev;
	struct video_device *radio_dev;
	struct video_device *vbi_dev;

	
	int has_remote;
	struct bttv_ir *remote;

	
	struct IR_i2c_init_data    init_data;

	
	spinlock_t s_lock;
	struct mutex lock;
	int resources;
#ifdef VIDIOC_G_PRIORITY
	struct v4l2_prio_state prio;
#endif

	
	unsigned int input;
	unsigned int audio;
	unsigned int mute;
	unsigned long freq;
	unsigned int tvnorm;
	int hue, contrast, bright, saturation;
	struct v4l2_framebuffer fbuf;
	unsigned int field_count;

	
	int opt_combfilter;
	int opt_lumafilter;
	int opt_automute;
	int opt_chroma_agc;
	int opt_adc_crush;
	int opt_vcr_hack;
	int opt_whitecrush_upper;
	int opt_whitecrush_lower;
	int opt_uv_ratio;
	int opt_full_luma_range;
	int opt_coring;

	
	int has_radio;
	int radio_user;

	int has_matchbox;
	int mbox_we;
	int mbox_data;
	int mbox_clk;
	int mbox_most;
	int mbox_mask;

	
	int mbox_ior;
	int mbox_iow;
	int mbox_csel;

	struct btcx_riscmem     main;
	struct bttv_buffer      *screen;    
	struct list_head        capture;    
	struct list_head        vcapture;   
	struct bttv_buffer_set  curr;       
	struct bttv_buffer      *cvbi;      
	int                     loop_irq;
	int                     new_input;

	unsigned long cap_ctl;
	unsigned long dma_on;
	struct timer_list timeout;
	struct bttv_suspend_state state;

	
	unsigned int errors;
	unsigned int framedrop;
	unsigned int irq_total;
	unsigned int irq_me;

	unsigned int users;
	struct bttv_fh init;

	
	struct work_struct request_module_wk;

	struct bttv_crop crop[2];

	__s32			vbi_end;

	__s32			crop_start;
};

static inline struct bttv *to_bttv(struct v4l2_device *v4l2_dev)
{
	return container_of(v4l2_dev, struct bttv, c.v4l2_dev);
}

#define BTTV_MAX 32
extern unsigned int bttv_num;
extern struct bttv *bttvs[BTTV_MAX];

static inline unsigned int bttv_muxsel(const struct bttv *btv,
				       unsigned int input)
{
	return (bttv_tvcards[btv->c.type].muxsel >> (input * 2)) & 3;
}

#endif

#define btwrite(dat,adr)    writel((dat), btv->bt848_mmio+(adr))
#define btread(adr)         readl(btv->bt848_mmio+(adr))

#define btand(dat,adr)      btwrite((dat) & btread(adr), adr)
#define btor(dat,adr)       btwrite((dat) | btread(adr), adr)
#define btaor(dat,mask,adr) btwrite((dat) | ((mask) & btread(adr)), adr)

#endif 

