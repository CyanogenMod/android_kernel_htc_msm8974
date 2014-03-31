
/*
 *	yam.h  -- YAM radio modem driver.
 *
 *	Copyright (C) 1998 Frederic Rible F1OAT (frible@teaser.fr)
 *	Adapted from baycom.c driver written by Thomas Sailer (sailer@ife.ee.ethz.ch)
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
 *  Please note that the GPL allows you to use the driver, NOT the radio.
 *  In order to use the radio, you need a license from the communications
 *  authority of your country.
 *
 *
 */


#define SIOCYAMRESERVED	(0)
#define SIOCYAMSCFG 	(1)	
#define SIOCYAMGCFG 	(2)	
#define SIOCYAMSMCS 	(3)	

#define YAM_IOBASE   (1 << 0)
#define YAM_IRQ      (1 << 1)
#define YAM_BITRATE  (1 << 2) 
#define YAM_MODE     (1 << 3) 
#define YAM_HOLDDLY  (1 << 4) 
#define YAM_TXDELAY  (1 << 5) 
#define YAM_TXTAIL   (1 << 6) 
#define YAM_PERSIST  (1 << 7) 
#define YAM_SLOTTIME (1 << 8) 
#define YAM_BAUDRATE (1 << 9) 

#define YAM_MAXBITRATE  57600
#define YAM_MAXBAUDRATE 115200
#define YAM_MAXMODE     2
#define YAM_MAXHOLDDLY  99
#define YAM_MAXTXDELAY  999
#define YAM_MAXTXTAIL   999
#define YAM_MAXPERSIST  255
#define YAM_MAXSLOTTIME 999

#define YAM_FPGA_SIZE	5302

struct yamcfg {
	unsigned int mask;		
	unsigned int iobase;	
	unsigned int irq;		
	unsigned int bitrate;	
	unsigned int baudrate;	
	unsigned int txdelay;	
	unsigned int txtail;	
	unsigned int persist;	
	unsigned int slottime;	
	unsigned int mode;		
	unsigned int holddly;	
};

struct yamdrv_ioctl_cfg {
	int cmd;
	struct yamcfg cfg;
};

struct yamdrv_ioctl_mcs {
	int cmd;
	int bitrate;
	unsigned char bits[YAM_FPGA_SIZE];
};
