/*
 *   Copyright (C) International Business Machines Corp., 2000-2004
 *
 *   This program is free software;  you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY;  without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
 *   the GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program;  if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */
#ifndef _H_JFS_TYPES
#define	_H_JFS_TYPES


#include <linux/types.h>
#include <linux/nls.h>

#include "endian24.h"

typedef u16 tid_t;
typedef u16 lid_t;

struct timestruc_t {
	__le32 tv_sec;
	__le32 tv_nsec;
};


#define LEFTMOSTONE	0x80000000
#define	HIGHORDER	0x80000000u	
#define	ONES		0xffffffffu	

typedef struct {
	unsigned len:24;
	unsigned addr1:8;
	__le32 addr2;
} pxd_t;


#define	PXDlength(pxd, length32)	((pxd)->len = __cpu_to_le24(length32))
#define	PXDaddress(pxd, address64)\
{\
	(pxd)->addr1 = ((s64)address64) >> 32;\
	(pxd)->addr2 = __cpu_to_le32((address64) & 0xffffffff);\
}

#define	lengthPXD(pxd)	__le24_to_cpu((pxd)->len)
#define	addressPXD(pxd)\
	( ((s64)((pxd)->addr1)) << 32 | __le32_to_cpu((pxd)->addr2))

#define MAXTREEHEIGHT 8
struct pxdlist {
	s16 maxnpxd;
	s16 npxd;
	pxd_t pxd[MAXTREEHEIGHT];
};


typedef struct {
	unsigned flag:8;	
	unsigned rsrvd:24;
	__le32 size;		
	unsigned len:24;	
	unsigned addr1:8;	
	__le32 addr2;		
} dxd_t;			

#define	DXD_INDEX	0x80	
#define	DXD_INLINE	0x40	
#define	DXD_EXTENT	0x20	
#define	DXD_FILE	0x10	
#define DXD_CORRUPT	0x08	

#define	DXDlength	PXDlength
#define	DXDaddress	PXDaddress
#define	lengthDXD	lengthPXD
#define	addressDXD	addressPXD
#define DXDsize(dxd, size32) ((dxd)->size = cpu_to_le32(size32))
#define sizeDXD(dxd)	le32_to_cpu((dxd)->size)

struct component_name {
	int namlen;
	wchar_t *name;
};


struct dasd {
	u8 thresh;		
	u8 delta;		
	u8 rsrvd1;
	u8 limit_hi;		
	__le32 limit_lo;	
	u8 rsrvd2[3];
	u8 used_hi;		
	__le32 used_lo;		
};

#define DASDLIMIT(dasdp) \
	(((u64)((dasdp)->limit_hi) << 32) + __le32_to_cpu((dasdp)->limit_lo))
#define setDASDLIMIT(dasdp, limit)\
{\
	(dasdp)->limit_hi = ((u64)limit) >> 32;\
	(dasdp)->limit_lo = __cpu_to_le32(limit);\
}
#define DASDUSED(dasdp) \
	(((u64)((dasdp)->used_hi) << 32) + __le32_to_cpu((dasdp)->used_lo))
#define setDASDUSED(dasdp, used)\
{\
	(dasdp)->used_hi = ((u64)used) >> 32;\
	(dasdp)->used_lo = __cpu_to_le32(used);\
}

#endif				
