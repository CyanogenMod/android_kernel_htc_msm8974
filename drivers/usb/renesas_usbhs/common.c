/*
 * Renesas USB driver
 *
 * Copyright (C) 2011 Renesas Solutions Corp.
 * Kuninori Morimoto <kuninori.morimoto.gx@renesas.com>
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */
#include <linux/io.h>
#include <linux/module.h>
#include <linux/pm_runtime.h>
#include <linux/slab.h>
#include <linux/sysfs.h>
#include "./common.h"



#define USBHSF_RUNTIME_PWCTRL	(1 << 0)

#define usbhsc_flags_init(p)   do {(p)->flags = 0; } while (0)
#define usbhsc_flags_set(p, b) ((p)->flags |=  (b))
#define usbhsc_flags_clr(p, b) ((p)->flags &= ~(b))
#define usbhsc_flags_has(p, b) ((p)->flags &   (b))

#define usbhs_platform_call(priv, func, args...)\
	(!(priv) ? -ENODEV :			\
	 !((priv)->pfunc.func) ? 0 :		\
	 (priv)->pfunc.func(args))

u16 usbhs_read(struct usbhs_priv *priv, u32 reg)
{
	return ioread16(priv->base + reg);
}

void usbhs_write(struct usbhs_priv *priv, u32 reg, u16 data)
{
	iowrite16(data, priv->base + reg);
}

void usbhs_bset(struct usbhs_priv *priv, u32 reg, u16 mask, u16 data)
{
	u16 val = usbhs_read(priv, reg);

	val &= ~mask;
	val |= data & mask;

	usbhs_write(priv, reg, val);
}

struct usbhs_priv *usbhs_pdev_to_priv(struct platform_device *pdev)
{
	return dev_get_drvdata(&pdev->dev);
}

static void usbhs_sys_clock_ctrl(struct usbhs_priv *priv, int enable)
{
	usbhs_bset(priv, SYSCFG, SCKE, enable ? SCKE : 0);
}

void usbhs_sys_host_ctrl(struct usbhs_priv *priv, int enable)
{
	u16 mask = DCFM | DRPD | DPRPU | HSE | USBE;
	u16 val  = DCFM | DRPD | HSE | USBE;
	int has_otg = usbhs_get_dparam(priv, has_otg);

	if (has_otg)
		usbhs_bset(priv, DVSTCTR, (EXTLP | PWEN), (EXTLP | PWEN));

	usbhs_bset(priv, SYSCFG, mask, enable ? val : 0);
}

void usbhs_sys_function_ctrl(struct usbhs_priv *priv, int enable)
{
	u16 mask = DCFM | DRPD | DPRPU | HSE | USBE;
	u16 val  = DPRPU | HSE | USBE;

	usbhs_bset(priv, SYSCFG, mask, enable ? val : 0);
}

void usbhs_sys_set_test_mode(struct usbhs_priv *priv, u16 mode)
{
	usbhs_write(priv, TESTMODE, mode);
}

int usbhs_frame_get_num(struct usbhs_priv *priv)
{
	return usbhs_read(priv, FRMNUM) & FRNM_MASK;
}

void usbhs_usbreq_get_val(struct usbhs_priv *priv, struct usb_ctrlrequest *req)
{
	u16 val;

	val = usbhs_read(priv, USBREQ);
	req->bRequest		= (val >> 8) & 0xFF;
	req->bRequestType	= (val >> 0) & 0xFF;

	req->wValue	= usbhs_read(priv, USBVAL);
	req->wIndex	= usbhs_read(priv, USBINDX);
	req->wLength	= usbhs_read(priv, USBLENG);
}

void usbhs_usbreq_set_val(struct usbhs_priv *priv, struct usb_ctrlrequest *req)
{
	usbhs_write(priv, USBREQ,  (req->bRequest << 8) | req->bRequestType);
	usbhs_write(priv, USBVAL,  req->wValue);
	usbhs_write(priv, USBINDX, req->wIndex);
	usbhs_write(priv, USBLENG, req->wLength);

	usbhs_bset(priv, DCPCTR, SUREQ, SUREQ);
}

