/*
 * SpanDSP - a series of DSP components for telephony
 *
 * echo.c - A line echo canceller.  This code is being developed
 *          against and partially complies with G168.
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *         and David Rowe <david_at_rowetel_dot_com>
 *
 * Copyright (C) 2001 Steve Underwood and 2007 David Rowe
 *
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2, as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef __ECHO_H
#define __ECHO_H


#include "fir.h"
#include "oslec.h"

struct oslec_state {
	int16_t tx, rx;
	int16_t clean;
	int16_t clean_nlp;

	int nonupdate_dwell;
	int curr_pos;
	int taps;
	int log2taps;
	int adaption_mode;

	int cond_met;
	int32_t Pstates;
	int16_t adapt;
	int32_t factor;
	int16_t shift;

	
	int Ltxacc, Lrxacc, Lcleanacc, Lclean_bgacc;
	int Ltx, Lrx;
	int Lclean;
	int Lclean_bg;
	int Lbgn, Lbgn_acc, Lbgn_upper, Lbgn_upper_acc;

	
	struct fir16_state_t fir_state;
	struct fir16_state_t fir_state_bg;
	int16_t *fir_taps16[2];

	
	int tx_1, tx_2, rx_1, rx_2;

	
	int32_t xvtx[5], yvtx[5];
	int32_t xvrx[5], yvrx[5];

	
	int cng_level;
	int cng_rndnum;
	int cng_filter;

	
	int16_t *snapshot;
};

#endif 
