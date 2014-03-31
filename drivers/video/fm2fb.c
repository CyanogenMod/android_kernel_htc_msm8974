/*
 *  linux/drivers/video/fm2fb.c -- BSC FrameMaster II/Rainbow II frame buffer
 *				   device
 *
 *	Copyright (C) 1998 Steffen A. Mork (linux-dev@morknet.de)
 *	Copyright (C) 1999 Geert Uytterhoeven
 *
 *  Written for 2.0.x by Steffen A. Mork
 *  Ported to 2.1.x by Geert Uytterhoeven
 *  Ported to new api by James Simmons
 *
 *  This file is subject to the terms and conditions of the GNU General Public
 *  License. See the file COPYING in the main directory of this archive for
 *  more details.
 */

#include <linux/module.h>
#include <linux/mm.h>
#include <linux/fb.h>
#include <linux/init.h>
#include <linux/zorro.h>
#include <asm/io.h>



#define FRAMEMASTER_SIZE	0x200000
#define FRAMEMASTER_REG		0x1ffff8

#define FRAMEMASTER_NOLACE	1
#define FRAMEMASTER_ENABLE	2
#define FRAMEMASTER_COMPL	4
#define FRAMEMASTER_ROM		8

static volatile unsigned char *fm2fb_reg;

static struct fb_fix_screeninfo fb_fix __devinitdata = {
	.smem_len =	FRAMEMASTER_REG,
	.type =		FB_TYPE_PACKED_PIXELS,
	.visual =	FB_VISUAL_TRUECOLOR,
	.line_length =	(768 << 2),
	.mmio_len =	(8),
	.accel =	FB_ACCEL_NONE,
};

static int fm2fb_mode __devinitdata = -1;

#define FM2FB_MODE_PAL	0
#define FM2FB_MODE_NTSC	1

static struct fb_var_screeninfo fb_var_modes[] __devinitdata = {
    {
	
	768, 576, 768, 576, 0, 0, 32, 0,
	{ 16, 8, 0 }, { 8, 8, 0 }, { 0, 8, 0 }, { 24, 8, 0 },
	0, FB_ACTIVATE_NOW, -1, -1, FB_ACCEL_NONE,
	33333, 10, 102, 10, 5, 80, 34, FB_SYNC_COMP_HIGH_ACT, 0
    }, {
	
	768, 480, 768, 480, 0, 0, 32, 0,
	{ 16, 8, 0 }, { 8, 8, 0 }, { 0, 8, 0 }, { 24, 8, 0 },
	0, FB_ACTIVATE_NOW, -1, -1, FB_ACCEL_NONE,
	33333, 10, 102, 10, 5, 80, 34, FB_SYNC_COMP_HIGH_ACT, 0
    }
};


static int fm2fb_setcolreg(u_int regno, u_int red, u_int green, u_int blue,
                           u_int transp, struct fb_info *info);
static int fm2fb_blank(int blank, struct fb_info *info);

static struct fb_ops fm2fb_ops = {
	.owner		= THIS_MODULE,
	.fb_setcolreg	= fm2fb_setcolreg,
	.fb_blank	= fm2fb_blank,
	.fb_fillrect	= cfb_fillrect,
	.fb_copyarea	= cfb_copyarea,
	.fb_imageblit	= cfb_imageblit,
};

static int fm2fb_blank(int blank, struct fb_info *info)
{
	unsigned char t = FRAMEMASTER_ROM;

	if (!blank)
		t |= FRAMEMASTER_ENABLE | FRAMEMASTER_NOLACE;
	fm2fb_reg[0] = t;
	return 0;
}

static int fm2fb_setcolreg(u_int regno, u_int red, u_int green, u_int blue,
                         u_int transp, struct fb_info *info)
{
	if (regno < 16) {
		red >>= 8;
		green >>= 8;
		blue >>= 8;

		((u32*)(info->pseudo_palette))[regno] = (red << 16) |
			(green << 8) | blue;
	}

	return 0;
}


