/*
 * ds.h -- 16-bit PCMCIA core support
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * The initial developer of the original code is David A. Hinds
 * <dahinds@users.sourceforge.net>.  Portions created by David A. Hinds
 * are Copyright (C) 1999 David A. Hinds.  All Rights Reserved.
 *
 * (C) 1999		David A. Hinds
 * (C) 2003 - 2008	Dominik Brodowski
 */

#ifndef _LINUX_DS_H
#define _LINUX_DS_H

#ifdef __KERNEL__
#include <linux/mod_devicetable.h>
#endif

#include <pcmcia/device_id.h>

#ifdef __KERNEL__
#include <linux/device.h>
#include <linux/interrupt.h>
#include <pcmcia/ss.h>
#include <linux/atomic.h>


struct pcmcia_socket;
struct pcmcia_device;
struct config_t;
struct net_device;

struct pcmcia_dynids {
	struct mutex		lock;
	struct list_head	list;
};

struct pcmcia_driver {
	const char		*name;

	int (*probe)		(struct pcmcia_device *dev);
	void (*remove)		(struct pcmcia_device *dev);

	int (*suspend)		(struct pcmcia_device *dev);
	int (*resume)		(struct pcmcia_device *dev);

	struct module		*owner;
	const struct pcmcia_device_id	*id_table;
	struct device_driver	drv;
	struct pcmcia_dynids	dynids;
};

int pcmcia_register_driver(struct pcmcia_driver *driver);
void pcmcia_unregister_driver(struct pcmcia_driver *driver);

enum {
	PCMCIA_IOPORT_0,
	PCMCIA_IOPORT_1,
	PCMCIA_IOMEM_0,
	PCMCIA_IOMEM_1,
	PCMCIA_IOMEM_2,
	PCMCIA_IOMEM_3,
	PCMCIA_NUM_RESOURCES,
};

struct pcmcia_device {
	struct pcmcia_socket	*socket;

	char			*devname;

	u8			device_no;

	u8			func;
	struct config_t		*function_config;

	struct list_head	socket_device_list;

	
	unsigned int		irq;
	struct resource		*resource[PCMCIA_NUM_RESOURCES];
	resource_size_t		card_addr;	
	unsigned int		vpp;

	unsigned int		config_flags;	
	unsigned int		config_base;
	unsigned int		config_index;
	unsigned int		config_regs;	
	unsigned int		io_lines;	

	
	u16			suspended:1;

	u16			_irq:1;
	u16			_io:1;
	u16			_win:4;
	u16			_locked:1;

	u16			allow_func_id_match:1;

	
	u16			has_manf_id:1;
	u16			has_card_id:1;
	u16			has_func_id:1;

	u16			reserved:4;

	u8			func_id;
	u16			manf_id;
	u16			card_id;

	char			*prod_id[4];

	u64			dma_mask;
	struct device		dev;

	
	void			*priv;
	unsigned int		open;
};

#define to_pcmcia_dev(n) container_of(n, struct pcmcia_device, dev)
#define to_pcmcia_drv(n) container_of(n, struct pcmcia_driver, drv)



size_t pcmcia_get_tuple(struct pcmcia_device *p_dev, cisdata_t code,
			u8 **buf);

int pcmcia_loop_tuple(struct pcmcia_device *p_dev, cisdata_t code,
		      int (*loop_tuple) (struct pcmcia_device *p_dev,
					 tuple_t *tuple,
					 void *priv_data),
		      void *priv_data);

int pcmcia_get_mac_from_cis(struct pcmcia_device *p_dev,
			    struct net_device *dev);


int pcmcia_parse_tuple(tuple_t *tuple, cisparse_t *parse);

int pcmcia_loop_config(struct pcmcia_device *p_dev,
		       int	(*conf_check)	(struct pcmcia_device *p_dev,
						 void *priv_data),
		       void *priv_data);

struct pcmcia_device *pcmcia_dev_present(struct pcmcia_device *p_dev);

int pcmcia_reset_card(struct pcmcia_socket *skt);

int pcmcia_read_config_byte(struct pcmcia_device *p_dev, off_t where, u8 *val);
int pcmcia_write_config_byte(struct pcmcia_device *p_dev, off_t where, u8 val);

int pcmcia_request_io(struct pcmcia_device *p_dev);

int __must_check
__pcmcia_request_exclusive_irq(struct pcmcia_device *p_dev,
				irq_handler_t handler);
static inline __must_check __deprecated int
pcmcia_request_exclusive_irq(struct pcmcia_device *p_dev,
				irq_handler_t handler)
{
	return __pcmcia_request_exclusive_irq(p_dev, handler);
}

int __must_check pcmcia_request_irq(struct pcmcia_device *p_dev,
				irq_handler_t handler);

int pcmcia_enable_device(struct pcmcia_device *p_dev);

int pcmcia_request_window(struct pcmcia_device *p_dev, struct resource *res,
			unsigned int speed);
int pcmcia_release_window(struct pcmcia_device *p_dev, struct resource *res);
int pcmcia_map_mem_page(struct pcmcia_device *p_dev, struct resource *res,
			unsigned int offset);

int pcmcia_fixup_vpp(struct pcmcia_device *p_dev, unsigned char new_vpp);
int pcmcia_fixup_iowidth(struct pcmcia_device *p_dev);

void pcmcia_disable_device(struct pcmcia_device *p_dev);

#define IO_DATA_PATH_WIDTH	0x18
#define IO_DATA_PATH_WIDTH_8	0x00
#define IO_DATA_PATH_WIDTH_16	0x08
#define IO_DATA_PATH_WIDTH_AUTO	0x10

#define WIN_MEMORY_TYPE_CM	0x00 
#define WIN_MEMORY_TYPE_AM	0x20 
#define WIN_DATA_WIDTH_8	0x00 
#define WIN_DATA_WIDTH_16	0x02 
#define WIN_ENABLE		0x01 
#define WIN_USE_WAIT		0x40 

#define WIN_FLAGS_MAP		0x63 
#define WIN_FLAGS_REQ		0x1c 

#define PRESENT_OPTION		0x001
#define PRESENT_STATUS		0x002
#define PRESENT_PIN_REPLACE	0x004
#define PRESENT_COPY		0x008
#define PRESENT_EXT_STATUS	0x010
#define PRESENT_IOBASE_0	0x020
#define PRESENT_IOBASE_1	0x040
#define PRESENT_IOBASE_2	0x080
#define PRESENT_IOBASE_3	0x100
#define PRESENT_IOSIZE		0x200

#define CONF_ENABLE_IRQ         0x0001
#define CONF_ENABLE_SPKR        0x0002
#define CONF_ENABLE_PULSE_IRQ   0x0004
#define CONF_ENABLE_ESR         0x0008
#define CONF_ENABLE_IOCARD	0x0010 
#define CONF_ENABLE_ZVCARD	0x0020

#define CONF_AUTO_CHECK_VCC	0x0100 
#define CONF_AUTO_SET_VPP	0x0200 
#define CONF_AUTO_AUDIO		0x0400 
#define CONF_AUTO_SET_IO	0x0800 
#define CONF_AUTO_SET_IOMEM	0x1000 

#endif 

#endif 
