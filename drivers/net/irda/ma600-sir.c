/*********************************************************************
 *                
 * Filename:      ma600.c
 * Version:       0.1
 * Description:   Implementation of the MA600 dongle
 * Status:        Experimental.
 * Author:        Leung <95Etwl@alumni.ee.ust.hk> http://www.engsvr.ust/~eetwl95
 * Created at:    Sat Jun 10 20:02:35 2000
 * Modified at:   Sat Aug 16 09:34:13 2003
 * Modified by:   Martin Diehl <mad@mdiehl.de> (modified for new sir_dev)
 *
 * Note: very thanks to Mr. Maru Wang <maru@mobileaction.com.tw> for providing 
 *       information on the MA600 dongle
 * 
 *     Copyright (c) 2000 Leung, All Rights Reserved.
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

static int ma600_open(struct sir_dev *);
static int ma600_close(struct sir_dev *);
static int ma600_change_speed(struct sir_dev *, unsigned);
static int ma600_reset(struct sir_dev *);

#define MA600_9600	0x00
#define MA600_19200	0x01
#define MA600_38400	0x02
#define MA600_57600	0x03
#define MA600_115200	0x04
#define MA600_DEV_ID1	0x05
#define MA600_DEV_ID2	0x06
#define MA600_2400	0x08

static struct dongle_driver ma600 = {
	.owner          = THIS_MODULE,
	.driver_name    = "MA600",
	.type           = IRDA_MA600_DONGLE,
	.open           = ma600_open,
	.close          = ma600_close,
	.reset          = ma600_reset,
	.set_speed      = ma600_change_speed,
};


static int __init ma600_sir_init(void)
{
	IRDA_DEBUG(2, "%s()\n", __func__);
	return irda_register_dongle(&ma600);
}

static void __exit ma600_sir_cleanup(void)
{
	IRDA_DEBUG(2, "%s()\n", __func__);
	irda_unregister_dongle(&ma600);
}

static int ma600_open(struct sir_dev *dev)
{
	struct qos_info *qos = &dev->qos;

	IRDA_DEBUG(2, "%s()\n", __func__);

	sirdev_set_dtr_rts(dev, TRUE, TRUE);

	
	qos->baud_rate.bits &= IR_2400|IR_9600|IR_19200|IR_38400
				|IR_57600|IR_115200;
	
	qos->min_turn_time.bits = 0x01;			
	irda_qos_bits_to_value(qos);

	

	return 0;
}

static int ma600_close(struct sir_dev *dev)
{
	IRDA_DEBUG(2, "%s()\n", __func__);

	
	sirdev_set_dtr_rts(dev, FALSE, FALSE);

	return 0;
}

static __u8 get_control_byte(__u32 speed)
{
	__u8 byte;

	switch (speed) {
	default:
	case 115200:
		byte = MA600_115200;
		break;
	case 57600:
		byte = MA600_57600;
		break;
	case 38400:
		byte = MA600_38400;
		break;
	case 19200:
		byte = MA600_19200;
		break;
	case 9600:
		byte = MA600_9600;
		break;
	case 2400:
		byte = MA600_2400;
		break;
	}

	return byte;
}




static int ma600_change_speed(struct sir_dev *dev, unsigned speed)
{
	u8	byte;
	
	IRDA_DEBUG(2, "%s(), speed=%d (was %d)\n", __func__,
		speed, dev->speed);

	

	
	sirdev_set_dtr_rts(dev, TRUE, FALSE);
	mdelay(1);

	
	byte = get_control_byte(speed);
	sirdev_raw_write(dev, &byte, sizeof(byte));

	
	msleep(15);					

#if 1

	sirdev_raw_read(dev, &byte, sizeof(byte));
	if (byte != get_control_byte(speed))  {
		IRDA_WARNING("%s(): bad control byte read-back %02x != %02x\n",
			     __func__, (unsigned) byte,
			     (unsigned) get_control_byte(speed));
		return -1;
	}
	else
		IRDA_DEBUG(2, "%s() control byte write read OK\n", __func__);
#endif

	
	sirdev_set_dtr_rts(dev, TRUE, TRUE);

	
	msleep(10);

	
	dev->speed = speed;

	return 0;
}



static int ma600_reset(struct sir_dev *dev)
{
	IRDA_DEBUG(2, "%s()\n", __func__);

	
	sirdev_set_dtr_rts(dev, FALSE, TRUE);
	msleep(10);

	
	sirdev_set_dtr_rts(dev, TRUE, TRUE);
	msleep(10);

	dev->speed = 9600;      

	return 0;
}

MODULE_AUTHOR("Leung <95Etwl@alumni.ee.ust.hk> http://www.engsvr.ust/~eetwl95");
MODULE_DESCRIPTION("MA600 dongle driver version 0.1");
MODULE_LICENSE("GPL");
MODULE_ALIAS("irda-dongle-11"); 
		
module_init(ma600_sir_init);
module_exit(ma600_sir_cleanup);

