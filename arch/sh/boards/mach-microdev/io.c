/*
 * linux/arch/sh/boards/superh/microdev/io.c
 *
 * Copyright (C) 2003 Sean McGoogan (Sean.McGoogan@superh.com)
 * Copyright (C) 2003, 2004 SuperH, Inc.
 * Copyright (C) 2004 Paul Mundt
 *
 * SuperH SH4-202 MicroDev board support.
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 */

#include <linux/init.h>
#include <linux/pci.h>
#include <linux/wait.h>
#include <asm/io.h>
#include <mach/microdev.h>

#define	PVR	0xff000030	


#define	IO_IDE2_BASE		0x170ul	
#define	IO_IDE1_BASE		0x1f0ul	
#define IO_ISP1161_BASE		0x290ul 
#define IO_SERIAL2_BASE		0x2f8ul 
#define	IO_LAN91C111_BASE	0x300ul	
#define	IO_IDE2_MISC		0x376ul	
#define IO_SUPERIO_BASE		0x3f0ul 
#define	IO_IDE1_MISC		0x3f6ul	
#define IO_SERIAL1_BASE		0x3f8ul 

#define	IO_ISP1161_EXTENT	0x04ul	
#define	IO_LAN91C111_EXTENT	0x10ul	
#define	IO_SUPERIO_EXTENT	0x02ul	
#define	IO_IDE_EXTENT		0x08ul	
#define IO_SERIAL_EXTENT	0x10ul

#define	IO_LAN91C111_PHYS	0xa7500000ul	
#define	IO_ISP1161_PHYS		0xa7700000ul	
#define	IO_SUPERIO_PHYS		0xa7800000ul	

void __iomem *microdev_ioport_map(unsigned long offset, unsigned int len)
{
	unsigned long result;

	if ((offset >= IO_LAN91C111_BASE) &&
	    (offset <  IO_LAN91C111_BASE + IO_LAN91C111_EXTENT)) {
		result = IO_LAN91C111_PHYS + offset - IO_LAN91C111_BASE;
	} else if ((offset >= IO_SUPERIO_BASE) &&
		   (offset <  IO_SUPERIO_BASE + IO_SUPERIO_EXTENT)) {
		result = IO_SUPERIO_PHYS + (offset << 1);
	} else if (((offset >= IO_IDE1_BASE) &&
		    (offset <  IO_IDE1_BASE + IO_IDE_EXTENT)) ||
		    (offset == IO_IDE1_MISC)) {
	        result = IO_SUPERIO_PHYS + (offset << 1);
	} else if (((offset >= IO_IDE2_BASE) &&
		    (offset <  IO_IDE2_BASE + IO_IDE_EXTENT)) ||
		    (offset == IO_IDE2_MISC)) {
	        result = IO_SUPERIO_PHYS + (offset << 1);
	} else if ((offset >= IO_SERIAL1_BASE) &&
		   (offset <  IO_SERIAL1_BASE + IO_SERIAL_EXTENT)) {
		result = IO_SUPERIO_PHYS + (offset << 1);
	} else if ((offset >= IO_SERIAL2_BASE) &&
		   (offset <  IO_SERIAL2_BASE + IO_SERIAL_EXTENT)) {
		result = IO_SUPERIO_PHYS + (offset << 1);
	} else if ((offset >= IO_ISP1161_BASE) &&
		   (offset < IO_ISP1161_BASE + IO_ISP1161_EXTENT)) {
		result = IO_ISP1161_PHYS + offset - IO_ISP1161_BASE;
	} else {
		printk("Warning: unexpected port in %s( offset = 0x%lx )\n",
		       __func__, offset);
		result = PVR;
	}

	return (void __iomem *)result;
}
