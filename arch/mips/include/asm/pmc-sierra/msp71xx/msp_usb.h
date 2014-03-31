/******************************************************************
 * Copyright (c) 2000-2007 PMC-Sierra INC.
 *
 *     This program is free software; you can redistribute it
 *     and/or modify it under the terms of the GNU General
 *     Public License as published by the Free Software
 *     Foundation; either version 2 of the License, or (at your
 *     option) any later version.
 *
 *     This program is distributed in the hope that it will be
 *     useful, but WITHOUT ANY WARRANTY; without even the implied
 *     warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *     PURPOSE.  See the GNU General Public License for more
 *     details.
 *
 *     You should have received a copy of the GNU General Public
 *     License along with this program; if not, write to the Free
 *     Software Foundation, Inc., 675 Mass Ave, Cambridge, MA
 *     02139, USA.
 *
 * PMC-SIERRA INC. DISCLAIMS ANY LIABILITY OF ANY KIND
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS
 * SOFTWARE.
 */
#ifndef MSP_USB_H_
#define MSP_USB_H_

#ifdef CONFIG_MSP_HAS_DUAL_USB
#define NUM_USB_DEVS   2
#else
#define NUM_USB_DEVS   1
#endif

#define MSP_USB0_MAB_START	(MSP_USB0_BASE + 0x0)
#define MSP_USB0_MAB_END	(MSP_USB0_BASE + 0x17)
#define MSP_USB0_ID_START	(MSP_USB0_BASE + 0x40000)
#define MSP_USB0_ID_END		(MSP_USB0_BASE + 0x4008f)
#define MSP_USB0_HS_START	(MSP_USB0_BASE + 0x40100)
#define MSP_USB0_HS_END		(MSP_USB0_BASE + 0x401FF)

#define	MSP_USB1_MAB_START	(MSP_USB1_BASE + 0x0)
#define MSP_USB1_MAB_END	(MSP_USB1_BASE + 0x17)
#define MSP_USB1_ID_START	(MSP_USB1_BASE + 0x40000)
#define MSP_USB1_ID_END		(MSP_USB1_BASE + 0x4008f)
#define MSP_USB1_HS_START	(MSP_USB1_BASE + 0x40100)
#define MSP_USB1_HS_END		(MSP_USB1_BASE + 0x401ff)

struct msp_usbid_regs {
	u32 id;		
	u32 hwgen;	
	u32 hwhost;	
	u32 hwdev;	
	u32 hwtxbuf;	
	u32 hwrxbuf;	
	u32 reserved[26];
	u32 timer0_load; 
	u32 timer0_ctrl; 
	u32 timer1_load; 
	u32 timer1_ctrl; 
};

struct msp_mab_regs {
	u32 isr;	
	u32 imr;	
	u32 thcr0;	
	u32 thcr1;	
	u32 int_stat;	
	u32 phy_cfg;	
};

struct msp_usbhs_regs {
	u32 hciver;	
	u32 hcsparams;	
	u32 hccparams;	
	u32 reserved0[5];
	u32 dciver;	
	u32 dccparams;	
	u32 reserved1[6];
	u32 cmd;	
	u32 sts;	
	u32 int_ena;	
	u32 frindex;	
	u32 reserved3;
	union {
		struct {
			u32 flb_addr; 
			u32 next_async_addr; 
			u32 ttctrl; 
			u32 burst_size; 
			u32 tx_fifo_ctrl; 
			u32 reserved0[4];
			u32 endpt_nak; 
			u32 endpt_nak_ena; 
			u32 cfg_flag; 
			u32 port_sc1; 
			u32 reserved1[7];
			u32 otgsc;	
			u32 mode;	
		} host;

		struct {
			u32 dev_addr; 
			u32 endpt_list_addr; 
			u32 reserved0[7];
			u32 endpt_nak;	
			u32 endpt_nak_ctrl; 
			u32 cfg_flag; 
			u32 port_sc1; 
			u32 reserved[7];
			u32 otgsc;	
			u32 mode;	
			u32 endpt_setup_stat; 
			u32 endpt_prime; 
			u32 endpt_flush; 
			u32 endpt_stat; 
			u32 endpt_complete; 
			u32 endpt_ctrl0; 
			u32 endpt_ctrl1; 
			u32 endpt_ctrl2; 
			u32 endpt_ctrl3; 
		} device;
	} u;
};
struct mspusb_device {
	struct msp_mab_regs   __iomem *mab_regs;
	struct msp_usbid_regs __iomem *usbid_regs;
	struct msp_usbhs_regs __iomem *usbhs_regs;
	struct platform_device dev;
};

#define to_mspusb_device(x) container_of((x), struct mspusb_device, dev)
#define TO_HOST_ID(x) ((x) & 0x3)
#endif 