static int __devinit fm2fb_probe(struct zorro_dev *z,
				 const struct zorro_device_id *id);

static struct zorro_device_id fm2fb_devices[] __devinitdata = {
	{ ZORRO_PROD_BSC_FRAMEMASTER_II },
	{ ZORRO_PROD_HELFRICH_RAINBOW_II },
	{ 0 }
};
MODULE_DEVICE_TABLE(zorro, fm2fb_devices);

static struct zorro_driver fm2fb_driver = {
	.name		= "fm2fb",
	.id_table	= fm2fb_devices,
	.probe		= fm2fb_probe,
};

static int __devinit fm2fb_probe(struct zorro_dev *z,
				 const struct zorro_device_id *id)
{
	struct fb_info *info;
	unsigned long *ptr;
	int is_fm;
	int x, y;

	is_fm = z->id == ZORRO_PROD_BSC_FRAMEMASTER_II;

	if (!zorro_request_device(z,"fm2fb"))
		return -ENXIO;

	info = framebuffer_alloc(16 * sizeof(u32), &z->dev);
	if (!info) {
		zorro_release_device(z);
		return -ENOMEM;
	}

	if (fb_alloc_cmap(&info->cmap, 256, 0) < 0) {
		framebuffer_release(info);
		zorro_release_device(z);
		return -ENOMEM;
	}

	
	fb_fix.smem_start = zorro_resource_start(z);
	info->screen_base = ioremap(fb_fix.smem_start, FRAMEMASTER_SIZE);
	fb_fix.mmio_start = fb_fix.smem_start + FRAMEMASTER_REG;
	fm2fb_reg  = (unsigned char *)(info->screen_base+FRAMEMASTER_REG);

	strcpy(fb_fix.id, is_fm ? "FrameMaster II" : "Rainbow II");

	
	ptr = (unsigned long *)fb_fix.smem_start;
	for (y = 0; y < 576; y++) {
		for (x = 0; x < 96; x++) *ptr++ = 0xffffff;
		for (x = 0; x < 96; x++) *ptr++ = 0xffff00;
		for (x = 0; x < 96; x++) *ptr++ = 0x00ffff;
		for (x = 0; x < 96; x++) *ptr++ = 0x00ff00;
		for (x = 0; x < 96; x++) *ptr++ = 0xff00ff;
		for (x = 0; x < 96; x++) *ptr++ = 0xff0000;
		for (x = 0; x < 96; x++) *ptr++ = 0x0000ff;
		for (x = 0; x < 96; x++) *ptr++ = 0x000000;
	}
	fm2fb_blank(0, info);

	if (fm2fb_mode == -1)
		fm2fb_mode = FM2FB_MODE_PAL;

	info->fbops = &fm2fb_ops;
	info->var = fb_var_modes[fm2fb_mode];
	info->pseudo_palette = info->par;
	info->par = NULL;
	info->fix = fb_fix;
	info->flags = FBINFO_DEFAULT;

	if (register_framebuffer(info) < 0) {
		fb_dealloc_cmap(&info->cmap);
		iounmap(info->screen_base);
		framebuffer_release(info);
		zorro_release_device(z);
		return -EINVAL;
	}
	printk("fb%d: %s frame buffer device\n", info->node, fb_fix.id);
	return 0;
}

int __init fm2fb_setup(char *options)
{
	char *this_opt;

	if (!options || !*options)
		return 0;

	while ((this_opt = strsep(&options, ",")) != NULL) {
		if (!strncmp(this_opt, "pal", 3))
			fm2fb_mode = FM2FB_MODE_PAL;
		else if (!strncmp(this_opt, "ntsc", 4))
			fm2fb_mode = FM2FB_MODE_NTSC;
	}
	return 0;
}

int __init fm2fb_init(void)
{
	char *option = NULL;

	if (fb_get_options("fm2fb", &option))
		return -ENODEV;
	fm2fb_setup(option);
	return zorro_register_driver(&fm2fb_driver);
}

module_init(fm2fb_init);
MODULE_LICENSE("GPL");
