/*********************************************************************
 *                
 * Filename:      actisys.c
 * Version:       1.1
 * Description:   Implementation for the ACTiSYS IR-220L and IR-220L+ 
 *                dongles
 * Status:        Beta.
 * Authors:       Dag Brattli <dagb@cs.uit.no> (initially)
 *		  Jean Tourrilhes <jt@hpl.hp.com> (new version)
 *		  Martin Diehl <mad@mdiehl.de> (new version for sir_dev)
 * Created at:    Wed Oct 21 20:02:35 1998
 * Modified at:   Sun Oct 27 22:02:13 2002
 * Modified by:   Martin Diehl <mad@mdiehl.de>
 * 
 *     Copyright (c) 1998-1999 Dag Brattli, All Rights Reserved.
 *     Copyright (c) 1999 Jean Tourrilhes
 *     Copyright (c) 2002 Martin Diehl
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

#define MIN_DELAY 10	

static int actisys_open(struct sir_dev *);
static int actisys_close(struct sir_dev *);
static int actisys_change_speed(struct sir_dev *, unsigned);
static int actisys_reset(struct sir_dev *);

static unsigned baud_rates[] = { 9600, 19200, 57600, 115200, 38400 };

#define MAX_SPEEDS ARRAY_SIZE(baud_rates)

static struct dongle_driver act220l = {
	.owner		= THIS_MODULE,
	.driver_name	= "Actisys ACT-220L",
	.type		= IRDA_ACTISYS_DONGLE,
	.open		= actisys_open,
	.close		= actisys_close,
	.reset		= actisys_reset,
	.set_speed	= actisys_change_speed,
};

static struct dongle_driver act220l_plus = {
	.owner		= THIS_MODULE,
	.driver_name	= "Actisys ACT-220L+",
	.type		= IRDA_ACTISYS_PLUS_DONGLE,
	.open		= actisys_open,
	.close		= actisys_close,
	.reset		= actisys_reset,
	.set_speed	= actisys_change_speed,
};

static int __init actisys_sir_init(void)
{
	int ret;

	
	ret = irda_register_dongle(&act220l);
	if (ret < 0)
		return ret;

	
	ret = irda_register_dongle(&act220l_plus);
	if (ret < 0) {
		irda_unregister_dongle(&act220l);
		return ret;
	}
	return 0;
}

static void __exit actisys_sir_cleanup(void)
{
	
	irda_unregister_dongle(&act220l_plus);
	irda_unregister_dongle(&act220l);
}

static int actisys_open(struct sir_dev *dev)
{
	struct qos_info *qos = &dev->qos;

	sirdev_set_dtr_rts(dev, TRUE, TRUE);

	
	qos->baud_rate.bits &= IR_9600|IR_19200|IR_38400|IR_57600|IR_115200;

	
	if (dev->dongle_drv->type == IRDA_ACTISYS_DONGLE)
		qos->baud_rate.bits &= ~IR_38400;

	qos->min_turn_time.bits = 0x7f; 
	irda_qos_bits_to_value(qos);

	

	return 0;
}

static int actisys_close(struct sir_dev *dev)
{
	
	sirdev_set_dtr_rts(dev, FALSE, FALSE);

	return 0;
}

static int actisys_change_speed(struct sir_dev *dev, unsigned speed)
{
	int ret = 0;
	int i = 0;

        IRDA_DEBUG(4, "%s(), speed=%d (was %d)\n", __func__,
        	speed, dev->speed);


	for (i = 0; i < MAX_SPEEDS; i++) {
		if (speed == baud_rates[i]) {
			dev->speed = speed;
			break;
		}
		
		sirdev_set_dtr_rts(dev, TRUE, FALSE);
		udelay(MIN_DELAY);

		
		sirdev_set_dtr_rts(dev, TRUE, TRUE);
		udelay(MIN_DELAY);
	}

	
	if (i >= MAX_SPEEDS) {
		actisys_reset(dev);
		ret = -EINVAL;  
	}

	
	return ret;
}


static int actisys_reset(struct sir_dev *dev)
{
	
	sirdev_set_dtr_rts(dev, FALSE, TRUE);
	udelay(MIN_DELAY);

	
	sirdev_set_dtr_rts(dev, TRUE, TRUE);
	
	dev->speed = 9600;	

	return 0;
}

MODULE_AUTHOR("Dag Brattli <dagb@cs.uit.no> - Jean Tourrilhes <jt@hpl.hp.com>");
MODULE_DESCRIPTION("ACTiSYS IR-220L and IR-220L+ dongle driver");	
MODULE_LICENSE("GPL");
MODULE_ALIAS("irda-dongle-2"); 
MODULE_ALIAS("irda-dongle-3"); 

module_init(actisys_sir_init);
module_exit(actisys_sir_cleanup);
