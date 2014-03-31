/*********************************************************************
 *                
 * Filename:      tekram.c
 * Version:       1.3
 * Description:   Implementation of the Tekram IrMate IR-210B dongle
 * Status:        Experimental.
 * Author:        Dag Brattli <dagb@cs.uit.no>
 * Created at:    Wed Oct 21 20:02:35 1998
 * Modified at:   Sun Oct 27 22:02:38 2002
 * Modified by:   Martin Diehl <mad@mdiehl.de>
 * 
 *     Copyright (c) 1998-1999 Dag Brattli,
 *     Copyright (c) 2002 Martin Diehl,
 *     All Rights Reserved.
 *      
 *     This program is free software; you can redistribute it and/or 
 *     modify it under the terms of the GNU General Public License as 
 *     published by the Free Software Foundation; either version 2 of 
 *     the License, or (at your option) any later version.
 *  
 *     Neither Dag Brattli nor University of Tromsø admit liability nor
 *     provide warranty for any of this software. This material is 
 *     provided "AS-IS" and at no charge.
 *     
 ********************************************************************/

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/init.h>

#include <net/irda/irda.h>

#include "sir-dev.h"

static int tekram_delay = 150;		
module_param(tekram_delay, int, 0);
MODULE_PARM_DESC(tekram_delay, "tekram dongle write complete delay");

static int tekram_open(struct sir_dev *);
static int tekram_close(struct sir_dev *);
static int tekram_change_speed(struct sir_dev *, unsigned);
static int tekram_reset(struct sir_dev *);

#define TEKRAM_115200 0x00
#define TEKRAM_57600  0x01
#define TEKRAM_38400  0x02
#define TEKRAM_19200  0x03
#define TEKRAM_9600   0x04

#define TEKRAM_PW     0x10 

static struct dongle_driver tekram = {
	.owner		= THIS_MODULE,
	.driver_name	= "Tekram IR-210B",
	.type		= IRDA_TEKRAM_DONGLE,
	.open		= tekram_open,
	.close		= tekram_close,
	.reset		= tekram_reset,
	.set_speed	= tekram_change_speed,
};

static int __init tekram_sir_init(void)
{
	if (tekram_delay < 1  ||  tekram_delay > 500)
		tekram_delay = 200;
	IRDA_DEBUG(1, "%s - using %d ms delay\n",
		tekram.driver_name, tekram_delay);
	return irda_register_dongle(&tekram);
}

static void __exit tekram_sir_cleanup(void)
{
	irda_unregister_dongle(&tekram);
}

static int tekram_open(struct sir_dev *dev)
{
	struct qos_info *qos = &dev->qos;

	IRDA_DEBUG(2, "%s()\n", __func__);

	sirdev_set_dtr_rts(dev, TRUE, TRUE);

	qos->baud_rate.bits &= IR_9600|IR_19200|IR_38400|IR_57600|IR_115200;
	qos->min_turn_time.bits = 0x01; 	
	irda_qos_bits_to_value(qos);

	

	return 0;
}

static int tekram_close(struct sir_dev *dev)
{
	IRDA_DEBUG(2, "%s()\n", __func__);

	
	sirdev_set_dtr_rts(dev, FALSE, FALSE);

	return 0;
}


#define TEKRAM_STATE_WAIT_SPEED	(SIRDEV_STATE_DONGLE_SPEED + 1)

static int tekram_change_speed(struct sir_dev *dev, unsigned speed)
{
	unsigned state = dev->fsm.substate;
	unsigned delay = 0;
	u8 byte;
	static int ret = 0;
	
	IRDA_DEBUG(2, "%s()\n", __func__);

	switch(state) {
	case SIRDEV_STATE_DONGLE_SPEED:

		switch (speed) {
		default:
			speed = 9600;
			ret = -EINVAL;
			
		case 9600:
			byte = TEKRAM_PW|TEKRAM_9600;
			break;
		case 19200:
			byte = TEKRAM_PW|TEKRAM_19200;
			break;
		case 38400:
			byte = TEKRAM_PW|TEKRAM_38400;
			break;
		case 57600:
			byte = TEKRAM_PW|TEKRAM_57600;
			break;
		case 115200:
			byte = TEKRAM_115200;
			break;
		}

		
		sirdev_set_dtr_rts(dev, TRUE, FALSE);
	
		
		udelay(14);

		
		sirdev_raw_write(dev, &byte, 1);
		
		dev->speed = speed;

		state = TEKRAM_STATE_WAIT_SPEED;
		delay = tekram_delay;
		break;

	case TEKRAM_STATE_WAIT_SPEED:
		
		sirdev_set_dtr_rts(dev, TRUE, TRUE);
		udelay(50);
		break;

	default:
		IRDA_ERROR("%s - undefined state %d\n", __func__, state);
		ret = -EINVAL;
		break;
	}

	dev->fsm.substate = state;
	return (delay > 0) ? delay : ret;
}


static int tekram_reset(struct sir_dev *dev)
{
	IRDA_DEBUG(2, "%s()\n", __func__);

	
	sirdev_set_dtr_rts(dev, FALSE, TRUE); 

	
	msleep(1);

	
	sirdev_set_dtr_rts(dev, TRUE, TRUE);
	
	
	udelay(75);

	dev->speed = 9600;

	return 0;
}

MODULE_AUTHOR("Dag Brattli <dagb@cs.uit.no>");
MODULE_DESCRIPTION("Tekram IrMate IR-210B dongle driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("irda-dongle-0"); 
		
module_init(tekram_sir_init);
module_exit(tekram_sir_cleanup);
