/*
 * Amiga Linux interrupt handling code
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive
 * for more details.
 */

#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/errno.h>
#include <linux/irq.h>

#include <asm/irq.h>
#include <asm/traps.h>
#include <asm/amigahw.h>
#include <asm/amigaints.h>
#include <asm/amipcmcia.h>



static void amiga_irq_enable(struct irq_data *data)
{
	amiga_custom.intena = IF_SETCLR | (1 << (data->irq - IRQ_USER));
}

static void amiga_irq_disable(struct irq_data *data)
{
	amiga_custom.intena = 1 << (data->irq - IRQ_USER);
}

static struct irq_chip amiga_irq_chip = {
	.name		= "amiga",
	.irq_enable	= amiga_irq_enable,
	.irq_disable	= amiga_irq_disable,
};



static void ami_int1(unsigned int irq, struct irq_desc *desc)
{
	unsigned short ints = amiga_custom.intreqr & amiga_custom.intenar;

	
	if (ints & IF_TBE) {
		amiga_custom.intreq = IF_TBE;
		generic_handle_irq(IRQ_AMIGA_TBE);
	}

	
	if (ints & IF_DSKBLK) {
		amiga_custom.intreq = IF_DSKBLK;
		generic_handle_irq(IRQ_AMIGA_DSKBLK);
	}

	
	if (ints & IF_SOFT) {
		amiga_custom.intreq = IF_SOFT;
		generic_handle_irq(IRQ_AMIGA_SOFT);
	}
}

static void ami_int3(unsigned int irq, struct irq_desc *desc)
{
	unsigned short ints = amiga_custom.intreqr & amiga_custom.intenar;

	
	if (ints & IF_BLIT) {
		amiga_custom.intreq = IF_BLIT;
		generic_handle_irq(IRQ_AMIGA_BLIT);
	}

	
	if (ints & IF_COPER) {
		amiga_custom.intreq = IF_COPER;
		generic_handle_irq(IRQ_AMIGA_COPPER);
	}

	
	if (ints & IF_VERTB) {
		amiga_custom.intreq = IF_VERTB;
		generic_handle_irq(IRQ_AMIGA_VERTB);
	}
}

static void ami_int4(unsigned int irq, struct irq_desc *desc)
{
	unsigned short ints = amiga_custom.intreqr & amiga_custom.intenar;

	
	if (ints & IF_AUD0) {
		amiga_custom.intreq = IF_AUD0;
		generic_handle_irq(IRQ_AMIGA_AUD0);
	}

	
	if (ints & IF_AUD1) {
		amiga_custom.intreq = IF_AUD1;
		generic_handle_irq(IRQ_AMIGA_AUD1);
	}

	
	if (ints & IF_AUD2) {
		amiga_custom.intreq = IF_AUD2;
		generic_handle_irq(IRQ_AMIGA_AUD2);
	}

	
	if (ints & IF_AUD3) {
		amiga_custom.intreq = IF_AUD3;
		generic_handle_irq(IRQ_AMIGA_AUD3);
	}
}

static void ami_int5(unsigned int irq, struct irq_desc *desc)
{
	unsigned short ints = amiga_custom.intreqr & amiga_custom.intenar;

	
	if (ints & IF_RBF) {
		
		generic_handle_irq(IRQ_AMIGA_RBF);
	}

	
	if (ints & IF_DSKSYN) {
		amiga_custom.intreq = IF_DSKSYN;
		generic_handle_irq(IRQ_AMIGA_DSKSYN);
	}
}



void __init amiga_init_IRQ(void)
{
	m68k_setup_irq_controller(&amiga_irq_chip, handle_simple_irq, IRQ_USER,
				  AMI_STD_IRQS);

	irq_set_chained_handler(IRQ_AUTO_1, ami_int1);
	irq_set_chained_handler(IRQ_AUTO_3, ami_int3);
	irq_set_chained_handler(IRQ_AUTO_4, ami_int4);
	irq_set_chained_handler(IRQ_AUTO_5, ami_int5);

	
	if (AMIGAHW_PRESENT(PCMCIA))
		gayle.inten = GAYLE_IRQ_IDE;

	
	amiga_custom.intena = 0x7fff;
	amiga_custom.intreq = 0x7fff;
	amiga_custom.intena = IF_SETCLR | IF_INTEN;

	cia_init_IRQ(&ciaa_base);
	cia_init_IRQ(&ciab_base);
}
