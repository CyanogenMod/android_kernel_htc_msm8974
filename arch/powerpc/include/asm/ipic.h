/*
 * IPIC external definitions and structure.
 *
 * Maintainer: Kumar Gala <galak@kernel.crashing.org>
 *
 * Copyright 2005 Freescale Semiconductor, Inc
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */
#ifdef __KERNEL__
#ifndef __ASM_IPIC_H__
#define __ASM_IPIC_H__

#include <linux/irq.h>

#define IPIC_SPREADMODE_GRP_A	0x00000001
#define IPIC_SPREADMODE_GRP_B	0x00000002
#define IPIC_SPREADMODE_GRP_C	0x00000004
#define IPIC_SPREADMODE_GRP_D	0x00000008
#define IPIC_SPREADMODE_MIX_A	0x00000010
#define IPIC_SPREADMODE_MIX_B	0x00000020
#define IPIC_DISABLE_MCP_OUT	0x00000040
#define IPIC_IRQ0_MCP		0x00000080

#define IPIC_SICFR	0x00	
#define IPIC_SIVCR	0x04	
#define IPIC_SIPNR_H	0x08	
#define IPIC_SIPNR_L	0x0C	
#define IPIC_SIPRR_A	0x10	
#define IPIC_SIPRR_B	0x14	
#define IPIC_SIPRR_C	0x18	
#define IPIC_SIPRR_D	0x1C	
#define IPIC_SIMSR_H	0x20	
#define IPIC_SIMSR_L	0x24	
#define IPIC_SICNR	0x28	
#define IPIC_SEPNR	0x2C	
#define IPIC_SMPRR_A	0x30	
#define IPIC_SMPRR_B	0x34	
#define IPIC_SEMSR	0x38	
#define IPIC_SECNR	0x3C	
#define IPIC_SERSR	0x40	
#define IPIC_SERMR	0x44	
#define IPIC_SERCR	0x48	
#define IPIC_SIFCR_H	0x50	
#define IPIC_SIFCR_L	0x54	
#define IPIC_SEFCR	0x58	
#define IPIC_SERFR	0x5C	
#define IPIC_SCVCR	0x60	
#define IPIC_SMVCR	0x64	

enum ipic_prio_grp {
	IPIC_INT_GRP_A = IPIC_SIPRR_A,
	IPIC_INT_GRP_D = IPIC_SIPRR_D,
	IPIC_MIX_GRP_A = IPIC_SMPRR_A,
	IPIC_MIX_GRP_B = IPIC_SMPRR_B,
};

enum ipic_mcp_irq {
	IPIC_MCP_IRQ0 = 0,
	IPIC_MCP_WDT  = 1,
	IPIC_MCP_SBA  = 2,
	IPIC_MCP_PCI1 = 5,
	IPIC_MCP_PCI2 = 6,
	IPIC_MCP_MU   = 7,
};

extern int ipic_set_priority(unsigned int irq, unsigned int priority);
extern void ipic_set_highest_priority(unsigned int irq);
extern void ipic_set_default_priority(void);
extern void ipic_enable_mcp(enum ipic_mcp_irq mcp_irq);
extern void ipic_disable_mcp(enum ipic_mcp_irq mcp_irq);
extern u32 ipic_get_mcp_status(void);
extern void ipic_clear_mcp_status(u32 mask);

extern struct ipic * ipic_init(struct device_node *node, unsigned int flags);
extern unsigned int ipic_get_irq(void);

#endif 
#endif 
