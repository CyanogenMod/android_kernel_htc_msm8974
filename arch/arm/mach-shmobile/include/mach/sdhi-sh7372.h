#ifndef SDHI_SH7372_H
#define SDHI_SH7372_H

#define SDGENCNTA       0xfe40009c

static void udelay(int us)
{
	__raw_writel(us * 150, SDGENCNTA);
	while(__raw_readl(SDGENCNTA)) ;
}

static void msleep(int ms)
{
	udelay(ms * 1000);
}

#endif
