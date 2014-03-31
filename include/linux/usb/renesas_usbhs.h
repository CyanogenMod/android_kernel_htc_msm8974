/*
 * Renesas USB
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
#ifndef RENESAS_USB_H
#define RENESAS_USB_H
#include <linux/platform_device.h>
#include <linux/usb/ch9.h>

enum {
	USBHS_HOST = 0,
	USBHS_GADGET,
	USBHS_MAX,
};

struct renesas_usbhs_driver_callback {
	int (*notify_hotplug)(struct platform_device *pdev);
};

struct renesas_usbhs_platform_callback {

	int (*hardware_init)(struct platform_device *pdev);

	void (*hardware_exit)(struct platform_device *pdev);

	void (*power_ctrl)(struct platform_device *pdev,
			   void __iomem *base, int enable);

	void (*phy_reset)(struct platform_device *pdev);

	int (*get_id)(struct platform_device *pdev);

	int (*get_vbus)(struct platform_device *pdev);

	int (*set_vbus)(struct platform_device *pdev, int enable);
};

struct renesas_usbhs_driver_param {
	u32 *pipe_type; 
	int pipe_size; 

	int buswait_bwait;

	int detection_delay; 

	int d0_tx_id;
	int d0_rx_id;
	int d1_tx_id;
	int d1_rx_id;

	int pio_dma_border; 

	u32 has_otg:1; 
	u32 has_sudmac:1; 
};

struct renesas_usbhs_platform_info {
	struct renesas_usbhs_platform_callback	platform_callback;

	struct renesas_usbhs_driver_callback	driver_callback;

	struct renesas_usbhs_driver_param	driver_param;
};

#define renesas_usbhs_get_info(pdev)\
	((struct renesas_usbhs_platform_info *)(pdev)->dev.platform_data)

#define renesas_usbhs_call_notify_hotplug(pdev)				\
	({								\
		struct renesas_usbhs_driver_callback *dc;		\
		dc = &(renesas_usbhs_get_info(pdev)->driver_callback);	\
		if (dc && dc->notify_hotplug)				\
			dc->notify_hotplug(pdev);			\
	})
#endif 
