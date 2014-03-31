/*
 * bbc.h: Defines for BootBus Controller found on UltraSPARC-III
 *        systems.
 *
 * Copyright (C) 2000 David S. Miller (davem@redhat.com)
 */

#ifndef _SPARC64_BBC_H
#define _SPARC64_BBC_H


#define BBC_AID		0x00	
#define BBC_DEVP	0x01	
#define BBC_ARB		0x02	
#define BBC_QUIESCE	0x03	
#define BBC_WDACTION	0x04	
#define BBC_SPG		0x06	
#define BBC_SXG		0x07	
#define BBC_PSRC	0x08	
#define BBC_XSRC	0x0c	
#define BBC_CSC		0x0d	
#define BBC_ES_CTRL	0x0e	
#define BBC_ES_ACT	0x10	
#define BBC_ES_DACT	0x14	
#define BBC_ES_DABT	0x15	
#define BBC_ES_ABT	0x16	
#define BBC_ES_PST	0x18	
#define BBC_ES_FSL	0x1c	
#define BBC_EBUST	0x20	
#define BBC_JTAG_CMD	0x28	
#define BBC_JTAG_CTRL	0x2c	
#define BBC_I2C_SEL	0x2d	
#define BBC_I2C_0_S1	0x2e	
#define BBC_I2C_0_S0	0x2f	
#define BBC_I2C_1_S1	0x30	
#define BBC_I2C_1_S0	0x31	
#define BBC_KBD_BEEP	0x32	
#define BBC_KBD_BCNT	0x34	

#define BBC_REGS_SIZE	0x40


#define BBC_AID_ID	0x07	
#define BBC_AID_RESV	0xf8	

#define BBC_DEVP_CPU0	0x01	
#define BBC_DEVP_CPU1	0x02	
#define BBC_DEVP_CPU2	0x04	
#define BBC_DEVP_CPU3	0x08	
#define BBC_DEVP_RESV	0xf0	

#define BBC_ARB_CPU0	0x01	
#define BBC_ARB_CPU1	0x02	
#define BBC_ARB_CPU2	0x04	
#define BBC_ARB_CPU3	0x08	
#define BBC_ARB_RESV	0xf0	

#define BBC_QUIESCE_S02	0x01	
#define BBC_QUIESCE_S13	0x02	
#define BBC_QUIESCE_B02	0x04	
#define BBC_QUIESCE_B13	0x08	
#define BBC_QUIESCE_FD0 0x10	
#define BBC_QUIESCE_FD1 0x20	
#define BBC_QUIESCE_FD2 0x40	
#define BBC_QUIESCE_FD3 0x80	

#define BBC_WDACTION_RST  0x01	
#define BBC_WDACTION_RESV 0xfe	

#define BBC_SPG_CPU0	0x01 
#define BBC_SPG_CPU1	0x02 
#define BBC_SPG_CPU2	0x04 
#define BBC_SPG_CPU3	0x08 
#define BBC_SPG_CPUALL	0x10 
#define BBC_SPG_RESV	0xe0 

#define BBC_SXG_CPU0	0x01 
#define BBC_SXG_CPU1	0x02 
#define BBC_SXG_CPU2	0x04 
#define BBC_SXG_CPU3	0x08 
#define BBC_SXG_RESV	0xf0 

#define BBC_PSRC_SPG0	0x0001 
#define BBC_PSRC_SPG1	0x0002 
#define BBC_PSRC_SPG2	0x0004 
#define BBC_PSRC_SPG3	0x0008 
#define BBC_PSRC_SPGSYS	0x0010 
#define BBC_PSRC_JTAG	0x0020 
#define BBC_PSRC_BUTTON	0x0040 
#define BBC_PSRC_PWRUP	0x0080 
#define BBC_PSRC_FE0	0x0100 
#define BBC_PSRC_FE1	0x0200 
#define BBC_PSRC_FE2	0x0400 
#define BBC_PSRC_FE3	0x0800 
#define BBC_PSRC_FE4	0x1000 
#define BBC_PSRC_FE5	0x2000 
#define BBC_PSRC_FE6	0x4000 
#define BBC_PSRC_SYNTH	0x8000 
#define BBC_PSRC_WDT   0x10000 
#define BBC_PSRC_RSC   0x20000 

#define BBC_XSRC_SXG0	0x01	
#define BBC_XSRC_SXG1	0x02	
#define BBC_XSRC_SXG2	0x04	
#define BBC_XSRC_SXG3	0x08	
#define BBC_XSRC_JTAG	0x10	
#define BBC_XSRC_W_OR_B	0x20	
#define BBC_XSRC_RESV	0xc0	

#define BBC_CSC_SLOAD	0x01	
#define BBC_CSC_SDATA	0x02	
#define BBC_CSC_SCLOCK	0x04	
#define BBC_CSC_RESV	0x78	
#define BBC_CSC_RST	0x80	

#define BBC_ES_CTRL_1_1		0x01	
#define BBC_ES_CTRL_1_2		0x02	
#define BBC_ES_CTRL_1_32	0x20	
#define BBC_ES_RESV		0xdc	

#define BBC_ES_ACT_VAL	0xff

#define BBC_ES_ABT_VAL	0xffff

#define BBC_ES_PST_VAL	0xffffffff

#define BBC_ES_FSL_VAL	0xffffffff

#define BBC_KBD_BEEP_ENABLE	0x01 
#define BBC_KBD_BEEP_RESV	0xfe 

#define BBC_KBD_BCNT_BITS	0x0007fc00
#define BBC_KBC_BCNT_RESV	0xfff803ff

#endif 

