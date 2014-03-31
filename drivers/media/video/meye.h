/*
 * Motion Eye video4linux driver for Sony Vaio PictureBook
 *
 * Copyright (C) 2001-2004 Stelian Pop <stelian@popies.net>
 *
 * Copyright (C) 2001-2002 Alcôve <www.alcove.com>
 *
 * Copyright (C) 2000 Andrew Tridgell <tridge@valinux.com>
 *
 * Earlier work by Werner Almesberger, Paul `Rusty' Russell and Paul Mackerras.
 *
 * Some parts borrowed from various video4linux drivers, especially
 * bttv-driver.c and zoran.c, see original files for credits.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef _MEYE_PRIV_H_
#define _MEYE_PRIV_H_

#define MEYE_DRIVER_MAJORVERSION	 1
#define MEYE_DRIVER_MINORVERSION	14

#define MEYE_DRIVER_VERSION __stringify(MEYE_DRIVER_MAJORVERSION) "." \
			    __stringify(MEYE_DRIVER_MINORVERSION)

#include <linux/types.h>
#include <linux/pci.h>
#include <linux/kfifo.h>


#define MCHIP_PCI_POWER_CSR		0x54
#define MCHIP_PCI_MCORE_STATUS		0x60		
#define MCHIP_PCI_HOSTUSEREQ_SET	0x64
#define MCHIP_PCI_HOSTUSEREQ_CLR	0x68
#define MCHIP_PCI_LOWPOWER_SET		0x6c
#define MCHIP_PCI_LOWPOWER_CLR		0x70
#define MCHIP_PCI_SOFTRESET_SET		0x74

#define MCHIP_MM_REGS			0x200		
#define MCHIP_REG_TIMEOUT		1000		
#define MCHIP_MCC_VRJ_TIMEOUT		1000		

#define MCHIP_MM_PCI_MODE		0x00		
#define MCHIP_MM_PCI_MODE_RETRY		0x00000001	
#define MCHIP_MM_PCI_MODE_MASTER	0x00000002	
#define MCHIP_MM_PCI_MODE_READ_LINE	0x00000004	

#define MCHIP_MM_INTA			0x04		
#define MCHIP_MM_INTA_MCC		0x00000001	
#define MCHIP_MM_INTA_VRJ		0x00000002	
#define MCHIP_MM_INTA_HIC_1		0x00000004	
#define MCHIP_MM_INTA_HIC_1_MASK	0x00000400	
#define MCHIP_MM_INTA_HIC_END		0x00000008	
#define MCHIP_MM_INTA_HIC_END_MASK	0x00000800
#define MCHIP_MM_INTA_JPEG		0x00000010	
#define MCHIP_MM_INTA_JPEG_MASK		0x00001000
#define MCHIP_MM_INTA_CAPTURE		0x00000020	
#define MCHIP_MM_INTA_PCI_ERR		0x00000040	
#define MCHIP_MM_INTA_PCI_ERR_MASK	0x00004000

#define MCHIP_MM_PT_ADDR		0x08		
							
#define MCHIP_NB_PAGES			1024		
#define MCHIP_NB_PAGES_MJPEG		256		

#define MCHIP_MM_FIR(n)			(0x0c+(n)*4)	
#define MCHIP_MM_FIR_RDY		0x00000001	
#define MCHIP_MM_FIR_FAILFR_MASK	0xf8000000	
#define MCHIP_MM_FIR_FAILFR_SHIFT	27

	
#define MCHIP_MM_FIR_C_ENDL_MASK	0x000007fe	
#define MCHIP_MM_FIR_C_ENDL_SHIFT	1
#define MCHIP_MM_FIR_C_ENDP_MASK	0x0007f800	
#define MCHIP_MM_FIR_C_ENDP_SHIFT	11
#define MCHIP_MM_FIR_C_STARTP_MASK	0x07f80000	
#define MCHIP_MM_FIR_C_STARTP_SHIFT	19

	
#define MCHIP_MM_FIR_O_STARTP_MASK	0x7ffe0000	
#define MCHIP_MM_FIR_O_STARTP_SHIFT	17

#define MCHIP_MM_FIFO_DATA		0x1c		
#define MCHIP_MM_FIFO_STATUS		0x20		
#define MCHIP_MM_FIFO_MASK		0x00000003
#define MCHIP_MM_FIFO_WAIT_OR_READY	0x00000002      
#define MCHIP_MM_FIFO_IDLE		0x0		
#define MCHIP_MM_FIFO_IDLE1		0x1		
#define	MCHIP_MM_FIFO_WAIT		0x2		
#define MCHIP_MM_FIFO_READY		0x3		

#define MCHIP_HIC_HOST_USEREQ		0x40		

#define MCHIP_HIC_TP_BUSY		0x44		

#define MCHIP_HIC_PIC_SAVED		0x48		

#define MCHIP_HIC_LOWPOWER		0x4c		

#define MCHIP_HIC_CTL			0x50		
#define MCHIP_HIC_CTL_SOFT_RESET	0x00000001	
#define MCHIP_HIC_CTL_MCORE_RDY		0x00000002	

#define MCHIP_HIC_CMD			0x54		
#define MCHIP_HIC_CMD_BITS		0x00000003      
#define MCHIP_HIC_CMD_NOOP		0x0
#define MCHIP_HIC_CMD_START		0x1
#define MCHIP_HIC_CMD_STOP		0x2

#define MCHIP_HIC_MODE			0x58
#define MCHIP_HIC_MODE_NOOP		0x0
#define MCHIP_HIC_MODE_STILL_CAP	0x1		
#define MCHIP_HIC_MODE_DISPLAY		0x2		
#define MCHIP_HIC_MODE_STILL_COMP	0x3		
#define MCHIP_HIC_MODE_STILL_DECOMP	0x4		
#define MCHIP_HIC_MODE_CONT_COMP	0x5		
#define MCHIP_HIC_MODE_CONT_DECOMP	0x6		
#define MCHIP_HIC_MODE_STILL_OUT	0x7		
#define MCHIP_HIC_MODE_CONT_OUT		0x8		

#define MCHIP_HIC_STATUS		0x5c
#define MCHIP_HIC_STATUS_MCC_RDY	0x00000001	
#define MCHIP_HIC_STATUS_VRJ_RDY	0x00000002	
#define MCHIP_HIC_STATUS_IDLE           0x00000003
#define MCHIP_HIC_STATUS_CAPDIS		0x00000004	
#define MCHIP_HIC_STATUS_COMPDEC	0x00000008	
#define MCHIP_HIC_STATUS_BUSY		0x00000010	

#define MCHIP_HIC_S_RATE		0x60		

#define MCHIP_HIC_PCI_VFMT		0x64		
#define MCHIP_HIC_PCI_VFMT_YVYU		0x00000001	
							

#define MCHIP_MCC_CMD			0x80		
#define MCHIP_MCC_CMD_INITIAL		0x0		
#define MCHIP_MCC_CMD_IIC_START_SET	0x1
#define MCHIP_MCC_CMD_IIC_END_SET	0x2
#define MCHIP_MCC_CMD_FM_WRITE		0x3		
#define MCHIP_MCC_CMD_FM_READ		0x4
#define MCHIP_MCC_CMD_FM_STOP		0x5
#define MCHIP_MCC_CMD_CAPTURE		0x6
#define MCHIP_MCC_CMD_DISPLAY		0x7
#define MCHIP_MCC_CMD_END_DISP		0x8
#define MCHIP_MCC_CMD_STILL_COMP	0x9
#define MCHIP_MCC_CMD_STILL_DECOMP	0xa
#define MCHIP_MCC_CMD_STILL_OUTPUT	0xb
#define MCHIP_MCC_CMD_CONT_OUTPUT	0xc
#define MCHIP_MCC_CMD_CONT_COMP		0xd
#define MCHIP_MCC_CMD_CONT_DECOMP	0xe
#define MCHIP_MCC_CMD_RESET		0xf		

#define MCHIP_MCC_IIC_WR		0x84

#define MCHIP_MCC_MCC_WR		0x88

#define MCHIP_MCC_MCC_RD		0x8c

#define MCHIP_MCC_STATUS		0x90
#define MCHIP_MCC_STATUS_CAPT		0x00000001	
#define MCHIP_MCC_STATUS_DISP		0x00000002	
#define MCHIP_MCC_STATUS_COMP		0x00000004	
#define MCHIP_MCC_STATUS_DECOMP		0x00000008	
#define MCHIP_MCC_STATUS_MCC_WR		0x00000010	
#define MCHIP_MCC_STATUS_MCC_RD		0x00000020	
#define MCHIP_MCC_STATUS_IIC_WR		0x00000040	
#define MCHIP_MCC_STATUS_OUTPUT		0x00000080	

#define MCHIP_MCC_SIG_POLARITY		0x94
#define MCHIP_MCC_SIG_POL_VS_H		0x00000001	
#define MCHIP_MCC_SIG_POL_HS_H		0x00000002	
#define MCHIP_MCC_SIG_POL_DOE_H		0x00000004	

#define MCHIP_MCC_IRQ			0x98
#define MCHIP_MCC_IRQ_CAPDIS_STRT	0x00000001	
#define MCHIP_MCC_IRQ_CAPDIS_STRT_MASK	0x00000010
#define MCHIP_MCC_IRQ_CAPDIS_END	0x00000002	
#define MCHIP_MCC_IRQ_CAPDIS_END_MASK	0x00000020
#define MCHIP_MCC_IRQ_COMPDEC_STRT	0x00000004	
#define MCHIP_MCC_IRQ_COMPDEC_STRT_MASK	0x00000040
#define MCHIP_MCC_IRQ_COMPDEC_END	0x00000008	
#define MCHIP_MCC_IRQ_COMPDEC_END_MASK	0x00000080

#define MCHIP_MCC_HSTART		0x9c		
#define MCHIP_MCC_VSTART		0xa0
#define MCHIP_MCC_HCOUNT		0xa4
#define MCHIP_MCC_VCOUNT		0xa8
#define MCHIP_MCC_R_XBASE		0xac		
#define MCHIP_MCC_R_YBASE		0xb0
#define MCHIP_MCC_R_XRANGE		0xb4
#define MCHIP_MCC_R_YRANGE		0xb8
#define MCHIP_MCC_B_XBASE		0xbc		
#define MCHIP_MCC_B_YBASE		0xc0
#define MCHIP_MCC_B_XRANGE		0xc4
#define MCHIP_MCC_B_YRANGE		0xc8

#define MCHIP_MCC_R_SAMPLING		0xcc		

#define MCHIP_VRJ_CMD			0x100		

#define MCHIP_VRJ_COMPRESSED_DATA	0x1b0
#define MCHIP_VRJ_PIXEL_DATA		0x1b8

#define MCHIP_VRJ_BUS_MODE		0x100
#define MCHIP_VRJ_SIGNAL_ACTIVE_LEVEL	0x108
#define MCHIP_VRJ_PDAT_USE		0x110
#define MCHIP_VRJ_MODE_SPECIFY		0x118
#define MCHIP_VRJ_LIMIT_COMPRESSED_LO	0x120
#define MCHIP_VRJ_LIMIT_COMPRESSED_HI	0x124
#define MCHIP_VRJ_COMP_DATA_FORMAT	0x128
#define MCHIP_VRJ_TABLE_DATA		0x140
#define MCHIP_VRJ_RESTART_INTERVAL	0x148
#define MCHIP_VRJ_NUM_LINES		0x150
#define MCHIP_VRJ_NUM_PIXELS		0x158
#define MCHIP_VRJ_NUM_COMPONENTS	0x160
#define MCHIP_VRJ_SOF1			0x168
#define MCHIP_VRJ_SOF2			0x170
#define MCHIP_VRJ_SOF3			0x178
#define MCHIP_VRJ_SOF4			0x180
#define MCHIP_VRJ_SOS			0x188
#define MCHIP_VRJ_SOFT_RESET		0x190

#define MCHIP_VRJ_STATUS		0x1c0
#define MCHIP_VRJ_STATUS_BUSY		0x00001
#define MCHIP_VRJ_STATUS_COMP_ACCESS	0x00002
#define MCHIP_VRJ_STATUS_PIXEL_ACCESS	0x00004
#define MCHIP_VRJ_STATUS_ERROR		0x00008

#define MCHIP_VRJ_IRQ_FLAG		0x1c8
#define MCHIP_VRJ_ERROR_REPORT		0x1d8

#define MCHIP_VRJ_START_COMMAND		0x1a0


#include <linux/sony-laptop.h>

#include <linux/meye.h>
#include <linux/mutex.h>


#define MEYE_JPEG_CORRECTION	1

#define MEYE_MAX_BUFSIZE	614400	

#define MEYE_MAX_BUFNBRS	32

#define MEYE_BUF_UNUSED	0	
#define MEYE_BUF_USING	1	
#define MEYE_BUF_DONE	2	

struct meye_grab_buffer {
	int state;			
	unsigned long size;		
	struct timeval timestamp;	
	unsigned long sequence;		
};

#define MEYE_QUEUE_SIZE	MEYE_MAX_BUFNBRS

struct meye {
	struct v4l2_device v4l2_dev;	
	struct pci_dev *mchip_dev;	
	u8 mchip_irq;			
	u8 mchip_mode;			
	u8 mchip_fnum;			
	unsigned char __iomem *mchip_mmregs;
	u8 *mchip_ptable[MCHIP_NB_PAGES];
	void *mchip_ptable_toc;		
	dma_addr_t mchip_dmahandle;	
	unsigned char *grab_fbuffer;	
	unsigned char *grab_temp;	
					
	struct meye_grab_buffer grab_buffer[MEYE_MAX_BUFNBRS];
	int vma_use_count[MEYE_MAX_BUFNBRS]; 
	struct mutex lock;		
	struct kfifo grabq;		
	spinlock_t grabq_lock;		
	struct kfifo doneq;		
	spinlock_t doneq_lock;		
	wait_queue_head_t proc_list;	
	struct video_device *vdev;	
	u16 brightness;
	u16 hue;
	u16 contrast;
	u16 colour;
	struct meye_params params;	
	unsigned long in_use;		
#ifdef CONFIG_PM
	u8 pm_mchip_mode;		
#endif
};

#endif
