/*
 * Linux 2.6.32 and later Kernel module for VMware MVP Hypervisor Support
 *
 * Copyright (C) 2010-2013 VMware, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; see the file COPYING.  If not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */
#line 5

#ifndef _MVP_BALLOON_H
#define _MVP_BALLOON_H

#define INCLUDE_ALLOW_VMX
#define INCLUDE_ALLOW_PV
#define INCLUDE_ALLOW_GPL
#define INCLUDE_ALLOW_GUESTUSER
#define INCLUDE_ALLOW_MODULE
#include "include_check.h"

#define BALLOON_WATCHDOG_TIMEOUT_SECS 90

#define USE_OOM_SCORE_ADJ_THRESHOLD 50

typedef union {
	struct {
		
		int32 delta:21;
	};
	uint32 u;
} Balloon_GetDeltaRet;


typedef enum {
	BALLOON_ANDROID_GUEST_MIN_FREE_FOREGROUND_APP_PAGES = 1536,
	BALLOON_ANDROID_GUEST_MIN_FREE_VISIBLE_APP_PAGES = 2048,
	BALLOON_ANDROID_GUEST_MIN_FREE_SECONDARY_SERVER_PAGES = 4096,
	BALLOON_ANDROID_GUEST_MIN_FREE_BACKUP_APP_PAGES = 4096,
	BALLOON_ANDROID_GUEST_MIN_FREE_HOME_APP_PAGES = 4096,
	BALLOON_ANDROID_GUEST_MIN_FREE_HIDDEN_APP_PAGES = 5120,
	BALLOON_ANDROID_GUEST_MIN_FREE_CONTENT_PROVIDER_PAGES = 5632,
	BALLOON_ANDROID_GUEST_MIN_FREE_EMPTY_APP_MEM_PAGES = 6144
} Balloon_AndroidGuestMinFreePages;


typedef enum {
	HOST_FOREGROUND_APP = 0,
	HOST_VISIBLE_APP = 1,
	HOST_PERCEPTIBLE_APP = 2,
	HOST_BACKUP_APP = 3,
	HOST_HIDDEN_APP_MIN = 4,
	HOST_HIDDEN_APP_MAX = 5,
} Balloon_AndroidHostApp;

static inline int32
Balloon_LowMemDistance(uint32 freePages,
		       uint32 filePages,
		       uint32 emptyAppPages)
{
	return filePages >= emptyAppPages ?
		(int32) (freePages + filePages) - (int32) emptyAppPages :
		(int32) freePages - (int32) emptyAppPages;
}

#ifdef __KERNEL__
static uint32
Balloon_AndroidBackgroundPages(uint32 minHiddenAppOOMScoreAdj)
{
	uint32 backgroundPages = 0, nonBackgroundPages = 0;
	struct task_struct *t;

	rcu_read_lock();

	for_each_process(t) {
		int oom_score_adj = 0;

		task_lock(t);

		if (t->signal == NULL) {
			task_unlock(t);
			continue;
		} else {
			oom_score_adj =
			  minHiddenAppOOMScoreAdj > USE_OOM_SCORE_ADJ_THRESHOLD
				?  t->signal->oom_score_adj
				: t->signal->oom_adj;
		}

		if (t->mm != NULL) {
#ifdef BALLOON_DEBUG_PRINT_ANDROID_PAGES
			pr_info("Balloon_AndroidBackgroundPages: %d %d %s\n",
				oom_score_adj,
				(int)get_mm_counter(t->mm, MM_ANONPAGES),
				t->comm);
#endif

			if (oom_score_adj >= (int)minHiddenAppOOMScoreAdj)
				backgroundPages +=
					get_mm_counter(t->mm, MM_ANONPAGES);
			else
				nonBackgroundPages +=
					get_mm_counter(t->mm, MM_ANONPAGES);
		}

		task_unlock(t);
	}

	rcu_read_unlock();

#ifdef BALLOON_DEBUG_PRINT_ANDROID_PAGES
	pr_info("Balloon_AndroidBackgroundPages: non-background pages: %d " \
		"background pages: %d\n",
		nonBackgroundPages,
		backgroundPages);
#endif

	return backgroundPages;
}
#endif

#endif
