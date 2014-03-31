/*
 * MUSB OTG driver virtual root hub support
 *
 * Copyright 2005 Mentor Graphics Corporation
 * Copyright (C) 2005-2006 by Texas Instruments
 * Copyright (C) 2006-2007 Nokia Corporation
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 * NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/time.h>
#include <linux/timer.h>

#include <asm/unaligned.h>

#include "musb_core.h"


static void musb_port_suspend(struct musb *musb, bool do_suspend)
{
	struct usb_otg	*otg = musb->xceiv->otg;
	u8		power;
	void __iomem	*mbase = musb->mregs;

	if (!is_host_active(musb))
		return;

	power = musb_readb(mbase, MUSB_POWER);
	if (do_suspend) {
		int retries = 10000;

		power &= ~MUSB_POWER_RESUME;
		power |= MUSB_POWER_SUSPENDM;
		musb_writeb(mbase, MUSB_POWER, power);

		
		power = musb_readb(mbase, MUSB_POWER);
		while (power & MUSB_POWER_SUSPENDM) {
			power = musb_readb(mbase, MUSB_POWER);
			if (retries-- < 1)
				break;
		}

		dev_dbg(musb->controller, "Root port suspended, power %02x\n", power);

		musb->port1_status |= USB_PORT_STAT_SUSPEND;
		switch (musb->xceiv->state) {
		case OTG_STATE_A_HOST:
			musb->xceiv->state = OTG_STATE_A_SUSPEND;
			musb->is_active = is_otg_enabled(musb)
					&& otg->host->b_hnp_enable;
			if (musb->is_active)
				mod_timer(&musb->otg_timer, jiffies
					+ msecs_to_jiffies(
						OTG_TIME_A_AIDL_BDIS));
			musb_platform_try_idle(musb, 0);
			break;
		case OTG_STATE_B_HOST:
			musb->xceiv->state = OTG_STATE_B_WAIT_ACON;
			musb->is_active = is_otg_enabled(musb)
					&& otg->host->b_hnp_enable;
			musb_platform_try_idle(musb, 0);
			break;
		default:
			dev_dbg(musb->controller, "bogus rh suspend? %s\n",
				otg_state_string(musb->xceiv->state));
		}
	} else if (power & MUSB_POWER_SUSPENDM) {
		power &= ~MUSB_POWER_SUSPENDM;
		power |= MUSB_POWER_RESUME;
		musb_writeb(mbase, MUSB_POWER, power);

		dev_dbg(musb->controller, "Root port resuming, power %02x\n", power);

		
		musb->port1_status |= MUSB_PORT_STAT_RESUME;
		musb->rh_timer = jiffies + msecs_to_jiffies(20);
	}
}

static void musb_port_reset(struct musb *musb, bool do_reset)
{
	u8		power;
	void __iomem	*mbase = musb->mregs;

	if (musb->xceiv->state == OTG_STATE_B_IDLE) {
		dev_dbg(musb->controller, "HNP: Returning from HNP; no hub reset from b_idle\n");
		musb->port1_status &= ~USB_PORT_STAT_RESET;
		return;
	}

	if (!is_host_active(musb))
		return;

	power = musb_readb(mbase, MUSB_POWER);
	if (do_reset) {

		if (power &  MUSB_POWER_RESUME) {
			while (time_before(jiffies, musb->rh_timer))
				msleep(1);
			musb_writeb(mbase, MUSB_POWER,
				power & ~MUSB_POWER_RESUME);
			msleep(1);
		}

		musb->ignore_disconnect = true;
		power &= 0xf0;
		musb_writeb(mbase, MUSB_POWER,
				power | MUSB_POWER_RESET);

		musb->port1_status |= USB_PORT_STAT_RESET;
		musb->port1_status &= ~USB_PORT_STAT_ENABLE;
		musb->rh_timer = jiffies + msecs_to_jiffies(50);
	} else {
		dev_dbg(musb->controller, "root port reset stopped\n");
		musb_writeb(mbase, MUSB_POWER,
				power & ~MUSB_POWER_RESET);

		musb->ignore_disconnect = false;

		power = musb_readb(mbase, MUSB_POWER);
		if (power & MUSB_POWER_HSMODE) {
			dev_dbg(musb->controller, "high-speed device connected\n");
			musb->port1_status |= USB_PORT_STAT_HIGH_SPEED;
		}

		musb->port1_status &= ~USB_PORT_STAT_RESET;
		musb->port1_status |= USB_PORT_STAT_ENABLE
					| (USB_PORT_STAT_C_RESET << 16)
					| (USB_PORT_STAT_C_ENABLE << 16);
		usb_hcd_poll_rh_status(musb_to_hcd(musb));

		musb->vbuserr_retry = VBUSERR_RETRY_COUNT;
	}
}

void musb_root_disconnect(struct musb *musb)
{
	struct usb_otg	*otg = musb->xceiv->otg;

	musb->port1_status = USB_PORT_STAT_POWER
			| (USB_PORT_STAT_C_CONNECTION << 16);

	usb_hcd_poll_rh_status(musb_to_hcd(musb));
	musb->is_active = 0;

	switch (musb->xceiv->state) {
	case OTG_STATE_A_SUSPEND:
		if (is_otg_enabled(musb)
				&& otg->host->b_hnp_enable) {
			musb->xceiv->state = OTG_STATE_A_PERIPHERAL;
			musb->g.is_a_peripheral = 1;
			break;
		}
		
	case OTG_STATE_A_HOST:
		musb->xceiv->state = OTG_STATE_A_WAIT_BCON;
		musb->is_active = 0;
		break;
	case OTG_STATE_A_WAIT_VFALL:
		musb->xceiv->state = OTG_STATE_B_IDLE;
		break;
	default:
		dev_dbg(musb->controller, "host disconnect (%s)\n",
			otg_state_string(musb->xceiv->state));
	}
}



int musb_hub_status_data(struct usb_hcd *hcd, char *buf)
{
	struct musb	*musb = hcd_to_musb(hcd);
	int		retval = 0;

	
	if (musb->port1_status & 0xffff0000) {
		*buf = 0x02;
		retval = 1;
	}
	return retval;
}

int musb_hub_control(
	struct usb_hcd	*hcd,
	u16		typeReq,
	u16		wValue,
	u16		wIndex,
	char		*buf,
	u16		wLength)
{
	struct musb	*musb = hcd_to_musb(hcd);
	u32		temp;
	int		retval = 0;
	unsigned long	flags;

	spin_lock_irqsave(&musb->lock, flags);

	if (unlikely(!HCD_HW_ACCESSIBLE(hcd))) {
		spin_unlock_irqrestore(&musb->lock, flags);
		return -ESHUTDOWN;
	}

	switch (typeReq) {
	case ClearHubFeature:
	case SetHubFeature:
		switch (wValue) {
		case C_HUB_OVER_CURRENT:
		case C_HUB_LOCAL_POWER:
			break;
		default:
			goto error;
		}
		break;
	case ClearPortFeature:
		if ((wIndex & 0xff) != 1)
			goto error;

		switch (wValue) {
		case USB_PORT_FEAT_ENABLE:
			break;
		case USB_PORT_FEAT_SUSPEND:
			musb_port_suspend(musb, false);
			break;
		case USB_PORT_FEAT_POWER:
			if (!(is_otg_enabled(musb) && hcd->self.is_b_host))
				musb_platform_set_vbus(musb, 0);
			break;
		case USB_PORT_FEAT_C_CONNECTION:
		case USB_PORT_FEAT_C_ENABLE:
		case USB_PORT_FEAT_C_OVER_CURRENT:
		case USB_PORT_FEAT_C_RESET:
		case USB_PORT_FEAT_C_SUSPEND:
			break;
		default:
			goto error;
		}
		dev_dbg(musb->controller, "clear feature %d\n", wValue);
		musb->port1_status &= ~(1 << wValue);
		break;
	case GetHubDescriptor:
		{
		struct usb_hub_descriptor *desc = (void *)buf;

		desc->bDescLength = 9;
		desc->bDescriptorType = 0x29;
		desc->bNbrPorts = 1;
		desc->wHubCharacteristics = cpu_to_le16(
				  0x0001	
				| 0x0010	
				);
		desc->bPwrOn2PwrGood = 5;	
		desc->bHubContrCurrent = 0;

		
		desc->u.hs.DeviceRemovable[0] = 0x02;	
		desc->u.hs.DeviceRemovable[1] = 0xff;
		}
		break;
	case GetHubStatus:
		temp = 0;
		*(__le32 *) buf = cpu_to_le32(temp);
		break;
	case GetPortStatus:
		if (wIndex != 1)
			goto error;

		
		if ((musb->port1_status & USB_PORT_STAT_RESET)
				&& time_after_eq(jiffies, musb->rh_timer))
			musb_port_reset(musb, false);

		
		if ((musb->port1_status & MUSB_PORT_STAT_RESUME)
				&& time_after_eq(jiffies, musb->rh_timer)) {
			u8		power;

			power = musb_readb(musb->mregs, MUSB_POWER);
			power &= ~MUSB_POWER_RESUME;
			dev_dbg(musb->controller, "root port resume stopped, power %02x\n",
					power);
			musb_writeb(musb->mregs, MUSB_POWER, power);


			musb->is_active = 1;
			musb->port1_status &= ~(USB_PORT_STAT_SUSPEND
					| MUSB_PORT_STAT_RESUME);
			musb->port1_status |= USB_PORT_STAT_C_SUSPEND << 16;
			usb_hcd_poll_rh_status(musb_to_hcd(musb));
			
			musb->xceiv->state = OTG_STATE_A_HOST;
		}

		put_unaligned(cpu_to_le32(musb->port1_status
					& ~MUSB_PORT_STAT_RESUME),
				(__le32 *) buf);

		
		dev_dbg(musb->controller, "port status %08x\n",
				musb->port1_status);
		break;
	case SetPortFeature:
		if ((wIndex & 0xff) != 1)
			goto error;

		switch (wValue) {
		case USB_PORT_FEAT_POWER:
			if (!(is_otg_enabled(musb) && hcd->self.is_b_host))
				musb_start(musb);
			break;
		case USB_PORT_FEAT_RESET:
			musb_port_reset(musb, true);
			break;
		case USB_PORT_FEAT_SUSPEND:
			musb_port_suspend(musb, true);
			break;
		case USB_PORT_FEAT_TEST:
			if (unlikely(is_host_active(musb)))
				goto error;

			wIndex >>= 8;
			switch (wIndex) {
			case 1:
				pr_debug("TEST_J\n");
				temp = MUSB_TEST_J;
				break;
			case 2:
				pr_debug("TEST_K\n");
				temp = MUSB_TEST_K;
				break;
			case 3:
				pr_debug("TEST_SE0_NAK\n");
				temp = MUSB_TEST_SE0_NAK;
				break;
			case 4:
				pr_debug("TEST_PACKET\n");
				temp = MUSB_TEST_PACKET;
				musb_load_testpacket(musb);
				break;
			case 5:
				pr_debug("TEST_FORCE_ENABLE\n");
				temp = MUSB_TEST_FORCE_HOST
					| MUSB_TEST_FORCE_HS;

				musb_writeb(musb->mregs, MUSB_DEVCTL,
						MUSB_DEVCTL_SESSION);
				break;
			case 6:
				pr_debug("TEST_FIFO_ACCESS\n");
				temp = MUSB_TEST_FIFO_ACCESS;
				break;
			default:
				goto error;
			}
			musb_writeb(musb->mregs, MUSB_TESTMODE, temp);
			break;
		default:
			goto error;
		}
		dev_dbg(musb->controller, "set feature %d\n", wValue);
		musb->port1_status |= 1 << wValue;
		break;

	default:
error:
		
		retval = -EPIPE;
	}
	spin_unlock_irqrestore(&musb->lock, flags);
	return retval;
}
