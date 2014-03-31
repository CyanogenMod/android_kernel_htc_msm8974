/*********************************************************************
 *
 * Filename:      toim3232-sir.c
 * Version:       1.0
 * Description:   Implementation of dongles based on the Vishay/Temic
 * 		  TOIM3232 SIR Endec chipset. Currently only the
 * 		  IRWave IR320ST-2 is tested, although it should work
 * 		  with any TOIM3232 or TOIM4232 chipset based RS232
 * 		  dongle with minimal modification.
 * 		  Based heavily on the Tekram driver (tekram.c),
 * 		  with thanks to Dag Brattli and Martin Diehl.
 * Status:        Experimental.
 * Author:        David Basden <davidb-irda@rcpt.to>
 * Created at:    Thu Feb 09 23:47:32 2006
 *
 *     Copyright (c) 2006 David Basden.
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
#include <linux/sched.h>

#include <net/irda/irda.h>

#include "sir-dev.h"

static int toim3232delay = 150;	
module_param(toim3232delay, int, 0);
MODULE_PARM_DESC(toim3232delay, "toim3232 dongle write complete delay");

#if 0
static int toim3232flipdtr = 0;	
module_param(toim3232flipdtr, int, 0);
MODULE_PARM_DESC(toim3232flipdtr, "toim3232 dongle invert DTR (Reset)");

static int toim3232fliprts = 0;	
module_param(toim3232fliptrs, int, 0);
MODULE_PARM_DESC(toim3232fliprts, "toim3232 dongle invert RTS (BR/D)");
#endif

static int toim3232_open(struct sir_dev *);
static int toim3232_close(struct sir_dev *);
static int toim3232_change_speed(struct sir_dev *, unsigned);
static int toim3232_reset(struct sir_dev *);

#define TOIM3232_115200 0x00
#define TOIM3232_57600  0x01
#define TOIM3232_38400  0x02
#define TOIM3232_19200  0x03
#define TOIM3232_9600   0x06
#define TOIM3232_2400   0x0A

#define TOIM3232_PW     0x10 

static struct dongle_driver toim3232 = {
	.owner		= THIS_MODULE,
	.driver_name	= "Vishay TOIM3232",
	.type		= IRDA_TOIM3232_DONGLE,
	.open		= toim3232_open,
	.close		= toim3232_close,
	.reset		= toim3232_reset,
	.set_speed	= toim3232_change_speed,
};

static int __init toim3232_sir_init(void)
{
	if (toim3232delay < 1  ||  toim3232delay > 500)
		toim3232delay = 200;
	IRDA_DEBUG(1, "%s - using %d ms delay\n",
		toim3232.driver_name, toim3232delay);
	return irda_register_dongle(&toim3232);
}

static void __exit toim3232_sir_cleanup(void)
{
	irda_unregister_dongle(&toim3232);
}

static int toim3232_open(struct sir_dev *dev)
{
	struct qos_info *qos = &dev->qos;

	IRDA_DEBUG(2, "%s()\n", __func__);

	sirdev_set_dtr_rts(dev, TRUE, TRUE);

	qos->baud_rate.bits &= IR_2400|IR_9600|IR_19200|IR_38400|IR_57600|IR_115200;

	
	qos->min_turn_time.bits = 0x01; 
	irda_qos_bits_to_value(qos);

	

	return 0;
}

static int toim3232_close(struct sir_dev *dev)
{
	IRDA_DEBUG(2, "%s()\n", __func__);

	
	sirdev_set_dtr_rts(dev, FALSE, FALSE);

	return 0;
}


#define TOIM3232_STATE_WAIT_SPEED	(SIRDEV_STATE_DONGLE_SPEED + 1)

static int toim3232_change_speed(struct sir_dev *dev, unsigned speed)
{
	unsigned state = dev->fsm.substate;
	unsigned delay = 0;
	u8 byte;
	static int ret = 0;

	IRDA_DEBUG(2, "%s()\n", __func__);

	switch(state) {
	case SIRDEV_STATE_DONGLE_SPEED:

		
		switch (speed) {
		case 2400:
			byte = TOIM3232_PW|TOIM3232_2400;
			break;
		default:
			speed = 9600;
			ret = -EINVAL;
			
		case 9600:
			byte = TOIM3232_PW|TOIM3232_9600;
			break;
		case 19200:
			byte = TOIM3232_PW|TOIM3232_19200;
			break;
		case 38400:
			byte = TOIM3232_PW|TOIM3232_38400;
			break;
		case 57600:
			byte = TOIM3232_PW|TOIM3232_57600;
			break;
		case 115200:
			byte = TOIM3232_115200;
			break;
		}

		
		sirdev_set_dtr_rts(dev, TRUE, FALSE);

		
		udelay(14);

		
		sirdev_raw_write(dev, &byte, 1);

		dev->speed = speed;

		state = TOIM3232_STATE_WAIT_SPEED;
		delay = toim3232delay;
		break;

	case TOIM3232_STATE_WAIT_SPEED:
		
		udelay(14);

		
		sirdev_set_dtr_rts(dev, TRUE, TRUE);

		
		udelay(50);
		break;

	default:
		printk(KERN_ERR "%s - undefined state %d\n", __func__, state);
		ret = -EINVAL;
		break;
	}

	dev->fsm.substate = state;
	return (delay > 0) ? delay : ret;
}


static int toim3232_reset(struct sir_dev *dev)
{
	IRDA_DEBUG(2, "%s()\n", __func__);

	
	sirdev_set_dtr_rts(dev, FALSE, FALSE);

	
	set_current_state(TASK_UNINTERRUPTIBLE);
	schedule_timeout(msecs_to_jiffies(50));

	
	sirdev_set_dtr_rts(dev, TRUE, TRUE);

	
	set_current_state(TASK_UNINTERRUPTIBLE);
	schedule_timeout(msecs_to_jiffies(10));

	
	dev->speed = 9600;

	return 0;
}

MODULE_AUTHOR("David Basden <davidb-linux@rcpt.to>");
MODULE_DESCRIPTION("Vishay/Temic TOIM3232 based dongle driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("irda-dongle-12"); 

module_init(toim3232_sir_init);
module_exit(toim3232_sir_cleanup);
