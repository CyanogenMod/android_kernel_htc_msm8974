/*
 * pc300.h	Cyclades-PC300(tm) Kernel API Definitions.
 *
 * Author:	Ivan Passos <ivan@cyclades.com>
 *
 * Copyright:	(c) 1999-2002 Cyclades Corp.
 *
 *	This program is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License
 *	as published by the Free Software Foundation; either version
 *	2 of the License, or (at your option) any later version.
 *
 * $Log: pc300.h,v $
 * Revision 3.12  2002/03/07 14:17:09  henrique
 * License data fixed
 *
 * Revision 3.11  2002/01/28 21:09:39  daniela
 * Included ';' after pc300hw.bus.
 *
 * Revision 3.10  2002/01/17 17:58:52  ivan
 * Support for PC300-TE/M (PMC).
 *
 * Revision 3.9  2001/09/28 13:30:53  daniela
 * Renamed dma_start routine to rx_dma_start.
 *
 * Revision 3.8  2001/09/24 13:03:45  daniela
 * Fixed BOF interrupt treatment. Created dma_start routine.
 *
 * Revision 3.7  2001/08/10 17:19:58  daniela
 * Fixed IOCTLs defines.
 *
 * Revision 3.6  2001/07/18 19:24:42  daniela
 * Included kernel version.
 *
 * Revision 3.5  2001/07/05 18:38:08  daniela
 * DMA transmission bug fix.
 *
 * Revision 3.4  2001/06/26 17:10:40  daniela
 * New configuration parameters (line code, CRC calculation and clock).
 *
 * Revision 3.3  2001/06/22 13:13:02  regina
 * MLPPP implementation
 *
 * Revision 3.2  2001/06/18 17:56:09  daniela
 * Increased DEF_MTU and TX_QUEUE_LEN.
 *
 * Revision 3.1  2001/06/15 12:41:10  regina
 * upping major version number
 *
 * Revision 1.1.1.1  2001/06/13 20:25:06  daniela
 * PC300 initial CVS version (3.4.0-pre1)
 *
 * Revision 2.3 2001/03/05 daniela
 * Created struct pc300conf, to provide the hardware information to pc300util.
 * Inclusion of 'alloc_ramsize' field on structure 'pc300hw'.
 * 
 * Revision 2.2 2000/12/22 daniela
 * Structures and defines to support pc300util: statistics, status, 
 * loopback tests, trace.
 * 
 * Revision 2.1 2000/09/28 ivan
 * Inclusion of 'iophys' and 'iosize' fields on structure 'pc300hw', to 
 * allow release of I/O region at module unload.
 * Changed location of include files.
 *
 * Revision 2.0 2000/03/27 ivan
 * Added support for the PC300/TE cards.
 *
 * Revision 1.1 2000/01/31 ivan
 * Replaced 'pc300[drv|sca].h' former PC300 driver include files.
 *
 * Revision 1.0 1999/12/16 ivan
 * First official release.
 * Inclusion of 'nchan' field on structure 'pc300hw', to allow variable 
 * number of ports per card.
 * Inclusion of 'if_ptr' field on structure 'pc300dev'.
 *
 * Revision 0.6 1999/11/17 ivan
 * Changed X.25-specific function names to comply with adopted convention.
 *
 * Revision 0.5 1999/11/16 Daniela Squassoni
 * X.25 support.
 *
 * Revision 0.4 1999/11/15 ivan
 * Inclusion of 'clock' field on structure 'pc300hw'.
 *
 * Revision 0.3 1999/11/10 ivan
 * IOCTL name changing.
 * Inclusion of driver function prototypes.
 *
 * Revision 0.2 1999/11/03 ivan
 * Inclusion of 'tx_skb' and union 'ifu' on structure 'pc300dev'.
 *
 * Revision 0.1 1999/01/15 ivan
 * Initial version.
 *
 */

#ifndef	_PC300_H
#define	_PC300_H

#include <linux/hdlc.h>
#include "hd64572.h"
#include "pc300-falc-lh.h"

#define PC300_PROTO_MLPPP 1

#define	PC300_MAXCHAN	2	

#define	PC300_RAMSIZE	0x40000 
#define	PC300_FALCSIZE	0x400	

#define PC300_OSC_CLOCK	24576000
#define PC300_PCI_CLOCK	33000000

#define BD_DEF_LEN	0x0800	
#define DMA_TX_MEMSZ	0x8000	
#define DMA_RX_MEMSZ	0x10000	

#define N_DMA_TX_BUF	(DMA_TX_MEMSZ / BD_DEF_LEN)	
#define N_DMA_RX_BUF	(DMA_RX_MEMSZ / BD_DEF_LEN)	

