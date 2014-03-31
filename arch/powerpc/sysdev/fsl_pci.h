/*
 * MPC85xx/86xx PCI Express structure define
 *
 * Copyright 2007,2011 Freescale Semiconductor, Inc
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */

#ifdef __KERNEL__
#ifndef __POWERPC_FSL_PCI_H
#define __POWERPC_FSL_PCI_H

#define PCIE_LTSSM	0x0404		
#define PCIE_LTSSM_L0	0x16		
#define PIWAR_EN		0x80000000	
#define PIWAR_PF		0x20000000	
#define PIWAR_TGI_LOCAL		0x00f00000	
#define PIWAR_READ_SNOOP	0x00050000
#define PIWAR_WRITE_SNOOP	0x00005000
#define PIWAR_SZ_MASK          0x0000003f

struct pci_outbound_window_regs {
	__be32	potar;	
	__be32	potear;	
	__be32	powbar;	
	u8	res1[4];
	__be32	powar;	
	u8	res2[12];
};

struct pci_inbound_window_regs {
	__be32	pitar;	
	u8	res1[4];
	__be32	piwbar;	
	__be32	piwbear;	
	__be32	piwar;	
	u8	res2[12];
};

struct ccsr_pci {
	__be32	config_addr;		
	__be32	config_data;		
	__be32	int_ack;		
	__be32	pex_otb_cpl_tor;	
	__be32	pex_conf_tor;		
	__be32	pex_config;		
	__be32	pex_int_status;		
	u8	res2[4];
	__be32	pex_pme_mes_dr;		
	__be32	pex_pme_mes_disr;	
	__be32	pex_pme_mes_ier;	
	__be32	pex_pmcr;		
	u8	res3[3024];

	struct pci_outbound_window_regs pow[5];
	u8	res14[96];
	struct pci_inbound_window_regs	pmit;	
	u8	res6[96];
	struct pci_inbound_window_regs piw[4];

	__be32	pex_err_dr;		
	u8	res21[4];
	__be32	pex_err_en;		
	u8	res22[4];
	__be32	pex_err_disr;		
	u8	res23[12];
	__be32	pex_err_cap_stat;	
	u8	res24[4];
	__be32	pex_err_cap_r0;		
	__be32	pex_err_cap_r1;		
	__be32	pex_err_cap_r2;		
	__be32	pex_err_cap_r3;		
};

extern int fsl_add_bridge(struct device_node *dev, int is_primary);
extern void fsl_pcibios_fixup_bus(struct pci_bus *bus);
extern int mpc83xx_add_bridge(struct device_node *dev);
u64 fsl_pci_immrbar_base(struct pci_controller *hose);

#endif 
#endif 
