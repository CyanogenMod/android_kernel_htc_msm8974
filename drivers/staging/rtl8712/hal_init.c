/******************************************************************************
 * hal_init.c
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
 * WLAN FAE <wlanfae@realtek.com>.
 * Larry Finger <Larry.Finger@lwfinger.net>
 *
 ******************************************************************************/

#define _HAL_INIT_C_

#include <linux/usb.h>
#include <linux/device.h>
#include <linux/usb/ch9.h>
#include <linux/firmware.h>
#include <linux/module.h>

#include "osdep_service.h"
#include "drv_types.h"
#include "rtl871x_byteorder.h"
#include "usb_osintf.h"

#define FWBUFF_ALIGN_SZ 512
#define MAX_DUMP_FWSZ	49152 

static void rtl871x_load_fw_cb(const struct firmware *firmware, void *context)
{
	struct _adapter *padapter = context;

	complete(&padapter->rtl8712_fw_ready);
	if (!firmware) {
		struct usb_device *udev = padapter->dvobjpriv.pusbdev;
		struct usb_interface *pusb_intf = padapter->pusb_intf;
		printk(KERN_ERR "r8712u: Firmware request failed\n");
		padapter->fw_found = false;
		usb_put_dev(udev);
		usb_set_intfdata(pusb_intf, NULL);
		return;
	}
	padapter->fw = firmware;
	padapter->fw_found = true;
	
	register_netdev(padapter->pnetdev);
}

static const char firmware_file[] = "rtlwifi/rtl8712u.bin";

int rtl871x_load_fw(struct _adapter *padapter)
{
	struct device *dev = &padapter->dvobjpriv.pusbdev->dev;
	int rc;

	init_completion(&padapter->rtl8712_fw_ready);
	printk(KERN_INFO "r8712u: Loading firmware from \"%s\"\n",
	       firmware_file);
	rc = request_firmware_nowait(THIS_MODULE, 1, firmware_file, dev,
				     GFP_KERNEL, padapter, rtl871x_load_fw_cb);
	if (rc)
		printk(KERN_ERR "r8712u: Firmware request error %d\n", rc);
	return rc;
}
MODULE_FIRMWARE("rtlwifi/rtl8712u.bin");

static u32 rtl871x_open_fw(struct _adapter *padapter, const u8 **ppmappedfw)
{
	const struct firmware **praw = &padapter->fw;

	if (padapter->fw->size > 200000) {
		printk(KERN_ERR "r8172u: Badfw->size of %d\n",
		       (int)padapter->fw->size);
		return 0;
	}
	*ppmappedfw = (u8 *)((*praw)->data);
	return (*praw)->size;
}

static void fill_fwpriv(struct _adapter *padapter, struct fw_priv *pfwpriv)
{
	struct dvobj_priv *pdvobj = (struct dvobj_priv *)&padapter->dvobjpriv;
	struct registry_priv *pregpriv = &padapter->registrypriv;

	memset(pfwpriv, 0, sizeof(struct fw_priv));
	
	pfwpriv->hci_sel =  RTL8712_HCI_TYPE_72USB;
	pfwpriv->usb_ep_num = (u8)pdvobj->nr_endpoint;
	pfwpriv->bw_40MHz_en = pregpriv->cbw40_enable;
	switch (pregpriv->rf_config) {
	case RTL8712_RF_1T1R:
		pfwpriv->rf_config = RTL8712_RFC_1T1R;
		break;
	case RTL8712_RF_2T2R:
		pfwpriv->rf_config = RTL8712_RFC_2T2R;
		break;
	case RTL8712_RF_1T2R:
	default:
		pfwpriv->rf_config = RTL8712_RFC_1T2R;
	}
	pfwpriv->mp_mode = (pregpriv->mp_mode == 1) ? 1 : 0;
	pfwpriv->vcsType = pregpriv->vrtl_carrier_sense; 
	pfwpriv->vcsMode = pregpriv->vcs_type; 
	
	pfwpriv->turboMode = ((pregpriv->wifi_test == 1) ? 0 : 1);
	pfwpriv->lowPowerMode = pregpriv->low_power;
}

