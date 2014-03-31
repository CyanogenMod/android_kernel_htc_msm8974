/*****************************************************************************
 *
 * Filename:      irda-usb.h
 * Version:       0.10
 * Description:   IrDA-USB Driver
 * Status:        Experimental 
 * Author:        Dag Brattli <dag@brattli.net>
 *
 *	Copyright (C) 2001, Roman Weissgaerber <weissg@vienna.at>
 *      Copyright (C) 2000, Dag Brattli <dag@brattli.net>
 *      Copyright (C) 2001, Jean Tourrilhes <jt@hpl.hp.com>
 *      Copyright (C) 2004, SigmaTel, Inc. <irquality@sigmatel.com>
 *      Copyright (C) 2005, Milan Beno <beno@pobox.sk>
 *      Copyright (C) 2006, Nick FEdchik <nick@fedchik.org.ua>
 *          
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with this program; if not, write to the Free Software
 *	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *****************************************************************************/

#include <linux/time.h>

#include <net/irda/irda.h>
#include <net/irda/irda_device.h>      

#define RX_COPY_THRESHOLD 200
#define IRDA_USB_MAX_MTU 2051
#define IRDA_USB_SPEED_MTU 64		

#define IU_MAX_ACTIVE_RX_URBS	1	

#define IU_MAX_RX_URBS	(IU_MAX_ACTIVE_RX_URBS + 1)

#define IU_BUG_KICK_TIMEOUT
#undef IU_DUMP_CLASS_DESC 
#define IU_USB_MIN_RTT		500	

#define MEDIA_BUSY    0x80

#define SPEED_2400     0x01
#define SPEED_9600     0x02
#define SPEED_19200    0x03
#define SPEED_38400    0x04
#define SPEED_57600    0x05
#define SPEED_115200   0x06
#define SPEED_576000   0x07
#define SPEED_1152000  0x08
#define SPEED_4000000  0x09
#define SPEED_16000000 0x0a

#define IUC_DEFAULT	0x00	
#define IUC_SPEED_BUG	0x01	
#define IUC_NO_WINDOW	0x02	
#define IUC_NO_TURN	0x04	
#define IUC_SIR_ONLY	0x08	
#define IUC_SMALL_PKT	0x10	
#define IUC_MAX_WINDOW	0x20	
#define IUC_MAX_XBOFS	0x40	
#define IUC_STIR421X	0x80	

#define USB_IRDA_HEADER            0x01
#define USB_CLASS_IRDA             0x02 
#define USB_DT_IRDA                0x21
#define USB_IRDA_STIR421X_HEADER   0x03
#define IU_SIGMATEL_MAX_RX_URBS    (IU_MAX_ACTIVE_RX_URBS + \
                                    USB_IRDA_STIR421X_HEADER)

struct irda_class_desc {
	__u8  bLength;
	__u8  bDescriptorType;
	__le16 bcdSpecRevision;
	__u8  bmDataSize;
	__u8  bmWindowSize;
	__u8  bmMinTurnaroundTime;
	__le16 wBaudRate;
	__u8  bmAdditionalBOFs;
	__u8  bIrdaRateSniff;
	__u8  bMaxUnicastList;
} __packed;


#define IU_REQ_GET_CLASS_DESC	0x06
#define STIR421X_MAX_PATCH_DOWNLOAD_SIZE 1023

struct irda_usb_cb {
	struct irda_class_desc *irda_desc;
	struct usb_device *usbdev;	
	struct usb_interface *usbintf;	
	int netopen;			
	int present;			
	__u32 capability;		
	__u8  bulk_in_ep;		
	__u8  bulk_out_ep;		
	__u16 bulk_out_mtu;		
	__u8  bulk_int_ep;		

	__u8  max_rx_urb;
	struct urb **rx_urb;	        
	struct urb *idle_rx_urb;	
	struct urb *tx_urb;		
	struct urb *speed_urb;		
	
	struct net_device *netdev;	
	struct irlap_cb   *irlap;	
	struct qos_info qos;
	char *speed_buff;		
	char *tx_buff;

	struct timeval stamp;
	struct timeval now;

	spinlock_t lock;		

	__u16 xbofs;			
	__s16 new_xbofs;		
	__u32 speed;			
	__s32 new_speed;		

	__u8 header_length;             
	int needspatch;        		

	struct timer_list rx_defer_timer;	
};

