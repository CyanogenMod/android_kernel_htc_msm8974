#ifndef _ACPI_PMTMR_H_
#define _ACPI_PMTMR_H_

#include <linux/clocksource.h>

#define PMTMR_TICKS_PER_SEC 3579545

#define ACPI_PM_MASK CLOCKSOURCE_MASK(24)

#define ACPI_PM_OVRRUN	(1<<24)

#ifdef CONFIG_X86_PM_TIMER

extern u32 acpi_pm_read_verified(void);
extern u32 pmtmr_ioport;

static inline u32 acpi_pm_read_early(void)
{
	if (!pmtmr_ioport)
		return 0;
	
	return acpi_pm_read_verified() & ACPI_PM_MASK;
}

#else

static inline u32 acpi_pm_read_early(void)
{
	return 0;
}

#endif

#endif

