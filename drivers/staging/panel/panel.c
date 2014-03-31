/*
 * Front panel driver for Linux
 * Copyright (C) 2000-2008, Willy Tarreau <w@1wt.eu>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 *
 * This code drives an LCD module (/dev/lcd), and a keypad (/dev/keypad)
 * connected to a parallel printer port.
 *
 * The LCD module may either be an HD44780-like 8-bit parallel LCD, or a 1-bit
 * serial module compatible with Samsung's KS0074. The pins may be connected in
 * any combination, everything is programmable.
 *
 * The keypad consists in a matrix of push buttons connecting input pins to
 * data output pins or to the ground. The combinations have to be hard-coded
 * in the driver, though several profiles exist and adding new ones is easy.
 *
 * Several profiles are provided for commonly found LCD+keypad modules on the
 * market, such as those found in Nexcom's appliances.
 *
 * FIXME:
 *      - the initialization/deinitialization process is very dirty and should
 *        be rewritten. It may even be buggy.
 *
 * TODO:
 *	- document 24 keys keyboard (3 rows of 8 cols, 32 diodes + 2 inputs)
 *      - make the LCD a part of a virtual screen of Vx*Vy
 *	- make the inputs list smp-safe
 *      - change the keyboard to a double mapping : signals -> key_id -> values
 *        so that applications can change values without knowing signals
 *
 */

#include <linux/module.h>

#include <linux/types.h>
#include <linux/errno.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <linux/ioport.h>
#include <linux/fcntl.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/ctype.h>
#include <linux/parport.h>
#include <linux/list.h>
#include <linux/notifier.h>
#include <linux/reboot.h>
#include <generated/utsrelease.h>

#include <linux/io.h>
#include <linux/uaccess.h>

#define LCD_MINOR		156
#define KEYPAD_MINOR		185

#define PANEL_VERSION		"0.9.5"

#define LCD_MAXBYTES		256	

#define KEYPAD_BUFFER		64

#define INPUT_POLL_TIME		(HZ/50)
#define KEYPAD_REP_START	(10)
#define KEYPAD_REP_DELAY	(2)

#define FLASH_LIGHT_TEMPO	(200)

#define PNL_PINPUT(a)		((((unsigned char)(a)) ^ 0x7F) >> 3)

#define PNL_PBUSY		0x80	
#define PNL_PACK		0x40	
#define PNL_POUTPA		0x20	
#define PNL_PSELECD		0x10	
#define PNL_PERRORP		0x08	

#define PNL_PBIDIR		0x20	
#define PNL_PINTEN		0x10
#define PNL_PSELECP		0x08	
#define PNL_PINITP		0x04	
#define PNL_PAUTOLF		0x02	
#define PNL_PSTROBE		0x01	

#define PNL_PD0			0x01
#define PNL_PD1			0x02
#define PNL_PD2			0x04
#define PNL_PD3			0x08
#define PNL_PD4			0x10
#define PNL_PD5			0x20
#define PNL_PD6			0x40
#define PNL_PD7			0x80

#define PIN_NONE		0
#define PIN_STROBE		1
#define PIN_D0			2
#define PIN_D1			3
#define PIN_D2			4
#define PIN_D3			5
#define PIN_D4			6
#define PIN_D5			7
#define PIN_D6			8
#define PIN_D7			9
#define PIN_AUTOLF		14
#define PIN_INITP		16
#define PIN_SELECP		17
#define PIN_NOT_SET		127

#define LCD_FLAG_S		0x0001
#define LCD_FLAG_ID		0x0002
#define LCD_FLAG_B		0x0004	
#define LCD_FLAG_C		0x0008	
#define LCD_FLAG_D		0x0010	
#define LCD_FLAG_F		0x0020	
#define LCD_FLAG_N		0x0040	
#define LCD_FLAG_L		0x0080	

#define LCD_ESCAPE_LEN		24	
#define LCD_ESCAPE_CHAR	27	

#define r_ctr(x)        (parport_read_control((x)->port))
#define r_dtr(x)        (parport_read_data((x)->port))
#define r_str(x)        (parport_read_status((x)->port))
#define w_ctr(x, y)     do { parport_write_control((x)->port, (y)); } while (0)
#define w_dtr(x, y)     do { parport_write_data((x)->port, (y)); } while (0)

static __u8 scan_mask_o;
static __u8 scan_mask_i;

typedef __u64 pmask_t;

enum input_type {
	INPUT_TYPE_STD,
	INPUT_TYPE_KBD,
};

enum input_state {
	INPUT_ST_LOW,
	INPUT_ST_RISING,
	INPUT_ST_HIGH,
	INPUT_ST_FALLING,
};

struct logical_input {
	struct list_head list;
	pmask_t mask;
	pmask_t value;
	enum input_type type;
	enum input_state state;
	__u8 rise_time, fall_time;
	__u8 rise_timer, fall_timer, high_timer;

	union {
		struct {	
			void (*press_fct) (int);
			void (*release_fct) (int);
			int press_data;
			int release_data;
		} std;
		struct {	
			
			char press_str[sizeof(void *) + sizeof(int)];
			char repeat_str[sizeof(void *) + sizeof(int)];
			char release_str[sizeof(void *) + sizeof(int)];
		} kbd;
	} u;
};

LIST_HEAD(logical_inputs);	


static pmask_t phys_read;
static pmask_t phys_read_prev;
static pmask_t phys_curr;
static pmask_t phys_prev;
static char inputs_stable;

static char keypad_buffer[KEYPAD_BUFFER];
static int keypad_buflen;
static int keypad_start;
static char keypressed;
static wait_queue_head_t keypad_read_wait;


static unsigned long int lcd_flags;
static unsigned long int lcd_addr_x;
static unsigned long int lcd_addr_y;
static char lcd_escape[LCD_ESCAPE_LEN + 1];
static int lcd_escape_len = -1;

#define BIT_CLR		0
#define BIT_SET		1
#define BIT_MSK		2
#define BIT_STATES	3
#define LCD_BIT_E	0
#define LCD_BIT_RS	1
#define LCD_BIT_RW	2
#define LCD_BIT_BL	3
#define LCD_BIT_CL	4
#define LCD_BIT_DA	5
#define LCD_BITS	6

#define LCD_PORT_C	0
#define LCD_PORT_D	1
#define LCD_PORTS	2

static unsigned char lcd_bits[LCD_PORTS][LCD_BITS][BIT_STATES];

#define LCD_PROTO_PARALLEL      0
#define LCD_PROTO_SERIAL        1
#define LCD_PROTO_TI_DA8XX_LCD	2

#define LCD_CHARSET_NORMAL      0
#define LCD_CHARSET_KS0074      1

#define LCD_TYPE_NONE		0
#define LCD_TYPE_OLD		1
#define LCD_TYPE_KS0074		2
#define LCD_TYPE_HANTRONIX	3
#define LCD_TYPE_NEXCOM		4
#define LCD_TYPE_CUSTOM		5

#define KEYPAD_TYPE_NONE	0
#define KEYPAD_TYPE_OLD		1
#define KEYPAD_TYPE_NEW		2
#define KEYPAD_TYPE_NEXCOM	3

#define PANEL_PROFILE_CUSTOM	0
#define PANEL_PROFILE_OLD	1
#define PANEL_PROFILE_NEW	2
#define PANEL_PROFILE_HANTRONIX	3
#define PANEL_PROFILE_NEXCOM	4
#define PANEL_PROFILE_LARGE	5

#define DEFAULT_PROFILE         PANEL_PROFILE_LARGE
#define DEFAULT_PARPORT         0
#define DEFAULT_LCD             LCD_TYPE_OLD
#define DEFAULT_KEYPAD          KEYPAD_TYPE_OLD
#define DEFAULT_LCD_WIDTH       40
#define DEFAULT_LCD_BWIDTH      40
#define DEFAULT_LCD_HWIDTH      64
#define DEFAULT_LCD_HEIGHT      2
#define DEFAULT_LCD_PROTO       LCD_PROTO_PARALLEL

#define DEFAULT_LCD_PIN_E       PIN_AUTOLF
#define DEFAULT_LCD_PIN_RS      PIN_SELECP
#define DEFAULT_LCD_PIN_RW      PIN_INITP
#define DEFAULT_LCD_PIN_SCL     PIN_STROBE
#define DEFAULT_LCD_PIN_SDA     PIN_D0
#define DEFAULT_LCD_PIN_BL      PIN_NOT_SET
#define DEFAULT_LCD_CHARSET     LCD_CHARSET_NORMAL

