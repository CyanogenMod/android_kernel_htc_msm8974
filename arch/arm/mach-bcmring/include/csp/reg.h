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


#ifndef CSP_REG_H
#define CSP_REG_H


#include <csp/stdint.h>


#define __REG32(x)      (*((volatile uint32_t *)(x)))
#define __REG16(x)      (*((volatile uint16_t *)(x)))
#define __REG8(x)       (*((volatile uint8_t *) (x)))

#define REG8_RSVD(start, end)   uint8_t rsvd_##start[(end - start) / sizeof(uint8_t)]
#define REG16_RSVD(start, end)  uint16_t rsvd_##start[(end - start) / sizeof(uint16_t)]
#define REG32_RSVD(start, end)  uint32_t rsvd_##start[(end - start) / sizeof(uint32_t)]



#if defined(__KERNEL__) && !defined(STANDALONE)
#include <mach/hardware.h>
#include <linux/interrupt.h>

#define REG_LOCAL_IRQ_SAVE      HW_DECLARE_SPINLOCK(reg32) \
	unsigned long flags; HW_IRQ_SAVE(reg32, flags)

#define REG_LOCAL_IRQ_RESTORE   HW_IRQ_RESTORE(reg32, flags)

#else

#define REG_LOCAL_IRQ_SAVE
#define REG_LOCAL_IRQ_RESTORE

#endif

static inline void reg32_modify_and(volatile uint32_t *reg, uint32_t value)
{
	REG_LOCAL_IRQ_SAVE;
	*reg &= value;
	REG_LOCAL_IRQ_RESTORE;
}

static inline void reg32_modify_or(volatile uint32_t *reg, uint32_t value)
{
	REG_LOCAL_IRQ_SAVE;
	*reg |= value;
	REG_LOCAL_IRQ_RESTORE;
}

static inline void reg32_modify_mask(volatile uint32_t *reg, uint32_t mask,
				     uint32_t value)
{
	REG_LOCAL_IRQ_SAVE;
	*reg = (*reg & mask) | value;
	REG_LOCAL_IRQ_RESTORE;
}

static inline void reg32_write(volatile uint32_t *reg, uint32_t value)
{
	*reg = value;
}

#endif 
