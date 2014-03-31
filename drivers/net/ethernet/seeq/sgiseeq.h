/*
 * sgiseeq.h: Defines for the Seeq8003 ethernet controller.
 *
 * Copyright (C) 1996 David S. Miller (davem@davemloft.net)
 */
#ifndef _SGISEEQ_H
#define _SGISEEQ_H

struct sgiseeq_wregs {
	volatile unsigned int multicase_high[2];
	volatile unsigned int frame_gap;
	volatile unsigned int control;
};

struct sgiseeq_rregs {
	volatile unsigned int collision_tx[2];
	volatile unsigned int collision_all[2];
	volatile unsigned int _unused0;
	volatile unsigned int rflags;
};

struct sgiseeq_regs {
	union {
		volatile unsigned int eth_addr[6];
		volatile unsigned int multicast_low[6];
		struct sgiseeq_wregs wregs;
		struct sgiseeq_rregs rregs;
	} rw;
	volatile unsigned int rstat;
	volatile unsigned int tstat;
};

#define SEEQ_RSTAT_OVERF   0x001 
#define SEEQ_RSTAT_CERROR  0x002 
#define SEEQ_RSTAT_DERROR  0x004 
#define SEEQ_RSTAT_SFRAME  0x008 
#define SEEQ_RSTAT_REOF    0x010 
#define SEEQ_RSTAT_FIG     0x020 
#define SEEQ_RSTAT_TIMEO   0x040 
#define SEEQ_RSTAT_WHICH   0x080 
#define SEEQ_RSTAT_LITTLE  0x100 
#define SEEQ_RSTAT_SDMA    0x200 
#define SEEQ_RSTAT_ADMA    0x400 
#define SEEQ_RSTAT_ROVERF  0x800 

#define SEEQ_RCMD_RDISAB   0x000 
#define SEEQ_RCMD_IOVERF   0x001 
#define SEEQ_RCMD_ICRC     0x002 
#define SEEQ_RCMD_IDRIB    0x004 
#define SEEQ_RCMD_ISHORT   0x008 
#define SEEQ_RCMD_IEOF     0x010 
#define SEEQ_RCMD_IGOOD    0x020 
#define SEEQ_RCMD_RANY     0x040 
#define SEEQ_RCMD_RBCAST   0x080 
#define SEEQ_RCMD_RBMCAST  0x0c0 

#define SEEQ_TSTAT_UFLOW   0x001 
#define SEEQ_TSTAT_CLS     0x002 
#define SEEQ_TSTAT_R16     0x004 
#define SEEQ_TSTAT_PTRANS  0x008 
#define SEEQ_TSTAT_LCLS    0x010 
#define SEEQ_TSTAT_WHICH   0x080 
#define SEEQ_TSTAT_TLE     0x100 
#define SEEQ_TSTAT_SDMA    0x200 
#define SEEQ_TSTAT_ADMA    0x400 

#define SEEQ_TCMD_RB0      0x00 
#define SEEQ_TCMD_IUF      0x01 
#define SEEQ_TCMD_IC       0x02 
#define SEEQ_TCMD_I16      0x04 
#define SEEQ_TCMD_IPT      0x08 
#define SEEQ_TCMD_RB1      0x20 
#define SEEQ_TCMD_RB2      0x40 

#define SEEQ_CTRL_XCNT     0x01
#define SEEQ_CTRL_ACCNT    0x02
#define SEEQ_CTRL_SFLAG    0x04
#define SEEQ_CTRL_EMULTI   0x08
#define SEEQ_CTRL_ESHORT   0x10
#define SEEQ_CTRL_ENCARR   0x20

#define SEEQ_HPIO_P1BITS  0x00000001 
#define SEEQ_HPIO_P2BITS  0x00000060 
#define SEEQ_HPIO_P3BITS  0x00000100 
#define SEEQ_HDMA_D1BITS  0x00000006 
#define SEEQ_HDMA_D2BITS  0x00000020 
#define SEEQ_HDMA_D3BITS  0x00000000 
#define SEEQ_HDMA_TIMEO   0x00030000 
#define SEEQ_HCTL_NORM    0x00000000 
#define SEEQ_HCTL_RESET   0x00000001 
#define SEEQ_HCTL_IPEND   0x00000002 
#define SEEQ_HCTL_IPG     0x00001000 
#define SEEQ_HCTL_RFIX    0x00002000 
#define SEEQ_HCTL_EFIX    0x00004000 
#define SEEQ_HCTL_IFIX    0x00008000 

#endif 
