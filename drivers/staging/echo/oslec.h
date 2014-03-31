/*
 *  OSLEC - A line echo canceller.  This code is being developed
 *          against and partially complies with G168. Using code from SpanDSP
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *         and David Rowe <david_at_rowetel_dot_com>
 *
 * Copyright (C) 2001 Steve Underwood and 2007-2008 David Rowe
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
 *
 */

#ifndef __OSLEC_H
#define __OSLEC_H

#define ECHO_CAN_USE_ADAPTION	0x01
#define ECHO_CAN_USE_NLP	0x02
#define ECHO_CAN_USE_CNG	0x04
#define ECHO_CAN_USE_CLIP	0x08
#define ECHO_CAN_USE_TX_HPF	0x10
#define ECHO_CAN_USE_RX_HPF	0x20
#define ECHO_CAN_DISABLE	0x40

struct oslec_state;

struct oslec_state *oslec_create(int len, int adaption_mode);

void oslec_free(struct oslec_state *ec);

void oslec_flush(struct oslec_state *ec);

void oslec_adaption_mode(struct oslec_state *ec, int adaption_mode);

void oslec_snapshot(struct oslec_state *ec);

int16_t oslec_update(struct oslec_state *ec, int16_t tx, int16_t rx);

int16_t oslec_hpf_tx(struct oslec_state *ec, int16_t tx);

#endif 
