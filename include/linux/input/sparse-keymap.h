#ifndef _SPARSE_KEYMAP_H
#define _SPARSE_KEYMAP_H

/*
 * Copyright (c) 2009 Dmitry Torokhov
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 */

#define KE_END		0	
#define KE_KEY		1	
#define KE_SW		2	
#define KE_VSW		3	
#define KE_IGNORE	4	
#define KE_LAST		KE_IGNORE

struct key_entry {
	int type;		
	u32 code;
	union {
		u16 keycode;		
		struct {		
			u8 code;
			u8 value;	
		} sw;
	};
};

struct key_entry *sparse_keymap_entry_from_scancode(struct input_dev *dev,
						    unsigned int code);
struct key_entry *sparse_keymap_entry_from_keycode(struct input_dev *dev,
						   unsigned int code);
int sparse_keymap_setup(struct input_dev *dev,
			const struct key_entry *keymap,
			int (*setup)(struct input_dev *, struct key_entry *));
void sparse_keymap_free(struct input_dev *dev);

void sparse_keymap_report_entry(struct input_dev *dev, const struct key_entry *ke,
				unsigned int value, bool autorelease);

bool sparse_keymap_report_event(struct input_dev *dev, unsigned int code,
				unsigned int value, bool autorelease);

#endif 
