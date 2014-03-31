
/*
 * (C) Copyright 2000, 2001
 * Wolfgang Denk, DENX Software Engineering, wd@denx.de.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#ifndef __PPCBOOT_H__
#define __PPCBOOT_H__


#include "types.h"

typedef struct bd_info {
	unsigned long	bi_memstart;	
	unsigned long	bi_memsize;	
	unsigned long	bi_flashstart;	
	unsigned long	bi_flashsize;	
	unsigned long	bi_flashoffset; 
	unsigned long	bi_sramstart;	
	unsigned long	bi_sramsize;	
#if defined(TARGET_8xx) || defined(TARGET_CPM2) || defined(TARGET_85xx) ||\
	defined(TARGET_83xx)
	unsigned long	bi_immr_base;	
#endif
#if defined(TARGET_PPC_MPC52xx)
	unsigned long   bi_mbar_base;   
#endif
	unsigned long	bi_bootflags;	
	unsigned long	bi_ip_addr;	
	unsigned char	bi_enetaddr[6];	
	unsigned short	bi_ethspeed;	
	unsigned long	bi_intfreq;	
	unsigned long	bi_busfreq;	
#if defined(TARGET_CPM2)
	unsigned long	bi_cpmfreq;	
	unsigned long	bi_brgfreq;	
	unsigned long	bi_sccfreq;	
	unsigned long	bi_vco;		
#endif
#if defined(TARGET_PPC_MPC52xx)
	unsigned long   bi_ipbfreq;     
	unsigned long   bi_pcifreq;     
#endif
	unsigned long	bi_baudrate;	
#if defined(TARGET_4xx)
	unsigned char	bi_s_version[4];	
	unsigned char	bi_r_version[32];	
	unsigned int	bi_procfreq;	
	unsigned int	bi_plb_busfreq;	
	unsigned int	bi_pci_busfreq;	
	unsigned char	bi_pci_enetaddr[6];	
#endif
#if defined(TARGET_HYMOD)
	hymod_conf_t	bi_hymod_conf;	
#endif
#if defined(TARGET_EVB64260) || defined(TARGET_405EP) || defined(TARGET_44x) || \
	defined(TARGET_85xx) ||	defined(TARGET_83xx) || defined(TARGET_HAS_ETH1)
	
	unsigned char	bi_enet1addr[6];
#define HAVE_ENET1ADDR
#endif
#if defined(TARGET_EVB64260) || defined(TARGET_440GX) || \
    defined(TARGET_85xx) || defined(TARGET_HAS_ETH2)
	
	unsigned char	bi_enet2addr[6];
#define HAVE_ENET2ADDR
#endif
#if defined(TARGET_440GX) || defined(TARGET_HAS_ETH3)
	
	unsigned char	bi_enet3addr[6];
#define HAVE_ENET3ADDR
#endif
#if defined(TARGET_4xx)
	unsigned int	bi_opbfreq;		
	int		bi_iic_fast[2];		
#endif
#if defined(TARGET_440GX)
	int		bi_phynum[4];		
	int		bi_phymode[4];		
#endif
} bd_t;

#define bi_tbfreq	bi_intfreq

#endif	
