/*
 *  linux/drivers/video/tgafb.c -- DEC 21030 TGA frame buffer device
 *
 *	Copyright (C) 1995 Jay Estabrook
 *	Copyright (C) 1997 Geert Uytterhoeven
 *	Copyright (C) 1999,2000 Martin Lucina, Tom Zerucha
 *	Copyright (C) 2002 Richard Henderson
 *	Copyright (C) 2006, 2007  Maciej W. Rozycki
 *
 *  This file is subject to the terms and conditions of the GNU General Public
 *  License. See the file COPYING in the main directory of this archive for
 *  more details.
 */

#include <linux/bitrev.h>
#include <linux/compiler.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/fb.h>
#include <linux/init.h>
#include <linux/ioport.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/selection.h>
#include <linux/string.h>
#include <linux/tc.h>

#include <asm/io.h>

#include <video/tgafb.h>

#ifdef CONFIG_PCI
#define TGA_BUS_PCI(dev) (dev->bus == &pci_bus_type)
#else
#define TGA_BUS_PCI(dev) 0
#endif

#ifdef CONFIG_TC
#define TGA_BUS_TC(dev) (dev->bus == &tc_bus_type)
#else
#define TGA_BUS_TC(dev) 0
#endif


static int tgafb_check_var(struct fb_var_screeninfo *, struct fb_info *);
static int tgafb_set_par(struct fb_info *);
static void tgafb_set_pll(struct tga_par *, int);
static int tgafb_setcolreg(unsigned, unsigned, unsigned, unsigned,
			   unsigned, struct fb_info *);
static int tgafb_blank(int, struct fb_info *);
static void tgafb_init_fix(struct fb_info *);

static void tgafb_imageblit(struct fb_info *, const struct fb_image *);
static void tgafb_fillrect(struct fb_info *, const struct fb_fillrect *);
static void tgafb_copyarea(struct fb_info *, const struct fb_copyarea *);
static int tgafb_pan_display(struct fb_var_screeninfo *var, struct fb_info *info);

static int __devinit tgafb_register(struct device *dev);
static void __devexit tgafb_unregister(struct device *dev);

static const char *mode_option;
static const char *mode_option_pci = "640x480@60";
static const char *mode_option_tc = "1280x1024@72";


static struct pci_driver tgafb_pci_driver;
static struct tc_driver tgafb_tc_driver;


static struct fb_ops tgafb_ops = {
	.owner			= THIS_MODULE,
	.fb_check_var		= tgafb_check_var,
	.fb_set_par		= tgafb_set_par,
	.fb_setcolreg		= tgafb_setcolreg,
	.fb_blank		= tgafb_blank,
	.fb_pan_display		= tgafb_pan_display,
	.fb_fillrect		= tgafb_fillrect,
	.fb_copyarea		= tgafb_copyarea,
	.fb_imageblit		= tgafb_imageblit,
};


#ifdef CONFIG_PCI
static int __devinit tgafb_pci_register(struct pci_dev *,
					const struct pci_device_id *);
static void __devexit tgafb_pci_unregister(struct pci_dev *);

static struct pci_device_id const tgafb_pci_table[] = {
	{ PCI_DEVICE(PCI_VENDOR_ID_DEC, PCI_DEVICE_ID_DEC_TGA) },
	{ }
};
MODULE_DEVICE_TABLE(pci, tgafb_pci_table);

static struct pci_driver tgafb_pci_driver = {
	.name			= "tgafb",
	.id_table		= tgafb_pci_table,
	.probe			= tgafb_pci_register,
	.remove			= __devexit_p(tgafb_pci_unregister),
};

static int __devinit
tgafb_pci_register(struct pci_dev *pdev, const struct pci_device_id *ent)
{
	return tgafb_register(&pdev->dev);
}

static void __devexit
tgafb_pci_unregister(struct pci_dev *pdev)
{
	tgafb_unregister(&pdev->dev);
}
#endif 

#ifdef CONFIG_TC
static int __devinit tgafb_tc_register(struct device *);
static int __devexit tgafb_tc_unregister(struct device *);

static struct tc_device_id const tgafb_tc_table[] = {
	{ "DEC     ", "PMAGD-AA" },
	{ "DEC     ", "PMAGD   " },
	{ }
};
MODULE_DEVICE_TABLE(tc, tgafb_tc_table);

static struct tc_driver tgafb_tc_driver = {
	.id_table		= tgafb_tc_table,
	.driver			= {
		.name		= "tgafb",
		.bus		= &tc_bus_type,
		.probe		= tgafb_tc_register,
		.remove		= __devexit_p(tgafb_tc_unregister),
	},
};

static int __devinit
tgafb_tc_register(struct device *dev)
{
	int status = tgafb_register(dev);
	if (!status)
		get_device(dev);
	return status;
}

static int __devexit
tgafb_tc_unregister(struct device *dev)
{
	put_device(dev);
	tgafb_unregister(dev);
	return 0;
}
#endif 


static int
tgafb_check_var(struct fb_var_screeninfo *var, struct fb_info *info)
{
	struct tga_par *par = (struct tga_par *)info->par;

	if (par->tga_type == TGA_TYPE_8PLANE) {
		if (var->bits_per_pixel != 8)
			return -EINVAL;
	} else {
		if (var->bits_per_pixel != 32)
			return -EINVAL;
	}
	var->red.length = var->green.length = var->blue.length = 8;
	if (var->bits_per_pixel == 32) {
		var->red.offset = 16;
		var->green.offset = 8;
		var->blue.offset = 0;
	}

	if (var->xres_virtual != var->xres || var->yres_virtual != var->yres)
		return -EINVAL;
	if (var->nonstd)
		return -EINVAL;
	if (1000000000 / var->pixclock > TGA_PLL_MAX_FREQ)
		return -EINVAL;
	if ((var->vmode & FB_VMODE_MASK) != FB_VMODE_NONINTERLACED)
		return -EINVAL;

	if (var->xres * (par->tga_type == TGA_TYPE_8PLANE ? 1 : 4) % 64)
		return -EINVAL;

	return 0;
}