#ifdef CONFIG_PANEL_PROFILE
#undef DEFAULT_PROFILE
#define DEFAULT_PROFILE CONFIG_PANEL_PROFILE
#endif

#ifdef CONFIG_PANEL_PARPORT
#undef DEFAULT_PARPORT
#define DEFAULT_PARPORT CONFIG_PANEL_PARPORT
#endif

#if DEFAULT_PROFILE == 0	
#ifdef CONFIG_PANEL_KEYPAD
#undef DEFAULT_KEYPAD
#define DEFAULT_KEYPAD CONFIG_PANEL_KEYPAD
#endif

#ifdef CONFIG_PANEL_LCD
#undef DEFAULT_LCD
#define DEFAULT_LCD CONFIG_PANEL_LCD
#endif

#ifdef CONFIG_PANEL_LCD_WIDTH
#undef DEFAULT_LCD_WIDTH
#define DEFAULT_LCD_WIDTH CONFIG_PANEL_LCD_WIDTH
#endif

#ifdef CONFIG_PANEL_LCD_BWIDTH
#undef DEFAULT_LCD_BWIDTH
#define DEFAULT_LCD_BWIDTH CONFIG_PANEL_LCD_BWIDTH
#endif

#ifdef CONFIG_PANEL_LCD_HWIDTH
#undef DEFAULT_LCD_HWIDTH
#define DEFAULT_LCD_HWIDTH CONFIG_PANEL_LCD_HWIDTH
#endif

#ifdef CONFIG_PANEL_LCD_HEIGHT
#undef DEFAULT_LCD_HEIGHT
#define DEFAULT_LCD_HEIGHT CONFIG_PANEL_LCD_HEIGHT
#endif

#ifdef CONFIG_PANEL_LCD_PROTO
#undef DEFAULT_LCD_PROTO
#define DEFAULT_LCD_PROTO CONFIG_PANEL_LCD_PROTO
#endif

#ifdef CONFIG_PANEL_LCD_PIN_E
#undef DEFAULT_LCD_PIN_E
#define DEFAULT_LCD_PIN_E CONFIG_PANEL_LCD_PIN_E
#endif

#ifdef CONFIG_PANEL_LCD_PIN_RS
#undef DEFAULT_LCD_PIN_RS
#define DEFAULT_LCD_PIN_RS CONFIG_PANEL_LCD_PIN_RS
#endif

#ifdef CONFIG_PANEL_LCD_PIN_RW
#undef DEFAULT_LCD_PIN_RW
#define DEFAULT_LCD_PIN_RW CONFIG_PANEL_LCD_PIN_RW
#endif

#ifdef CONFIG_PANEL_LCD_PIN_SCL
#undef DEFAULT_LCD_PIN_SCL
#define DEFAULT_LCD_PIN_SCL CONFIG_PANEL_LCD_PIN_SCL
#endif

#ifdef CONFIG_PANEL_LCD_PIN_SDA
#undef DEFAULT_LCD_PIN_SDA
#define DEFAULT_LCD_PIN_SDA CONFIG_PANEL_LCD_PIN_SDA
#endif

#ifdef CONFIG_PANEL_LCD_PIN_BL
#undef DEFAULT_LCD_PIN_BL
#define DEFAULT_LCD_PIN_BL CONFIG_PANEL_LCD_PIN_BL
#endif

#ifdef CONFIG_PANEL_LCD_CHARSET
#undef DEFAULT_LCD_CHARSET
#define DEFAULT_LCD_CHARSET CONFIG_PANEL_LCD_CHARSET
#endif

#endif 

static int keypad_open_cnt;	
static int lcd_open_cnt;	
static struct pardevice *pprt;

static int lcd_initialized;
static int keypad_initialized;

static int light_tempo;

static char lcd_must_clear;
static char lcd_left_shift;
static char init_in_progress;

static void (*lcd_write_cmd) (int);
static void (*lcd_write_data) (int);
static void (*lcd_clear_fast) (void);

static DEFINE_SPINLOCK(pprt_lock);
static struct timer_list scan_timer;

MODULE_DESCRIPTION("Generic parallel port LCD/Keypad driver");

static int parport = -1;
module_param(parport, int, 0000);
MODULE_PARM_DESC(parport, "Parallel port index (0=lpt1, 1=lpt2, ...)");

static int lcd_height = -1;
module_param(lcd_height, int, 0000);
MODULE_PARM_DESC(lcd_height, "Number of lines on the LCD");

static int lcd_width = -1;
module_param(lcd_width, int, 0000);
MODULE_PARM_DESC(lcd_width, "Number of columns on the LCD");

static int lcd_bwidth = -1;	
module_param(lcd_bwidth, int, 0000);
MODULE_PARM_DESC(lcd_bwidth, "Internal LCD line width (40)");

static int lcd_hwidth = -1;	
module_param(lcd_hwidth, int, 0000);
MODULE_PARM_DESC(lcd_hwidth, "LCD line hardware address (64)");

static int lcd_enabled = -1;
module_param(lcd_enabled, int, 0000);
MODULE_PARM_DESC(lcd_enabled, "Deprecated option, use lcd_type instead");

static int keypad_enabled = -1;
module_param(keypad_enabled, int, 0000);
MODULE_PARM_DESC(keypad_enabled, "Deprecated option, use keypad_type instead");

static int lcd_type = -1;
module_param(lcd_type, int, 0000);
MODULE_PARM_DESC(lcd_type,
		 "LCD type: 0=none, 1=old //, 2=serial ks0074, "
		 "3=hantronix //, 4=nexcom //, 5=compiled-in");

static int lcd_proto = -1;
module_param(lcd_proto, int, 0000);
MODULE_PARM_DESC(lcd_proto,
		"LCD communication: 0=parallel (//), 1=serial,"
		"2=TI LCD Interface");

static int lcd_charset = -1;
module_param(lcd_charset, int, 0000);
MODULE_PARM_DESC(lcd_charset, "LCD character set: 0=standard, 1=KS0074");

static int keypad_type = -1;
module_param(keypad_type, int, 0000);
MODULE_PARM_DESC(keypad_type,
		 "Keypad type: 0=none, 1=old 6 keys, 2=new 6+1 keys, "
		 "3=nexcom 4 keys");

static int profile = DEFAULT_PROFILE;
module_param(profile, int, 0000);
MODULE_PARM_DESC(profile,
		 "1=16x2 old kp; 2=serial 16x2, new kp; 3=16x2 hantronix; "
		 "4=16x2 nexcom; default=40x2, old kp");


static int lcd_e_pin  = PIN_NOT_SET;
module_param(lcd_e_pin, int, 0000);
MODULE_PARM_DESC(lcd_e_pin,
		 "# of the // port pin connected to LCD 'E' signal, "
		 "with polarity (-17..17)");

static int lcd_rs_pin = PIN_NOT_SET;
module_param(lcd_rs_pin, int, 0000);
MODULE_PARM_DESC(lcd_rs_pin,
		 "# of the // port pin connected to LCD 'RS' signal, "
		 "with polarity (-17..17)");

static int lcd_rw_pin = PIN_NOT_SET;
module_param(lcd_rw_pin, int, 0000);
MODULE_PARM_DESC(lcd_rw_pin,
		 "# of the // port pin connected to LCD 'RW' signal, "
		 "with polarity (-17..17)");

static int lcd_bl_pin = PIN_NOT_SET;
module_param(lcd_bl_pin, int, 0000);
MODULE_PARM_DESC(lcd_bl_pin,
		 "# of the // port pin connected to LCD backlight, "
		 "with polarity (-17..17)");

static int lcd_da_pin = PIN_NOT_SET;
module_param(lcd_da_pin, int, 0000);
MODULE_PARM_DESC(lcd_da_pin,
		 "# of the // port pin connected to serial LCD 'SDA' "
		 "signal, with polarity (-17..17)");

static int lcd_cl_pin = PIN_NOT_SET;
module_param(lcd_cl_pin, int, 0000);
MODULE_PARM_DESC(lcd_cl_pin,
		 "# of the // port pin connected to serial LCD 'SCL' "
		 "signal, with polarity (-17..17)");

static unsigned char *lcd_char_conv;

