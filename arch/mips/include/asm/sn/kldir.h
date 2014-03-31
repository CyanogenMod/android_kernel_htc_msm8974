/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Derived from IRIX <sys/SN/kldir.h>, revision 1.21.
 *
 * Copyright (C) 1992 - 1997, 1999, 2000 Silicon Graphics, Inc.
 * Copyright (C) 1999, 2000 by Ralf Baechle
 */
#ifndef _ASM_SN_KLDIR_H
#define _ASM_SN_KLDIR_H



#define KLDIR_MAGIC		0x434d5f53505f5357

/*
 * The upper portion of the memory map applies during boot
 * only and is overwritten by IRIX/SYMMON.
 *
 *                                    MEMORY MAP PER NODE
 *
 * 0x2000000 (32M)         +-----------------------------------------+
 *                         |      IO6 BUFFERS FOR FLASH ENET IOC3    |
 * 0x1F80000 (31.5M)       +-----------------------------------------+
 *                         |      IO6 TEXT/DATA/BSS/stack            |
 * 0x1C00000 (30M)         +-----------------------------------------+
 *                         |      IO6 PROM DEBUG TEXT/DATA/BSS/stack |
 * 0x0800000 (28M)         +-----------------------------------------+
 *                         |      IP27 PROM TEXT/DATA/BSS/stack      |
 * 0x1B00000 (27M)         +-----------------------------------------+
 *                         |      IP27 CFG                           |
 * 0x1A00000 (26M)         +-----------------------------------------+
 *                         |      Graphics PROM                      |
 * 0x1800000 (24M)         +-----------------------------------------+
 *                         |      3rd Party PROM drivers             |
 * 0x1600000 (22M)         +-----------------------------------------+
 *                         |                                         |
 *                         |      Free                               |
 *                         |                                         |
 *                         +-----------------------------------------+
 *                         |      UNIX DEBUG Version                 |
 * 0x190000 (2M--)         +-----------------------------------------+
 *                         |      SYMMON                             |
 *                         |      (For UNIX Debug only)              |
 * 0x34000 (208K)          +-----------------------------------------+
 *                         |      SYMMON STACK [NUM_CPU_PER_NODE]    |
 *                         |      (For UNIX Debug only)              |
 * 0x25000 (148K)          +-----------------------------------------+
 *                         |      KLCONFIG - II (temp)               |
 *                         |                                         |
 *                         |    ----------------------------         |
 *                         |                                         |
 *                         |      UNIX NON-DEBUG Version             |
 * 0x19000 (100K)          +-----------------------------------------+
 *
 *
 * The lower portion of the memory map contains information that is
 * permanent and is used by the IP27PROM, IO6PROM and IRIX.
 *
 * 0x19000 (100K)          +-----------------------------------------+
 *                         |                                         |
 *                         |      PI Error Spools (32K)              |
 *                         |                                         |
 * 0x12000 (72K)           +-----------------------------------------+
 *                         |      Unused                             |
 * 0x11c00 (71K)           +-----------------------------------------+
 *                         |      CPU 1 NMI Eframe area       	     |
 * 0x11a00 (70.5K)         +-----------------------------------------+
 *                         |      CPU 0 NMI Eframe area       	     |
 * 0x11800 (70K)           +-----------------------------------------+
 *                         |      CPU 1 NMI Register save area       |
 * 0x11600 (69.5K)         +-----------------------------------------+
 *                         |      CPU 0 NMI Register save area       |
 * 0x11400 (69K)           +-----------------------------------------+
 *                         |      GDA (1k)                           |
 * 0x11000 (68K)           +-----------------------------------------+
 *                         |      Early cache Exception stack        |
 *                         |             and/or                      |
 *			   |      kernel/io6prom nmi registers	     |
 * 0x10800  (66k)	   +-----------------------------------------+
 *			   |      cache error eframe   	 	     |
 * 0x10400 (65K)           +-----------------------------------------+
 *                         |      Exception Handlers (UALIAS copy)   |
 * 0x10000 (64K)           +-----------------------------------------+
 *                         |                                         |
 *                         |                                         |
 *                         |      KLCONFIG - I (permanent) (48K)     |
 *                         |                                         |
 *                         |                                         |
 *                         |                                         |
 * 0x4000 (16K)            +-----------------------------------------+
 *                         |      NMI Handler (Protected Page)       |
 * 0x3000 (12K)            +-----------------------------------------+
 *                         |      ARCS PVECTORS (master node only)   |
 * 0x2c00 (11K)            +-----------------------------------------+
 *                         |      ARCS TVECTORS (master node only)   |
 * 0x2800 (10K)            +-----------------------------------------+
 *                         |      LAUNCH [NUM_CPU]                   |
 * 0x2400 (9K)             +-----------------------------------------+
 *                         |      Low memory directory (KLDIR)       |
 * 0x2000 (8K)             +-----------------------------------------+
 *                         |      ARCS SPB (1K)                      |
 * 0x1000 (4K)             +-----------------------------------------+
 *                         |      Early cache Exception stack        |
 *                         |             and/or                      |
 *			   |      kernel/io6prom nmi registers	     |
 * 0x800  (2k)	           +-----------------------------------------+
 *			   |      cache error eframe   	 	     |
 * 0x400 (1K)              +-----------------------------------------+
 *                         |      Exception Handlers                 |
 * 0x0   (0K)              +-----------------------------------------+
 */