#define DMA_TX_BASE	((N_DMA_TX_BUF + N_DMA_RX_BUF) *	\
			 PC300_MAXCHAN * sizeof(pcsca_bd_t))
#define DMA_RX_BASE	(DMA_TX_BASE + PC300_MAXCHAN*DMA_TX_MEMSZ)

#define DMA_TX_BD_BASE	0x0000
#define DMA_RX_BD_BASE	(DMA_TX_BD_BASE + ((PC300_MAXCHAN*DMA_TX_MEMSZ / \
				BD_DEF_LEN) * sizeof(pcsca_bd_t)))

#define TX_BD_ADDR(chan, n)	(DMA_TX_BD_BASE + \
				 ((N_DMA_TX_BUF*chan) + n) * sizeof(pcsca_bd_t))
#define RX_BD_ADDR(chan, n)	(DMA_RX_BD_BASE + \
				 ((N_DMA_RX_BUF*chan) + n) * sizeof(pcsca_bd_t))

#define F_REG(reg, chan)	(0x200*(chan) + ((reg)<<2))

#define cpc_writeb(port,val)	{writeb((u8)(val),(port)); mb();}
#define cpc_writew(port,val)	{writew((ushort)(val),(port)); mb();}
#define cpc_writel(port,val)	{writel((u32)(val),(port)); mb();}

#define cpc_readb(port)		readb(port)
#define cpc_readw(port)		readw(port)
#define cpc_readl(port)		readl(port)


struct RUNTIME_9050 {
	u32 loc_addr_range[4];	
	u32 loc_rom_range;	
	u32 loc_addr_base[4];	
	u32 loc_rom_base;	
	u32 loc_bus_descr[4];	
	u32 rom_bus_descr;	
	u32 cs_base[4];		
	u32 intr_ctrl_stat;	
	u32 init_ctrl;		
};

#define PLX_9050_LINT1_ENABLE	0x01
#define PLX_9050_LINT1_POL	0x02
#define PLX_9050_LINT1_STATUS	0x04
#define PLX_9050_LINT2_ENABLE	0x08
#define PLX_9050_LINT2_POL	0x10
#define PLX_9050_LINT2_STATUS	0x20
#define PLX_9050_INTR_ENABLE	0x40
#define PLX_9050_SW_INTR	0x80

#define	PC300_CLKSEL_MASK		(0x00000004UL)
#define	PC300_CHMEDIA_MASK(chan)	(0x00000020UL<<(chan*3))
#define	PC300_CTYPE_MASK		(0x00000800UL)

#define CPLD_REG1	0x140	
#define CPLD_REG2	0x144	
#define CPLD_V2_REG1	0x100	
#define CPLD_V2_REG2	0x104	
#define CPLD_ID_REG	0x108	

#define CPLD_REG1_FALC_RESET	0x01
#define CPLD_REG1_SCA_RESET	0x02
#define CPLD_REG1_GLOBAL_CLK	0x08
#define CPLD_REG1_FALC_DCD	0x10
#define CPLD_REG1_FALC_CTS	0x20

#define CPLD_REG2_FALC_TX_CLK	0x01
#define CPLD_REG2_FALC_RX_CLK	0x02
#define CPLD_REG2_FALC_LED1	0x10
#define CPLD_REG2_FALC_LED2	0x20

#define PC300_FALC_MAXLOOP	0x0000ffff	

typedef struct falc {
	u8 sync;	
	u8 active;	
	u8 loop_active;	
	u8 loop_gen;	

	u8 num_channels;
	u8 offset;	
	u8 full_bandwidth;

	u8 xmb_cause;
	u8 multiframe_mode;

	
	u16 pden;	
	u16 los;	
	u16 losr;	
	u16 lfa;	
	u16 farec;	
	u16 lmfa;	
	u16 ais;	
	u16 sec;	
	u16 es;		
	u16 rai;	
	u16 bec;
	u16 fec;
	u16 cvc;
	u16 cec;
	u16 ebc;

	
	u8 red_alarm;
	u8 blue_alarm;
	u8 loss_fa;
	u8 yellow_alarm;
	u8 loss_mfa;
	u8 prbs;
} falc_t;

typedef struct falc_status {
	u8 sync;	
	u8 red_alarm;
	u8 blue_alarm;
	u8 loss_fa;
	u8 yellow_alarm;
	u8 loss_mfa;
	u8 prbs;
} falc_status_t;

typedef struct rsv_x21_status {
	u8 dcd;
	u8 dsr;
	u8 cts;
	u8 rts;
	u8 dtr;
} rsv_x21_status_t;

