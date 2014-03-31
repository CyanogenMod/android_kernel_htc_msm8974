/*
 * SH7760/SH7763 LCDC Framebuffer driver.
 *
 * (c) 2006-2008 MSC Vertriebsges.m.b.H.,
 *             Manuel Lauss <mano@roarinelk.homelinux.net>
 * (c) 2008 Nobuhiro Iwamatsu <iwamatsu.nobuhiro@renesas.com>
 *
 *  This file is subject to the terms and conditions of the GNU General
 *  Public License.  See the file COPYING in the main directory of this
 *  archive for more details.
 *
 * PLEASE HAVE A LOOK AT Documentation/fb/sh7760fb.txt!
 *
 * Thanks to Siegfried Schaefer <s.schaefer at schaefer-edv.de>
 *     for his original source and testing!
 *
 * sh7760_setcolreg get from drivers/video/sh_mobile_lcdcfb.c
 */

#include <linux/completion.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/fb.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

#include <asm/sh7760fb.h>

struct sh7760fb_par {
	void __iomem *base;
	int irq;

	struct sh7760fb_platdata *pd;	

	dma_addr_t fbdma;	

	int rot;		

	u32 pseudo_palette[16];

	struct platform_device *dev;
	struct resource *ioarea;
	struct completion vsync;	
};

static irqreturn_t sh7760fb_irq(int irq, void *data)
{
	struct completion *c = data;

	complete(c);

	return IRQ_HANDLED;
}

static int wait_for_lps(struct sh7760fb_par *par, int val)
{
	int i = 100;
	while (--i && ((ioread16(par->base + LDPMMR) & 3) != val))
		msleep(1);

	if (i <= 0)
		return -ETIMEDOUT;

	return 0;
}

static int sh7760fb_blank(int blank, struct fb_info *info)
{
	struct sh7760fb_par *par = info->par;
	struct sh7760fb_platdata *pd = par->pd;
	unsigned short cntr = ioread16(par->base + LDCNTR);
	unsigned short intr = ioread16(par->base + LDINTR);
	int lps;

	if (blank == FB_BLANK_UNBLANK) {
		intr |= VINT_START;
		cntr = LDCNTR_DON2 | LDCNTR_DON;
		lps = 3;
	} else {
		intr &= ~VINT_START;
		cntr = LDCNTR_DON2;
		lps = 0;
	}

	if (pd->blank)
		pd->blank(blank);

	iowrite16(intr, par->base + LDINTR);
	iowrite16(cntr, par->base + LDCNTR);

	return wait_for_lps(par, lps);
}

static int sh7760_setcolreg (u_int regno,
	u_int red, u_int green, u_int blue,
	u_int transp, struct fb_info *info)
{
	u32 *palette = info->pseudo_palette;

	if (regno >= 16)
		return -EINVAL;

	

	red >>= 16 - info->var.red.length;
	green >>= 16 - info->var.green.length;
	blue >>= 16 - info->var.blue.length;
	transp >>= 16 - info->var.transp.length;

	palette[regno] = (red << info->var.red.offset) |
		(green << info->var.green.offset) |
		(blue << info->var.blue.offset) |
		(transp << info->var.transp.offset);

	return 0;
}

static int sh7760fb_get_color_info(struct device *dev,
				   u16 lddfr, int *bpp, int *gray)
{
	int lbpp, lgray;

	lgray = lbpp = 0;

	switch (lddfr & LDDFR_COLOR_MASK) {
	case LDDFR_1BPP_MONO:
		lgray = 1;
		lbpp = 1;
		break;
	case LDDFR_2BPP_MONO:
		lgray = 1;
		lbpp = 2;
		break;
	case LDDFR_4BPP_MONO:
		lgray = 1;
	case LDDFR_4BPP:
		lbpp = 4;
		break;
	case LDDFR_6BPP_MONO:
		lgray = 1;
	case LDDFR_8BPP:
		lbpp = 8;
		break;
	case LDDFR_16BPP_RGB555:
	case LDDFR_16BPP_RGB565:
		lbpp = 16;
		lgray = 0;
		break;
	default:
		dev_dbg(dev, "unsupported LDDFR bit depth.\n");
		return -EINVAL;
	}

	if (bpp)
		*bpp = lbpp;
	if (gray)
		*gray = lgray;

	return 0;
}

static int sh7760fb_check_var(struct fb_var_screeninfo *var,
			      struct fb_info *info)
{
	struct fb_fix_screeninfo *fix = &info->fix;
	struct sh7760fb_par *par = info->par;
	int ret, bpp;

	
	ret = sh7760fb_get_color_info(info->dev, par->pd->lddfr, &bpp, NULL);
	if (ret)
		return ret;

