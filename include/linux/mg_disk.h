/*
 *  include/linux/mg_disk.c
 *
 *  Private data for mflash platform driver
 *
 * (c) 2008 mGine Co.,LTD
 * (c) 2008 unsik Kim <donari75@gmail.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */

#ifndef __MG_DISK_H__
#define __MG_DISK_H__

#define MG_DEV_NAME "mg_disk"

#define MG_RST_PIN	"mg_rst"
#define MG_RSTOUT_PIN	"mg_rstout"

#define MG_BOOT_DEV		(1 << 0)
#define MG_STORAGE_DEV		(1 << 1)
#define MG_STORAGE_DEV_SKIP_RST	(1 << 2)

struct mg_drv_data {
	
	u32 use_polling;

	
	u32 dev_attr;

	
	void *host;
};

#endif