static unsigned char lcd_char_conv_ks0074[256] = {
	
	 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
	 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
	 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
	 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
	 0x20, 0x21, 0x22, 0x23, 0xa2, 0x25, 0x26, 0x27,
	 0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f,
	 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
	 0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f,
	 0xa0, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47,
	 0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f,
	 0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57,
	 0x58, 0x59, 0x5a, 0xfa, 0xfb, 0xfc, 0x1d, 0xc4,
	 0x96, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67,
	 0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f,
	 0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77,
	 0x78, 0x79, 0x7a, 0xfd, 0xfe, 0xff, 0xce, 0x20,
	 0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
	 0x88, 0x89, 0x8a, 0x8b, 0x8c, 0x8d, 0x8e, 0x8f,
	 0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97,
	 0x98, 0x99, 0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f,
	 0x20, 0x40, 0xb1, 0xa1, 0x24, 0xa3, 0xfe, 0x5f,
	 0x22, 0xc8, 0x61, 0x14, 0x97, 0x2d, 0xad, 0x96,
	 0x80, 0x8c, 0x82, 0x83, 0x27, 0x8f, 0x86, 0xdd,
	 0x2c, 0x81, 0x6f, 0x15, 0x8b, 0x8a, 0x84, 0x60,
	 0xe2, 0xe2, 0xe2, 0x5b, 0x5b, 0xae, 0xbc, 0xa9,
	 0xc5, 0xbf, 0xc6, 0xf1, 0xe3, 0xe3, 0xe3, 0xe3,
	 0x44, 0x5d, 0xa8, 0xe4, 0xec, 0xec, 0x5c, 0x78,
	 0xab, 0xa6, 0xe5, 0x5e, 0x5e, 0xe6, 0xaa, 0xbe,
	 0x7f, 0xe7, 0xaf, 0x7b, 0x7b, 0xaf, 0xbd, 0xc8,
	 0xa4, 0xa5, 0xc7, 0xf6, 0xa7, 0xe8, 0x69, 0x69,
	 0xed, 0x7d, 0xa8, 0xe4, 0xec, 0x5c, 0x5c, 0x25,
	 0xac, 0xa6, 0xea, 0xef, 0x7e, 0xeb, 0xb2, 0x79,
};

char old_keypad_profile[][4][9] = {
	{"S0", "Left\n", "Left\n", ""},
	{"S1", "Down\n", "Down\n", ""},
	{"S2", "Up\n", "Up\n", ""},
	{"S3", "Right\n", "Right\n", ""},
	{"S4", "Esc\n", "Esc\n", ""},
	{"S5", "Ret\n", "Ret\n", ""},
	{"", "", "", ""}
};

char new_keypad_profile[][4][9] = {
	{"S0", "Left\n", "Left\n", ""},
	{"S1", "Down\n", "Down\n", ""},
	{"S2", "Up\n", "Up\n", ""},
	{"S3", "Right\n", "Right\n", ""},
	{"S4s5", "", "Esc\n", "Esc\n"},
	{"s4S5", "", "Ret\n", "Ret\n"},
	{"S4S5", "Help\n", "", ""},
	
	{"", "", "", ""}
};

char nexcom_keypad_profile[][4][9] = {
	{"a-p-e-", "Down\n", "Down\n", ""},
	{"a-p-E-", "Ret\n", "Ret\n", ""},
	{"a-P-E-", "Esc\n", "Esc\n", ""},
	{"a-P-e-", "Up\n", "Up\n", ""},
	
	{"", "", "", ""}
};

static char (*keypad_profile)[4][9] = old_keypad_profile;

static struct {
	unsigned char e;  
	unsigned char rs; 
	unsigned char rw; 
	unsigned char bl; 
	unsigned char cl; 
	unsigned char da; 
} bits;

static void init_scan_timer(void);

static int set_data_bits(void)
{
	int val, bit;

	val = r_dtr(pprt);
	for (bit = 0; bit < LCD_BITS; bit++)
		val &= lcd_bits[LCD_PORT_D][bit][BIT_MSK];

	val |= lcd_bits[LCD_PORT_D][LCD_BIT_E][bits.e]
	    | lcd_bits[LCD_PORT_D][LCD_BIT_RS][bits.rs]
	    | lcd_bits[LCD_PORT_D][LCD_BIT_RW][bits.rw]
	    | lcd_bits[LCD_PORT_D][LCD_BIT_BL][bits.bl]
	    | lcd_bits[LCD_PORT_D][LCD_BIT_CL][bits.cl]
	    | lcd_bits[LCD_PORT_D][LCD_BIT_DA][bits.da];

	w_dtr(pprt, val);
	return val;
}

static int set_ctrl_bits(void)
{
	int val, bit;

	val = r_ctr(pprt);
	for (bit = 0; bit < LCD_BITS; bit++)
		val &= lcd_bits[LCD_PORT_C][bit][BIT_MSK];

	val |= lcd_bits[LCD_PORT_C][LCD_BIT_E][bits.e]
	    | lcd_bits[LCD_PORT_C][LCD_BIT_RS][bits.rs]
	    | lcd_bits[LCD_PORT_C][LCD_BIT_RW][bits.rw]
	    | lcd_bits[LCD_PORT_C][LCD_BIT_BL][bits.bl]
	    | lcd_bits[LCD_PORT_C][LCD_BIT_CL][bits.cl]
	    | lcd_bits[LCD_PORT_C][LCD_BIT_DA][bits.da];

	w_ctr(pprt, val);
	return val;
}

static void panel_set_bits(void)
{
	set_data_bits();
	set_ctrl_bits();
}

void pin_to_bits(int pin, unsigned char *d_val, unsigned char *c_val)
{
	int d_bit, c_bit, inv;

	d_val[0] = c_val[0] = d_val[1] = c_val[1] = 0;
	d_val[2] = c_val[2] = 0xFF;

	if (pin == 0)
		return;

	inv = (pin < 0);
	if (inv)
		pin = -pin;

	d_bit = c_bit = 0;

	switch (pin) {
	case PIN_STROBE:	
		c_bit = PNL_PSTROBE;
		inv = !inv;
		break;
	case PIN_D0...PIN_D7:	
		d_bit = 1 << (pin - 2);
		break;
	case PIN_AUTOLF:	
		c_bit = PNL_PAUTOLF;
		inv = !inv;
		break;
	case PIN_INITP:		
		c_bit = PNL_PINITP;
		break;
	case PIN_SELECP:	
		c_bit = PNL_PSELECP;
		inv = !inv;
		break;
	default:		
		break;
	}

	if (c_bit) {
		c_val[2] &= ~c_bit;
		c_val[!inv] = c_bit;
	} else if (d_bit) {
		d_val[2] &= ~d_bit;
		d_val[!inv] = d_bit;
	}
}

static void long_sleep(int ms)
{

	if (in_interrupt())
		mdelay(ms);
	else {
		current->state = TASK_INTERRUPTIBLE;
		schedule_timeout((ms * HZ + 999) / 1000);
	}
}

static void lcd_send_serial(int byte)
{
	int bit;

	for (bit = 0; bit < 8; bit++) {
		bits.cl = BIT_CLR;	
		panel_set_bits();
		bits.da = byte & 1;
		panel_set_bits();
		udelay(2);  
		bits.cl = BIT_SET;	
		panel_set_bits();
		udelay(1);  
		byte >>= 1;
	}
}

static void lcd_backlight(int on)
{
	if (lcd_bl_pin == PIN_NONE)
		return;

	
	spin_lock(&pprt_lock);
	bits.bl = on;
	panel_set_bits();
	spin_unlock(&pprt_lock);
}

static void lcd_write_cmd_s(int cmd)
{
	spin_lock(&pprt_lock);
	lcd_send_serial(0x1F);	
	lcd_send_serial(cmd & 0x0F);
	lcd_send_serial((cmd >> 4) & 0x0F);
	udelay(40);		
	spin_unlock(&pprt_lock);
}

static void lcd_write_data_s(int data)
{
	spin_lock(&pprt_lock);
	lcd_send_serial(0x5F);	
	lcd_send_serial(data & 0x0F);
	lcd_send_serial((data >> 4) & 0x0F);
	udelay(40);		
	spin_unlock(&pprt_lock);
}

