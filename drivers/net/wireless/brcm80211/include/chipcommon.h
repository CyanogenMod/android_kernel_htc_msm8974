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

#ifndef	_SBCHIPC_H
#define	_SBCHIPC_H

#include "defs.h"		

#define CHIPCREGOFFS(field)	offsetof(struct chipcregs, field)

struct chipcregs {
	u32 chipid;		
	u32 capabilities;
	u32 corecontrol;	
	u32 bist;

	
	u32 otpstatus;	
	u32 otpcontrol;
	u32 otpprog;
	u32 otplayout;	

	
	u32 intstatus;	
	u32 intmask;

	
	u32 chipcontrol;	
	u32 chipstatus;	

	
	u32 jtagcmd;		
	u32 jtagir;
	u32 jtagdr;
	u32 jtagctrl;

	
	u32 flashcontrol;	
	u32 flashaddress;
	u32 flashdata;
	u32 PAD[1];

	
	u32 broadcastaddress;	
	u32 broadcastdata;

	
	u32 gpiopullup;	
	u32 gpiopulldown;	
	u32 gpioin;		
	u32 gpioout;		
	u32 gpioouten;	
	u32 gpiocontrol;	
	u32 gpiointpolarity;	
	u32 gpiointmask;	

	
	u32 gpioevent;
	u32 gpioeventintmask;

	
	u32 watchdog;	

	
	u32 gpioeventintpolarity;

	
	u32 gpiotimerval;	
	u32 gpiotimeroutmask;

	
	u32 clockcontrol_n;	
	u32 clockcontrol_sb;	
	u32 clockcontrol_pci;	
	u32 clockcontrol_m2;	
	u32 clockcontrol_m3;	
	u32 clkdiv;		
	u32 gpiodebugsel;	
	u32 capabilities_ext;	

	
	u32 pll_on_delay;	
	u32 fref_sel_delay;
	u32 slow_clk_ctl;	
	u32 PAD;

	
	u32 system_clk_ctl;	
	u32 clkstatestretch;
	u32 PAD[2];

	
	u32 bp_addrlow;	
	u32 bp_addrhigh;
	u32 bp_data;
	u32 PAD;
	u32 bp_indaccess;
	u32 PAD[3];

	
	u32 clkdiv2;
	u32 PAD[2];

	
	u32 eromptr;		

	
	u32 pcmcia_config;	
	u32 pcmcia_memwait;
	u32 pcmcia_attrwait;
	u32 pcmcia_iowait;
	u32 ide_config;
	u32 ide_memwait;
	u32 ide_attrwait;
	u32 ide_iowait;
	u32 prog_config;
	u32 prog_waitcount;
	u32 flash_config;
	u32 flash_waitcount;
	u32 SECI_config;	
	u32 PAD[3];

	
	u32 eci_output;	
	u32 eci_control;
	u32 eci_inputlo;
	u32 eci_inputmi;
	u32 eci_inputhi;
	u32 eci_inputintpolaritylo;
	u32 eci_inputintpolaritymi;
	u32 eci_inputintpolarityhi;
	u32 eci_intmasklo;
	u32 eci_intmaskmi;
	u32 eci_intmaskhi;
	u32 eci_eventlo;
	u32 eci_eventmi;
	u32 eci_eventhi;
	u32 eci_eventmasklo;
	u32 eci_eventmaskmi;
	u32 eci_eventmaskhi;
	u32 PAD[3];

	
	u32 sromcontrol;	
	u32 sromaddress;
	u32 sromdata;
	u32 PAD[17];

	
	u32 clk_ctl_st;	
	u32 hw_war;
	u32 PAD[70];

	
	u8 uart0data;	
	u8 uart0imr;
	u8 uart0fcr;
	u8 uart0lcr;
	u8 uart0mcr;
	u8 uart0lsr;
	u8 uart0msr;
	u8 uart0scratch;
	u8 PAD[248];		

