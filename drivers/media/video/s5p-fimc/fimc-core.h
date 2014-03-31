/*
 * Copyright (C) 2010 - 2011 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef FIMC_CORE_H_
#define FIMC_CORE_H_


#include <linux/platform_device.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/types.h>
#include <linux/videodev2.h>
#include <linux/io.h>

#include <media/media-entity.h>
#include <media/videobuf2-core.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-mem2mem.h>
#include <media/v4l2-mediabus.h>
#include <media/s5p_fimc.h>

#include "regs-fimc.h"

#define err(fmt, args...) \
	printk(KERN_ERR "%s:%d: " fmt "\n", __func__, __LINE__, ##args)

#define dbg(fmt, args...) \
	pr_debug("%s:%d: " fmt "\n", __func__, __LINE__, ##args)

#define FIMC_SHUTDOWN_TIMEOUT	((100*HZ)/1000)
#define MAX_FIMC_CLOCKS		2
#define FIMC_MODULE_NAME	"s5p-fimc"
#define FIMC_MAX_DEVS		4
#define FIMC_MAX_OUT_BUFS	4
#define SCALER_MAX_HRATIO	64
#define SCALER_MAX_VRATIO	64
#define DMA_MIN_SIZE		8
#define FIMC_CAMIF_MAX_HEIGHT	0x2000

enum {
	CLK_BUS,
	CLK_GATE,
};

enum fimc_dev_flags {
	ST_LPM,
	
	ST_M2M_RUN,
	ST_M2M_PEND,
	ST_M2M_SUSPENDING,
	ST_M2M_SUSPENDED,
	
	ST_CAPT_PEND,
	ST_CAPT_RUN,
	ST_CAPT_STREAM,
	ST_CAPT_ISP_STREAM,
	ST_CAPT_SUSPENDED,
	ST_CAPT_SHUT,
	ST_CAPT_BUSY,
	ST_CAPT_APPLY_CFG,
	ST_CAPT_JPEG,
};

#define fimc_m2m_active(dev) test_bit(ST_M2M_RUN, &(dev)->state)
#define fimc_m2m_pending(dev) test_bit(ST_M2M_PEND, &(dev)->state)

#define fimc_capture_running(dev) test_bit(ST_CAPT_RUN, &(dev)->state)
#define fimc_capture_pending(dev) test_bit(ST_CAPT_PEND, &(dev)->state)
#define fimc_capture_busy(dev) test_bit(ST_CAPT_BUSY, &(dev)->state)

enum fimc_datapath {
	FIMC_CAMERA,
	FIMC_DMA,
	FIMC_LCDFIFO,
	FIMC_WRITEBACK
};

enum fimc_color_fmt {
	S5P_FIMC_RGB444 = 0x10,
	S5P_FIMC_RGB555,
	S5P_FIMC_RGB565,
	S5P_FIMC_RGB666,
	S5P_FIMC_RGB888,
	S5P_FIMC_RGB30_LOCAL,
	S5P_FIMC_YCBCR420 = 0x20,
	S5P_FIMC_YCBYCR422,
	S5P_FIMC_YCRYCB422,
	S5P_FIMC_CBYCRY422,
	S5P_FIMC_CRYCBY422,
	S5P_FIMC_YCBCR444_LOCAL,
	S5P_FIMC_JPEG = 0x40,
};

#define fimc_fmt_is_rgb(x) (!!((x) & 0x10))
#define fimc_fmt_is_jpeg(x) (!!((x) & 0x40))

#define IS_M2M(__strt) ((__strt) == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE || \
			__strt == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE)

#define	S5P_FIMC_LSB_CRCB	S5P_CIOCTRL_ORDER422_2P_LSB_CRCB

#define	S5P_FIMC_EFFECT_ORIGINAL	S5P_CIIMGEFF_FIN_BYPASS
#define	S5P_FIMC_EFFECT_ARBITRARY	S5P_CIIMGEFF_FIN_ARBITRARY
#define	S5P_FIMC_EFFECT_NEGATIVE	S5P_CIIMGEFF_FIN_NEGATIVE
#define	S5P_FIMC_EFFECT_ARTFREEZE	S5P_CIIMGEFF_FIN_ARTFREEZE
#define	S5P_FIMC_EFFECT_EMBOSSING	S5P_CIIMGEFF_FIN_EMBOSSING
#define	S5P_FIMC_EFFECT_SIKHOUETTE	S5P_CIIMGEFF_FIN_SILHOUETTE

#define	FIMC_PARAMS		(1 << 0)
#define	FIMC_SRC_ADDR		(1 << 1)
#define	FIMC_DST_ADDR		(1 << 2)
#define	FIMC_SRC_FMT		(1 << 3)
#define	FIMC_DST_FMT		(1 << 4)
#define	FIMC_DST_CROP		(1 << 5)
#define	FIMC_CTX_M2M		(1 << 16)
#define	FIMC_CTX_CAP		(1 << 17)
#define	FIMC_CTX_SHUT		(1 << 18)

#define	FIMC_IN_DMA_ACCESS_TILED	(1 << 0)
#define	FIMC_IN_DMA_ACCESS_LINEAR	(0 << 0)
#define	FIMC_OUT_DMA_ACCESS_TILED	(1 << 1)
#define	FIMC_OUT_DMA_ACCESS_LINEAR	(0 << 1)
#define	FIMC_SCAN_MODE_PROGRESSIVE	(0 << 2)
#define	FIMC_SCAN_MODE_INTERLACED	(1 << 2)
#define	FIMC_COLOR_RANGE_WIDE		(0 << 3)
#define	FIMC_COLOR_RANGE_NARROW		(1 << 3)

struct fimc_fmt {
	enum v4l2_mbus_pixelcode mbus_code;
	char	*name;
	u32	fourcc;
	u32	color;
	u16	memplanes;
	u16	colplanes;
	u8	depth[VIDEO_MAX_PLANES];
	u16	flags;
#define FMT_FLAGS_CAM		(1 << 0)
#define FMT_FLAGS_M2M_IN	(1 << 1)
#define FMT_FLAGS_M2M_OUT	(1 << 2)
#define FMT_FLAGS_M2M		(1 << 1 | 1 << 2)
#define FMT_HAS_ALPHA		(1 << 3)
};

struct fimc_dma_offset {
	int	y_h;
	int	y_v;
	int	cb_h;
	int	cb_v;
	int	cr_h;
	int	cr_v;
};

struct fimc_effect {
	u32	type;
	u8	pat_cb;
	u8	pat_cr;
};

struct fimc_scaler {
	unsigned int scaleup_h:1;
	unsigned int scaleup_v:1;
	unsigned int copy_mode:1;
	unsigned int enabled:1;
	u32	hfactor;
	u32	vfactor;
	u32	pre_hratio;
	u32	pre_vratio;
	u32	pre_dst_width;
	u32	pre_dst_height;
	u32	main_hratio;
	u32	main_vratio;
	u32	real_width;
	u32	real_height;
};

struct fimc_addr {
	u32	y;
	u32	cb;
	u32	cr;
};

struct fimc_vid_buffer {
	struct vb2_buffer	vb;
	struct list_head	list;
	struct fimc_addr	paddr;
	int			index;
};

struct fimc_frame {
	u32	f_width;
	u32	f_height;
	u32	o_width;
	u32	o_height;
	u32	offs_h;
	u32	offs_v;
	u32	width;
	u32	height;
	unsigned long		payload[VIDEO_MAX_PLANES];
	struct fimc_addr	paddr;
	struct fimc_dma_offset	dma_offset;
	struct fimc_fmt		*fmt;
	u8			alpha;
};

struct fimc_m2m_device {
	struct video_device	*vfd;
	struct v4l2_m2m_dev	*m2m_dev;
	struct fimc_ctx		*ctx;
	int			refcnt;
};

#define FIMC_SD_PAD_SINK	0
#define FIMC_SD_PAD_SOURCE	1
#define FIMC_SD_PADS_NUM	2

struct fimc_vid_cap {
	struct fimc_ctx			*ctx;
	struct vb2_alloc_ctx		*alloc_ctx;
	struct video_device		*vfd;
	struct v4l2_subdev		*subdev;
	struct media_pad		vd_pad;
	struct v4l2_mbus_framefmt	mf;
	struct media_pad		sd_pads[FIMC_SD_PADS_NUM];
	struct list_head		pending_buf_q;
	struct list_head		active_buf_q;
	struct vb2_queue		vbq;
	int				active_buf_cnt;
	int				buf_index;
	unsigned int			frame_count;
	unsigned int			reqbufs_count;
	int				input_index;
	int				refcnt;
	u32				input;
	bool				user_subdev_api;
};

struct fimc_pix_limit {
	u16 scaler_en_w;
	u16 scaler_dis_w;
	u16 in_rot_en_h;
	u16 in_rot_dis_w;
	u16 out_rot_en_w;
	u16 out_rot_dis_w;
};

struct samsung_fimc_variant {
	unsigned int	pix_hoff:1;
	unsigned int	has_inp_rot:1;
	unsigned int	has_out_rot:1;
	unsigned int	has_cistatus2:1;
	unsigned int	has_mainscaler_ext:1;
	unsigned int	has_cam_if:1;
	unsigned int	has_alpha:1;
	struct fimc_pix_limit *pix_limit;
	u16		min_inp_pixsize;
	u16		min_out_pixsize;
	u16		hor_offs_align;
	u16		min_vsize_align;
	u16		out_buf_count;
};

struct samsung_fimc_driverdata {
	struct samsung_fimc_variant *variant[FIMC_MAX_DEVS];
	unsigned long	lclk_frequency;
	int		num_entities;
};

struct fimc_pipeline {
	struct media_pipeline *pipe;
	struct v4l2_subdev *sensor;
	struct v4l2_subdev *csis;
};

struct fimc_ctx;

struct fimc_dev {
	spinlock_t			slock;
	struct mutex			lock;
	struct platform_device		*pdev;
	struct s5p_platform_fimc	*pdata;
	struct samsung_fimc_variant	*variant;
	u16				id;
	u16				num_clocks;
	struct clk			*clock[MAX_FIMC_CLOCKS];
	void __iomem			*regs;
	int				irq;
	wait_queue_head_t		irq_queue;
	struct v4l2_device		*v4l2_dev;
	struct fimc_m2m_device		m2m;
	struct fimc_vid_cap		vid_cap;
	unsigned long			state;
	struct vb2_alloc_ctx		*alloc_ctx;
	struct fimc_pipeline		pipeline;
};

struct fimc_ctx {
	spinlock_t		slock;
	struct fimc_frame	s_frame;
	struct fimc_frame	d_frame;
	u32			out_order_1p;
	u32			out_order_2p;
	u32			in_order_1p;
	u32			in_order_2p;
	enum fimc_datapath	in_path;
	enum fimc_datapath	out_path;
	struct fimc_scaler	scaler;
	struct fimc_effect	effect;
	int			rotation;
	unsigned int		hflip:1;
	unsigned int		vflip:1;
	u32			flags;
	u32			state;
	struct fimc_dev		*fimc_dev;
	struct v4l2_m2m_ctx	*m2m_ctx;
	struct v4l2_fh		fh;
	struct v4l2_ctrl_handler ctrl_handler;
	struct v4l2_ctrl	*ctrl_rotate;
	struct v4l2_ctrl	*ctrl_hflip;
	struct v4l2_ctrl	*ctrl_vflip;
	struct v4l2_ctrl	*ctrl_alpha;
	bool			ctrls_rdy;
};

#define fh_to_ctx(__fh) container_of(__fh, struct fimc_ctx, fh)

static inline void set_frame_bounds(struct fimc_frame *f, u32 width, u32 height)
{
	f->o_width  = width;
	f->o_height = height;
	f->f_width  = width;
	f->f_height = height;
}

static inline void set_frame_crop(struct fimc_frame *f,
				  u32 left, u32 top, u32 width, u32 height)
{
	f->offs_h = left;
	f->offs_v = top;
	f->width  = width;
	f->height = height;
}

static inline u32 fimc_get_format_depth(struct fimc_fmt *ff)
{
	u32 i, depth = 0;

	if (ff != NULL)
		for (i = 0; i < ff->colplanes; i++)
			depth += ff->depth[i];
	return depth;
}

static inline bool fimc_capture_active(struct fimc_dev *fimc)
{
	unsigned long flags;
	bool ret;

	spin_lock_irqsave(&fimc->slock, flags);
	ret = !!(fimc->state & (1 << ST_CAPT_RUN) ||
		 fimc->state & (1 << ST_CAPT_PEND));
	spin_unlock_irqrestore(&fimc->slock, flags);
	return ret;
}

static inline void fimc_ctx_state_lock_set(u32 state, struct fimc_ctx *ctx)
{
	unsigned long flags;

	spin_lock_irqsave(&ctx->slock, flags);
	ctx->state |= state;
	spin_unlock_irqrestore(&ctx->slock, flags);
}

static inline bool fimc_ctx_state_is_set(u32 mask, struct fimc_ctx *ctx)
{
	unsigned long flags;
	bool ret;

	spin_lock_irqsave(&ctx->slock, flags);
	ret = (ctx->state & mask) == mask;
	spin_unlock_irqrestore(&ctx->slock, flags);
	return ret;
}

static inline int tiled_fmt(struct fimc_fmt *fmt)
{
	return fmt->fourcc == V4L2_PIX_FMT_NV12MT;
}

static inline int fimc_get_alpha_mask(struct fimc_fmt *fmt)
{
	switch (fmt->color) {
	case S5P_FIMC_RGB444:	return 0x0f;
	case S5P_FIMC_RGB555:	return 0x01;
	case S5P_FIMC_RGB888:	return 0xff;
	default:		return 0;
	};
}

static inline void fimc_hw_clear_irq(struct fimc_dev *dev)
{
	u32 cfg = readl(dev->regs + S5P_CIGCTRL);
	cfg |= S5P_CIGCTRL_IRQ_CLR;
	writel(cfg, dev->regs + S5P_CIGCTRL);
}

static inline void fimc_hw_enable_scaler(struct fimc_dev *dev, bool on)
{
	u32 cfg = readl(dev->regs + S5P_CISCCTRL);
	if (on)
		cfg |= S5P_CISCCTRL_SCALERSTART;
	else
		cfg &= ~S5P_CISCCTRL_SCALERSTART;
	writel(cfg, dev->regs + S5P_CISCCTRL);
}

static inline void fimc_hw_activate_input_dma(struct fimc_dev *dev, bool on)
{
	u32 cfg = readl(dev->regs + S5P_MSCTRL);
	if (on)
		cfg |= S5P_MSCTRL_ENVID;
	else
		cfg &= ~S5P_MSCTRL_ENVID;
	writel(cfg, dev->regs + S5P_MSCTRL);
}

static inline void fimc_hw_dis_capture(struct fimc_dev *dev)
{
	u32 cfg = readl(dev->regs + S5P_CIIMGCPT);
	cfg &= ~(S5P_CIIMGCPT_IMGCPTEN | S5P_CIIMGCPT_IMGCPTEN_SC);
	writel(cfg, dev->regs + S5P_CIIMGCPT);
}

static inline void fimc_hw_set_dma_seq(struct fimc_dev *dev, u32 mask)
{
	writel(mask, dev->regs + S5P_CIFCNTSEQ);
}

static inline struct fimc_frame *ctx_get_frame(struct fimc_ctx *ctx,
					       enum v4l2_buf_type type)
{
	struct fimc_frame *frame;

	if (V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE == type) {
		if (fimc_ctx_state_is_set(FIMC_CTX_M2M, ctx))
			frame = &ctx->s_frame;
		else
			return ERR_PTR(-EINVAL);
	} else if (V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE == type) {
		frame = &ctx->d_frame;
	} else {
		v4l2_err(ctx->fimc_dev->v4l2_dev,
			"Wrong buffer/video queue type (%d)\n", type);
		return ERR_PTR(-EINVAL);
	}

	return frame;
}

/* Return an index to the buffer actually being written. */
static inline u32 fimc_hw_get_frame_index(struct fimc_dev *dev)
{
	u32 reg;

	if (dev->variant->has_cistatus2) {
		reg = readl(dev->regs + S5P_CISTATUS2) & 0x3F;
		return reg > 0 ? --reg : reg;
	} else {
		reg = readl(dev->regs + S5P_CISTATUS);
		return (reg & S5P_CISTATUS_FRAMECNT_MASK) >>
			S5P_CISTATUS_FRAMECNT_SHIFT;
	}
}