static void lcd_write_cmd_p8(int cmd)
{
	spin_lock(&pprt_lock);
	
	w_dtr(pprt, cmd);
	udelay(20);	

	bits.e = BIT_SET;
	bits.rs = BIT_CLR;
	bits.rw = BIT_CLR;
	set_ctrl_bits();

	udelay(40);	

	bits.e = BIT_CLR;
	set_ctrl_bits();

	udelay(120);	
	spin_unlock(&pprt_lock);
}

static void lcd_write_data_p8(int data)
{
	spin_lock(&pprt_lock);
	
	w_dtr(pprt, data);
	udelay(20);	

	bits.e = BIT_SET;
	bits.rs = BIT_SET;
	bits.rw = BIT_CLR;
	set_ctrl_bits();

	udelay(40);	

	bits.e = BIT_CLR;
	set_ctrl_bits();

	udelay(45);	
	spin_unlock(&pprt_lock);
}

static void lcd_write_cmd_tilcd(int cmd)
{
	spin_lock(&pprt_lock);
	
	w_ctr(pprt, cmd);
	udelay(60);
	spin_unlock(&pprt_lock);
}

static void lcd_write_data_tilcd(int data)
{
	spin_lock(&pprt_lock);
	
	w_dtr(pprt, data);
	udelay(60);
	spin_unlock(&pprt_lock);
}

static void lcd_gotoxy(void)
{
	lcd_write_cmd(0x80	
		      | (lcd_addr_y ? lcd_hwidth : 0)
		      | ((lcd_addr_x < lcd_bwidth) ? lcd_addr_x &
			 (lcd_hwidth - 1) : lcd_bwidth - 1));
}

static void lcd_print(char c)
{
	if (lcd_addr_x < lcd_bwidth) {
		if (lcd_char_conv != NULL)
			c = lcd_char_conv[(unsigned char)c];
		lcd_write_data(c);
		lcd_addr_x++;
	}
	
	if (lcd_addr_x == lcd_bwidth)
		lcd_gotoxy();
}

static void lcd_clear_fast_s(void)
{
	int pos;
	lcd_addr_x = lcd_addr_y = 0;
	lcd_gotoxy();

	spin_lock(&pprt_lock);
	for (pos = 0; pos < lcd_height * lcd_hwidth; pos++) {
		lcd_send_serial(0x5F);	
		lcd_send_serial(' ' & 0x0F);
		lcd_send_serial((' ' >> 4) & 0x0F);
		udelay(40);	
	}
	spin_unlock(&pprt_lock);

	lcd_addr_x = lcd_addr_y = 0;
	lcd_gotoxy();
}

static void lcd_clear_fast_p8(void)
{
	int pos;
	lcd_addr_x = lcd_addr_y = 0;
	lcd_gotoxy();

	spin_lock(&pprt_lock);
	for (pos = 0; pos < lcd_height * lcd_hwidth; pos++) {
		
		w_dtr(pprt, ' ');

		
		udelay(20);

		bits.e = BIT_SET;
		bits.rs = BIT_SET;
		bits.rw = BIT_CLR;
		set_ctrl_bits();

		
		udelay(40);

		bits.e = BIT_CLR;
		set_ctrl_bits();

		
		udelay(45);
	}
	spin_unlock(&pprt_lock);

	lcd_addr_x = lcd_addr_y = 0;
	lcd_gotoxy();
}

static void lcd_clear_fast_tilcd(void)
{
	int pos;
	lcd_addr_x = lcd_addr_y = 0;
	lcd_gotoxy();

	spin_lock(&pprt_lock);
	for (pos = 0; pos < lcd_height * lcd_hwidth; pos++) {
		
		w_dtr(pprt, ' ');
		udelay(60);
	}

	spin_unlock(&pprt_lock);

	lcd_addr_x = lcd_addr_y = 0;
	lcd_gotoxy();
}

static void lcd_clear_display(void)
{
	lcd_write_cmd(0x01);	
	lcd_addr_x = lcd_addr_y = 0;
	
	long_sleep(15);
}

static void lcd_init_display(void)
{

	lcd_flags = ((lcd_height > 1) ? LCD_FLAG_N : 0)
	    | LCD_FLAG_D | LCD_FLAG_C | LCD_FLAG_B;

	long_sleep(20);		

	lcd_write_cmd(0x30);	
	long_sleep(10);
	lcd_write_cmd(0x30);	
	long_sleep(10);
	lcd_write_cmd(0x30);	
	long_sleep(10);

	lcd_write_cmd(0x30	
		      | ((lcd_flags & LCD_FLAG_F) ? 4 : 0)
		      | ((lcd_flags & LCD_FLAG_N) ? 8 : 0)
	    );
	long_sleep(10);

	lcd_write_cmd(0x08);	
	long_sleep(10);

	lcd_write_cmd(0x08	
		      | ((lcd_flags & LCD_FLAG_D) ? 4 : 0)
		      | ((lcd_flags & LCD_FLAG_C) ? 2 : 0)
		      | ((lcd_flags & LCD_FLAG_B) ? 1 : 0)
	    );

	lcd_backlight((lcd_flags & LCD_FLAG_L) ? 1 : 0);

	long_sleep(10);

	
	lcd_write_cmd(0x06);

	lcd_clear_display();
}


static inline int handle_lcd_special_code(void)
{
	

	int processed = 0;

	char *esc = lcd_escape + 2;
	int oldflags = lcd_flags;

	
	switch (*esc) {
	case 'D':	
		lcd_flags |= LCD_FLAG_D;
		processed = 1;
		break;
	case 'd':	
		lcd_flags &= ~LCD_FLAG_D;
		processed = 1;
		break;
	case 'C':	
		lcd_flags |= LCD_FLAG_C;
		processed = 1;
		break;
	case 'c':	
		lcd_flags &= ~LCD_FLAG_C;
		processed = 1;
		break;
	case 'B':	
		lcd_flags |= LCD_FLAG_B;
		processed = 1;
		break;
	case 'b':	
		lcd_flags &= ~LCD_FLAG_B;
		processed = 1;
		break;
	case '+':	
		lcd_flags |= LCD_FLAG_L;
		processed = 1;
		break;
	case '-':	
		lcd_flags &= ~LCD_FLAG_L;
		processed = 1;
		break;
	case '*':
		
		if (scan_timer.function != NULL) {
			if (light_tempo == 0 && ((lcd_flags & LCD_FLAG_L) == 0))
				lcd_backlight(1);
			light_tempo = FLASH_LIGHT_TEMPO;
		}
		processed = 1;
		break;
	case 'f':	
		lcd_flags &= ~LCD_FLAG_F;
		processed = 1;
		break;
	case 'F':	
		lcd_flags |= LCD_FLAG_F;
		processed = 1;
		break;
	case 'n':	
		lcd_flags &= ~LCD_FLAG_N;
		processed = 1;
		break;
	case 'N':	
		lcd_flags |= LCD_FLAG_N;
		break;
	case 'l':	
		if (lcd_addr_x > 0) {
			
			if (lcd_addr_x < lcd_bwidth)
				lcd_write_cmd(0x10);
			lcd_addr_x--;
		}
		processed = 1;
		break;
	case 'r':	
		if (lcd_addr_x < lcd_width) {
			
			if (lcd_addr_x <
			    (lcd_bwidth - 1))
				lcd_write_cmd(0x14);
			lcd_addr_x++;
		}
		processed = 1;
		break;
	case 'L':	
		lcd_left_shift++;
		lcd_write_cmd(0x18);
		processed = 1;
		break;
	case 'R':	
		lcd_left_shift--;
		lcd_write_cmd(0x1C);
		processed = 1;
		break;
	case 'k': {	
		int x;
		for (x = lcd_addr_x; x < lcd_bwidth; x++)
			lcd_write_data(' ');

		
		lcd_gotoxy();
		processed = 1;
		break;
	}
	case 'I':	
		lcd_init_display();
		lcd_left_shift = 0;
		processed = 1;
		break;
	case 'G': {

		unsigned char cgbytes[8];
		unsigned char cgaddr;
		int cgoffset;
		int shift;
		char value;
		int addr;

		if (strchr(esc, ';') == NULL)
			break;

		esc++;

		cgaddr = *(esc++) - '0';
		if (cgaddr > 7) {
			processed = 1;
			break;
		}

		cgoffset = 0;
		shift = 0;
		value = 0;
		while (*esc && cgoffset < 8) {
			shift ^= 4;
			if (*esc >= '0' && *esc <= '9')
				value |= (*esc - '0') << shift;
			else if (*esc >= 'A' && *esc <= 'Z')
				value |= (*esc - 'A' + 10) << shift;
			else if (*esc >= 'a' && *esc <= 'z')
				value |= (*esc - 'a' + 10) << shift;
			else {
				esc++;
				continue;
			}

			if (shift == 0) {
				cgbytes[cgoffset++] = value;
				value = 0;
			}

			esc++;
		}

		lcd_write_cmd(0x40 | (cgaddr * 8));
		for (addr = 0; addr < cgoffset; addr++)
			lcd_write_data(cgbytes[addr]);

		
		lcd_gotoxy();
		processed = 1;
		break;
	}
	case 'x':	
	case 'y':	
		if (strchr(esc, ';') == NULL)
			break;

		while (*esc) {
			if (*esc == 'x') {
				esc++;
				if (kstrtoul(esc, 10, &lcd_addr_x) < 0)
					break;
			} else if (*esc == 'y') {
				esc++;
				if (kstrtoul(esc, 10, &lcd_addr_y) < 0)
					break;
			} else
				break;
		}

		lcd_gotoxy();
		processed = 1;
		break;
	}

	
	if (oldflags != lcd_flags) {
		
		if ((oldflags ^ lcd_flags) &
		    (LCD_FLAG_B | LCD_FLAG_C | LCD_FLAG_D))
			
			lcd_write_cmd(0x08
				      | ((lcd_flags & LCD_FLAG_D) ? 4 : 0)
				      | ((lcd_flags & LCD_FLAG_C) ? 2 : 0)
				      | ((lcd_flags & LCD_FLAG_B) ? 1 : 0));
		
		else if ((oldflags ^ lcd_flags) & (LCD_FLAG_F | LCD_FLAG_N))
			lcd_write_cmd(0x30
				      | ((lcd_flags & LCD_FLAG_F) ? 4 : 0)
				      | ((lcd_flags & LCD_FLAG_N) ? 8 : 0));
		
		else if ((oldflags ^ lcd_flags) & (LCD_FLAG_L)) {
			if (lcd_flags & (LCD_FLAG_L))
				lcd_backlight(1);
			else if (light_tempo == 0)
				lcd_backlight(0);
		}
	}

	return processed;
}