static void update_fwhdr(struct fw_hdr	*pfwhdr, const u8 *pmappedfw)
{
	pfwhdr->signature = le16_to_cpu(*(u16 *)pmappedfw);
	pfwhdr->version = le16_to_cpu(*(u16 *)(pmappedfw+2));
	
	pfwhdr->dmem_size = le32_to_cpu(*(uint *)(pmappedfw+4));
	
	pfwhdr->img_IMEM_size = le32_to_cpu(*(uint *)(pmappedfw+8));
	
	pfwhdr->img_SRAM_size = le32_to_cpu(*(uint *)(pmappedfw+12));
	
	pfwhdr->fw_priv_sz = le32_to_cpu(*(uint *)(pmappedfw+16));
}

static u8 chk_fwhdr(struct fw_hdr *pfwhdr, u32 ulfilelength)
{
	u32	fwhdrsz, fw_sz;
	u8 intf, rfconf;

	
	if ((pfwhdr->signature != 0x8712) && (pfwhdr->signature != 0x8192))
		return _FAIL;
	
	intf = (u8)((pfwhdr->version&0x3000) >> 12);
	
	rfconf = (u8)((pfwhdr->version&0xC000) >> 14);
	
	if (pfwhdr->fw_priv_sz != sizeof(struct fw_priv))
		return _FAIL;
	
	fwhdrsz = FIELD_OFFSET(struct fw_hdr, fwpriv) + pfwhdr->fw_priv_sz;
	fw_sz =  fwhdrsz + pfwhdr->img_IMEM_size + pfwhdr->img_SRAM_size +
		 pfwhdr->dmem_size;
	if (fw_sz != ulfilelength)
		return _FAIL;
	return _SUCCESS;
}