void fimc_hw_reset(struct fimc_dev *fimc);
void fimc_hw_set_rotation(struct fimc_ctx *ctx);
void fimc_hw_set_target_format(struct fimc_ctx *ctx);
void fimc_hw_set_out_dma(struct fimc_ctx *ctx);
void fimc_hw_en_lastirq(struct fimc_dev *fimc, int enable);
void fimc_hw_en_irq(struct fimc_dev *fimc, int enable);
void fimc_hw_set_prescaler(struct fimc_ctx *ctx);
void fimc_hw_set_mainscaler(struct fimc_ctx *ctx);
void fimc_hw_en_capture(struct fimc_ctx *ctx);
void fimc_hw_set_effect(struct fimc_ctx *ctx, bool active);
void fimc_hw_set_rgb_alpha(struct fimc_ctx *ctx);
void fimc_hw_set_in_dma(struct fimc_ctx *ctx);
void fimc_hw_set_input_path(struct fimc_ctx *ctx);
void fimc_hw_set_output_path(struct fimc_ctx *ctx);
void fimc_hw_set_input_addr(struct fimc_dev *fimc, struct fimc_addr *paddr);
void fimc_hw_set_output_addr(struct fimc_dev *fimc, struct fimc_addr *paddr,
			     int index);
int fimc_hw_set_camera_source(struct fimc_dev *fimc,
			      struct s5p_fimc_isp_info *cam);
