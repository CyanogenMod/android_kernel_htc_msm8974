/*********************************************************************
 *                
 * Filename:      litelink.c
 * Version:       1.1
 * Description:   Driver for the Parallax LiteLink dongle
 * Status:        Stable
 * Author:        Dag Brattli <dagb@cs.uit.no>
 * Created at:    Fri May  7 12:50:33 1999
 * Modified at:   Fri Dec 17 09:14:23 1999
 * Modified by:   Dag Brattli <dagb@cs.uit.no>
 * 
 *     Copyright (c) 1999 Dag Brattli, All Rights Reserved.
 *     
 *     This program is free software; you can redistribute it and/or 
 *     modify it under the terms of the GNU General Public License as 
 *     published by the Free Software Foundation; either version 2 of 
 *     the License, or (at your option) any later version.
 * 
 *     This program is distributed in the hope that it will be useful,
 *     but WITHOUT ANY WARRANTY; without even the implied warranty of
 *     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *     GNU General Public License for more details.
 * 
 *     You should have received a copy of the GNU General Public License 
 *     along with this program; if not, write to the Free Software 
 *     Foundation, Inc., 59 Temple Place, Suite 330, Boston, 
 *     MA 02111-1307 USA
 *     
 ********************************************************************/


#include <linux/module.h>
#include <linux/delay.h>
#include <linux/init.h>

#include <net/irda/irda.h>

#include "sir-dev.h"

#define MIN_DELAY 25      
#define MAX_DELAY 10000   

static int litelink_open(struct sir_dev *dev);
static int litelink_close(struct sir_dev *dev);
static int litelink_change_speed(struct sir_dev *dev, unsigned speed);
static int litelink_reset(struct sir_dev *dev);

static unsigned baud_rates[] = { 115200, 57600, 38400, 19200, 9600 };

static struct dongle_driver litelink = {
	.owner		= THIS_MODULE,
	.driver_name	= "Parallax LiteLink",
	.type		= IRDA_LITELINK_DONGLE,
	.open		= litelink_open,
	.close		= litelink_close,
	.reset		= litelink_reset,
	.set_speed	= litelink_change_speed,
};

static int __init litelink_sir_init(void)
{
	return irda_register_dongle(&litelink);
}

static void __exit litelink_sir_cleanup(void)
{
	irda_unregister_dongle(&litelink);
}

static int litelink_open(struct sir_dev *dev)
{
	struct qos_info *qos = &dev->qos;

	IRDA_DEBUG(2, "%s()\n", __func__);

	
	sirdev_set_dtr_rts(dev, TRUE, TRUE);

	
	qos->baud_rate.bits &= IR_115200|IR_57600|IR_38400|IR_19200|IR_9600;
	qos->min_turn_time.bits = 0x7f; 
	irda_qos_bits_to_value(qos);

	

	return 0;
}

static int litelink_close(struct sir_dev *dev)
{
	IRDA_DEBUG(2, "%s()\n", __func__);

	
	sirdev_set_dtr_rts(dev, FALSE, FALSE);

	return 0;
}

static int litelink_change_speed(struct sir_dev *dev, unsigned speed)
{
        int i;

	IRDA_DEBUG(2, "%s()\n", __func__);


	
	for (i = 0; baud_rates[i] != speed; i++) {

		
		if (baud_rates[i] == 9600)
			break;

		
		sirdev_set_dtr_rts(dev, FALSE, TRUE);

		
		udelay(MIN_DELAY);

		
		sirdev_set_dtr_rts(dev, TRUE, TRUE);

		
		udelay(MIN_DELAY);
        }

	dev->speed = baud_rates[i];


	return (dev->speed == speed) ? 0 : -EINVAL;
}

static int litelink_reset(struct sir_dev *dev)
{
	IRDA_DEBUG(2, "%s()\n", __func__);


	
	sirdev_set_dtr_rts(dev, TRUE, TRUE);

	
	udelay(MIN_DELAY);

	
	sirdev_set_dtr_rts(dev, TRUE, FALSE);

	
	udelay(MIN_DELAY);

	
	sirdev_set_dtr_rts(dev, TRUE, TRUE);

	
	udelay(MIN_DELAY);

	
	dev->speed = 115200;

	return 0;
}

MODULE_AUTHOR("Dag Brattli <dagb@cs.uit.no>");
MODULE_DESCRIPTION("Parallax Litelink dongle driver");	
MODULE_LICENSE("GPL");
MODULE_ALIAS("irda-dongle-5"); 

module_init(litelink_sir_init);

module_exit(litelink_sir_cleanup);
