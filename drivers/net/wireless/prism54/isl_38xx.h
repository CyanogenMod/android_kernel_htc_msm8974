/*
 *  Copyright (C) 2002 Intersil Americas Inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifndef _ISL_38XX_H
#define _ISL_38XX_H

#include <asm/io.h>
#include <asm/byteorder.h>

#define ISL38XX_CB_RX_QSIZE                     8
#define ISL38XX_CB_TX_QSIZE                     32

#define ISL38XX_MAX_WDS_LINKS                   8

#define ISL38XX_PSM_ACTIVE_STATE                0
#define ISL38XX_PSM_POWERSAVE_STATE             1

#define ISL38XX_PCI_MEM_SIZE                    0x02000
#define ISL38XX_MEMORY_WINDOW_SIZE              0x01000
#define ISL38XX_DEV_FIRMWARE_ADDRES             0x20000
#define ISL38XX_WRITEIO_DELAY                   10	
#define ISL38XX_RESET_DELAY                     50	
#define ISL38XX_WAIT_CYCLE                      10	
#define ISL38XX_MAX_WAIT_CYCLES                 10

#define ISL38XX_HARDWARE_REG                    0x0000
#define ISL38XX_CARDBUS_CIS                     0x0800
#define ISL38XX_DIRECT_MEM_WIN                  0x1000

#define ISL38XX_DEV_INT_REG                     0x0000
#define ISL38XX_INT_IDENT_REG                   0x0010
#define ISL38XX_INT_ACK_REG                     0x0014
#define ISL38XX_INT_EN_REG                      0x0018
#define ISL38XX_GEN_PURP_COM_REG_1              0x0020
#define ISL38XX_GEN_PURP_COM_REG_2              0x0024
#define ISL38XX_CTRL_BLK_BASE_REG               ISL38XX_GEN_PURP_COM_REG_1
#define ISL38XX_DIR_MEM_BASE_REG                0x0030
#define ISL38XX_CTRL_STAT_REG                   0x0078

#define ISL38XX_PCI_POSTING_FLUSH		ISL38XX_INT_EN_REG

static inline void
isl38xx_w32_flush(void __iomem *base, u32 val, unsigned long offset)
{
	writel(val, base + offset);
	(void) readl(base + ISL38XX_PCI_POSTING_FLUSH);
}

#define ISL38XX_DEV_INT_RESET                   0x0001
#define ISL38XX_DEV_INT_UPDATE                  0x0002
#define ISL38XX_DEV_INT_WAKEUP                  0x0008
#define ISL38XX_DEV_INT_SLEEP                   0x0010

#define ISL38XX_INT_IDENT_UPDATE                0x0002
#define ISL38XX_INT_IDENT_INIT                  0x0004
#define ISL38XX_INT_IDENT_WAKEUP                0x0008
#define ISL38XX_INT_IDENT_SLEEP                 0x0010
#define ISL38XX_INT_SOURCES                     0x001E

#define ISL38XX_CTRL_STAT_SLEEPMODE             0x00000200
#define	ISL38XX_CTRL_STAT_CLKRUN		0x00800000
#define ISL38XX_CTRL_STAT_RESET                 0x10000000
#define ISL38XX_CTRL_STAT_RAMBOOT               0x20000000
#define ISL38XX_CTRL_STAT_STARTHALTED           0x40000000
#define ISL38XX_CTRL_STAT_HOST_OVERRIDE         0x80000000

#define ISL38XX_CB_RX_DATA_LQ                   0
#define ISL38XX_CB_TX_DATA_LQ                   1
#define ISL38XX_CB_RX_DATA_HQ                   2
#define ISL38XX_CB_TX_DATA_HQ                   3
#define ISL38XX_CB_RX_MGMTQ                     4
#define ISL38XX_CB_TX_MGMTQ                     5
#define ISL38XX_CB_QCOUNT                       6
#define ISL38XX_CB_MGMT_QSIZE                   4
#define ISL38XX_MIN_QTHRESHOLD                  4	

#define MGMT_FRAME_SIZE                         1500	
#define MGMT_TX_FRAME_COUNT                     24	
#define MGMT_RX_FRAME_COUNT                     24	
#define MGMT_FRAME_COUNT                        (MGMT_TX_FRAME_COUNT + MGMT_RX_FRAME_COUNT)
#define CONTROL_BLOCK_SIZE                      1024	
#define PSM_FRAME_SIZE                          1536
#define PSM_MINIMAL_STATION_COUNT               64
#define PSM_FRAME_COUNT                         PSM_MINIMAL_STATION_COUNT
#define PSM_BUFFER_SIZE                         PSM_FRAME_SIZE * PSM_FRAME_COUNT
#define MAX_TRAP_RX_QUEUE                       4
#define HOST_MEM_BLOCK                          CONTROL_BLOCK_SIZE + PSM_BUFFER_SIZE

#define FRAGMENT_FLAG_MF                        0x0001
#define MAX_FRAGMENT_SIZE                       1536

#define MAX_FRAGMENT_SIZE_RX	                1600

typedef struct {
	__le32 address;		
	__le16 size;		
	__le16 flags;		
} isl38xx_fragment;

struct isl38xx_cb {
	__le32 driver_curr_frag[ISL38XX_CB_QCOUNT];
	__le32 device_curr_frag[ISL38XX_CB_QCOUNT];
	isl38xx_fragment rx_data_low[ISL38XX_CB_RX_QSIZE];
	isl38xx_fragment tx_data_low[ISL38XX_CB_TX_QSIZE];
	isl38xx_fragment rx_data_high[ISL38XX_CB_RX_QSIZE];
	isl38xx_fragment tx_data_high[ISL38XX_CB_TX_QSIZE];
	isl38xx_fragment rx_data_mgmt[ISL38XX_CB_MGMT_QSIZE];
	isl38xx_fragment tx_data_mgmt[ISL38XX_CB_MGMT_QSIZE];
};

typedef struct isl38xx_cb isl38xx_control_block;

int isl38xx_in_queue(isl38xx_control_block *cb, int queue);

void isl38xx_disable_interrupts(void __iomem *);
void isl38xx_enable_common_interrupts(void __iomem *);

void isl38xx_handle_sleep_request(isl38xx_control_block *, int *,
				  void __iomem *);
void isl38xx_handle_wakeup(isl38xx_control_block *, int *, void __iomem *);
void isl38xx_trigger_device(int, void __iomem *);
void isl38xx_interface_reset(void __iomem *, dma_addr_t);

#endif				
