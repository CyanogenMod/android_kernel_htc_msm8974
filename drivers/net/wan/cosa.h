
/*
 *  Copyright (C) 1995-1997  Jan "Yenya" Kasprzak <kas@fi.muni.cz>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef COSA_H__
#define COSA_H__

#include <linux/ioctl.h>

#ifdef __KERNEL__
#define SR_RX_DMA_ENA   0x04    
#define SR_TX_DMA_ENA   0x08    
#define SR_RST          0x10    
#define SR_USR_INT_ENA  0x20    
#define SR_TX_INT_ENA   0x40    
#define SR_RX_INT_ENA   0x80    

#define SR_USR_RQ       0x20    
#define SR_TX_RDY       0x40    
#define SR_RX_RDY       0x80    

#define SR_UP_REQUEST   0x02    
#define SR_DOWN_REQUEST 0x01    
#define SR_END_OF_TRANSFER      0x03    

#define SR_CMD_FROM_SRP_MASK    0x03    

#define SR_RDY_RCV      0x01    
#define SR_RDY_SND      0x02    
#define SR_CMD_PND      0x04     

#define SR_PKT_UP       0x01    
#define SR_PKT_DOWN     0x02    

#endif 

#define SR_LOAD_ADDR    0x4400  
#define SR_START_ADDR   0x4400  

#define COSA_LOAD_ADDR    0x400  
#define COSA_MAX_FIRMWARE_SIZE	0x10000

struct cosa_download {
	int addr, len;
	char __user *code;
};

#define COSAIORSET	_IO('C',0xf0)

#define COSAIOSTRT	_IOW('C',0xf1, int)

#define COSAIORMEM	_IOWR('C',0xf2, struct cosa_download *)

#define COSAIODOWNLD	_IOW('C',0xf2, struct cosa_download *)

#define COSAIORTYPE	_IOR('C',0xf3, char *)

#define COSAIORIDSTR	_IOR('C',0xf4, char *)
#define COSA_MAX_ID_STRING 128


#define COSAIONRCARDS	_IO('C',0xf7)

#define COSAIONRCHANS	_IO('C',0xf8)

#define COSAIOBMSET	_IOW('C', 0xf9, unsigned short)

#define COSA_BM_OFF	0	
#define COSA_BM_ON	1	

#define COSAIOBMGET	_IO('C', 0xfa)

#endif 
