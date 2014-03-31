/* -*- linux-c -*-
 *
 * (C) 2003 zecke@handhelds.org
 *
 * GPL version 2
 *
 * based on arch/arm/kernel/apm.c
 * factor out the information needed by architectures to provide
 * apm status
 */
#ifndef __LINUX_APM_EMULATION_H
#define __LINUX_APM_EMULATION_H

#include <linux/apm_bios.h>

struct apm_power_info {
	unsigned char	ac_line_status;
#define APM_AC_OFFLINE			0
#define APM_AC_ONLINE			1
#define APM_AC_BACKUP			2
#define APM_AC_UNKNOWN			0xff

	unsigned char	battery_status;
#define APM_BATTERY_STATUS_HIGH		0
#define APM_BATTERY_STATUS_LOW		1
#define APM_BATTERY_STATUS_CRITICAL	2
#define APM_BATTERY_STATUS_CHARGING	3
#define APM_BATTERY_STATUS_NOT_PRESENT	4
#define APM_BATTERY_STATUS_UNKNOWN	0xff

	unsigned char	battery_flag;
#define APM_BATTERY_FLAG_HIGH		(1 << 0)
#define APM_BATTERY_FLAG_LOW		(1 << 1)
#define APM_BATTERY_FLAG_CRITICAL	(1 << 2)
#define APM_BATTERY_FLAG_CHARGING	(1 << 3)
#define APM_BATTERY_FLAG_NOT_PRESENT	(1 << 7)
#define APM_BATTERY_FLAG_UNKNOWN	0xff

	int		battery_life;
	int		time;
	int		units;
#define APM_UNITS_MINS			0
#define APM_UNITS_SECS			1
#define APM_UNITS_UNKNOWN		-1

};

extern void (*apm_get_power_status)(struct apm_power_info *);

void apm_queue_event(apm_event_t event);

#endif 
