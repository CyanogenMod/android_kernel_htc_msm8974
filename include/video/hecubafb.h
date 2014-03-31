/*
 * hecubafb.h - definitions for the hecuba framebuffer driver
 *
 * Copyright (C) 2008 by Jaya Kumar
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of this archive for
 * more details.
 *
 */

#ifndef _LINUX_HECUBAFB_H_
#define _LINUX_HECUBAFB_H_

#define APOLLO_START_NEW_IMG	0xA0
#define APOLLO_STOP_IMG_DATA	0xA1
#define APOLLO_DISPLAY_IMG	0xA2
#define APOLLO_ERASE_DISPLAY	0xA3
#define APOLLO_INIT_DISPLAY	0xA4

#define HCB_WUP_BIT	0x01
#define HCB_DS_BIT 	0x02
#define HCB_RW_BIT 	0x04
#define HCB_CD_BIT 	0x08
#define HCB_ACK_BIT 	0x80

struct hecubafb_par {
	struct fb_info *info;
	struct hecuba_board *board;
	void (*send_command)(struct hecubafb_par *, unsigned char);
	void (*send_data)(struct hecubafb_par *, unsigned char);
};

struct hecuba_board {
	struct module *owner;
	void (*remove)(struct hecubafb_par *);
	void (*set_ctl)(struct hecubafb_par *, unsigned char, unsigned char);
	void (*set_data)(struct hecubafb_par *, unsigned char);
	void (*wait_for_ack)(struct hecubafb_par *, int);
	int (*init)(struct hecubafb_par *);
};


#endif
