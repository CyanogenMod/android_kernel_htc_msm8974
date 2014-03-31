/*
 *  linux/drivers/video/macmodes.c -- Standard MacOS video modes
 *
 *	Copyright (C) 1998 Geert Uytterhoeven
 *
 *      2000 - Removal of OpenFirmware dependencies by:
 *      - Ani Joshi
 *      - Brad Douglas <brad@neruo.com>
 *
 *	2001 - Documented with DocBook
 *	- Brad Douglas <brad@neruo.com>
 *
 *  This file is subject to the terms and conditions of the GNU General Public
 *  License. See the file COPYING in the main directory of this archive for
 *  more details.
 */

#include <linux/errno.h>
#include <linux/fb.h>
#include <linux/string.h>
#include <linux/module.h>

#include "macmodes.h"


#define DEFAULT_MODEDB_INDEX	0

static const struct fb_videomode mac_modedb[] = {
    {
	
	"mac2", 60, 512, 384, 63828, 80, 16, 19, 1, 32, 3,
	0, FB_VMODE_NONINTERLACED
    }, {
	
	"mac5", 60, 640, 480, 39722, 32, 32, 33, 10, 96, 2,
	0, FB_VMODE_NONINTERLACED
    }, {
	
	"mac6", 67, 640, 480, 33334, 80, 80, 39, 3, 64, 3,
	0, FB_VMODE_NONINTERLACED
    }, {
	
	"mac7", 75, 640, 870, 17457, 80, 32, 42, 3, 80, 3,
	0, FB_VMODE_NONINTERLACED
    }, {
	
	"mac9", 56, 800, 600, 27778, 112, 40, 22, 1, 72, 2,
	FB_SYNC_HOR_HIGH_ACT|FB_SYNC_VERT_HIGH_ACT, FB_VMODE_NONINTERLACED
    }, {
	
	"mac10", 60, 800, 600, 25000, 72, 56, 23, 1, 128, 4,
	FB_SYNC_HOR_HIGH_ACT|FB_SYNC_VERT_HIGH_ACT, FB_VMODE_NONINTERLACED
    }, {
	
	"mac11", 72, 800, 600, 20000, 48, 72, 23, 37, 120, 6,
	FB_SYNC_HOR_HIGH_ACT|FB_SYNC_VERT_HIGH_ACT, FB_VMODE_NONINTERLACED
    }, {
	
	"mac12", 75, 800, 600, 20203, 144, 32, 21, 1, 80, 3,
	FB_SYNC_HOR_HIGH_ACT|FB_SYNC_VERT_HIGH_ACT, FB_VMODE_NONINTERLACED
    }, {
	
	"mac13", 75, 832, 624, 17362, 208, 48, 39, 1, 64, 3,
	0, FB_VMODE_NONINTERLACED
    }, {
	
	"mac14", 60, 1024, 768, 15385, 144, 40, 29, 3, 136, 6,
	0, FB_VMODE_NONINTERLACED
    }, {
	
	"mac15", 72, 1024, 768, 13334, 128, 40, 29, 3, 136, 6,
	0, FB_VMODE_NONINTERLACED
    }, {
	
	"mac16", 75, 1024, 768, 12699, 176, 16, 28, 1, 96, 3,
	FB_SYNC_HOR_HIGH_ACT|FB_SYNC_VERT_HIGH_ACT, FB_VMODE_NONINTERLACED
    }, {
	
	"mac17", 75, 1024, 768, 12699, 160, 32, 28, 1, 96, 3,
	FB_SYNC_HOR_HIGH_ACT|FB_SYNC_VERT_HIGH_ACT, FB_VMODE_NONINTERLACED
    }, {
	
	"mac18", 75, 1152, 870, 10000, 128, 48, 39, 3, 128, 3,
	FB_SYNC_HOR_HIGH_ACT|FB_SYNC_VERT_HIGH_ACT, FB_VMODE_NONINTERLACED
    }, {
	
	"mac19", 75, 1280, 960, 7937, 224, 32, 36, 1, 144, 3,
	0, FB_VMODE_NONINTERLACED
    }, {
	
	"mac20", 75, 1280, 1024, 7408, 232, 64, 38, 1, 112, 3,
	FB_SYNC_HOR_HIGH_ACT|FB_SYNC_VERT_HIGH_ACT, FB_VMODE_NONINTERLACED
    }, {
	
	"mac21", 60, 1152, 768, 15386, 158, 26, 29, 3, 136, 6,
	FB_SYNC_HOR_HIGH_ACT|FB_SYNC_VERT_HIGH_ACT, FB_VMODE_NONINTERLACED
    }, {
	
	"mac22", 60, 1600, 1024, 8908, 88, 104, 1, 10, 16, 1,
	FB_SYNC_HOR_HIGH_ACT|FB_SYNC_VERT_HIGH_ACT, FB_VMODE_NONINTERLACED
    }

#if 0
    
    {
	
	"mac1", 60, 512, 384, pixclock, left, right, upper, lower, hslen, vslen,
	sync, FB_VMODE_INTERLACED
    }, {
	
	"mac3", 50, 640, 480, pixclock, left, right, upper, lower, hslen, vslen,
	sync, FB_VMODE_INTERLACED
    }, {
	
	"mac4", 60, 640, 480, pixclock, left, right, upper, lower, hslen, vslen,
	sync, FB_VMODE_INTERLACED
    }, {
	
	"mac8", 50, 768, 576, pixclock, left, right, upper, lower, hslen, vslen,
	sync, FB_VMODE_INTERLACED
    },
#endif
};



