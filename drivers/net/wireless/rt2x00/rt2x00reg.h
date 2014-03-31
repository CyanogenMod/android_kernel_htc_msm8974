/*
	Copyright (C) 2004 - 2009 Ivo van Doorn <IvDoorn@gmail.com>
	<http://rt2x00.serialmonkey.com>

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the
	Free Software Foundation, Inc.,
	59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */


#ifndef RT2X00REG_H
#define RT2X00REG_H

enum rx_crypto {
	RX_CRYPTO_SUCCESS = 0,
	RX_CRYPTO_FAIL_ICV = 1,
	RX_CRYPTO_FAIL_MIC = 2,
	RX_CRYPTO_FAIL_KEY = 3,
};

enum antenna {
	ANTENNA_SW_DIVERSITY = 0,
	ANTENNA_A = 1,
	ANTENNA_B = 2,
	ANTENNA_HW_DIVERSITY = 3,
};

enum led_mode {
	LED_MODE_DEFAULT = 0,
	LED_MODE_TXRX_ACTIVITY = 1,
	LED_MODE_SIGNAL_STRENGTH = 2,
	LED_MODE_ASUS = 3,
	LED_MODE_ALPHA = 4,
};

enum tsf_sync {
	TSF_SYNC_NONE = 0,
	TSF_SYNC_INFRA = 1,
	TSF_SYNC_ADHOC = 2,
	TSF_SYNC_AP_NONE = 3,
};

enum dev_state {
	STATE_DEEP_SLEEP = 0,
	STATE_SLEEP = 1,
	STATE_STANDBY = 2,
	STATE_AWAKE = 3,

	STATE_RADIO_ON,
	STATE_RADIO_OFF,
	STATE_RADIO_IRQ_ON,
	STATE_RADIO_IRQ_OFF,
};

enum ifs {
	IFS_BACKOFF = 0,
	IFS_SIFS = 1,
	IFS_NEW_BACKOFF = 2,
	IFS_NONE = 3,
};

enum txop {
	TXOP_HTTXOP = 0,
	TXOP_PIFS = 1,
	TXOP_SIFS = 2,
	TXOP_BACKOFF = 3,
};

enum cipher {
	CIPHER_NONE = 0,
	CIPHER_WEP64 = 1,
	CIPHER_WEP128 = 2,
	CIPHER_TKIP = 3,
	CIPHER_AES = 4,
	CIPHER_CKIP64 = 5,
	CIPHER_CKIP128 = 6,
	CIPHER_TKIP_NO_MIC = 7, 

	CIPHER_MAX = 4,
};

enum rate_modulation {
	RATE_MODE_CCK = 0,
	RATE_MODE_OFDM = 1,
	RATE_MODE_HT_MIX = 2,
	RATE_MODE_HT_GREENFIELD = 3,
};

enum firmware_errors {
	FW_OK,
	FW_BAD_CRC,
	FW_BAD_LENGTH,
	FW_BAD_VERSION,
};

struct rt2x00_field8 {
	u8 bit_offset;
	u8 bit_mask;
};

struct rt2x00_field16 {
	u16 bit_offset;
	u16 bit_mask;
};

struct rt2x00_field32 {
	u32 bit_offset;
	u32 bit_mask;
};

#define is_power_of_two(x)	( !((x) & ((x)-1)) )
#define low_bit_mask(x)		( ((x)-1) & ~(x) )
#define is_valid_mask(x)	is_power_of_two(1LU + (x) + low_bit_mask(x))

#define compile_ffs2(__x) \
	__builtin_choose_expr(((__x) & 0x1), 0, 1)

#define compile_ffs4(__x) \
	__builtin_choose_expr(((__x) & 0x3), \
			      (compile_ffs2((__x))), \
			      (compile_ffs2((__x) >> 2) + 2))

#define compile_ffs8(__x) \
	__builtin_choose_expr(((__x) & 0xf), \
			      (compile_ffs4((__x))), \
			      (compile_ffs4((__x) >> 4) + 4))

#define compile_ffs16(__x) \
	__builtin_choose_expr(((__x) & 0xff), \
			      (compile_ffs8((__x))), \
			      (compile_ffs8((__x) >> 8) + 8))

#define compile_ffs32(__x) \
	__builtin_choose_expr(((__x) & 0xffff), \
			      (compile_ffs16((__x))), \
			      (compile_ffs16((__x) >> 16) + 16))

#define FIELD_CHECK(__mask, __type)			\
	BUILD_BUG_ON(!(__mask) ||			\
		     !is_valid_mask(__mask) ||		\
		     (__mask) != (__type)(__mask))	\

#define FIELD8(__mask)				\
({						\
	FIELD_CHECK(__mask, u8);		\
	(struct rt2x00_field8) {		\
		compile_ffs8(__mask), (__mask)	\
	};					\
})

#define FIELD16(__mask)				\
({						\
	FIELD_CHECK(__mask, u16);		\
	(struct rt2x00_field16) {		\
		compile_ffs16(__mask), (__mask)	\
	};					\
})

#define FIELD32(__mask)				\
({						\
	FIELD_CHECK(__mask, u32);		\
	(struct rt2x00_field32) {		\
		compile_ffs32(__mask), (__mask)	\
	};					\
})

#define SET_FIELD(__reg, __type, __field, __value)\
({						\
	typecheck(__type, __field);		\
	*(__reg) &= ~((__field).bit_mask);	\
	*(__reg) |= ((__value) <<		\
	    ((__field).bit_offset)) &		\
	    ((__field).bit_mask);		\
})

#define GET_FIELD(__reg, __type, __field)	\
({						\
	typecheck(__type, __field);		\
	((__reg) & ((__field).bit_mask)) >>	\
	    ((__field).bit_offset);		\
})

#define rt2x00_set_field32(__reg, __field, __value) \
	SET_FIELD(__reg, struct rt2x00_field32, __field, __value)
#define rt2x00_get_field32(__reg, __field) \
	GET_FIELD(__reg, struct rt2x00_field32, __field)

#define rt2x00_set_field16(__reg, __field, __value) \
	SET_FIELD(__reg, struct rt2x00_field16, __field, __value)
#define rt2x00_get_field16(__reg, __field) \
	GET_FIELD(__reg, struct rt2x00_field16, __field)

#define rt2x00_set_field8(__reg, __field, __value) \
	SET_FIELD(__reg, struct rt2x00_field8, __field, __value)
#define rt2x00_get_field8(__reg, __field) \
	GET_FIELD(__reg, struct rt2x00_field8, __field)

#endif 
