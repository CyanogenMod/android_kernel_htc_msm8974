/* -*- linux-c -*- ------------------------------------------------------- *
 *
 *   Copyright (C) 1991, 1992 Linus Torvalds
 *   Copyright 2007 rPath, Inc. - All Rights Reserved
 *
 *   This file is part of the Linux kernel, and is made available under
 *   the terms of the GNU General Public License version 2.
 *
 * ----------------------------------------------------------------------- */


#ifndef BOOT_VIDEO_H
#define BOOT_VIDEO_H

#include <linux/types.h>

#define VIDEO_FIRST_MENU 0x0000

#define VIDEO_FIRST_BIOS 0x0100

#define VIDEO_FIRST_VESA 0x0200

#define VIDEO_FIRST_V7 0x0900

#define VIDEO_FIRST_SPECIAL 0x0f00
#define VIDEO_80x25 0x0f00
#define VIDEO_8POINT 0x0f01
#define VIDEO_80x43 0x0f02
#define VIDEO_80x28 0x0f03
#define VIDEO_CURRENT_MODE 0x0f04
#define VIDEO_80x30 0x0f05
#define VIDEO_80x34 0x0f06
#define VIDEO_80x60 0x0f07
#define VIDEO_GFX_HACK 0x0f08
#define VIDEO_LAST_SPECIAL 0x0f09

#define VIDEO_FIRST_RESOLUTION 0x1000

#define VIDEO_RECALC 0x8000

void store_screen(void);
#define DO_STORE() store_screen()


struct mode_info {
	u16 mode;		
	u16 x, y;		
	u16 depth;		
};

struct card_info {
	const char *card_name;
	int (*set_mode)(struct mode_info *mode);
	int (*probe)(void);
	struct mode_info *modes;
	int nmodes;		
	int unsafe;		
	u16 xmode_first;	
	u16 xmode_n;		
};

#define __videocard struct card_info __attribute__((section(".videocards")))
extern struct card_info video_cards[], video_cards_end[];

int mode_defined(u16 mode);	

#define ADAPTER_CGA	0	
#define ADAPTER_EGA	1
#define ADAPTER_VGA	2

extern int adapter;
extern u16 video_segment;
extern int force_x, force_y;	
extern int do_restore;		
extern int graphic_mode;	

static inline u8 in_idx(u16 port, u8 index)
{
	outb(index, port);
	return inb(port+1);
}

static inline void out_idx(u8 v, u16 port, u8 index)
{
	outw(index+(v << 8), port);
}

static inline u8 tst_idx(u8 v, u16 port, u8 index)
{
	out_idx(port, index, v);
	return in_idx(port, index);
}

u16 vga_crtc(void);		

#endif 
