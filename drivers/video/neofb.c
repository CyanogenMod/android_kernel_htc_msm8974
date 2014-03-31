/*
 * linux/drivers/video/neofb.c -- NeoMagic Framebuffer Driver
 *
 * Copyright (c) 2001-2002  Denis Oliver Kropp <dok@directfb.org>
 *
 *
 * Card specific code is based on XFree86's neomagic driver.
 * Framebuffer framework code is based on code of cyber2000fb.
 *
 * This file is subject to the terms and conditions of the GNU General
 * Public License.  See the file COPYING in the main directory of this
 * archive for more details.
 *
 *
 * 0.4.1
 *  - Cosmetic changes (dok)
 *
 * 0.4
 *  - Toshiba Libretto support, allow modes larger than LCD size if
 *    LCD is disabled, keep BIOS settings if internal/external display
 *    haven't been enabled explicitly
 *                          (Thomas J. Moore <dark@mama.indstate.edu>)
 *
 * 0.3.3
 *  - Porting over to new fbdev api. (jsimmons)
 *  
 * 0.3.2
 *  - got rid of all floating point (dok) 
 *
 * 0.3.1
 *  - added module license (dok)
 *
 * 0.3
 *  - hardware accelerated clear and move for 2200 and above (dok)
 *  - maximum allowed dotclock is handled now (dok)
 *
 * 0.2.1
 *  - correct panning after X usage (dok)
 *  - added module and kernel parameters (dok)
 *  - no stretching if external display is enabled (dok)
 *
 * 0.2
 *  - initial version (dok)
 *
 *
 * TODO
 * - ioctl for internal/external switching
 * - blanking
 * - 32bit depth support, maybe impossible
 * - disable pan-on-sync, need specs
 *
 * BUGS
 * - white margin on bootup like with tdfxfb (colormap problem?)
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/fb.h>
#include <linux/pci.h>
#include <linux/init.h>
#ifdef CONFIG_TOSHIBA
#include <linux/toshiba.h>
#endif

#include <asm/io.h>
#include <asm/irq.h>
#include <asm/pgtable.h>

#ifdef CONFIG_MTRR
#include <asm/mtrr.h>
#endif

#include <video/vga.h>
#include <video/neomagic.h>

#define NEOFB_VERSION "0.4.2"


static bool internal;
static bool external;
static bool libretto;
static bool nostretch;
static bool nopciburst;
static char *mode_option __devinitdata = NULL;

#ifdef MODULE

MODULE_AUTHOR("(c) 2001-2002  Denis Oliver Kropp <dok@convergence.de>");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("FBDev driver for NeoMagic PCI Chips");
module_param(internal, bool, 0);
MODULE_PARM_DESC(internal, "Enable output on internal LCD Display.");
module_param(external, bool, 0);
MODULE_PARM_DESC(external, "Enable output on external CRT.");
module_param(libretto, bool, 0);
MODULE_PARM_DESC(libretto, "Force Libretto 100/110 800x480 LCD.");
module_param(nostretch, bool, 0);
MODULE_PARM_DESC(nostretch,
		 "Disable stretching of modes smaller than LCD.");
module_param(nopciburst, bool, 0);
MODULE_PARM_DESC(nopciburst, "Disable PCI burst mode.");
module_param(mode_option, charp, 0);
MODULE_PARM_DESC(mode_option, "Preferred video mode ('640x480-8@60', etc)");

#endif



static biosMode bios8[] = {
	{320, 240, 0x40},
	{300, 400, 0x42},
	{640, 400, 0x20},
	{640, 480, 0x21},
	{800, 600, 0x23},
	{1024, 768, 0x25},
};

static biosMode bios16[] = {
	{320, 200, 0x2e},
	{320, 240, 0x41},
	{300, 400, 0x43},
	{640, 480, 0x31},
	{800, 600, 0x34},
	{1024, 768, 0x37},
};

static biosMode bios24[] = {
	{640, 480, 0x32},
	{800, 600, 0x35},
	{1024, 768, 0x38}
};

#ifdef NO_32BIT_SUPPORT_YET
static biosMode bios32[] = {
	{640, 480, 0x33},
	{800, 600, 0x36},
	{1024, 768, 0x39}
};
#endif

static inline void write_le32(int regindex, u32 val, const struct neofb_par *par)
{
	writel(val, par->neo2200 + par->cursorOff + regindex);
}

static int neoFindMode(int xres, int yres, int depth)
{
	int xres_s;
	int i, size;
	biosMode *mode;

	switch (depth) {
	case 8:
		size = ARRAY_SIZE(bios8);
		mode = bios8;
		break;
	case 16:
		size = ARRAY_SIZE(bios16);
		mode = bios16;
		break;
	case 24:
		size = ARRAY_SIZE(bios24);
		mode = bios24;
		break;
#ifdef NO_32BIT_SUPPORT_YET
	case 32:
		size = ARRAY_SIZE(bios32);
		mode = bios32;
		break;
#endif
	default:
		return 0;
	}

	for (i = 0; i < size; i++) {
		if (xres <= mode[i].x_res) {
			xres_s = mode[i].x_res;
			for (; i < size; i++) {
				if (mode[i].x_res != xres_s)
					return mode[i - 1].mode;
				if (yres <= mode[i].y_res)
					return mode[i].mode;
			}
		}
	}
	return mode[size - 1].mode;
}

#define MAX_N 127
#define MAX_D 31
#define MAX_F 1

static void neoCalcVCLK(const struct fb_info *info,
			struct neofb_par *par, long freq)
{
	int n, d, f;
	int n_best = 0, d_best = 0, f_best = 0;
	long f_best_diff = 0x7ffff;

	for (f = 0; f <= MAX_F; f++)
		for (d = 0; d <= MAX_D; d++)
			for (n = 0; n <= MAX_N; n++) {
				long f_out;
				long f_diff;

				f_out = ((14318 * (n + 1)) / (d + 1)) >> f;
				f_diff = abs(f_out - freq);
				if (f_diff <= f_best_diff) {
					f_best_diff = f_diff;
					n_best = n;
					d_best = d;
					f_best = f;
				}
				if (f_out > freq)
					break;
			}

	if (info->fix.accel == FB_ACCEL_NEOMAGIC_NM2200 ||
	    info->fix.accel == FB_ACCEL_NEOMAGIC_NM2230 ||
	    info->fix.accel == FB_ACCEL_NEOMAGIC_NM2360 ||
	    info->fix.accel == FB_ACCEL_NEOMAGIC_NM2380) {
		par->VCLK3NumeratorLow = n_best;
		par->VCLK3NumeratorHigh = (f_best << 7);
	} else
		par->VCLK3NumeratorLow = n_best | (f_best << 7);

	par->VCLK3Denominator = d_best;

#ifdef NEOFB_DEBUG
	printk(KERN_DEBUG "neoVCLK: f:%ld NumLow=%d NumHi=%d Den=%d Df=%ld\n",
	       freq,
	       par->VCLK3NumeratorLow,
	       par->VCLK3NumeratorHigh,
	       par->VCLK3Denominator, f_best_diff);
#endif
}


static int vgaHWInit(const struct fb_var_screeninfo *var,
		     struct neofb_par *par)
{
	int hsync_end = var->xres + var->right_margin + var->hsync_len;
	int htotal = (hsync_end + var->left_margin) >> 3;
	int vsync_start = var->yres + var->lower_margin;
	int vsync_end = vsync_start + var->vsync_len;
	int vtotal = vsync_end + var->upper_margin;

	par->MiscOutReg = 0x23;

	if (!(var->sync & FB_SYNC_HOR_HIGH_ACT))
		par->MiscOutReg |= 0x40;

	if (!(var->sync & FB_SYNC_VERT_HIGH_ACT))
		par->MiscOutReg |= 0x80;

	par->Sequencer[0] = 0x00;
	par->Sequencer[1] = 0x01;
	par->Sequencer[2] = 0x0F;
	par->Sequencer[3] = 0x00;	
	par->Sequencer[4] = 0x0E;	

	par->CRTC[0] = htotal - 5;
	par->CRTC[1] = (var->xres >> 3) - 1;
	par->CRTC[2] = (var->xres >> 3) - 1;
	par->CRTC[3] = ((htotal - 1) & 0x1F) | 0x80;
	par->CRTC[4] = ((var->xres + var->right_margin) >> 3);
	par->CRTC[5] = (((htotal - 1) & 0x20) << 2)
	    | (((hsync_end >> 3)) & 0x1F);
	par->CRTC[6] = (vtotal - 2) & 0xFF;
	par->CRTC[7] = (((vtotal - 2) & 0x100) >> 8)
	    | (((var->yres - 1) & 0x100) >> 7)
	    | ((vsync_start & 0x100) >> 6)
	    | (((var->yres - 1) & 0x100) >> 5)
	    | 0x10 | (((vtotal - 2) & 0x200) >> 4)
	    | (((var->yres - 1) & 0x200) >> 3)
	    | ((vsync_start & 0x200) >> 2);
	par->CRTC[8] = 0x00;
	par->CRTC[9] = (((var->yres - 1) & 0x200) >> 4) | 0x40;

	if (var->vmode & FB_VMODE_DOUBLE)
		par->CRTC[9] |= 0x80;

	par->CRTC[10] = 0x00;
	par->CRTC[11] = 0x00;
	par->CRTC[12] = 0x00;
	par->CRTC[13] = 0x00;
	par->CRTC[14] = 0x00;
	par->CRTC[15] = 0x00;
	par->CRTC[16] = vsync_start & 0xFF;
	par->CRTC[17] = (vsync_end & 0x0F) | 0x20;
	par->CRTC[18] = (var->yres - 1) & 0xFF;
	par->CRTC[19] = var->xres_virtual >> 4;
	par->CRTC[20] = 0x00;
	par->CRTC[21] = (var->yres - 1) & 0xFF;
	par->CRTC[22] = (vtotal - 1) & 0xFF;
	par->CRTC[23] = 0xC3;
	par->CRTC[24] = 0xFF;


	par->Graphics[0] = 0x00;
	par->Graphics[1] = 0x00;
	par->Graphics[2] = 0x00;
	par->Graphics[3] = 0x00;
	par->Graphics[4] = 0x00;
	par->Graphics[5] = 0x40;
	par->Graphics[6] = 0x05;	
	par->Graphics[7] = 0x0F;
	par->Graphics[8] = 0xFF;


	par->Attribute[0] = 0x00;	
	par->Attribute[1] = 0x01;
	par->Attribute[2] = 0x02;
	par->Attribute[3] = 0x03;
	par->Attribute[4] = 0x04;
	par->Attribute[5] = 0x05;
	par->Attribute[6] = 0x06;
	par->Attribute[7] = 0x07;
	par->Attribute[8] = 0x08;
	par->Attribute[9] = 0x09;
	par->Attribute[10] = 0x0A;
	par->Attribute[11] = 0x0B;
	par->Attribute[12] = 0x0C;
	par->Attribute[13] = 0x0D;
	par->Attribute[14] = 0x0E;
	par->Attribute[15] = 0x0F;
	par->Attribute[16] = 0x41;
	par->Attribute[17] = 0xFF;
	par->Attribute[18] = 0x0F;
	par->Attribute[19] = 0x00;
	par->Attribute[20] = 0x00;
	return 0;
}

static void vgaHWLock(struct vgastate *state)
{
	
	vga_wcrt(state->vgabase, 0x11, vga_rcrt(state->vgabase, 0x11) | 0x80);
}

static void vgaHWUnlock(void)
{
	
	vga_wcrt(NULL, 0x11, vga_rcrt(NULL, 0x11) & ~0x80);
}

static void neoLock(struct vgastate *state)
{
	vga_wgfx(state->vgabase, 0x09, 0x00);
	vgaHWLock(state);
}

static void neoUnlock(void)
{
	vgaHWUnlock();
	vga_wgfx(NULL, 0x09, 0x26);
}

static int paletteEnabled = 0;

static inline void VGAenablePalette(void)
{
	vga_r(NULL, VGA_IS1_RC);
	vga_w(NULL, VGA_ATT_W, 0x00);
	paletteEnabled = 1;
}

static inline void VGAdisablePalette(void)
{
	vga_r(NULL, VGA_IS1_RC);
	vga_w(NULL, VGA_ATT_W, 0x20);
	paletteEnabled = 0;
}

static inline void VGAwATTR(u8 index, u8 value)
{
	if (paletteEnabled)
		index &= ~0x20;
	else
		index |= 0x20;

	vga_r(NULL, VGA_IS1_RC);
	vga_wattr(NULL, index, value);
}

static void vgaHWProtect(int on)
{
	unsigned char tmp;

	tmp = vga_rseq(NULL, 0x01);
	if (on) {
		vga_wseq(NULL, 0x00, 0x01);		
		vga_wseq(NULL, 0x01, tmp | 0x20);	

		VGAenablePalette();
	} else {
		vga_wseq(NULL, 0x01, tmp & ~0x20);	
		vga_wseq(NULL, 0x00, 0x03);		

		VGAdisablePalette();
	}
}

static void vgaHWRestore(const struct fb_info *info,
			 const struct neofb_par *par)
{
	int i;

	vga_w(NULL, VGA_MIS_W, par->MiscOutReg);

	for (i = 1; i < 5; i++)
		vga_wseq(NULL, i, par->Sequencer[i]);

	
	vga_wcrt(NULL, 17, par->CRTC[17] & ~0x80);

	for (i = 0; i < 25; i++)
		vga_wcrt(NULL, i, par->CRTC[i]);

	for (i = 0; i < 9; i++)
		vga_wgfx(NULL, i, par->Graphics[i]);

	VGAenablePalette();

	for (i = 0; i < 21; i++)
		VGAwATTR(i, par->Attribute[i]);

	VGAdisablePalette();
}



static inline int neo2200_sync(struct fb_info *info)
{
	struct neofb_par *par = info->par;

	while (readl(&par->neo2200->bltStat) & 1)
		cpu_relax();
	return 0;
}

static inline void neo2200_wait_fifo(struct fb_info *info,
				     int requested_fifo_space)
{
	
	


	neo2200_sync(info);
}

static inline void neo2200_accel_init(struct fb_info *info,
				      struct fb_var_screeninfo *var)
{
	struct neofb_par *par = info->par;
	Neo2200 __iomem *neo2200 = par->neo2200;
	u32 bltMod, pitch;

	neo2200_sync(info);

	switch (var->bits_per_pixel) {
	case 8:
		bltMod = NEO_MODE1_DEPTH8;
		pitch = var->xres_virtual;
		break;
	case 15:
	case 16:
		bltMod = NEO_MODE1_DEPTH16;
		pitch = var->xres_virtual * 2;
		break;
	case 24:
		bltMod = NEO_MODE1_DEPTH24;
		pitch = var->xres_virtual * 3;
		break;
	default:
		printk(KERN_ERR
		       "neofb: neo2200_accel_init: unexpected bits per pixel!\n");
		return;
	}

	writel(bltMod << 16, &neo2200->bltStat);
	writel((pitch << 16) | pitch, &neo2200->pitch);
}


static int
neofb_open(struct fb_info *info, int user)
{
	struct neofb_par *par = info->par;

	if (!par->ref_count) {
		memset(&par->state, 0, sizeof(struct vgastate));
		par->state.flags = VGA_SAVE_MODE | VGA_SAVE_FONTS;
		save_vga(&par->state);
	}
	par->ref_count++;

	return 0;
}

static int
neofb_release(struct fb_info *info, int user)
{
	struct neofb_par *par = info->par;

	if (!par->ref_count)
		return -EINVAL;

	if (par->ref_count == 1) {
		restore_vga(&par->state);
	}
	par->ref_count--;

	return 0;
}

static int
neofb_check_var(struct fb_var_screeninfo *var, struct fb_info *info)
{
	struct neofb_par *par = info->par;
	int memlen, vramlen;
	int mode_ok = 0;

	DBG("neofb_check_var");

	if (PICOS2KHZ(var->pixclock) > par->maxClock)
		return -EINVAL;

	
	if (par->internal_display &&
            ((var->xres > par->NeoPanelWidth) ||
	     (var->yres > par->NeoPanelHeight))) {
		printk(KERN_INFO
		       "Mode (%dx%d) larger than the LCD panel (%dx%d)\n",
		       var->xres, var->yres, par->NeoPanelWidth,
		       par->NeoPanelHeight);
		return -EINVAL;
	}

	
	if (!par->internal_display)
		mode_ok = 1;
	else {
		switch (var->xres) {
		case 1280:
			if (var->yres == 1024)
				mode_ok = 1;
			break;
		case 1024:
			if (var->yres == 768)
				mode_ok = 1;
			break;
		case 800:
			if (var->yres == (par->libretto ? 480 : 600))
				mode_ok = 1;
			break;
		case 640:
			if (var->yres == 480)
				mode_ok = 1;
			break;
		}
	}

	if (!mode_ok) {
		printk(KERN_INFO
		       "Mode (%dx%d) won't display properly on LCD\n",
		       var->xres, var->yres);
		return -EINVAL;
	}

	var->red.msb_right = 0;
	var->green.msb_right = 0;
	var->blue.msb_right = 0;
	var->transp.msb_right = 0;

	var->transp.offset = 0;
	var->transp.length = 0;
	switch (var->bits_per_pixel) {
	case 8:		
		var->red.offset = 0;
		var->red.length = 8;
		var->green.offset = 0;
		var->green.length = 8;
		var->blue.offset = 0;
		var->blue.length = 8;
		break;

	case 16:		
		var->red.offset = 11;
		var->red.length = 5;
		var->green.offset = 5;
		var->green.length = 6;
		var->blue.offset = 0;
		var->blue.length = 5;
		break;

	case 24:		
		var->red.offset = 16;
		var->red.length = 8;
		var->green.offset = 8;
		var->green.length = 8;
		var->blue.offset = 0;
		var->blue.length = 8;
		break;

#ifdef NO_32BIT_SUPPORT_YET
	case 32:		
		var->transp.offset = 24;
		var->transp.length = 8;
		var->red.offset = 16;
		var->red.length = 8;
		var->green.offset = 8;
		var->green.length = 8;
		var->blue.offset = 0;
		var->blue.length = 8;
		break;
#endif
	default:
		printk(KERN_WARNING "neofb: no support for %dbpp\n",
		       var->bits_per_pixel);
		return -EINVAL;
	}

	vramlen = info->fix.smem_len;
	if (vramlen > 4 * 1024 * 1024)
		vramlen = 4 * 1024 * 1024;

	if (var->xres_virtual < var->xres)
		var->xres_virtual = var->xres;

	memlen = var->xres_virtual * var->bits_per_pixel * var->yres_virtual >> 3;

	if (memlen > vramlen) {
		var->yres_virtual =  vramlen * 8 / (var->xres_virtual *
				   	var->bits_per_pixel);
		memlen = var->xres_virtual * var->bits_per_pixel *
				var->yres_virtual / 8;
	}

	if (var->yres_virtual < var->yres)
		var->yres = var->yres_virtual;
	if (var->xoffset + var->xres > var->xres_virtual)
		var->xoffset = var->xres_virtual - var->xres;
	if (var->yoffset + var->yres > var->yres_virtual)
		var->yoffset = var->yres_virtual - var->yres;

	var->nonstd = 0;
	var->height = -1;
	var->width = -1;

	if (var->bits_per_pixel >= 24 || !par->neo2200)
		var->accel_flags &= ~FB_ACCELF_TEXT;
	return 0;
}

static int neofb_set_par(struct fb_info *info)
{
	struct neofb_par *par = info->par;
	unsigned char temp;
	int i, clock_hi = 0;
	int lcd_stretch;
	int hoffset, voffset;
	int vsync_start, vtotal;

	DBG("neofb_set_par");

	neoUnlock();

	vgaHWProtect(1);	

	vsync_start = info->var.yres + info->var.lower_margin;
	vtotal = vsync_start + info->var.vsync_len + info->var.upper_margin;


	if (vgaHWInit(&info->var, par))
		return -EINVAL;

	par->Attribute[16] = 0x01;

	switch (info->var.bits_per_pixel) {
	case 8:
		par->CRTC[0x13] = info->var.xres_virtual >> 3;
		par->ExtCRTOffset = info->var.xres_virtual >> 11;
		par->ExtColorModeSelect = 0x11;
		break;
	case 16:
		par->CRTC[0x13] = info->var.xres_virtual >> 2;
		par->ExtCRTOffset = info->var.xres_virtual >> 10;
		par->ExtColorModeSelect = 0x13;
		break;
	case 24:
		par->CRTC[0x13] = (info->var.xres_virtual * 3) >> 3;
		par->ExtCRTOffset = (info->var.xres_virtual * 3) >> 11;
		par->ExtColorModeSelect = 0x14;
		break;
#ifdef NO_32BIT_SUPPORT_YET
	case 32:		
		par->CRTC[0x13] = info->var.xres_virtual >> 1;
		par->ExtCRTOffset = info->var.xres_virtual >> 9;
		par->ExtColorModeSelect = 0x15;
		break;
#endif
	default:
		break;
	}

	par->ExtCRTDispAddr = 0x10;

	
	par->VerticalExt = (((vtotal - 2) & 0x400) >> 10)
	    | (((info->var.yres - 1) & 0x400) >> 9)
	    | (((vsync_start) & 0x400) >> 8)
	    | (((vsync_start) & 0x400) >> 7);

	
	if (par->pci_burst)
		par->SysIfaceCntl1 = 0x30;
	else
		par->SysIfaceCntl1 = 0x00;

	par->SysIfaceCntl2 = 0xc0;	

	
	par->PanelDispCntlRegRead = 1;

	
	par->PanelDispCntlReg1 = 0x00;
	if (par->internal_display)
		par->PanelDispCntlReg1 |= 0x02;
	if (par->external_display)
		par->PanelDispCntlReg1 |= 0x01;

	
	if (par->PanelDispCntlReg1 == 0x00) {
		
		par->PanelDispCntlReg1 = vga_rgfx(NULL, 0x20) & 0x03;
	}

	
	switch (info->var.xres) {
	case 1280:
		par->PanelDispCntlReg1 |= 0x60;
		break;
	case 1024:
		par->PanelDispCntlReg1 |= 0x40;
		break;
	case 800:
		par->PanelDispCntlReg1 |= 0x20;
		break;
	case 640:
	default:
		break;
	}

	
	switch (par->PanelDispCntlReg1 & 0x03) {
	case 0x01:		
		par->GeneralLockReg = 0x00;
		
		par->ProgramVCLK = 1;
		break;
	case 0x02:		
	case 0x03:		
		par->GeneralLockReg = 0x01;
		
		par->ProgramVCLK = 0;
		break;
	}

	par->PanelDispCntlReg2 = 0x00;
	par->PanelDispCntlReg3 = 0x00;

	if (par->lcd_stretch && (par->PanelDispCntlReg1 == 0x02) &&	
	    (info->var.xres != par->NeoPanelWidth)) {
		switch (info->var.xres) {
		case 320:	
		case 400:	
		case 640:
		case 800:
		case 1024:
			lcd_stretch = 1;
			par->PanelDispCntlReg2 |= 0xC6;
			break;
		default:
			lcd_stretch = 0;
			
		}
	} else
		lcd_stretch = 0;

	par->PanelVertCenterReg1 = 0x00;
	par->PanelVertCenterReg2 = 0x00;
	par->PanelVertCenterReg3 = 0x00;
	par->PanelVertCenterReg4 = 0x00;
	par->PanelVertCenterReg5 = 0x00;
	par->PanelHorizCenterReg1 = 0x00;
	par->PanelHorizCenterReg2 = 0x00;
	par->PanelHorizCenterReg3 = 0x00;
	par->PanelHorizCenterReg4 = 0x00;
	par->PanelHorizCenterReg5 = 0x00;


	if (par->PanelDispCntlReg1 & 0x02) {
		if (info->var.xres == par->NeoPanelWidth) {
		} else {
			par->PanelDispCntlReg2 |= 0x01;
			par->PanelDispCntlReg3 |= 0x10;

			
			if (!lcd_stretch) {
				hoffset =
				    ((par->NeoPanelWidth -
				      info->var.xres) >> 4) - 1;
				voffset =
				    ((par->NeoPanelHeight -
				      info->var.yres) >> 1) - 2;
			} else {
				
				hoffset = 0;
				voffset = 0;
			}

			switch (info->var.xres) {
			case 320:	
				par->PanelHorizCenterReg3 = hoffset;
				par->PanelVertCenterReg2 = voffset;
				break;
			case 400:	
				par->PanelHorizCenterReg4 = hoffset;
				par->PanelVertCenterReg1 = voffset;
				break;
			case 640:
				par->PanelHorizCenterReg1 = hoffset;
				par->PanelVertCenterReg3 = voffset;
				break;
			case 800:
				par->PanelHorizCenterReg2 = hoffset;
				par->PanelVertCenterReg4 = voffset;
				break;
			case 1024:
				par->PanelHorizCenterReg5 = hoffset;
				par->PanelVertCenterReg5 = voffset;
				break;
			case 1280:
			default:
				
				break;
			}
		}
	}

	par->biosMode =
	    neoFindMode(info->var.xres, info->var.yres,
			info->var.bits_per_pixel);

	neoCalcVCLK(info, par, PICOS2KHZ(info->var.pixclock));

	
	par->MiscOutReg |= 0x0C;

	
	

	
	vga_wgfx(NULL, 0x15, 0x00);

	
	vga_wgfx(NULL, 0x0A, par->GeneralLockReg);

	temp = vga_rgfx(NULL, 0x90);
	switch (info->fix.accel) {
	case FB_ACCEL_NEOMAGIC_NM2070:
		temp &= 0xF0;	
		temp |= (par->ExtColorModeSelect & ~0xF0);
		break;
	case FB_ACCEL_NEOMAGIC_NM2090:
	case FB_ACCEL_NEOMAGIC_NM2093:
	case FB_ACCEL_NEOMAGIC_NM2097:
	case FB_ACCEL_NEOMAGIC_NM2160:
	case FB_ACCEL_NEOMAGIC_NM2200:
	case FB_ACCEL_NEOMAGIC_NM2230:
	case FB_ACCEL_NEOMAGIC_NM2360:
	case FB_ACCEL_NEOMAGIC_NM2380:
		temp &= 0x70;	
		temp |= (par->ExtColorModeSelect & ~0x70);
		break;
	}

	vga_wgfx(NULL, 0x90, temp);

	

	temp = vga_rgfx(NULL, 0x25);
	temp &= 0x39;
	vga_wgfx(NULL, 0x25, temp);

	mdelay(200);

	vgaHWRestore(info, par);

	
	switch (info->var.bits_per_pixel) {
	case 8:
		
		info->fix.visual = FB_VISUAL_PSEUDOCOLOR;
		break;
	case 16:
		
		info->fix.visual = FB_VISUAL_TRUECOLOR;

		for (i = 0; i < 64; i++) {
			outb(i, 0x3c8);

			outb(i << 1, 0x3c9);
			outb(i, 0x3c9);
			outb(i << 1, 0x3c9);
		}
		break;
	case 24:
#ifdef NO_32BIT_SUPPORT_YET
	case 32:
#endif
		
		info->fix.visual = FB_VISUAL_TRUECOLOR;

		for (i = 0; i < 256; i++) {
			outb(i, 0x3c8);

			outb(i, 0x3c9);
			outb(i, 0x3c9);
			outb(i, 0x3c9);
		}
		break;
	}

	vga_wgfx(NULL, 0x0E, par->ExtCRTDispAddr);
	vga_wgfx(NULL, 0x0F, par->ExtCRTOffset);
	temp = vga_rgfx(NULL, 0x10);
	temp &= 0x0F;		
	temp |= (par->SysIfaceCntl1 & ~0x0F);	
	vga_wgfx(NULL, 0x10, temp);

	vga_wgfx(NULL, 0x11, par->SysIfaceCntl2);
	vga_wgfx(NULL, 0x15, 0  );
	vga_wgfx(NULL, 0x16, 0  );

	temp = vga_rgfx(NULL, 0x20);
	switch (info->fix.accel) {
	case FB_ACCEL_NEOMAGIC_NM2070:
		temp &= 0xFC;	
		temp |= (par->PanelDispCntlReg1 & ~0xFC);
		break;
	case FB_ACCEL_NEOMAGIC_NM2090:
	case FB_ACCEL_NEOMAGIC_NM2093:
	case FB_ACCEL_NEOMAGIC_NM2097:
	case FB_ACCEL_NEOMAGIC_NM2160:
		temp &= 0xDC;	
		temp |= (par->PanelDispCntlReg1 & ~0xDC);
		break;
	case FB_ACCEL_NEOMAGIC_NM2200:
	case FB_ACCEL_NEOMAGIC_NM2230:
	case FB_ACCEL_NEOMAGIC_NM2360:
	case FB_ACCEL_NEOMAGIC_NM2380:
		temp &= 0x98;	
		temp |= (par->PanelDispCntlReg1 & ~0x98);
		break;
	}
	vga_wgfx(NULL, 0x20, temp);

	temp = vga_rgfx(NULL, 0x25);
	temp &= 0x38;		
	temp |= (par->PanelDispCntlReg2 & ~0x38);
	vga_wgfx(NULL, 0x25, temp);

	if (info->fix.accel != FB_ACCEL_NEOMAGIC_NM2070) {
		temp = vga_rgfx(NULL, 0x30);
		temp &= 0xEF;	
		temp |= (par->PanelDispCntlReg3 & ~0xEF);
		vga_wgfx(NULL, 0x30, temp);
	}

	vga_wgfx(NULL, 0x28, par->PanelVertCenterReg1);
	vga_wgfx(NULL, 0x29, par->PanelVertCenterReg2);
	vga_wgfx(NULL, 0x2a, par->PanelVertCenterReg3);

	if (info->fix.accel != FB_ACCEL_NEOMAGIC_NM2070) {
		vga_wgfx(NULL, 0x32, par->PanelVertCenterReg4);
		vga_wgfx(NULL, 0x33, par->PanelHorizCenterReg1);
		vga_wgfx(NULL, 0x34, par->PanelHorizCenterReg2);
		vga_wgfx(NULL, 0x35, par->PanelHorizCenterReg3);
	}

	if (info->fix.accel == FB_ACCEL_NEOMAGIC_NM2160)
		vga_wgfx(NULL, 0x36, par->PanelHorizCenterReg4);

	if (info->fix.accel == FB_ACCEL_NEOMAGIC_NM2200 ||
	    info->fix.accel == FB_ACCEL_NEOMAGIC_NM2230 ||
	    info->fix.accel == FB_ACCEL_NEOMAGIC_NM2360 ||
	    info->fix.accel == FB_ACCEL_NEOMAGIC_NM2380) {
		vga_wgfx(NULL, 0x36, par->PanelHorizCenterReg4);
		vga_wgfx(NULL, 0x37, par->PanelVertCenterReg5);
		vga_wgfx(NULL, 0x38, par->PanelHorizCenterReg5);

		clock_hi = 1;
	}

	
	if (par->ProgramVCLK && ((vga_rgfx(NULL, 0x9B) != par->VCLK3NumeratorLow)
				 || (vga_rgfx(NULL, 0x9F) != par->VCLK3Denominator)
				 || (clock_hi && ((vga_rgfx(NULL, 0x8F) & ~0x0f)
						  != (par->VCLK3NumeratorHigh &
						      ~0x0F))))) {
		vga_wgfx(NULL, 0x9B, par->VCLK3NumeratorLow);
		if (clock_hi) {
			temp = vga_rgfx(NULL, 0x8F);
			temp &= 0x0F;	
			temp |= (par->VCLK3NumeratorHigh & ~0x0F);
			vga_wgfx(NULL, 0x8F, temp);
		}
		vga_wgfx(NULL, 0x9F, par->VCLK3Denominator);
	}

	if (par->biosMode)
		vga_wcrt(NULL, 0x23, par->biosMode);

	vga_wgfx(NULL, 0x93, 0xc0);	

	
	if (info->fix.accel == FB_ACCEL_NEOMAGIC_NM2200 ||
	    info->fix.accel == FB_ACCEL_NEOMAGIC_NM2230 ||
	    info->fix.accel == FB_ACCEL_NEOMAGIC_NM2360 ||
	    info->fix.accel == FB_ACCEL_NEOMAGIC_NM2380) {
		vga_wcrt(NULL, 0x70, par->VerticalExt);
	}

	vgaHWProtect(0);	

	
	neoLock(&par->state);

	info->fix.line_length =
	    info->var.xres_virtual * (info->var.bits_per_pixel >> 3);

	switch (info->fix.accel) {
		case FB_ACCEL_NEOMAGIC_NM2200:
		case FB_ACCEL_NEOMAGIC_NM2230: 
		case FB_ACCEL_NEOMAGIC_NM2360: 
		case FB_ACCEL_NEOMAGIC_NM2380: 
			neo2200_accel_init(info, &info->var);
			break;
		default:
			break;
	}	
	return 0;
}

static int neofb_pan_display(struct fb_var_screeninfo *var,
			     struct fb_info *info)
{
	struct neofb_par *par = info->par;
	struct vgastate *state = &par->state;
	int oldExtCRTDispAddr;
	int Base;

	DBG("neofb_update_start");

	Base = (var->yoffset * info->var.xres_virtual + var->xoffset) >> 2;
	Base *= (info->var.bits_per_pixel + 7) / 8;

	neoUnlock();

	vga_wcrt(state->vgabase, 0x0C, (Base & 0x00FF00) >> 8);
	vga_wcrt(state->vgabase, 0x0D, (Base & 0x00FF));

	oldExtCRTDispAddr = vga_rgfx(NULL, 0x0E);
	vga_wgfx(state->vgabase, 0x0E, (((Base >> 16) & 0x0f) | (oldExtCRTDispAddr & 0xf0)));

	neoLock(state);

	return 0;
}

static int neofb_setcolreg(u_int regno, u_int red, u_int green, u_int blue,
			   u_int transp, struct fb_info *fb)
{
	if (regno >= fb->cmap.len || regno > 255)
		return -EINVAL;

	if (fb->var.bits_per_pixel <= 8) {
		outb(regno, 0x3c8);

		outb(red >> 10, 0x3c9);
		outb(green >> 10, 0x3c9);
		outb(blue >> 10, 0x3c9);
	} else if (regno < 16) {
		switch (fb->var.bits_per_pixel) {
		case 16:
			((u32 *) fb->pseudo_palette)[regno] =
				((red & 0xf800)) | ((green & 0xfc00) >> 5) |
				((blue & 0xf800) >> 11);
			break;
		case 24:
			((u32 *) fb->pseudo_palette)[regno] =
				((red & 0xff00) << 8) | ((green & 0xff00)) |
				((blue & 0xff00) >> 8);
			break;
#ifdef NO_32BIT_SUPPORT_YET
		case 32:
			((u32 *) fb->pseudo_palette)[regno] =
				((transp & 0xff00) << 16) | ((red & 0xff00) << 8) |
				((green & 0xff00)) | ((blue & 0xff00) >> 8);
			break;
#endif
		default:
			return 1;
		}
	}

	return 0;
}

static int neofb_blank(int blank_mode, struct fb_info *info)
{
	struct neofb_par *par = info->par;
	int seqflags, lcdflags, dpmsflags, reg, tmpdisp;

	neoUnlock();
	tmpdisp = vga_rgfx(NULL, 0x20) & 0x03;
	neoLock(&par->state);

	if (par->PanelDispCntlRegRead) {
		par->PanelDispCntlReg1 = tmpdisp;
	}
	par->PanelDispCntlRegRead = !blank_mode;

	switch (blank_mode) {
	case FB_BLANK_POWERDOWN:	
		seqflags = VGA_SR01_SCREEN_OFF; 
		lcdflags = 0;			
		dpmsflags = NEO_GR01_SUPPRESS_HSYNC |
			    NEO_GR01_SUPPRESS_VSYNC;
#ifdef CONFIG_TOSHIBA
		
		
		{
			SMMRegisters regs;

			regs.eax = 0xff00; 
			regs.ebx = 0x0002; 
			regs.ecx = 0x0000; 
			tosh_smm(&regs);
		}
#endif
		break;
	case FB_BLANK_HSYNC_SUSPEND:		
		seqflags = VGA_SR01_SCREEN_OFF;	
		lcdflags = 0;			
		dpmsflags = NEO_GR01_SUPPRESS_HSYNC;
		break;
	case FB_BLANK_VSYNC_SUSPEND:		
		seqflags = VGA_SR01_SCREEN_OFF;	
		lcdflags = 0;			
		dpmsflags = NEO_GR01_SUPPRESS_VSYNC;
		break;
	case FB_BLANK_NORMAL:		
		seqflags = VGA_SR01_SCREEN_OFF;	
		lcdflags = ((par->PanelDispCntlReg1 | tmpdisp) & 0x02); 
		dpmsflags = 0x00;	
		break;
	case FB_BLANK_UNBLANK:		
		seqflags = 0;			
		lcdflags = ((par->PanelDispCntlReg1 | tmpdisp) & 0x02); 
		dpmsflags = 0x00;	
#ifdef CONFIG_TOSHIBA
		
		
		{
			SMMRegisters regs;

			regs.eax = 0xff00; 
			regs.ebx = 0x0002; 
			regs.ecx = 0x0001; 
			tosh_smm(&regs);
		}
#endif
		break;
	default:	
		return 1;
	}

	neoUnlock();
	reg = (vga_rseq(NULL, 0x01) & ~0x20) | seqflags;
	vga_wseq(NULL, 0x01, reg);
	reg = (vga_rgfx(NULL, 0x20) & ~0x02) | lcdflags;
	vga_wgfx(NULL, 0x20, reg);
	reg = (vga_rgfx(NULL, 0x01) & ~0xF0) | 0x80 | dpmsflags;
	vga_wgfx(NULL, 0x01, reg);
	neoLock(&par->state);
	return 0;
}

static void
neo2200_fillrect(struct fb_info *info, const struct fb_fillrect *rect)
{
	struct neofb_par *par = info->par;
	u_long dst, rop;

	dst = rect->dx + rect->dy * info->var.xres_virtual;
	rop = rect->rop ? 0x060000 : 0x0c0000;

	neo2200_wait_fifo(info, 4);

	
	writel(NEO_BC3_FIFO_EN |
	       NEO_BC0_SRC_IS_FG | NEO_BC3_SKIP_MAPPING |
	       
	       
	       rop, &par->neo2200->bltCntl);

	switch (info->var.bits_per_pixel) {
	case 8:
		writel(rect->color, &par->neo2200->fgColor);
		break;
	case 16:
	case 24:
		writel(((u32 *) (info->pseudo_palette))[rect->color],
		       &par->neo2200->fgColor);
		break;
	}

	writel(dst * ((info->var.bits_per_pixel + 7) >> 3),
	       &par->neo2200->dstStart);
	writel((rect->height << 16) | (rect->width & 0xffff),
	       &par->neo2200->xyExt);
}

static void
neo2200_copyarea(struct fb_info *info, const struct fb_copyarea *area)
{
	u32 sx = area->sx, sy = area->sy, dx = area->dx, dy = area->dy;
	struct neofb_par *par = info->par;
	u_long src, dst, bltCntl;

	bltCntl = NEO_BC3_FIFO_EN | NEO_BC3_SKIP_MAPPING | 0x0C0000;

	if ((dy > sy) || ((dy == sy) && (dx > sx))) {
		
		sy += (area->height - 1);
		dy += (area->height - 1);
		sx += (area->width - 1);
		dx += (area->width - 1);

		bltCntl |= NEO_BC0_X_DEC | NEO_BC0_DST_Y_DEC | NEO_BC0_SRC_Y_DEC;
	}

	src = sx * (info->var.bits_per_pixel >> 3) + sy*info->fix.line_length;
	dst = dx * (info->var.bits_per_pixel >> 3) + dy*info->fix.line_length;

	neo2200_wait_fifo(info, 4);

	
	writel(bltCntl, &par->neo2200->bltCntl);

	writel(src, &par->neo2200->srcStart);
	writel(dst, &par->neo2200->dstStart);
	writel((area->height << 16) | (area->width & 0xffff),
	       &par->neo2200->xyExt);
}

static void
neo2200_imageblit(struct fb_info *info, const struct fb_image *image)
{
	struct neofb_par *par = info->par;
	int s_pitch = (image->width * image->depth + 7) >> 3;
	int scan_align = info->pixmap.scan_align - 1;
	int buf_align = info->pixmap.buf_align - 1;
	int bltCntl_flags, d_pitch, data_len;

	
	d_pitch = (s_pitch + scan_align) & ~scan_align;
	data_len = ((d_pitch * image->height) + buf_align) & ~buf_align;

	neo2200_sync(info);

	if (image->depth == 1) {
		if (info->var.bits_per_pixel == 24 && image->width < 16) {
			cfb_imageblit(info, image);
			return;
		}
		bltCntl_flags = NEO_BC0_SRC_MONO;
	} else if (image->depth == info->var.bits_per_pixel) {
		bltCntl_flags = 0;
	} else {
		cfb_imageblit(info, image);
		return;
	}

	switch (info->var.bits_per_pixel) {
	case 8:
		writel(image->fg_color, &par->neo2200->fgColor);
		writel(image->bg_color, &par->neo2200->bgColor);
		break;
	case 16:
	case 24:
		writel(((u32 *) (info->pseudo_palette))[image->fg_color],
		       &par->neo2200->fgColor);
		writel(((u32 *) (info->pseudo_palette))[image->bg_color],
		       &par->neo2200->bgColor);
		break;
	}

	writel(NEO_BC0_SYS_TO_VID |
		NEO_BC3_SKIP_MAPPING | bltCntl_flags |
		
		0x0c0000, &par->neo2200->bltCntl);

	writel(0, &par->neo2200->srcStart);
	writel(((image->dx & 0xffff) * (info->var.bits_per_pixel >> 3) +
		image->dy * info->fix.line_length), &par->neo2200->dstStart);
	writel((image->height << 16) | (image->width & 0xffff),
	       &par->neo2200->xyExt);

	memcpy_toio(par->mmio_vbase + 0x100000, image->data, data_len);
}

static void
neofb_fillrect(struct fb_info *info, const struct fb_fillrect *rect)
{
	switch (info->fix.accel) {
		case FB_ACCEL_NEOMAGIC_NM2200:
		case FB_ACCEL_NEOMAGIC_NM2230: 
		case FB_ACCEL_NEOMAGIC_NM2360: 
		case FB_ACCEL_NEOMAGIC_NM2380:
			neo2200_fillrect(info, rect);
			break;
		default:
			cfb_fillrect(info, rect);
			break;
	}	
}

static void
neofb_copyarea(struct fb_info *info, const struct fb_copyarea *area)
{
	switch (info->fix.accel) {
		case FB_ACCEL_NEOMAGIC_NM2200:
		case FB_ACCEL_NEOMAGIC_NM2230: 
		case FB_ACCEL_NEOMAGIC_NM2360: 
		case FB_ACCEL_NEOMAGIC_NM2380: 
			neo2200_copyarea(info, area);
			break;
		default:
			cfb_copyarea(info, area);
			break;
	}	
}

static void
neofb_imageblit(struct fb_info *info, const struct fb_image *image)
{
	switch (info->fix.accel) {
		case FB_ACCEL_NEOMAGIC_NM2200:
		case FB_ACCEL_NEOMAGIC_NM2230:
		case FB_ACCEL_NEOMAGIC_NM2360:
		case FB_ACCEL_NEOMAGIC_NM2380:
			neo2200_imageblit(info, image);
			break;
		default:
			cfb_imageblit(info, image);
			break;
	}
}

static int 
neofb_sync(struct fb_info *info)
{
	switch (info->fix.accel) {
		case FB_ACCEL_NEOMAGIC_NM2200:
		case FB_ACCEL_NEOMAGIC_NM2230: 
		case FB_ACCEL_NEOMAGIC_NM2360: 
		case FB_ACCEL_NEOMAGIC_NM2380: 
			neo2200_sync(info);
			break;
		default:
			break;
	}
	return 0;		
}


static struct fb_ops neofb_ops = {
	.owner		= THIS_MODULE,
	.fb_open	= neofb_open,
	.fb_release	= neofb_release,
	.fb_check_var	= neofb_check_var,
	.fb_set_par	= neofb_set_par,
	.fb_setcolreg	= neofb_setcolreg,
	.fb_pan_display	= neofb_pan_display,
	.fb_blank	= neofb_blank,
	.fb_sync	= neofb_sync,
	.fb_fillrect	= neofb_fillrect,
	.fb_copyarea	= neofb_copyarea,
	.fb_imageblit	= neofb_imageblit,
};


static struct fb_videomode __devinitdata mode800x480 = {
	.xres           = 800,
	.yres           = 480,
	.pixclock       = 25000,
	.left_margin    = 88,
	.right_margin   = 40,
	.upper_margin   = 23,
	.lower_margin   = 1,
	.hsync_len      = 128,
	.vsync_len      = 4,
	.sync           = FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	.vmode          = FB_VMODE_NONINTERLACED
};

static int __devinit neo_map_mmio(struct fb_info *info,
				  struct pci_dev *dev)
{
	struct neofb_par *par = info->par;

	DBG("neo_map_mmio");

	switch (info->fix.accel) {
		case FB_ACCEL_NEOMAGIC_NM2070:
			info->fix.mmio_start = pci_resource_start(dev, 0)+
				0x100000;
			break;
		case FB_ACCEL_NEOMAGIC_NM2090:
		case FB_ACCEL_NEOMAGIC_NM2093:
			info->fix.mmio_start = pci_resource_start(dev, 0)+
				0x200000;
			break;
		case FB_ACCEL_NEOMAGIC_NM2160:
		case FB_ACCEL_NEOMAGIC_NM2097:
		case FB_ACCEL_NEOMAGIC_NM2200:
		case FB_ACCEL_NEOMAGIC_NM2230:
		case FB_ACCEL_NEOMAGIC_NM2360:
		case FB_ACCEL_NEOMAGIC_NM2380:
			info->fix.mmio_start = pci_resource_start(dev, 1);
			break;
		default:
			info->fix.mmio_start = pci_resource_start(dev, 0);
	}
	info->fix.mmio_len = MMIO_SIZE;

	if (!request_mem_region
	    (info->fix.mmio_start, MMIO_SIZE, "memory mapped I/O")) {
		printk("neofb: memory mapped IO in use\n");
		return -EBUSY;
	}

	par->mmio_vbase = ioremap(info->fix.mmio_start, MMIO_SIZE);
	if (!par->mmio_vbase) {
		printk("neofb: unable to map memory mapped IO\n");
		release_mem_region(info->fix.mmio_start,
				   info->fix.mmio_len);
		return -ENOMEM;
	} else
		printk(KERN_INFO "neofb: mapped io at %p\n",
		       par->mmio_vbase);
	return 0;
}

static void neo_unmap_mmio(struct fb_info *info)
{
	struct neofb_par *par = info->par;

	DBG("neo_unmap_mmio");

	iounmap(par->mmio_vbase);
	par->mmio_vbase = NULL;

	release_mem_region(info->fix.mmio_start,
			   info->fix.mmio_len);
}

static int __devinit neo_map_video(struct fb_info *info,
				   struct pci_dev *dev, int video_len)
{
	

	DBG("neo_map_video");

	info->fix.smem_start = pci_resource_start(dev, 0);
	info->fix.smem_len = video_len;

	if (!request_mem_region(info->fix.smem_start, info->fix.smem_len,
				"frame buffer")) {
		printk("neofb: frame buffer in use\n");
		return -EBUSY;
	}

	info->screen_base =
	    ioremap(info->fix.smem_start, info->fix.smem_len);
	if (!info->screen_base) {
		printk("neofb: unable to map screen memory\n");
		release_mem_region(info->fix.smem_start,
				   info->fix.smem_len);
		return -ENOMEM;
	} else
		printk(KERN_INFO "neofb: mapped framebuffer at %p\n",
		       info->screen_base);

#ifdef CONFIG_MTRR
	((struct neofb_par *)(info->par))->mtrr =
		mtrr_add(info->fix.smem_start, pci_resource_len(dev, 0),
				MTRR_TYPE_WRCOMB, 1);
#endif

	
	memset_io(info->screen_base, 0, info->fix.smem_len);

	return 0;
}

static void neo_unmap_video(struct fb_info *info)
{
	DBG("neo_unmap_video");

#ifdef CONFIG_MTRR
	{
		struct neofb_par *par = info->par;

		mtrr_del(par->mtrr, info->fix.smem_start,
			 info->fix.smem_len);
	}
#endif
	iounmap(info->screen_base);
	info->screen_base = NULL;

	release_mem_region(info->fix.smem_start,
			   info->fix.smem_len);
}

static int __devinit neo_scan_monitor(struct fb_info *info)
{
	struct neofb_par *par = info->par;
	unsigned char type, display;
	int w;

	
	info->monspecs.modedb = kmalloc(sizeof(struct fb_videomode), GFP_KERNEL);
	if (!info->monspecs.modedb)
		return -ENOMEM;
	info->monspecs.modedb_len = 1;

	
	vga_wgfx(NULL, 0x09, 0x26);
	type = vga_rgfx(NULL, 0x21);
	display = vga_rgfx(NULL, 0x20);
	if (!par->internal_display && !par->external_display) {
		par->internal_display = display & 2 || !(display & 3) ? 1 : 0;
		par->external_display = display & 1;
		printk (KERN_INFO "Autodetected %s display\n",
			par->internal_display && par->external_display ? "simultaneous" :
			par->internal_display ? "internal" : "external");
	}

	
	w = vga_rgfx(NULL, 0x20);
	vga_wgfx(NULL, 0x09, 0x00);
	switch ((w & 0x18) >> 3) {
	case 0x00:
		
		par->NeoPanelWidth = 640;
		par->NeoPanelHeight = 480;
		memcpy(info->monspecs.modedb, &vesa_modes[3], sizeof(struct fb_videomode));
		break;
	case 0x01:
		par->NeoPanelWidth = 800;
		if (par->libretto) {
			par->NeoPanelHeight = 480;
			memcpy(info->monspecs.modedb, &mode800x480, sizeof(struct fb_videomode));
		} else {
			
			par->NeoPanelHeight = 600;
			memcpy(info->monspecs.modedb, &vesa_modes[8], sizeof(struct fb_videomode));
		}
		break;
	case 0x02:
		
		par->NeoPanelWidth = 1024;
		par->NeoPanelHeight = 768;
		memcpy(info->monspecs.modedb, &vesa_modes[13], sizeof(struct fb_videomode));
		break;
	case 0x03:
		
#ifdef NOT_DONE
		par->NeoPanelWidth = 1280;
		par->NeoPanelHeight = 1024;
		memcpy(info->monspecs.modedb, &vesa_modes[20], sizeof(struct fb_videomode));
		break;
#else
		printk(KERN_ERR
		       "neofb: Only 640x480, 800x600/480 and 1024x768 panels are currently supported\n");
		return -1;
#endif
	default:
		
		par->NeoPanelWidth = 640;
		par->NeoPanelHeight = 480;
		memcpy(info->monspecs.modedb, &vesa_modes[3], sizeof(struct fb_videomode));
		break;
	}

	printk(KERN_INFO "Panel is a %dx%d %s %s display\n",
	       par->NeoPanelWidth,
	       par->NeoPanelHeight,
	       (type & 0x02) ? "color" : "monochrome",
	       (type & 0x10) ? "TFT" : "dual scan");
	return 0;
}

static int __devinit neo_init_hw(struct fb_info *info)
{
	struct neofb_par *par = info->par;
	int videoRam = 896;
	int maxClock = 65000;
	int CursorMem = 1024;
	int CursorOff = 0x100;

	DBG("neo_init_hw");

	neoUnlock();

#if 0
	printk(KERN_DEBUG "--- Neo extended register dump ---\n");
	for (int w = 0; w < 0x85; w++)
		printk(KERN_DEBUG "CR %p: %p\n", (void *) w,
		       (void *) vga_rcrt(NULL, w));
	for (int w = 0; w < 0xC7; w++)
		printk(KERN_DEBUG "GR %p: %p\n", (void *) w,
		       (void *) vga_rgfx(NULL, w));
#endif
	switch (info->fix.accel) {
	case FB_ACCEL_NEOMAGIC_NM2070:
		videoRam = 896;
		maxClock = 65000;
		break;
	case FB_ACCEL_NEOMAGIC_NM2090:
	case FB_ACCEL_NEOMAGIC_NM2093:
	case FB_ACCEL_NEOMAGIC_NM2097:
		videoRam = 1152;
		maxClock = 80000;
		break;
	case FB_ACCEL_NEOMAGIC_NM2160:
		videoRam = 2048;
		maxClock = 90000;
		break;
	case FB_ACCEL_NEOMAGIC_NM2200:
		videoRam = 2560;
		maxClock = 110000;
		break;
	case FB_ACCEL_NEOMAGIC_NM2230:
		videoRam = 3008;
		maxClock = 110000;
		break;
	case FB_ACCEL_NEOMAGIC_NM2360:
		videoRam = 4096;
		maxClock = 110000;
		break;
	case FB_ACCEL_NEOMAGIC_NM2380:
		videoRam = 6144;
		maxClock = 110000;
		break;
	}
	switch (info->fix.accel) {
	case FB_ACCEL_NEOMAGIC_NM2070:
	case FB_ACCEL_NEOMAGIC_NM2090:
	case FB_ACCEL_NEOMAGIC_NM2093:
		CursorMem = 2048;
		CursorOff = 0x100;
		break;
	case FB_ACCEL_NEOMAGIC_NM2097:
	case FB_ACCEL_NEOMAGIC_NM2160:
		CursorMem = 1024;
		CursorOff = 0x100;
		break;
	case FB_ACCEL_NEOMAGIC_NM2200:
	case FB_ACCEL_NEOMAGIC_NM2230:
	case FB_ACCEL_NEOMAGIC_NM2360:
	case FB_ACCEL_NEOMAGIC_NM2380:
		CursorMem = 1024;
		CursorOff = 0x1000;

		par->neo2200 = (Neo2200 __iomem *) par->mmio_vbase;
		break;
	}
	par->maxClock = maxClock;
	par->cursorOff = CursorOff;
	return videoRam * 1024;
}


static struct fb_info *__devinit neo_alloc_fb_info(struct pci_dev *dev, const struct
						   pci_device_id *id)
{
	struct fb_info *info;
	struct neofb_par *par;

	info = framebuffer_alloc(sizeof(struct neofb_par), &dev->dev);

	if (!info)
		return NULL;

	par = info->par;

	info->fix.accel = id->driver_data;

	par->pci_burst = !nopciburst;
	par->lcd_stretch = !nostretch;
	par->libretto = libretto;

	par->internal_display = internal;
	par->external_display = external;
	info->flags = FBINFO_DEFAULT | FBINFO_HWACCEL_YPAN;

	switch (info->fix.accel) {
	case FB_ACCEL_NEOMAGIC_NM2070:
		snprintf(info->fix.id, sizeof(info->fix.id),
				"MagicGraph 128");
		break;
	case FB_ACCEL_NEOMAGIC_NM2090:
		snprintf(info->fix.id, sizeof(info->fix.id),
				"MagicGraph 128V");
		break;
	case FB_ACCEL_NEOMAGIC_NM2093:
		snprintf(info->fix.id, sizeof(info->fix.id),
				"MagicGraph 128ZV");
		break;
	case FB_ACCEL_NEOMAGIC_NM2097:
		snprintf(info->fix.id, sizeof(info->fix.id),
				"MagicGraph 128ZV+");
		break;
	case FB_ACCEL_NEOMAGIC_NM2160:
		snprintf(info->fix.id, sizeof(info->fix.id),
				"MagicGraph 128XD");
		break;
	case FB_ACCEL_NEOMAGIC_NM2200:
		snprintf(info->fix.id, sizeof(info->fix.id),
				"MagicGraph 256AV");
		info->flags |= FBINFO_HWACCEL_IMAGEBLIT |
		               FBINFO_HWACCEL_COPYAREA |
                	       FBINFO_HWACCEL_FILLRECT;
		break;
	case FB_ACCEL_NEOMAGIC_NM2230:
		snprintf(info->fix.id, sizeof(info->fix.id),
				"MagicGraph 256AV+");
		info->flags |= FBINFO_HWACCEL_IMAGEBLIT |
		               FBINFO_HWACCEL_COPYAREA |
                	       FBINFO_HWACCEL_FILLRECT;
		break;
	case FB_ACCEL_NEOMAGIC_NM2360:
		snprintf(info->fix.id, sizeof(info->fix.id),
				"MagicGraph 256ZX");
		info->flags |= FBINFO_HWACCEL_IMAGEBLIT |
		               FBINFO_HWACCEL_COPYAREA |
                	       FBINFO_HWACCEL_FILLRECT;
		break;
	case FB_ACCEL_NEOMAGIC_NM2380:
		snprintf(info->fix.id, sizeof(info->fix.id),
				"MagicGraph 256XL+");
		info->flags |= FBINFO_HWACCEL_IMAGEBLIT |
		               FBINFO_HWACCEL_COPYAREA |
                	       FBINFO_HWACCEL_FILLRECT;
		break;
	}

	info->fix.type = FB_TYPE_PACKED_PIXELS;
	info->fix.type_aux = 0;
	info->fix.xpanstep = 0;
	info->fix.ypanstep = 4;
	info->fix.ywrapstep = 0;
	info->fix.accel = id->driver_data;

	info->fbops = &neofb_ops;
	info->pseudo_palette = par->palette;
	return info;
}

static void neo_free_fb_info(struct fb_info *info)
{
	if (info) {
		fb_dealloc_cmap(&info->cmap);
		framebuffer_release(info);
	}
}


static int __devinit neofb_probe(struct pci_dev *dev,
				 const struct pci_device_id *id)
{
	struct fb_info *info;
	u_int h_sync, v_sync;
	int video_len, err;

	DBG("neofb_probe");

	err = pci_enable_device(dev);
	if (err)
		return err;

	err = -ENOMEM;
	info = neo_alloc_fb_info(dev, id);
	if (!info)
		return err;

	err = neo_map_mmio(info, dev);
	if (err)
		goto err_map_mmio;

	err = neo_scan_monitor(info);
	if (err)
		goto err_scan_monitor;

	video_len = neo_init_hw(info);
	if (video_len < 0) {
		err = video_len;
		goto err_init_hw;
	}

	err = neo_map_video(info, dev, video_len);
	if (err)
		goto err_init_hw;

	if (!fb_find_mode(&info->var, info, mode_option, NULL, 0,
			info->monspecs.modedb, 16)) {
		printk(KERN_ERR "neofb: Unable to find usable video mode.\n");
		goto err_map_video;
	}

	h_sync = 1953125000 / info->var.pixclock;
	h_sync =
	    h_sync * 512 / (info->var.xres + info->var.left_margin +
			    info->var.right_margin + info->var.hsync_len);
	v_sync =
	    h_sync / (info->var.yres + info->var.upper_margin +
		      info->var.lower_margin + info->var.vsync_len);

	printk(KERN_INFO "neofb v" NEOFB_VERSION
	       ": %dkB VRAM, using %dx%d, %d.%03dkHz, %dHz\n",
	       info->fix.smem_len >> 10, info->var.xres,
	       info->var.yres, h_sync / 1000, h_sync % 1000, v_sync);

	if (fb_alloc_cmap(&info->cmap, 256, 0) < 0)
		goto err_map_video;

	err = register_framebuffer(info);
	if (err < 0)
		goto err_reg_fb;

	printk(KERN_INFO "fb%d: %s frame buffer device\n",
	       info->node, info->fix.id);

	pci_set_drvdata(dev, info);
	return 0;

err_reg_fb:
	fb_dealloc_cmap(&info->cmap);
err_map_video:
	neo_unmap_video(info);
err_init_hw:
	fb_destroy_modedb(info->monspecs.modedb);
err_scan_monitor:
	neo_unmap_mmio(info);
err_map_mmio:
	neo_free_fb_info(info);
	return err;
}

static void __devexit neofb_remove(struct pci_dev *dev)
{
	struct fb_info *info = pci_get_drvdata(dev);

	DBG("neofb_remove");

	if (info) {
		if (unregister_framebuffer(info))
			printk(KERN_WARNING
			       "neofb: danger danger!  Oopsen imminent!\n");

		neo_unmap_video(info);
		fb_destroy_modedb(info->monspecs.modedb);
		neo_unmap_mmio(info);
		neo_free_fb_info(info);

		pci_set_drvdata(dev, NULL);
	}
}

static struct pci_device_id neofb_devices[] = {
	{PCI_VENDOR_ID_NEOMAGIC, PCI_CHIP_NM2070,
	 PCI_ANY_ID, PCI_ANY_ID, 0, 0, FB_ACCEL_NEOMAGIC_NM2070},

	{PCI_VENDOR_ID_NEOMAGIC, PCI_CHIP_NM2090,
	 PCI_ANY_ID, PCI_ANY_ID, 0, 0, FB_ACCEL_NEOMAGIC_NM2090},

	{PCI_VENDOR_ID_NEOMAGIC, PCI_CHIP_NM2093,
	 PCI_ANY_ID, PCI_ANY_ID, 0, 0, FB_ACCEL_NEOMAGIC_NM2093},

	{PCI_VENDOR_ID_NEOMAGIC, PCI_CHIP_NM2097,
	 PCI_ANY_ID, PCI_ANY_ID, 0, 0, FB_ACCEL_NEOMAGIC_NM2097},

	{PCI_VENDOR_ID_NEOMAGIC, PCI_CHIP_NM2160,
	 PCI_ANY_ID, PCI_ANY_ID, 0, 0, FB_ACCEL_NEOMAGIC_NM2160},

	{PCI_VENDOR_ID_NEOMAGIC, PCI_CHIP_NM2200,
	 PCI_ANY_ID, PCI_ANY_ID, 0, 0, FB_ACCEL_NEOMAGIC_NM2200},

	{PCI_VENDOR_ID_NEOMAGIC, PCI_CHIP_NM2230,
	 PCI_ANY_ID, PCI_ANY_ID, 0, 0, FB_ACCEL_NEOMAGIC_NM2230},

	{PCI_VENDOR_ID_NEOMAGIC, PCI_CHIP_NM2360,
	 PCI_ANY_ID, PCI_ANY_ID, 0, 0, FB_ACCEL_NEOMAGIC_NM2360},

	{PCI_VENDOR_ID_NEOMAGIC, PCI_CHIP_NM2380,
	 PCI_ANY_ID, PCI_ANY_ID, 0, 0, FB_ACCEL_NEOMAGIC_NM2380},

	{0, 0, 0, 0, 0, 0, 0}
};

MODULE_DEVICE_TABLE(pci, neofb_devices);

static struct pci_driver neofb_driver = {
	.name =		"neofb",
	.id_table =	neofb_devices,
	.probe =	neofb_probe,
	.remove =	__devexit_p(neofb_remove)
};


#ifndef MODULE
static int __init neofb_setup(char *options)
{
	char *this_opt;

	DBG("neofb_setup");

	if (!options || !*options)
		return 0;

	while ((this_opt = strsep(&options, ",")) != NULL) {
		if (!*this_opt)
			continue;

		if (!strncmp(this_opt, "internal", 8))
			internal = 1;
		else if (!strncmp(this_opt, "external", 8))
			external = 1;
		else if (!strncmp(this_opt, "nostretch", 9))
			nostretch = 1;
		else if (!strncmp(this_opt, "nopciburst", 10))
			nopciburst = 1;
		else if (!strncmp(this_opt, "libretto", 8))
			libretto = 1;
		else
			mode_option = this_opt;
	}
	return 0;
}
#endif  

static int __init neofb_init(void)
{
#ifndef MODULE
	char *option = NULL;

	if (fb_get_options("neofb", &option))
		return -ENODEV;
	neofb_setup(option);
#endif
	return pci_register_driver(&neofb_driver);
}

module_init(neofb_init);

#ifdef MODULE
static void __exit neofb_exit(void)
{
	pci_unregister_driver(&neofb_driver);
}

module_exit(neofb_exit);
#endif				
