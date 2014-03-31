#ifndef _ASM_X86_MC146818RTC_H
#define _ASM_X86_MC146818RTC_H

#include <asm/io.h>
#include <asm/processor.h>
#include <linux/mc146818rtc.h>

#ifndef RTC_PORT
#define RTC_PORT(x)	(0x70 + (x))
#define RTC_ALWAYS_BCD	1	
#endif

#if defined(CONFIG_X86_32) && defined(__HAVE_ARCH_CMPXCHG)
#include <linux/smp.h>
extern volatile unsigned long cmos_lock;


static inline void lock_cmos(unsigned char reg)
{
	unsigned long new;
	new = ((smp_processor_id() + 1) << 8) | reg;
	for (;;) {
		if (cmos_lock) {
			cpu_relax();
			continue;
		}
		if (__cmpxchg(&cmos_lock, 0, new, sizeof(cmos_lock)) == 0)
			return;
	}
}

static inline void unlock_cmos(void)
{
	cmos_lock = 0;
}

static inline int do_i_have_lock_cmos(void)
{
	return (cmos_lock >> 8) == (smp_processor_id() + 1);
}

static inline unsigned char current_lock_cmos_reg(void)
{
	return cmos_lock & 0xff;
}

#define lock_cmos_prefix(reg)			\
	do {					\
		unsigned long cmos_flags;	\
		local_irq_save(cmos_flags);	\
		lock_cmos(reg)

#define lock_cmos_suffix(reg)			\
	unlock_cmos();				\
	local_irq_restore(cmos_flags);		\
	} while (0)
#else
#define lock_cmos_prefix(reg) do {} while (0)
#define lock_cmos_suffix(reg) do {} while (0)
#define lock_cmos(reg) do { } while (0)
#define unlock_cmos() do { } while (0)
#define do_i_have_lock_cmos() 0
#define current_lock_cmos_reg() 0
#endif

#define CMOS_READ(addr) rtc_cmos_read(addr)
#define CMOS_WRITE(val, addr) rtc_cmos_write(val, addr)
unsigned char rtc_cmos_read(unsigned char addr);
void rtc_cmos_write(unsigned char val, unsigned char addr);

extern int mach_set_rtc_mmss(unsigned long nowtime);
extern unsigned long mach_get_cmos_time(void);

#define RTC_IRQ 8

#endif 