static u8 rtl8712_dl_fw(struct _adapter *padapter)
{
	sint i;
	u8 tmp8, tmp8_a;
	u16 tmp16;
	u32 maxlen = 0, tmp32; 
	uint dump_imem_sz, imem_sz, dump_emem_sz, emem_sz; 
	struct fw_hdr fwhdr;
	u32 ulfilelength;	
	const u8 *pmappedfw = NULL;
	u8 *ptmpchar = NULL, *ppayload, *ptr;
	struct tx_desc *ptx_desc;
	u32 txdscp_sz = sizeof(struct tx_desc);
	u8 ret = _FAIL;

	ulfilelength = rtl871x_open_fw(padapter, &pmappedfw);
	if (pmappedfw && (ulfilelength > 0)) {
		update_fwhdr(&fwhdr, pmappedfw);
		if (chk_fwhdr(&fwhdr, ulfilelength) == _FAIL)
			return ret;
		fill_fwpriv(padapter, &fwhdr.fwpriv);
		
		maxlen = (fwhdr.img_IMEM_size > fwhdr.img_SRAM_size) ?
			  fwhdr.img_IMEM_size : fwhdr.img_SRAM_size;
		maxlen += txdscp_sz;
		ptmpchar = _malloc(maxlen + FWBUFF_ALIGN_SZ);
		if (ptmpchar == NULL)
			return ret;

		ptx_desc = (struct tx_desc *)(ptmpchar + FWBUFF_ALIGN_SZ -
			    ((addr_t)(ptmpchar) & (FWBUFF_ALIGN_SZ - 1)));
		ppayload = (u8 *)(ptx_desc) + txdscp_sz;
		ptr = (u8 *)pmappedfw + FIELD_OFFSET(struct fw_hdr, fwpriv) +
		      fwhdr.fw_priv_sz;
		
		
		imem_sz = fwhdr.img_IMEM_size;
		do {
			memset(ptx_desc, 0, TXDESC_SIZE);
			if (imem_sz >  MAX_DUMP_FWSZ)
				dump_imem_sz = MAX_DUMP_FWSZ;
			else {
				dump_imem_sz = imem_sz;
				ptx_desc->txdw0 |= cpu_to_le32(BIT(28));
			}
			ptx_desc->txdw0 |= cpu_to_le32(dump_imem_sz &
						       0x0000ffff);
			memcpy(ppayload, ptr, dump_imem_sz);
			r8712_write_mem(padapter, RTL8712_DMA_VOQ,
				  dump_imem_sz + TXDESC_SIZE,
				  (u8 *)ptx_desc);
			ptr += dump_imem_sz;
			imem_sz -= dump_imem_sz;
		} while (imem_sz > 0);
		i = 10;
		tmp16 = r8712_read16(padapter, TCR);
		while (((tmp16 & _IMEM_CODE_DONE) == 0) && (i > 0)) {
			udelay(10);
			tmp16 = r8712_read16(padapter, TCR);
			i--;
		}
		if (i == 0 || (tmp16 & _IMEM_CHK_RPT) == 0)
			goto exit_fail;

		
		emem_sz = fwhdr.img_SRAM_size;
		do {
			memset(ptx_desc, 0, TXDESC_SIZE);
			if (emem_sz >  MAX_DUMP_FWSZ) 
				dump_emem_sz = MAX_DUMP_FWSZ;
			else {
				dump_emem_sz = emem_sz;
				ptx_desc->txdw0 |= cpu_to_le32(BIT(28));
			}
			ptx_desc->txdw0 |= cpu_to_le32(dump_emem_sz &
						       0x0000ffff);
			memcpy(ppayload, ptr, dump_emem_sz);
			r8712_write_mem(padapter, RTL8712_DMA_VOQ,
				  dump_emem_sz+TXDESC_SIZE, (u8 *)ptx_desc);
			ptr += dump_emem_sz;
			emem_sz -= dump_emem_sz;
		} while (emem_sz > 0);
		i = 5;
		tmp16 = r8712_read16(padapter, TCR);
		while (((tmp16 & _EMEM_CODE_DONE) == 0) && (i > 0)) {
			udelay(10);
			tmp16 = r8712_read16(padapter, TCR);
			i--;
		}
		if (i == 0 || (tmp16 & _EMEM_CHK_RPT) == 0)
			goto exit_fail;

		
		tmp8 = r8712_read8(padapter, SYS_CLKR);
		r8712_write8(padapter, SYS_CLKR, tmp8|BIT(2));
		tmp8_a = r8712_read8(padapter, SYS_CLKR);
		if (tmp8_a != (tmp8|BIT(2)))
			goto exit_fail;

		tmp8 = r8712_read8(padapter, SYS_FUNC_EN + 1);
		r8712_write8(padapter, SYS_FUNC_EN+1, tmp8|BIT(2));
		tmp8_a = r8712_read8(padapter, SYS_FUNC_EN + 1);
		if (tmp8_a != (tmp8|BIT(2)))
			goto exit_fail;

		tmp32 = r8712_read32(padapter, TCR);

		
		i = 100;
		tmp16 = r8712_read16(padapter, TCR);
		while (((tmp16 & _IMEM_RDY) == 0) && (i > 0)) {
			msleep(20);
			tmp16 = r8712_read16(padapter, TCR);
			i--;
		}
		if (i == 0) {
			r8712_write16(padapter, 0x10250348, 0xc000);
			r8712_write16(padapter, 0x10250348, 0xc001);
			r8712_write16(padapter, 0x10250348, 0x2000);
			r8712_write16(padapter, 0x10250348, 0x2001);
			r8712_write16(padapter, 0x10250348, 0x2002);
			r8712_write16(padapter, 0x10250348, 0x2003);
			goto exit_fail;
		}
		
		memset(ptx_desc, 0, TXDESC_SIZE);
		ptx_desc->txdw0 |= cpu_to_le32(fwhdr.fw_priv_sz&0x0000ffff);
		ptx_desc->txdw0 |= cpu_to_le32(BIT(28));
		memcpy(ppayload, &fwhdr.fwpriv, fwhdr.fw_priv_sz);
		r8712_write_mem(padapter, RTL8712_DMA_VOQ,
			  fwhdr.fw_priv_sz + TXDESC_SIZE, (u8 *)ptx_desc);

		
		i = 100;
		tmp16 = r8712_read16(padapter, TCR);
		while (((tmp16 & _DMEM_CODE_DONE) == 0) && (i > 0)) {
			msleep(20);
			tmp16 = r8712_read16(padapter, TCR);
			i--;
		}
		if (i == 0)
			goto exit_fail;

		tmp8 = r8712_read8(padapter, 0x1025000A);
		if (tmp8 & BIT(4)) 
			i = 60;
		else			
			i = 30;
		tmp16 = r8712_read16(padapter, TCR);
		while (((tmp16 & _FWRDY) == 0) && (i > 0)) {
			msleep(100);
			tmp16 = r8712_read16(padapter, TCR);
			i--;
		}
		if (i == 0)
			goto exit_fail;
	} else
		goto exit_fail;
	ret = _SUCCESS;

exit_fail:
	kfree(ptmpchar);
	return ret;
}