int fimc_hw_set_camera_offset(struct fimc_dev *fimc, struct fimc_frame *f);
int fimc_hw_set_camera_polarity(struct fimc_dev *fimc,
				struct s5p_fimc_isp_info *cam);
int fimc_hw_set_camera_type(struct fimc_dev *fimc,
			    struct s5p_fimc_isp_info *cam);

int fimc_vidioc_enum_fmt_mplane(struct file *file, void *priv,
				struct v4l2_fmtdesc *f);
int fimc_ctrls_create(struct fimc_ctx *ctx);
void fimc_ctrls_delete(struct fimc_ctx *ctx);
void fimc_ctrls_activate(struct fimc_ctx *ctx, bool active);
void fimc_alpha_ctrl_update(struct fimc_ctx *ctx);
int fimc_fill_format(struct fimc_frame *frame, struct v4l2_format *f);
void fimc_adjust_mplane_format(struct fimc_fmt *fmt, u32 width, u32 height,
			       struct v4l2_pix_format_mplane *pix);
struct fimc_fmt *fimc_find_format(const u32 *pixelformat, const u32 *mbus_code,
				  unsigned int mask, int index);

int fimc_check_scaler_ratio(struct fimc_ctx *ctx, int sw, int sh,
			    int dw, int dh, int rotation);
