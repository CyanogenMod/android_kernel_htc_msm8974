/* -*- mode: c; c-basic-offset: 8; -*-
 * vim: noexpandtab sw=8 ts=8 sts=0:
 *
 * Copyright (C) 2005 Oracle.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 021110-1307, USA.
 */

#ifndef O2CLUSTER_MASKLOG_H
#define O2CLUSTER_MASKLOG_H


#include <linux/sched.h>

#define ML_TCP		0x0000000000000001ULL 
#define ML_MSG		0x0000000000000002ULL 
#define ML_SOCKET	0x0000000000000004ULL 
#define ML_HEARTBEAT	0x0000000000000008ULL 
#define ML_HB_BIO	0x0000000000000010ULL 
#define ML_DLMFS	0x0000000000000020ULL 
#define ML_DLM		0x0000000000000040ULL 
#define ML_DLM_DOMAIN	0x0000000000000080ULL 
#define ML_DLM_THREAD	0x0000000000000100ULL 
#define ML_DLM_MASTER	0x0000000000000200ULL 
#define ML_DLM_RECOVERY	0x0000000000000400ULL 
#define ML_DLM_GLUE	0x0000000000000800ULL 
#define ML_VOTE		0x0000000000001000ULL 
#define ML_CONN		0x0000000000002000ULL 
#define ML_QUORUM	0x0000000000004000ULL 
#define ML_BASTS	0x0000000000008000ULL 
#define ML_CLUSTER	0x0000000000010000ULL 

#define ML_ERROR	0x1000000000000000ULL 
#define ML_NOTICE	0x2000000000000000ULL 
#define ML_KTHREAD	0x4000000000000000ULL 

#define MLOG_INITIAL_AND_MASK (ML_ERROR|ML_NOTICE)
#ifndef MLOG_MASK_PREFIX
#define MLOG_MASK_PREFIX 0
#endif

#if defined(CONFIG_OCFS2_DEBUG_MASKLOG)
#define ML_ALLOWED_BITS ~0
#else
#define ML_ALLOWED_BITS (ML_ERROR|ML_NOTICE)
#endif

#define MLOG_MAX_BITS 64

struct mlog_bits {
	unsigned long words[MLOG_MAX_BITS / BITS_PER_LONG];
};

extern struct mlog_bits mlog_and_bits, mlog_not_bits;

#if BITS_PER_LONG == 32

#define __mlog_test_u64(mask, bits)			\
	( (u32)(mask & 0xffffffff) & bits.words[0] || 	\
	  ((u64)(mask) >> 32) & bits.words[1] )
#define __mlog_set_u64(mask, bits) do {			\
	bits.words[0] |= (u32)(mask & 0xffffffff);	\
       	bits.words[1] |= (u64)(mask) >> 32;		\
} while (0)
#define __mlog_clear_u64(mask, bits) do {		\
	bits.words[0] &= ~((u32)(mask & 0xffffffff));	\
       	bits.words[1] &= ~((u64)(mask) >> 32);		\
} while (0)
#define MLOG_BITS_RHS(mask) {				\
	{						\
		[0] = (u32)(mask & 0xffffffff),		\
		[1] = (u64)(mask) >> 32,		\
	}						\
}

#else 

#define __mlog_test_u64(mask, bits)	((mask) & bits.words[0])
#define __mlog_set_u64(mask, bits) do {		\
	bits.words[0] |= (mask);		\
} while (0)
#define __mlog_clear_u64(mask, bits) do {	\
	bits.words[0] &= ~(mask);		\
} while (0)
#define MLOG_BITS_RHS(mask) { { (mask) } }

#endif

#define __mlog_cpu_guess ({		\
	unsigned long _cpu = get_cpu();	\
	put_cpu();			\
	_cpu;				\
})

#define __mlog_printk(level, fmt, args...)				\
	printk(level "(%s,%u,%lu):%s:%d " fmt, current->comm,		\
	       task_pid_nr(current), __mlog_cpu_guess,			\
	       __PRETTY_FUNCTION__, __LINE__ , ##args)

#define mlog(mask, fmt, args...) do {					\
	u64 __m = MLOG_MASK_PREFIX | (mask);				\
	if ((__m & ML_ALLOWED_BITS) &&					\
	    __mlog_test_u64(__m, mlog_and_bits) &&			\
	    !__mlog_test_u64(__m, mlog_not_bits)) {			\
		if (__m & ML_ERROR)					\
			__mlog_printk(KERN_ERR, "ERROR: "fmt , ##args);	\
		else if (__m & ML_NOTICE)				\
			__mlog_printk(KERN_NOTICE, fmt , ##args);	\
		else __mlog_printk(KERN_INFO, fmt , ##args);		\
	}								\
} while (0)

#define mlog_errno(st) do {						\
	int _st = (st);							\
	if (_st != -ERESTARTSYS && _st != -EINTR &&			\
	    _st != AOP_TRUNCATED_PAGE && _st != -ENOSPC)		\
		mlog(ML_ERROR, "status = %lld\n", (long long)_st);	\
} while (0)

#define mlog_bug_on_msg(cond, fmt, args...) do {			\
	if (cond) {							\
		mlog(ML_ERROR, "bug expression: " #cond "\n");		\
		mlog(ML_ERROR, fmt, ##args);				\
		BUG();							\
	}								\
} while (0)

#include <linux/kobject.h>
#include <linux/sysfs.h>
int mlog_sys_init(struct kset *o2cb_subsys);
void mlog_sys_shutdown(void);

#endif 
