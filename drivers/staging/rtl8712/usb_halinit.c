/******************************************************************************
 * usb_halinit.c
 *
 * Copyright(c) 2007 - 2010 Realtek Corporation. All rights reserved.
 * Linux device driver for RTL8192SU
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110, USA
 *
 * Modifications for inclusion into the Linux staging tree are
 * Copyright(c) 2010 Larry Finger. All rights reserved.
 *
 * Contact information:
 * WLAN FAE <wlanfae@realtek.com>
 * Larry Finger <Larry.Finger@lwfinger.net>
 *
 ******************************************************************************/

#define _HCI_HAL_INIT_C_

#include "osdep_service.h"
#include "drv_types.h"
#include "usb_ops.h"
#include "usb_osintf.h"

u8 r8712_usb_hal_bus_init(struct _adapter *padapter)
{
	u8 val8 = 0;
	u8 ret = _SUCCESS;
	int PollingCnt = 20;
	struct registry_priv *pregistrypriv = &padapter->registrypriv;

	if (pregistrypriv->chip_version == RTL8712_FPGA) {
		val8 = 0x01;
		
		r8712_write8(padapter, SYS_CLKR, val8);
		val8 = r8712_read8(padapter, SPS1_CTRL);
		val8 = val8 | 0x01;
		
		r8712_write8(padapter, SPS1_CTRL, val8);
		val8 = r8712_read8(padapter, AFE_MISC);
		val8 = val8 | 0x01;
		
		r8712_write8(padapter, AFE_MISC, val8);
		val8 = r8712_read8(padapter, LDOA15_CTRL);
		val8 = val8 | 0x01;
		
		r8712_write8(padapter, LDOA15_CTRL, val8);
		val8 = r8712_read8(padapter, SPS1_CTRL);
		val8 = val8 | 0x02;
		
		r8712_write8(padapter, SPS1_CTRL, val8);
		val8 = r8712_read8(padapter, AFE_MISC);
		val8 = val8 | 0x02;
		
		r8712_write8(padapter, AFE_MISC, val8);
		val8 = r8712_read8(padapter, SYS_ISO_CTRL + 1);
		val8 = val8 | 0x08;
		
		r8712_write8(padapter, SYS_ISO_CTRL + 1, val8);
		val8 = r8712_read8(padapter, SYS_ISO_CTRL + 1);
		val8 = val8 & 0xEF;
		
		r8712_write8(padapter, SYS_ISO_CTRL + 1, val8);
		val8 = r8712_read8(padapter, AFE_XTAL_CTRL + 1);
		val8 = val8 & 0xFB;
		
		r8712_write8(padapter, AFE_XTAL_CTRL + 1, val8);
		val8 = r8712_read8(padapter, AFE_PLL_CTRL);
		val8 = val8 | 0x01;
		
		r8712_write8(padapter, AFE_PLL_CTRL, val8);
		val8 = 0xEE;
		
		r8712_write8(padapter, SYS_ISO_CTRL, val8);
		val8 = r8712_read8(padapter, SYS_CLKR + 1);
		val8 = val8 | 0x08;
		
		r8712_write8(padapter, SYS_CLKR + 1, val8);
		val8 = r8712_read8(padapter, SYS_FUNC_EN + 1);
		val8 = val8 | 0x08;
		
		r8712_write8(padapter, SYS_FUNC_EN + 1, val8);
		val8 = val8 | 0x80;
		
		r8712_write8(padapter, SYS_FUNC_EN + 1, val8);
		val8 = r8712_read8(padapter, SYS_CLKR + 1);
		val8 = (val8 | 0x80) & 0xBF;
		
		r8712_write8(padapter, SYS_CLKR + 1, val8);
		val8 = 0xFC;
		r8712_write8(padapter, CR, val8);
		val8 = 0x37;
		r8712_write8(padapter, CR + 1, val8);
		
		r8712_write8(padapter, 0x102500ab, r8712_read8(padapter,
			     0x102500ab) | BIT(6) | BIT(7));
		
		r8712_write8(padapter, 0x10250008, r8712_read8(padapter,
			     0x10250008) & 0xfffffffb);
	} else if (pregistrypriv->chip_version == RTL8712_1stCUT) {
		
		r8712_write8(padapter, SPS0_CTRL + 1, 0x53);
		r8712_write8(padapter, SPS0_CTRL, 0x57);
		val8 = r8712_read8(padapter, AFE_MISC);
		r8712_write8(padapter, AFE_MISC, (val8 | AFE_MISC_BGEN |
			     AFE_MISC_MBEN));
		
		val8 = r8712_read8(padapter, LDOA15_CTRL);
		r8712_write8(padapter, LDOA15_CTRL, (val8 | LDA15_EN));
		val8 = r8712_read8(padapter, SPS1_CTRL);
		r8712_write8(padapter, SPS1_CTRL, (val8 | SPS1_LDEN));
		msleep(20);
		
		val8 = r8712_read8(padapter, SPS1_CTRL);
		r8712_write8(padapter, SPS1_CTRL, (val8 | SPS1_SWEN));
		r8712_write32(padapter, SPS1_CTRL, 0x00a7b267);
		val8 = r8712_read8(padapter, SYS_ISO_CTRL + 1);
		r8712_write8(padapter, SYS_ISO_CTRL + 1, (val8 | 0x08));
		
		val8 = r8712_read8(padapter, SYS_FUNC_EN + 1);
		r8712_write8(padapter, SYS_FUNC_EN + 1, (val8 | 0x20));
		val8 = r8712_read8(padapter, SYS_ISO_CTRL + 1);
		r8712_write8(padapter, SYS_ISO_CTRL + 1, (val8 & 0x6F));
		
		val8 = r8712_read8(padapter, AFE_XTAL_CTRL + 1);
		r8712_write8(padapter, AFE_XTAL_CTRL + 1, (val8 & 0xfb));
		
		val8 = r8712_read8(padapter, AFE_PLL_CTRL);
		r8712_write8(padapter, AFE_PLL_CTRL, (val8 | 0x11));
		
		val8 = r8712_read8(padapter, SYS_ISO_CTRL);
		r8712_write8(padapter, SYS_ISO_CTRL, (val8 & 0xEE));
		
		val8 = r8712_read8(padapter, SYS_CLKR);
		r8712_write8(padapter, SYS_CLKR, val8 & (~SYS_CLKSEL));
		
		val8 = r8712_read8(padapter, SYS_CLKR);
		
		val8 = r8712_read8(padapter, SYS_CLKR + 1);
		r8712_write8(padapter, SYS_CLKR + 1, (val8 | 0x18));
		
		r8712_write8(padapter, PMC_FSM, 0x02);
		
		val8 = r8712_read8(padapter, SYS_FUNC_EN + 1);
		r8712_write8(padapter, SYS_FUNC_EN + 1, (val8 | 0x08));
		
		val8 = r8712_read8(padapter, SYS_FUNC_EN + 1);
		r8712_write8(padapter, SYS_FUNC_EN + 1, (val8 | 0x80));
		
		val8 = r8712_read8(padapter, SYS_CLKR + 1);
		r8712_write8(padapter, SYS_CLKR + 1, (val8 | 0x80) & 0xBF);
		r8712_write8(padapter, CR, 0xFC);
		r8712_write8(padapter, CR + 1, 0x37);
		
		val8 = r8712_read8(padapter, 0x1025FE5c);
		r8712_write8(padapter, 0x1025FE5c, (val8|BIT(7)));
		val8 = r8712_read8(padapter, 0x102500ab);
		r8712_write8(padapter, 0x102500ab, (val8|BIT(6)|BIT(7)));
		
		val8 = r8712_read8(padapter, SYS_CLKR);
		r8712_write8(padapter, SYS_CLKR, val8&(~CPU_CLKSEL));
	} else if (pregistrypriv->chip_version == RTL8712_2ndCUT ||
		  pregistrypriv->chip_version == RTL8712_3rdCUT) {
		r8712_write8(padapter, 0x37, 0xb0);
		msleep(20);
		r8712_write8(padapter, 0x37, 0x30);
		val8 = r8712_read8(padapter, SYS_CLKR + 1);
		if (val8 & 0x80) {
			val8 &= 0x3f;
			r8712_write8(padapter, SYS_CLKR + 1, val8);
		}
		val8 = r8712_read8(padapter, SYS_FUNC_EN + 1);
		val8 &= 0x73;
		r8712_write8(padapter, SYS_FUNC_EN + 1, val8);
		msleep(20);
		
		r8712_write8(padapter, SPS0_CTRL + 1, 0x53);
		r8712_write8(padapter, SPS0_CTRL, 0x57);
		val8 = r8712_read8(padapter, AFE_MISC);
		
		r8712_write8(padapter, AFE_MISC, (val8 | AFE_MISC_BGEN));
		r8712_write8(padapter, AFE_MISC, (val8 | AFE_MISC_BGEN |
			     AFE_MISC_MBEN | AFE_MISC_I32_EN));
		
		val8 = r8712_read8(padapter, LDOA15_CTRL);
		r8712_write8(padapter, LDOA15_CTRL, (val8 | LDA15_EN));
		
		val8 = r8712_read8(padapter, LDOV12D_CTRL);
		r8712_write8(padapter, LDOV12D_CTRL, (val8 | LDV12_EN));
		val8 = r8712_read8(padapter, SYS_ISO_CTRL + 1);
		r8712_write8(padapter, SYS_ISO_CTRL + 1, (val8 | 0x08));
		
		val8 = r8712_read8(padapter, SYS_FUNC_EN + 1);
		r8712_write8(padapter, SYS_FUNC_EN + 1, (val8 | 0x20));
		
		val8 = r8712_read8(padapter, SYS_ISO_CTRL + 1);
		r8712_write8(padapter, SYS_ISO_CTRL + 1, (val8 & 0x68));
		
		val8 = r8712_read8(padapter, AFE_XTAL_CTRL + 1);
		r8712_write8(padapter, AFE_XTAL_CTRL + 1, (val8 & 0xfb));
		
		val8 = r8712_read8(padapter, AFE_PLL_CTRL);
		r8712_write8(padapter, AFE_PLL_CTRL, (val8 | 0x11));
		udelay(500);
		r8712_write8(padapter, AFE_PLL_CTRL, (val8 | 0x51));
		udelay(500);
		r8712_write8(padapter, AFE_PLL_CTRL, (val8 | 0x11));
		udelay(500);
		
		val8 = r8712_read8(padapter, SYS_ISO_CTRL);
		r8712_write8(padapter, SYS_ISO_CTRL, (val8 & 0xEE));
		
		r8712_write8(padapter, SYS_CLKR, 0x00);
		val8 = r8712_read8(padapter, SYS_CLKR);
		r8712_write8(padapter, SYS_CLKR, (val8 | 0xa0));
		
		val8 = r8712_read8(padapter, SYS_CLKR + 1);
		r8712_write8(padapter, SYS_CLKR + 1, (val8 | 0x18));
		
		r8712_write8(padapter, PMC_FSM, 0x02);
		
		val8 = r8712_read8(padapter, SYS_FUNC_EN + 1);
		r8712_write8(padapter, SYS_FUNC_EN + 1, (val8 | 0x08));
		
		val8 = r8712_read8(padapter, SYS_FUNC_EN + 1);
		r8712_write8(padapter, SYS_FUNC_EN + 1, (val8 | 0x80));
		
		val8 = r8712_read8(padapter, SYS_CLKR + 1);
		r8712_write8(padapter, SYS_CLKR + 1, (val8 | 0x80) & 0xBF);
		r8712_write8(padapter, CR, 0xFC);
		r8712_write8(padapter, CR + 1, 0x37);
		
		val8 = r8712_read8(padapter, 0x1025FE5c);
		r8712_write8(padapter, 0x1025FE5c, (val8 | BIT(7)));
		
		val8 = r8712_read8(padapter, SYS_CLKR);
		r8712_write8(padapter, SYS_CLKR, val8 & (~CPU_CLKSEL));
		
		r8712_write8(padapter, 0x1025fe1c, 0x80);
		do {
			val8 = r8712_read8(padapter, TCR);
			if ((val8 & _TXDMA_INIT_VALUE) == _TXDMA_INIT_VALUE)
				break;
			udelay(5); 
		} while (PollingCnt--);	

		if (PollingCnt <= 0) {
			val8 = r8712_read8(padapter, CR);
			r8712_write8(padapter, CR, val8&(~_TXDMA_EN));
			udelay(2); 
			
			r8712_write8(padapter, CR, val8|_TXDMA_EN);
		}
	} else
		ret = _FAIL;
	return ret;
}

unsigned int r8712_usb_inirp_init(struct _adapter *padapter)
{
	u8 i;
	struct recv_buf *precvbuf;
	struct intf_hdl *pintfhdl = &padapter->pio_queue->intf;
	struct recv_priv *precvpriv = &(padapter->recvpriv);

	precvpriv->ff_hwaddr = RTL8712_DMA_RX0FF; 
	
	precvbuf = (struct recv_buf *)precvpriv->precv_buf;
	for (i = 0; i < NR_RECVBUFF; i++) {
		if (r8712_usb_read_port(pintfhdl, precvpriv->ff_hwaddr, 0,
		   (unsigned char *)precvbuf) == false)
			return _FAIL;
		precvbuf++;
		precvpriv->free_recv_buf_queue_cnt--;
	}
	return _SUCCESS;
}

unsigned int r8712_usb_inirp_deinit(struct _adapter *padapter)
{
	r8712_usb_read_port_cancel(padapter);
	return _SUCCESS;
}
