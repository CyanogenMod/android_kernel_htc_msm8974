
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/delay.h>

#include <asm/irq.h>
#include <asm/macintosh.h>
#include <asm/macints.h>
#include <asm/mac_via.h>
#include <asm/mac_psc.h>
#include <asm/mac_oss.h>
#include <asm/mac_iop.h>
#include <asm/mac_baboon.h>
#include <asm/hwtest.h>
#include <asm/irq_regs.h>

#define SHUTUP_SONIC


irqreturn_t mac_nmi_handler(int, void *);
irqreturn_t mac_debug_handler(int, void *);


static unsigned int mac_irq_startup(struct irq_data *);
static void mac_irq_shutdown(struct irq_data *);

static struct irq_chip mac_irq_chip = {
	.name		= "mac",
	.irq_enable	= mac_irq_enable,
	.irq_disable	= mac_irq_disable,
	.irq_startup	= mac_irq_startup,
	.irq_shutdown	= mac_irq_shutdown,
};

void __init mac_init_IRQ(void)
{
#ifdef DEBUG_MACINTS
	printk("mac_init_IRQ(): Setting things up...\n");
#endif
	m68k_setup_irq_controller(&mac_irq_chip, handle_simple_irq, IRQ_USER,
				  NUM_MAC_SOURCES - IRQ_USER);
	
#ifdef SHUTUP_SONIC
	printk("Killing onboard sonic... ");
	
	if (hwreg_present((void*)(0x50f0a000))) {
		*(long *)(0x50f0a014) = 0x7fffL;
		*(long *)(0x50f0a010) = 0L;
	}
	printk("Done.\n");
#endif 


	if (oss_present)
		oss_register_interrupts();
	else
		via_register_interrupts();
	if (psc_present)
		psc_register_interrupts();
	if (baboon_present)
		baboon_register_interrupts();
	iop_register_interrupts();
	if (request_irq(IRQ_AUTO_7, mac_nmi_handler, 0, "NMI",
			mac_nmi_handler))
		pr_err("Couldn't register NMI\n");
#ifdef DEBUG_MACINTS
	printk("mac_init_IRQ(): Done!\n");
#endif
}


void mac_irq_enable(struct irq_data *data)
{
	int irq = data->irq;
	int irq_src = IRQ_SRC(irq);

	switch(irq_src) {
	case 1:
	case 2:
	case 7:
		if (oss_present)
			oss_irq_enable(irq);
		else
			via_irq_enable(irq);
		break;
	case 3:
	case 4:
	case 5:
	case 6:
		if (psc_present)
			psc_irq_enable(irq);
		else if (oss_present)
			oss_irq_enable(irq);
		break;
	case 8:
		if (baboon_present)
			baboon_irq_enable(irq);
		break;
	}
}

void mac_irq_disable(struct irq_data *data)
{
	int irq = data->irq;
	int irq_src = IRQ_SRC(irq);

	switch(irq_src) {
	case 1:
	case 2:
	case 7:
		if (oss_present)
			oss_irq_disable(irq);
		else
			via_irq_disable(irq);
		break;
	case 3:
	case 4:
	case 5:
	case 6:
		if (psc_present)
			psc_irq_disable(irq);
		else if (oss_present)
			oss_irq_disable(irq);
		break;
	case 8:
		if (baboon_present)
			baboon_irq_disable(irq);
		break;
	}
}

static unsigned int mac_irq_startup(struct irq_data *data)
{
	int irq = data->irq;

	if (IRQ_SRC(irq) == 7 && !oss_present)
		via_nubus_irq_startup(irq);
	else
		mac_irq_enable(data);

	return 0;
}

static void mac_irq_shutdown(struct irq_data *data)
{
	int irq = data->irq;

	if (IRQ_SRC(irq) == 7 && !oss_present)
		via_nubus_irq_shutdown(irq);
	else
		mac_irq_disable(data);
}

static int num_debug[8];

irqreturn_t mac_debug_handler(int irq, void *dev_id)
{
	if (num_debug[irq] < 10) {
		printk("DEBUG: Unexpected IRQ %d\n", irq);
		num_debug[irq]++;
	}
	return IRQ_HANDLED;
}

static int in_nmi;
static volatile int nmi_hold;

irqreturn_t mac_nmi_handler(int irq, void *dev_id)
{
	int i;

	in_nmi++;
	for (i=0; i<100; i++)
		udelay(1000);

	if (in_nmi == 1) {
		nmi_hold = 1;
		printk("... pausing, press NMI to resume ...");
	} else {
		printk(" ok!\n");
		nmi_hold = 0;
	}

	barrier();

	while (nmi_hold == 1)
		udelay(1000);

	if (console_loglevel >= 8) {
#if 0
		struct pt_regs *fp = get_irq_regs();
		show_state();
		printk("PC: %08lx\nSR: %04x  SP: %p\n", fp->pc, fp->sr, fp);
		printk("d0: %08lx    d1: %08lx    d2: %08lx    d3: %08lx\n",
		       fp->d0, fp->d1, fp->d2, fp->d3);
		printk("d4: %08lx    d5: %08lx    a0: %08lx    a1: %08lx\n",
		       fp->d4, fp->d5, fp->a0, fp->a1);

		if (STACK_MAGIC != *(unsigned long *)current->kernel_stack_page)
			printk("Corrupted stack page\n");
		printk("Process %s (pid: %d, stackpage=%08lx)\n",
			current->comm, current->pid, current->kernel_stack_page);
		if (intr_count == 1)
			dump_stack((struct frame *)fp);
#else
		
#endif
	}
	in_nmi--;
	return IRQ_HANDLED;
}
