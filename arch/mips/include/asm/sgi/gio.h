/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * gio.h: Definitions for SGI GIO bus
 *
 * Copyright (C) 2002 Ladislav Michl
 */

#ifndef _SGI_GIO_H
#define _SGI_GIO_H


#define GIO_ID(x)		(x & 0x7f)
#define GIO_32BIT_ID		0x80
#define GIO_REV(x)		((x >> 8) & 0xff)
#define GIO_64BIT_IFACE		0x10000
#define GIO_ROM_PRESENT		0x20000
#define GIO_VENDOR_CODE(x)	((x >> 18) & 0x3fff)

#define GIO_SLOT_GFX_BASE	0x1f000000
#define GIO_SLOT_EXP0_BASE	0x1f400000
#define GIO_SLOT_EXP1_BASE	0x1f600000

#endif 
