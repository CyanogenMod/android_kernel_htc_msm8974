/*
 * Copyright(c) 2007 Intel Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Maintained at www.Open-FCoE.org
 */
#ifndef _FC_ENCAPS_H_
#define _FC_ENCAPS_H_

#define FC_ENCAPS_MIN_FRAME_LEN 64	
#define FC_ENCAPS_MAX_FRAME_LEN (FC_ENCAPS_MIN_FRAME_LEN + FC_MAX_PAYLOAD)

#define FC_ENCAPS_VER       1           

struct fc_encaps_hdr {
	__u8	fc_proto;	
	__u8	fc_ver;		
	__u8	fc_proto_n;	
	__u8	fc_ver_n;	

	unsigned char fc_proto_data[8]; 

	__be16	fc_len_flags;	
	__be16	fc_len_flags_n;	

	__be32	fc_time[2];	
	__be32	fc_crc;		
	__be32	fc_sof;		

	
};

#define FCIP_ENCAPS_HDR_LEN 0x20	

#define FC_XY(x, y)		((((x) & 0xff) << 8) | ((y) & 0xff))
#define FC_XYXY(x, y)		((FCIP_XY(x, y) << 16) | FCIP_XY(x, y))
#define FC_XYNN(x, y)		(FCIP_XYXY(x, y) ^ 0xffff)

#define FC_SOF_ENCODE(n)	FC_XYNN(n, n)
#define FC_EOF_ENCODE(n)	FC_XYNN(n, n)

enum fc_sof {
	FC_SOF_F =	0x28,	
	FC_SOF_I4 =	0x29,	
	FC_SOF_I2 =	0x2d,	
	FC_SOF_I3 =	0x2e,	
	FC_SOF_N4 =	0x31,	
	FC_SOF_N2 =	0x35,	
	FC_SOF_N3 =	0x36,	
	FC_SOF_C4 =	0x39,	
} __attribute__((packed));

enum fc_eof {
	FC_EOF_N =	0x41,	
	FC_EOF_T =	0x42,	
	FC_EOF_RT =	0x44,
	FC_EOF_DT =	0x46,	
	FC_EOF_NI =	0x49,	
	FC_EOF_DTI =	0x4e,	
	FC_EOF_RTI =	0x4f,
	FC_EOF_A =	0x50,	
} __attribute__((packed));

#define FC_SOF_CLASS_MASK 0x06	

enum fc_class {
	FC_CLASS_NONE = 0,	
	FC_CLASS_2 =	FC_SOF_I2,
	FC_CLASS_3 =	FC_SOF_I3,
	FC_CLASS_4 =	FC_SOF_I4,
	FC_CLASS_F =	FC_SOF_F,
};

static inline int fc_sof_needs_ack(enum fc_sof sof)
{
	return (~sof) & 0x02;	
}

static inline enum fc_sof fc_sof_normal(enum fc_class class)
{
	return class + FC_SOF_N3 - FC_SOF_I3;	
}

static inline enum fc_class fc_sof_class(enum fc_sof sof)
{
	return (sof & 0x7) | FC_SOF_F;
}

static inline int fc_sof_is_init(enum fc_sof sof)
{
	return sof < 0x30;
}

#endif 
