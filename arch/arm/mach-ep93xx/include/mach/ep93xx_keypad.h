
#ifndef __ASM_ARCH_EP93XX_KEYPAD_H
#define __ASM_ARCH_EP93XX_KEYPAD_H

struct matrix_keymap_data;

#define EP93XX_KEYPAD_DISABLE_3_KEY	(1<<0)	
#define EP93XX_KEYPAD_DIAG_MODE		(1<<1)	
#define EP93XX_KEYPAD_BACK_DRIVE	(1<<2)	
#define EP93XX_KEYPAD_TEST_MODE		(1<<3)	
#define EP93XX_KEYPAD_KDIV		(1<<4)	
#define EP93XX_KEYPAD_AUTOREPEAT	(1<<5)	

struct ep93xx_keypad_platform_data {
	struct matrix_keymap_data *keymap_data;
	unsigned int	debounce;
	unsigned int	prescale;
	unsigned int	flags;
};

#define EP93XX_MATRIX_ROWS		(8)
#define EP93XX_MATRIX_COLS		(8)

#endif	
