/*
 *  Copyright (C) 2002 Intersil Americas Inc.
 *  Copyright (C) 2003-2004 Luis R. Rodriguez <mcgrof@ruslug.rutgers.edu>_
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

#include <linux/module.h>
#include <linux/types.h>
#include <linux/delay.h>

#include <asm/uaccess.h>
#include <asm/io.h>

#include "prismcompat.h"
#include "isl_38xx.h"
#include "islpci_dev.h"
#include "islpci_mgt.h"


void
isl38xx_disable_interrupts(void __iomem *device)
{
	isl38xx_w32_flush(device, 0x00000000, ISL38XX_INT_EN_REG);
	udelay(ISL38XX_WRITEIO_DELAY);
}

void
isl38xx_handle_sleep_request(isl38xx_control_block *control_block,
			     int *powerstate, void __iomem *device_base)
{
	if (isl38xx_in_queue(control_block, ISL38XX_CB_TX_DATA_LQ))
		
		return;

	if (isl38xx_in_queue(control_block, ISL38XX_CB_TX_MGMTQ))
		
		return;

	
	if (isl38xx_in_queue(control_block, ISL38XX_CB_RX_DATA_LQ))
		
		return;

	if (isl38xx_in_queue(control_block, ISL38XX_CB_RX_MGMTQ))
		
		return;

#if VERBOSE > SHOW_ERROR_MESSAGES
	DEBUG(SHOW_TRACING, "Device going to sleep mode\n");
#endif

	
	*powerstate = ISL38XX_PSM_POWERSAVE_STATE;

	
	isl38xx_w32_flush(device_base, ISL38XX_DEV_INT_SLEEP,
			  ISL38XX_DEV_INT_REG);
	udelay(ISL38XX_WRITEIO_DELAY);
}

void
isl38xx_handle_wakeup(isl38xx_control_block *control_block,
		      int *powerstate, void __iomem *device_base)
{
	
	*powerstate = ISL38XX_PSM_ACTIVE_STATE;

	
	if (!isl38xx_in_queue(control_block, ISL38XX_CB_TX_DATA_LQ)
	    && !isl38xx_in_queue(control_block, ISL38XX_CB_TX_MGMTQ))
		return;

#if VERBOSE > SHOW_ERROR_MESSAGES
	DEBUG(SHOW_ANYTHING, "Wake up handler trigger the device\n");
#endif

	isl38xx_w32_flush(device_base, ISL38XX_DEV_INT_UPDATE,
			  ISL38XX_DEV_INT_REG);
	udelay(ISL38XX_WRITEIO_DELAY);
}

void
isl38xx_trigger_device(int asleep, void __iomem *device_base)
{
	u32 reg;

#if VERBOSE > SHOW_ERROR_MESSAGES
	u32 counter = 0;
	struct timeval current_time;
	DEBUG(SHOW_FUNCTION_CALLS, "isl38xx trigger device\n");
#endif

	
	if (asleep) {
		
#if VERBOSE > SHOW_ERROR_MESSAGES
		do_gettimeofday(&current_time);
		DEBUG(SHOW_TRACING, "%08li.%08li Device wakeup triggered\n",
		      current_time.tv_sec, (long)current_time.tv_usec);

		DEBUG(SHOW_TRACING, "%08li.%08li Device register read %08x\n",
		      current_time.tv_sec, (long)current_time.tv_usec,
		      readl(device_base + ISL38XX_CTRL_STAT_REG));
#endif

		reg = readl(device_base + ISL38XX_INT_IDENT_REG);
		if (reg == 0xabadface) {
#if VERBOSE > SHOW_ERROR_MESSAGES
			do_gettimeofday(&current_time);
			DEBUG(SHOW_TRACING,
			      "%08li.%08li Device register abadface\n",
			      current_time.tv_sec, (long)current_time.tv_usec);
#endif
			
			while (reg = readl(device_base + ISL38XX_CTRL_STAT_REG),
			       (reg & ISL38XX_CTRL_STAT_SLEEPMODE) == 0) {
				udelay(ISL38XX_WRITEIO_DELAY);
#if VERBOSE > SHOW_ERROR_MESSAGES
				counter++;
#endif
			}

#if VERBOSE > SHOW_ERROR_MESSAGES
			DEBUG(SHOW_TRACING,
			      "%08li.%08li Device register read %08x\n",
			      current_time.tv_sec, (long)current_time.tv_usec,
			      readl(device_base + ISL38XX_CTRL_STAT_REG));
			do_gettimeofday(&current_time);
			DEBUG(SHOW_TRACING,
			      "%08li.%08li Device asleep counter %i\n",
			      current_time.tv_sec, (long)current_time.tv_usec,
			      counter);
#endif
		}
		
		isl38xx_w32_flush(device_base, ISL38XX_DEV_INT_WAKEUP,
				  ISL38XX_DEV_INT_REG);

#if VERBOSE > SHOW_ERROR_MESSAGES
		udelay(ISL38XX_WRITEIO_DELAY);

		
		reg = readl(device_base + ISL38XX_CTRL_STAT_REG);
		do_gettimeofday(&current_time);
		DEBUG(SHOW_TRACING, "%08li.%08li Device register read %08x\n",
		      current_time.tv_sec, (long)current_time.tv_usec, reg);
#endif
	} else {
		
#if VERBOSE > SHOW_ERROR_MESSAGES
		DEBUG(SHOW_TRACING, "Device is in active state\n");
#endif
		

		isl38xx_w32_flush(device_base, ISL38XX_DEV_INT_UPDATE,
				  ISL38XX_DEV_INT_REG);
	}
}

void
isl38xx_interface_reset(void __iomem *device_base, dma_addr_t host_address)
{
#if VERBOSE > SHOW_ERROR_MESSAGES
	DEBUG(SHOW_FUNCTION_CALLS, "isl38xx_interface_reset\n");
#endif

	
	isl38xx_w32_flush(device_base, host_address, ISL38XX_CTRL_BLK_BASE_REG);
	udelay(ISL38XX_WRITEIO_DELAY);

	
	isl38xx_w32_flush(device_base, ISL38XX_DEV_INT_RESET, ISL38XX_DEV_INT_REG);
	udelay(ISL38XX_WRITEIO_DELAY);

	

	isl38xx_w32_flush(device_base, ISL38XX_INT_IDENT_INIT, ISL38XX_INT_EN_REG);
	udelay(ISL38XX_WRITEIO_DELAY);  
}

void
isl38xx_enable_common_interrupts(void __iomem *device_base)
{
	u32 reg;

	reg = ISL38XX_INT_IDENT_UPDATE | ISL38XX_INT_IDENT_SLEEP |
	      ISL38XX_INT_IDENT_WAKEUP;
	isl38xx_w32_flush(device_base, reg, ISL38XX_INT_EN_REG);
	udelay(ISL38XX_WRITEIO_DELAY);
}

int
isl38xx_in_queue(isl38xx_control_block *cb, int queue)
{
	const s32 delta = (le32_to_cpu(cb->driver_curr_frag[queue]) -
			   le32_to_cpu(cb->device_curr_frag[queue]));


	BUG_ON(delta < 0);	

	switch (queue) {
		
	case ISL38XX_CB_TX_MGMTQ:
		BUG_ON(delta > ISL38XX_CB_MGMT_QSIZE);

	case ISL38XX_CB_TX_DATA_LQ:
	case ISL38XX_CB_TX_DATA_HQ:
		BUG_ON(delta > ISL38XX_CB_TX_QSIZE);
		return delta;

		
	case ISL38XX_CB_RX_MGMTQ:
		BUG_ON(delta > ISL38XX_CB_MGMT_QSIZE);
		return ISL38XX_CB_MGMT_QSIZE - delta;

	case ISL38XX_CB_RX_DATA_LQ:
	case ISL38XX_CB_RX_DATA_HQ:
		BUG_ON(delta > ISL38XX_CB_RX_QSIZE);
		return ISL38XX_CB_RX_QSIZE - delta;
	}
	BUG();
	return 0;
}