static const struct mode_map {
    int vmode;
    const struct fb_videomode *mode;
} mac_modes[] = {
    
    { VMODE_512_384_60, &mac_modedb[0] },
    
    { VMODE_640_480_60, &mac_modedb[1] },
    { VMODE_640_480_67, &mac_modedb[2] },
    
    { VMODE_640_870_75P, &mac_modedb[3] },
    
    { VMODE_800_600_56, &mac_modedb[4] },
    { VMODE_800_600_60, &mac_modedb[5] },
    { VMODE_800_600_75, &mac_modedb[7] },
    { VMODE_800_600_72, &mac_modedb[6] },
    
    { VMODE_832_624_75, &mac_modedb[8] },
    
    { VMODE_1024_768_60, &mac_modedb[9] },
    { VMODE_1024_768_70, &mac_modedb[10] },
    { VMODE_1024_768_75V, &mac_modedb[11] },
    { VMODE_1024_768_75, &mac_modedb[12] },
    
    { VMODE_1152_768_60, &mac_modedb[16] },
    
    { VMODE_1152_870_75, &mac_modedb[13] },
    
    { VMODE_1280_960_75, &mac_modedb[14] },
    
    { VMODE_1280_1024_75, &mac_modedb[15] },
    
    { VMODE_1600_1024_60, &mac_modedb[17] },
    { -1, NULL }
};



static const struct monitor_map {
    int sense;
    int vmode;
} mac_monitors[] = {
    { 0x000, VMODE_1280_1024_75 },	
    { 0x114, VMODE_640_870_75P },	
    { 0x221, VMODE_512_384_60 },	
    { 0x331, VMODE_1280_1024_75 },	
    { 0x334, VMODE_1280_1024_75 },	
    { 0x335, VMODE_1280_1024_75 },	
    { 0x40A, VMODE_640_480_60I },	
    { 0x51E, VMODE_640_870_75P },	
    { 0x603, VMODE_832_624_75 },	
    { 0x60b, VMODE_1024_768_70 },	
    { 0x623, VMODE_1152_870_75 },	
    { 0x62b, VMODE_640_480_67 },	
    { 0x700, VMODE_640_480_50I },	
    { 0x714, VMODE_640_480_60I },	
    { 0x717, VMODE_800_600_75 },	
    { 0x72d, VMODE_832_624_75 },	
    { 0x730, VMODE_768_576_50I },	
    { 0x73a, VMODE_1152_870_75 },	
    { 0x73f, VMODE_640_480_67 },	
    { 0xBEEF, VMODE_1600_1024_60 },	
    { -1,    VMODE_640_480_60 },	
};