	var->bits_per_pixel = bpp;

	if ((var->grayscale) && (var->bits_per_pixel == 1))
		fix->visual = FB_VISUAL_MONO10;
	else if (var->bits_per_pixel >= 15)
		fix->visual = FB_VISUAL_TRUECOLOR;
	else
		fix->visual = FB_VISUAL_PSEUDOCOLOR;

	
	return 0;
}

static int sh7760fb_set_par(struct fb_info *info)
{
	struct sh7760fb_par *par = info->par;
	struct fb_videomode *vm = par->pd->def_mode;
	unsigned long sbase, dstn_off, ldsarl, stride;
	unsigned short hsynp, hsynw, htcn, hdcn;
	unsigned short vsynp, vsynw, vtln, vdln;
	unsigned short lddfr, ldmtr;
	int ret, bpp, gray;

	par->rot = par->pd->rotate;

	
	if (par->rot && (vm->xres > 320)) {
		dev_dbg(info->dev, "rotation disabled due to display size\n");
		par->rot = 0;
	}

	
	hsynp = vm->right_margin + vm->xres;
	hsynw = vm->hsync_len;
	htcn = vm->left_margin + hsynp + hsynw;
	hdcn = vm->xres;
	vsynp = vm->lower_margin + vm->yres;
	vsynw = vm->vsync_len;
	vtln = vm->upper_margin + vsynp + vsynw;
	vdln = vm->yres;

	
	ret = sh7760fb_get_color_info(info->dev, par->pd->lddfr, &bpp, &gray);
	if (ret)
		return ret;

	dev_dbg(info->dev, "%dx%d %dbpp %s (orientation %s)\n", hdcn,
		vdln, bpp, gray ? "grayscale" : "color",
		par->rot ? "rotated" : "normal");

#ifdef CONFIG_CPU_LITTLE_ENDIAN
	lddfr = par->pd->lddfr | (1 << 8);
#else
	lddfr = par->pd->lddfr & ~(1 << 8);
#endif

	ldmtr = par->pd->ldmtr;

	if (!(vm->sync & FB_SYNC_HOR_HIGH_ACT))
		ldmtr |= LDMTR_CL1POL;
	if (!(vm->sync & FB_SYNC_VERT_HIGH_ACT))
		ldmtr |= LDMTR_FLMPOL;

	
	sh7760fb_blank(FB_BLANK_POWERDOWN, info);

	iowrite16(par->pd->ldickr, par->base + LDICKR);	
	iowrite16(ldmtr, par->base + LDMTR);	
	iowrite16(lddfr, par->base + LDDFR);	
	iowrite16((par->rot ? 1 << 13 : 0), par->base + LDSMR);	
	iowrite16(par->pd->ldpmmr, par->base + LDPMMR);	
	iowrite16(par->pd->ldpspr, par->base + LDPSPR);	

	
	iowrite16(((htcn >> 3) - 1) | (((hdcn >> 3) - 1) << 8),
		  par->base + LDHCNR);
	iowrite16(vdln - 1, par->base + LDVDLNR);
	iowrite16(vtln - 1, par->base + LDVTLNR);
	
	iowrite16((vsynp - 1) | ((vsynw - 1) << 12), par->base + LDVSYNR);
	iowrite16(((hsynp >> 3) - 1) | (((hsynw >> 3) - 1) << 12),
		  par->base + LDHSYNR);
	
	iowrite16(par->pd->ldaclnr, par->base + LDACLNR);

	stride = (par->rot) ? vtln : hdcn;
	if (!gray)
		stride *= (bpp + 7) >> 3;
	else {
		if (bpp == 1)
			stride >>= 3;
		else if (bpp == 2)
			stride >>= 2;
		else if (bpp == 4)
			stride >>= 1;
		
	}

	
	if (par->rot) {
		unsigned long bit = 1 << 31;
		while (bit) {
			if (stride & bit)
				break;
			bit >>= 1;
		}
		if (stride & ~bit)
			stride = bit << 1;	
	}
	iowrite16(stride, par->base + LDLAOR);

	
	sbase = (unsigned long)par->fbdma;
	if (par->rot)
		sbase += (hdcn - 1) * stride;

	iowrite32(sbase, par->base + LDSARU);

	if (((ldmtr & 0x003f) >= LDMTR_DSTN_MONO_8) &&
	    ((ldmtr & 0x003f) <= LDMTR_DSTN_COLOR_16)) {

		dev_dbg(info->dev, " ***** DSTN untested! *****\n");

		dstn_off = stride;
		if (par->rot)
			dstn_off *= hdcn >> 1;
		else
			dstn_off *= vdln >> 1;

		ldsarl = sbase + dstn_off;
	} else
		ldsarl = 0;

	iowrite32(ldsarl, par->base + LDSARL);	

	info->fix.line_length = stride;

	sh7760fb_check_var(&info->var, info);

	sh7760fb_blank(FB_BLANK_UNBLANK, info);	

	dev_dbg(info->dev, "hdcn  : %6d htcn  : %6d\n", hdcn, htcn);
	dev_dbg(info->dev, "hsynw : %6d hsynp : %6d\n", hsynw, hsynp);
	dev_dbg(info->dev, "vdln  : %6d vtln  : %6d\n", vdln, vtln);
	dev_dbg(info->dev, "vsynw : %6d vsynp : %6d\n", vsynw, vsynp);
	dev_dbg(info->dev, "clksrc: %6d clkdiv: %6d\n",
		(par->pd->ldickr >> 12) & 3, par->pd->ldickr & 0x1f);
	dev_dbg(info->dev, "ldpmmr: 0x%04x ldpspr: 0x%04x\n", par->pd->ldpmmr,
		par->pd->ldpspr);
	dev_dbg(info->dev, "ldmtr : 0x%04x lddfr : 0x%04x\n", ldmtr, lddfr);
	dev_dbg(info->dev, "ldlaor: %ld\n", stride);
	dev_dbg(info->dev, "ldsaru: 0x%08lx ldsarl: 0x%08lx\n", sbase, ldsarl);

	return 0;
}

