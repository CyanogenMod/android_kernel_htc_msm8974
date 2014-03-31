

#ifndef __ASM_ARCH_FSMC_H
#define __ASM_ARCH_FSMC_H

#include <mach/hardware.h>

#define FSMC_BCR(x)     (NOMADIK_FSMC_VA + (x << 3))
#define FSMC_BTR(x)     (NOMADIK_FSMC_VA + (x << 3) + 0x04)

#define FSMC_PCR(x)     (NOMADIK_FSMC_VA + ((2 + x) << 5) + 0x00)
#define FSMC_PMEM(x)    (NOMADIK_FSMC_VA + ((2 + x) << 5) + 0x08)
#define FSMC_PATT(x)    (NOMADIK_FSMC_VA + ((2 + x) << 5) + 0x0c)
#define FSMC_PIO(x)     (NOMADIK_FSMC_VA + ((2 + x) << 5) + 0x10)
#define FSMC_PECCR(x)   (NOMADIK_FSMC_VA + ((2 + x) << 5) + 0x14)

#endif 
