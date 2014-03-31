/* Copyright (c) 2009-2012, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __LINUX_USB_GADGET_MSM72K_OTG_H__
#define __LINUX_USB_GADGET_MSM72K_OTG_H__

#include <linux/usb.h>
#include <linux/usb/gadget.h>
#include <linux/usb/otg.h>
#include <linux/wakelock.h>
#include <mach/msm_hsusb.h>

#include <asm/mach-types.h>
#include <mach/msm_hsusb.h>

#define OTGSC_BSVIE            (1 << 27)
#define OTGSC_IDIE             (1 << 24)
#define OTGSC_IDPU             (1 << 5)
#define OTGSC_BSVIS            (1 << 19)
#define OTGSC_ID               (1 << 8)
#define OTGSC_IDIS             (1 << 16)
#define OTGSC_BSV              (1 << 11)
#define OTGSC_DPIE             (1 << 30)
#define OTGSC_DPIS             (1 << 22)
#define OTGSC_HADP             (1 << 6)
#define OTGSC_IDPU             (1 << 5)

#define ULPI_STP_CTRL   (1 << 30)
#define ASYNC_INTR_CTRL (1 << 29)
#define ULPI_SYNC_STATE (1 << 27)

#define PORTSC_PHCD     (1 << 23)
#define PORTSC_CSC	(1 << 1)
#define disable_phy_clk() (writel(readl(USB_PORTSC) | PORTSC_PHCD, USB_PORTSC))
#define enable_phy_clk() (writel(readl(USB_PORTSC) & ~PORTSC_PHCD, USB_PORTSC))
#define is_phy_clk_disabled() (readl(USB_PORTSC) & PORTSC_PHCD)
#define is_phy_active()       (readl_relaxed(USB_ULPI_VIEWPORT) &\
						ULPI_SYNC_STATE)
#define is_usb_active()       (!(readl(USB_PORTSC) & PORTSC_SUSP))


#define TA_WAIT_VRISE	100	
#define TA_WAIT_VFALL	500	

#ifdef CONFIG_MSM_OTG_ENABLE_A_WAIT_BCON_TIMEOUT
#define TA_WAIT_BCON	30000	
#else
#define TA_WAIT_BCON	-1
#endif

#define TA_AIDL_BDIS	200	
#define TA_BIDL_ADIS	155	
#define TB_SRP_FAIL	6000	
#define TB_ASE0_BRST	155	

#define TB_SRP_INIT	2000	


#define A_WAIT_VRISE	0
#define A_WAIT_VFALL	1
#define A_WAIT_BCON	2
#define A_AIDL_BDIS	3
#define A_BIDL_ADIS	4
#define B_SRP_FAIL	5
#define B_ASE0_BRST	6


#define A_BUS_DROP		0
#define A_BUS_REQ		1
#define A_SRP_DET		2
#define A_VBUS_VLD		3
#define B_CONN			4
#define ID			5
#define ADP_CHANGE		6
#define POWER_UP		7
#define A_CLR_ERR		8
#define A_BUS_RESUME		9
#define A_BUS_SUSPEND		10
#define A_CONN			11
#define B_BUS_REQ		12
#define B_SESS_VLD		13
#define ID_A			14
#define ID_B			15
#define ID_C			16

#define USB_IDCHG_MIN	500
#define USB_IDCHG_MAX	1500
#define USB_IB_UNCFG	2
#define OTG_ID_POLL_MS	1000

struct msm_otg {
	struct usb_phy phy;

	
	struct clk		*alt_core_clk;
	struct clk		*iface_clk;
	struct clk		*core_clk;

	struct clk		*phy_reset_clk;

	int			irq;
	int			vbus_on_irq;
	int			id_irq;
	void __iomem		*regs;
	atomic_t		in_lpm;
	atomic_t 		chg_type;

	void (*start_host)	(struct usb_bus *bus, int suspend);
	
	int (*set_clk)		(struct usb_phy *phy, int on);
	
	void (*reset)		(struct usb_phy *phy, int phy_reset);
	
	u8 pmic_vbus_notif_supp;
	u8 pmic_id_notif_supp;
	struct msm_otg_platform_data *pdata;

	spinlock_t lock; 
	struct wake_lock wlock;
	unsigned long b_last_se0_sess; 
	unsigned long inputs;
	unsigned long tmouts;
	u8 active_tmout;
	struct hrtimer timer;
	struct workqueue_struct *wq;
	struct work_struct sm_work; 
	struct work_struct otg_resume_work;
	struct notifier_block usbdev_nb;
	struct msm_xo_voter *xo_handle; 
	unsigned curr_power;
#ifdef CONFIG_USB_MSM_ACA
	struct timer_list	id_timer;	
	unsigned		b_max_power;	
#endif
};

static inline int can_phy_power_collapse(struct msm_otg *dev)
{
	if (!dev || !dev->pdata)
		return -ENODEV;

	return dev->pdata->phy_can_powercollapse;
}

#endif