static struct fb_ops sh7760fb_ops = {
	.owner = THIS_MODULE,
	.fb_blank = sh7760fb_blank,
	.fb_check_var = sh7760fb_check_var,
	.fb_setcolreg = sh7760_setcolreg,
	.fb_set_par = sh7760fb_set_par,
	.fb_fillrect = cfb_fillrect,
	.fb_copyarea = cfb_copyarea,
	.fb_imageblit = cfb_imageblit,
};

static void sh7760fb_free_mem(struct fb_info *info)
{
	struct sh7760fb_par *par = info->par;

	if (!info->screen_base)
		return;

	dma_free_coherent(info->dev, info->screen_size,
			  info->screen_base, par->fbdma);

	par->fbdma = 0;
	info->screen_base = NULL;
	info->screen_size = 0;
}

static int sh7760fb_alloc_mem(struct fb_info *info)
{
	struct sh7760fb_par *par = info->par;
	void *fbmem;
	unsigned long vram;
	int ret, bpp;

	if (info->screen_base)
		return 0;

	
	ret = sh7760fb_get_color_info(info->dev, par->pd->lddfr, &bpp, NULL);
	if (ret) {
		printk(KERN_ERR "colinfo\n");
		return ret;
	}


	vram = info->var.xres * info->var.yres;
	if (info->var.grayscale) {
		if (bpp == 1)
			vram >>= 3;
		else if (bpp == 2)
			vram >>= 2;
		else if (bpp == 4)
			vram >>= 1;
	} else if (bpp > 8)
		vram *= 2;
	if ((vram < 1) || (vram > 1024 * 2048)) {
		dev_dbg(info->dev, "too much VRAM required. Check settings\n");
		return -ENODEV;
	}

	if (vram < PAGE_SIZE)
		vram = PAGE_SIZE;

	fbmem = dma_alloc_coherent(info->dev, vram, &par->fbdma, GFP_KERNEL);

	if (!fbmem)
		return -ENOMEM;

	if ((par->fbdma & SH7760FB_DMA_MASK) != SH7760FB_DMA_MASK) {
		sh7760fb_free_mem(info);
		dev_err(info->dev, "kernel gave me memory at 0x%08lx, which is"
			"unusable for the LCDC\n", (unsigned long)par->fbdma);
		return -ENOMEM;
	}

	info->screen_base = fbmem;
	info->screen_size = vram;
	info->fix.smem_start = (unsigned long)info->screen_base;
	info->fix.smem_len = info->screen_size;

	return 0;
}