static int
tgafb_set_par(struct fb_info *info)
{
	static unsigned int const deep_presets[4] = {
		0x00004000,
		0x0000440d,
		0xffffffff,
		0x0000441d
	};
	static unsigned int const rasterop_presets[4] = {
		0x00000003,
		0x00000303,
		0xffffffff,
		0x00000303
	};
	static unsigned int const mode_presets[4] = {
		0x00000000,
		0x00000300,
		0xffffffff,
		0x00000300
	};
	static unsigned int const base_addr_presets[4] = {
		0x00000000,
		0x00000001,
		0xffffffff,
		0x00000001
	};

	struct tga_par *par = (struct tga_par *) info->par;
	int tga_bus_pci = TGA_BUS_PCI(par->dev);
	int tga_bus_tc = TGA_BUS_TC(par->dev);
	u32 htimings, vtimings, pll_freq;
	u8 tga_type;
	int i;

	
	htimings = (((info->var.xres/4) & TGA_HORIZ_ACT_LSB)
		    | (((info->var.xres/4) & 0x600 << 19) & TGA_HORIZ_ACT_MSB));
	vtimings = (info->var.yres & TGA_VERT_ACTIVE);
	htimings |= ((info->var.right_margin/4) << 9) & TGA_HORIZ_FP;
	vtimings |= (info->var.lower_margin << 11) & TGA_VERT_FP;
	htimings |= ((info->var.hsync_len/4) << 14) & TGA_HORIZ_SYNC;
	vtimings |= (info->var.vsync_len << 16) & TGA_VERT_SYNC;
	htimings |= ((info->var.left_margin/4) << 21) & TGA_HORIZ_BP;
	vtimings |= (info->var.upper_margin << 22) & TGA_VERT_BP;

	if (info->var.sync & FB_SYNC_HOR_HIGH_ACT)
		htimings |= TGA_HORIZ_POLARITY;
	if (info->var.sync & FB_SYNC_VERT_HIGH_ACT)
		vtimings |= TGA_VERT_POLARITY;

	par->htimings = htimings;
	par->vtimings = vtimings;

	par->sync_on_green = !!(info->var.sync & FB_SYNC_ON_GREEN);

	
	par->xres = info->var.xres;
	par->yres = info->var.yres;
	par->pll_freq = pll_freq = 1000000000 / info->var.pixclock;
	par->bits_per_pixel = info->var.bits_per_pixel;

	tga_type = par->tga_type;

	
	TGA_WRITE_REG(par, TGA_VALID_VIDEO | TGA_VALID_BLANK, TGA_VALID_REG);

	
	while (TGA_READ_REG(par, TGA_CMD_STAT_REG) & 1) 
		continue;
	mb();
	TGA_WRITE_REG(par, deep_presets[tga_type] |
			   (par->sync_on_green ? 0x0 : 0x00010000),
		      TGA_DEEP_REG);
	while (TGA_READ_REG(par, TGA_CMD_STAT_REG) & 1) 
		continue;
	mb();

	
	TGA_WRITE_REG(par, rasterop_presets[tga_type], TGA_RASTEROP_REG);
	TGA_WRITE_REG(par, mode_presets[tga_type], TGA_MODE_REG);
	TGA_WRITE_REG(par, base_addr_presets[tga_type], TGA_BASE_ADDR_REG);

	
	tgafb_set_pll(par, pll_freq);

	
	TGA_WRITE_REG(par, 0xffffffff, TGA_PLANEMASK_REG);
	TGA_WRITE_REG(par, 0xffffffff, TGA_PIXELMASK_REG);

	
	TGA_WRITE_REG(par, htimings, TGA_HORIZ_REG);
	TGA_WRITE_REG(par, vtimings, TGA_VERT_REG);

	
	if (tga_type == TGA_TYPE_8PLANE && tga_bus_pci) {

		
		BT485_WRITE(par, 0xa2 | (par->sync_on_green ? 0x8 : 0x0),
			    BT485_CMD_0);
		BT485_WRITE(par, 0x01, BT485_ADDR_PAL_WRITE);
		BT485_WRITE(par, 0x14, BT485_CMD_3); 
		BT485_WRITE(par, 0x40, BT485_CMD_1);
		BT485_WRITE(par, 0x20, BT485_CMD_2); 
		BT485_WRITE(par, 0xff, BT485_PIXEL_MASK);

		
		BT485_WRITE(par, 0x00, BT485_ADDR_PAL_WRITE);
		TGA_WRITE_REG(par, BT485_DATA_PAL, TGA_RAMDAC_SETUP_REG);

		for (i = 0; i < 256 * 3; i += 4) {
			TGA_WRITE_REG(par, 0x55 | (BT485_DATA_PAL << 8),
				      TGA_RAMDAC_REG);
			TGA_WRITE_REG(par, 0x00 | (BT485_DATA_PAL << 8),
				      TGA_RAMDAC_REG);
			TGA_WRITE_REG(par, 0x00 | (BT485_DATA_PAL << 8),
				      TGA_RAMDAC_REG);
			TGA_WRITE_REG(par, 0x00 | (BT485_DATA_PAL << 8),
				      TGA_RAMDAC_REG);
		}

	} else if (tga_type == TGA_TYPE_8PLANE && tga_bus_tc) {

		
		BT459_WRITE(par, BT459_REG_ACC, BT459_CMD_REG_0, 0x40);
		BT459_WRITE(par, BT459_REG_ACC, BT459_CMD_REG_1, 0x00);
		BT459_WRITE(par, BT459_REG_ACC, BT459_CMD_REG_2,
			    (par->sync_on_green ? 0xc0 : 0x40));

		BT459_WRITE(par, BT459_REG_ACC, BT459_CUR_CMD_REG, 0x00);

		
		BT459_LOAD_ADDR(par, 0x0000);
		TGA_WRITE_REG(par, BT459_PALETTE << 2, TGA_RAMDAC_SETUP_REG);

		for (i = 0; i < 256 * 3; i += 4) {
			TGA_WRITE_REG(par, 0x55, TGA_RAMDAC_REG);
			TGA_WRITE_REG(par, 0x00, TGA_RAMDAC_REG);
			TGA_WRITE_REG(par, 0x00, TGA_RAMDAC_REG);
			TGA_WRITE_REG(par, 0x00, TGA_RAMDAC_REG);
		}

	} else { 

		
		BT463_WRITE(par, BT463_REG_ACC, BT463_CMD_REG_0, 0x40);
		BT463_WRITE(par, BT463_REG_ACC, BT463_CMD_REG_1, 0x08);
		BT463_WRITE(par, BT463_REG_ACC, BT463_CMD_REG_2,
			    (par->sync_on_green ? 0xc0 : 0x40));

		BT463_WRITE(par, BT463_REG_ACC, BT463_READ_MASK_0, 0xff);
		BT463_WRITE(par, BT463_REG_ACC, BT463_READ_MASK_1, 0xff);
		BT463_WRITE(par, BT463_REG_ACC, BT463_READ_MASK_2, 0xff);
		BT463_WRITE(par, BT463_REG_ACC, BT463_READ_MASK_3, 0x0f);

		BT463_WRITE(par, BT463_REG_ACC, BT463_BLINK_MASK_0, 0x00);
		BT463_WRITE(par, BT463_REG_ACC, BT463_BLINK_MASK_1, 0x00);
		BT463_WRITE(par, BT463_REG_ACC, BT463_BLINK_MASK_2, 0x00);
		BT463_WRITE(par, BT463_REG_ACC, BT463_BLINK_MASK_3, 0x00);

		
		BT463_LOAD_ADDR(par, 0x0000);
		TGA_WRITE_REG(par, BT463_PALETTE << 2, TGA_RAMDAC_SETUP_REG);

#ifdef CONFIG_HW_CONSOLE
		for (i = 0; i < 16; i++) {
			int j = color_table[i];

			TGA_WRITE_REG(par, default_red[j], TGA_RAMDAC_REG);
			TGA_WRITE_REG(par, default_grn[j], TGA_RAMDAC_REG);
			TGA_WRITE_REG(par, default_blu[j], TGA_RAMDAC_REG);
		}
		for (i = 0; i < 512 * 3; i += 4) {
#else
		for (i = 0; i < 528 * 3; i += 4) {
#endif
			TGA_WRITE_REG(par, 0x55, TGA_RAMDAC_REG);
			TGA_WRITE_REG(par, 0x00, TGA_RAMDAC_REG);
			TGA_WRITE_REG(par, 0x00, TGA_RAMDAC_REG);
			TGA_WRITE_REG(par, 0x00, TGA_RAMDAC_REG);
		}

		
		while (!(TGA_READ_REG(par, TGA_INTR_STAT_REG) & 0x01))
			continue;
		TGA_WRITE_REG(par, 0x01, TGA_INTR_STAT_REG);
		mb();
		while (!(TGA_READ_REG(par, TGA_INTR_STAT_REG) & 0x01))
			continue;
		TGA_WRITE_REG(par, 0x01, TGA_INTR_STAT_REG);

		BT463_LOAD_ADDR(par, BT463_WINDOW_TYPE_BASE);
		TGA_WRITE_REG(par, BT463_REG_ACC << 2, TGA_RAMDAC_SETUP_REG);

		for (i = 0; i < 16; i++) {
			TGA_WRITE_REG(par, 0x00, TGA_RAMDAC_REG);
			TGA_WRITE_REG(par, 0x01, TGA_RAMDAC_REG);
			TGA_WRITE_REG(par, 0x00, TGA_RAMDAC_REG);
		}

	}

	
	TGA_WRITE_REG(par, TGA_VALID_VIDEO, TGA_VALID_REG);

	return 0;
}

#define DIFFCHECK(X)							  \
do {									  \
	if (m <= 0x3f) {						  \
		int delta = f - (TGA_PLL_BASE_FREQ * (X)) / (r << shift); \
		if (delta < 0)						  \
			delta = -delta;					  \
		if (delta < min_diff)					  \
			min_diff = delta, vm = m, va = a, vr = r;	  \
	}								  \
} while (0)

static void
tgafb_set_pll(struct tga_par *par, int f)
{
	int n, shift, base, min_diff, target;
	int r,a,m,vm = 34, va = 1, vr = 30;

	for (r = 0 ; r < 12 ; r++)
		TGA_WRITE_REG(par, !r, TGA_CLOCK_REG);

	if (f > TGA_PLL_MAX_FREQ)
		f = TGA_PLL_MAX_FREQ;

	if (f >= TGA_PLL_MAX_FREQ / 2)
		shift = 0;
	else if (f >= TGA_PLL_MAX_FREQ / 4)
		shift = 1;
	else
		shift = 2;

	TGA_WRITE_REG(par, shift & 1, TGA_CLOCK_REG);
	TGA_WRITE_REG(par, shift >> 1, TGA_CLOCK_REG);

	for (r = 0 ; r < 10 ; r++)
		TGA_WRITE_REG(par, 0, TGA_CLOCK_REG);

	if (f <= 120000) {
		TGA_WRITE_REG(par, 0, TGA_CLOCK_REG);
		TGA_WRITE_REG(par, 0, TGA_CLOCK_REG);
	}
	else if (f <= 200000) {
		TGA_WRITE_REG(par, 1, TGA_CLOCK_REG);
		TGA_WRITE_REG(par, 0, TGA_CLOCK_REG);
	}
	else {
		TGA_WRITE_REG(par, 0, TGA_CLOCK_REG);
		TGA_WRITE_REG(par, 1, TGA_CLOCK_REG);
	}

	TGA_WRITE_REG(par, 1, TGA_CLOCK_REG);
	TGA_WRITE_REG(par, 0, TGA_CLOCK_REG);
	TGA_WRITE_REG(par, 0, TGA_CLOCK_REG);
	TGA_WRITE_REG(par, 1, TGA_CLOCK_REG);
	TGA_WRITE_REG(par, 0, TGA_CLOCK_REG);
	TGA_WRITE_REG(par, 1, TGA_CLOCK_REG);

	target = (f << shift) / TGA_PLL_BASE_FREQ;
	min_diff = TGA_PLL_MAX_FREQ;

	r = 7 / target;
	if (!r) r = 1;

	base = target * r;
	while (base < 449) {
		for (n = base < 7 ? 7 : base; n < base + target && n < 449; n++) {
			m = ((n + 3) / 7) - 1;
			a = 0;
			DIFFCHECK((m + 1) * 7);
			m++;
			DIFFCHECK((m + 1) * 7);
			m = (n / 6) - 1;
			if ((a = n % 6))
				DIFFCHECK(n);
		}
		r++;
		base += target;
	}

	vr--;

	for (r = 0; r < 8; r++)
		TGA_WRITE_REG(par, (vm >> r) & 1, TGA_CLOCK_REG);
	for (r = 0; r < 8 ; r++)
		TGA_WRITE_REG(par, (va >> r) & 1, TGA_CLOCK_REG);
	for (r = 0; r < 7 ; r++)
		TGA_WRITE_REG(par, (vr >> r) & 1, TGA_CLOCK_REG);
	TGA_WRITE_REG(par, ((vr >> 7) & 1)|2, TGA_CLOCK_REG);
}


static int
tgafb_setcolreg(unsigned regno, unsigned red, unsigned green, unsigned blue,
		unsigned transp, struct fb_info *info)
{
	struct tga_par *par = (struct tga_par *) info->par;
	int tga_bus_pci = TGA_BUS_PCI(par->dev);
	int tga_bus_tc = TGA_BUS_TC(par->dev);

	if (regno > 255)
		return 1;
	red >>= 8;
	green >>= 8;
	blue >>= 8;

	if (par->tga_type == TGA_TYPE_8PLANE && tga_bus_pci) {
		BT485_WRITE(par, regno, BT485_ADDR_PAL_WRITE);
		TGA_WRITE_REG(par, BT485_DATA_PAL, TGA_RAMDAC_SETUP_REG);
		TGA_WRITE_REG(par, red|(BT485_DATA_PAL<<8),TGA_RAMDAC_REG);
		TGA_WRITE_REG(par, green|(BT485_DATA_PAL<<8),TGA_RAMDAC_REG);
		TGA_WRITE_REG(par, blue|(BT485_DATA_PAL<<8),TGA_RAMDAC_REG);
	} else if (par->tga_type == TGA_TYPE_8PLANE && tga_bus_tc) {
		BT459_LOAD_ADDR(par, regno);
		TGA_WRITE_REG(par, BT459_PALETTE << 2, TGA_RAMDAC_SETUP_REG);
		TGA_WRITE_REG(par, red, TGA_RAMDAC_REG);
		TGA_WRITE_REG(par, green, TGA_RAMDAC_REG);
		TGA_WRITE_REG(par, blue, TGA_RAMDAC_REG);
	} else {
		if (regno < 16) {
			u32 value = (regno << 16) | (regno << 8) | regno;
			((u32 *)info->pseudo_palette)[regno] = value;
		}
		BT463_LOAD_ADDR(par, regno);
		TGA_WRITE_REG(par, BT463_PALETTE << 2, TGA_RAMDAC_SETUP_REG);
		TGA_WRITE_REG(par, red, TGA_RAMDAC_REG);
		TGA_WRITE_REG(par, green, TGA_RAMDAC_REG);
		TGA_WRITE_REG(par, blue, TGA_RAMDAC_REG);
	}

	return 0;
}


static int
tgafb_blank(int blank, struct fb_info *info)
{
	struct tga_par *par = (struct tga_par *) info->par;
	u32 vhcr, vvcr, vvvr;
	unsigned long flags;

	local_irq_save(flags);

	vhcr = TGA_READ_REG(par, TGA_HORIZ_REG);
	vvcr = TGA_READ_REG(par, TGA_VERT_REG);
	vvvr = TGA_READ_REG(par, TGA_VALID_REG);
	vvvr &= ~(TGA_VALID_VIDEO | TGA_VALID_BLANK);

	switch (blank) {
	case FB_BLANK_UNBLANK: 
		if (par->vesa_blanked) {
			TGA_WRITE_REG(par, vhcr & 0xbfffffff, TGA_HORIZ_REG);
			TGA_WRITE_REG(par, vvcr & 0xbfffffff, TGA_VERT_REG);
			par->vesa_blanked = 0;
		}
		TGA_WRITE_REG(par, vvvr | TGA_VALID_VIDEO, TGA_VALID_REG);
		break;

	case FB_BLANK_NORMAL: 
		TGA_WRITE_REG(par, vvvr | TGA_VALID_VIDEO | TGA_VALID_BLANK,
			      TGA_VALID_REG);
		break;

	case FB_BLANK_VSYNC_SUSPEND: 
		TGA_WRITE_REG(par, vvcr | 0x40000000, TGA_VERT_REG);
		TGA_WRITE_REG(par, vvvr | TGA_VALID_BLANK, TGA_VALID_REG);
		par->vesa_blanked = 1;
		break;

	case FB_BLANK_HSYNC_SUSPEND: 
		TGA_WRITE_REG(par, vhcr | 0x40000000, TGA_HORIZ_REG);
		TGA_WRITE_REG(par, vvvr | TGA_VALID_BLANK, TGA_VALID_REG);
		par->vesa_blanked = 1;
		break;

	case FB_BLANK_POWERDOWN: 
		TGA_WRITE_REG(par, vhcr | 0x40000000, TGA_HORIZ_REG);
		TGA_WRITE_REG(par, vvcr | 0x40000000, TGA_VERT_REG);
		TGA_WRITE_REG(par, vvvr | TGA_VALID_BLANK, TGA_VALID_REG);
		par->vesa_blanked = 1;
		break;
	}

	local_irq_restore(flags);
	return 0;
}



static void
tgafb_mono_imageblit(struct fb_info *info, const struct fb_image *image)
{
	struct tga_par *par = (struct tga_par *) info->par;
	u32 fgcolor, bgcolor, dx, dy, width, height, vxres, vyres, pixelmask;
	unsigned long rincr, line_length, shift, pos, is8bpp;
	unsigned long i, j;
	const unsigned char *data;
	void __iomem *regs_base;
	void __iomem *fb_base;

	is8bpp = info->var.bits_per_pixel == 8;

	dx = image->dx;
	dy = image->dy;
	width = image->width;
	height = image->height;
	vxres = info->var.xres_virtual;
	vyres = info->var.yres_virtual;
	line_length = info->fix.line_length;
	rincr = (width + 7) / 8;

	
	if (unlikely(width == 0))
		return;
	
	if (dx > vxres || dy > vyres)
		return;
	if (dx + width > vxres)
		width = vxres - dx;
	if (dy + height > vyres)
		height = vyres - dy;

	regs_base = par->tga_regs_base;
	fb_base = par->tga_fb_base;

	
	fgcolor = image->fg_color;
	bgcolor = image->bg_color;
	if (is8bpp) {
		fgcolor |= fgcolor << 8;
		fgcolor |= fgcolor << 16;
		bgcolor |= bgcolor << 8;
		bgcolor |= bgcolor << 16;
	} else {
		if (fgcolor < 16)
			fgcolor = ((u32 *)info->pseudo_palette)[fgcolor];
		if (bgcolor < 16)
			bgcolor = ((u32 *)info->pseudo_palette)[bgcolor];
	}
	__raw_writel(fgcolor, regs_base + TGA_FOREGROUND_REG);
	__raw_writel(bgcolor, regs_base + TGA_BACKGROUND_REG);

	pos = dy * line_length;
	if (is8bpp) {
		pos += dx;
		shift = pos & 3;
		pos &= -4;
	} else {
		pos += dx * 4;
		shift = (pos & 7) >> 2;
		pos &= -8;
	}

	data = (const unsigned char *) image->data;

	
	__raw_writel((is8bpp
		      ? TGA_MODE_SBM_8BPP | TGA_MODE_OPAQUE_STIPPLE
		      : TGA_MODE_SBM_24BPP | TGA_MODE_OPAQUE_STIPPLE),
		     regs_base + TGA_MODE_REG);

	if (width + shift <= 32) {
		unsigned long bwidth;


		
		pixelmask = (2ul << (width - 1)) - 1;
		pixelmask <<= shift;
		__raw_writel(pixelmask, regs_base + TGA_PIXELMASK_REG);
		wmb();

		bwidth = (width + 7) / 8;

		for (i = 0; i < height; ++i) {
			u32 mask = 0;

			for (j = 0; j < bwidth; ++j)
				mask |= bitrev8(data[j]) << (j * 8);

			__raw_writel(mask << shift, fb_base + pos);

			pos += line_length;
			data += rincr;
		}
		wmb();
		__raw_writel(0xffffffff, regs_base + TGA_PIXELMASK_REG);
	} else if (shift == 0) {
		unsigned long pos0 = pos;
		const unsigned char *data0 = data;
		unsigned long bincr = (is8bpp ? 8 : 8*4);
		unsigned long bwidth;

		/* Handle another common case in which accel_putcs
		   generates a large bitmap, which happens to be aligned.
		   Allow the tail to be misaligned.  This case is 
		   interesting because we've not got to hold partial
		   bytes across the words being written.  */

		wmb();

		bwidth = (width / 8) & -4;
		for (i = 0; i < height; ++i) {
			for (j = 0; j < bwidth; j += 4) {
				u32 mask = 0;
				mask |= bitrev8(data[j+0]) << (0 * 8);
				mask |= bitrev8(data[j+1]) << (1 * 8);
				mask |= bitrev8(data[j+2]) << (2 * 8);
				mask |= bitrev8(data[j+3]) << (3 * 8);
				__raw_writel(mask, fb_base + pos + j*bincr);
			}
			pos += line_length;
			data += rincr;
		}
		wmb();

		pixelmask = (1ul << (width & 31)) - 1;
		if (pixelmask) {
			__raw_writel(pixelmask, regs_base + TGA_PIXELMASK_REG);
			wmb();

			pos = pos0 + bwidth*bincr;
			data = data0 + bwidth;
			bwidth = ((width & 31) + 7) / 8;

			for (i = 0; i < height; ++i) {
				u32 mask = 0;
				for (j = 0; j < bwidth; ++j)
					mask |= bitrev8(data[j]) << (j * 8);
				__raw_writel(mask, fb_base + pos);
				pos += line_length;
				data += rincr;
			}
			wmb();
			__raw_writel(0xffffffff, regs_base + TGA_PIXELMASK_REG);
		}
	} else {
		unsigned long pos0 = pos;
		const unsigned char *data0 = data;
		unsigned long bincr = (is8bpp ? 8 : 8*4);
		unsigned long bwidth;


		pixelmask = 0xffff << shift;
		__raw_writel(pixelmask, regs_base + TGA_PIXELMASK_REG);
		wmb();

		bwidth = (width / 8) & -2;
		for (i = 0; i < height; ++i) {
			for (j = 0; j < bwidth; j += 2) {
				u32 mask = 0;
				mask |= bitrev8(data[j+0]) << (0 * 8);
				mask |= bitrev8(data[j+1]) << (1 * 8);
				mask <<= shift;
				__raw_writel(mask, fb_base + pos + j*bincr);
			}
			pos += line_length;
			data += rincr;
		}
		wmb();

		pixelmask = ((1ul << (width & 15)) - 1) << shift;
		if (pixelmask) {
			__raw_writel(pixelmask, regs_base + TGA_PIXELMASK_REG);
			wmb();

			pos = pos0 + bwidth*bincr;
			data = data0 + bwidth;
			bwidth = (width & 15) > 8;

			for (i = 0; i < height; ++i) {
				u32 mask = bitrev8(data[0]);
				if (bwidth)
					mask |= bitrev8(data[1]) << 8;
				mask <<= shift;
				__raw_writel(mask, fb_base + pos);
				pos += line_length;
				data += rincr;
			}
			wmb();
		}
		__raw_writel(0xffffffff, regs_base + TGA_PIXELMASK_REG);
	}

	
	__raw_writel((is8bpp
		      ? TGA_MODE_SBM_8BPP | TGA_MODE_SIMPLE
		      : TGA_MODE_SBM_24BPP | TGA_MODE_SIMPLE),
		     regs_base + TGA_MODE_REG);
}

static void
tgafb_clut_imageblit(struct fb_info *info, const struct fb_image *image)
{
	struct tga_par *par = (struct tga_par *) info->par;
	u32 color, dx, dy, width, height, vxres, vyres;
	u32 *palette = ((u32 *)info->pseudo_palette);
	unsigned long pos, line_length, i, j;
	const unsigned char *data;
	void __iomem *regs_base, *fb_base;

	dx = image->dx;
	dy = image->dy;
	width = image->width;
	height = image->height;
	vxres = info->var.xres_virtual;
	vyres = info->var.yres_virtual;
	line_length = info->fix.line_length;

	
	if (dx > vxres || dy > vyres)
		return;
	if (dx + width > vxres)
		width = vxres - dx;
	if (dy + height > vyres)
		height = vyres - dy;

	regs_base = par->tga_regs_base;
	fb_base = par->tga_fb_base;

	pos = dy * line_length + (dx * 4);
	data = image->data;

	
	for (i = 0; i < height; i++) {
		for (j = 0; j < width; j++) {
			color = palette[*data++];
			__raw_writel(color, fb_base + pos + j*4);
		}
		pos += line_length;
	}
}

static void
tgafb_imageblit(struct fb_info *info, const struct fb_image *image)
{
	unsigned int is8bpp = info->var.bits_per_pixel == 8;

	
	if (image->depth == 1) {
		tgafb_mono_imageblit(info, image);
		return;
	}

	if (image->depth == info->var.bits_per_pixel) {
		cfb_imageblit(info, image);
		return;
	}

	
	if (!is8bpp && image->depth == 8) {
		tgafb_clut_imageblit(info, image);
		return;
	}

	
}

static void
tgafb_fillrect(struct fb_info *info, const struct fb_fillrect *rect)
{
	struct tga_par *par = (struct tga_par *) info->par;
	int is8bpp = info->var.bits_per_pixel == 8;
	u32 dx, dy, width, height, vxres, vyres, color;
	unsigned long pos, align, line_length, i, j;
	void __iomem *regs_base;
	void __iomem *fb_base;

	dx = rect->dx;
	dy = rect->dy;
	width = rect->width;
	height = rect->height;
	vxres = info->var.xres_virtual;
	vyres = info->var.yres_virtual;
	line_length = info->fix.line_length;
	regs_base = par->tga_regs_base;
	fb_base = par->tga_fb_base;

	
	if (dx > vxres || dy > vyres || !width || !height)
		return;
	if (dx + width > vxres)
		width = vxres - dx;
	if (dy + height > vyres)
		height = vyres - dy;

	pos = dy * line_length + dx * (is8bpp ? 1 : 4);

	if (rect->rop != ROP_COPY) {
		cfb_fillrect(info, rect);
		return;
	}

	
	color = rect->color;
	if (is8bpp) {
		color |= color << 8;
		color |= color << 16;
		__raw_writel(color, regs_base + TGA_BLOCK_COLOR0_REG);
		__raw_writel(color, regs_base + TGA_BLOCK_COLOR1_REG);
	} else {
		if (color < 16)
			color = ((u32 *)info->pseudo_palette)[color];
		__raw_writel(color, regs_base + TGA_BLOCK_COLOR0_REG);
		__raw_writel(color, regs_base + TGA_BLOCK_COLOR1_REG);
		__raw_writel(color, regs_base + TGA_BLOCK_COLOR2_REG);
		__raw_writel(color, regs_base + TGA_BLOCK_COLOR3_REG);
		__raw_writel(color, regs_base + TGA_BLOCK_COLOR4_REG);
		__raw_writel(color, regs_base + TGA_BLOCK_COLOR5_REG);
		__raw_writel(color, regs_base + TGA_BLOCK_COLOR6_REG);
		__raw_writel(color, regs_base + TGA_BLOCK_COLOR7_REG);
	}

	__raw_writel(0xffffffff, regs_base + TGA_DATA_REG);

	
	__raw_writel((is8bpp
		      ? TGA_MODE_SBM_8BPP | TGA_MODE_BLOCK_FILL
		      : TGA_MODE_SBM_24BPP | TGA_MODE_BLOCK_FILL),
		     regs_base + TGA_MODE_REG);
	wmb();

	if (width == line_length)
		width *= height, height = 1;

	/* The write into the frame buffer must be aligned to 4 bytes,
	   but we are allowed to encode the offset within the word in
	   the data word written.  */
	align = (pos & 3) << 16;
	pos &= -4;

	if (width <= 2048) {
		u32 data;

		data = (width - 1) | align;

		for (i = 0; i < height; ++i) {
			__raw_writel(data, fb_base + pos);
			pos += line_length;
		}
	} else {
		unsigned long Bpp = (is8bpp ? 1 : 4);
		unsigned long nwidth = width & -2048;
		u32 fdata, ldata;

		fdata = (2048 - 1) | align;
		ldata = ((width & 2047) - 1) | align;

		for (i = 0; i < height; ++i) {
			for (j = 0; j < nwidth; j += 2048)
				__raw_writel(fdata, fb_base + pos + j*Bpp);
			if (j < width)
				__raw_writel(ldata, fb_base + pos + j*Bpp);
			pos += line_length;
		}
	}
	wmb();

	
	__raw_writel((is8bpp
		      ? TGA_MODE_SBM_8BPP | TGA_MODE_SIMPLE
		      : TGA_MODE_SBM_24BPP | TGA_MODE_SIMPLE),
		     regs_base + TGA_MODE_REG);
}



static inline void
copyarea_line_8bpp(struct fb_info *info, u32 dy, u32 sy,
		   u32 height, u32 width)
{
	struct tga_par *par = (struct tga_par *) info->par;
	void __iomem *tga_regs = par->tga_regs_base;
	unsigned long dpos, spos, i, n64;

	
	__raw_writel(TGA_MODE_SBM_8BPP | TGA_MODE_COPY, tga_regs+TGA_MODE_REG);
	__raw_writel(0, tga_regs+TGA_PIXELSHIFT_REG);
	wmb();

	n64 = (height * width) / 64;

	if (sy < dy) {
		spos = (sy + height) * width;
		dpos = (dy + height) * width;

		for (i = 0; i < n64; ++i) {
			spos -= 64;
			dpos -= 64;
			__raw_writel(spos, tga_regs+TGA_COPY64_SRC);
			wmb();
			__raw_writel(dpos, tga_regs+TGA_COPY64_DST);
			wmb();
		}
	} else {
		spos = sy * width;
		dpos = dy * width;

		for (i = 0; i < n64; ++i) {
			__raw_writel(spos, tga_regs+TGA_COPY64_SRC);
			wmb();
			__raw_writel(dpos, tga_regs+TGA_COPY64_DST);
			wmb();
			spos += 64;
			dpos += 64;
		}
	}

	
	__raw_writel(TGA_MODE_SBM_8BPP|TGA_MODE_SIMPLE, tga_regs+TGA_MODE_REG);
}

static inline void
copyarea_line_32bpp(struct fb_info *info, u32 dy, u32 sy,
		    u32 height, u32 width)
{
	struct tga_par *par = (struct tga_par *) info->par;
	void __iomem *tga_regs = par->tga_regs_base;
	void __iomem *tga_fb = par->tga_fb_base;
	void __iomem *src;
	void __iomem *dst;
	unsigned long i, n16;

	
	__raw_writel(TGA_MODE_SBM_24BPP | TGA_MODE_COPY, tga_regs+TGA_MODE_REG);
	__raw_writel(0, tga_regs+TGA_PIXELSHIFT_REG);
	wmb();

	n16 = (height * width) / 16;

	if (sy < dy) {
		src = tga_fb + (sy + height) * width * 4;
		dst = tga_fb + (dy + height) * width * 4;

		for (i = 0; i < n16; ++i) {
			src -= 64;
			dst -= 64;
			__raw_writel(0xffff, src);
			wmb();
			__raw_writel(0xffff, dst);
			wmb();
		}
	} else {
		src = tga_fb + sy * width * 4;
		dst = tga_fb + dy * width * 4;

		for (i = 0; i < n16; ++i) {
			__raw_writel(0xffff, src);
			wmb();
			__raw_writel(0xffff, dst);
			wmb();
			src += 64;
			dst += 64;
		}
	}

	
	__raw_writel(TGA_MODE_SBM_24BPP|TGA_MODE_SIMPLE, tga_regs+TGA_MODE_REG);
}

static inline void
copyarea_foreward_8bpp(struct fb_info *info, u32 dx, u32 dy, u32 sx, u32 sy,
		       u32 height, u32 width, u32 line_length)
{
	struct tga_par *par = (struct tga_par *) info->par;
	unsigned long i, copied, left;
	unsigned long dpos, spos, dalign, salign, yincr;
	u32 smask_first, dmask_first, dmask_last;
	int pixel_shift, need_prime, need_second;
	unsigned long n64, n32, xincr_first;
	void __iomem *tga_regs;
	void __iomem *tga_fb;

	yincr = line_length;
	if (dy > sy) {
		dy += height - 1;
		sy += height - 1;
		yincr = -yincr;
	}

	dpos = dy * line_length + dx;
	spos = sy * line_length + sx;
	dalign = dpos & 7;
	salign = spos & 7;
	dpos &= -8;
	spos &= -8;

	if (dalign >= salign)
		pixel_shift = dalign - salign;
	else
		pixel_shift = 8 - (salign - dalign);

	need_prime = (salign > dalign);
	if (need_prime)
		dpos -= 8;

	copied = 32 - (dalign + (dpos & 31));
	if (copied == 32)
		copied = 0;
	xincr_first = (copied + 7) & -8;
	smask_first = dmask_first = (1ul << copied) - 1;
	smask_first <<= salign;
	dmask_first <<= dalign + need_prime*8;
	if (need_prime && copied > 24)
		copied -= 8;
	left = width - copied;

	
	if (copied > width) {
		u32 t;
		t = (1ul << width) - 1;
		t <<= dalign + need_prime*8;
		dmask_first &= t;
		left = 0;
	}

	n64 = need_second = 0;
	if ((dpos & 63) == (spos & 63)
	    && (height == 1 || line_length % 64 == 0)) {
		
		need_second = (dpos + xincr_first) & 63;
		if ((need_second & 32) != need_second)
			printk(KERN_ERR "tgafb: need_second wrong\n");
		if (left >= need_second + 64) {
			left -= need_second;
			n64 = left / 64;
			left %= 64;
		} else
			need_second = 0;
	}

	n32 = left / 32;
	left %= 32;

	
	dmask_last = (1ul << left) - 1;

	tga_regs = par->tga_regs_base;
	tga_fb = par->tga_fb_base;

	
	__raw_writel(TGA_MODE_SBM_8BPP|TGA_MODE_COPY, tga_regs+TGA_MODE_REG);
	__raw_writel(pixel_shift, tga_regs+TGA_PIXELSHIFT_REG);
	wmb();

	for (i = 0; i < height; ++i) {
		unsigned long j;
		void __iomem *sfb;
		void __iomem *dfb;

		sfb = tga_fb + spos;
		dfb = tga_fb + dpos;
		if (dmask_first) {
			__raw_writel(smask_first, sfb);
			wmb();
			__raw_writel(dmask_first, dfb);
			wmb();
			sfb += xincr_first;
			dfb += xincr_first;
		}

		if (need_second) {
			__raw_writel(0xffffffff, sfb);
			wmb();
			__raw_writel(0xffffffff, dfb);
			wmb();
			sfb += 32;
			dfb += 32;
		}

		if (n64 && (((unsigned long)sfb | (unsigned long)dfb) & 63))
			printk(KERN_ERR
			       "tgafb: misaligned copy64 (s:%p, d:%p)\n",
			       sfb, dfb);

		for (j = 0; j < n64; ++j) {
			__raw_writel(sfb - tga_fb, tga_regs+TGA_COPY64_SRC);
			wmb();
			__raw_writel(dfb - tga_fb, tga_regs+TGA_COPY64_DST);
			wmb();
			sfb += 64;
			dfb += 64;
		}

		for (j = 0; j < n32; ++j) {
			__raw_writel(0xffffffff, sfb);
			wmb();
			__raw_writel(0xffffffff, dfb);
			wmb();
			sfb += 32;
			dfb += 32;
		}

		if (dmask_last) {
			__raw_writel(0xffffffff, sfb);
			wmb();
			__raw_writel(dmask_last, dfb);
			wmb();
		}

		spos += yincr;
		dpos += yincr;
	}

	
	__raw_writel(TGA_MODE_SBM_8BPP|TGA_MODE_SIMPLE, tga_regs+TGA_MODE_REG);
}

static inline void
copyarea_backward_8bpp(struct fb_info *info, u32 dx, u32 dy, u32 sx, u32 sy,
		       u32 height, u32 width, u32 line_length,
		       const struct fb_copyarea *area)
{
	struct tga_par *par = (struct tga_par *) info->par;
	unsigned long i, left, yincr;
	unsigned long depos, sepos, dealign, sealign;
	u32 mask_first, mask_last;
	unsigned long n32;
	void __iomem *tga_regs;
	void __iomem *tga_fb;

	yincr = line_length;
	if (dy > sy) {
		dy += height - 1;
		sy += height - 1;
		yincr = -yincr;
	}

	depos = dy * line_length + dx + width;
	sepos = sy * line_length + sx + width;
	dealign = depos & 7;
	sealign = sepos & 7;

	if (dealign != sealign) {
		cfb_copyarea(info, area);
		return;
	}

	mask_first = (1ul << dealign) - 1;
	left = width - dealign;

	
	if (dealign > width) {
		mask_first ^= (1ul << (dealign - width)) - 1;
		left = 0;
	}

	
	n32 = left / 32;
	left %= 32;

	
	mask_last = -1 << (32 - left);

	tga_regs = par->tga_regs_base;
	tga_fb = par->tga_fb_base;

	
	__raw_writel(TGA_MODE_SBM_8BPP|TGA_MODE_COPY, tga_regs+TGA_MODE_REG);
	__raw_writel(0, tga_regs+TGA_PIXELSHIFT_REG);
	wmb();

	for (i = 0; i < height; ++i) {
		unsigned long j;
		void __iomem *sfb;
		void __iomem *dfb;

		sfb = tga_fb + sepos;
		dfb = tga_fb + depos;
		if (mask_first) {
			__raw_writel(mask_first, sfb);
			wmb();
			__raw_writel(mask_first, dfb);
			wmb();
		}

		for (j = 0; j < n32; ++j) {
			sfb -= 32;
			dfb -= 32;
			__raw_writel(0xffffffff, sfb);
			wmb();
			__raw_writel(0xffffffff, dfb);
			wmb();
		}

		if (mask_last) {
			sfb -= 32;
			dfb -= 32;
			__raw_writel(mask_last, sfb);
			wmb();
			__raw_writel(mask_last, dfb);
			wmb();
		}

		sepos += yincr;
		depos += yincr;
	}

	
	__raw_writel(TGA_MODE_SBM_8BPP|TGA_MODE_SIMPLE, tga_regs+TGA_MODE_REG);
}

static void
tgafb_copyarea(struct fb_info *info, const struct fb_copyarea *area) 
{
	unsigned long dx, dy, width, height, sx, sy, vxres, vyres;
	unsigned long line_length, bpp;

	dx = area->dx;
	dy = area->dy;
	width = area->width;
	height = area->height;
	sx = area->sx;
	sy = area->sy;
	vxres = info->var.xres_virtual;
	vyres = info->var.yres_virtual;
	line_length = info->fix.line_length;

	
	if (dx > vxres || sx > vxres || dy > vyres || sy > vyres)
		return;

	
	if (dx + width > vxres)
		width = vxres - dx;
	if (dy + height > vyres)
		height = vyres - dy;

	
	if (sx + width > vxres || sy + height > vyres)
		return;

	bpp = info->var.bits_per_pixel;

	
	if (width * (bpp >> 3) == line_length) {
		if (bpp == 8)
			copyarea_line_8bpp(info, dy, sy, height, width);
		else
			copyarea_line_32bpp(info, dy, sy, height, width);
	}

	else if (bpp == 32)
		cfb_copyarea(info, area);

	else if (dy == sy && dx > sx && dx < sx + width)
		copyarea_backward_8bpp(info, dx, dy, sx, sy, height,
				       width, line_length, area);
	else
		copyarea_foreward_8bpp(info, dx, dy, sx, sy, height,
				       width, line_length);
}



static void
tgafb_init_fix(struct fb_info *info)
{
	struct tga_par *par = (struct tga_par *)info->par;
	int tga_bus_pci = TGA_BUS_PCI(par->dev);
	int tga_bus_tc = TGA_BUS_TC(par->dev);
	u8 tga_type = par->tga_type;
	const char *tga_type_name = NULL;

	switch (tga_type) {
	case TGA_TYPE_8PLANE:
		if (tga_bus_pci)
			tga_type_name = "Digital ZLXp-E1";
		if (tga_bus_tc)
			tga_type_name = "Digital ZLX-E1";
		break;
	case TGA_TYPE_24PLANE:
		if (tga_bus_pci)
			tga_type_name = "Digital ZLXp-E2";
		if (tga_bus_tc)
			tga_type_name = "Digital ZLX-E2";
		break;
	case TGA_TYPE_24PLUSZ:
		if (tga_bus_pci)
			tga_type_name = "Digital ZLXp-E3";
		if (tga_bus_tc)
			tga_type_name = "Digital ZLX-E3";
		break;
	default:
		tga_type_name = "Unknown";
		break;
	}

	strlcpy(info->fix.id, tga_type_name, sizeof(info->fix.id));

	info->fix.type = FB_TYPE_PACKED_PIXELS;
	info->fix.type_aux = 0;
	info->fix.visual = (tga_type == TGA_TYPE_8PLANE
			    ? FB_VISUAL_PSEUDOCOLOR
			    : FB_VISUAL_DIRECTCOLOR);

	info->fix.line_length = par->xres * (par->bits_per_pixel >> 3);
	info->fix.smem_start = (size_t) par->tga_fb_base;
	info->fix.smem_len = info->fix.line_length * par->yres;
	info->fix.mmio_start = (size_t) par->tga_regs_base;
	info->fix.mmio_len = 512;

	info->fix.xpanstep = 0;
	info->fix.ypanstep = 0;
	info->fix.ywrapstep = 0;

	info->fix.accel = FB_ACCEL_DEC_TGA;

	if (tga_type != TGA_TYPE_8PLANE) {
		info->var.red.length = 8;
		info->var.green.length = 8;
		info->var.blue.length = 8;
		info->var.red.offset = 16;
		info->var.green.offset = 8;
		info->var.blue.offset = 0;
	}
}

static int tgafb_pan_display(struct fb_var_screeninfo *var, struct fb_info *info)
{
	
	tgafb_set_par(info); 
	return 0;
}

static int __devinit
tgafb_register(struct device *dev)
{
	static const struct fb_videomode modedb_tc = {
		
		"1280x1024@72", 0, 1280, 1024, 7645, 224, 28, 33, 3, 160, 3,
		FB_SYNC_ON_GREEN, FB_VMODE_NONINTERLACED
	};

	static unsigned int const fb_offset_presets[4] = {
		TGA_8PLANE_FB_OFFSET,
		TGA_24PLANE_FB_OFFSET,
		0xffffffff,
		TGA_24PLUSZ_FB_OFFSET
	};

	const struct fb_videomode *modedb_tga = NULL;
	resource_size_t bar0_start = 0, bar0_len = 0;
	const char *mode_option_tga = NULL;
	int tga_bus_pci = TGA_BUS_PCI(dev);
	int tga_bus_tc = TGA_BUS_TC(dev);
	unsigned int modedbsize_tga = 0;
	void __iomem *mem_base;
	struct fb_info *info;
	struct tga_par *par;
	u8 tga_type;
	int ret = 0;

	
	if (tga_bus_pci && pci_enable_device(to_pci_dev(dev))) {
		printk(KERN_ERR "tgafb: Cannot enable PCI device\n");
		return -ENODEV;
	}

	
	info = framebuffer_alloc(sizeof(struct tga_par), dev);
	if (!info) {
		printk(KERN_ERR "tgafb: Cannot allocate memory\n");
		return -ENOMEM;
	}

	par = info->par;
	dev_set_drvdata(dev, info);

	
	ret = -ENODEV;
	if (tga_bus_pci) {
		bar0_start = pci_resource_start(to_pci_dev(dev), 0);
		bar0_len = pci_resource_len(to_pci_dev(dev), 0);
	}
	if (tga_bus_tc) {
		bar0_start = to_tc_dev(dev)->resource.start;
		bar0_len = to_tc_dev(dev)->resource.end - bar0_start + 1;
	}
	if (!request_mem_region (bar0_start, bar0_len, "tgafb")) {
		printk(KERN_ERR "tgafb: cannot reserve FB region\n");
		goto err0;
	}

	
	mem_base = ioremap_nocache(bar0_start, bar0_len);
	if (!mem_base) {
		printk(KERN_ERR "tgafb: Cannot map MMIO\n");
		goto err1;
	}

	
	tga_type = (readl(mem_base) >> 12) & 0x0f;
	par->dev = dev;
	par->tga_mem_base = mem_base;
	par->tga_fb_base = mem_base + fb_offset_presets[tga_type];
	par->tga_regs_base = mem_base + TGA_REGS_OFFSET;
	par->tga_type = tga_type;
	if (tga_bus_pci)
		par->tga_chip_rev = (to_pci_dev(dev))->revision;
	if (tga_bus_tc)
		par->tga_chip_rev = TGA_READ_REG(par, TGA_START_REG) & 0xff;

	
	info->flags = FBINFO_DEFAULT | FBINFO_HWACCEL_COPYAREA |
		      FBINFO_HWACCEL_IMAGEBLIT | FBINFO_HWACCEL_FILLRECT;
	info->fbops = &tgafb_ops;
	info->screen_base = par->tga_fb_base;
	info->pseudo_palette = par->palette;

	
	if (tga_bus_pci) {
		mode_option_tga = mode_option_pci;
	}
	if (tga_bus_tc) {
		mode_option_tga = mode_option_tc;
		modedb_tga = &modedb_tc;
		modedbsize_tga = 1;
	}
	ret = fb_find_mode(&info->var, info,
			   mode_option ? mode_option : mode_option_tga,
			   modedb_tga, modedbsize_tga, NULL,
			   tga_type == TGA_TYPE_8PLANE ? 8 : 32);
	if (ret == 0 || ret == 4) {
		printk(KERN_ERR "tgafb: Could not find valid video mode\n");
		ret = -EINVAL;
		goto err1;
	}

	if (fb_alloc_cmap(&info->cmap, 256, 0)) {
		printk(KERN_ERR "tgafb: Could not allocate color map\n");
		ret = -ENOMEM;
		goto err1;
	}

	tgafb_set_par(info);
	tgafb_init_fix(info);

	if (register_framebuffer(info) < 0) {
		printk(KERN_ERR "tgafb: Could not register framebuffer\n");
		ret = -EINVAL;
		goto err2;
	}

	if (tga_bus_pci) {
		pr_info("tgafb: DC21030 [TGA] detected, rev=0x%02x\n",
			par->tga_chip_rev);
		pr_info("tgafb: at PCI bus %d, device %d, function %d\n",
			to_pci_dev(dev)->bus->number,
			PCI_SLOT(to_pci_dev(dev)->devfn),
			PCI_FUNC(to_pci_dev(dev)->devfn));
	}
	if (tga_bus_tc)
		pr_info("tgafb: SFB+ detected, rev=0x%02x\n",
			par->tga_chip_rev);
	pr_info("fb%d: %s frame buffer device at 0x%lx\n",
		info->node, info->fix.id, (long)bar0_start);

	return 0;

 err2:
	fb_dealloc_cmap(&info->cmap);
 err1:
	if (mem_base)
		iounmap(mem_base);
	release_mem_region(bar0_start, bar0_len);
 err0:
	framebuffer_release(info);
	return ret;
}

static void __devexit
tgafb_unregister(struct device *dev)
{
	resource_size_t bar0_start = 0, bar0_len = 0;
	int tga_bus_pci = TGA_BUS_PCI(dev);
	int tga_bus_tc = TGA_BUS_TC(dev);
	struct fb_info *info = NULL;
	struct tga_par *par;

	info = dev_get_drvdata(dev);
	if (!info)
		return;

	par = info->par;
	unregister_framebuffer(info);
	fb_dealloc_cmap(&info->cmap);
	iounmap(par->tga_mem_base);
	if (tga_bus_pci) {
		bar0_start = pci_resource_start(to_pci_dev(dev), 0);
		bar0_len = pci_resource_len(to_pci_dev(dev), 0);
	}
	if (tga_bus_tc) {
		bar0_start = to_tc_dev(dev)->resource.start;
		bar0_len = to_tc_dev(dev)->resource.end - bar0_start + 1;
	}
	release_mem_region(bar0_start, bar0_len);
	framebuffer_release(info);
}

static void __devexit
tgafb_exit(void)
{
	tc_unregister_driver(&tgafb_tc_driver);
	pci_unregister_driver(&tgafb_pci_driver);
}

#ifndef MODULE
static int __devinit
tgafb_setup(char *arg)
{
	char *this_opt;

	if (arg && *arg) {
		while ((this_opt = strsep(&arg, ","))) {
			if (!*this_opt)
				continue;
			if (!strncmp(this_opt, "mode:", 5))
				mode_option = this_opt+5;
			else
				printk(KERN_ERR
				       "tgafb: unknown parameter %s\n",
				       this_opt);
		}
	}

	return 0;
}
#endif 

static int __devinit
tgafb_init(void)
{
	int status;
#ifndef MODULE
	char *option = NULL;

	if (fb_get_options("tgafb", &option))
		return -ENODEV;
	tgafb_setup(option);
#endif
	status = pci_register_driver(&tgafb_pci_driver);
	if (!status)
		status = tc_register_driver(&tgafb_tc_driver);
	return status;
}


module_init(tgafb_init);
module_exit(tgafb_exit);

MODULE_DESCRIPTION("Framebuffer driver for TGA/SFB+ chipset");
MODULE_LICENSE("GPL");
