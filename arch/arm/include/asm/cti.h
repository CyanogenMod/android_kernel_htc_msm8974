#ifndef __ASMARM_CTI_H
#define __ASMARM_CTI_H

#include	<asm/io.h>

#define		CTICONTROL		0x000
#define		CTISTATUS		0x004
#define		CTILOCK			0x008
#define		CTIPROTECTION		0x00C
#define		CTIINTACK		0x010
#define		CTIAPPSET		0x014
#define		CTIAPPCLEAR		0x018
#define		CTIAPPPULSE		0x01c
#define		CTIINEN			0x020
#define		CTIOUTEN		0x0A0
#define		CTITRIGINSTATUS		0x130
#define		CTITRIGOUTSTATUS	0x134
#define		CTICHINSTATUS		0x138
#define		CTICHOUTSTATUS		0x13c
#define		CTIPERIPHID0		0xFE0
#define		CTIPERIPHID1		0xFE4
#define		CTIPERIPHID2		0xFE8
#define		CTIPERIPHID3		0xFEC
#define		CTIPCELLID0		0xFF0
#define		CTIPCELLID1		0xFF4
#define		CTIPCELLID2		0xFF8
#define		CTIPCELLID3		0xFFC

#define		LOCKACCESS		0xFB0
#define		LOCKSTATUS		0xFB4

#define		LOCKCODE		0xC5ACCE55

struct cti {
	void __iomem *base;
	int irq;
	int trig_out_for_irq;
};

static inline void cti_init(struct cti *cti,
	void __iomem *base, int irq, int trig_out)
{
	cti->base = base;
	cti->irq  = irq;
	cti->trig_out_for_irq = trig_out;
}

static inline void cti_map_trigger(struct cti *cti,
	int trig_in, int trig_out, int chan)
{
	void __iomem *base = cti->base;
	unsigned long val;

	val = __raw_readl(base + CTIINEN + trig_in * 4);
	val |= BIT(chan);
	__raw_writel(val, base + CTIINEN + trig_in * 4);

	val = __raw_readl(base + CTIOUTEN + trig_out * 4);
	val |= BIT(chan);
	__raw_writel(val, base + CTIOUTEN + trig_out * 4);
}

static inline void cti_enable(struct cti *cti)
{
	__raw_writel(0x1, cti->base + CTICONTROL);
}

static inline void cti_disable(struct cti *cti)
{
	__raw_writel(0, cti->base + CTICONTROL);
}

static inline void cti_irq_ack(struct cti *cti)
{
	void __iomem *base = cti->base;
	unsigned long val;

	val = __raw_readl(base + CTIINTACK);
	val |= BIT(cti->trig_out_for_irq);
	__raw_writel(val, base + CTIINTACK);
}

static inline void cti_unlock(struct cti *cti)
{
	void __iomem *base = cti->base;
	unsigned long val;

	val = __raw_readl(base + LOCKSTATUS);

	if (val & 1) {
		val = LOCKCODE;
		__raw_writel(val, base + LOCKACCESS);
	}
}

static inline void cti_lock(struct cti *cti)
{
	void __iomem *base = cti->base;
	unsigned long val;

	val = __raw_readl(base + LOCKSTATUS);

	if (!(val & 1)) {
		val = ~LOCKCODE;
		__raw_writel(val, base + LOCKACCESS);
	}
}
#endif