static int __devinit sh7760fb_probe(struct platform_device *pdev)
{
	struct fb_info *info;
	struct resource *res;
	struct sh7760fb_par *par;
	int ret;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (unlikely(res == NULL)) {
		dev_err(&pdev->dev, "invalid resource\n");
		return -EINVAL;
	}

	info = framebuffer_alloc(sizeof(struct sh7760fb_par), &pdev->dev);
	if (!info)
		return -ENOMEM;

	par = info->par;
	par->dev = pdev;

	par->pd = pdev->dev.platform_data;
	if (!par->pd) {
		dev_dbg(info->dev, "no display setup data!\n");
		ret = -ENODEV;
		goto out_fb;
	}

	par->ioarea = request_mem_region(res->start,
					 resource_size(res), pdev->name);
	if (!par->ioarea) {
		dev_err(&pdev->dev, "mmio area busy\n");
		ret = -EBUSY;
		goto out_fb;
	}

	par->base = ioremap_nocache(res->start, resource_size(res));
	if (!par->base) {
		dev_err(&pdev->dev, "cannot remap\n");
		ret = -ENODEV;
		goto out_res;
	}

	iowrite16(0, par->base + LDINTR);	
	par->irq = platform_get_irq(pdev, 0);
	if (par->irq >= 0) {
		ret = request_irq(par->irq, sh7760fb_irq, 0,
				  "sh7760-lcdc", &par->vsync);
		if (ret) {
			dev_err(&pdev->dev, "cannot grab IRQ\n");
			par->irq = -ENXIO;
		} else
			disable_irq_nosync(par->irq);
	}

	fb_videomode_to_var(&info->var, par->pd->def_mode);

	ret = sh7760fb_alloc_mem(info);
	if (ret) {
		dev_dbg(info->dev, "framebuffer memory allocation failed!\n");
		goto out_unmap;
	}

	info->pseudo_palette = par->pseudo_palette;

	
	info->var.red.offset = 11;
	info->var.red.length = 5;
	info->var.red.msb_right = 0;

	info->var.green.offset = 5;
	info->var.green.length = 6;
	info->var.green.msb_right = 0;

	info->var.blue.offset = 0;
	info->var.blue.length = 5;
	info->var.blue.msb_right = 0;

	info->var.transp.offset = 0;
	info->var.transp.length = 0;
	info->var.transp.msb_right = 0;

	strcpy(info->fix.id, "sh7760-lcdc");

	iowrite16(LDCNTR_DON2, par->base + LDCNTR);
	info->fbops = &sh7760fb_ops;

	ret = fb_alloc_cmap(&info->cmap, 256, 0);
	if (ret) {
		dev_dbg(info->dev, "Unable to allocate cmap memory\n");
		goto out_mem;
	}

	ret = register_framebuffer(info);
	if (ret < 0) {
		dev_dbg(info->dev, "cannot register fb!\n");
		goto out_cmap;
	}
	platform_set_drvdata(pdev, info);

	printk(KERN_INFO "%s: memory at phys 0x%08lx-0x%08lx, size %ld KiB\n",
	       pdev->name,
	       (unsigned long)par->fbdma,
	       (unsigned long)(par->fbdma + info->screen_size - 1),
	       info->screen_size >> 10);

	return 0;

out_cmap:
	sh7760fb_blank(FB_BLANK_POWERDOWN, info);
	fb_dealloc_cmap(&info->cmap);
out_mem:
	sh7760fb_free_mem(info);
out_unmap:
	if (par->irq >= 0)
		free_irq(par->irq, &par->vsync);
	iounmap(par->base);
out_res:
	release_mem_region(res->start, resource_size(res));
out_fb:
	framebuffer_release(info);
	return ret;
}

static int __devexit sh7760fb_remove(struct platform_device *dev)
{
	struct fb_info *info = platform_get_drvdata(dev);
	struct sh7760fb_par *par = info->par;

	sh7760fb_blank(FB_BLANK_POWERDOWN, info);
	unregister_framebuffer(info);
	fb_dealloc_cmap(&info->cmap);
	sh7760fb_free_mem(info);
	if (par->irq >= 0)
		free_irq(par->irq, par);
	iounmap(par->base);
	release_mem_region(par->ioarea->start, resource_size(par->ioarea));
	framebuffer_release(info);
	platform_set_drvdata(dev, NULL);

	return 0;
}

static struct platform_driver sh7760_lcdc_driver = {
	.driver = {
		   .name = "sh7760-lcdc",
		   .owner = THIS_MODULE,
		   },
	.probe = sh7760fb_probe,
	.remove = __devexit_p(sh7760fb_remove),
};

module_platform_driver(sh7760_lcdc_driver);

MODULE_AUTHOR("Nobuhiro Iwamatsu, Manuel Lauss");
MODULE_DESCRIPTION("FBdev for SH7760/63 integrated LCD Controller");
MODULE_LICENSE("GPL");
