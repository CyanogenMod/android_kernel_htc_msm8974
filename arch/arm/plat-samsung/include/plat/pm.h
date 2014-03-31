/* arch/arm/plat-samsung/include/plat/pm.h
 *
 * Copyright (c) 2004 Simtec Electronics
 *	http://armlinux.simtec.co.uk/
 *	Written by Ben Dooks, <ben@simtec.co.uk>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/


#include <linux/irq.h>

struct device;

#ifdef CONFIG_PM

extern __init int s3c_pm_init(void);
extern __init int s3c64xx_pm_init(void);

#else

static inline int s3c_pm_init(void)
{
	return 0;
}

static inline int s3c64xx_pm_init(void)
{
	return 0;
}
#endif

extern unsigned long s3c_irqwake_intmask;
extern unsigned long s3c_irqwake_eintmask;

extern unsigned long s3c_irqwake_intallow;
extern unsigned long s3c_irqwake_eintallow;


extern void (*pm_cpu_prep)(void);
extern int (*pm_cpu_sleep)(unsigned long);


extern unsigned long s3c_pm_flags;

extern unsigned char pm_uart_udivslot;  


extern void s3c_cpu_resume(void);

extern int s3c2410_cpu_suspend(unsigned long);


struct sleep_save {
	void __iomem	*reg;
	unsigned long	val;
};

#define SAVE_ITEM(x) \
	{ .reg = (x) }

struct pm_uart_save {
	u32	ulcon;
	u32	ucon;
	u32	ufcon;
	u32	umcon;
	u32	ubrdiv;
	u32	udivslot;
};


extern void s3c_pm_do_save(struct sleep_save *ptr, int count);
extern void s3c_pm_do_restore(struct sleep_save *ptr, int count);
extern void s3c_pm_do_restore_core(struct sleep_save *ptr, int count);

#ifdef CONFIG_PM
extern int s3c_irqext_wake(struct irq_data *data, unsigned int state);
extern int s3c24xx_irq_suspend(void);
extern void s3c24xx_irq_resume(void);
#else
#define s3c_irqext_wake NULL
#define s3c24xx_irq_suspend NULL
#define s3c24xx_irq_resume  NULL
#endif

extern struct syscore_ops s3c24xx_irq_syscore_ops;


#ifdef CONFIG_SAMSUNG_PM_DEBUG
extern void s3c_pm_dbg(const char *msg, ...);

#define S3C_PMDBG(fmt...) s3c_pm_dbg(fmt)
#else
#define S3C_PMDBG(fmt...) printk(KERN_DEBUG fmt)
#endif

#ifdef CONFIG_S3C_PM_DEBUG_LED_SMDK
extern void s3c_pm_debug_smdkled(u32 set, u32 clear);

#else
static inline void s3c_pm_debug_smdkled(u32 set, u32 clear) { }
#endif 


#ifdef CONFIG_SAMSUNG_PM_CHECK
extern void s3c_pm_check_prepare(void);
extern void s3c_pm_check_restore(void);
extern void s3c_pm_check_cleanup(void);
extern void s3c_pm_check_store(void);
#else
#define s3c_pm_check_prepare() do { } while(0)
#define s3c_pm_check_restore() do { } while(0)
#define s3c_pm_check_cleanup() do { } while(0)
#define s3c_pm_check_store()   do { } while(0)
#endif

extern void s3c_pm_configure_extint(void);

extern void samsung_pm_restore_gpios(void);

extern void samsung_pm_save_gpios(void);

extern void s3c_pm_save_core(void);
extern void s3c_pm_restore_core(void);
