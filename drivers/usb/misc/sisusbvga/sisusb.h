/*
 * sisusb - usb kernel driver for Net2280/SiS315 based USB2VGA dongles
 *
 * Copyright (C) 2005 by Thomas Winischhofer, Vienna, Austria
 *
 * If distributed as part of the Linux kernel, this code is licensed under the
 * terms of the GPL v2.
 *
 * Otherwise, the following license terms apply:
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1) Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2) Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3) The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESSED OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Author:	Thomas Winischhofer <thomas@winischhofer.net>
 *
 */

#ifndef _SISUSB_H_
#define _SISUSB_H_

#ifdef CONFIG_COMPAT
#define SISUSB_NEW_CONFIG_COMPAT
#endif

#include <linux/mutex.h>



#define SISUSB_VERSION		0
#define SISUSB_REVISION		0
#define SISUSB_PATCHLEVEL	8


#ifdef CONFIG_USB_SISUSBVGA_CON
#define INCL_SISUSB_CON		1
#endif

#include <linux/console.h>
#include <linux/vt_kern.h>
#include "sisusb_struct.h"


#define SISUSB_MINOR		133	

#define SISUSB_IBUF_SIZE  0x01000
#define SISUSB_OBUF_SIZE  0x10000	

#define NUMOBUFS 8		


#ifdef __BIG_ENDIAN
#define SISUSB_CORRECT_ENDIANNESS_PACKET(p)		\
	do {						\
		p->header  = cpu_to_le16(p->header);	\
		p->address = cpu_to_le32(p->address);	\
		p->data    = cpu_to_le32(p->data);	\
	} while(0)
#else
#define SISUSB_CORRECT_ENDIANNESS_PACKET(p)
#endif

struct sisusb_usb_data;

struct sisusb_urb_context {	
	struct sisusb_usb_data *sisusb;
	int urbindex;
	int *actual_length;
};

struct sisusb_usb_data {
	struct usb_device *sisusb_dev;
	struct usb_interface *interface;
	struct kref kref;
	wait_queue_head_t wait_q;	
	struct mutex lock;	
	unsigned int ifnum;	
	int minor;		
	int isopen;		
	int present;		
	int ready;		
	int numobufs;		
	char *obuf[NUMOBUFS], *ibuf;	
	int obufsize, ibufsize;
	struct urb *sisurbout[NUMOBUFS];
	struct urb *sisurbin;
	unsigned char urbstatus[NUMOBUFS];
	unsigned char completein;
	struct sisusb_urb_context urbout_context[NUMOBUFS];
	unsigned long flagb0;
	unsigned long vrambase;	
	unsigned int vramsize;	
	unsigned long mmiobase;
	unsigned int mmiosize;
	unsigned long ioportbase;
	unsigned char devinit;	
	unsigned char gfxinit;	
	unsigned short chipid, chipvendor;
	unsigned short chiprevision;
#ifdef INCL_SISUSB_CON
	struct SiS_Private *SiS_Pr;
	unsigned long scrbuf;
	unsigned int scrbuf_size;
	int haveconsole, con_first, con_last;
	int havethisconsole[MAX_NR_CONSOLES];
	int textmodedestroyed;
	unsigned int sisusb_num_columns;	
	int cur_start_addr, con_rolled_over;
	int sisusb_cursor_loc, bad_cursor_pos;
	int sisusb_cursor_size_from;
	int sisusb_cursor_size_to;
	int current_font_height, current_font_512;
	int font_backup_size, font_backup_height, font_backup_512;
	char *font_backup;
	int font_slot;
	struct vc_data *sisusb_display_fg;
	int is_gfx;
	int con_blanked;
#endif
};

#define to_sisusb_dev(d) container_of(d, struct sisusb_usb_data, kref)


#define SU_URB_BUSY   1
#define SU_URB_ALLOC  2


#define SISUSB_EP_GFX_IN	0x0e	
#define SISUSB_EP_GFX_OUT	0x0e

#define SISUSB_EP_GFX_BULK_OUT	0x01	
#define SISUSB_EP_GFX_BULK_IN	0x02	

#define SISUSB_EP_GFX_LBULK_OUT	0x03	

#define SISUSB_EP_UNKNOWN_04	0x04	

#define SISUSB_EP_BRIDGE_IN	0x0d	
#define SISUSB_EP_BRIDGE_OUT	0x0d

#define SISUSB_TYPE_MEM		0
#define SISUSB_TYPE_IO		1

struct sisusb_packet {
	unsigned short header;
	u32 address;
	u32 data;
} __attribute__ ((__packed__));

#define CLEARPACKET(packet) memset(packet, 0, 10)


#define SISUSB_PCI_MEMBASE	0xd0000000
#define SISUSB_PCI_MMIOBASE	0xe4000000
#define SISUSB_PCI_IOPORTBASE	0x0000d000

