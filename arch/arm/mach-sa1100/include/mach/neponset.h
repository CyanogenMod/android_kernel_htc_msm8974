#ifndef __ASM_ARCH_NEPONSET_H
#define __ASM_ARCH_NEPONSET_H

#define NCR_GP01_OFF		(1<<0)
#define NCR_TP_PWR_EN		(1<<1)
#define NCR_MS_PWR_EN		(1<<2)
#define NCR_ENET_OSC_EN		(1<<3)
#define NCR_SPI_KB_WK_UP	(1<<4)
#define NCR_A0VPP		(1<<5)
#define NCR_A1VPP		(1<<6)

void neponset_ncr_frob(unsigned int, unsigned int);
#define neponset_ncr_set(v)	neponset_ncr_frob(0, v)
#define neponset_ncr_clear(v)	neponset_ncr_frob(v, 0)

#endif
