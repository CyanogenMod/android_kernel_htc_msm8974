/*
 * Universal Host Controller Interface driver for USB.
 *
 * Maintainer: Alan Stern <stern@rowland.harvard.edu>
 *
 * (C) Copyright 1999 Linus Torvalds
 * (C) Copyright 1999-2002 Johannes Erdfelt, johannes@erdfelt.com
 * (C) Copyright 1999 Randy Dunlap
 * (C) Copyright 1999 Georg Acher, acher@in.tum.de
 * (C) Copyright 1999 Deti Fliegl, deti@fliegl.de
 * (C) Copyright 1999 Thomas Sailer, sailer@ife.ee.ethz.ch
 * (C) Copyright 2004 Alan Stern, stern@rowland.harvard.edu
 */

static const __u8 root_hub_hub_des[] =
{
	0x09,			
	0x29,			
	0x02,			
	0x0a,			
	0x00,			
	0x01,			
	0x00,			
	0x00,			
	0xff			
};

#define	UHCI_RH_MAXCHILD	7

#define WZ_BITS		(USBPORTSC_RES2 | USBPORTSC_RES3 | USBPORTSC_RES4)

#define RWC_BITS	(USBPORTSC_OCC | USBPORTSC_PEC | USBPORTSC_CSC)

#define SUSPEND_BITS	(USBPORTSC_SUSP | USBPORTSC_RD)

static int any_ports_active(struct uhci_hcd *uhci)
{
	int port;

	for (port = 0; port < uhci->rh_numports; ++port) {
		if ((uhci_readw(uhci, USBPORTSC1 + port * 2) &
				(USBPORTSC_CCS | RWC_BITS)) ||
				test_bit(port, &uhci->port_c_suspend))
			return 1;
	}
	return 0;
}

static inline int get_hub_status_data(struct uhci_hcd *uhci, char *buf)
{
	int port;
	int mask = RWC_BITS;

	if (ignore_oc)
		mask &= ~USBPORTSC_OCC;

	*buf = 0;
	for (port = 0; port < uhci->rh_numports; ++port) {
		if ((uhci_readw(uhci, USBPORTSC1 + port * 2) & mask) ||
				test_bit(port, &uhci->port_c_suspend))
			*buf |= (1 << (port + 1));
	}
	return !!*buf;
}

#define OK(x)			len = (x); break

#define CLR_RH_PORTSTAT(x) \
	status = uhci_readw(uhci, port_addr);	\
	status &= ~(RWC_BITS|WZ_BITS); \
	status &= ~(x); \
	status |= RWC_BITS & (x); \
	uhci_writew(uhci, status, port_addr)

#define SET_RH_PORTSTAT(x) \
	status = uhci_readw(uhci, port_addr);	\
	status |= (x); \
	status &= ~(RWC_BITS|WZ_BITS); \
	uhci_writew(uhci, status, port_addr)

static void uhci_finish_suspend(struct uhci_hcd *uhci, int port,
		unsigned long port_addr)
{
	int status;
	int i;

	if (uhci_readw(uhci, port_addr) & SUSPEND_BITS) {
		CLR_RH_PORTSTAT(SUSPEND_BITS);
		if (test_bit(port, &uhci->resuming_ports))
			set_bit(port, &uhci->port_c_suspend);

		for (i = 0; i < 10; ++i) {
			if (!(uhci_readw(uhci, port_addr) & SUSPEND_BITS))
				break;
			udelay(1);
		}
	}
	clear_bit(port, &uhci->resuming_ports);
}

static void wait_for_HP(struct uhci_hcd *uhci, unsigned long port_addr)
{
	int i;

	for (i = 10; i < 250; i += 10) {
		if (uhci_readw(uhci, port_addr) & USBPORTSC_CSC)
			return;
		udelay(10);
	}
	
}

