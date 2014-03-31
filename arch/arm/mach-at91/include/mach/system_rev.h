/*
 * Copyright (C) 2011 Jean-Christophe PLAGNIOL-VILLARD <plagnioj@jcrosoft.com>
 *
 * Under GPLv2 only
 */

#ifndef __ARCH_SYSTEM_REV_H__
#define __ARCH_SYSTEM_REV_H__

#include <asm/system_info.h>

#define BOARD_HAVE_NAND_16BIT	(1 << 31)
static inline int board_have_nand_16bit(void)
{
	return (system_rev & BOARD_HAVE_NAND_16BIT) ? 1 : 0;
}

#endif 
