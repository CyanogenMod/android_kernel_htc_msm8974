/*
 * The Marvell camera core.  This device appears in a number of settings,
 * so it needs platform-specific support outside of the core.
 *
 * Copyright 2011 Jonathan Corbet corbet@lwn.net
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/wait.h>
#include <linux/list.h>
#include <linux/dma-mapping.h>
#include <linux/delay.h>
#include <linux/vmalloc.h>
#include <linux/io.h>
#include <linux/videodev2.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-chip-ident.h>
#include <media/ov7670.h>
#include <media/videobuf2-vmalloc.h>
#include <media/videobuf2-dma-contig.h>
#include <media/videobuf2-dma-sg.h>

#include "mcam-core.h"

static int frames;
static int singles;
static int delivered;

#ifdef MCAM_MODE_VMALLOC

static bool alloc_bufs_at_read;
module_param(alloc_bufs_at_read, bool, 0444);
MODULE_PARM_DESC(alloc_bufs_at_read,
		"Non-zero value causes DMA buffers to be allocated when the "
		"video capture device is read, rather than at module load "
		"time.  This saves memory, but decreases the chances of "
		"successfully getting those buffers.  This parameter is "
		"only used in the vmalloc buffer mode");

static int n_dma_bufs = 3;
module_param(n_dma_bufs, uint, 0644);
MODULE_PARM_DESC(n_dma_bufs,
		"The number of DMA buffers to allocate.  Can be either two "
		"(saves memory, makes timing tighter) or three.");

static int dma_buf_size = VGA_WIDTH * VGA_HEIGHT * 2;  
module_param(dma_buf_size, uint, 0444);
MODULE_PARM_DESC(dma_buf_size,
		"The size of the allocated DMA buffers.  If actual operating "
		"parameters require larger buffers, an attempt to reallocate "
		"will be made.");
#else 
static const bool alloc_bufs_at_read = 0;
static const int n_dma_bufs = 3;  
#endif 

static bool flip;
module_param(flip, bool, 0444);
MODULE_PARM_DESC(flip,
		"If set, the sensor will be instructed to flip the image "
		"vertically.");

static int buffer_mode = -1;
module_param(buffer_mode, int, 0444);
MODULE_PARM_DESC(buffer_mode,
		"Set the buffer mode to be used; default is to go with what "
		"the platform driver asks for.  Set to 0 for vmalloc, 1 for "
		"DMA contiguous.");

#define CF_BUF0_VALID	 0	
#define CF_BUF1_VALID	 1
#define CF_BUF2_VALID	 2
#define CF_DMA_ACTIVE	 3	
#define CF_CONFIG_NEEDED 4	
#define CF_SINGLE_BUFFER 5	
#define CF_SG_RESTART	 6	

#define sensor_call(cam, o, f, args...) \
	v4l2_subdev_call(cam->sensor, o, f, ##args)

static struct mcam_format_struct {
	__u8 *desc;
	__u32 pixelformat;
	int bpp;   
	enum v4l2_mbus_pixelcode mbus_code;
} mcam_formats[] = {
	{
		.desc		= "YUYV 4:2:2",
		.pixelformat	= V4L2_PIX_FMT_YUYV,
		.mbus_code	= V4L2_MBUS_FMT_YUYV8_2X8,
		.bpp		= 2,
	},
	{
		.desc		= "RGB 444",
		.pixelformat	= V4L2_PIX_FMT_RGB444,
		.mbus_code	= V4L2_MBUS_FMT_RGB444_2X8_PADHI_LE,
		.bpp		= 2,
	},
	{
		.desc		= "RGB 565",
		.pixelformat	= V4L2_PIX_FMT_RGB565,
		.mbus_code	= V4L2_MBUS_FMT_RGB565_2X8_LE,
		.bpp		= 2,
	},
	{
		.desc		= "Raw RGB Bayer",
		.pixelformat	= V4L2_PIX_FMT_SBGGR8,
		.mbus_code	= V4L2_MBUS_FMT_SBGGR8_1X8,
		.bpp		= 1
	},
};
#define N_MCAM_FMTS ARRAY_SIZE(mcam_formats)

static struct mcam_format_struct *mcam_find_format(u32 pixelformat)
{
	unsigned i;

	for (i = 0; i < N_MCAM_FMTS; i++)
		if (mcam_formats[i].pixelformat == pixelformat)
			return mcam_formats + i;
	
	return mcam_formats;
}

static const struct v4l2_pix_format mcam_def_pix_format = {
	.width		= VGA_WIDTH,
	.height		= VGA_HEIGHT,
	.pixelformat	= V4L2_PIX_FMT_YUYV,
	.field		= V4L2_FIELD_NONE,
	.bytesperline	= VGA_WIDTH*2,
	.sizeimage	= VGA_WIDTH*VGA_HEIGHT*2,
};

static const enum v4l2_mbus_pixelcode mcam_def_mbus_code =
					V4L2_MBUS_FMT_YUYV8_2X8;


struct mcam_dma_desc {
	u32 dma_addr;
	u32 segment_len;
};

struct mcam_vb_buffer {
	struct vb2_buffer vb_buf;
	struct list_head queue;
	struct mcam_dma_desc *dma_desc;	
	dma_addr_t dma_desc_pa;		
	int dma_desc_nent;		
};

static inline struct mcam_vb_buffer *vb_to_mvb(struct vb2_buffer *vb)
{
	return container_of(vb, struct mcam_vb_buffer, vb_buf);
}

static void mcam_buffer_done(struct mcam_camera *cam, int frame,
		struct vb2_buffer *vbuf)
{
	vbuf->v4l2_buf.bytesused = cam->pix_format.sizeimage;
	vbuf->v4l2_buf.sequence = cam->buf_seq[frame];
	vb2_set_plane_payload(vbuf, 0, cam->pix_format.sizeimage);
	vb2_buffer_done(vbuf, VB2_BUF_STATE_DONE);
}



#define cam_err(cam, fmt, arg...) \
	dev_err((cam)->dev, fmt, ##arg);
#define cam_warn(cam, fmt, arg...) \
	dev_warn((cam)->dev, fmt, ##arg);
#define cam_dbg(cam, fmt, arg...) \
	dev_dbg((cam)->dev, fmt, ##arg);


static void mcam_reset_buffers(struct mcam_camera *cam)
{
	int i;

	cam->next_buf = -1;
	for (i = 0; i < cam->nbufs; i++)
		clear_bit(i, &cam->flags);
}

static inline int mcam_needs_config(struct mcam_camera *cam)
{
	return test_bit(CF_CONFIG_NEEDED, &cam->flags);
}

static void mcam_set_config_needed(struct mcam_camera *cam, int needed)
{
	if (needed)
		set_bit(CF_CONFIG_NEEDED, &cam->flags);
	else
		clear_bit(CF_CONFIG_NEEDED, &cam->flags);
}

static void mcam_ctlr_start(struct mcam_camera *cam)
{
	mcam_reg_set_bit(cam, REG_CTRL0, C0_ENABLE);
}

static void mcam_ctlr_stop(struct mcam_camera *cam)
{
	mcam_reg_clear_bit(cam, REG_CTRL0, C0_ENABLE);
}


#ifdef MCAM_MODE_VMALLOC

static int mcam_alloc_dma_bufs(struct mcam_camera *cam, int loadtime)
{
	int i;

	mcam_set_config_needed(cam, 1);
	if (loadtime)
		cam->dma_buf_size = dma_buf_size;
	else
		cam->dma_buf_size = cam->pix_format.sizeimage;
	if (n_dma_bufs > 3)
		n_dma_bufs = 3;

	cam->nbufs = 0;
	for (i = 0; i < n_dma_bufs; i++) {
		cam->dma_bufs[i] = dma_alloc_coherent(cam->dev,
				cam->dma_buf_size, cam->dma_handles + i,
				GFP_KERNEL);
		if (cam->dma_bufs[i] == NULL) {
			cam_warn(cam, "Failed to allocate DMA buffer\n");
			break;
		}
		(cam->nbufs)++;
	}

	switch (cam->nbufs) {
	case 1:
		dma_free_coherent(cam->dev, cam->dma_buf_size,
				cam->dma_bufs[0], cam->dma_handles[0]);
		cam->nbufs = 0;
	case 0:
		cam_err(cam, "Insufficient DMA buffers, cannot operate\n");
		return -ENOMEM;

	case 2:
		if (n_dma_bufs > 2)
			cam_warn(cam, "Will limp along with only 2 buffers\n");
		break;
	}
	return 0;
}

static void mcam_free_dma_bufs(struct mcam_camera *cam)
{
	int i;

	for (i = 0; i < cam->nbufs; i++) {
		dma_free_coherent(cam->dev, cam->dma_buf_size,
				cam->dma_bufs[i], cam->dma_handles[i]);
		cam->dma_bufs[i] = NULL;
	}
	cam->nbufs = 0;
}


static void mcam_ctlr_dma_vmalloc(struct mcam_camera *cam)
{
	mcam_reg_write(cam, REG_Y0BAR, cam->dma_handles[0]);
	mcam_reg_write(cam, REG_Y1BAR, cam->dma_handles[1]);
	if (cam->nbufs > 2) {
		mcam_reg_write(cam, REG_Y2BAR, cam->dma_handles[2]);
		mcam_reg_clear_bit(cam, REG_CTRL1, C1_TWOBUFS);
	} else
		mcam_reg_set_bit(cam, REG_CTRL1, C1_TWOBUFS);
	if (cam->chip_id == V4L2_IDENT_CAFE)
		mcam_reg_write(cam, REG_UBAR, 0); 
}

static void mcam_frame_tasklet(unsigned long data)
{
	struct mcam_camera *cam = (struct mcam_camera *) data;
	int i;
	unsigned long flags;
	struct mcam_vb_buffer *buf;

	spin_lock_irqsave(&cam->dev_lock, flags);
	for (i = 0; i < cam->nbufs; i++) {
		int bufno = cam->next_buf;

		if (cam->state != S_STREAMING || bufno < 0)
			break;  
		if (++(cam->next_buf) >= cam->nbufs)
			cam->next_buf = 0;
		if (!test_bit(bufno, &cam->flags))
			continue;
		if (list_empty(&cam->buffers)) {
			singles++;
			break;  
		}
		delivered++;
		clear_bit(bufno, &cam->flags);
		buf = list_first_entry(&cam->buffers, struct mcam_vb_buffer,
				queue);
		list_del_init(&buf->queue);
		spin_unlock_irqrestore(&cam->dev_lock, flags);
		memcpy(vb2_plane_vaddr(&buf->vb_buf, 0), cam->dma_bufs[bufno],
				cam->pix_format.sizeimage);
		mcam_buffer_done(cam, bufno, &buf->vb_buf);
		spin_lock_irqsave(&cam->dev_lock, flags);
	}
	spin_unlock_irqrestore(&cam->dev_lock, flags);
}


static int mcam_check_dma_buffers(struct mcam_camera *cam)
{
	if (cam->nbufs > 0 && cam->dma_buf_size < cam->pix_format.sizeimage)
			mcam_free_dma_bufs(cam);
	if (cam->nbufs == 0)
		return mcam_alloc_dma_bufs(cam, 0);
	return 0;
}

static void mcam_vmalloc_done(struct mcam_camera *cam, int frame)
{
	tasklet_schedule(&cam->s_tasklet);
}

#else 

static inline int mcam_alloc_dma_bufs(struct mcam_camera *cam, int loadtime)
{
	return 0;
}

static inline void mcam_free_dma_bufs(struct mcam_camera *cam)
{
	return;
}

static inline int mcam_check_dma_buffers(struct mcam_camera *cam)
{
	return 0;
}



#endif 


#ifdef MCAM_MODE_DMA_CONTIG
static void mcam_set_contig_buffer(struct mcam_camera *cam, int frame)
{
	struct mcam_vb_buffer *buf;
	if (list_empty(&cam->buffers)) {
		buf = cam->vb_bufs[frame ^ 0x1];
		cam->vb_bufs[frame] = buf;
		mcam_reg_write(cam, frame == 0 ? REG_Y0BAR : REG_Y1BAR,
				vb2_dma_contig_plane_dma_addr(&buf->vb_buf, 0));
		set_bit(CF_SINGLE_BUFFER, &cam->flags);
		singles++;
		return;
	}
	buf = list_first_entry(&cam->buffers, struct mcam_vb_buffer, queue);
	list_del_init(&buf->queue);
	mcam_reg_write(cam, frame == 0 ? REG_Y0BAR : REG_Y1BAR,
			vb2_dma_contig_plane_dma_addr(&buf->vb_buf, 0));
	cam->vb_bufs[frame] = buf;
	clear_bit(CF_SINGLE_BUFFER, &cam->flags);
}

static void mcam_ctlr_dma_contig(struct mcam_camera *cam)
{
	mcam_reg_set_bit(cam, REG_CTRL1, C1_TWOBUFS);
	cam->nbufs = 2;
	mcam_set_contig_buffer(cam, 0);
	mcam_set_contig_buffer(cam, 1);
}

static void mcam_dma_contig_done(struct mcam_camera *cam, int frame)
{
	struct mcam_vb_buffer *buf = cam->vb_bufs[frame];

	if (!test_bit(CF_SINGLE_BUFFER, &cam->flags)) {
		delivered++;
		mcam_buffer_done(cam, frame, &buf->vb_buf);
	}
	mcam_set_contig_buffer(cam, frame);
}

#endif 

#ifdef MCAM_MODE_DMA_SG

static void mcam_sg_next_buffer(struct mcam_camera *cam)
{
	struct mcam_vb_buffer *buf;

	buf = list_first_entry(&cam->buffers, struct mcam_vb_buffer, queue);
	list_del_init(&buf->queue);
	mcam_reg_clear_bit(cam, REG_CTRL1, C1_DESC_ENA);
	mcam_reg_write(cam, REG_DMA_DESC_Y, buf->dma_desc_pa);
	mcam_reg_write(cam, REG_DESC_LEN_Y,
			buf->dma_desc_nent*sizeof(struct mcam_dma_desc));
	mcam_reg_write(cam, REG_DESC_LEN_U, 0);
	mcam_reg_write(cam, REG_DESC_LEN_V, 0);
	mcam_reg_set_bit(cam, REG_CTRL1, C1_DESC_ENA);
	cam->vb_bufs[0] = buf;
}

static void mcam_ctlr_dma_sg(struct mcam_camera *cam)
{
	if (list_empty(&cam->buffers)) {
		set_bit(CF_SG_RESTART, &cam->flags);
		return;
	}

	mcam_reg_clear_bit(cam, REG_CTRL1, C1_DESC_3WORD);
	mcam_sg_next_buffer(cam);
	cam->nbufs = 3;
}


static void mcam_dma_sg_done(struct mcam_camera *cam, int frame)
{
	struct mcam_vb_buffer *buf = cam->vb_bufs[0];

	if (cam->state != S_STREAMING)
		return;
	if (!list_empty(&cam->buffers)) {
		mcam_sg_next_buffer(cam);
		mcam_ctlr_start(cam);
	} else {
		set_bit(CF_SG_RESTART, &cam->flags);
		singles++;
		cam->vb_bufs[0] = NULL;
	}
	delivered++;
	mcam_buffer_done(cam, frame, &buf->vb_buf);
}


static void mcam_sg_restart(struct mcam_camera *cam)
{
	mcam_ctlr_dma_sg(cam);
	mcam_ctlr_start(cam);
	clear_bit(CF_SG_RESTART, &cam->flags);
}

#else 

static inline void mcam_sg_restart(struct mcam_camera *cam)
{
	return;
}

#endif 


static void mcam_ctlr_image(struct mcam_camera *cam)
{
	int imgsz;
	struct v4l2_pix_format *fmt = &cam->pix_format;

	imgsz = ((fmt->height << IMGSZ_V_SHIFT) & IMGSZ_V_MASK) |
		(fmt->bytesperline & IMGSZ_H_MASK);
	mcam_reg_write(cam, REG_IMGSIZE, imgsz);
	mcam_reg_write(cam, REG_IMGOFFSET, 0);
	
	mcam_reg_write_mask(cam, REG_IMGPITCH, fmt->bytesperline,
			IMGP_YP_MASK);
	switch (cam->pix_format.pixelformat) {
	case V4L2_PIX_FMT_YUYV:
	    mcam_reg_write_mask(cam, REG_CTRL0,
			    C0_DF_YUV|C0_YUV_PACKED|C0_YUVE_YUYV,
			    C0_DF_MASK);
	    break;

	case V4L2_PIX_FMT_RGB444:
	    mcam_reg_write_mask(cam, REG_CTRL0,
			    C0_DF_RGB|C0_RGBF_444|C0_RGB4_XRGB,
			    C0_DF_MASK);
		
	    break;

	case V4L2_PIX_FMT_RGB565:
	    mcam_reg_write_mask(cam, REG_CTRL0,
			    C0_DF_RGB|C0_RGBF_565|C0_RGB5_BGGR,
			    C0_DF_MASK);
	    break;

	default:
	    cam_err(cam, "Unknown format %x\n", cam->pix_format.pixelformat);
	    break;
	}
	mcam_reg_write_mask(cam, REG_CTRL0, C0_SIF_HVSYNC,
			C0_SIFM_MASK);
}


static int mcam_ctlr_configure(struct mcam_camera *cam)
{
	unsigned long flags;

	spin_lock_irqsave(&cam->dev_lock, flags);
	clear_bit(CF_SG_RESTART, &cam->flags);
	cam->dma_setup(cam);
	mcam_ctlr_image(cam);
	mcam_set_config_needed(cam, 0);
	spin_unlock_irqrestore(&cam->dev_lock, flags);
	return 0;
}

static void mcam_ctlr_irq_enable(struct mcam_camera *cam)
{
	mcam_reg_write(cam, REG_IRQSTAT, FRAMEIRQS);
	mcam_reg_set_bit(cam, REG_IRQMASK, FRAMEIRQS);
}

static void mcam_ctlr_irq_disable(struct mcam_camera *cam)
{
	mcam_reg_clear_bit(cam, REG_IRQMASK, FRAMEIRQS);
}



static void mcam_ctlr_init(struct mcam_camera *cam)
{
	unsigned long flags;

	spin_lock_irqsave(&cam->dev_lock, flags);
	mcam_reg_clear_bit(cam, REG_CTRL1, C1_PWRDWN);
	mcam_reg_clear_bit(cam, REG_CTRL0, C0_ENABLE);
	mcam_reg_write_mask(cam, REG_CLKCTRL, 2, CLK_DIV_MASK);
	spin_unlock_irqrestore(&cam->dev_lock, flags);
}


static void mcam_ctlr_stop_dma(struct mcam_camera *cam)
{
	unsigned long flags;

	spin_lock_irqsave(&cam->dev_lock, flags);
	clear_bit(CF_SG_RESTART, &cam->flags);
	mcam_ctlr_stop(cam);
	cam->state = S_IDLE;
	spin_unlock_irqrestore(&cam->dev_lock, flags);
	msleep(150);
	if (test_bit(CF_DMA_ACTIVE, &cam->flags))
		cam_err(cam, "Timeout waiting for DMA to end\n");
		
	spin_lock_irqsave(&cam->dev_lock, flags);
	mcam_ctlr_irq_disable(cam);
	spin_unlock_irqrestore(&cam->dev_lock, flags);
}

static void mcam_ctlr_power_up(struct mcam_camera *cam)
{
	unsigned long flags;

	spin_lock_irqsave(&cam->dev_lock, flags);
	cam->plat_power_up(cam);
	mcam_reg_clear_bit(cam, REG_CTRL1, C1_PWRDWN);
	spin_unlock_irqrestore(&cam->dev_lock, flags);
	msleep(5); 
}

static void mcam_ctlr_power_down(struct mcam_camera *cam)
{
	unsigned long flags;

	spin_lock_irqsave(&cam->dev_lock, flags);
	mcam_reg_set_bit(cam, REG_CTRL1, C1_PWRDWN);
	cam->plat_power_down(cam);
	spin_unlock_irqrestore(&cam->dev_lock, flags);
}


static int __mcam_cam_reset(struct mcam_camera *cam)
{
	return sensor_call(cam, core, reset, 0);
}

static int mcam_cam_init(struct mcam_camera *cam)
{
	struct v4l2_dbg_chip_ident chip;
	int ret;

	mutex_lock(&cam->s_mutex);
	if (cam->state != S_NOTREADY)
		cam_warn(cam, "Cam init with device in funky state %d",
				cam->state);
	ret = __mcam_cam_reset(cam);
	if (ret)
		goto out;
	chip.ident = V4L2_IDENT_NONE;
	chip.match.type = V4L2_CHIP_MATCH_I2C_ADDR;
	chip.match.addr = cam->sensor_addr;
	ret = sensor_call(cam, core, g_chip_ident, &chip);
	if (ret)
		goto out;
	cam->sensor_type = chip.ident;
	if (cam->sensor_type != V4L2_IDENT_OV7670) {
		cam_err(cam, "Unsupported sensor type 0x%x", cam->sensor_type);
		ret = -EINVAL;
		goto out;
	}
	ret = 0;
	cam->state = S_IDLE;
out:
	mcam_ctlr_power_down(cam);
	mutex_unlock(&cam->s_mutex);
	return ret;
}

static int mcam_cam_set_flip(struct mcam_camera *cam)
{
	struct v4l2_control ctrl;

	memset(&ctrl, 0, sizeof(ctrl));
	ctrl.id = V4L2_CID_VFLIP;
	ctrl.value = flip;
	return sensor_call(cam, core, s_ctrl, &ctrl);
}


static int mcam_cam_configure(struct mcam_camera *cam)
{
	struct v4l2_mbus_framefmt mbus_fmt;
	int ret;

	v4l2_fill_mbus_format(&mbus_fmt, &cam->pix_format, cam->mbus_code);
	ret = sensor_call(cam, core, init, 0);
	if (ret == 0)
		ret = sensor_call(cam, video, s_mbus_fmt, &mbus_fmt);
	ret += mcam_cam_set_flip(cam);
	return ret;
}

static int mcam_read_setup(struct mcam_camera *cam)
{
	int ret;
	unsigned long flags;

	if (cam->buffer_mode == B_vmalloc && cam->nbufs == 0 &&
			mcam_alloc_dma_bufs(cam, 0))
		return -ENOMEM;

	if (mcam_needs_config(cam)) {
		mcam_cam_configure(cam);
		ret = mcam_ctlr_configure(cam);
		if (ret)
			return ret;
	}

	spin_lock_irqsave(&cam->dev_lock, flags);
	clear_bit(CF_DMA_ACTIVE, &cam->flags);
	mcam_reset_buffers(cam);
	mcam_ctlr_irq_enable(cam);
	cam->state = S_STREAMING;
	if (!test_bit(CF_SG_RESTART, &cam->flags))
		mcam_ctlr_start(cam);
	spin_unlock_irqrestore(&cam->dev_lock, flags);
	return 0;
}


static int mcam_vb_queue_setup(struct vb2_queue *vq,
		const struct v4l2_format *fmt, unsigned int *nbufs,
		unsigned int *num_planes, unsigned int sizes[],
		void *alloc_ctxs[])
{
	struct mcam_camera *cam = vb2_get_drv_priv(vq);
	int minbufs = (cam->buffer_mode == B_DMA_contig) ? 3 : 2;

	sizes[0] = cam->pix_format.sizeimage;
	*num_planes = 1; 
	if (*nbufs < minbufs)
		*nbufs = minbufs;
	if (cam->buffer_mode == B_DMA_contig)
		alloc_ctxs[0] = cam->vb_alloc_ctx;
	return 0;
}


static void mcam_vb_buf_queue(struct vb2_buffer *vb)
{
	struct mcam_vb_buffer *mvb = vb_to_mvb(vb);
	struct mcam_camera *cam = vb2_get_drv_priv(vb->vb2_queue);
	unsigned long flags;
	int start;

	spin_lock_irqsave(&cam->dev_lock, flags);
	start = (cam->state == S_BUFWAIT) && !list_empty(&cam->buffers);
	list_add(&mvb->queue, &cam->buffers);
	if (cam->state == S_STREAMING && test_bit(CF_SG_RESTART, &cam->flags))
		mcam_sg_restart(cam);
	spin_unlock_irqrestore(&cam->dev_lock, flags);
	if (start)
		mcam_read_setup(cam);
}


static void mcam_vb_wait_prepare(struct vb2_queue *vq)
{
	struct mcam_camera *cam = vb2_get_drv_priv(vq);

	mutex_unlock(&cam->s_mutex);
}

static void mcam_vb_wait_finish(struct vb2_queue *vq)
{
	struct mcam_camera *cam = vb2_get_drv_priv(vq);

	mutex_lock(&cam->s_mutex);
}

static int mcam_vb_start_streaming(struct vb2_queue *vq, unsigned int count)
{
	struct mcam_camera *cam = vb2_get_drv_priv(vq);

	if (cam->state != S_IDLE) {
		INIT_LIST_HEAD(&cam->buffers);
		return -EINVAL;
	}
	cam->sequence = 0;
	if (cam->buffer_mode != B_vmalloc && list_empty(&cam->buffers)) {
		cam->state = S_BUFWAIT;
		return 0;
	}
	return mcam_read_setup(cam);
}

static int mcam_vb_stop_streaming(struct vb2_queue *vq)
{
	struct mcam_camera *cam = vb2_get_drv_priv(vq);
	unsigned long flags;

	if (cam->state == S_BUFWAIT) {
		
		cam->state = S_IDLE;
		return 0;
	}
	if (cam->state != S_STREAMING)
		return -EINVAL;
	mcam_ctlr_stop_dma(cam);
	spin_lock_irqsave(&cam->dev_lock, flags);
	INIT_LIST_HEAD(&cam->buffers);
	spin_unlock_irqrestore(&cam->dev_lock, flags);
	return 0;
}


static const struct vb2_ops mcam_vb2_ops = {
	.queue_setup		= mcam_vb_queue_setup,
	.buf_queue		= mcam_vb_buf_queue,
	.start_streaming	= mcam_vb_start_streaming,
	.stop_streaming		= mcam_vb_stop_streaming,
	.wait_prepare		= mcam_vb_wait_prepare,
	.wait_finish		= mcam_vb_wait_finish,
};


#ifdef MCAM_MODE_DMA_SG
static int mcam_vb_sg_buf_init(struct vb2_buffer *vb)
{
	struct mcam_vb_buffer *mvb = vb_to_mvb(vb);
	struct mcam_camera *cam = vb2_get_drv_priv(vb->vb2_queue);
	int ndesc = cam->pix_format.sizeimage/PAGE_SIZE + 1;

	mvb->dma_desc = dma_alloc_coherent(cam->dev,
			ndesc * sizeof(struct mcam_dma_desc),
			&mvb->dma_desc_pa, GFP_KERNEL);
	if (mvb->dma_desc == NULL) {
		cam_err(cam, "Unable to get DMA descriptor array\n");
		return -ENOMEM;
	}
	return 0;
}

static int mcam_vb_sg_buf_prepare(struct vb2_buffer *vb)
{
	struct mcam_vb_buffer *mvb = vb_to_mvb(vb);
	struct mcam_camera *cam = vb2_get_drv_priv(vb->vb2_queue);
	struct vb2_dma_sg_desc *sgd = vb2_dma_sg_plane_desc(vb, 0);
	struct mcam_dma_desc *desc = mvb->dma_desc;
	struct scatterlist *sg;
	int i;

	mvb->dma_desc_nent = dma_map_sg(cam->dev, sgd->sglist, sgd->num_pages,
			DMA_FROM_DEVICE);
	if (mvb->dma_desc_nent <= 0)
		return -EIO;  
	for_each_sg(sgd->sglist, sg, mvb->dma_desc_nent, i) {
		desc->dma_addr = sg_dma_address(sg);
		desc->segment_len = sg_dma_len(sg);
		desc++;
	}
	return 0;
}

static int mcam_vb_sg_buf_finish(struct vb2_buffer *vb)
{
	struct mcam_camera *cam = vb2_get_drv_priv(vb->vb2_queue);
	struct vb2_dma_sg_desc *sgd = vb2_dma_sg_plane_desc(vb, 0);

	dma_unmap_sg(cam->dev, sgd->sglist, sgd->num_pages, DMA_FROM_DEVICE);
	return 0;
}

static void mcam_vb_sg_buf_cleanup(struct vb2_buffer *vb)
{
	struct mcam_camera *cam = vb2_get_drv_priv(vb->vb2_queue);
	struct mcam_vb_buffer *mvb = vb_to_mvb(vb);
	int ndesc = cam->pix_format.sizeimage/PAGE_SIZE + 1;

	dma_free_coherent(cam->dev, ndesc * sizeof(struct mcam_dma_desc),
			mvb->dma_desc, mvb->dma_desc_pa);
}


static const struct vb2_ops mcam_vb2_sg_ops = {
	.queue_setup		= mcam_vb_queue_setup,
	.buf_init		= mcam_vb_sg_buf_init,
	.buf_prepare		= mcam_vb_sg_buf_prepare,
	.buf_queue		= mcam_vb_buf_queue,
	.buf_finish		= mcam_vb_sg_buf_finish,
	.buf_cleanup		= mcam_vb_sg_buf_cleanup,
	.start_streaming	= mcam_vb_start_streaming,
	.stop_streaming		= mcam_vb_stop_streaming,
	.wait_prepare		= mcam_vb_wait_prepare,
	.wait_finish		= mcam_vb_wait_finish,
};

#endif 

static int mcam_setup_vb2(struct mcam_camera *cam)
{
	struct vb2_queue *vq = &cam->vb_queue;

	memset(vq, 0, sizeof(*vq));
	vq->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	vq->drv_priv = cam;
	INIT_LIST_HEAD(&cam->buffers);
	switch (cam->buffer_mode) {
	case B_DMA_contig:
#ifdef MCAM_MODE_DMA_CONTIG
		vq->ops = &mcam_vb2_ops;
		vq->mem_ops = &vb2_dma_contig_memops;
		cam->vb_alloc_ctx = vb2_dma_contig_init_ctx(cam->dev);
		vq->io_modes = VB2_MMAP | VB2_USERPTR;
		cam->dma_setup = mcam_ctlr_dma_contig;
		cam->frame_complete = mcam_dma_contig_done;
#endif
		break;
	case B_DMA_sg:
#ifdef MCAM_MODE_DMA_SG
		vq->ops = &mcam_vb2_sg_ops;
		vq->mem_ops = &vb2_dma_sg_memops;
		vq->io_modes = VB2_MMAP | VB2_USERPTR;
		cam->dma_setup = mcam_ctlr_dma_sg;
		cam->frame_complete = mcam_dma_sg_done;
#endif
		break;
	case B_vmalloc:
#ifdef MCAM_MODE_VMALLOC
		tasklet_init(&cam->s_tasklet, mcam_frame_tasklet,
				(unsigned long) cam);
		vq->ops = &mcam_vb2_ops;
		vq->mem_ops = &vb2_vmalloc_memops;
		vq->buf_struct_size = sizeof(struct mcam_vb_buffer);
		vq->io_modes = VB2_MMAP;
		cam->dma_setup = mcam_ctlr_dma_vmalloc;
		cam->frame_complete = mcam_vmalloc_done;
#endif
		break;
	}
	return vb2_queue_init(vq);
}

static void mcam_cleanup_vb2(struct mcam_camera *cam)
{
	vb2_queue_release(&cam->vb_queue);
#ifdef MCAM_MODE_DMA_CONTIG
	if (cam->buffer_mode == B_DMA_contig)
		vb2_dma_contig_cleanup_ctx(cam->vb_alloc_ctx);
#endif
}



static int mcam_vidioc_streamon(struct file *filp, void *priv,
		enum v4l2_buf_type type)
{
	struct mcam_camera *cam = filp->private_data;
	int ret;

	mutex_lock(&cam->s_mutex);
	ret = vb2_streamon(&cam->vb_queue, type);
	mutex_unlock(&cam->s_mutex);
	return ret;
}


static int mcam_vidioc_streamoff(struct file *filp, void *priv,
		enum v4l2_buf_type type)
{
	struct mcam_camera *cam = filp->private_data;
	int ret;

	mutex_lock(&cam->s_mutex);
	ret = vb2_streamoff(&cam->vb_queue, type);
	mutex_unlock(&cam->s_mutex);
	return ret;
}


static int mcam_vidioc_reqbufs(struct file *filp, void *priv,
		struct v4l2_requestbuffers *req)
{
	struct mcam_camera *cam = filp->private_data;
	int ret;

	mutex_lock(&cam->s_mutex);
	ret = vb2_reqbufs(&cam->vb_queue, req);
	mutex_unlock(&cam->s_mutex);
	return ret;
}


static int mcam_vidioc_querybuf(struct file *filp, void *priv,
		struct v4l2_buffer *buf)
{
	struct mcam_camera *cam = filp->private_data;
	int ret;

	mutex_lock(&cam->s_mutex);
	ret = vb2_querybuf(&cam->vb_queue, buf);
	mutex_unlock(&cam->s_mutex);
	return ret;
}

static int mcam_vidioc_qbuf(struct file *filp, void *priv,
		struct v4l2_buffer *buf)
{
	struct mcam_camera *cam = filp->private_data;
	int ret;

	mutex_lock(&cam->s_mutex);
	ret = vb2_qbuf(&cam->vb_queue, buf);
	mutex_unlock(&cam->s_mutex);
	return ret;
}

static int mcam_vidioc_dqbuf(struct file *filp, void *priv,
		struct v4l2_buffer *buf)
{
	struct mcam_camera *cam = filp->private_data;
	int ret;

	mutex_lock(&cam->s_mutex);
	ret = vb2_dqbuf(&cam->vb_queue, buf, filp->f_flags & O_NONBLOCK);
	mutex_unlock(&cam->s_mutex);
	return ret;
}



static int mcam_vidioc_queryctrl(struct file *filp, void *priv,
		struct v4l2_queryctrl *qc)
{
	struct mcam_camera *cam = priv;
	int ret;

	mutex_lock(&cam->s_mutex);
	ret = sensor_call(cam, core, queryctrl, qc);
	mutex_unlock(&cam->s_mutex);
	return ret;
}


static int mcam_vidioc_g_ctrl(struct file *filp, void *priv,
		struct v4l2_control *ctrl)
{
	struct mcam_camera *cam = priv;
	int ret;

	mutex_lock(&cam->s_mutex);
	ret = sensor_call(cam, core, g_ctrl, ctrl);
	mutex_unlock(&cam->s_mutex);
	return ret;
}


static int mcam_vidioc_s_ctrl(struct file *filp, void *priv,
		struct v4l2_control *ctrl)
{
	struct mcam_camera *cam = priv;
	int ret;

	mutex_lock(&cam->s_mutex);
	ret = sensor_call(cam, core, s_ctrl, ctrl);
	mutex_unlock(&cam->s_mutex);
	return ret;
}


static int mcam_vidioc_querycap(struct file *file, void *priv,
		struct v4l2_capability *cap)
{
	strcpy(cap->driver, "marvell_ccic");
	strcpy(cap->card, "marvell_ccic");
	cap->version = 1;
	cap->capabilities = V4L2_CAP_VIDEO_CAPTURE |
		V4L2_CAP_READWRITE | V4L2_CAP_STREAMING;
	return 0;
}


static int mcam_vidioc_enum_fmt_vid_cap(struct file *filp,
		void *priv, struct v4l2_fmtdesc *fmt)
{
	if (fmt->index >= N_MCAM_FMTS)
		return -EINVAL;
	strlcpy(fmt->description, mcam_formats[fmt->index].desc,
			sizeof(fmt->description));
	fmt->pixelformat = mcam_formats[fmt->index].pixelformat;
	return 0;
}

static int mcam_vidioc_try_fmt_vid_cap(struct file *filp, void *priv,
		struct v4l2_format *fmt)
{
	struct mcam_camera *cam = priv;
	struct mcam_format_struct *f;
	struct v4l2_pix_format *pix = &fmt->fmt.pix;
	struct v4l2_mbus_framefmt mbus_fmt;
	int ret;

	f = mcam_find_format(pix->pixelformat);
	pix->pixelformat = f->pixelformat;
	v4l2_fill_mbus_format(&mbus_fmt, pix, f->mbus_code);
	mutex_lock(&cam->s_mutex);
	ret = sensor_call(cam, video, try_mbus_fmt, &mbus_fmt);
	mutex_unlock(&cam->s_mutex);
	v4l2_fill_pix_format(pix, &mbus_fmt);
	pix->bytesperline = pix->width * f->bpp;
	pix->sizeimage = pix->height * pix->bytesperline;
	return ret;
}

static int mcam_vidioc_s_fmt_vid_cap(struct file *filp, void *priv,
		struct v4l2_format *fmt)
{
	struct mcam_camera *cam = priv;
	struct mcam_format_struct *f;
	int ret;

	if (cam->state != S_IDLE || cam->vb_queue.num_buffers > 0)
		return -EBUSY;

	f = mcam_find_format(fmt->fmt.pix.pixelformat);

	ret = mcam_vidioc_try_fmt_vid_cap(filp, priv, fmt);
	if (ret)
		return ret;
	mutex_lock(&cam->s_mutex);
	cam->pix_format = fmt->fmt.pix;
	cam->mbus_code = f->mbus_code;

	if (cam->buffer_mode == B_vmalloc) {
		ret = mcam_check_dma_buffers(cam);
		if (ret)
			goto out;
	}
	mcam_set_config_needed(cam, 1);
	ret = 0;
out:
	mutex_unlock(&cam->s_mutex);
	return ret;
}

static int mcam_vidioc_g_fmt_vid_cap(struct file *filp, void *priv,
		struct v4l2_format *f)
{
	struct mcam_camera *cam = priv;

	f->fmt.pix = cam->pix_format;
	return 0;
}

static int mcam_vidioc_enum_input(struct file *filp, void *priv,
		struct v4l2_input *input)
{
	if (input->index != 0)
		return -EINVAL;

	input->type = V4L2_INPUT_TYPE_CAMERA;
	input->std = V4L2_STD_ALL; 
	strcpy(input->name, "Camera");
	return 0;
}

static int mcam_vidioc_g_input(struct file *filp, void *priv, unsigned int *i)
{
	*i = 0;
	return 0;
}

static int mcam_vidioc_s_input(struct file *filp, void *priv, unsigned int i)
{
	if (i != 0)
		return -EINVAL;
	return 0;
}

static int mcam_vidioc_s_std(struct file *filp, void *priv, v4l2_std_id *a)
{
	return 0;
}

static int mcam_vidioc_g_parm(struct file *filp, void *priv,
		struct v4l2_streamparm *parms)
{
	struct mcam_camera *cam = priv;
	int ret;

	mutex_lock(&cam->s_mutex);
	ret = sensor_call(cam, video, g_parm, parms);
	mutex_unlock(&cam->s_mutex);
	parms->parm.capture.readbuffers = n_dma_bufs;
	return ret;
}

static int mcam_vidioc_s_parm(struct file *filp, void *priv,
		struct v4l2_streamparm *parms)
{
	struct mcam_camera *cam = priv;
	int ret;

	mutex_lock(&cam->s_mutex);
	ret = sensor_call(cam, video, s_parm, parms);
	mutex_unlock(&cam->s_mutex);
	parms->parm.capture.readbuffers = n_dma_bufs;
	return ret;
}

static int mcam_vidioc_g_chip_ident(struct file *file, void *priv,
		struct v4l2_dbg_chip_ident *chip)
{
	struct mcam_camera *cam = priv;

	chip->ident = V4L2_IDENT_NONE;
	chip->revision = 0;
	if (v4l2_chip_match_host(&chip->match)) {
		chip->ident = cam->chip_id;
		return 0;
	}
	return sensor_call(cam, core, g_chip_ident, chip);
}

static int mcam_vidioc_enum_framesizes(struct file *filp, void *priv,
		struct v4l2_frmsizeenum *sizes)
{
	struct mcam_camera *cam = priv;
	int ret;

	mutex_lock(&cam->s_mutex);
	ret = sensor_call(cam, video, enum_framesizes, sizes);
	mutex_unlock(&cam->s_mutex);
	return ret;
}

static int mcam_vidioc_enum_frameintervals(struct file *filp, void *priv,
		struct v4l2_frmivalenum *interval)
{
	struct mcam_camera *cam = priv;
	int ret;

	mutex_lock(&cam->s_mutex);
	ret = sensor_call(cam, video, enum_frameintervals, interval);
	mutex_unlock(&cam->s_mutex);
	return ret;
}

#ifdef CONFIG_VIDEO_ADV_DEBUG
static int mcam_vidioc_g_register(struct file *file, void *priv,
		struct v4l2_dbg_register *reg)
{
	struct mcam_camera *cam = priv;

	if (v4l2_chip_match_host(&reg->match)) {
		reg->val = mcam_reg_read(cam, reg->reg);
		reg->size = 4;
		return 0;
	}
	return sensor_call(cam, core, g_register, reg);
}

static int mcam_vidioc_s_register(struct file *file, void *priv,
		struct v4l2_dbg_register *reg)
{
	struct mcam_camera *cam = priv;

	if (v4l2_chip_match_host(&reg->match)) {
		mcam_reg_write(cam, reg->reg, reg->val);
		return 0;
	}
	return sensor_call(cam, core, s_register, reg);
}
#endif

static const struct v4l2_ioctl_ops mcam_v4l_ioctl_ops = {
	.vidioc_querycap	= mcam_vidioc_querycap,
	.vidioc_enum_fmt_vid_cap = mcam_vidioc_enum_fmt_vid_cap,
	.vidioc_try_fmt_vid_cap	= mcam_vidioc_try_fmt_vid_cap,
	.vidioc_s_fmt_vid_cap	= mcam_vidioc_s_fmt_vid_cap,
	.vidioc_g_fmt_vid_cap	= mcam_vidioc_g_fmt_vid_cap,
	.vidioc_enum_input	= mcam_vidioc_enum_input,
	.vidioc_g_input		= mcam_vidioc_g_input,
	.vidioc_s_input		= mcam_vidioc_s_input,
	.vidioc_s_std		= mcam_vidioc_s_std,
	.vidioc_reqbufs		= mcam_vidioc_reqbufs,
	.vidioc_querybuf	= mcam_vidioc_querybuf,
	.vidioc_qbuf		= mcam_vidioc_qbuf,
	.vidioc_dqbuf		= mcam_vidioc_dqbuf,
	.vidioc_streamon	= mcam_vidioc_streamon,
	.vidioc_streamoff	= mcam_vidioc_streamoff,
	.vidioc_queryctrl	= mcam_vidioc_queryctrl,
	.vidioc_g_ctrl		= mcam_vidioc_g_ctrl,
	.vidioc_s_ctrl		= mcam_vidioc_s_ctrl,
	.vidioc_g_parm		= mcam_vidioc_g_parm,
	.vidioc_s_parm		= mcam_vidioc_s_parm,
	.vidioc_enum_framesizes = mcam_vidioc_enum_framesizes,
	.vidioc_enum_frameintervals = mcam_vidioc_enum_frameintervals,
	.vidioc_g_chip_ident	= mcam_vidioc_g_chip_ident,
#ifdef CONFIG_VIDEO_ADV_DEBUG
	.vidioc_g_register	= mcam_vidioc_g_register,
	.vidioc_s_register	= mcam_vidioc_s_register,
#endif
};

static int mcam_v4l_open(struct file *filp)
{
	struct mcam_camera *cam = video_drvdata(filp);
	int ret = 0;

	filp->private_data = cam;

	frames = singles = delivered = 0;
	mutex_lock(&cam->s_mutex);
	if (cam->users == 0) {
		ret = mcam_setup_vb2(cam);
		if (ret)
			goto out;
		mcam_ctlr_power_up(cam);
		__mcam_cam_reset(cam);
		mcam_set_config_needed(cam, 1);
	}
	(cam->users)++;
out:
	mutex_unlock(&cam->s_mutex);
	return ret;
}


static int mcam_v4l_release(struct file *filp)
{
	struct mcam_camera *cam = filp->private_data;

	cam_dbg(cam, "Release, %d frames, %d singles, %d delivered\n", frames,
			singles, delivered);
	mutex_lock(&cam->s_mutex);
	(cam->users)--;
	if (cam->users == 0) {
		mcam_ctlr_stop_dma(cam);
		mcam_cleanup_vb2(cam);
		mcam_ctlr_power_down(cam);
		if (cam->buffer_mode == B_vmalloc && alloc_bufs_at_read)
			mcam_free_dma_bufs(cam);
	}
	mutex_unlock(&cam->s_mutex);
	return 0;
}

static ssize_t mcam_v4l_read(struct file *filp,
		char __user *buffer, size_t len, loff_t *pos)
{
	struct mcam_camera *cam = filp->private_data;
	int ret;

	mutex_lock(&cam->s_mutex);
	ret = vb2_read(&cam->vb_queue, buffer, len, pos,
			filp->f_flags & O_NONBLOCK);
	mutex_unlock(&cam->s_mutex);
	return ret;
}



static unsigned int mcam_v4l_poll(struct file *filp,
		struct poll_table_struct *pt)
{
	struct mcam_camera *cam = filp->private_data;
	int ret;

	mutex_lock(&cam->s_mutex);
	ret = vb2_poll(&cam->vb_queue, filp, pt);
	mutex_unlock(&cam->s_mutex);
	return ret;
}


static int mcam_v4l_mmap(struct file *filp, struct vm_area_struct *vma)
{
	struct mcam_camera *cam = filp->private_data;
	int ret;

	mutex_lock(&cam->s_mutex);
	ret = vb2_mmap(&cam->vb_queue, vma);
	mutex_unlock(&cam->s_mutex);
	return ret;
}



static const struct v4l2_file_operations mcam_v4l_fops = {
	.owner = THIS_MODULE,
	.open = mcam_v4l_open,
	.release = mcam_v4l_release,
	.read = mcam_v4l_read,
	.poll = mcam_v4l_poll,
	.mmap = mcam_v4l_mmap,
	.unlocked_ioctl = video_ioctl2,
};


static struct video_device mcam_v4l_template = {
	.name = "mcam",
	.tvnorms = V4L2_STD_NTSC_M,
	.current_norm = V4L2_STD_NTSC_M,  

	.fops = &mcam_v4l_fops,
	.ioctl_ops = &mcam_v4l_ioctl_ops,
	.release = video_device_release_empty,
};

static void mcam_frame_complete(struct mcam_camera *cam, int frame)
{
	set_bit(frame, &cam->flags);
	clear_bit(CF_DMA_ACTIVE, &cam->flags);
	cam->next_buf = frame;
	cam->buf_seq[frame] = ++(cam->sequence);
	frames++;
	if (cam->state != S_STREAMING)
		return;
	cam->frame_complete(cam, frame);
}


int mccic_irq(struct mcam_camera *cam, unsigned int irqs)
{
	unsigned int frame, handled = 0;

	mcam_reg_write(cam, REG_IRQSTAT, FRAMEIRQS); 
	for (frame = 0; frame < cam->nbufs; frame++)
		if (irqs & (IRQ_EOF0 << frame)) {
			mcam_frame_complete(cam, frame);
			handled = 1;
			if (cam->buffer_mode == B_DMA_sg)
				break;
		}
	if (irqs & (IRQ_SOF0 | IRQ_SOF1 | IRQ_SOF2)) {
		set_bit(CF_DMA_ACTIVE, &cam->flags);
		handled = 1;
		if (cam->buffer_mode == B_DMA_sg)
			mcam_ctlr_stop(cam);
	}
	return handled;
}

static struct ov7670_config sensor_cfg = {
	.min_width = 320,
	.min_height = 240,
};


int mccic_register(struct mcam_camera *cam)
{
	struct i2c_board_info ov7670_info = {
		.type = "ov7670",
		.addr = 0x42 >> 1,
		.platform_data = &sensor_cfg,
	};
	int ret;

	if (buffer_mode >= 0)
		cam->buffer_mode = buffer_mode;
	if (cam->buffer_mode == B_DMA_sg &&
			cam->chip_id == V4L2_IDENT_CAFE) {
		printk(KERN_ERR "marvell-cam: Cafe can't do S/G I/O, "
			"attempting vmalloc mode instead\n");
		cam->buffer_mode = B_vmalloc;
	}
	if (!mcam_buffer_mode_supported(cam->buffer_mode)) {
		printk(KERN_ERR "marvell-cam: buffer mode %d unsupported\n",
				cam->buffer_mode);
		return -EINVAL;
	}
	ret = v4l2_device_register(cam->dev, &cam->v4l2_dev);
	if (ret)
		return ret;

	mutex_init(&cam->s_mutex);
	cam->state = S_NOTREADY;
	mcam_set_config_needed(cam, 1);
	cam->pix_format = mcam_def_pix_format;
	cam->mbus_code = mcam_def_mbus_code;
	INIT_LIST_HEAD(&cam->buffers);
	mcam_ctlr_init(cam);

	sensor_cfg.clock_speed = cam->clock_speed;
	sensor_cfg.use_smbus = cam->use_smbus;
	cam->sensor_addr = ov7670_info.addr;
	cam->sensor = v4l2_i2c_new_subdev_board(&cam->v4l2_dev,
			cam->i2c_adapter, &ov7670_info, NULL);
	if (cam->sensor == NULL) {
		ret = -ENODEV;
		goto out_unregister;
	}

	ret = mcam_cam_init(cam);
	if (ret)
		goto out_unregister;
	mutex_lock(&cam->s_mutex);
	cam->vdev = mcam_v4l_template;
	cam->vdev.debug = 0;
	cam->vdev.v4l2_dev = &cam->v4l2_dev;
	ret = video_register_device(&cam->vdev, VFL_TYPE_GRABBER, -1);
	if (ret)
		goto out;
	video_set_drvdata(&cam->vdev, cam);

	if (cam->buffer_mode == B_vmalloc && !alloc_bufs_at_read) {
		if (mcam_alloc_dma_bufs(cam, 1))
			cam_warn(cam, "Unable to alloc DMA buffers at load"
					" will try again later.");
	}

out:
	mutex_unlock(&cam->s_mutex);
	return ret;
out_unregister:
	v4l2_device_unregister(&cam->v4l2_dev);
	return ret;
}


void mccic_shutdown(struct mcam_camera *cam)
{
	if (cam->users > 0) {
		cam_warn(cam, "Removing a device with users!\n");
		mcam_ctlr_power_down(cam);
	}
	vb2_queue_release(&cam->vb_queue);
	if (cam->buffer_mode == B_vmalloc)
		mcam_free_dma_bufs(cam);
	video_unregister_device(&cam->vdev);
	v4l2_device_unregister(&cam->v4l2_dev);
}

#ifdef CONFIG_PM

void mccic_suspend(struct mcam_camera *cam)
{
	mutex_lock(&cam->s_mutex);
	if (cam->users > 0) {
		enum mcam_state cstate = cam->state;

		mcam_ctlr_stop_dma(cam);
		mcam_ctlr_power_down(cam);
		cam->state = cstate;
	}
	mutex_unlock(&cam->s_mutex);
}

int mccic_resume(struct mcam_camera *cam)
{
	int ret = 0;

	mutex_lock(&cam->s_mutex);
	if (cam->users > 0) {
		mcam_ctlr_power_up(cam);
		__mcam_cam_reset(cam);
	} else {
		mcam_ctlr_power_down(cam);
	}
	mutex_unlock(&cam->s_mutex);

	set_bit(CF_CONFIG_NEEDED, &cam->flags);
	if (cam->state == S_STREAMING) {
		if (cam->buffer_mode == B_DMA_sg && cam->vb_bufs[0])
			list_add(&cam->vb_bufs[0]->queue, &cam->buffers);
		ret = mcam_read_setup(cam);
	}
	return ret;
}
#endif 
