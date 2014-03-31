/*****************************************************************************
* Copyright 2003 - 2008 Broadcom Corporation.  All rights reserved.
*
* Unless you and Broadcom execute a separate written software license
* agreement governing use of this software, this software is licensed to you
* under the terms of the GNU General Public License version 2, available at
* http://www.broadcom.com/licenses/GPLv2.php (the "GPL").
*
* Notwithstanding the above, under no circumstances may you combine this
* software in any way with any other Broadcom software provided under a
* license other than the GPL, without Broadcom's express prior written
* consent.
*****************************************************************************/

#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <asm/page.h>
#include <asm/mach/map.h>

#include <mach/hardware.h>
#include <mach/csp/mm_io.h>

#define IO_DESC(va, sz) { .virtual = va, \
	.pfn = __phys_to_pfn(HW_IO_VIRT_TO_PHYS(va)), \
	.length = sz, \
	.type = MT_DEVICE }

#define MEM_DESC(va, sz) { .virtual = va, \
	.pfn = __phys_to_pfn(HW_IO_VIRT_TO_PHYS(va)), \
	.length = sz, \
	.type = MT_MEMORY }

static struct map_desc bcmring_io_desc[] __initdata = {
	IO_DESC(MM_IO_BASE_NAND, SZ_64K),	
	IO_DESC(MM_IO_BASE_UMI, SZ_64K),	

	IO_DESC(MM_IO_BASE_BROM, SZ_64K),	
	MEM_DESC(MM_IO_BASE_ARAM, SZ_1M),	
	IO_DESC(MM_IO_BASE_DMA0, SZ_1M),	
	IO_DESC(MM_IO_BASE_DMA1, SZ_1M),	
	IO_DESC(MM_IO_BASE_ESW, SZ_1M),	
	IO_DESC(MM_IO_BASE_CLCD, SZ_1M),	
	IO_DESC(MM_IO_BASE_APM, SZ_1M),	
	IO_DESC(MM_IO_BASE_SPUM, SZ_1M),	
	IO_DESC(MM_IO_BASE_VPM_PROG, SZ_1M),	
	IO_DESC(MM_IO_BASE_VPM_DATA, SZ_1M),	

	IO_DESC(MM_IO_BASE_VRAM, SZ_64K),	
	IO_DESC(MM_IO_BASE_CHIPC, SZ_16M),	
	IO_DESC(MM_IO_BASE_VPM_EXTMEM_RSVD,
		SZ_16M),	
};

void __init bcmring_map_io(void)
{

	iotable_init(bcmring_io_desc, ARRAY_SIZE(bcmring_io_desc));
	
	init_consistent_dma_size(14 << 20);
}