#define SISUSB_PCI_PSEUDO_MEMBASE	0x10000000
#define SISUSB_PCI_PSEUDO_MMIOBASE	0x20000000
#define SISUSB_PCI_PSEUDO_IOPORTBASE	0x0000d000
#define SISUSB_PCI_PSEUDO_PCIBASE	0x00010000

#define SISUSB_PCI_MMIOSIZE	(128*1024)
#define SISUSB_PCI_PCONFSIZE	0x5c


#define AROFFSET	0x40
#define ARROFFSET	0x41
#define GROFFSET	0x4e
#define SROFFSET	0x44
#define CROFFSET	0x54
#define MISCROFFSET	0x4c
#define MISCWOFFSET	0x42
#define INPUTSTATOFFSET 0x5A
#define PART1OFFSET	0x04
#define PART2OFFSET	0x10
#define PART3OFFSET	0x12
#define PART4OFFSET	0x14
#define PART5OFFSET	0x16
#define CAPTUREOFFSET	0x00
#define VIDEOOFFSET	0x02
#define COLREGOFFSET	0x48
#define PELMASKOFFSET	0x46
#define VGAENABLE	0x43

#define SISAR		SISUSB_PCI_IOPORTBASE + AROFFSET
#define SISARR		SISUSB_PCI_IOPORTBASE + ARROFFSET
#define SISGR		SISUSB_PCI_IOPORTBASE + GROFFSET
#define SISSR		SISUSB_PCI_IOPORTBASE + SROFFSET
#define SISCR		SISUSB_PCI_IOPORTBASE + CROFFSET
#define SISMISCR	SISUSB_PCI_IOPORTBASE + MISCROFFSET
#define SISMISCW	SISUSB_PCI_IOPORTBASE + MISCWOFFSET
#define SISINPSTAT	SISUSB_PCI_IOPORTBASE + INPUTSTATOFFSET
#define SISPART1	SISUSB_PCI_IOPORTBASE + PART1OFFSET
#define SISPART2	SISUSB_PCI_IOPORTBASE + PART2OFFSET
#define SISPART3	SISUSB_PCI_IOPORTBASE + PART3OFFSET
#define SISPART4	SISUSB_PCI_IOPORTBASE + PART4OFFSET
#define SISPART5	SISUSB_PCI_IOPORTBASE + PART5OFFSET
#define SISCAP		SISUSB_PCI_IOPORTBASE + CAPTUREOFFSET
#define SISVID		SISUSB_PCI_IOPORTBASE + VIDEOOFFSET
#define SISCOLIDXR	SISUSB_PCI_IOPORTBASE + COLREGOFFSET - 1
#define SISCOLIDX	SISUSB_PCI_IOPORTBASE + COLREGOFFSET
#define SISCOLDATA	SISUSB_PCI_IOPORTBASE + COLREGOFFSET + 1
#define SISCOL2IDX	SISPART5
#define SISCOL2DATA	SISPART5 + 1
#define SISPEL		SISUSB_PCI_IOPORTBASE + PELMASKOFFSET
#define SISVGAEN	SISUSB_PCI_IOPORTBASE + VGAENABLE
#define SISDACA		SISCOLIDX
#define SISDACD		SISCOLDATA


struct sisusb_info {
	__u32 sisusb_id;	
#define SISUSB_ID  0x53495355	
	__u8 sisusb_version;
	__u8 sisusb_revision;
	__u8 sisusb_patchlevel;
	__u8 sisusb_gfxinit;	

	__u32 sisusb_vrambase;
	__u32 sisusb_mmiobase;
	__u32 sisusb_iobase;
	__u32 sisusb_pcibase;

	__u32 sisusb_vramsize;	

	__u32 sisusb_minor;

	__u32 sisusb_fbdevactive;	

	__u32 sisusb_conactive;	

	__u8 sisusb_reserved[28];	
};

struct sisusb_command {
	__u8 operation;		
	__u8 data0;		
	__u8 data1;		
	__u8 data2;		
	__u32 data3;		
	__u32 data4;		
};

#define SUCMD_GET	0x01	
#define SUCMD_SET	0x02	
#define SUCMD_SETOR	0x03	
#define SUCMD_SETAND	0x04	
#define SUCMD_SETANDOR	0x05	
#define SUCMD_SETMASK	0x06	

#define SUCMD_CLRSCR	0x07	

#define SUCMD_HANDLETEXTMODE 0x08	

#define SUCMD_SETMODE	0x09	
#define SUCMD_SETVESAMODE 0x0a	

#define SISUSB_COMMAND		_IOWR(0xF3,0x3D,struct sisusb_command)
#define SISUSB_GET_CONFIG_SIZE	_IOR(0xF3,0x3E,__u32)
#define SISUSB_GET_CONFIG	_IOR(0xF3,0x3F,struct sisusb_info)

#endif 
