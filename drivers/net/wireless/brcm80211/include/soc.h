/*
 * Copyright (c) 2010 Broadcom Corporation
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef	_BRCM_SOC_H
#define	_BRCM_SOC_H

#define SI_ENUM_BASE		0x18000000	

#define	NODEV_CORE_ID		0x700	
#define	CC_CORE_ID		0x800	
#define	ILINE20_CORE_ID		0x801	
#define	SRAM_CORE_ID		0x802	
#define	SDRAM_CORE_ID		0x803	
#define	PCI_CORE_ID		0x804	
#define	MIPS_CORE_ID		0x805	
#define	ENET_CORE_ID		0x806	
#define	CODEC_CORE_ID		0x807	
#define	USB_CORE_ID		0x808	
#define	ADSL_CORE_ID		0x809	
#define	ILINE100_CORE_ID	0x80a	
#define	IPSEC_CORE_ID		0x80b	
#define	UTOPIA_CORE_ID		0x80c	
#define	PCMCIA_CORE_ID		0x80d	
#define	SOCRAM_CORE_ID		0x80e	
#define	MEMC_CORE_ID		0x80f	
#define	OFDM_CORE_ID		0x810	
#define	EXTIF_CORE_ID		0x811	
#define	D11_CORE_ID		0x812	
#define	APHY_CORE_ID		0x813	
#define	BPHY_CORE_ID		0x814	
#define	GPHY_CORE_ID		0x815	
#define	MIPS33_CORE_ID		0x816	
#define	USB11H_CORE_ID		0x817	
#define	USB11D_CORE_ID		0x818	
#define	USB20H_CORE_ID		0x819	
#define	USB20D_CORE_ID		0x81a	
#define	SDIOH_CORE_ID		0x81b	
#define	ROBO_CORE_ID		0x81c	
#define	ATA100_CORE_ID		0x81d	
#define	SATAXOR_CORE_ID		0x81e	
#define	GIGETH_CORE_ID		0x81f	
#define	PCIE_CORE_ID		0x820	
#define	NPHY_CORE_ID		0x821	
#define	SRAMC_CORE_ID		0x822	
#define	MINIMAC_CORE_ID		0x823	
#define	ARM11_CORE_ID		0x824	
#define	ARM7S_CORE_ID		0x825	
#define	LPPHY_CORE_ID		0x826	
#define	PMU_CORE_ID		0x827	
#define	SSNPHY_CORE_ID		0x828	
#define	SDIOD_CORE_ID		0x829	
#define	ARMCM3_CORE_ID		0x82a	
#define	HTPHY_CORE_ID		0x82b	
#define	MIPS74K_CORE_ID		0x82c	
#define	GMAC_CORE_ID		0x82d	
#define	DMEMC_CORE_ID		0x82e	
#define	PCIERC_CORE_ID		0x82f	
#define	OCP_CORE_ID		0x830	
#define	SC_CORE_ID		0x831	
#define	AHB_CORE_ID		0x832	
#define	SPIH_CORE_ID		0x833	
#define	I2S_CORE_ID		0x834	
#define	DMEMS_CORE_ID		0x835	
#define	DEF_SHIM_COMP		0x837	
#define OOB_ROUTER_CORE_ID	0x367	
#define	DEF_AI_COMP		0xfff	

#define	SICF_BIST_EN		0x8000
#define	SICF_PME_EN		0x4000
#define	SICF_CORE_BITS		0x3ffc
#define	SICF_FGC		0x0002
#define	SICF_CLOCK_EN		0x0001

#define	SISF_BIST_DONE		0x8000
#define	SISF_BIST_ERROR		0x4000
#define	SISF_GATED_CLK		0x2000
#define	SISF_DMA64		0x1000
#define	SISF_CORE_BITS		0x0fff

#endif				
