/*
 * dvb_ca.h: generic DVB functions for EN50221 CA interfaces
 *
 * Copyright (C) 2004 Andrew de Quincey
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#ifndef _DVB_CA_EN50221_H_
#define _DVB_CA_EN50221_H_

#include <linux/list.h>
#include <linux/dvb/ca.h>

#include "dvbdev.h"

#define DVB_CA_EN50221_POLL_CAM_PRESENT	1
#define DVB_CA_EN50221_POLL_CAM_CHANGED	2
#define DVB_CA_EN50221_POLL_CAM_READY		4

#define DVB_CA_EN50221_FLAG_IRQ_CAMCHANGE	1
#define DVB_CA_EN50221_FLAG_IRQ_FR		2
#define DVB_CA_EN50221_FLAG_IRQ_DA		4

#define DVB_CA_EN50221_CAMCHANGE_REMOVED		0
#define DVB_CA_EN50221_CAMCHANGE_INSERTED		1



struct dvb_ca_en50221 {

	
	struct module* owner;


	
	int (*read_attribute_mem)(struct dvb_ca_en50221* ca, int slot, int address);
	int (*write_attribute_mem)(struct dvb_ca_en50221* ca, int slot, int address, u8 value);

	
	int (*read_cam_control)(struct dvb_ca_en50221* ca, int slot, u8 address);
	int (*write_cam_control)(struct dvb_ca_en50221* ca, int slot, u8 address, u8 value);

	
	int (*slot_reset)(struct dvb_ca_en50221* ca, int slot);
	int (*slot_shutdown)(struct dvb_ca_en50221* ca, int slot);
	int (*slot_ts_enable)(struct dvb_ca_en50221* ca, int slot);

	int (*poll_slot_status)(struct dvb_ca_en50221* ca, int slot, int open);

	
	void* data;

	
	void* private;
};





void dvb_ca_en50221_camchange_irq(struct dvb_ca_en50221* pubca, int slot, int change_type);

void dvb_ca_en50221_camready_irq(struct dvb_ca_en50221* pubca, int slot);

void dvb_ca_en50221_frda_irq(struct dvb_ca_en50221* ca, int slot);




extern int dvb_ca_en50221_init(struct dvb_adapter *dvb_adapter, struct dvb_ca_en50221* ca, int flags, int slot_count);

extern void dvb_ca_en50221_release(struct dvb_ca_en50221* ca);



#endif
