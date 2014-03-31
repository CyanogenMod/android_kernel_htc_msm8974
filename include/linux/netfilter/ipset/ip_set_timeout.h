#ifndef _IP_SET_TIMEOUT_H
#define _IP_SET_TIMEOUT_H

/* Copyright (C) 2003-2011 Jozsef Kadlecsik <kadlec@blackhole.kfki.hu>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifdef __KERNEL__

#define IPSET_GC_TIME			(3 * 60)

#define IPSET_GC_PERIOD(timeout) \
	((timeout/3) ? min_t(u32, (timeout)/3, IPSET_GC_TIME) : 1)

#define IPSET_NO_TIMEOUT	UINT_MAX

#define with_timeout(timeout)	((timeout) != IPSET_NO_TIMEOUT)

#define opt_timeout(opt, map)	\
	(with_timeout((opt)->timeout) ? (opt)->timeout : (map)->timeout)

static inline unsigned int
ip_set_timeout_uget(struct nlattr *tb)
{
	unsigned int timeout = ip_set_get_h32(tb);

	
	return timeout == IPSET_NO_TIMEOUT ? IPSET_NO_TIMEOUT - 1 : timeout;
}

#ifdef IP_SET_BITMAP_TIMEOUT


#define IPSET_ELEM_UNSET	0
#define IPSET_ELEM_PERMANENT	(UINT_MAX/2)

static inline bool
ip_set_timeout_test(unsigned long timeout)
{
	return timeout != IPSET_ELEM_UNSET &&
	       (timeout == IPSET_ELEM_PERMANENT ||
		time_is_after_jiffies(timeout));
}

static inline bool
ip_set_timeout_expired(unsigned long timeout)
{
	return timeout != IPSET_ELEM_UNSET &&
	       timeout != IPSET_ELEM_PERMANENT &&
	       time_is_before_jiffies(timeout);
}

static inline unsigned long
ip_set_timeout_set(u32 timeout)
{
	unsigned long t;

	if (!timeout)
		return IPSET_ELEM_PERMANENT;

	t = msecs_to_jiffies(timeout * 1000) + jiffies;
	if (t == IPSET_ELEM_UNSET || t == IPSET_ELEM_PERMANENT)
		
		t++;

	return t;
}

static inline u32
ip_set_timeout_get(unsigned long timeout)
{
	return timeout == IPSET_ELEM_PERMANENT ? 0 :
		jiffies_to_msecs(timeout - jiffies)/1000;
}

#else


#define IPSET_ELEM_PERMANENT	0

static inline bool
ip_set_timeout_test(unsigned long timeout)
{
	return timeout == IPSET_ELEM_PERMANENT ||
	       time_is_after_jiffies(timeout);
}

static inline bool
ip_set_timeout_expired(unsigned long timeout)
{
	return timeout != IPSET_ELEM_PERMANENT &&
	       time_is_before_jiffies(timeout);
}

static inline unsigned long
ip_set_timeout_set(u32 timeout)
{
	unsigned long t;

	if (!timeout)
		return IPSET_ELEM_PERMANENT;

	t = msecs_to_jiffies(timeout * 1000) + jiffies;
	if (t == IPSET_ELEM_PERMANENT)
		
		t++;

	return t;
}

static inline u32
ip_set_timeout_get(unsigned long timeout)
{
	return timeout == IPSET_ELEM_PERMANENT ? 0 :
		jiffies_to_msecs(timeout - jiffies)/1000;
}
#endif 

#endif	

#endif 
