/*
 * Freescale PowerQUICC MDIO Driver -- MII Management Bus Implementation
 * Driver for the MDIO bus controller on Freescale PowerQUICC processors
 *
 * Author: Andy Fleming
 * Modifier: Sandeep Gopalpet
 *
 * Copyright 2002-2004, 2008-2009 Freescale Semiconductor, Inc.
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */
#ifndef __FSL_PQ_MDIO_H
#define __FSL_PQ_MDIO_H

#define MIIMIND_BUSY            0x00000001
#define MIIMIND_NOTVALID        0x00000004
#define MIIMCFG_INIT_VALUE	0x00000007
#define MIIMCFG_RESET           0x80000000

#define MII_READ_COMMAND       0x00000001

struct fsl_pq_mdio {
	u8 res1[16];
	u32 ieventm;	
	u32 imaskm;	
	u8 res2[4];
	u32 emapm;	
	u8 res3[1280];
	u32 miimcfg;		
	u32 miimcom;		
	u32 miimadd;		
	u32 miimcon;		
	u32 miimstat;		
	u32 miimind;		
	u8 reserved[28];	
	u32 utbipar;		
	u8 res4[2728];
} __packed;

int fsl_pq_mdio_read(struct mii_bus *bus, int mii_id, int regnum);
int fsl_pq_mdio_write(struct mii_bus *bus, int mii_id, int regnum, u16 value);
int fsl_pq_local_mdio_write(struct fsl_pq_mdio __iomem *regs, int mii_id,
			  int regnum, u16 value);
int fsl_pq_local_mdio_read(struct fsl_pq_mdio __iomem *regs, int mii_id, int regnum);
int __init fsl_pq_mdio_init(void);
void fsl_pq_mdio_exit(void);
void fsl_pq_mdio_bus_name(char *name, struct device_node *np);
#endif 
