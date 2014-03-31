/*
 * OHCI HCD (Host Controller Driver) for USB.
 *
 * TI DA8xx (OMAP-L1x) Bus Glue
 *
 * Derived from: ohci-omap.c and ohci-s3c2410.c
 * Copyright (C) 2008-2009 MontaVista Software, Inc. <source@mvista.com>
 *
 * This file is licensed under the terms of the GNU General Public License
 * version 2. This program is licensed "as is" without any warranty of any
 * kind, whether express or implied.
 */

#include <linux/interrupt.h>
#include <linux/jiffies.h>
#include <linux/platform_device.h>
#include <linux/clk.h>

#include <mach/da8xx.h>
#include <mach/usb.h>

#ifndef CONFIG_ARCH_DAVINCI_DA8XX
#error "This file is DA8xx bus glue.  Define CONFIG_ARCH_DAVINCI_DA8XX."
#endif

#define CFGCHIP2	DA8XX_SYSCFG0_VIRT(DA8XX_CFGCHIP2_REG)

static struct clk *usb11_clk;
static struct clk *usb20_clk;

static volatile u16 ocic_mask;

static void ohci_da8xx_clock(int on)
{
	u32 cfgchip2;

	cfgchip2 = __raw_readl(CFGCHIP2);
	if (on) {
		clk_enable(usb11_clk);

		if (!(cfgchip2 & CFGCHIP2_USB1PHYCLKMUX)) {
			clk_enable(usb20_clk);

			cfgchip2 &= ~(CFGCHIP2_RESET | CFGCHIP2_PHYPWRDN);
			cfgchip2 |= CFGCHIP2_PHY_PLLON;
			__raw_writel(cfgchip2, CFGCHIP2);

			pr_info("Waiting for USB PHY clock good...\n");
			while (!(__raw_readl(CFGCHIP2) & CFGCHIP2_PHYCLKGD))
				cpu_relax();
		}

		
		cfgchip2 |= CFGCHIP2_USB1SUSPENDM;
	} else {
		clk_disable(usb11_clk);
		if (!(cfgchip2 & CFGCHIP2_USB1PHYCLKMUX))
			clk_disable(usb20_clk);

		
		cfgchip2 &= ~CFGCHIP2_USB1SUSPENDM;
	}
	__raw_writel(cfgchip2, CFGCHIP2);
}

static void ohci_da8xx_ocic_handler(struct da8xx_ohci_root_hub *hub,
				    unsigned port)
{
	ocic_mask |= 1 << port;

	
	if (hub->get_oci(port) > 0)
		hub->set_power(port, 0);
}

static int ohci_da8xx_init(struct usb_hcd *hcd)
{
	struct device *dev		= hcd->self.controller;
	struct da8xx_ohci_root_hub *hub	= dev->platform_data;
	struct ohci_hcd	*ohci		= hcd_to_ohci(hcd);
	int result;
	u32 rh_a;

	dev_dbg(dev, "starting USB controller\n");

	ohci_da8xx_clock(1);

	ohci->num_ports = 1;

	result = ohci_init(ohci);
	if (result < 0)
		return result;

	rh_a = ohci_readl(ohci, &ohci->regs->roothub.a);
	if (hub->set_power) {
		rh_a &= ~RH_A_NPS;
		rh_a |=  RH_A_PSM;
	}
	if (hub->get_oci) {
		rh_a &= ~RH_A_NOCP;
		rh_a |=  RH_A_OCPM;
	}
	rh_a &= ~RH_A_POTPGT;
	rh_a |= hub->potpgt << 24;
	ohci_writel(ohci, rh_a, &ohci->regs->roothub.a);

	return result;
}

static void ohci_da8xx_stop(struct usb_hcd *hcd)
{
	ohci_stop(hcd);
	ohci_da8xx_clock(0);
}

static int ohci_da8xx_start(struct usb_hcd *hcd)
{
	struct ohci_hcd	*ohci		= hcd_to_ohci(hcd);
	int result;

	result = ohci_run(ohci);
	if (result < 0)
		ohci_da8xx_stop(hcd);

	return result;
}

