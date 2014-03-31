/*
 *
 *  Copyright (C) 2005 Mike Isely <isely@pobox.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */
#ifndef __PVRUSB2_DEBUG_H
#define __PVRUSB2_DEBUG_H

extern int pvrusb2_debug;

#define pvr2_trace(msk, fmt, arg...) do {if(msk & pvrusb2_debug) printk(KERN_INFO "pvrusb2: " fmt "\n", ##arg); } while (0)

#define PVR2_TRACE_INFO       (1 <<  0) 
#define PVR2_TRACE_ERROR_LEGS (1 <<  1) 
#define PVR2_TRACE_TOLERANCE  (1 <<  2) 
#define PVR2_TRACE_TRAP       (1 <<  3) 
#define PVR2_TRACE_STD        (1 <<  4) 
#define PVR2_TRACE_INIT       (1 <<  5) 
#define PVR2_TRACE_START_STOP (1 <<  6) 
#define PVR2_TRACE_CTL        (1 <<  7) 
#define PVR2_TRACE_STATE      (1 <<  8) 
#define PVR2_TRACE_STBITS     (1 <<  9) 
#define PVR2_TRACE_EEPROM     (1 << 10) 
#define PVR2_TRACE_STRUCT     (1 << 11) 
#define PVR2_TRACE_OPEN_CLOSE (1 << 12) 
#define PVR2_TRACE_CTXT       (1 << 13) 
#define PVR2_TRACE_SYSFS      (1 << 14) 
#define PVR2_TRACE_FIRMWARE   (1 << 15) 
#define PVR2_TRACE_CHIPS      (1 << 16) 
#define PVR2_TRACE_I2C        (1 << 17) 
#define PVR2_TRACE_I2C_CMD    (1 << 18) 
#define PVR2_TRACE_I2C_CORE   (1 << 19) 
#define PVR2_TRACE_I2C_TRAF   (1 << 20) 
#define PVR2_TRACE_V4LIOCTL   (1 << 21) 
#define PVR2_TRACE_ENCODER    (1 << 22) 
#define PVR2_TRACE_BUF_POOL   (1 << 23) 
#define PVR2_TRACE_BUF_FLOW   (1 << 24) 
#define PVR2_TRACE_DATA_FLOW  (1 << 25) 
#define PVR2_TRACE_DEBUGIFC   (1 << 26) 
#define PVR2_TRACE_GPIO       (1 << 27) 
#define PVR2_TRACE_DVB_FEED   (1 << 28) 


#endif 

