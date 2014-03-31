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

#include "main.h"
#include "bitarray.h"

#include <linux/bitops.h>

int get_bit_status(const unsigned long *seq_bits, uint32_t last_seqno,
		   uint32_t curr_seqno)
{
	int32_t diff, word_offset, word_num;

	diff = last_seqno - curr_seqno;
	if (diff < 0 || diff >= TQ_LOCAL_WINDOW_SIZE) {
		return 0;
	} else {
		
		word_num = (last_seqno - curr_seqno) / WORD_BIT_SIZE;
		
		word_offset = (last_seqno - curr_seqno) % WORD_BIT_SIZE;

		if (test_bit(word_offset, &seq_bits[word_num]))
			return 1;
		else
			return 0;
	}
}

void bit_mark(unsigned long *seq_bits, int32_t n)
{
	int32_t word_offset, word_num;

	
	if (n < 0 || n >= TQ_LOCAL_WINDOW_SIZE)
		return;

	
	word_num = n / WORD_BIT_SIZE;
	
	word_offset = n % WORD_BIT_SIZE;

	set_bit(word_offset, &seq_bits[word_num]); 
}

static void bit_shift(unsigned long *seq_bits, int32_t n)
{
	int32_t word_offset, word_num;
	int32_t i;

	if (n <= 0 || n >= TQ_LOCAL_WINDOW_SIZE)
		return;

	word_offset = n % WORD_BIT_SIZE;
	word_num = n / WORD_BIT_SIZE;	

	for (i = NUM_WORDS - 1; i > word_num; i--) {

		seq_bits[i] =
			(seq_bits[i - word_num] << word_offset) +
			(seq_bits[i - word_num - 1] >>
			 (WORD_BIT_SIZE-word_offset));
	}

	seq_bits[i] = (seq_bits[i - word_num] << word_offset);

	
	i--;

	for (; i >= 0; i--)
		seq_bits[i] = 0;
}

static void bit_reset_window(unsigned long *seq_bits)
{
	int i;
	for (i = 0; i < NUM_WORDS; i++)
		seq_bits[i] = 0;
}


int bit_get_packet(void *priv, unsigned long *seq_bits,
		    int32_t seq_num_diff, int set_mark)
{
	struct bat_priv *bat_priv = priv;


	if ((seq_num_diff <= 0) && (seq_num_diff > -TQ_LOCAL_WINDOW_SIZE)) {
		if (set_mark)
			bit_mark(seq_bits, -seq_num_diff);
		return 0;
	}


	if ((seq_num_diff > 0) && (seq_num_diff < TQ_LOCAL_WINDOW_SIZE)) {
		bit_shift(seq_bits, seq_num_diff);

		if (set_mark)
			bit_mark(seq_bits, 0);
		return 1;
	}

	

	if ((seq_num_diff >= TQ_LOCAL_WINDOW_SIZE) &&
	    (seq_num_diff < EXPECTED_SEQNO_RANGE)) {
		bat_dbg(DBG_BATMAN, bat_priv,
			"We missed a lot of packets (%i) !\n",
			seq_num_diff - 1);
		bit_reset_window(seq_bits);
		if (set_mark)
			bit_mark(seq_bits, 0);
		return 1;
	}


	if ((seq_num_diff <= -TQ_LOCAL_WINDOW_SIZE) ||
	    (seq_num_diff >= EXPECTED_SEQNO_RANGE)) {

		bat_dbg(DBG_BATMAN, bat_priv,
			"Other host probably restarted!\n");

		bit_reset_window(seq_bits);
		if (set_mark)
			bit_mark(seq_bits, 0);

		return 1;
	}

	
	return 0;
}

int bit_packet_count(const unsigned long *seq_bits)
{
	int i, hamming = 0;

	for (i = 0; i < NUM_WORDS; i++)
		hamming += hweight_long(seq_bits[i]);

	return hamming;
}