static int ohci_da8xx_hub_status_data(struct usb_hcd *hcd, char *buf)
{
	int length		= ohci_hub_status_data(hcd, buf);

	
	if (ocic_mask & (1 << 1)) {
		dev_dbg(hcd->self.controller, "over-current indicator change "
			"on port 1\n");

		if (!length)
			length = 1;

		buf[0] |= 1 << 1;
	}
	return length;
}

static int ohci_da8xx_hub_control(struct usb_hcd *hcd, u16 typeReq, u16 wValue,
				  u16 wIndex, char *buf, u16 wLength)
{
	struct device *dev		= hcd->self.controller;
	struct da8xx_ohci_root_hub *hub	= dev->platform_data;
	int temp;

	switch (typeReq) {
	case GetPortStatus:
		
		if (wIndex != 1)
			break;

		dev_dbg(dev, "GetPortStatus(%u)\n", wIndex);

		temp = roothub_portstatus(hcd_to_ohci(hcd), wIndex - 1);

		
		if (hub->get_power && hub->get_power(wIndex) == 0)
			temp &= ~RH_PS_PPS;

		
		if (hub->get_oci && hub->get_oci(wIndex) > 0)
			temp |=  RH_PS_POCI;

		
		if (ocic_mask & (1 << wIndex))
			temp |=  RH_PS_OCIC;

		put_unaligned(cpu_to_le32(temp), (__le32 *)buf);
		return 0;
	case SetPortFeature:
		temp = 1;
		goto check_port;
	case ClearPortFeature:
		temp = 0;

check_port:
		
		if (wIndex != 1)
			break;

		switch (wValue) {
		case USB_PORT_FEAT_POWER:
			dev_dbg(dev, "%sPortFeature(%u): %s\n",
				temp ? "Set" : "Clear", wIndex, "POWER");

			if (!hub->set_power)
				return -EPIPE;

			return hub->set_power(wIndex, temp) ? -EPIPE : 0;
		case USB_PORT_FEAT_C_OVER_CURRENT:
			dev_dbg(dev, "%sPortFeature(%u): %s\n",
				temp ? "Set" : "Clear", wIndex,
				"C_OVER_CURRENT");

			if (temp)
				ocic_mask |= 1 << wIndex;
			else
				ocic_mask &= ~(1 << wIndex);
			return 0;
		}
	}

	return ohci_hub_control(hcd, typeReq, wValue, wIndex, buf, wLength);
}

static const struct hc_driver ohci_da8xx_hc_driver = {
	.description		= hcd_name,
	.product_desc		= "DA8xx OHCI",
	.hcd_priv_size		= sizeof(struct ohci_hcd),

	.irq			= ohci_irq,
	.flags			= HCD_USB11 | HCD_MEMORY,

	.reset			= ohci_da8xx_init,
	.start			= ohci_da8xx_start,
	.stop			= ohci_da8xx_stop,
	.shutdown		= ohci_shutdown,

	.urb_enqueue		= ohci_urb_enqueue,
	.urb_dequeue		= ohci_urb_dequeue,
	.endpoint_disable	= ohci_endpoint_disable,

	.get_frame_number	= ohci_get_frame,

	.hub_status_data	= ohci_da8xx_hub_status_data,
	.hub_control		= ohci_da8xx_hub_control,

#ifdef	CONFIG_PM
	.bus_suspend		= ohci_bus_suspend,
	.bus_resume		= ohci_bus_resume,
#endif
	.start_port_reset	= ohci_start_port_reset,
};



static int usb_hcd_da8xx_probe(const struct hc_driver *driver,
			       struct platform_device *pdev)
{
	struct da8xx_ohci_root_hub *hub	= pdev->dev.platform_data;
	struct usb_hcd	*hcd;
	struct resource *mem;
	int error, irq;

	if (hub == NULL)
		return -ENODEV;

	usb11_clk = clk_get(&pdev->dev, "usb11");
	if (IS_ERR(usb11_clk))
		return PTR_ERR(usb11_clk);

	usb20_clk = clk_get(&pdev->dev, "usb20");
	if (IS_ERR(usb20_clk)) {
		error = PTR_ERR(usb20_clk);
		goto err0;
	}

