/*
 * Copyright (c) 2011 Broadcom Corporation
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#ifndef __CRC8_H_
#define __CRC8_H_

#include <linux/types.h>

#define CRC8_INIT_VALUE		0xFF

#define CRC8_GOOD_VALUE(_table)	(_table[0xFF])

#define CRC8_TABLE_SIZE			256

#define DECLARE_CRC8_TABLE(_table) \
	static u8 _table[CRC8_TABLE_SIZE]

void crc8_populate_lsb(u8 table[CRC8_TABLE_SIZE], u8 polynomial);

void crc8_populate_msb(u8 table[CRC8_TABLE_SIZE], u8 polynomial);

u8 crc8(const u8 table[CRC8_TABLE_SIZE], u8 *pdata, size_t nbytes, u8 crc);

#endif 
