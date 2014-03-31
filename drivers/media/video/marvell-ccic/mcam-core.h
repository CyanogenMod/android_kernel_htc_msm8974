/*
 * Marvell camera core structures.
 *
 * Copyright 2011 Jonathan Corbet corbet@lwn.net
 */
#ifndef _MCAM_CORE_H
#define _MCAM_CORE_H

#include <linux/list.h>
#include <media/v4l2-common.h>
#include <media/v4l2-dev.h>
#include <media/videobuf2-core.h>

#if defined(CONFIG_VIDEOBUF2_VMALLOC) || defined(CONFIG_VIDEOBUF2_VMALLOC_MODULE)
#define MCAM_MODE_VMALLOC 1
#endif

#if defined(CONFIG_VIDEOBUF2_DMA_CONTIG) || defined(CONFIG_VIDEOBUF2_DMA_CONTIG_MODULE)
#define MCAM_MODE_DMA_CONTIG 1
#endif

#if defined(CONFIG_VIDEOBUF2_DMA_SG) || defined(CONFIG_VIDEOBUF2_DMA_SG_MODULE)
#define MCAM_MODE_DMA_SG 1
#endif

#if !defined(MCAM_MODE_VMALLOC) && !defined(MCAM_MODE_DMA_CONTIG) && \
	!defined(MCAM_MODE_DMA_SG)
#error One of the videobuf buffer modes must be selected in the config
#endif


enum mcam_state {
	S_NOTREADY,	
	S_IDLE,		
	S_FLAKED,	
	S_STREAMING,	
	S_BUFWAIT	
};
#define MAX_DMA_BUFS 3

enum mcam_buffer_mode {
	B_vmalloc = 0,
	B_DMA_contig = 1,
	B_DMA_sg = 2
};

static inline int mcam_buffer_mode_supported(enum mcam_buffer_mode mode)
{
	switch (mode) {
#ifdef MCAM_MODE_VMALLOC
	case B_vmalloc:
#endif
#ifdef MCAM_MODE_DMA_CONTIG
	case B_DMA_contig:
#endif
#ifdef MCAM_MODE_DMA_SG
	case B_DMA_sg:
#endif
		return 1;
	default:
		return 0;
	}
}


struct mcam_camera {
	struct i2c_adapter *i2c_adapter;
	unsigned char __iomem *regs;
	spinlock_t dev_lock;
	struct device *dev; 
	unsigned int chip_id;
	short int clock_speed;	
	short int use_smbus;	
	enum mcam_buffer_mode buffer_mode;
	void (*plat_power_up) (struct mcam_camera *cam);
	void (*plat_power_down) (struct mcam_camera *cam);

	struct v4l2_device v4l2_dev;
	enum mcam_state state;
	unsigned long flags;		
	int users;			

	struct video_device vdev;
	struct v4l2_subdev *sensor;
	unsigned short sensor_addr;

	
	struct vb2_queue vb_queue;
	struct list_head buffers;	

	unsigned int nbufs;		
	int next_buf;			

	
#ifdef MCAM_MODE_VMALLOC
	unsigned int dma_buf_size;	
	void *dma_bufs[MAX_DMA_BUFS];	
	dma_addr_t dma_handles[MAX_DMA_BUFS]; 
	struct tasklet_struct s_tasklet;
#endif
	unsigned int sequence;		
	unsigned int buf_seq[MAX_DMA_BUFS]; 

	
	struct mcam_vb_buffer *vb_bufs[MAX_DMA_BUFS];
	struct vb2_alloc_ctx *vb_alloc_ctx;

	
	void (*dma_setup)(struct mcam_camera *cam);
	void (*frame_complete)(struct mcam_camera *cam, int frame);

	
	u32 sensor_type;		
	struct v4l2_pix_format pix_format;
	enum v4l2_mbus_pixelcode mbus_code;

	
	struct mutex s_mutex; 
};


static inline void mcam_reg_write(struct mcam_camera *cam, unsigned int reg,
		unsigned int val)
{
	iowrite32(val, cam->regs + reg);
}

static inline unsigned int mcam_reg_read(struct mcam_camera *cam,
		unsigned int reg)
{
	return ioread32(cam->regs + reg);
}


static inline void mcam_reg_write_mask(struct mcam_camera *cam, unsigned int reg,
		unsigned int val, unsigned int mask)
{
	unsigned int v = mcam_reg_read(cam, reg);

	v = (v & ~mask) | (val & mask);
	mcam_reg_write(cam, reg, v);
}

static inline void mcam_reg_clear_bit(struct mcam_camera *cam,
		unsigned int reg, unsigned int val)
{
	mcam_reg_write_mask(cam, reg, 0, val);
}

static inline void mcam_reg_set_bit(struct mcam_camera *cam,
		unsigned int reg, unsigned int val)
{
	mcam_reg_write_mask(cam, reg, val, val);
}

int mccic_register(struct mcam_camera *cam);
int mccic_irq(struct mcam_camera *cam, unsigned int irqs);
void mccic_shutdown(struct mcam_camera *cam);
#ifdef CONFIG_PM
void mccic_suspend(struct mcam_camera *cam);
int mccic_resume(struct mcam_camera *cam);
#endif