void usbhs_bus_send_sof_enable(struct usbhs_priv *priv)
{
	u16 status = usbhs_read(priv, DVSTCTR) & (USBRST | UACT);

	if (status != USBRST) {
		struct device *dev = usbhs_priv_to_dev(priv);
		dev_err(dev, "usbhs should be reset\n");
	}

	usbhs_bset(priv, DVSTCTR, (USBRST | UACT), UACT);
}

void usbhs_bus_send_reset(struct usbhs_priv *priv)
{
	usbhs_bset(priv, DVSTCTR, (USBRST | UACT), USBRST);
}

int usbhs_bus_get_speed(struct usbhs_priv *priv)
{
	u16 dvstctr = usbhs_read(priv, DVSTCTR);

	switch (RHST & dvstctr) {
	case RHST_LOW_SPEED:
		return USB_SPEED_LOW;
	case RHST_FULL_SPEED:
		return USB_SPEED_FULL;
	case RHST_HIGH_SPEED:
		return USB_SPEED_HIGH;
	}

	return USB_SPEED_UNKNOWN;
}

int usbhs_vbus_ctrl(struct usbhs_priv *priv, int enable)
{
	struct platform_device *pdev = usbhs_priv_to_pdev(priv);

	return usbhs_platform_call(priv, set_vbus, pdev, enable);
}

static void usbhsc_bus_init(struct usbhs_priv *priv)
{
	usbhs_write(priv, DVSTCTR, 0);

	usbhs_vbus_ctrl(priv, 0);
}

int usbhs_set_device_config(struct usbhs_priv *priv, int devnum,
			   u16 upphub, u16 hubport, u16 speed)
{
	struct device *dev = usbhs_priv_to_dev(priv);
	u16 usbspd = 0;
	u32 reg = DEVADD0 + (2 * devnum);

	if (devnum > 10) {
		dev_err(dev, "cannot set speed to unknown device %d\n", devnum);
		return -EIO;
	}

	if (upphub > 0xA) {
		dev_err(dev, "unsupported hub number %d\n", upphub);
		return -EIO;
	}

	switch (speed) {
	case USB_SPEED_LOW:
		usbspd = USBSPD_SPEED_LOW;
		break;
	case USB_SPEED_FULL:
		usbspd = USBSPD_SPEED_FULL;
		break;
	case USB_SPEED_HIGH:
		usbspd = USBSPD_SPEED_HIGH;
		break;
	default:
		dev_err(dev, "unsupported speed %d\n", speed);
		return -EIO;
	}

	usbhs_write(priv, reg,	UPPHUB(upphub)	|
				HUBPORT(hubport)|
				USBSPD(usbspd));

	return 0;
}

static void usbhsc_set_buswait(struct usbhs_priv *priv)
{
	int wait = usbhs_get_dparam(priv, buswait_bwait);

	
	if (wait)
		usbhs_bset(priv, BUSWAIT, 0x000F, wait);
}

static u32 usbhsc_default_pipe_type[] = {
		USB_ENDPOINT_XFER_CONTROL,
		USB_ENDPOINT_XFER_ISOC,
		USB_ENDPOINT_XFER_ISOC,
		USB_ENDPOINT_XFER_BULK,
		USB_ENDPOINT_XFER_BULK,
		USB_ENDPOINT_XFER_BULK,
		USB_ENDPOINT_XFER_INT,
		USB_ENDPOINT_XFER_INT,
		USB_ENDPOINT_XFER_INT,
		USB_ENDPOINT_XFER_INT,
};

static void usbhsc_power_ctrl(struct usbhs_priv *priv, int enable)
{
	struct platform_device *pdev = usbhs_priv_to_pdev(priv);
	struct device *dev = usbhs_priv_to_dev(priv);

	if (enable) {
		
		pm_runtime_get_sync(dev);

		
		usbhs_platform_call(priv, power_ctrl, pdev, priv->base, enable);

		
		usbhs_sys_clock_ctrl(priv, enable);
	} else {
		
		usbhs_sys_clock_ctrl(priv, enable);

		
		usbhs_platform_call(priv, power_ctrl, pdev, priv->base, enable);

		
		pm_runtime_put_sync(dev);
	}
}