	u8 uart1data;	
	u8 uart1imr;
	u8 uart1fcr;
	u8 uart1lcr;
	u8 uart1mcr;
	u8 uart1lsr;
	u8 uart1msr;
	u8 uart1scratch;
	u32 PAD[126];

	
	u32 pmucontrol;	
	u32 pmucapabilities;
	u32 pmustatus;
	u32 res_state;
	u32 res_pending;
	u32 pmutimer;
	u32 min_res_mask;
	u32 max_res_mask;
	u32 res_table_sel;
	u32 res_dep_mask;
	u32 res_updn_timer;
	u32 res_timer;
	u32 clkstretch;
	u32 pmuwatchdog;
	u32 gpiosel;		
	u32 gpioenable;	
	u32 res_req_timer_sel;
	u32 res_req_timer;
	u32 res_req_mask;
	u32 PAD;
	u32 chipcontrol_addr;	
	u32 chipcontrol_data;	
	u32 regcontrol_addr;
	u32 regcontrol_data;
	u32 pllcontrol_addr;
	u32 pllcontrol_data;
	u32 pmustrapopt;	
	u32 pmu_xtalfreq;	
	u32 PAD[100];
	u16 sromotp[768];
};

#define	CID_ID_MASK		0x0000ffff	
#define	CID_REV_MASK		0x000f0000	
#define	CID_REV_SHIFT		16	
#define	CID_PKG_MASK		0x00f00000	
#define	CID_PKG_SHIFT		20	
#define	CID_CC_MASK		0x0f000000	
#define CID_CC_SHIFT		24
#define	CID_TYPE_MASK		0xf0000000	
#define CID_TYPE_SHIFT		28

#define	CC_CAP_UARTS_MASK	0x00000003	
#define CC_CAP_MIPSEB		0x00000004	
#define CC_CAP_UCLKSEL		0x00000018	
#define CC_CAP_UINTCLK		0x00000008
#define CC_CAP_UARTGPIO		0x00000020	
#define CC_CAP_EXTBUS_MASK	0x000000c0	
#define CC_CAP_EXTBUS_NONE	0x00000000	
#define CC_CAP_EXTBUS_FULL	0x00000040	
#define CC_CAP_EXTBUS_PROG	0x00000080	
#define	CC_CAP_FLASH_MASK	0x00000700	
#define	CC_CAP_PLL_MASK		0x00038000	
#define CC_CAP_PWR_CTL		0x00040000	
#define CC_CAP_OTPSIZE		0x00380000	
#define CC_CAP_OTPSIZE_SHIFT	19	
#define CC_CAP_OTPSIZE_BASE	5	
#define CC_CAP_JTAGP		0x00400000	
#define CC_CAP_ROM		0x00800000	
#define CC_CAP_BKPLN64		0x08000000	
#define	CC_CAP_PMU		0x10000000	
#define	CC_CAP_SROM		0x40000000	
#define	CC_CAP_NFLASH		0x80000000

#define	CC_CAP2_SECI		0x00000001	
#define	CC_CAP2_GSIO		0x00000002

#define PCAP_REV_MASK	0x000000ff
#define PCAP_RC_MASK	0x00001f00
#define PCAP_RC_SHIFT	8
#define PCAP_TC_MASK	0x0001e000
#define PCAP_TC_SHIFT	13
#define PCAP_PC_MASK	0x001e0000
#define PCAP_PC_SHIFT	17
#define PCAP_VC_MASK	0x01e00000
#define PCAP_VC_SHIFT	21
#define PCAP_CC_MASK	0x1e000000
#define PCAP_CC_SHIFT	25
#define PCAP5_PC_MASK	0x003e0000	
#define PCAP5_PC_SHIFT	17
#define PCAP5_VC_MASK	0x07c00000
#define PCAP5_VC_SHIFT	22
#define PCAP5_CC_MASK	0xf8000000
#define PCAP5_CC_SHIFT	27

#define PMU_MAX_TRANSITION_DLY	15000

#endif				
