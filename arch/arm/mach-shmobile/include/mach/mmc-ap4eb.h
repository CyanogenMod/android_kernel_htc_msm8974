#ifndef MMC_AP4EB_H
#define MMC_AP4EB_H

#define PORT185CR      (void __iomem *)0xe60520b9
#define PORT186CR      (void __iomem *)0xe60520ba
#define PORT187CR      (void __iomem *)0xe60520bb
#define PORT188CR      (void __iomem *)0xe60520bc

#define PORTR191_160DR (void __iomem *)0xe6056014

static inline void mmc_init_progress(void)
{
       __raw_writeb(0x10, PORT185CR);
       __raw_writeb(0x10, PORT186CR);
       __raw_writeb(0x10, PORT187CR);
       __raw_writeb(0x10, PORT188CR);
}

static inline void mmc_update_progress(int n)
{
	__raw_writel((__raw_readl(PORTR191_160DR) & ~(0xf << 25)) |
		     (1 << (25 + n)), PORTR191_160DR);
}

#endif 
