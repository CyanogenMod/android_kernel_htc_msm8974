/*
 * Intel Multimedia Timer device interface
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (c) 2001-2004 Silicon Graphics, Inc.  All rights reserved.
 *
 * This file should define an interface compatible with the IA-PC Multimedia
 * Timers Draft Specification (rev. 0.97) from Intel.  Note that some
 * hardware may not be able to safely export its registers to userspace,
 * so the ioctl interface should support all necessary functionality.
 *
 * 11/01/01 - jbarnes - initial revision
 * 9/10/04 - Christoph Lameter - remove interrupt support
 * 9/17/04 - jbarnes - remove test program, move some #defines to the driver
 */

#ifndef _LINUX_MMTIMER_H
#define _LINUX_MMTIMER_H

#define MMTIMER_IOCTL_BASE 'm'

#define MMTIMER_GETOFFSET _IO(MMTIMER_IOCTL_BASE, 0)
#define MMTIMER_GETRES _IOR(MMTIMER_IOCTL_BASE, 1, unsigned long)
#define MMTIMER_GETFREQ _IOR(MMTIMER_IOCTL_BASE, 2, unsigned long)
#define MMTIMER_GETBITS _IO(MMTIMER_IOCTL_BASE, 4)
#define MMTIMER_MMAPAVAIL _IO(MMTIMER_IOCTL_BASE, 6)
#define MMTIMER_GETCOUNTER _IOR(MMTIMER_IOCTL_BASE, 9, unsigned long)

#endif 