int fimc_set_scaler_info(struct fimc_ctx *ctx);
int fimc_prepare_config(struct fimc_ctx *ctx, u32 flags);
int fimc_prepare_addr(struct fimc_ctx *ctx, struct vb2_buffer *vb,
		      struct fimc_frame *frame, struct fimc_addr *paddr);
void fimc_prepare_dma_offset(struct fimc_ctx *ctx, struct fimc_frame *f);
void fimc_set_yuv_order(struct fimc_ctx *ctx);
void fimc_fill_frame(struct fimc_frame *frame, struct v4l2_format *f);
void fimc_capture_irq_handler(struct fimc_dev *fimc, bool done);

int fimc_register_m2m_device(struct fimc_dev *fimc,
			     struct v4l2_device *v4l2_dev);
void fimc_unregister_m2m_device(struct fimc_dev *fimc);
int fimc_register_driver(void);
void fimc_unregister_driver(void);

int fimc_register_capture_device(struct fimc_dev *fimc,
				 struct v4l2_device *v4l2_dev);
void fimc_unregister_capture_device(struct fimc_dev *fimc);
int fimc_capture_ctrls_create(struct fimc_dev *fimc);
int fimc_vid_cap_buf_queue(struct fimc_dev *fimc,
			     struct fimc_vid_buffer *fimc_vb);
