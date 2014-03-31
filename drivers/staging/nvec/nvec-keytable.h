/*
 * drivers/input/keyboard/tegra-nvec.c
 *
 * Keyboard class input driver for keyboards connected to an NvEc compliant
 * embedded controller
 *
 * Copyright (c) 2009, NVIDIA Corporation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

static unsigned short code_tab_102us[] = {
	
	KEY_GRAVE,
	KEY_ESC,
	KEY_1,
	KEY_2,
	KEY_3,
	KEY_4,
	KEY_5,
	KEY_6,
	KEY_7,
	KEY_8,
	KEY_9,
	KEY_0,
	KEY_MINUS,
	KEY_EQUAL,
	KEY_BACKSPACE,
	KEY_TAB,
	
	KEY_Q,
	KEY_W,
	KEY_E,
	KEY_R,
	KEY_T,
	KEY_Y,
	KEY_U,
	KEY_I,
	KEY_O,
	KEY_P,
	KEY_LEFTBRACE,
	KEY_RIGHTBRACE,
	KEY_ENTER,
	KEY_LEFTCTRL,
	KEY_A,
	KEY_S,
	
	KEY_D,
	KEY_F,
	KEY_G,
	KEY_H,
	KEY_J,
	KEY_K,
	KEY_L,
	KEY_SEMICOLON,
	KEY_APOSTROPHE,
	KEY_GRAVE,
	KEY_LEFTSHIFT,
	KEY_BACKSLASH,
	KEY_Z,
	KEY_X,
	KEY_C,
	KEY_V,
	
	KEY_B,
	KEY_N,
	KEY_M,
	KEY_COMMA,
	KEY_DOT,
	KEY_SLASH,
	KEY_RIGHTSHIFT,
	KEY_KPASTERISK,
	KEY_LEFTALT,
	KEY_SPACE,
	KEY_CAPSLOCK,
	KEY_F1,
	KEY_F2,
	KEY_F3,
	KEY_F4,
	KEY_F5,
	
	KEY_F6,
	KEY_F7,
	KEY_F8,
	KEY_F9,
	KEY_F10,
	KEY_FN,
	
	0,
	KEY_KP7,
	KEY_KP8,
	KEY_KP9,
	KEY_KPMINUS,
	KEY_KP4,
	KEY_KP5,
	KEY_KP6,
	KEY_KPPLUS,
	KEY_KP1,
	
	KEY_KP2,
	KEY_KP3,
	KEY_KP0,
	KEY_KPDOT,
	
	KEY_MENU,
	KEY_POWER,
	
	KEY_102ND,
	KEY_F11,
	KEY_F12,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	
	0,
	0,
	0,
	KEY_SEARCH,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	
	0,
	0,
	0,
	KEY_KP5,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	KEY_KP9,
};

static unsigned short extcode_tab_us102[] = {
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	
	0,
	0,
	0,
	
	0,
	KEY_RIGHTCTRL,
	0,
	0,
	
	KEY_MUTE,
	
	0,
	
	0,
	0,
	
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	
	KEY_VOLUMEUP,
	0,
	
	0,
	0,
	0,
	
	KEY_KPSLASH,
	0,
	
	KEY_SYSRQ,
	
	KEY_RIGHTALT,
	
	0,
	
	0,
	
	0,
	
	0,
	
	0,
	
	0,
	
	0,
	
	0,
	KEY_LEFT,
	0,
	0,
	0,
	0,
	KEY_CANCEL,
	KEY_HOME,
	KEY_UP,
	KEY_PAGEUP,
	0,
	KEY_LEFT,
	0,
	KEY_RIGHT,
	0,
	KEY_END,
	
	KEY_DOWN,
	KEY_PAGEDOWN,
	KEY_INSERT,
	KEY_DELETE,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	KEY_LEFTMETA,
	0,
	KEY_ESC,
	KEY_KPMINUS,
	0,
	0,
	0,
	0,
	0,
	0,
	
	0,
	
	0,
	
	0,
	
	0,
	
	0,
	
	0,
	
	0,
	
	0,
	
	0,
};

static unsigned short *code_tabs[] = { code_tab_102us, extcode_tab_us102 };
