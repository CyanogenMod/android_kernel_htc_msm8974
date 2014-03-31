/*
 * linux/arch/m68k/atari/debug.c
 *
 * Atari debugging and serial console stuff
 *
 * Assembled of parts of former atari/config.c 97-12-18 by Roman Hodek
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive
 * for more details.
 */

#include <linux/types.h>
#include <linux/tty.h>
#include <linux/console.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/module.h>

#include <asm/atarihw.h>
#include <asm/atariints.h>

int atari_SCC_reset_done;
EXPORT_SYMBOL(atari_SCC_reset_done);

static struct console atari_console_driver = {
	.name	= "debug",
	.flags	= CON_PRINTBUFFER,
	.index	= -1,
};


static inline void ata_mfp_out(char c)
{
	while (!(st_mfp.trn_stat & 0x80))	
		barrier();
	st_mfp.usart_dta = c;
}

static void atari_mfp_console_write(struct console *co, const char *str,
				    unsigned int count)
{
	while (count--) {
		if (*str == '\n')
			ata_mfp_out('\r');
		ata_mfp_out(*str++);
	}
}

static inline void ata_scc_out(char c)
{
	do {
		MFPDELAY();
	} while (!(atari_scc.cha_b_ctrl & 0x04)); 
	MFPDELAY();
	atari_scc.cha_b_data = c;
}

static void atari_scc_console_write(struct console *co, const char *str,
				    unsigned int count)
{
	while (count--) {
		if (*str == '\n')
			ata_scc_out('\r');
		ata_scc_out(*str++);
	}
}

static inline void ata_midi_out(char c)
{
	while (!(acia.mid_ctrl & ACIA_TDRE))	
		barrier();
	acia.mid_data = c;
}

static void atari_midi_console_write(struct console *co, const char *str,
				     unsigned int count)
{
	while (count--) {
		if (*str == '\n')
			ata_midi_out('\r');
		ata_midi_out(*str++);
	}
}

static int ata_par_out(char c)
{
	unsigned char tmp;
	
	unsigned long i = loops_per_jiffy > 1 ? loops_per_jiffy : 10000000/HZ;

	while ((st_mfp.par_dt_reg & 1) && --i) 
		;
	if (!i)
		return 0;

	sound_ym.rd_data_reg_sel = 15;	
	sound_ym.wd_data = c;		
	sound_ym.rd_data_reg_sel = 14;	
	tmp = sound_ym.rd_data_reg_sel;
	sound_ym.wd_data = tmp & ~0x20;	
	MFPDELAY();			
	sound_ym.wd_data = tmp | 0x20;	
	return 1;
}

static void atari_par_console_write(struct console *co, const char *str,
				    unsigned int count)
{
	static int printer_present = 1;

	if (!printer_present)
		return;

	while (count--) {
		if (*str == '\n') {
			if (!ata_par_out('\r')) {
				printer_present = 0;
				return;
			}
		}
		if (!ata_par_out(*str++)) {
			printer_present = 0;
			return;
		}
	}
}

#if 0
int atari_mfp_console_wait_key(struct console *co)
{
	while (!(st_mfp.rcv_stat & 0x80))	
		barrier();
	return st_mfp.usart_dta;
}

int atari_scc_console_wait_key(struct console *co)
{
	do {
		MFPDELAY();
	} while (!(atari_scc.cha_b_ctrl & 0x01)); 
	MFPDELAY();
	return atari_scc.cha_b_data;
}

int atari_midi_console_wait_key(struct console *co)
{
	while (!(acia.mid_ctrl & ACIA_RDRF)) 
		barrier();
	return acia.mid_data;
}
#endif

static void __init atari_init_mfp_port(int cflag)
{
	static int baud_table[9] = { 16, 11, 8, 4, 2, 1, 175, 143, 128 };
	int baud = cflag & CBAUD;
	int parity = (cflag & PARENB) ? ((cflag & PARODD) ? 0x04 : 0x06) : 0;
	int csize = ((cflag & CSIZE) == CS7) ? 0x20 : 0x00;

	if (cflag & CBAUDEX)
		baud += B38400;
	if (baud < B1200 || baud > B38400+2)
		baud = B9600;		
	baud -= B1200;			

	st_mfp.trn_stat &= ~0x01;	
	st_mfp.usart_ctr = parity | csize | 0x88; 
	st_mfp.tim_ct_cd &= 0x70;	
	st_mfp.tim_dt_d = baud_table[baud];
	st_mfp.tim_ct_cd |= 0x01;	
	st_mfp.trn_stat |= 0x01;	
}

#define SCC_WRITE(reg, val)				\
	do {						\
		atari_scc.cha_b_ctrl = (reg);		\
		MFPDELAY();				\
		atari_scc.cha_b_ctrl = (val);		\
		MFPDELAY();				\
	} while (0)

