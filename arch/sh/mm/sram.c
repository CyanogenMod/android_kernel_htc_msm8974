/*
 * SRAM pool for tiny memories not otherwise managed.
 *
 * Copyright (C) 2010  Paul Mundt
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 */
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <asm/sram.h>

struct gen_pool *sram_pool;

static int __init sram_pool_init(void)
{
	sram_pool = gen_pool_create(1, -1);
	if (unlikely(!sram_pool))
		return -ENOMEM;

	return 0;
}
core_initcall(sram_pool_init);
