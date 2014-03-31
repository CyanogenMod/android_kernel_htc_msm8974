/*
 * S3C24XX USB 2.0 High-speed USB controller gadget driver
 *
 * Copyright (c) 2010 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * The S3C24XX USB 2.0 high-speed USB controller supports upto 9 endpoints.
 * Each endpoint can be configured as either in or out endpoint. Endpoints
 * can be configured for Bulk or Interrupt transfer mode.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef __LINUX_USB_S3C_HSUDC_H
#define __LINUX_USB_S3C_HSUDC_H

struct s3c24xx_hsudc_platdata {
	unsigned int	epnum;
	void		(*gpio_init)(void);
	void		(*gpio_uninit)(void);
};

#endif	