static ssize_t lcd_write(struct file *file,
			 const char *buf, size_t count, loff_t *ppos)
{
	const char *tmp = buf;
	char c;

	for (; count-- > 0; (ppos ? (*ppos)++ : 0), ++tmp) {
		if (!in_interrupt() && (((count + 1) & 0x1f) == 0))
			schedule();

		if (ppos == NULL && file == NULL)
			
			c = *tmp;
		else if (get_user(c, tmp))
			return -EFAULT;

		
		if ((c != '\n') && lcd_escape_len >= 0) {
			
			lcd_escape[lcd_escape_len++] = c;
			lcd_escape[lcd_escape_len] = 0;
		} else {
			
			lcd_escape_len = -1;

			switch (c) {
			case LCD_ESCAPE_CHAR:
				
				lcd_escape_len = 0;
				lcd_escape[lcd_escape_len] = 0;
				break;
			case '\b':
				
				if (lcd_addr_x > 0) {
					if (lcd_addr_x < lcd_bwidth)
						
						lcd_write_cmd(0x10);
					lcd_addr_x--;
				}
				
				lcd_write_data(' ');
				
				lcd_write_cmd(0x10);
				break;
			case '\014':
				
				lcd_clear_fast();
				break;
			case '\n':
				for (; lcd_addr_x < lcd_bwidth; lcd_addr_x++)
					lcd_write_data(' ');
				lcd_addr_x = 0;
				lcd_addr_y = (lcd_addr_y + 1) % lcd_height;
				lcd_gotoxy();
				break;
			case '\r':
				
				lcd_addr_x = 0;
				lcd_gotoxy();
				break;
			case '\t':
				
				lcd_print(' ');
				break;
			default:
				
				lcd_print(c);
				break;
			}
		}

		if (lcd_escape_len >= 2) {
			int processed = 0;

			if (!strcmp(lcd_escape, "[2J")) {
				
				lcd_clear_fast();
				processed = 1;
			} else if (!strcmp(lcd_escape, "[H")) {
				
				lcd_addr_x = lcd_addr_y = 0;
				lcd_gotoxy();
				processed = 1;
			}
			
			else if ((lcd_escape_len >= 3) &&
				 (lcd_escape[0] == '[') &&
				 (lcd_escape[1] == 'L')) {
				processed = handle_lcd_special_code();
			}

			
			if (processed || (lcd_escape_len >= LCD_ESCAPE_LEN))
				lcd_escape_len = -1;
		} 
	}

	return tmp - buf;
}

static int lcd_open(struct inode *inode, struct file *file)
{
	if (lcd_open_cnt)
		return -EBUSY;	

	if (file->f_mode & FMODE_READ)	
		return -EPERM;

	if (lcd_must_clear) {
		lcd_clear_display();
		lcd_must_clear = 0;
	}
	lcd_open_cnt++;
	return nonseekable_open(inode, file);
}

static int lcd_release(struct inode *inode, struct file *file)
{
	lcd_open_cnt--;
	return 0;
}

static const struct file_operations lcd_fops = {
	.write   = lcd_write,
	.open    = lcd_open,
	.release = lcd_release,
	.llseek  = no_llseek,
};

static struct miscdevice lcd_dev = {
	LCD_MINOR,
	"lcd",
	&lcd_fops
};

void panel_lcd_print(char *s)
{
	if (lcd_enabled && lcd_initialized)
		lcd_write(NULL, s, strlen(s), NULL);
}

