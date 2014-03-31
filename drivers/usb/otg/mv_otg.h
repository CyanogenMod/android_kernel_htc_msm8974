/*
 * Copyright (C) 2011 Marvell International Ltd. All rights reserved.
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#ifndef	__MV_USB_OTG_CONTROLLER__
#define	__MV_USB_OTG_CONTROLLER__

#include <linux/types.h>

#define USBCMD_RUN_STOP			(0x00000001)
#define USBCMD_CTRL_RESET		(0x00000002)

#define OTGSC_CTRL_VUSB_DISCHARGE		0x00000001
#define OTGSC_CTRL_VUSB_CHARGE			0x00000002
#define OTGSC_CTRL_OTG_TERM			0x00000008
#define OTGSC_CTRL_DATA_PULSING			0x00000010
#define OTGSC_STS_USB_ID			0x00000100
#define OTGSC_STS_A_VBUS_VALID			0x00000200
#define OTGSC_STS_A_SESSION_VALID		0x00000400
#define OTGSC_STS_B_SESSION_VALID		0x00000800
#define OTGSC_STS_B_SESSION_END			0x00001000
#define OTGSC_STS_1MS_TOGGLE			0x00002000
#define OTGSC_STS_DATA_PULSING			0x00004000
#define OTGSC_INTSTS_USB_ID			0x00010000
#define OTGSC_INTSTS_A_VBUS_VALID		0x00020000
#define OTGSC_INTSTS_A_SESSION_VALID		0x00040000
#define OTGSC_INTSTS_B_SESSION_VALID		0x00080000
#define OTGSC_INTSTS_B_SESSION_END		0x00100000
#define OTGSC_INTSTS_1MS			0x00200000
#define OTGSC_INTSTS_DATA_PULSING		0x00400000
#define OTGSC_INTR_USB_ID			0x01000000
#define OTGSC_INTR_A_VBUS_VALID			0x02000000
#define OTGSC_INTR_A_SESSION_VALID		0x04000000
#define OTGSC_INTR_B_SESSION_VALID		0x08000000
#define OTGSC_INTR_B_SESSION_END		0x10000000
#define OTGSC_INTR_1MS_TIMER			0x20000000
#define OTGSC_INTR_DATA_PULSING			0x40000000

#define CAPLENGTH_MASK		(0xff)

#define T_A_WAIT_VRISE		100
#define T_A_WAIT_BCON		2000
#define T_A_AIDL_BDIS		100
#define T_A_BIDL_ADIS		20
#define T_B_ASE0_BRST		400
#define T_B_SE0_SRP		300
#define T_B_SRP_FAIL		2000
#define T_B_DATA_PLS		10
#define T_B_SRP_INIT		100
#define T_A_SRP_RSPNS		10
#define T_A_DRV_RSM		5

enum otg_function {
	OTG_B_DEVICE = 0,
	OTG_A_DEVICE
};

enum mv_otg_timer {
	A_WAIT_BCON_TIMER = 0,
	OTG_TIMER_NUM
};

struct mv_otg_ctrl {
	
	u8 a_set_b_hnp_en;	
	u8 b_srp_done;
	u8 b_hnp_en;

	
	u8 a_bus_drop;
	u8 a_bus_req;
	u8 a_clr_err;
	u8 a_bus_resume;
	u8 a_bus_suspend;
	u8 a_conn;
	u8 a_sess_vld;
	u8 a_srp_det;
	u8 a_vbus_vld;
	u8 b_bus_req;		
	u8 b_bus_resume;
	u8 b_bus_suspend;
	u8 b_conn;
	u8 b_se0_srp;
	u8 b_sess_end;
	u8 b_sess_vld;
	u8 id;
	u8 a_suspend_req;

	
	u8 a_aidl_bdis_timeout;
	u8 b_ase0_brst_timeout;
	u8 a_bidl_adis_timeout;
	u8 a_wait_bcon_timeout;

	struct timer_list timer[OTG_TIMER_NUM];
};

#define VUSBHS_MAX_PORTS	8

struct mv_otg_regs {
	u32 usbcmd;		
	u32 usbsts;		
	u32 usbintr;		
	u32 frindex;		
	u32 reserved1[1];
	u32 deviceaddr;		
	u32 eplistaddr;		
	u32 ttctrl;		
	u32 burstsize;		
	u32 txfilltuning;	
	u32 reserved[4];
	u32 epnak;		
	u32 epnaken;		
	u32 configflag;		
	u32 portsc[VUSBHS_MAX_PORTS];	
	u32 otgsc;
	u32 usbmode;		
	u32 epsetupstat;	
	u32 epprime;		
	u32 epflush;		
	u32 epstatus;		
	u32 epcomplete;		
	u32 epctrlx[16];	
	u32 mcr;		
	u32 isr;		
	u32 ier;		
};

struct mv_otg {
	struct usb_phy phy;
	struct mv_otg_ctrl otg_ctrl;

	
	void __iomem *phy_regs;
	void __iomem *cap_regs;
	struct mv_otg_regs __iomem *op_regs;

	struct platform_device *pdev;
	int irq;
	u32 irq_status;
	u32 irq_en;

	struct delayed_work work;
	struct workqueue_struct *qwork;

	spinlock_t wq_lock;

	struct mv_usb_platform_data *pdata;

	unsigned int active;
	unsigned int clock_gating;
	unsigned int clknum;
	struct clk *clk[0];
};

#endif