static void uhci_check_ports(struct uhci_hcd *uhci)
{
	unsigned int port;
	unsigned long port_addr;
	int status;

	for (port = 0; port < uhci->rh_numports; ++port) {
		port_addr = USBPORTSC1 + 2 * port;
		status = uhci_readw(uhci, port_addr);
		if (unlikely(status & USBPORTSC_PR)) {
			if (time_after_eq(jiffies, uhci->ports_timeout)) {
				CLR_RH_PORTSTAT(USBPORTSC_PR);
				udelay(10);

				if (uhci->wait_for_hp)
					wait_for_HP(uhci, port_addr);

				CLR_RH_PORTSTAT(USBPORTSC_CSC | USBPORTSC_PEC);
				SET_RH_PORTSTAT(USBPORTSC_PE);
			}
		}
		if (unlikely(status & USBPORTSC_RD)) {
			if (!test_bit(port, &uhci->resuming_ports)) {

				
				set_bit(port, &uhci->resuming_ports);
				uhci->ports_timeout = jiffies +
						msecs_to_jiffies(25);

				mod_timer(&uhci_to_hcd(uhci)->rh_timer,
						uhci->ports_timeout);
			} else if (time_after_eq(jiffies,
						uhci->ports_timeout)) {
				uhci_finish_suspend(uhci, port, port_addr);
			}
		}
	}
}

static int uhci_hub_status_data(struct usb_hcd *hcd, char *buf)
{
	struct uhci_hcd *uhci = hcd_to_uhci(hcd);
	unsigned long flags;
	int status = 0;

	spin_lock_irqsave(&uhci->lock, flags);

	uhci_scan_schedule(uhci);
	if (!HCD_HW_ACCESSIBLE(hcd) || uhci->dead)
		goto done;
	uhci_check_ports(uhci);

	status = get_hub_status_data(uhci, buf);

	switch (uhci->rh_state) {
	    case UHCI_RH_SUSPENDED:
		
		if (status || uhci->resuming_ports) {
			status = 1;
			usb_hcd_resume_root_hub(hcd);
		}
		break;

	    case UHCI_RH_AUTO_STOPPED:
		
		if (status)
			wakeup_rh(uhci);
		break;

	    case UHCI_RH_RUNNING:
		
		if (!any_ports_active(uhci)) {
			uhci->rh_state = UHCI_RH_RUNNING_NODEVS;
			uhci->auto_stop_time = jiffies + HZ;
		}
		break;

	    case UHCI_RH_RUNNING_NODEVS:
		
		if (any_ports_active(uhci))
			uhci->rh_state = UHCI_RH_RUNNING;
		else if (time_after_eq(jiffies, uhci->auto_stop_time))
			suspend_rh(uhci, UHCI_RH_AUTO_STOPPED);
		break;

	    default:
		break;
	}

done:
	spin_unlock_irqrestore(&uhci->lock, flags);
	return status;
}

