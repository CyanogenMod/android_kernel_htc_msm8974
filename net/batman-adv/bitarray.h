/*
 * Copyright (C) 2006-2012 B.A.T.M.A.N. contributors:
 *
 * Simon Wunderlich, Marek Lindner
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA
 *
 */

#ifndef _NET_BATMAN_ADV_BITARRAY_H_
#define _NET_BATMAN_ADV_BITARRAY_H_

#define WORD_BIT_SIZE (sizeof(unsigned long) * 8)

int get_bit_status(const unsigned long *seq_bits, uint32_t last_seqno,
		   uint32_t curr_seqno);

void bit_mark(unsigned long *seq_bits, int32_t n);


int bit_get_packet(void *priv, unsigned long *seq_bits,
		   int32_t seq_num_diff, int set_mark);

int bit_packet_count(const unsigned long *seq_bits);

#endif 
