/*
 *  linux/drivers/video/macmodes.h -- Standard MacOS video modes
 *
 *	Copyright (C) 1998 Geert Uytterhoeven
 *
 *  This file is subject to the terms and conditions of the GNU General Public
 *  License. See the file COPYING in the main directory of this archive for
 *  more details.
 */

#ifndef _VIDEO_MACMODES_H
#define _VIDEO_MACMODES_H


#define VMODE_NVRAM		0
#define VMODE_512_384_60I	1	
#define VMODE_512_384_60	2	
#define VMODE_640_480_50I	3	
#define VMODE_640_480_60I	4	
#define VMODE_640_480_60	5	
#define VMODE_640_480_67	6	
#define VMODE_640_870_75P	7	
#define VMODE_768_576_50I	8	
#define VMODE_800_600_56	9	
#define VMODE_800_600_60	10	
#define VMODE_800_600_72	11	
#define VMODE_800_600_75	12	
#define VMODE_832_624_75	13	
#define VMODE_1024_768_60	14	
#define VMODE_1024_768_70	15	
#define VMODE_1024_768_75V	16	
#define VMODE_1024_768_75	17	
#define VMODE_1152_870_75	18	
#define VMODE_1280_960_75	19	
#define VMODE_1280_1024_75	20	
#define VMODE_1152_768_60	21	
#define VMODE_1600_1024_60	22	
#define VMODE_MAX		22
#define VMODE_CHOOSE		99

#define CMODE_NVRAM		-1
#define CMODE_CHOOSE		-2
#define CMODE_8			0	
#define CMODE_16		1	
#define CMODE_32		2	


extern int mac_vmode_to_var(int vmode, int cmode,
			    struct fb_var_screeninfo *var);
extern int mac_var_to_vmode(const struct fb_var_screeninfo *var, int *vmode,
			    int *cmode);
extern int mac_map_monitor_sense(int sense);
extern int mac_find_mode(struct fb_var_screeninfo *var,
			 struct fb_info *info,
			 const char *mode_option,
			 unsigned int default_bpp);



#define NV_VMODE		0x140f
#define NV_CMODE		0x1410

#endif 