static void usbhsc_hotplug(struct usbhs_priv *priv)
{
	struct platform_device *pdev = usbhs_priv_to_pdev(priv);
	struct usbhs_mod *mod = usbhs_mod_get_current(priv);
	int id;
	int enable;
	int ret;

	enable = usbhs_platform_call(priv, get_vbus, pdev);

	id = usbhs_platform_call(priv, get_id, pdev);

	if (enable && !mod) {
		ret = usbhs_mod_change(priv, id);
		if (ret < 0)
			return;

		dev_dbg(&pdev->dev, "%s enable\n", __func__);

		
		if (usbhsc_flags_has(priv, USBHSF_RUNTIME_PWCTRL))
			usbhsc_power_ctrl(priv, enable);

		
		usbhsc_set_buswait(priv);
		usbhsc_bus_init(priv);

		
		usbhs_mod_call(priv, start, priv);

	} else if (!enable && mod) {
		dev_dbg(&pdev->dev, "%s disable\n", __func__);

		
		usbhs_mod_call(priv, stop, priv);

		
		usbhsc_bus_init(priv);

		
		if (usbhsc_flags_has(priv, USBHSF_RUNTIME_PWCTRL))
			usbhsc_power_ctrl(priv, enable);

		usbhs_mod_change(priv, -1);

		
		usbhs_platform_call(priv, phy_reset, pdev);
	}
}

static void usbhsc_notify_hotplug(struct work_struct *work)
{
	struct usbhs_priv *priv = container_of(work,
					       struct usbhs_priv,
					       notify_hotplug_work.work);
	usbhsc_hotplug(priv);
}

static int usbhsc_drvcllbck_notify_hotplug(struct platform_device *pdev)
{
	struct usbhs_priv *priv = usbhs_pdev_to_priv(pdev);
	int delay = usbhs_get_dparam(priv, detection_delay);

	schedule_delayed_work(&priv->notify_hotplug_work,
			      msecs_to_jiffies(delay));
	return 0;
}

static int usbhs_probe(struct platform_device *pdev)
{
	struct renesas_usbhs_platform_info *info = pdev->dev.platform_data;
	struct renesas_usbhs_driver_callback *dfunc;
	struct usbhs_priv *priv;
	struct resource *res, *irq_res;
	int ret;

	
	if (!info ||
	    !info->platform_callback.get_id) {
		dev_err(&pdev->dev, "no platform information\n");
		return -EINVAL;
	}

	
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	irq_res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (!res || !irq_res) {
		dev_err(&pdev->dev, "Not enough Renesas USB platform resources.\n");
		return -ENODEV;
	}

	
	priv = kzalloc(sizeof(*priv), GFP_KERNEL);
	if (!priv) {
		dev_err(&pdev->dev, "Could not allocate priv\n");
		return -ENOMEM;
	}

	priv->base = ioremap_nocache(res->start, resource_size(res));
	if (!priv->base) {
		dev_err(&pdev->dev, "ioremap error.\n");
		ret = -ENOMEM;
		goto probe_end_kfree;
	}

	memcpy(&priv->pfunc,
	       &info->platform_callback,
	       sizeof(struct renesas_usbhs_platform_callback));
	memcpy(&priv->dparam,
	       &info->driver_param,
	       sizeof(struct renesas_usbhs_driver_param));

	
	dfunc			= &info->driver_callback;
	dfunc->notify_hotplug	= usbhsc_drvcllbck_notify_hotplug;

	
	if (!priv->dparam.pipe_type) {
		priv->dparam.pipe_type = usbhsc_default_pipe_type;
		priv->dparam.pipe_size = ARRAY_SIZE(usbhsc_default_pipe_type);
	}
	if (!priv->dparam.pio_dma_border)
		priv->dparam.pio_dma_border = 64; 

	
	
	if (priv->pfunc.get_vbus)
		usbhsc_flags_set(priv, USBHSF_RUNTIME_PWCTRL);

	priv->irq	= irq_res->start;
	if (irq_res->flags & IORESOURCE_IRQ_SHAREABLE)
		priv->irqflags = IRQF_SHARED;
	priv->pdev	= pdev;
	INIT_DELAYED_WORK(&priv->notify_hotplug_work, usbhsc_notify_hotplug);
	spin_lock_init(usbhs_priv_to_lock(priv));

	
	ret = usbhs_pipe_probe(priv);
	if (ret < 0)
		goto probe_end_iounmap;

	ret = usbhs_fifo_probe(priv);
	if (ret < 0)
		goto probe_end_pipe_exit;

	ret = usbhs_mod_probe(priv);
	if (ret < 0)
		goto probe_end_fifo_exit;

	
	dev_set_drvdata(&pdev->dev, priv);

	usbhs_sys_clock_ctrl(priv, 0);

	ret = usbhs_platform_call(priv, hardware_init, pdev);
	if (ret < 0) {
		dev_err(&pdev->dev, "platform prove failed.\n");
		goto probe_end_mod_exit;
	}

	
	usbhs_platform_call(priv, phy_reset, pdev);

	
	pm_runtime_enable(&pdev->dev);
	if (!usbhsc_flags_has(priv, USBHSF_RUNTIME_PWCTRL)) {
		usbhsc_power_ctrl(priv, 1);
		usbhs_mod_autonomy_mode(priv);
	}

	ret = usbhsc_drvcllbck_notify_hotplug(pdev);
	if (ret < 0)
		goto probe_end_call_remove;

	dev_info(&pdev->dev, "probed\n");

	return ret;

probe_end_call_remove:
	usbhs_platform_call(priv, hardware_exit, pdev);
probe_end_mod_exit:
	usbhs_mod_remove(priv);
probe_end_fifo_exit:
	usbhs_fifo_remove(priv);
probe_end_pipe_exit:
	usbhs_pipe_remove(priv);
probe_end_iounmap:
	iounmap(priv->base);
probe_end_kfree:
	kfree(priv);

	dev_info(&pdev->dev, "probe failed\n");

	return ret;
}