typedef struct pc300stats {
	int hw_type;
	u32 line_on;
	u32 line_off;
	struct net_device_stats gen_stats;
	falc_t te_stats;
} pc300stats_t;

typedef struct pc300status {
	int hw_type;
	rsv_x21_status_t gen_status;
	falc_status_t te_status;
} pc300status_t;

typedef struct pc300loopback {
	char loop_type;
	char loop_on;
} pc300loopback_t;

typedef struct pc300patterntst {
	char patrntst_on;       
	u16 num_errors;
} pc300patterntst_t;

typedef struct pc300dev {
	struct pc300ch *chan;
	u8 trace_on;
	u32 line_on;		
	u32 line_off;
	char name[16];
	struct net_device *dev;
#ifdef CONFIG_PC300_MLPPP
	void *cpc_tty;	
#endif
}pc300dev_t;

typedef struct pc300hw {
	int type;		
	int bus;		
	int nchan;		
	int irq;		
	u32 clock;		
	u8 cpld_id;		
	u16 cpld_reg1;		
	u16 cpld_reg2;		
	u16 gpioc_reg;		
	u16 intctl_reg;		
	u32 iophys;		
	u32 iosize;		
	u32 plxphys;		
	void __iomem * plxbase;	
	u32 plxsize;		
	u32 scaphys;		
	void __iomem * scabase;	
	u32 scasize;		
	u32 ramphys;		
	void __iomem * rambase;	
	u32 alloc_ramsize;	
	u32 ramsize;		
	u32 falcphys;		
	void __iomem * falcbase;
	u32 falcsize;		
} pc300hw_t;

typedef struct pc300chconf {
	sync_serial_settings	phys_settings;	
	raw_hdlc_proto		proto_settings;	
	u32 media;		
	u32 proto;		

	
	u8 lcode;		
	u8 fr_mode;		
	u8 lbo;			
	u8 rx_sens;		
	u32 tslot_bitmap;	
} pc300chconf_t;

typedef struct pc300ch {
	struct pc300 *card;
	int channel;
	pc300dev_t d;
	pc300chconf_t conf;
	u8 tx_first_bd;	
	u8 tx_next_bd;	
	u8 rx_first_bd;	
	u8 rx_last_bd;	
	u8 nfree_tx_bd;	
	falc_t falc;	
} pc300ch_t;

typedef struct pc300 {
	pc300hw_t hw;			
	pc300ch_t chan[PC300_MAXCHAN];
	spinlock_t card_lock;
} pc300_t;

typedef struct pc300conf {
	pc300hw_t hw;
	pc300chconf_t conf;
} pc300conf_t;

#define	N_SPPP_IOCTLS	2

enum pc300_ioctl_cmds {
	SIOCCPCRESERVED = (SIOCDEVPRIVATE + N_SPPP_IOCTLS),
	SIOCGPC300CONF,
	SIOCSPC300CONF,
	SIOCGPC300STATUS,
	SIOCGPC300FALCSTATUS,
	SIOCGPC300UTILSTATS,
	SIOCGPC300UTILSTATUS,
	SIOCSPC300TRACE,
	SIOCSPC300LOOPBACK,
	SIOCSPC300PATTERNTEST,
};

enum pc300_loopback_cmds {
	PC300LOCLOOP = 1,
	PC300REMLOOP,
	PC300PAYLOADLOOP,
	PC300GENLOOPUP,
	PC300GENLOOPDOWN,
};

#define	PC300_RSV	0x01
#define	PC300_X21	0x02
#define	PC300_TE	0x03

#define	PC300_PCI	0x00
#define	PC300_PMC	0x01

#define PC300_LC_AMI	0x01
#define PC300_LC_B8ZS	0x02
#define PC300_LC_NRZ	0x03
#define PC300_LC_HDB3	0x04

#define PC300_FR_ESF		0x01
#define PC300_FR_D4		0x02
#define PC300_FR_ESF_JAPAN	0x03

#define PC300_FR_MF_CRC4	0x04
#define PC300_FR_MF_NON_CRC4	0x05
#define PC300_FR_UNFRAMED	0x06

#define PC300_LBO_0_DB		0x00
#define PC300_LBO_7_5_DB	0x01
#define PC300_LBO_15_DB		0x02
#define PC300_LBO_22_5_DB	0x03

#define PC300_RX_SENS_SH	0x01
#define PC300_RX_SENS_LH	0x02

#define PC300_TX_TIMEOUT	(2*HZ)
#define PC300_TX_QUEUE_LEN	100
#define	PC300_DEF_MTU		1600

int cpc_open(struct net_device *dev);

#endif	
