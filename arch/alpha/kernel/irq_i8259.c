
#include <linux/init.h>
#include <linux/cache.h>
#include <linux/sched.h>
#include <linux/irq.h>
#include <linux/interrupt.h>

#include <asm/io.h>

#include "proto.h"
#include "irq_impl.h"


static unsigned int cached_irq_mask = 0xffff;
static DEFINE_SPINLOCK(i8259_irq_lock);

static inline void
i8259_update_irq_hw(unsigned int irq, unsigned long mask)
{
	int port = 0x21;
	if (irq & 8) mask >>= 8;
	if (irq & 8) port = 0xA1;
	outb(mask, port);
}

inline void
i8259a_enable_irq(struct irq_data *d)
{
	spin_lock(&i8259_irq_lock);
	i8259_update_irq_hw(d->irq, cached_irq_mask &= ~(1 << d->irq));
	spin_unlock(&i8259_irq_lock);
}

static inline void
__i8259a_disable_irq(unsigned int irq)
{
	i8259_update_irq_hw(irq, cached_irq_mask |= 1 << irq);
}

void
i8259a_disable_irq(struct irq_data *d)
{
	spin_lock(&i8259_irq_lock);
	__i8259a_disable_irq(d->irq);
	spin_unlock(&i8259_irq_lock);
}

void
i8259a_mask_and_ack_irq(struct irq_data *d)
{
	unsigned int irq = d->irq;

	spin_lock(&i8259_irq_lock);
	__i8259a_disable_irq(irq);

	
	if (irq >= 8) {
		outb(0xE0 | (irq - 8), 0xa0);   
		irq = 2;
	}
	outb(0xE0 | irq, 0x20);			
	spin_unlock(&i8259_irq_lock);
}

struct irq_chip i8259a_irq_type = {
	.name		= "XT-PIC",
	.irq_unmask	= i8259a_enable_irq,
	.irq_mask	= i8259a_disable_irq,
	.irq_mask_ack	= i8259a_mask_and_ack_irq,
};

void __init
init_i8259a_irqs(void)
{
	static struct irqaction cascade = {
		.handler	= no_action,
		.name		= "cascade",
	};

	long i;

	outb(0xff, 0x21);	
	outb(0xff, 0xA1);	

	for (i = 0; i < 16; i++) {
		irq_set_chip_and_handler(i, &i8259a_irq_type, handle_level_irq);
	}

	setup_irq(2, &cascade);
}


#if defined(CONFIG_ALPHA_GENERIC)
# define IACK_SC	alpha_mv.iack_sc
#elif defined(CONFIG_ALPHA_APECS)
# define IACK_SC	APECS_IACK_SC
#elif defined(CONFIG_ALPHA_LCA)
# define IACK_SC	LCA_IACK_SC
#elif defined(CONFIG_ALPHA_CIA)
# define IACK_SC	CIA_IACK_SC
#elif defined(CONFIG_ALPHA_PYXIS)
# define IACK_SC	PYXIS_IACK_SC
#elif defined(CONFIG_ALPHA_TITAN)
# define IACK_SC	TITAN_IACK_SC
#elif defined(CONFIG_ALPHA_TSUNAMI)
# define IACK_SC	TSUNAMI_IACK_SC
#elif defined(CONFIG_ALPHA_IRONGATE)
# define IACK_SC        IRONGATE_IACK_SC
#endif

#if defined(IACK_SC)
void
isa_device_interrupt(unsigned long vector)
{
	int j = *(vuip) IACK_SC;
	j &= 0xff;
	handle_irq(j);
}
#endif

#if defined(CONFIG_ALPHA_GENERIC) || !defined(IACK_SC)
void
isa_no_iack_sc_device_interrupt(unsigned long vector)
{
	unsigned long pic;

	pic = inb(0x20) | (inb(0xA0) << 8);	
	pic &= 0xFFFB;				

	while (pic) {
		int j = ffz(~pic);
		pic &= pic - 1;
		handle_irq(j);
	}
}
#endif
