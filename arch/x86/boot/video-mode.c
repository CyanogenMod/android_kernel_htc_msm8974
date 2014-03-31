/* -*- linux-c -*- ------------------------------------------------------- *
 *
 *   Copyright (C) 1991, 1992 Linus Torvalds
 *   Copyright 2007-2008 rPath, Inc. - All Rights Reserved
 *
 *   This file is part of the Linux kernel, and is made available under
 *   the terms of the GNU General Public License version 2.
 *
 * ----------------------------------------------------------------------- */


#include "boot.h"
#include "video.h"
#include "vesa.h"

int adapter;			
u16 video_segment;
int force_x, force_y;	

int do_restore;		
int graphic_mode;	

void probe_cards(int unsafe)
{
	struct card_info *card;
	static u8 probed[2];

	if (probed[unsafe])
		return;

	probed[unsafe] = 1;

	for (card = video_cards; card < video_cards_end; card++) {
		if (card->unsafe == unsafe) {
			if (card->probe)
				card->nmodes = card->probe();
			else
				card->nmodes = 0;
		}
	}
}

int mode_defined(u16 mode)
{
	struct card_info *card;
	struct mode_info *mi;
	int i;

	for (card = video_cards; card < video_cards_end; card++) {
		mi = card->modes;
		for (i = 0; i < card->nmodes; i++, mi++) {
			if (mi->mode == mode)
				return 1;
		}
	}

	return 0;
}

static int raw_set_mode(u16 mode, u16 *real_mode)
{
	int nmode, i;
	struct card_info *card;
	struct mode_info *mi;

	
	mode &= ~VIDEO_RECALC;

	
	nmode = 0;
	for (card = video_cards; card < video_cards_end; card++) {
		mi = card->modes;
		for (i = 0; i < card->nmodes; i++, mi++) {
			int visible = mi->x || mi->y;

			if ((mode == nmode && visible) ||
			    mode == mi->mode ||
			    mode == (mi->y << 8)+mi->x) {
				*real_mode = mi->mode;
				return card->set_mode(mi);
			}

			if (visible)
				nmode++;
		}
	}

	
	for (card = video_cards; card < video_cards_end; card++) {
		if (mode >= card->xmode_first &&
		    mode < card->xmode_first+card->xmode_n) {
			struct mode_info mix;
			*real_mode = mix.mode = mode;
			mix.x = mix.y = 0;
			return card->set_mode(&mix);
		}
	}

	
	return -1;
}

static void vga_recalc_vertical(void)
{
	unsigned int font_size, rows;
	u16 crtc;
	u8 pt, ov;

	set_fs(0);
	font_size = rdfs8(0x485); 
	rows = force_y ? force_y : rdfs8(0x484)+1; 

	rows *= font_size;	
	rows--;			

	crtc = vga_crtc();

	pt = in_idx(crtc, 0x11);
	pt &= ~0x80;		
	out_idx(pt, crtc, 0x11);

	out_idx((u8)rows, crtc, 0x12); 

	ov = in_idx(crtc, 0x07); 
	ov &= 0xbd;
	ov |= (rows >> (8-1)) & 0x02;
	ov |= (rows >> (9-6)) & 0x40;
	out_idx(ov, crtc, 0x07);
}

int set_mode(u16 mode)
{
	int rv;
	u16 real_mode;

	
	if (mode == VIDEO_CURRENT_MODE)
		return 0;	
	else if (mode == NORMAL_VGA)
		mode = VIDEO_80x25;
	else if (mode == EXTENDED_VGA)
		mode = VIDEO_8POINT;

	rv = raw_set_mode(mode, &real_mode);
	if (rv)
		return rv;

	if (mode & VIDEO_RECALC)
		vga_recalc_vertical();

#ifndef _WAKEUP
	boot_params.hdr.vid_mode = real_mode;
#endif
	return 0;
}