static int __devexit usbhs_remove(struct platform_device *pdev)
{
	struct usbhs_priv *priv = usbhs_pdev_to_priv(pdev);
	struct renesas_usbhs_platform_info *info = pdev->dev.platform_data;
	struct renesas_usbhs_driver_callback *dfunc = &info->driver_callback;

	dev_dbg(&pdev->dev, "usb remove\n");

	dfunc->notify_hotplug = NULL;

	
	if (!usbhsc_flags_has(priv, USBHSF_RUNTIME_PWCTRL))
		usbhsc_power_ctrl(priv, 0);

	pm_runtime_disable(&pdev->dev);

	usbhs_platform_call(priv, hardware_exit, pdev);
	usbhs_mod_remove(priv);
	usbhs_fifo_remove(priv);
	usbhs_pipe_remove(priv);
	iounmap(priv->base);
	kfree(priv);

	return 0;
}

static int usbhsc_suspend(struct device *dev)
{
	struct usbhs_priv *priv = dev_get_drvdata(dev);
	struct usbhs_mod *mod = usbhs_mod_get_current(priv);

	if (mod) {
		usbhs_mod_call(priv, stop, priv);
		usbhs_mod_change(priv, -1);
	}

	if (mod || !usbhsc_flags_has(priv, USBHSF_RUNTIME_PWCTRL))
		usbhsc_power_ctrl(priv, 0);

	return 0;
}

static int usbhsc_resume(struct device *dev)
{
	struct usbhs_priv *priv = dev_get_drvdata(dev);
	struct platform_device *pdev = usbhs_priv_to_pdev(priv);

	usbhs_platform_call(priv, phy_reset, pdev);

	if (!usbhsc_flags_has(priv, USBHSF_RUNTIME_PWCTRL))
		usbhsc_power_ctrl(priv, 1);

	usbhsc_hotplug(priv);

	return 0;
}

static int usbhsc_runtime_nop(struct device *dev)
{
	return 0;
}

static const struct dev_pm_ops usbhsc_pm_ops = {
	.suspend		= usbhsc_suspend,
	.resume			= usbhsc_resume,
	.runtime_suspend	= usbhsc_runtime_nop,
	.runtime_resume		= usbhsc_runtime_nop,
};

static struct platform_driver renesas_usbhs_driver = {
	.driver		= {
		.name	= "renesas_usbhs",
		.pm	= &usbhsc_pm_ops,
	},
	.probe		= usbhs_probe,
	.remove		= __devexit_p(usbhs_remove),
};

module_platform_driver(renesas_usbhs_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Renesas USB driver");
MODULE_AUTHOR("Kuninori Morimoto <kuninori.morimoto.gx@renesas.com>");