int mac_vmode_to_var(int vmode, int cmode, struct fb_var_screeninfo *var)
{
    const struct fb_videomode *mode = NULL;
    const struct mode_map *map;

    for (map = mac_modes; map->vmode != -1; map++)
	if (map->vmode == vmode) {
	    mode = map->mode;
	    break;
	}
    if (!mode)
	return -EINVAL;

    memset(var, 0, sizeof(struct fb_var_screeninfo));
    switch (cmode) {
	case CMODE_8:
	    var->bits_per_pixel = 8;
	    var->red.offset = 0;
	    var->red.length = 8;   
	    var->green.offset = 0;
	    var->green.length = 8;
	    var->blue.offset = 0;
	    var->blue.length = 8;
	    break;

	case CMODE_16:
	    var->bits_per_pixel = 16;
	    var->red.offset = 10;
	    var->red.length = 5;
	    var->green.offset = 5;
	    var->green.length = 5;
	    var->blue.offset = 0;
	    var->blue.length = 5;
	    break;

	case CMODE_32:
	    var->bits_per_pixel = 32;
	    var->red.offset = 16;
	    var->red.length = 8;
	    var->green.offset = 8;
	    var->green.length = 8;
	    var->blue.offset = 0;
	    var->blue.length = 8;
	    var->transp.offset = 24;
	    var->transp.length = 8;
	    break;

	default:
	    return -EINVAL;
    }
    var->xres = mode->xres;
    var->yres = mode->yres;
    var->xres_virtual = mode->xres;
    var->yres_virtual = mode->yres;
    var->height = -1;
    var->width = -1;
    var->pixclock = mode->pixclock;
    var->left_margin = mode->left_margin;
    var->right_margin = mode->right_margin;
    var->upper_margin = mode->upper_margin;
    var->lower_margin = mode->lower_margin;
    var->hsync_len = mode->hsync_len;
    var->vsync_len = mode->vsync_len;
    var->sync = mode->sync;
    var->vmode = mode->vmode;
    return 0;
}
EXPORT_SYMBOL(mac_vmode_to_var);


int mac_var_to_vmode(const struct fb_var_screeninfo *var, int *vmode,
		     int *cmode)
{
    const struct mode_map *map;

    if (var->bits_per_pixel <= 8)
	*cmode = CMODE_8;
    else if (var->bits_per_pixel <= 16)
	*cmode = CMODE_16;
    else if (var->bits_per_pixel <= 32)
	*cmode = CMODE_32;
    else
	return -EINVAL;

    for (map = mac_modes; map->vmode != -1; map++) {
	const struct fb_videomode *mode = map->mode;

	if (var->xres > mode->xres || var->yres > mode->yres)
	    continue;
	if (var->xres_virtual > mode->xres || var->yres_virtual > mode->yres)
	    continue;
	if (var->pixclock > mode->pixclock)
	    continue;
	if ((var->vmode & FB_VMODE_MASK) != mode->vmode)
	    continue;
	*vmode = map->vmode;

	map++;
	while (map->vmode != -1) {
	    const struct fb_videomode *clk_mode = map->mode;

	    if (mode->xres != clk_mode->xres || mode->yres != clk_mode->yres)
		break;
	    if (var->pixclock > mode->pixclock)
	        break;
	    if (mode->vmode != clk_mode->vmode)
		continue;
	    *vmode = map->vmode;
	    map++;
	}
	return 0;
    }
    return -EINVAL;
}


int mac_map_monitor_sense(int sense)
{
    const struct monitor_map *map;

    for (map = mac_monitors; map->sense != -1; map++)
	if (map->sense == sense)
	    break;
    return map->vmode;
}
EXPORT_SYMBOL(mac_map_monitor_sense);


int mac_find_mode(struct fb_var_screeninfo *var, struct fb_info *info,
		  const char *mode_option, unsigned int default_bpp)
{
    const struct fb_videomode *db = NULL;
    unsigned int dbsize = 0;

    if (mode_option && !strncmp(mode_option, "mac", 3)) {
	mode_option += 3;
	db = mac_modedb;
	dbsize = ARRAY_SIZE(mac_modedb);
    }
    return fb_find_mode(var, info, mode_option, db, dbsize,
			&mac_modedb[DEFAULT_MODEDB_INDEX], default_bpp);
}
EXPORT_SYMBOL(mac_find_mode);

MODULE_LICENSE("GPL");