#define LONG_DELAY()					\
	do {						\
		int i;					\
		for (i = 100; i > 0; --i)		\
			MFPDELAY();			\
	} while (0)

static void __init atari_init_scc_port(int cflag)
{
	static int clksrc_table[9] =
		
		{ 0x50, 0x50, 0x50, 0x50, 0x50, 0x50, 0x50, 0x00, 0x00 };
	static int brgsrc_table[9] =
		
		{ 2, 2, 2, 2, 2, 2, 0, 2, 2 };
	static int clkmode_table[9] =
		
		{ 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0xc0, 0x80 };
	static int div_table[9] =
		
		{ 208, 138, 103, 50, 24, 11, 1, 0, 0 };

	int baud = cflag & CBAUD;
	int clksrc, clkmode, div, reg3, reg5;

	if (cflag & CBAUDEX)
		baud += B38400;
	if (baud < B1200 || baud > B38400+2)
		baud = B9600;		
	baud -= B1200;			

	clksrc  = clksrc_table[baud];
	clkmode = clkmode_table[baud];
	div     = div_table[baud];
	if (ATARIHW_PRESENT(TT_MFP) && baud >= 6) {
		
		clksrc = 0x28;		
		clkmode = baud == 6 ? 0xc0 :
			  baud == 7 ? 0x80 : 
				      0x40;  
		div = 0;
	}

	reg3 = (cflag & CSIZE) == CS8 ? 0xc0 : 0x40;
	reg5 = (cflag & CSIZE) == CS8 ? 0x60 : 0x20 | 0x82 ;

	(void)atari_scc.cha_b_ctrl;	
	SCC_WRITE(9, 0xc0);		
	LONG_DELAY();			
	SCC_WRITE(4, (cflag & PARENB) ? ((cflag & PARODD) ? 0x01 : 0x03)
				      : 0 | 0x04  | clkmode);
	SCC_WRITE(3, reg3);
	SCC_WRITE(5, reg5);
	SCC_WRITE(9, 0);		
	LONG_DELAY();			
	SCC_WRITE(10, 0);		
	SCC_WRITE(11, clksrc);		
	SCC_WRITE(12, div);		
	SCC_WRITE(13, 0);		
	SCC_WRITE(14, brgsrc_table[baud]);
	SCC_WRITE(14, brgsrc_table[baud] | (div ? 1 : 0));
	SCC_WRITE(3, reg3 | 1);
	SCC_WRITE(5, reg5 | 8);

	atari_SCC_reset_done = 1;
}

static void __init atari_init_midi_port(int cflag)
{
	int baud = cflag & CBAUD;
	int csize = ((cflag & CSIZE) == CS8) ? 0x10 : 0x00;
	
	int parity = (cflag & PARENB) ? ((cflag & PARODD) ? 0x0c : 0x08) : 0x04;
	int div;

	if (cflag & CBAUDEX)
		baud += B38400;
	if (baud == B4800)
		div = ACIA_DIV64;	
	else if (baud == B38400+2 )
		div = ACIA_DIV1;	
	else
		div = ACIA_DIV16;	

	
	acia.mid_ctrl = div | csize | parity |
		    ((atari_switches & ATARI_SWITCH_MIDI) ?
		     ACIA_RHTID : ACIA_RLTID);
}

static int __init atari_debug_setup(char *arg)
{
	if (!MACH_IS_ATARI)
		return 0;

	if (!strcmp(arg, "ser"))
		
		arg = MACH_IS_FALCON ? "ser2" : "ser1";

	if (!strcmp(arg, "ser1")) {
		
		atari_init_mfp_port(B9600|CS8);
		atari_console_driver.write = atari_mfp_console_write;
	} else if (!strcmp(arg, "ser2")) {
		
		atari_init_scc_port(B9600|CS8);
		atari_console_driver.write = atari_scc_console_write;
	} else if (!strcmp(arg, "midi")) {
		
		atari_init_midi_port(B9600|CS8);
		atari_console_driver.write = atari_midi_console_write;
	} else if (!strcmp(arg, "par")) {
		
		atari_turnoff_irq(IRQ_MFP_BUSY); 
		sound_ym.rd_data_reg_sel = 7;	
		sound_ym.wd_data = 0xff;	
		sound_ym.rd_data_reg_sel = 15;	
		sound_ym.wd_data = 0;		
		sound_ym.rd_data_reg_sel = 14;	
		sound_ym.wd_data = sound_ym.rd_data_reg_sel | 0x20; 
		atari_console_driver.write = atari_par_console_write;
	}
	if (atari_console_driver.write)
		register_console(&atari_console_driver);

	return 0;
}

early_param("debug", atari_debug_setup);
