/**
 * Copyright (C) 2008, Creative Technology Ltd. All Rights Reserved.
 *
 * This source file is released under GPL v2 license (no other versions).
 * See the COPYING file included in the main directory of this source
 * distribution for the license terms and conditions.
 */

#ifndef _20K2REGISTERS_H_
#define _20K2REGISTERS_H_


#define WC		0x1b7000
#define TIMR		0x1b7004
# define	TIMR_IE		(1<<15)
# define	TIMR_IP		(1<<14)
#define GIP		0x1b7010
#define GIE		0x1b7014

#define I2C_IF_ADDRESS   0x1B9000
#define I2C_IF_WDATA     0x1B9004
#define I2C_IF_RDATA     0x1B9008
#define I2C_IF_STATUS    0x1B900C
#define I2C_IF_WLOCK     0x1B9010

#define GLOBAL_CNTL_GCTL    0x1B7090

#define PLL_CTL 		0x1B7080
#define PLL_STAT		0x1B7084
#define PLL_ENB			0x1B7088

#define SRC_CTL             0x1A0000 
#define SRC_CCR             0x1A0004 
#define SRC_IMAP            0x1A0008 
#define SRC_CA              0x1A0010 
#define SRC_CF              0x1A0014 
#define SRC_SA              0x1A0018 
#define SRC_LA              0x1A001C 
#define SRC_CTLSWR	    0x1A0020 
#define SRC_CD		    0x1A0080 
#define SRC_MCTL		0x1A012C
#define SRC_IP			0x1A102C 
#define SRC_ENB			0x1A282C 
#define SRC_ENBSTAT		0x1A202C
#define SRC_ENBSA		0x1A232C
#define SRC_DN0Z		0x1A0030
#define SRC_DN1Z		0x1A0040
#define SRC_UPZ			0x1A0060

#define GPIO_DATA           0x1B7020
#define GPIO_CTRL           0x1B7024
#define GPIO_EXT_DATA       0x1B70A0

#define VMEM_PTPAL          0x1C6300 
#define VMEM_PTPAH          0x1C6304 
#define VMEM_CTL            0x1C7000

#define TRANSPORT_ENB       0x1B6000
#define TRANSPORT_CTL       0x1B6004
#define TRANSPORT_INT       0x1B6008

#define AUDIO_IO_AIM        0x1B5000 
#define AUDIO_IO_TX_CTL     0x1B5400 
#define AUDIO_IO_TX_CSTAT_L 0x1B5408 
#define AUDIO_IO_TX_CSTAT_H 0x1B540C 
#define AUDIO_IO_RX_CTL     0x1B5410 
#define AUDIO_IO_RX_SRT_CTL 0x1B5420 
#define AUDIO_IO_MCLK       0x1B5600
#define AUDIO_IO_TX_BLRCLK  0x1B5604
#define AUDIO_IO_RX_BLRCLK  0x1B5608

#define MIXER_AMOPLO		0x130000 
#define MIXER_AMOPHI		0x130004 
#define MIXER_PRING_LO_HI	0x188000 
#define MIXER_PMOPLO		0x138000 
#define MIXER_PMOPHI		0x138004 
#define MIXER_AR_ENABLE		0x19000C

#endif