	hcd = usb_create_hcd(driver, &pdev->dev, dev_name(&pdev->dev));
	if (!hcd) {
		error = -ENOMEM;
		goto err1;
	}

	mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!mem) {
		error = -ENODEV;
		goto err2;
	}
	hcd->rsrc_start = mem->start;
	hcd->rsrc_len = resource_size(mem);

	if (!request_mem_region(hcd->rsrc_start, hcd->rsrc_len, hcd_name)) {
		dev_dbg(&pdev->dev, "request_mem_region failed\n");
		error = -EBUSY;
		goto err2;
	}

	hcd->regs = ioremap(hcd->rsrc_start, hcd->rsrc_len);
	if (!hcd->regs) {
		dev_err(&pdev->dev, "ioremap failed\n");
		error = -ENOMEM;
		goto err3;
	}

	ohci_hcd_init(hcd_to_ohci(hcd));

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		error = -ENODEV;
		goto err4;
	}
	error = usb_add_hcd(hcd, irq, 0);
	if (error)
		goto err4;

	if (hub->ocic_notify) {
		error = hub->ocic_notify(ohci_da8xx_ocic_handler);
		if (!error)
			return 0;
	}

	usb_remove_hcd(hcd);
err4:
	iounmap(hcd->regs);
err3:
	release_mem_region(hcd->rsrc_start, hcd->rsrc_len);
err2:
	usb_put_hcd(hcd);
err1:
	clk_put(usb20_clk);
err0:
	clk_put(usb11_clk);
	return error;
}

static inline void
usb_hcd_da8xx_remove(struct usb_hcd *hcd, struct platform_device *pdev)
{
	struct da8xx_ohci_root_hub *hub	= pdev->dev.platform_data;

	hub->ocic_notify(NULL);
	usb_remove_hcd(hcd);
	iounmap(hcd->regs);
	release_mem_region(hcd->rsrc_start, hcd->rsrc_len);
	usb_put_hcd(hcd);
	clk_put(usb20_clk);
	clk_put(usb11_clk);
}

static int ohci_hcd_da8xx_drv_probe(struct platform_device *dev)
{
	return usb_hcd_da8xx_probe(&ohci_da8xx_hc_driver, dev);
}

static int ohci_hcd_da8xx_drv_remove(struct platform_device *dev)
{
	struct usb_hcd	*hcd = platform_get_drvdata(dev);

	usb_hcd_da8xx_remove(hcd, dev);
	platform_set_drvdata(dev, NULL);

	return 0;
}

#ifdef CONFIG_PM
static int ohci_da8xx_suspend(struct platform_device *dev, pm_message_t message)
{
	struct usb_hcd	*hcd	= platform_get_drvdata(dev);
	struct ohci_hcd	*ohci	= hcd_to_ohci(hcd);

	if (time_before(jiffies, ohci->next_statechange))
		msleep(5);
	ohci->next_statechange = jiffies;

	ohci_da8xx_clock(0);
	hcd->state = HC_STATE_SUSPENDED;
	dev->dev.power.power_state = PMSG_SUSPEND;
	return 0;
}

static int ohci_da8xx_resume(struct platform_device *dev)
{
	struct usb_hcd	*hcd	= platform_get_drvdata(dev);
	struct ohci_hcd	*ohci	= hcd_to_ohci(hcd);

	if (time_before(jiffies, ohci->next_statechange))
		msleep(5);
	ohci->next_statechange = jiffies;

	ohci_da8xx_clock(1);
	dev->dev.power.power_state = PMSG_ON;
	usb_hcd_resume_root_hub(hcd);
	return 0;
}
#endif

static struct platform_driver ohci_hcd_da8xx_driver = {
	.probe		= ohci_hcd_da8xx_drv_probe,
	.remove		= ohci_hcd_da8xx_drv_remove,
	.shutdown 	= usb_hcd_platform_shutdown,
#ifdef	CONFIG_PM
	.suspend	= ohci_da8xx_suspend,
	.resume		= ohci_da8xx_resume,
#endif
	.driver		= {
		.owner	= THIS_MODULE,
		.name	= "ohci",
	},
};