void lcd_init(void)
{
	switch (lcd_type) {
	case LCD_TYPE_OLD:
		
		if (lcd_proto < 0)
			lcd_proto = LCD_PROTO_PARALLEL;
		if (lcd_charset < 0)
			lcd_charset = LCD_CHARSET_NORMAL;
		if (lcd_e_pin == PIN_NOT_SET)
			lcd_e_pin = PIN_STROBE;
		if (lcd_rs_pin == PIN_NOT_SET)
			lcd_rs_pin = PIN_AUTOLF;

		if (lcd_width < 0)
			lcd_width = 40;
		if (lcd_bwidth < 0)
			lcd_bwidth = 40;
		if (lcd_hwidth < 0)
			lcd_hwidth = 64;
		if (lcd_height < 0)
			lcd_height = 2;
		break;
	case LCD_TYPE_KS0074:
		
		if (lcd_proto < 0)
			lcd_proto = LCD_PROTO_SERIAL;
		if (lcd_charset < 0)
			lcd_charset = LCD_CHARSET_KS0074;
		if (lcd_bl_pin == PIN_NOT_SET)
			lcd_bl_pin = PIN_AUTOLF;
		if (lcd_cl_pin == PIN_NOT_SET)
			lcd_cl_pin = PIN_STROBE;
		if (lcd_da_pin == PIN_NOT_SET)
			lcd_da_pin = PIN_D0;

		if (lcd_width < 0)
			lcd_width = 16;
		if (lcd_bwidth < 0)
			lcd_bwidth = 40;
		if (lcd_hwidth < 0)
			lcd_hwidth = 16;
		if (lcd_height < 0)
			lcd_height = 2;
		break;
	case LCD_TYPE_NEXCOM:
		
		if (lcd_proto < 0)
			lcd_proto = LCD_PROTO_PARALLEL;
		if (lcd_charset < 0)
			lcd_charset = LCD_CHARSET_NORMAL;
		if (lcd_e_pin == PIN_NOT_SET)
			lcd_e_pin = PIN_AUTOLF;
		if (lcd_rs_pin == PIN_NOT_SET)
			lcd_rs_pin = PIN_SELECP;
		if (lcd_rw_pin == PIN_NOT_SET)
			lcd_rw_pin = PIN_INITP;

		if (lcd_width < 0)
			lcd_width = 16;
		if (lcd_bwidth < 0)
			lcd_bwidth = 40;
		if (lcd_hwidth < 0)
			lcd_hwidth = 64;
		if (lcd_height < 0)
			lcd_height = 2;
		break;
	case LCD_TYPE_CUSTOM:
		
		if (lcd_proto < 0)
			lcd_proto = DEFAULT_LCD_PROTO;
		if (lcd_charset < 0)
			lcd_charset = DEFAULT_LCD_CHARSET;
		
		break;
	case LCD_TYPE_HANTRONIX:
		
	default:
		if (lcd_proto < 0)
			lcd_proto = LCD_PROTO_PARALLEL;
		if (lcd_charset < 0)
			lcd_charset = LCD_CHARSET_NORMAL;
		if (lcd_e_pin == PIN_NOT_SET)
			lcd_e_pin = PIN_STROBE;
		if (lcd_rs_pin == PIN_NOT_SET)
			lcd_rs_pin = PIN_SELECP;

		if (lcd_width < 0)
			lcd_width = 16;
		if (lcd_bwidth < 0)
			lcd_bwidth = 40;
		if (lcd_hwidth < 0)
			lcd_hwidth = 64;
		if (lcd_height < 0)
			lcd_height = 2;
		break;
	}

	
	if (lcd_width <= 0)
		lcd_width = DEFAULT_LCD_WIDTH;
	if (lcd_bwidth <= 0)
		lcd_bwidth = DEFAULT_LCD_BWIDTH;
	if (lcd_hwidth <= 0)
		lcd_hwidth = DEFAULT_LCD_HWIDTH;
	if (lcd_height <= 0)
		lcd_height = DEFAULT_LCD_HEIGHT;

	if (lcd_proto == LCD_PROTO_SERIAL) {	
		lcd_write_cmd = lcd_write_cmd_s;
		lcd_write_data = lcd_write_data_s;
		lcd_clear_fast = lcd_clear_fast_s;

		if (lcd_cl_pin == PIN_NOT_SET)
			lcd_cl_pin = DEFAULT_LCD_PIN_SCL;
		if (lcd_da_pin == PIN_NOT_SET)
			lcd_da_pin = DEFAULT_LCD_PIN_SDA;

	} else if (lcd_proto == LCD_PROTO_PARALLEL) {	
		lcd_write_cmd = lcd_write_cmd_p8;
		lcd_write_data = lcd_write_data_p8;
		lcd_clear_fast = lcd_clear_fast_p8;

		if (lcd_e_pin == PIN_NOT_SET)
			lcd_e_pin = DEFAULT_LCD_PIN_E;
		if (lcd_rs_pin == PIN_NOT_SET)
			lcd_rs_pin = DEFAULT_LCD_PIN_RS;
		if (lcd_rw_pin == PIN_NOT_SET)
			lcd_rw_pin = DEFAULT_LCD_PIN_RW;
	} else {
		lcd_write_cmd = lcd_write_cmd_tilcd;
		lcd_write_data = lcd_write_data_tilcd;
		lcd_clear_fast = lcd_clear_fast_tilcd;
	}

	if (lcd_bl_pin == PIN_NOT_SET)
		lcd_bl_pin = DEFAULT_LCD_PIN_BL;

	if (lcd_e_pin == PIN_NOT_SET)
		lcd_e_pin = PIN_NONE;
	if (lcd_rs_pin == PIN_NOT_SET)
		lcd_rs_pin = PIN_NONE;
	if (lcd_rw_pin == PIN_NOT_SET)
		lcd_rw_pin = PIN_NONE;
	if (lcd_bl_pin == PIN_NOT_SET)
		lcd_bl_pin = PIN_NONE;
	if (lcd_cl_pin == PIN_NOT_SET)
		lcd_cl_pin = PIN_NONE;
	if (lcd_da_pin == PIN_NOT_SET)
		lcd_da_pin = PIN_NONE;

	if (lcd_charset < 0)
		lcd_charset = DEFAULT_LCD_CHARSET;

	if (lcd_charset == LCD_CHARSET_KS0074)
		lcd_char_conv = lcd_char_conv_ks0074;
	else
		lcd_char_conv = NULL;

	if (lcd_bl_pin != PIN_NONE)
		init_scan_timer();

	pin_to_bits(lcd_e_pin, lcd_bits[LCD_PORT_D][LCD_BIT_E],
		    lcd_bits[LCD_PORT_C][LCD_BIT_E]);
	pin_to_bits(lcd_rs_pin, lcd_bits[LCD_PORT_D][LCD_BIT_RS],
		    lcd_bits[LCD_PORT_C][LCD_BIT_RS]);
	pin_to_bits(lcd_rw_pin, lcd_bits[LCD_PORT_D][LCD_BIT_RW],
		    lcd_bits[LCD_PORT_C][LCD_BIT_RW]);
	pin_to_bits(lcd_bl_pin, lcd_bits[LCD_PORT_D][LCD_BIT_BL],
		    lcd_bits[LCD_PORT_C][LCD_BIT_BL]);
	pin_to_bits(lcd_cl_pin, lcd_bits[LCD_PORT_D][LCD_BIT_CL],
		    lcd_bits[LCD_PORT_C][LCD_BIT_CL]);
	pin_to_bits(lcd_da_pin, lcd_bits[LCD_PORT_D][LCD_BIT_DA],
		    lcd_bits[LCD_PORT_C][LCD_BIT_DA]);

	lcd_initialized = 1;
	lcd_init_display();

	
#ifdef CONFIG_PANEL_CHANGE_MESSAGE
#ifdef CONFIG_PANEL_BOOT_MESSAGE
	panel_lcd_print("\x1b[Lc\x1b[Lb\x1b[L*" CONFIG_PANEL_BOOT_MESSAGE);
#endif
#else
	panel_lcd_print("\x1b[Lc\x1b[Lb\x1b[L*Linux-" UTS_RELEASE "\nPanel-"
			PANEL_VERSION);
#endif
	lcd_addr_x = lcd_addr_y = 0;
	
	lcd_must_clear = 1;
	lcd_gotoxy();
}


static ssize_t keypad_read(struct file *file,
			   char *buf, size_t count, loff_t *ppos)
{

	unsigned i = *ppos;
	char *tmp = buf;

	if (keypad_buflen == 0) {
		if (file->f_flags & O_NONBLOCK)
			return -EAGAIN;

		interruptible_sleep_on(&keypad_read_wait);
		if (signal_pending(current))
			return -EINTR;
	}

	for (; count-- > 0 && (keypad_buflen > 0);
	     ++i, ++tmp, --keypad_buflen) {
		put_user(keypad_buffer[keypad_start], tmp);
		keypad_start = (keypad_start + 1) % KEYPAD_BUFFER;
	}
	*ppos = i;

	return tmp - buf;
}

static int keypad_open(struct inode *inode, struct file *file)
{

	if (keypad_open_cnt)
		return -EBUSY;	

	if (file->f_mode & FMODE_WRITE)	
		return -EPERM;

	keypad_buflen = 0;	
	keypad_open_cnt++;
	return 0;
}

static int keypad_release(struct inode *inode, struct file *file)
{
	keypad_open_cnt--;
	return 0;
}

static const struct file_operations keypad_fops = {
	.read    = keypad_read,		
	.open    = keypad_open,		
	.release = keypad_release,	
	.llseek  = default_llseek,
};

static struct miscdevice keypad_dev = {
	KEYPAD_MINOR,
	"keypad",
	&keypad_fops
};

static void keypad_send_key(char *string, int max_len)
{
	if (init_in_progress)
		return;

	
	if (keypad_open_cnt > 0) {
		while (max_len-- && keypad_buflen < KEYPAD_BUFFER && *string) {
			keypad_buffer[(keypad_start + keypad_buflen++) %
				      KEYPAD_BUFFER] = *string++;
		}
		wake_up_interruptible(&keypad_read_wait);
	}
}

static void phys_scan_contacts(void)
{
	int bit, bitval;
	char oldval;
	char bitmask;
	char gndmask;

	phys_prev = phys_curr;
	phys_read_prev = phys_read;
	phys_read = 0;		

	
	oldval = r_dtr(pprt) | scan_mask_o;
	
	w_dtr(pprt, oldval & ~scan_mask_o);

	
	bitmask = PNL_PINPUT(r_str(pprt)) & scan_mask_i;
	
	w_dtr(pprt, oldval);


	
	gndmask = PNL_PINPUT(r_str(pprt)) & scan_mask_i;

	
	phys_read |= (pmask_t) gndmask << 40;

	if (bitmask != gndmask) {
		for (bit = 0; bit < 8; bit++) {
			bitval = 1 << bit;

			if (!(scan_mask_o & bitval))	
				continue;

			w_dtr(pprt, oldval & ~bitval);	
			bitmask = PNL_PINPUT(r_str(pprt)) & ~gndmask;
			phys_read |= (pmask_t) bitmask << (5 * bit);
		}
		w_dtr(pprt, oldval);	
	}
	phys_curr = (phys_prev & (phys_read ^ phys_read_prev)) |
		    (phys_read & ~(phys_read ^ phys_read_prev));
}

