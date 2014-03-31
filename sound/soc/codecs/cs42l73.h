/*
 * ALSA SoC CS42L73 codec driver
 *
 * Copyright 2011 Cirrus Logic, Inc.
 *
 * Author: Georgi Vlaev <joe@nucleusys.com>
 *	   Brian Austin <brian.austin@cirrus.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

#ifndef __CS42L73_H__
#define __CS42L73_H__

#define CS42L73_CHIP_ID		0x4a
#define CS42L73_DEVID_AB	0x01	
#define CS42L73_DEVID_CD	0x02    
#define CS42L73_DEVID_E		0x03    
#define CS42L73_REVID		0x05    
#define CS42L73_PWRCTL1		0x06    
#define CS42L73_PWRCTL2		0x07    
#define CS42L73_PWRCTL3		0x08    
#define CS42L73_CPFCHC		0x09    
#define CS42L73_OLMBMSDC	0x0A    
#define CS42L73_DMMCC		0x0B    
#define CS42L73_XSPC		0x0C    
#define CS42L73_XSPMMCC		0x0D    
#define CS42L73_ASPC		0x0E    
#define CS42L73_ASPMMCC		0x0F    
#define CS42L73_VSPC		0x10    
#define CS42L73_VSPMMCC		0x11    
#define CS42L73_VXSPFS		0x12    
#define CS42L73_MIOPC		0x13    
#define CS42L73_ADCIPC		0x14	
#define CS42L73_MICAPREPGAAVOL	0x15	
#define CS42L73_MICBPREPGABVOL	0x16	
#define CS42L73_IPADVOL		0x17	
#define CS42L73_IPBDVOL		0x18	
#define CS42L73_PBDC		0x19	
#define CS42L73_HLADVOL		0x1A	
#define CS42L73_HLBDVOL		0x1B	
#define CS42L73_SPKDVOL		0x1C	
#define CS42L73_ESLDVOL		0x1D	
#define CS42L73_HPAAVOL		0x1E	
#define CS42L73_HPBAVOL		0x1F	
#define CS42L73_LOAAVOL		0x20	
#define CS42L73_LOBAVOL		0x21	
#define CS42L73_STRINV		0x22	
#define CS42L73_XSPINV		0x23	
#define CS42L73_ASPINV		0x24	
#define CS42L73_VSPINV		0x25	
#define CS42L73_LIMARATEHL	0x26	
#define CS42L73_LIMRRATEHL	0x27	
#define CS42L73_LMAXHL		0x28	
#define CS42L73_LIMARATESPK	0x29	
#define CS42L73_LIMRRATESPK	0x2A	
#define CS42L73_LMAXSPK		0x2B	
#define CS42L73_LIMARATEESL	0x2C	
#define CS42L73_LIMRRATEESL	0x2D	
#define CS42L73_LMAXESL		0x2E	
#define CS42L73_ALCARATE	0x2F	
#define CS42L73_ALCRRATE	0x30	
#define CS42L73_ALCMINMAX	0x31	
#define CS42L73_NGCAB		0x32	
#define CS42L73_ALCNGMC		0x33	
#define CS42L73_MIXERCTL	0x34	
#define CS42L73_HLAIPAA		0x35	
#define CS42L73_HLBIPBA		0x36	
#define CS42L73_HLAXSPAA	0x37	
#define CS42L73_HLBXSPBA	0x38	
#define CS42L73_HLAASPAA	0x39	
#define CS42L73_HLBASPBA	0x3A	
#define CS42L73_HLAVSPMA	0x3B	
#define CS42L73_HLBVSPMA	0x3C	
#define CS42L73_XSPAIPAA	0x3D	
#define CS42L73_XSPBIPBA	0x3E	
#define CS42L73_XSPAXSPAA	0x3F	
#define CS42L73_XSPBXSPBA	0x40	
#define CS42L73_XSPAASPAA	0x41	
#define CS42L73_XSPAASPBA	0x42	
#define CS42L73_XSPAVSPMA	0x43	
#define CS42L73_XSPBVSPMA	0x44	
#define CS42L73_ASPAIPAA	0x45	
#define CS42L73_ASPBIPBA	0x46	
#define CS42L73_ASPAXSPAA	0x47	
#define CS42L73_ASPBXSPBA	0x48	
#define CS42L73_ASPAASPAA	0x49	
#define CS42L73_ASPBASPBA	0x4A	
#define CS42L73_ASPAVSPMA	0x4B	
#define CS42L73_ASPBVSPMA	0x4C	
#define CS42L73_VSPAIPAA	0x4D	
#define CS42L73_VSPBIPBA	0x4E	
#define CS42L73_VSPAXSPAA	0x4F	
#define CS42L73_VSPBXSPBA	0x50	
#define CS42L73_VSPAASPAA	0x51	
#define CS42L73_VSPBASPBA	0x52	
#define CS42L73_VSPAVSPMA	0x53	
#define CS42L73_VSPBVSPMA	0x54	
#define CS42L73_MMIXCTL		0x55	
#define CS42L73_SPKMIPMA	0x56	
#define CS42L73_SPKMXSPA	0x57	
#define CS42L73_SPKMASPA	0x58	
#define CS42L73_SPKMVSPMA	0x59	
#define CS42L73_ESLMIPMA	0x5A	
#define CS42L73_ESLMXSPA	0x5B	
#define CS42L73_ESLMASPA	0x5C	
#define CS42L73_ESLMVSPMA	0x5D	
#define CS42L73_IM1		0x5E	
#define CS42L73_IM2		0x5F	
#define CS42L73_IS1		0x60	
#define CS42L73_IS2		0x61	
#define CS42L73_MAX_REGISTER	0x61	

#define PDN_ADCB		(1 << 7)
#define PDN_DMICB		(1 << 6)
#define PDN_ADCA		(1 << 5)
#define PDN_DMICA		(1 << 4)
#define PDN_LDO			(1 << 2)
#define DISCHG_FILT		(1 << 1)
#define PDN			(1 << 0)

#define PDN_MIC2_BIAS		(1 << 7)
#define PDN_MIC1_BIAS		(1 << 6)
#define PDN_VSP			(1 << 4)
#define PDN_ASP_SDOUT		(1 << 3)
#define PDN_ASP_SDIN		(1 << 2)
#define PDN_XSP_SDOUT		(1 << 1)
#define PDN_XSP_SDIN		(1 << 0)

#define PDN_THMS		(1 << 5)
#define PDN_SPKLO		(1 << 4)
#define PDN_EAR			(1 << 3)
#define PDN_SPK			(1 << 2)
#define PDN_LO			(1 << 1)
#define PDN_HP			(1 << 0)

#define THMOVLD_150C		0
#define THMOVLD_132C		1
#define THMOVLD_115C		2
#define THMOVLD_098C		3


#define	SP_3ST			(1 << 7)
#define SPDIF_I2S		(0 << 6)
#define SPDIF_PCM		(1 << 6)
#define PCM_MODE0		(0 << 4)
#define PCM_MODE1		(1 << 4)
#define PCM_MODE2		(2 << 4)
#define PCM_MODE_MASK		(3 << 4)
#define PCM_BIT_ORDER		(1 << 3)
#define MCK_SCLK_64FS		(0 << 0)
#define MCK_SCLK_MCLK		(2 << 0)
#define MCK_SCLK_PREMCLK	(3 << 0)

#define MS_MASTER		(1 << 7)


#define MCLKDIS			(1 << 0)
#define MCLKSEL_MCLK2		(1 << 4)
#define MCLKSEL_MCLK1		(0 << 4)

#define CS42L73_CLKID_MCLK1     0
#define CS42L73_CLKID_MCLK2     1

#define CS42L73_MCLKXDIV	0
#define CS42L73_MMCCDIV         1

#define CS42L73_XSP		0
#define CS42L73_ASP		1
#define CS42L73_VSP		2

#define MIC2_SDET		(1 << 6)
#define THMOVLD			(1 << 4)
#define DIGMIXOVFL		(1 << 3)
#define IPBOVFL			(1 << 1)
#define IPAOVFL			(1 << 0)

#define ANLGOSFT		(1 << 0)

#define HPA_MUTE		(1 << 7)
#define LOA_MUTE		(1 << 7)
#define HLAD_MUTE		(1 << 0)
#define HLBD_MUTE		(1 << 1)
#define SPKD_MUTE		(1 << 2)
#define ESLD_MUTE		(1 << 3)

#define CS42L73_RESET_GPIO 143

#define CS42L73_DEVID		0x00042A73
#define CS42L73_MCLKX_MIN	5644800
#define CS42L73_MCLKX_MAX	38400000

#define CS42L73_SPC(id)		(CS42L73_XSPC + (id << 1))
#define CS42L73_MMCC(id)	(CS42L73_XSPMMCC + (id << 1))
#define CS42L73_SPFS(id)	((id == CS42L73_ASP) ? CS42L73_ASPC : CS42L73_VXSPFS)

#endif	
