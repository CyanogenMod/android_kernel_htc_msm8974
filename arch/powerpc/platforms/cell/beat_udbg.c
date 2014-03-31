/*
 * udbg function for Beat
 *
 * (C) Copyright 2006 TOSHIBA CORPORATION
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <linux/kernel.h>
#include <linux/console.h>

#include <asm/machdep.h>
#include <asm/prom.h>
#include <asm/udbg.h>

#include "beat.h"

#define	celleb_vtermno	0

static void udbg_putc_beat(char c)
{
	unsigned long rc;

	if (c == '\n')
		udbg_putc_beat('\r');

	rc = beat_put_term_char(celleb_vtermno, 1, (uint64_t)c << 56, 0);
}

static u64 inbuflen;
static u64 inbuf[2];	

static int udbg_getc_poll_beat(void)
{
	char ch, *buf = (char *)inbuf;
	int i;
	long rc;
	if (inbuflen == 0) {
		
		inbuflen = 0;
		rc = beat_get_term_char(celleb_vtermno, &inbuflen,
					inbuf+0, inbuf+1);
		if (rc != 0)
			inbuflen = 0;	
	}
	if (inbuflen <= 0 || inbuflen > 16) {
		
		inbuflen = 0;
		return -1;
	}
	ch = buf[0];
	for (i = 1; i < inbuflen; i++)	
		buf[i-1] = buf[i];
	inbuflen--;
	return ch;
}

static int udbg_getc_beat(void)
{
	int ch;
	for (;;) {
		ch = udbg_getc_poll_beat();
		if (ch == -1) {
			
			volatile unsigned long delay;
			for (delay = 0; delay < 2000000; delay++)
				;
		} else {
			return ch;
		}
	}
}

void __init udbg_init_debug_beat(void)
{
	udbg_putc = udbg_putc_beat;
	udbg_getc = udbg_getc_beat;
	udbg_getc_poll = udbg_getc_poll_beat;
}