static inline int input_state_high(struct logical_input *input)
{
#if 0

	if (((phys_prev & input->mask) == input->value)
	    && ((phys_curr & input->mask) > input->value)) {
		input->state = INPUT_ST_LOW; 
		return 1;
	}
#endif

	if ((phys_curr & input->mask) == input->value) {
		if ((input->type == INPUT_TYPE_STD) &&
		    (input->high_timer == 0)) {
			input->high_timer++;
			if (input->u.std.press_fct != NULL)
				input->u.std.press_fct(input->u.std.press_data);
		} else if (input->type == INPUT_TYPE_KBD) {
			
			keypressed = 1;

			if (input->high_timer == 0) {
				char *press_str = input->u.kbd.press_str;
				if (press_str[0])
					keypad_send_key(press_str,
							sizeof(press_str));
			}

			if (input->u.kbd.repeat_str[0]) {
				char *repeat_str = input->u.kbd.repeat_str;
				if (input->high_timer >= KEYPAD_REP_START) {
					input->high_timer -= KEYPAD_REP_DELAY;
					keypad_send_key(repeat_str,
							sizeof(repeat_str));
				}
				
				inputs_stable = 0;
			}

			if (input->high_timer < 255)
				input->high_timer++;
		}
		return 1;
	} else {
		
		input->state = INPUT_ST_FALLING;
		input->fall_timer = 0;
	}
	return 0;
}

static inline void input_state_falling(struct logical_input *input)
{
#if 0
	
	if (((phys_prev & input->mask) == input->value)
	    && ((phys_curr & input->mask) > input->value)) {
		input->state = INPUT_ST_LOW;	
		return;
	}
#endif

	if ((phys_curr & input->mask) == input->value) {
		if (input->type == INPUT_TYPE_KBD) {
			
			keypressed = 1;

			if (input->u.kbd.repeat_str[0]) {
				char *repeat_str = input->u.kbd.repeat_str;
				if (input->high_timer >= KEYPAD_REP_START)
					input->high_timer -= KEYPAD_REP_DELAY;
					keypad_send_key(repeat_str,
							sizeof(repeat_str));
				
				inputs_stable = 0;
			}

			if (input->high_timer < 255)
				input->high_timer++;
		}
		input->state = INPUT_ST_HIGH;
	} else if (input->fall_timer >= input->fall_time) {
		
		if (input->type == INPUT_TYPE_STD) {
			void (*release_fct)(int) = input->u.std.release_fct;
			if (release_fct != NULL)
				release_fct(input->u.std.release_data);
		} else if (input->type == INPUT_TYPE_KBD) {
			char *release_str = input->u.kbd.release_str;
			if (release_str[0])
				keypad_send_key(release_str,
						sizeof(release_str));
		}

		input->state = INPUT_ST_LOW;
	} else {
		input->fall_timer++;
		inputs_stable = 0;
	}
}

static void panel_process_inputs(void)
{
	struct list_head *item;
	struct logical_input *input;

#if 0
	printk(KERN_DEBUG
	       "entering panel_process_inputs with pp=%016Lx & pc=%016Lx\n",
	       phys_prev, phys_curr);
#endif

	keypressed = 0;
	inputs_stable = 1;
	list_for_each(item, &logical_inputs) {
		input = list_entry(item, struct logical_input, list);

		switch (input->state) {
		case INPUT_ST_LOW:
			if ((phys_curr & input->mask) != input->value)
				break;
			if ((phys_prev & input->mask) == input->value)
				break;
			input->rise_timer = 0;
			input->state = INPUT_ST_RISING;
			
		case INPUT_ST_RISING:
			if ((phys_curr & input->mask) != input->value) {
				input->state = INPUT_ST_LOW;
				break;
			}
			if (input->rise_timer < input->rise_time) {
				inputs_stable = 0;
				input->rise_timer++;
				break;
			}
			input->high_timer = 0;
			input->state = INPUT_ST_HIGH;
			
		case INPUT_ST_HIGH:
			if (input_state_high(input))
				break;
			
		case INPUT_ST_FALLING:
			input_state_falling(input);
		}
	}
}

static void panel_scan_timer(void)
{
	if (keypad_enabled && keypad_initialized) {
		if (spin_trylock(&pprt_lock)) {
			phys_scan_contacts();

			
			spin_unlock(&pprt_lock);
		}

		if (!inputs_stable || phys_curr != phys_prev)
			panel_process_inputs();
	}

	if (lcd_enabled && lcd_initialized) {
		if (keypressed) {
			if (light_tempo == 0 && ((lcd_flags & LCD_FLAG_L) == 0))
				lcd_backlight(1);
			light_tempo = FLASH_LIGHT_TEMPO;
		} else if (light_tempo > 0) {
			light_tempo--;
			if (light_tempo == 0 && ((lcd_flags & LCD_FLAG_L) == 0))
				lcd_backlight(0);
		}
	}

	mod_timer(&scan_timer, jiffies + INPUT_POLL_TIME);
}

static void init_scan_timer(void)
{
	if (scan_timer.function != NULL)
		return;		

	init_timer(&scan_timer);
	scan_timer.expires = jiffies + INPUT_POLL_TIME;
	scan_timer.data = 0;
	scan_timer.function = (void *)&panel_scan_timer;
	add_timer(&scan_timer);
}

/* converts a name of the form "({BbAaPpSsEe}{01234567-})*" to a series of bits.
 * if <omask> or <imask> are non-null, they will be or'ed with the bits
 * corresponding to out and in bits respectively.
 * returns 1 if ok, 0 if error (in which case, nothing is written).
 */
static int input_name2mask(char *name, pmask_t *mask, pmask_t *value,
			   char *imask, char *omask)
{
	static char sigtab[10] = "EeSsPpAaBb";
	char im, om;
	pmask_t m, v;

	om = im = m = v = 0ULL;
	while (*name) {
		int in, out, bit, neg;
		for (in = 0; (in < sizeof(sigtab)) &&
			     (sigtab[in] != *name); in++)
			;
		if (in >= sizeof(sigtab))
			return 0;	
		neg = (in & 1);	
		in >>= 1;
		im |= (1 << in);

		name++;
		if (isdigit(*name)) {
			out = *name - '0';
			om |= (1 << out);
		} else if (*name == '-')
			out = 8;
		else
			return 0;	

		bit = (out * 5) + in;

		m |= 1ULL << bit;
		if (!neg)
			v |= 1ULL << bit;
		name++;
	}
	*mask = m;
	*value = v;
	if (imask)
		*imask |= im;
	if (omask)
		*omask |= om;
	return 1;
}

static struct logical_input *panel_bind_key(char *name, char *press,
					    char *repeat, char *release)
{
	struct logical_input *key;

	key = kzalloc(sizeof(struct logical_input), GFP_KERNEL);
	if (!key) {
		printk(KERN_ERR "panel: not enough memory\n");
		return NULL;
	}
	if (!input_name2mask(name, &key->mask, &key->value, &scan_mask_i,
			     &scan_mask_o)) {
		kfree(key);
		return NULL;
	}

	key->type = INPUT_TYPE_KBD;
	key->state = INPUT_ST_LOW;
	key->rise_time = 1;
	key->fall_time = 1;

#if 0
	printk(KERN_DEBUG "bind: <%s> : m=%016Lx v=%016Lx\n", name, key->mask,
	       key->value);
#endif
	strncpy(key->u.kbd.press_str, press, sizeof(key->u.kbd.press_str));
	strncpy(key->u.kbd.repeat_str, repeat, sizeof(key->u.kbd.repeat_str));
	strncpy(key->u.kbd.release_str, release,
		sizeof(key->u.kbd.release_str));
	list_add(&key->list, &logical_inputs);
	return key;
}

