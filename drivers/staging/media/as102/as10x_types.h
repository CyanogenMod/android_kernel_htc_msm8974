/*
 * Abilis Systems Single DVB-T Receiver
 * Copyright (C) 2008 Pierrick Hascoet <pierrick.hascoet@abilis.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
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
#ifndef _AS10X_TYPES_H_
#define _AS10X_TYPES_H_

#include "as10x_handle.h"


#define BW_5_MHZ		0x00
#define BW_6_MHZ		0x01
#define BW_7_MHZ		0x02
#define BW_8_MHZ		0x03

#define HIER_NO_PRIORITY	0x00
#define HIER_LOW_PRIORITY	0x01
#define HIER_HIGH_PRIORITY	0x02

#define CONST_QPSK		0x00
#define CONST_QAM16		0x01
#define CONST_QAM64		0x02
#define CONST_UNKNOWN		0xFF

#define HIER_NONE		0x00
#define HIER_ALPHA_1		0x01
#define HIER_ALPHA_2		0x02
#define HIER_ALPHA_4		0x03
#define HIER_UNKNOWN		0xFF

#define INTLV_NATIVE		0x00
#define INTLV_IN_DEPTH		0x01
#define INTLV_UNKNOWN		0xFF

#define CODE_RATE_1_2		0x00
#define CODE_RATE_2_3		0x01
#define CODE_RATE_3_4		0x02
#define CODE_RATE_5_6		0x03
#define CODE_RATE_7_8		0x04
#define CODE_RATE_UNKNOWN	0xFF

#define GUARD_INT_1_32		0x00
#define GUARD_INT_1_16		0x01
#define GUARD_INT_1_8		0x02
#define GUARD_INT_1_4		0x03
#define GUARD_UNKNOWN		0xFF

#define TRANS_MODE_2K		0x00
#define TRANS_MODE_8K		0x01
#define TRANS_MODE_4K		0x02
#define TRANS_MODE_UNKNOWN	0xFF

#define TIMESLICING_PRESENT	0x01
#define MPE_FEC_PRESENT		0x02

#define TUNE_STATUS_NOT_TUNED		0x00
#define TUNE_STATUS_IDLE		0x01
#define TUNE_STATUS_LOCKING		0x02
#define TUNE_STATUS_SIGNAL_DVB_OK	0x03
#define TUNE_STATUS_STREAM_DETECTED	0x04
#define TUNE_STATUS_STREAM_TUNED	0x05
#define TUNE_STATUS_ERROR		0xFF

#define TS_PID_TYPE_TS		0
#define TS_PID_TYPE_PSI_SI	1
#define TS_PID_TYPE_MPE		2

#define MAX_ECHOS	15

#define CONTEXT_LNA			1010
#define CONTEXT_ELNA_HYSTERESIS		4003
#define CONTEXT_ELNA_GAIN		4004
#define CONTEXT_MER_THRESHOLD		5005
#define CONTEXT_MER_OFFSET		5006
#define CONTEXT_IR_STATE		7000
#define CONTEXT_TSOUT_MSB_FIRST		7004
#define CONTEXT_TSOUT_FALLING_EDGE	7005

#define CFG_MODE_ON	0
#define CFG_MODE_OFF	1
#define CFG_MODE_AUTO	2

struct as10x_tps {
	uint8_t modulation;
	uint8_t hierarchy;
	uint8_t interleaving_mode;
	uint8_t code_rate_HP;
	uint8_t code_rate_LP;
	uint8_t guard_interval;
	uint8_t transmission_mode;
	uint8_t DVBH_mask_HP;
	uint8_t DVBH_mask_LP;
	uint16_t cell_ID;
} __packed;

struct as10x_tune_args {
	
	uint32_t freq;
	
	uint8_t bandwidth;
	
	uint8_t hier_select;
	
	uint8_t modulation;
	
	uint8_t hierarchy;
	
	uint8_t interleaving_mode;
	
	uint8_t code_rate;
	
	uint8_t guard_interval;
	
	uint8_t transmission_mode;
} __packed;

struct as10x_tune_status {
	
	uint8_t tune_state;
	
	int16_t signal_strength;
	
	uint16_t PER;
	
	uint16_t BER;
} __packed;

struct as10x_demod_stats {
	
	uint32_t frame_count;
	
	uint32_t bad_frame_count;
	
	uint32_t bytes_fixed_by_rs;
	
	uint16_t mer;
	
	uint8_t has_started;
} __packed;

struct as10x_ts_filter {
	uint16_t pid;  
	uint8_t  type; 
	uint8_t  idx;  
} __packed;

struct as10x_register_value {
	uint8_t mode;
	union {
		uint8_t  value8;   
		uint16_t value16;  
		uint32_t value32;  
	} __packed u;
} __packed;

struct as10x_register_addr {
	
	uint32_t addr;
	
	uint8_t mode;
};

#endif
