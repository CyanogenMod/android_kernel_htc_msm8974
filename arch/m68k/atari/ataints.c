/*
 * arch/m68k/atari/ataints.c -- Atari Linux interrupt handling code
 *
 * 5/2/94 Roman Hodek:
 *  Added support for TT interrupts; setup for TT SCU (may someone has
 *  twiddled there and we won't get the right interrupts :-()
 *
 *  Major change: The device-independent code in m68k/ints.c didn't know
 *  about non-autovec ints yet. It hardcoded the number of possible ints to
 *  7 (IRQ1...IRQ7). But the Atari has lots of non-autovec ints! I made the
 *  number of possible ints a constant defined in interrupt.h, which is
 *  47 for the Atari. So we can call request_irq() for all Atari interrupts
 *  just the normal way. Additionally, all vectors >= 48 are initialized to
 *  call trap() instead of inthandler(). This must be changed here, too.
 *
 * 1995-07-16 Lars Brinkhoff <f93labr@dd.chalmers.se>:
 *  Corrected a bug in atari_add_isr() which rejected all SCC
 *  interrupt sources if there were no TT MFP!
 *
 * 12/13/95: New interface functions atari_level_triggered_int() and
 *  atari_register_vme_int() as support for level triggered VME interrupts.
 *
 * 02/12/96: (Roman)
 *  Total rewrite of Atari interrupt handling, for new scheme see comments
 *  below.
 *
 * 1996-09-03 lars brinkhoff <f93labr@dd.chalmers.se>:
 *  Added new function atari_unregister_vme_int(), and
 *  modified atari_register_vme_int() as well as IS_VALID_INTNO()
 *  to work with it.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive
 * for more details.
 *
 */

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/kernel_stat.h>
#include <linux/init.h>
#include <linux/seq_file.h>
#include <linux/module.h>

#include <asm/traps.h>

#include <asm/atarihw.h>
#include <asm/atariints.h>
#include <asm/atari_stdma.h>
#include <asm/irq.h>
#include <asm/entry.h>



static int free_vme_vec_bitmap;

asmlinkage void falcon_hblhandler(void);
asm(".text\n"
__ALIGN_STR "\n\t"
"falcon_hblhandler:\n\t"
	"orw	#0x200,%sp@\n\t"	
	"rte");

extern void atari_microwire_cmd(int cmd);

static unsigned int atari_irq_startup(struct irq_data *data)
{
	unsigned int irq = data->irq;

	m68k_irq_startup(data);
	atari_turnon_irq(irq);
	atari_enable_irq(irq);
	return 0;
}

static void atari_irq_shutdown(struct irq_data *data)
{
	unsigned int irq = data->irq;

	atari_disable_irq(irq);
	atari_turnoff_irq(irq);
	m68k_irq_shutdown(data);

	if (irq == IRQ_AUTO_4)
	    vectors[VEC_INT4] = falcon_hblhandler;
}

static void atari_irq_enable(struct irq_data *data)
{
	atari_enable_irq(data->irq);
}

static void atari_irq_disable(struct irq_data *data)
{
	atari_disable_irq(data->irq);
}

static struct irq_chip atari_irq_chip = {
	.name		= "atari",
	.irq_startup	= atari_irq_startup,
	.irq_shutdown	= atari_irq_shutdown,
	.irq_enable	= atari_irq_enable,
	.irq_disable	= atari_irq_disable,
};


void __init atari_init_IRQ(void)
{
	m68k_setup_user_interrupt(VEC_USER, NUM_ATARI_SOURCES - IRQ_USER);
	m68k_setup_irq_controller(&atari_irq_chip, handle_simple_irq, 1,
				  NUM_ATARI_SOURCES - 1);

	

#ifdef ATARI_USE_SOFTWARE_EOI
	st_mfp.vec_adr  = 0x48;	
#else
	st_mfp.vec_adr  = 0x40;	
#endif
	st_mfp.int_en_a = 0x00;	
	st_mfp.int_en_b = 0x00;
	st_mfp.int_mk_a = 0xff;	
	st_mfp.int_mk_b = 0xff;

	if (ATARIHW_PRESENT(TT_MFP)) {
#ifdef ATARI_USE_SOFTWARE_EOI
		tt_mfp.vec_adr  = 0x58;		
#else
		tt_mfp.vec_adr  = 0x50;		
#endif
		tt_mfp.int_en_a = 0x00;		
		tt_mfp.int_en_b = 0x00;
		tt_mfp.int_mk_a = 0xff;		
		tt_mfp.int_mk_b = 0xff;
	}

	if (ATARIHW_PRESENT(SCC) && !atari_SCC_reset_done) {
		atari_scc.cha_a_ctrl = 9;
		MFPDELAY();
		atari_scc.cha_a_ctrl = (char) 0xc0; 
	}

	if (ATARIHW_PRESENT(SCU)) {
		
		tt_scu.sys_mask = 0x10;		
		tt_scu.vme_mask = 0x60;		
	} else {

		vectors[VEC_INT2] = falcon_hblhandler;
		vectors[VEC_INT4] = falcon_hblhandler;
	}

	if (ATARIHW_PRESENT(PCM_8BIT) && ATARIHW_PRESENT(MICROWIRE)) {
		atari_microwire_cmd(MW_LM1992_PSG_HIGH);
	}

	stdma_init();

	
	sound_ym.rd_data_reg_sel = 7;
	sound_ym.wd_data = 0xff;
}



unsigned long atari_register_vme_int(void)
{
	int i;

	for (i = 0; i < 32; i++)
		if ((free_vme_vec_bitmap & (1 << i)) == 0)
			break;

	if (i == 16)
		return 0;

	free_vme_vec_bitmap |= 1 << i;
	return VME_SOURCE_BASE + i;
}
EXPORT_SYMBOL(atari_register_vme_int);


void atari_unregister_vme_int(unsigned long irq)
{
	if (irq >= VME_SOURCE_BASE && irq < VME_SOURCE_BASE + VME_MAX_SOURCES) {
		irq -= VME_SOURCE_BASE;
		free_vme_vec_bitmap &= ~(1 << irq);
	}
}
EXPORT_SYMBOL(atari_unregister_vme_int);