#ifdef __ASSEMBLY__
#define KLDIR_OFF_MAGIC			0x00
#define KLDIR_OFF_OFFSET		0x08
#define KLDIR_OFF_POINTER		0x10
#define KLDIR_OFF_SIZE			0x18
#define KLDIR_OFF_COUNT			0x20
#define KLDIR_OFF_STRIDE		0x28
#endif 

#define SYMMON_STACK_SIZE		0x8000

#if defined(PROM)

#define IP27_LAUNCH_OFFSET		0x2400
#define IP27_LAUNCH_SIZE		0x400
#define IP27_LAUNCH_COUNT		2
#define IP27_LAUNCH_STRIDE		0x200

#define IP27_KLCONFIG_OFFSET		0x4000
#define IP27_KLCONFIG_SIZE		0xc000
#define IP27_KLCONFIG_COUNT		1
#define IP27_KLCONFIG_STRIDE		0

#define IP27_NMI_OFFSET			0x3000
#define IP27_NMI_SIZE			0x40
#define IP27_NMI_COUNT			2
#define IP27_NMI_STRIDE			0x40

#define IP27_PI_ERROR_OFFSET		0x12000
#define IP27_PI_ERROR_SIZE		0x4000
#define IP27_PI_ERROR_COUNT		1
#define IP27_PI_ERROR_STRIDE		0

#define IP27_SYMMON_STK_OFFSET		0x25000
#define IP27_SYMMON_STK_SIZE		0xe000
#define IP27_SYMMON_STK_COUNT		2
#define IP27_SYMMON_STK_STRIDE		0x7000

#define IP27_FREEMEM_OFFSET		0x19000
#define IP27_FREEMEM_SIZE		-1
#define IP27_FREEMEM_COUNT		1
#define IP27_FREEMEM_STRIDE		0

#endif 
#define IO6_GDA_OFFSET			0x11000
#define IO6_GDA_SIZE			0x400
#define IO6_GDA_COUNT			1
#define IO6_GDA_STRIDE			0

#define IP27_NMI_KREGS_OFFSET		0x11400
#define IP27_NMI_KREGS_CPU_SIZE		0x200
#define IP27_NMI_EFRAME_OFFSET		0x11800
#define IP27_NMI_EFRAME_SIZE		0x200

#define KLDIR_ENT_SIZE			0x40
#define KLDIR_MAX_ENTRIES		(0x400 / 0x40)

#ifndef __ASSEMBLY__
typedef struct kldir_ent_s {
	u64		magic;		
	off_t		offset;		
	unsigned long	pointer;	
	size_t		size;		
	u64		count;		
	size_t		stride;		
	char		rsvd[16];	
} kldir_ent_t;
#endif 

#endif 
