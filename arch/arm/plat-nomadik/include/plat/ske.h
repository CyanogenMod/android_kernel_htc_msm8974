/*
 * Copyright (C) ST-Ericsson SA 2010
 *
 * License Terms: GNU General Public License v2
 * Author: Naveen Kumar Gaddipati <naveen.gaddipati@stericsson.com>
 *
 * ux500 Scroll key and Keypad Encoder (SKE) header
 */

#ifndef __SKE_H
#define __SKE_H

#include <linux/input/matrix_keypad.h>

#define SKE_CR		0x00
#define SKE_VAL0	0x04
#define SKE_VAL1	0x08
#define SKE_DBCR	0x0C
#define SKE_IMSC	0x10
#define SKE_RIS		0x14
#define SKE_MIS		0x18
#define SKE_ICR		0x1C


struct ske_keypad_platform_data {
	int (*init)(void);
	int (*exit)(void);
	const struct matrix_keymap_data *keymap_data;
	u8 krow;
	u8 kcol;
	u8 debounce_ms;
	bool no_autorepeat;
	bool wakeup_enable;
};
#endif	