void fimc_sensor_notify(struct v4l2_subdev *sd, unsigned int notification,
			void *arg);
int fimc_capture_suspend(struct fimc_dev *fimc);
int fimc_capture_resume(struct fimc_dev *fimc);
int fimc_capture_config_update(struct fimc_ctx *ctx);

static inline void fimc_activate_capture(struct fimc_ctx *ctx)
{
	fimc_hw_enable_scaler(ctx->fimc_dev, ctx->scaler.enabled);
	fimc_hw_en_capture(ctx);
}

static inline void fimc_deactivate_capture(struct fimc_dev *fimc)
{
	fimc_hw_en_lastirq(fimc, true);
	fimc_hw_dis_capture(fimc);
	fimc_hw_enable_scaler(fimc, false);
	fimc_hw_en_lastirq(fimc, false);
}


static inline void fimc_active_queue_add(struct fimc_vid_cap *vid_cap,
					 struct fimc_vid_buffer *buf)
{
	list_add_tail(&buf->list, &vid_cap->active_buf_q);
	vid_cap->active_buf_cnt++;
}

static inline struct fimc_vid_buffer *fimc_active_queue_pop(
				    struct fimc_vid_cap *vid_cap)
{
	struct fimc_vid_buffer *buf;
	buf = list_entry(vid_cap->active_buf_q.next,
			 struct fimc_vid_buffer, list);
	list_del(&buf->list);
	vid_cap->active_buf_cnt--;
	return buf;
}

static inline void fimc_pending_queue_add(struct fimc_vid_cap *vid_cap,
					  struct fimc_vid_buffer *buf)
{
	list_add_tail(&buf->list, &vid_cap->pending_buf_q);
}

static inline struct fimc_vid_buffer *fimc_pending_queue_pop(
				     struct fimc_vid_cap *vid_cap)
{
	struct fimc_vid_buffer *buf;
	buf = list_entry(vid_cap->pending_buf_q.next,
			struct fimc_vid_buffer, list);
	list_del(&buf->list);
	return buf;
}

#endif 