uint rtl8712_hal_init(struct _adapter *padapter)
{
	u32 val32;
	int i;

	
	if (rtl8712_dl_fw(padapter) != _SUCCESS)
		return _FAIL;

	printk(KERN_INFO "r8712u: 1 RCR=0x%x\n",  r8712_read32(padapter, RCR));
	val32 = r8712_read32(padapter, RCR);
	r8712_write32(padapter, RCR, (val32 | BIT(26))); 
	printk(KERN_INFO "r8712u: 2 RCR=0x%x\n", r8712_read32(padapter, RCR));
	val32 = r8712_read32(padapter, RCR);
	r8712_write32(padapter, RCR, (val32|BIT(25))); 
	val32 = 0;
	val32 = r8712_read32(padapter, 0x10250040);
	r8712_write32(padapter,  0x10250040, (val32&0x00FFFFFF));
	
	r8712_write8(padapter, 0x102500B5, r8712_read8(padapter, 0x102500B5) |
	       BIT(0)); 
	r8712_write8(padapter, 0x102500BD, r8712_read8(padapter, 0x102500BD) |
	       BIT(7)); 
	r8712_write8(padapter, 0x102500D9, 1); 
	r8712_write8(padapter, 0x1025FE5B, 0x04); 
	
	r8712_write8(padapter, 0x1025fe5C, r8712_read8(padapter, 0x1025fe5C)
		     | BIT(7));
	for (i = 0; i < 6; i++)
		padapter->eeprompriv.mac_addr[i] = r8712_read8(padapter,
							       MACID + i);
	return _SUCCESS;
}

uint rtl8712_hal_deinit(struct _adapter *padapter)
{
	r8712_write8(padapter, RF_CTRL, 0x00);
	
	msleep(20);
	
	r8712_write8(padapter, SYS_CLKR+1, 0x38); 
	r8712_write8(padapter, SYS_FUNC_EN+1, 0x70);
	r8712_write8(padapter, PMC_FSM, 0x06);  
	r8712_write8(padapter, SYS_ISO_CTRL, 0xF9); 
	r8712_write8(padapter, SYS_ISO_CTRL+1, 0xe8); 
	r8712_write8(padapter, AFE_PLL_CTRL, 0x00); 
	r8712_write8(padapter, LDOA15_CTRL, 0x54);  
	r8712_write8(padapter, SYS_FUNC_EN+1, 0x50); 
	r8712_write8(padapter, LDOV12D_CTRL, 0x24); 
	r8712_write8(padapter, AFE_MISC, 0x30); 
	
	r8712_write8(padapter, SPS0_CTRL, 0x56); 
	r8712_write8(padapter, SPS0_CTRL+1, 0x43);  
	return _SUCCESS;
}

uint rtl871x_hal_init(struct _adapter *padapter)
{
	padapter->hw_init_completed = false;
	if (padapter->halpriv.hal_bus_init == NULL)
		return _FAIL;
	else {
		if (padapter->halpriv.hal_bus_init(padapter) != _SUCCESS)
			return _FAIL;
	}
	if (rtl8712_hal_init(padapter) == _SUCCESS)
		padapter->hw_init_completed = true;
	else {
		padapter->hw_init_completed = false;
		return _FAIL;
	}
	return _SUCCESS;
}