#define REG_Y0BAR	0x00
#define REG_Y1BAR	0x04
#define REG_Y2BAR	0x08

#define REG_IMGPITCH	0x24	
#define   IMGP_YP_SHFT	  2		
#define   IMGP_YP_MASK	  0x00003ffc	
#define	  IMGP_UVP_SHFT	  18		
#define   IMGP_UVP_MASK   0x3ffc0000
#define REG_IRQSTATRAW	0x28	
#define   IRQ_EOF0	  0x00000001	
#define   IRQ_EOF1	  0x00000002	
#define   IRQ_EOF2	  0x00000004	
#define   IRQ_SOF0	  0x00000008	
#define   IRQ_SOF1	  0x00000010	
#define   IRQ_SOF2	  0x00000020	
#define   IRQ_OVERFLOW	  0x00000040	
#define   IRQ_TWSIW	  0x00010000	
#define   IRQ_TWSIR	  0x00020000	
#define   IRQ_TWSIE	  0x00040000	
#define   TWSIIRQS (IRQ_TWSIW|IRQ_TWSIR|IRQ_TWSIE)
#define   FRAMEIRQS (IRQ_EOF0|IRQ_EOF1|IRQ_EOF2|IRQ_SOF0|IRQ_SOF1|IRQ_SOF2)
#define   ALLIRQS (TWSIIRQS|FRAMEIRQS|IRQ_OVERFLOW)
#define REG_IRQMASK	0x2c	
#define REG_IRQSTAT	0x30	

#define REG_IMGSIZE	0x34	
#define  IMGSZ_V_MASK	  0x1fff0000
#define  IMGSZ_V_SHIFT	  16
#define	 IMGSZ_H_MASK	  0x00003fff
#define REG_IMGOFFSET	0x38	

#define REG_CTRL0	0x3c	
#define   C0_ENABLE	  0x00000001	

#define   C0_DF_MASK	  0x00fffffc    

#define	  C0_RGB4_RGBX	  0x00000000
#define	  C0_RGB4_XRGB	  0x00000004
#define	  C0_RGB4_BGRX	  0x00000008
#define	  C0_RGB4_XBGR	  0x0000000c
#define	  C0_RGB5_RGGB	  0x00000000
#define	  C0_RGB5_GRBG	  0x00000004
#define	  C0_RGB5_GBRG	  0x00000008
#define	  C0_RGB5_BGGR	  0x0000000c

#define	  C0_DF_YUV	  0x00000000	
#define	  C0_DF_RGB	  0x000000a0	
#define	  C0_DF_BAYER	  0x00000140	
#define	  C0_RGBF_565	  0x00000000
#define	  C0_RGBF_444	  0x00000800
#define	  C0_RGB_BGR	  0x00001000	
#define	  C0_YUV_PLANAR	  0x00000000	
#define	  C0_YUV_PACKED	  0x00008000	
#define	  C0_YUV_420PL	  0x0000a000	
#define	  C0_YUVE_YUYV	  0x00000000	
#define	  C0_YUVE_YVYU	  0x00010000	
#define	  C0_YUVE_VYUY	  0x00020000	
#define	  C0_YUVE_UYVY	  0x00030000	
#define	  C0_YUVE_XYUV	  0x00000000	
#define	  C0_YUVE_XYVU	  0x00010000	
#define	  C0_YUVE_XUVY	  0x00020000	
#define	  C0_YUVE_XVUY	  0x00030000	
#define	  C0_HPOL_LOW	  0x01000000	
#define	  C0_VPOL_LOW	  0x02000000	
#define	  C0_VCLK_LOW	  0x04000000	
#define	  C0_DOWNSCALE	  0x08000000	
#define	  C0_SIFM_MASK	  0xc0000000	
#define	  C0_SIF_HVSYNC	  0x00000000	
#define	  CO_SOF_NOSYNC	  0x40000000	

#define REG_CTRL1	0x40	
#define	  C1_CLKGATE	  0x00000001	
#define   C1_DESC_ENA	  0x00000100	
#define   C1_DESC_3WORD   0x00000200	
#define	  C1_444ALPHA	  0x00f00000	
#define	  C1_ALPHA_SHFT	  20
#define	  C1_DMAB32	  0x00000000	
#define	  C1_DMAB16	  0x02000000	
#define	  C1_DMAB64	  0x04000000	
#define	  C1_DMAB_MASK	  0x06000000
#define	  C1_TWOBUFS	  0x08000000	
#define	  C1_PWRDWN	  0x10000000	

#define REG_CLKCTRL	0x88	
#define	  CLK_DIV_MASK	  0x0000ffff	

#define REG_UBAR	0xc4	

#define	REG_DMA_DESC_Y	0x200
#define	REG_DMA_DESC_U	0x204
#define	REG_DMA_DESC_V	0x208
#define REG_DESC_LEN_Y	0x20c	
#define	REG_DESC_LEN_U	0x210
#define REG_DESC_LEN_V	0x214

#define VGA_WIDTH	640
#define VGA_HEIGHT	480

#endif 
