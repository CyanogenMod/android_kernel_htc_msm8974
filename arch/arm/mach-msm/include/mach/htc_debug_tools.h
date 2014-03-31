/* Copyright (c) 2013, HTC Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#ifndef __HTC_DEBUG_TOOLS_H__
#define __HTC_DEBUG_TOOLS_H__

unsigned long htc_debug_get_sched_clock_ms(void);

#if defined(CONFIG_HTC_DEBUG_WORKQUEUE)
void htc_debug_workqueue_show_pending_work_on_gcwq(void);
#endif 

#if defined(CONFIG_HTC_DEBUG_WATCHDOG)
int htc_debug_watchdog_enabled(void);

void htc_debug_watchdog_check_pet(unsigned long long timestamp);
void htc_debug_watchdog_update_last_pet(unsigned long long last_pet);
void htc_debug_watchdog_dump_irqs(unsigned int dump);
#endif 

#endif 
