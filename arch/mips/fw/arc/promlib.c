/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1996 David S. Miller (dm@sgi.com)
 * Compatibility with board caches, Ulf Carlsson
 */
#include <linux/kernel.h>
#include <asm/sgialib.h>
#include <asm/bcache.h>


void prom_putchar(char c)
{
	ULONG cnt;
	CHAR it = c;

	bc_disable();
	ArcWrite(1, &it, 1, &cnt);
	bc_enable();
}

char prom_getchar(void)
{
	ULONG cnt;
	CHAR c;

	bc_disable();
	ArcRead(0, &c, 1, &cnt);
	bc_enable();

	return c;
}