static int uhci_hub_control(struct usb_hcd *hcd, u16 typeReq, u16 wValue,
			u16 wIndex, char *buf, u16 wLength)
{
	struct uhci_hcd *uhci = hcd_to_uhci(hcd);
	int status, lstatus, retval = 0, len = 0;
	unsigned int port = wIndex - 1;
	unsigned long port_addr = USBPORTSC1 + 2 * port;
	u16 wPortChange, wPortStatus;
	unsigned long flags;

	if (!HCD_HW_ACCESSIBLE(hcd) || uhci->dead)
		return -ETIMEDOUT;

	spin_lock_irqsave(&uhci->lock, flags);
	switch (typeReq) {

	case GetHubStatus:
		*(__le32 *)buf = cpu_to_le32(0);
		OK(4);		
	case GetPortStatus:
		if (port >= uhci->rh_numports)
			goto err;

		uhci_check_ports(uhci);
		status = uhci_readw(uhci, port_addr);

		if (uhci->oc_low)
			status ^= USBPORTSC_OC;

		
		wPortChange = lstatus = 0;
		if (status & USBPORTSC_CSC)
			wPortChange |= USB_PORT_STAT_C_CONNECTION;
		if (status & USBPORTSC_PEC)
			wPortChange |= USB_PORT_STAT_C_ENABLE;
		if ((status & USBPORTSC_OCC) && !ignore_oc)
			wPortChange |= USB_PORT_STAT_C_OVERCURRENT;

		if (test_bit(port, &uhci->port_c_suspend)) {
			wPortChange |= USB_PORT_STAT_C_SUSPEND;
			lstatus |= 1;
		}
		if (test_bit(port, &uhci->resuming_ports))
			lstatus |= 4;

		
		wPortStatus = USB_PORT_STAT_POWER;
		if (status & USBPORTSC_CCS)
			wPortStatus |= USB_PORT_STAT_CONNECTION;
		if (status & USBPORTSC_PE) {
			wPortStatus |= USB_PORT_STAT_ENABLE;
			if (status & SUSPEND_BITS)
				wPortStatus |= USB_PORT_STAT_SUSPEND;
		}
		if (status & USBPORTSC_OC)
			wPortStatus |= USB_PORT_STAT_OVERCURRENT;
		if (status & USBPORTSC_PR)
			wPortStatus |= USB_PORT_STAT_RESET;
		if (status & USBPORTSC_LSDA)
			wPortStatus |= USB_PORT_STAT_LOW_SPEED;

		if (wPortChange)
			dev_dbg(uhci_dev(uhci), "port %d portsc %04x,%02x\n",
					wIndex, status, lstatus);

		*(__le16 *)buf = cpu_to_le16(wPortStatus);
		*(__le16 *)(buf + 2) = cpu_to_le16(wPortChange);
		OK(4);
	case SetHubFeature:		
	case ClearHubFeature:
		switch (wValue) {
		case C_HUB_OVER_CURRENT:
		case C_HUB_LOCAL_POWER:
			OK(0);
		default:
			goto err;
		}
		break;
	case SetPortFeature:
		if (port >= uhci->rh_numports)
			goto err;

		switch (wValue) {
		case USB_PORT_FEAT_SUSPEND:
			SET_RH_PORTSTAT(USBPORTSC_SUSP);
			OK(0);
		case USB_PORT_FEAT_RESET:
			SET_RH_PORTSTAT(USBPORTSC_PR);

			
			uhci_finish_suspend(uhci, port, port_addr);

			
			uhci->ports_timeout = jiffies + msecs_to_jiffies(50);
			OK(0);
		case USB_PORT_FEAT_POWER:
			
			OK(0);
		default:
			goto err;
		}
		break;
	case ClearPortFeature:
		if (port >= uhci->rh_numports)
			goto err;

		switch (wValue) {
		case USB_PORT_FEAT_ENABLE:
			CLR_RH_PORTSTAT(USBPORTSC_PE);

			
			uhci_finish_suspend(uhci, port, port_addr);
			OK(0);
		case USB_PORT_FEAT_C_ENABLE:
			CLR_RH_PORTSTAT(USBPORTSC_PEC);
			OK(0);
		case USB_PORT_FEAT_SUSPEND:
			if (!(uhci_readw(uhci, port_addr) & USBPORTSC_SUSP)) {

				
				uhci_finish_suspend(uhci, port, port_addr);
			} else if (!test_and_set_bit(port,
						&uhci->resuming_ports)) {
				SET_RH_PORTSTAT(USBPORTSC_RD);

				if (!(uhci_readw(uhci, port_addr) &
						USBPORTSC_RD))
					uhci_finish_suspend(uhci, port,
							port_addr);
				else
					
					uhci->ports_timeout = jiffies +
						msecs_to_jiffies(20);
			}
			OK(0);
		case USB_PORT_FEAT_C_SUSPEND:
			clear_bit(port, &uhci->port_c_suspend);
			OK(0);
		case USB_PORT_FEAT_POWER:
			
			goto err;
		case USB_PORT_FEAT_C_CONNECTION:
			CLR_RH_PORTSTAT(USBPORTSC_CSC);
			OK(0);
		case USB_PORT_FEAT_C_OVER_CURRENT:
			CLR_RH_PORTSTAT(USBPORTSC_OCC);
			OK(0);
		case USB_PORT_FEAT_C_RESET:
			
			OK(0);
		default:
			goto err;
		}
		break;
	case GetHubDescriptor:
		len = min_t(unsigned int, sizeof(root_hub_hub_des), wLength);
		memcpy(buf, root_hub_hub_des, len);
		if (len > 2)
			buf[2] = uhci->rh_numports;
		OK(len);
	default:
err:
		retval = -EPIPE;
	}
	spin_unlock_irqrestore(&uhci->lock, flags);

	return retval;
}
