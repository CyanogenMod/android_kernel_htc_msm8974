/******************************************************************************
 *
 * Copyright(c) 2007 - 2010 Realtek Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
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
#ifndef __RTL8712_SYSCFG_BITDEF_H__
#define __RTL8712_SYSCFG_BITDEF_H__


#define iso_LDR2RP_SHT		8 
#define iso_LDR2RP		BIT(iso_LDR2RP_SHT) 

#define FEN_DIO_SDIO_SHT	0
#define FEN_DIO_SDIO		BIT(FEN_DIO_SDIO_SHT)
#define FEN_SDIO_SHT		1
#define FEN_SDIO		BIT(FEN_SDIO_SHT)
#define FEN_USBA_SHT		2
#define FEN_USBA		BIT(FEN_USBA_SHT)
#define FEN_UPLL_SHT		3
#define FEN_UPLL		BIT(FEN_UPLL_SHT)
#define FEN_USBD_SHT		4
#define FEN_USBD		BIT(FEN_USBD_SHT)
#define FEN_DIO_PCIE_SHT	5
#define FEN_DIO_PCIE		BIT(FEN_DIO_PCIE_SHT)
#define FEN_PCIEA_SHT		6
#define FEN_PCIEA		BIT(FEN_PCIEA_SHT)
#define FEN_PPLL_SHT		7
#define FEN_PPLL		BIT(FEN_PPLL_SHT)
#define FEN_PCIED_SHT		8
#define FEN_PCIED		BIT(FEN_PCIED_SHT)
#define FEN_CPUEN_SHT		10
#define FEN_CPUEN		BIT(FEN_CPUEN_SHT)
#define FEN_DCORE_SHT		11
#define FEN_DCORE		BIT(FEN_DCORE_SHT)
#define FEN_ELDR_SHT		12
#define FEN_ELDR		BIT(FEN_ELDR_SHT)
#define PWC_DV2LDR_SHT		13
#define PWC_DV2LDR		BIT(PWC_DV2LDR_SHT) 

#define SYS_CLKSEL_SHT		0
#define SYS_CLKSEL		BIT(SYS_CLKSEL_SHT) 
#define PS_CLKSEL_SHT		1
#define PS_CLKSEL		BIT(PS_CLKSEL_SHT) 
#define CPU_CLKSEL_SHT		2
#define CPU_CLKSEL		BIT(CPU_CLKSEL_SHT) 
#define INT32K_EN_SHT		3
#define INT32K_EN		BIT(INT32K_EN_SHT)
#define MACSLP_SHT		4
#define MACSLP			BIT(MACSLP_SHT)
#define MAC_CLK_EN_SHT		11
#define MAC_CLK_EN		BIT(MAC_CLK_EN_SHT) 
#define SYS_CLK_EN_SHT		12
#define SYS_CLK_EN		BIT(SYS_CLK_EN_SHT)
#define RING_CLK_EN_SHT		13
#define RING_CLK_EN		BIT(RING_CLK_EN_SHT)
#define SWHW_SEL_SHT		14
#define SWHW_SEL		BIT(SWHW_SEL_SHT) 
#define FWHW_SEL_SHT		15
#define FWHW_SEL		BIT(FWHW_SEL_SHT) 

#define	_VPDIDX_MSK		0xFF00
#define	_VPDIDX_SHT		8
#define	_EEM_MSK		0x00C0
#define	_EEM_SHT		6
#define	_EEM0			BIT(6)
#define	_EEM1			BIT(7)
#define	_EEPROM_EN		BIT(5)
#define	_9356SEL		BIT(4)
#define	_EECS			BIT(3)
#define	_EESK			BIT(2)
#define	_EEDI			BIT(1)
#define	_EEDO			BIT(0)

#define	AFE_MISC_USB_MBEN_SHT	7
#define	AFE_MISC_USB_MBEN	BIT(AFE_MISC_USB_MBEN_SHT)
#define	AFE_MISC_USB_BGEN_SHT	6
#define	AFE_MISC_USB_BGEN	BIT(AFE_MISC_USB_BGEN_SHT)
#define	AFE_MISC_LD12_VDAJ_SHT	4
#define	AFE_MISC_LD12_VDAJ_MSK	0X0030
#define	AFE_MISC_LD12_VDAJ	BIT(AFE_MISC_LD12_VDAJ_SHT)
#define	AFE_MISC_I32_EN_SHT	3
#define	AFE_MISC_I32_EN		BIT(AFE_MISC_I32_EN_SHT)
#define	AFE_MISC_E32_EN_SHT	2
#define	AFE_MISC_E32_EN		BIT(AFE_MISC_E32_EN_SHT)
#define	AFE_MISC_MBEN_SHT	1
#define	AFE_MISC_MBEN		BIT(AFE_MISC_MBEN_SHT)
#define	AFE_MISC_BGEN_SHT	0
#define	AFE_MISC_BGEN		BIT(AFE_MISC_BGEN_SHT)


#define	SPS1_SWEN		BIT(1)	
#define	SPS1_LDEN		BIT(0)	


#define	LDA15_EN		BIT(0)	


#define	LDV12_EN		BIT(0)	
#define	LDV12_SDBY		BIT(1)	

#define	_CLK_GATE_EN		BIT(0)


#define EF_FLAG			BIT(31)		
#define EF_PGPD			0x70000000	
#define EF_RDT			0x0F000000	
#define EF_PDN_EN		BIT(19)		
#define ALD_EN			BIT(18)		
#define EF_ADDR			0x0003FF00	
#define EF_DATA			0x000000FF	

#define LDOE25_EN		BIT(31)		

#define EFUSE_CLK_EN		BIT(1)		
#define EFUSE_CLK_SEL		BIT(0)		

#endif	

