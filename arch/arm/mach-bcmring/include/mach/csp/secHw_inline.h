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


#ifndef SECHW_INLINE_H
#define SECHW_INLINE_H

static inline void secHw_setSecure(uint32_t mask	
    ) {
	secHw_REGS_t *regp = (secHw_REGS_t *) MM_IO_BASE_TZPC;

	if (mask & 0x0000FFFF) {
		regp->reg[secHw_IDX_LS].setSecure = mask & 0x0000FFFF;
	}

	if (mask & 0xFFFF0000) {
		regp->reg[secHw_IDX_MS].setSecure = mask >> 16;
	}
}

static inline void secHw_setUnsecure(uint32_t mask	
    ) {
	secHw_REGS_t *regp = (secHw_REGS_t *) MM_IO_BASE_TZPC;

	if (mask & 0x0000FFFF) {
		regp->reg[secHw_IDX_LS].setUnsecure = mask & 0x0000FFFF;
	}
	if (mask & 0xFFFF0000) {
		regp->reg[secHw_IDX_MS].setUnsecure = mask >> 16;
	}
}

static inline uint32_t secHw_getStatus(void)
{
	secHw_REGS_t *regp = (secHw_REGS_t *) MM_IO_BASE_TZPC;

	return (regp->reg[1].status << 16) + regp->reg[0].status;
}

#endif 