#if 0
static struct logical_input *panel_bind_callback(char *name,
						 void (*press_fct) (int),
						 int press_data,
						 void (*release_fct) (int),
						 int release_data)
{
	struct logical_input *callback;

	callback = kmalloc(sizeof(struct logical_input), GFP_KERNEL);
	if (!callback) {
		printk(KERN_ERR "panel: not enough memory\n");
		return NULL;
	}
	memset(callback, 0, sizeof(struct logical_input));
	if (!input_name2mask(name, &callback->mask, &callback->value,
			     &scan_mask_i, &scan_mask_o))
		return NULL;

	callback->type = INPUT_TYPE_STD;
	callback->state = INPUT_ST_LOW;
	callback->rise_time = 1;
	callback->fall_time = 1;
	callback->u.std.press_fct = press_fct;
	callback->u.std.press_data = press_data;
	callback->u.std.release_fct = release_fct;
	callback->u.std.release_data = release_data;
	list_add(&callback->list, &logical_inputs);
	return callback;
}
#endif

static void keypad_init(void)
{
	int keynum;
	init_waitqueue_head(&keypad_read_wait);
	keypad_buflen = 0;	

	

	for (keynum = 0; keypad_profile[keynum][0][0]; keynum++) {
		panel_bind_key(keypad_profile[keynum][0],
			       keypad_profile[keynum][1],
			       keypad_profile[keynum][2],
			       keypad_profile[keynum][3]);
	}

	init_scan_timer();
	keypad_initialized = 1;
}


static int panel_notify_sys(struct notifier_block *this, unsigned long code,
			    void *unused)
{
	if (lcd_enabled && lcd_initialized) {
		switch (code) {
		case SYS_DOWN:
			panel_lcd_print
			    ("\x0cReloading\nSystem...\x1b[Lc\x1b[Lb\x1b[L+");
			break;
		case SYS_HALT:
			panel_lcd_print
			    ("\x0cSystem Halted.\x1b[Lc\x1b[Lb\x1b[L+");
			break;
		case SYS_POWER_OFF:
			panel_lcd_print("\x0cPower off.\x1b[Lc\x1b[Lb\x1b[L+");
			break;
		default:
			break;
		}
	}
	return NOTIFY_DONE;
}

static struct notifier_block panel_notifier = {
	panel_notify_sys,
	NULL,
	0
};

static void panel_attach(struct parport *port)
{
	if (port->number != parport)
		return;

	if (pprt) {
		printk(KERN_ERR
		       "panel_attach(): port->number=%d parport=%d, "
		       "already registered !\n",
		       port->number, parport);
		return;
	}

	pprt = parport_register_device(port, "panel", NULL, NULL,  
				       NULL,
				       
				       0, (void *)&pprt);
	if (pprt == NULL) {
		pr_err("panel_attach(): port->number=%d parport=%d, "
		       "parport_register_device() failed\n",
		       port->number, parport);
		return;
	}

	if (parport_claim(pprt)) {
		printk(KERN_ERR
		       "Panel: could not claim access to parport%d. "
		       "Aborting.\n", parport);
		goto err_unreg_device;
	}

	if (lcd_enabled) {
		lcd_init();
		if (misc_register(&lcd_dev))
			goto err_unreg_device;
	}

	if (keypad_enabled) {
		keypad_init();
		if (misc_register(&keypad_dev))
			goto err_lcd_unreg;
	}
	return;

err_lcd_unreg:
	if (lcd_enabled)
		misc_deregister(&lcd_dev);
err_unreg_device:
	parport_unregister_device(pprt);
	pprt = NULL;
}

static void panel_detach(struct parport *port)
{
	if (port->number != parport)
		return;

	if (!pprt) {
		printk(KERN_ERR
		       "panel_detach(): port->number=%d parport=%d, "
		       "nothing to unregister.\n",
		       port->number, parport);
		return;
	}

	if (keypad_enabled && keypad_initialized) {
		misc_deregister(&keypad_dev);
		keypad_initialized = 0;
	}

	if (lcd_enabled && lcd_initialized) {
		misc_deregister(&lcd_dev);
		lcd_initialized = 0;
	}

	parport_release(pprt);
	parport_unregister_device(pprt);
	pprt = NULL;
}

static struct parport_driver panel_driver = {
	.name = "panel",
	.attach = panel_attach,
	.detach = panel_detach,
};

int panel_init(void)
{
	
	if (keypad_type < 0)
		keypad_type = keypad_enabled;

	if (lcd_type < 0)
		lcd_type = lcd_enabled;

	if (parport < 0)
		parport = DEFAULT_PARPORT;

	
	switch (profile) {
	case PANEL_PROFILE_CUSTOM:
		
		if (keypad_type < 0)
			keypad_type = DEFAULT_KEYPAD;
		if (lcd_type < 0)
			lcd_type = DEFAULT_LCD;
		break;
	case PANEL_PROFILE_OLD:
		
		if (keypad_type < 0)
			keypad_type = KEYPAD_TYPE_OLD;
		if (lcd_type < 0)
			lcd_type = LCD_TYPE_OLD;
		if (lcd_width < 0)
			lcd_width = 16;
		if (lcd_hwidth < 0)
			lcd_hwidth = 16;
		break;
	case PANEL_PROFILE_NEW:
		
		if (keypad_type < 0)
			keypad_type = KEYPAD_TYPE_NEW;
		if (lcd_type < 0)
			lcd_type = LCD_TYPE_KS0074;
		break;
	case PANEL_PROFILE_HANTRONIX:
		
		if (keypad_type < 0)
			keypad_type = KEYPAD_TYPE_NONE;
		if (lcd_type < 0)
			lcd_type = LCD_TYPE_HANTRONIX;
		break;
	case PANEL_PROFILE_NEXCOM:
		
		if (keypad_type < 0)
			keypad_type = KEYPAD_TYPE_NEXCOM;
		if (lcd_type < 0)
			lcd_type = LCD_TYPE_NEXCOM;
		break;
	case PANEL_PROFILE_LARGE:
		
		if (keypad_type < 0)
			keypad_type = KEYPAD_TYPE_OLD;
		if (lcd_type < 0)
			lcd_type = LCD_TYPE_OLD;
		break;
	}

	lcd_enabled = (lcd_type > 0);
	keypad_enabled = (keypad_type > 0);

	switch (keypad_type) {
	case KEYPAD_TYPE_OLD:
		keypad_profile = old_keypad_profile;
		break;
	case KEYPAD_TYPE_NEW:
		keypad_profile = new_keypad_profile;
		break;
	case KEYPAD_TYPE_NEXCOM:
		keypad_profile = nexcom_keypad_profile;
		break;
	default:
		keypad_profile = NULL;
		break;
	}

	
	init_in_progress = 1;

	if (parport_register_driver(&panel_driver)) {
		printk(KERN_ERR
		       "Panel: could not register with parport. Aborting.\n");
		return -EIO;
	}

	if (!lcd_enabled && !keypad_enabled) {
		
		if (pprt) {
			parport_release(pprt);
			parport_unregister_device(pprt);
			pprt = NULL;
		}
		parport_unregister_driver(&panel_driver);
		printk(KERN_ERR "Panel driver version " PANEL_VERSION
		       " disabled.\n");
		return -ENODEV;
	}

	register_reboot_notifier(&panel_notifier);

	if (pprt)
		printk(KERN_INFO "Panel driver version " PANEL_VERSION
		       " registered on parport%d (io=0x%lx).\n", parport,
		       pprt->port->base);
	else
		printk(KERN_INFO "Panel driver version " PANEL_VERSION
		       " not yet registered\n");
	init_in_progress = 0;
	return 0;
}

static int __init panel_init_module(void)
{
	return panel_init();
}

static void __exit panel_cleanup_module(void)
{
	unregister_reboot_notifier(&panel_notifier);

	if (scan_timer.function != NULL)
		del_timer(&scan_timer);

	if (pprt != NULL) {
		if (keypad_enabled) {
			misc_deregister(&keypad_dev);
			keypad_initialized = 0;
		}

		if (lcd_enabled) {
			panel_lcd_print("\x0cLCD driver " PANEL_VERSION
					"\nunloaded.\x1b[Lc\x1b[Lb\x1b[L-");
			misc_deregister(&lcd_dev);
			lcd_initialized = 0;
		}

		
		parport_release(pprt);
		parport_unregister_device(pprt);
		pprt = NULL;
	}
	parport_unregister_driver(&panel_driver);
}

module_init(panel_init_module);
module_exit(panel_cleanup_module);
MODULE_AUTHOR("Willy Tarreau");
MODULE_LICENSE("GPL");

