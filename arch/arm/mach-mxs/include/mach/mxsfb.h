/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 */

#ifndef __MACH_FB_H
#define __MACH_FB_H

#include <linux/fb.h>

#define STMLCDIF_8BIT 1	
#define STMLCDIF_16BIT 0 
#define STMLCDIF_18BIT 2 
#define STMLCDIF_24BIT 3 

#define FB_SYNC_DATA_ENABLE_HIGH_ACT	(1 << 6)
#define FB_SYNC_DOTCLK_FAILING_ACT	(1 << 7) 

struct mxsfb_platform_data {
	struct fb_videomode *mode_list;
	unsigned mode_count;

	unsigned default_bpp;

	unsigned dotclk_delay;	
	unsigned ld_intf_width;	

	unsigned fb_size;	
	unsigned long fb_phys;	
};

#endif 
