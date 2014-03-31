/*
 * HSI core header file.
 *
 * Copyright (C) 2010 Nokia Corporation. All rights reserved.
 *
 * Contact: Carlos Chinea <carlos.chinea@nokia.com>
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
 */

#ifndef __LINUX_HSI_H__
#define __LINUX_HSI_H__

#include <linux/device.h>
#include <linux/mutex.h>
#include <linux/scatterlist.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/notifier.h>

#define HSI_MSG_READ	0
#define HSI_MSG_WRITE	1

enum {
	HSI_MODE_STREAM	= 1,
	HSI_MODE_FRAME,
};

enum {
	HSI_FLOW_SYNC,	
	HSI_FLOW_PIPE,	
};

enum {
	HSI_ARB_RR,	
	HSI_ARB_PRIO,	
};

#define HSI_MAX_CHANNELS	16

enum {
	HSI_STATUS_COMPLETED,	
	HSI_STATUS_PENDING,	
	HSI_STATUS_PROCEEDING,	
	HSI_STATUS_QUEUED,	
	HSI_STATUS_ERROR,	
};

enum {
	HSI_EVENT_START_RX,
	HSI_EVENT_STOP_RX,
};

struct hsi_config {
	unsigned int	mode;
	unsigned int	channels;
	unsigned int	speed;
	union {
		unsigned int	flow;		
		unsigned int	arb_mode;	
	};
};

struct hsi_board_info {
	const char		*name;
	unsigned int		hsi_id;
	unsigned int		port;
	struct hsi_config	tx_cfg;
	struct hsi_config	rx_cfg;
	void			*platform_data;
	struct dev_archdata	*archdata;
};

#ifdef CONFIG_HSI_BOARDINFO
extern int hsi_register_board_info(struct hsi_board_info const *info,
							unsigned int len);
#else
static inline int hsi_register_board_info(struct hsi_board_info const *info,
							unsigned int len)
{
	return 0;
}
#endif 

struct hsi_client {
	struct device		device;
	struct hsi_config	tx_cfg;
	struct hsi_config	rx_cfg;
	
	void			(*ehandler)(struct hsi_client *, unsigned long);
	unsigned int		pclaimed:1;
	struct notifier_block	nb;
};

#define to_hsi_client(dev) container_of(dev, struct hsi_client, device)

static inline void hsi_client_set_drvdata(struct hsi_client *cl, void *data)
{
	dev_set_drvdata(&cl->device, data);
}

static inline void *hsi_client_drvdata(struct hsi_client *cl)
{
	return dev_get_drvdata(&cl->device);
}

int hsi_register_port_event(struct hsi_client *cl,
			void (*handler)(struct hsi_client *, unsigned long));
int hsi_unregister_port_event(struct hsi_client *cl);

struct hsi_client_driver {
	struct device_driver	driver;
};

#define to_hsi_client_driver(drv) container_of(drv, struct hsi_client_driver,\
									driver)

int hsi_register_client_driver(struct hsi_client_driver *drv);

static inline void hsi_unregister_client_driver(struct hsi_client_driver *drv)
{
	driver_unregister(&drv->driver);
}

struct hsi_msg {
	struct list_head	link;
	struct hsi_client	*cl;
	struct sg_table		sgt;
	void			*context;

	void			(*complete)(struct hsi_msg *msg);
	void			(*destructor)(struct hsi_msg *msg);

	int			status;
	unsigned int		actual_len;
	unsigned int		channel;
	unsigned int		ttype:1;
	unsigned int		break_frame:1;
};

struct hsi_msg *hsi_alloc_msg(unsigned int n_frag, gfp_t flags);
void hsi_free_msg(struct hsi_msg *msg);

struct hsi_port {
	struct device			device;
	struct hsi_config		tx_cfg;
	struct hsi_config		rx_cfg;
	unsigned int			num;
	unsigned int			shared:1;
	int				claimed;
	struct mutex			lock;
	int				(*async)(struct hsi_msg *msg);
	int				(*setup)(struct hsi_client *cl);
	int				(*flush)(struct hsi_client *cl);
	int				(*start_tx)(struct hsi_client *cl);
	int				(*stop_tx)(struct hsi_client *cl);
	int				(*release)(struct hsi_client *cl);
	
	struct atomic_notifier_head	n_head;
};

#define to_hsi_port(dev) container_of(dev, struct hsi_port, device)
#define hsi_get_port(cl) to_hsi_port((cl)->device.parent)

int hsi_event(struct hsi_port *port, unsigned long event);
int hsi_claim_port(struct hsi_client *cl, unsigned int share);
void hsi_release_port(struct hsi_client *cl);

static inline int hsi_port_claimed(struct hsi_client *cl)
{
	return cl->pclaimed;
}

static inline void hsi_port_set_drvdata(struct hsi_port *port, void *data)
{
	dev_set_drvdata(&port->device, data);
}

static inline void *hsi_port_drvdata(struct hsi_port *port)
{
	return dev_get_drvdata(&port->device);
}

struct hsi_controller {
	struct device		device;
	struct module		*owner;
	unsigned int		id;
	unsigned int		num_ports;
	struct hsi_port		**port;
};

#define to_hsi_controller(dev) container_of(dev, struct hsi_controller, device)

struct hsi_controller *hsi_alloc_controller(unsigned int n_ports, gfp_t flags);
void hsi_put_controller(struct hsi_controller *hsi);
int hsi_register_controller(struct hsi_controller *hsi);
void hsi_unregister_controller(struct hsi_controller *hsi);

static inline void hsi_controller_set_drvdata(struct hsi_controller *hsi,
								void *data)
{
	dev_set_drvdata(&hsi->device, data);
}

static inline void *hsi_controller_drvdata(struct hsi_controller *hsi)
{
	return dev_get_drvdata(&hsi->device);
}

static inline struct hsi_port *hsi_find_port_num(struct hsi_controller *hsi,
							unsigned int num)
{
	return (num < hsi->num_ports) ? hsi->port[num] : NULL;
}

int hsi_async(struct hsi_client *cl, struct hsi_msg *msg);

static inline unsigned int hsi_id(struct hsi_client *cl)
{
	return	to_hsi_controller(cl->device.parent->parent)->id;
}

static inline unsigned int hsi_port_id(struct hsi_client *cl)
{
	return	to_hsi_port(cl->device.parent)->num;
}

static inline int hsi_setup(struct hsi_client *cl)
{
	if (!hsi_port_claimed(cl))
		return -EACCES;
	return	hsi_get_port(cl)->setup(cl);
}

static inline int hsi_flush(struct hsi_client *cl)
{
	if (!hsi_port_claimed(cl))
		return -EACCES;
	return hsi_get_port(cl)->flush(cl);
}

static inline int hsi_async_read(struct hsi_client *cl, struct hsi_msg *msg)
{
	msg->ttype = HSI_MSG_READ;
	return hsi_async(cl, msg);
}

static inline int hsi_async_write(struct hsi_client *cl, struct hsi_msg *msg)
{
	msg->ttype = HSI_MSG_WRITE;
	return hsi_async(cl, msg);
}

static inline int hsi_start_tx(struct hsi_client *cl)
{
	if (!hsi_port_claimed(cl))
		return -EACCES;
	return hsi_get_port(cl)->start_tx(cl);
}

static inline int hsi_stop_tx(struct hsi_client *cl)
{
	if (!hsi_port_claimed(cl))
		return -EACCES;
	return hsi_get_port(cl)->stop_tx(cl);
}
#endif 
