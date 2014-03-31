/* cg14.c: CGFOURTEEN frame buffer driver
 *
 * Copyright (C) 2003, 2006 David S. Miller (davem@davemloft.net)
 * Copyright (C) 1996,1998 Jakub Jelinek (jj@ultra.linux.cz)
 * Copyright (C) 1995 Miguel de Icaza (miguel@nuclecu.unam.mx)
 *
 * Driver layout based loosely on tgafb.c, see that file for credits.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/fb.h>
#include <linux/mm.h>
#include <linux/uaccess.h>
#include <linux/of_device.h>

#include <asm/io.h>
#include <asm/fbio.h>

#include "sbuslib.h"


static int cg14_setcolreg(unsigned, unsigned, unsigned, unsigned,
			 unsigned, struct fb_info *);

static int cg14_mmap(struct fb_info *, struct vm_area_struct *);
static int cg14_ioctl(struct fb_info *, unsigned int, unsigned long);
static int cg14_pan_display(struct fb_var_screeninfo *, struct fb_info *);


static struct fb_ops cg14_ops = {
	.owner			= THIS_MODULE,
	.fb_setcolreg		= cg14_setcolreg,
	.fb_pan_display		= cg14_pan_display,
	.fb_fillrect		= cfb_fillrect,
	.fb_copyarea		= cfb_copyarea,
	.fb_imageblit		= cfb_imageblit,
	.fb_mmap		= cg14_mmap,
	.fb_ioctl		= cg14_ioctl,
#ifdef CONFIG_COMPAT
	.fb_compat_ioctl	= sbusfb_compat_ioctl,
#endif
};

#define CG14_MCR_INTENABLE_SHIFT	7
#define CG14_MCR_INTENABLE_MASK		0x80
#define CG14_MCR_VIDENABLE_SHIFT	6
#define CG14_MCR_VIDENABLE_MASK		0x40
#define CG14_MCR_PIXMODE_SHIFT		4
#define CG14_MCR_PIXMODE_MASK		0x30
#define CG14_MCR_TMR_SHIFT		2
#define CG14_MCR_TMR_MASK		0x0c
#define CG14_MCR_TMENABLE_SHIFT		1
#define CG14_MCR_TMENABLE_MASK		0x02
#define CG14_MCR_RESET_SHIFT		0
#define CG14_MCR_RESET_MASK		0x01
#define CG14_REV_REVISION_SHIFT		4
#define CG14_REV_REVISION_MASK		0xf0
#define CG14_REV_IMPL_SHIFT		0
#define CG14_REV_IMPL_MASK		0x0f
#define CG14_VBR_FRAMEBASE_SHIFT	12
#define CG14_VBR_FRAMEBASE_MASK		0x00fff000
#define CG14_VMCR1_SETUP_SHIFT		0
#define CG14_VMCR1_SETUP_MASK		0x000001ff
#define CG14_VMCR1_VCONFIG_SHIFT	9
#define CG14_VMCR1_VCONFIG_MASK		0x00000e00
#define CG14_VMCR2_REFRESH_SHIFT	0
#define CG14_VMCR2_REFRESH_MASK		0x00000001
#define CG14_VMCR2_TESTROWCNT_SHIFT	1
#define CG14_VMCR2_TESTROWCNT_MASK	0x00000002
#define CG14_VMCR2_FBCONFIG_SHIFT	2
#define CG14_VMCR2_FBCONFIG_MASK	0x0000000c
#define CG14_VCR_REFRESHREQ_SHIFT	0
#define CG14_VCR_REFRESHREQ_MASK	0x000003ff
#define CG14_VCR1_REFRESHENA_SHIFT	10
#define CG14_VCR1_REFRESHENA_MASK	0x00000400
#define CG14_VCA_CAD_SHIFT		0
#define CG14_VCA_CAD_MASK		0x000003ff
#define CG14_VCA_VERS_SHIFT		10
#define CG14_VCA_VERS_MASK		0x00000c00
#define CG14_VCA_RAMSPEED_SHIFT		12
#define CG14_VCA_RAMSPEED_MASK		0x00001000
#define CG14_VCA_8MB_SHIFT		13
#define CG14_VCA_8MB_MASK		0x00002000

#define CG14_MCR_PIXMODE_8		0
#define CG14_MCR_PIXMODE_16		2
#define CG14_MCR_PIXMODE_32		3

struct cg14_regs{
	u8 mcr;	
	u8 ppr;	
	u8 tms[2];	
	u8 msr;	
	u8 fsr;	
	u8 rev;	
	u8 ccr;	
	u32 tmr;	
	u8 mod;	
	u8 acr;	
	u8 xxx0[6];
	u16 hct;	
	u16 vct;	
	u16 hbs;	
	u16 hbc;	
	u16 hss;	
	u16 hsc;	
	u16 csc;	
	u16 vbs;	
	u16 vbc;	
	u16 vss;	
	u16 vsc;	
	u16 xcs;
	u16 xcc;
	u16 fsa;	
	u16 adr;	
	u8 xxx1[0xce];
	u8 pcg[0x100]; 
	u32 vbr;	
	u32 vmcr;	
	u32 vcr;	
	u32 vca;	
};

#define CG14_CCR_ENABLE	0x04
#define CG14_CCR_SELECT 0x02	

struct cg14_cursor {
	u32 cpl0[32];	
	u32 cpl1[32];  
	u8 ccr;	
	u8 xxx0[3];
	u16 cursx;	
	u16 cursy;	
	u32 color0;
	u32 color1;
	u32 xxx1[0x1bc];
	u32 cpl0i[32];	
	u32 cpl1i[32]; 
};

struct cg14_dac {
	u8 addr;	
	u8 xxx0[255];
	u8 glut;	
	u8 xxx1[255];
	u8 select;	
	u8 xxx2[255];
	u8 mode;	
};

struct cg14_xlut{
	u8 x_xlut [256];
	u8 x_xlutd [256];
	u8 xxx0[0x600];
	u8 x_xlut_inc [256];
	u8 x_xlutd_inc [256];
};

struct cg14_clut {
	u32 c_clut [256];
	u32 c_clutd [256];    
	u32 c_clut_inc [256];
	u32 c_clutd_inc [256];
};

#define CG14_MMAP_ENTRIES	16

struct cg14_par {
	spinlock_t		lock;
	struct cg14_regs	__iomem *regs;
	struct cg14_clut	__iomem *clut;
	struct cg14_cursor	__iomem *cursor;

	u32			flags;
#define CG14_FLAG_BLANKED	0x00000001

	unsigned long		iospace;

	struct sbus_mmap_map	mmap_map[CG14_MMAP_ENTRIES];

	int			mode;
	int			ramsize;
};

static void __cg14_reset(struct cg14_par *par)
{
	struct cg14_regs __iomem *regs = par->regs;
	u8 val;

	val = sbus_readb(&regs->mcr);
	val &= ~(CG14_MCR_PIXMODE_MASK);
	sbus_writeb(val, &regs->mcr);
}

static int cg14_pan_display(struct fb_var_screeninfo *var, struct fb_info *info)
{
	struct cg14_par *par = (struct cg14_par *) info->par;
	unsigned long flags;

	spin_lock_irqsave(&par->lock, flags);
	__cg14_reset(par);
	spin_unlock_irqrestore(&par->lock, flags);

	if (var->xoffset || var->yoffset || var->vmode)
		return -EINVAL;
	return 0;
}

static int cg14_setcolreg(unsigned regno,
			  unsigned red, unsigned green, unsigned blue,
			  unsigned transp, struct fb_info *info)
{
	struct cg14_par *par = (struct cg14_par *) info->par;
	struct cg14_clut __iomem *clut = par->clut;
	unsigned long flags;
	u32 val;

	if (regno >= 256)
		return 1;

	red >>= 8;
	green >>= 8;
	blue >>= 8;
	val = (red | (green << 8) | (blue << 16));

	spin_lock_irqsave(&par->lock, flags);
	sbus_writel(val, &clut->c_clut[regno]);
	spin_unlock_irqrestore(&par->lock, flags);

	return 0;
}

static int cg14_mmap(struct fb_info *info, struct vm_area_struct *vma)
{
	struct cg14_par *par = (struct cg14_par *) info->par;

	return sbusfb_mmap_helper(par->mmap_map,
				  info->fix.smem_start, info->fix.smem_len,
				  par->iospace, vma);
}

static int cg14_ioctl(struct fb_info *info, unsigned int cmd, unsigned long arg)
{
	struct cg14_par *par = (struct cg14_par *) info->par;
	struct cg14_regs __iomem *regs = par->regs;
	struct mdi_cfginfo kmdi, __user *mdii;
	unsigned long flags;
	int cur_mode, mode, ret = 0;

	switch (cmd) {
	case MDI_RESET:
		spin_lock_irqsave(&par->lock, flags);
		__cg14_reset(par);
		spin_unlock_irqrestore(&par->lock, flags);
		break;

	case MDI_GET_CFGINFO:
		memset(&kmdi, 0, sizeof(kmdi));

		spin_lock_irqsave(&par->lock, flags);
		kmdi.mdi_type = FBTYPE_MDICOLOR;
		kmdi.mdi_height = info->var.yres;
		kmdi.mdi_width = info->var.xres;
		kmdi.mdi_mode = par->mode;
		kmdi.mdi_pixfreq = 72; 
		kmdi.mdi_size = par->ramsize;
		spin_unlock_irqrestore(&par->lock, flags);

		mdii = (struct mdi_cfginfo __user *) arg;
		if (copy_to_user(mdii, &kmdi, sizeof(kmdi)))
			ret = -EFAULT;
		break;

	case MDI_SET_PIXELMODE:
		if (get_user(mode, (int __user *) arg)) {
			ret = -EFAULT;
			break;
		}

		spin_lock_irqsave(&par->lock, flags);
		cur_mode = sbus_readb(&regs->mcr);
		cur_mode &= ~CG14_MCR_PIXMODE_MASK;
		switch(mode) {
		case MDI_32_PIX:
			cur_mode |= (CG14_MCR_PIXMODE_32 <<
				     CG14_MCR_PIXMODE_SHIFT);
			break;

		case MDI_16_PIX:
			cur_mode |= (CG14_MCR_PIXMODE_16 <<
				     CG14_MCR_PIXMODE_SHIFT);
			break;

		case MDI_8_PIX:
			break;

		default:
			ret = -ENOSYS;
			break;
		};
		if (!ret) {
			sbus_writeb(cur_mode, &regs->mcr);
			par->mode = mode;
		}
		spin_unlock_irqrestore(&par->lock, flags);
		break;

	default:
		ret = sbusfb_ioctl_helper(cmd, arg, info,
					  FBTYPE_MDICOLOR, 8,
					  info->fix.smem_len);
		break;
	};

	return ret;
}


static void __devinit cg14_init_fix(struct fb_info *info, int linebytes,
				    struct device_node *dp)
{
	const char *name = dp->name;

	strlcpy(info->fix.id, name, sizeof(info->fix.id));

	info->fix.type = FB_TYPE_PACKED_PIXELS;
	info->fix.visual = FB_VISUAL_PSEUDOCOLOR;

	info->fix.line_length = linebytes;

	info->fix.accel = FB_ACCEL_SUN_CG14;
}

static struct sbus_mmap_map __cg14_mmap_map[CG14_MMAP_ENTRIES] __devinitdata = {
	{
		.voff	= CG14_REGS,
		.poff	= 0x80000000,
		.size	= 0x1000
	},
	{
		.voff	= CG14_XLUT,
		.poff	= 0x80003000,
		.size	= 0x1000
	},
	{
		.voff	= CG14_CLUT1,
		.poff	= 0x80004000,
		.size	= 0x1000
	},
	{
		.voff	= CG14_CLUT2,
		.poff	= 0x80005000,
		.size	= 0x1000
	},
	{
		.voff	= CG14_CLUT3,
		.poff	= 0x80006000,
		.size	= 0x1000
	},
	{
		.voff	= CG3_MMAP_OFFSET - 0x7000,
		.poff	= 0x80000000,
		.size	= 0x7000
	},
	{
		.voff	= CG3_MMAP_OFFSET,
		.poff	= 0x00000000,
		.size	= SBUS_MMAP_FBSIZE(1)
	},
	{
		.voff	= MDI_CURSOR_MAP,
		.poff	= 0x80001000,
		.size	= 0x1000
	},
	{
		.voff	= MDI_CHUNKY_BGR_MAP,
		.poff	= 0x01000000,
		.size	= 0x400000
	},
	{
		.voff	= MDI_PLANAR_X16_MAP,
		.poff	= 0x02000000,
		.size	= 0x200000
	},
	{
		.voff	= MDI_PLANAR_C16_MAP,
		.poff	= 0x02800000,
		.size	= 0x200000
	},
	{
		.voff	= MDI_PLANAR_X32_MAP,
		.poff	= 0x03000000,
		.size	= 0x100000
	},
	{
		.voff	= MDI_PLANAR_B32_MAP,
		.poff	= 0x03400000,
		.size	= 0x100000
	},
	{
		.voff	= MDI_PLANAR_G32_MAP,
		.poff	= 0x03800000,
		.size	= 0x100000
	},
	{
		.voff	= MDI_PLANAR_R32_MAP,
		.poff	= 0x03c00000,
		.size	= 0x100000
	},
	{ .size = 0 }
};

static void cg14_unmap_regs(struct platform_device *op, struct fb_info *info,
			    struct cg14_par *par)
{
	if (par->regs)
		of_iounmap(&op->resource[0],
			   par->regs, sizeof(struct cg14_regs));
	if (par->clut)
		of_iounmap(&op->resource[0],
			   par->clut, sizeof(struct cg14_clut));
	if (par->cursor)
		of_iounmap(&op->resource[0],
			   par->cursor, sizeof(struct cg14_cursor));
	if (info->screen_base)
		of_iounmap(&op->resource[1],
			   info->screen_base, info->fix.smem_len);
}

static int __devinit cg14_probe(struct platform_device *op)
{
	struct device_node *dp = op->dev.of_node;
	struct fb_info *info;
	struct cg14_par *par;
	int is_8mb, linebytes, i, err;

	info = framebuffer_alloc(sizeof(struct cg14_par), &op->dev);

	err = -ENOMEM;
	if (!info)
		goto out_err;
	par = info->par;

	spin_lock_init(&par->lock);

	sbusfb_fill_var(&info->var, dp, 8);
	info->var.red.length = 8;
	info->var.green.length = 8;
	info->var.blue.length = 8;

	linebytes = of_getintprop_default(dp, "linebytes",
					  info->var.xres);
	info->fix.smem_len = PAGE_ALIGN(linebytes * info->var.yres);

	if (!strcmp(dp->parent->name, "sbus") ||
	    !strcmp(dp->parent->name, "sbi")) {
		info->fix.smem_start = op->resource[0].start;
		par->iospace = op->resource[0].flags & IORESOURCE_BITS;
	} else {
		info->fix.smem_start = op->resource[1].start;
		par->iospace = op->resource[0].flags & IORESOURCE_BITS;
	}

	par->regs = of_ioremap(&op->resource[0], 0,
			       sizeof(struct cg14_regs), "cg14 regs");
	par->clut = of_ioremap(&op->resource[0], CG14_CLUT1,
			       sizeof(struct cg14_clut), "cg14 clut");
	par->cursor = of_ioremap(&op->resource[0], CG14_CURSORREGS,
				 sizeof(struct cg14_cursor), "cg14 cursor");

	info->screen_base = of_ioremap(&op->resource[1], 0,
				       info->fix.smem_len, "cg14 ram");

	if (!par->regs || !par->clut || !par->cursor || !info->screen_base)
		goto out_unmap_regs;

	is_8mb = (((op->resource[1].end - op->resource[1].start) + 1) ==
		  (8 * 1024 * 1024));

	BUILD_BUG_ON(sizeof(par->mmap_map) != sizeof(__cg14_mmap_map));
		
	memcpy(&par->mmap_map, &__cg14_mmap_map, sizeof(par->mmap_map));

	for (i = 0; i < CG14_MMAP_ENTRIES; i++) {
		struct sbus_mmap_map *map = &par->mmap_map[i];

		if (!map->size)
			break;
		if (map->poff & 0x80000000)
			map->poff = (map->poff & 0x7fffffff) +
				(op->resource[0].start -
				 op->resource[1].start);
		if (is_8mb &&
		    map->size >= 0x100000 &&
		    map->size <= 0x400000)
			map->size *= 2;
	}

	par->mode = MDI_8_PIX;
	par->ramsize = (is_8mb ? 0x800000 : 0x400000);

	info->flags = FBINFO_DEFAULT | FBINFO_HWACCEL_YPAN;
	info->fbops = &cg14_ops;

	__cg14_reset(par);

	if (fb_alloc_cmap(&info->cmap, 256, 0))
		goto out_unmap_regs;

	fb_set_cmap(&info->cmap, info);

	cg14_init_fix(info, linebytes, dp);

	err = register_framebuffer(info);
	if (err < 0)
		goto out_dealloc_cmap;

	dev_set_drvdata(&op->dev, info);

	printk(KERN_INFO "%s: cgfourteen at %lx:%lx, %dMB\n",
	       dp->full_name,
	       par->iospace, info->fix.smem_start,
	       par->ramsize >> 20);

	return 0;

out_dealloc_cmap:
	fb_dealloc_cmap(&info->cmap);

out_unmap_regs:
	cg14_unmap_regs(op, info, par);
	framebuffer_release(info);

out_err:
	return err;
}

static int __devexit cg14_remove(struct platform_device *op)
{
	struct fb_info *info = dev_get_drvdata(&op->dev);
	struct cg14_par *par = info->par;

	unregister_framebuffer(info);
	fb_dealloc_cmap(&info->cmap);

	cg14_unmap_regs(op, info, par);

	framebuffer_release(info);

	dev_set_drvdata(&op->dev, NULL);

	return 0;
}

static const struct of_device_id cg14_match[] = {
	{
		.name = "cgfourteen",
	},
	{},
};
MODULE_DEVICE_TABLE(of, cg14_match);

static struct platform_driver cg14_driver = {
	.driver = {
		.name = "cg14",
		.owner = THIS_MODULE,
		.of_match_table = cg14_match,
	},
	.probe		= cg14_probe,
	.remove		= __devexit_p(cg14_remove),
};

static int __init cg14_init(void)
{
	if (fb_get_options("cg14fb", NULL))
		return -ENODEV;

	return platform_driver_register(&cg14_driver);
}

static void __exit cg14_exit(void)
{
	platform_driver_unregister(&cg14_driver);
}

module_init(cg14_init);
module_exit(cg14_exit);

MODULE_DESCRIPTION("framebuffer driver for CGfourteen chipsets");
MODULE_AUTHOR("David S. Miller <davem@davemloft.net>");
MODULE_VERSION("2.0");
MODULE_LICENSE("GPL");
