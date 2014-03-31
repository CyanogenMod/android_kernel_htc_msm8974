/*  Generic MTRR (Memory Type Range Register) ioctls.

    Copyright (C) 1997-1999  Richard Gooch

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public
    License along with this library; if not, write to the Free
    Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

    Richard Gooch may be reached by email at  rgooch@atnf.csiro.au
    The postal address is:
      Richard Gooch, c/o ATNF, P. O. Box 76, Epping, N.S.W., 2121, Australia.
*/
#ifndef _ASM_X86_MTRR_H
#define _ASM_X86_MTRR_H

#include <linux/types.h>
#include <linux/ioctl.h>
#include <linux/errno.h>

#define	MTRR_IOCTL_BASE	'M'


#ifdef __i386__
struct mtrr_sentry {
    unsigned long base;    
    unsigned int size;    
    unsigned int type;     
};

struct mtrr_gentry {
    unsigned int regnum;   
    unsigned long base;    
    unsigned int size;    
    unsigned int type;     
};

#else 

struct mtrr_sentry {
	__u64 base;		
	__u32 size;		
	__u32 type;		
};

struct mtrr_gentry {
	__u64 base;		
	__u32 size;		
	__u32 regnum;		
	__u32 type;		
	__u32 _pad;		
};

#endif 

struct mtrr_var_range {
	__u32 base_lo;
	__u32 base_hi;
	__u32 mask_lo;
	__u32 mask_hi;
};

typedef __u8 mtrr_type;

#define MTRR_NUM_FIXED_RANGES 88
#define MTRR_MAX_VAR_RANGES 256

struct mtrr_state_type {
	struct mtrr_var_range var_ranges[MTRR_MAX_VAR_RANGES];
	mtrr_type fixed_ranges[MTRR_NUM_FIXED_RANGES];
	unsigned char enabled;
	unsigned char have_fixed;
	mtrr_type def_type;
};

#define MTRRphysBase_MSR(reg) (0x200 + 2 * (reg))
#define MTRRphysMask_MSR(reg) (0x200 + 2 * (reg) + 1)

#define MTRRIOC_ADD_ENTRY        _IOW(MTRR_IOCTL_BASE,  0, struct mtrr_sentry)
#define MTRRIOC_SET_ENTRY        _IOW(MTRR_IOCTL_BASE,  1, struct mtrr_sentry)
#define MTRRIOC_DEL_ENTRY        _IOW(MTRR_IOCTL_BASE,  2, struct mtrr_sentry)
#define MTRRIOC_GET_ENTRY        _IOWR(MTRR_IOCTL_BASE, 3, struct mtrr_gentry)
#define MTRRIOC_KILL_ENTRY       _IOW(MTRR_IOCTL_BASE,  4, struct mtrr_sentry)
#define MTRRIOC_ADD_PAGE_ENTRY   _IOW(MTRR_IOCTL_BASE,  5, struct mtrr_sentry)
#define MTRRIOC_SET_PAGE_ENTRY   _IOW(MTRR_IOCTL_BASE,  6, struct mtrr_sentry)
#define MTRRIOC_DEL_PAGE_ENTRY   _IOW(MTRR_IOCTL_BASE,  7, struct mtrr_sentry)
#define MTRRIOC_GET_PAGE_ENTRY   _IOWR(MTRR_IOCTL_BASE, 8, struct mtrr_gentry)
#define MTRRIOC_KILL_PAGE_ENTRY  _IOW(MTRR_IOCTL_BASE,  9, struct mtrr_sentry)

#define MTRR_TYPE_UNCACHABLE 0
#define MTRR_TYPE_WRCOMB     1
#define MTRR_TYPE_WRTHROUGH  4
#define MTRR_TYPE_WRPROT     5
#define MTRR_TYPE_WRBACK     6
#define MTRR_NUM_TYPES       7

#ifdef __KERNEL__

# ifdef CONFIG_MTRR
extern u8 mtrr_type_lookup(u64 addr, u64 end);
extern void mtrr_save_fixed_ranges(void *);
extern void mtrr_save_state(void);
extern int mtrr_add(unsigned long base, unsigned long size,
		    unsigned int type, bool increment);
extern int mtrr_add_page(unsigned long base, unsigned long size,
			 unsigned int type, bool increment);
extern int mtrr_del(int reg, unsigned long base, unsigned long size);
extern int mtrr_del_page(int reg, unsigned long base, unsigned long size);
extern void mtrr_centaur_report_mcr(int mcr, u32 lo, u32 hi);
extern void mtrr_ap_init(void);
extern void mtrr_bp_init(void);
extern void set_mtrr_aps_delayed_init(void);
extern void mtrr_aps_init(void);
extern void mtrr_bp_restore(void);
extern int mtrr_trim_uncached_memory(unsigned long end_pfn);
extern int amd_special_default_mtrr(void);
#  else
static inline u8 mtrr_type_lookup(u64 addr, u64 end)
{
	return 0xff;
}
#define mtrr_save_fixed_ranges(arg) do {} while (0)
#define mtrr_save_state() do {} while (0)
static inline int mtrr_add(unsigned long base, unsigned long size,
			   unsigned int type, bool increment)
{
    return -ENODEV;
}
static inline int mtrr_add_page(unsigned long base, unsigned long size,
				unsigned int type, bool increment)
{
    return -ENODEV;
}
static inline int mtrr_del(int reg, unsigned long base, unsigned long size)
{
    return -ENODEV;
}
static inline int mtrr_del_page(int reg, unsigned long base, unsigned long size)
{
    return -ENODEV;
}
static inline int mtrr_trim_uncached_memory(unsigned long end_pfn)
{
	return 0;
}
static inline void mtrr_centaur_report_mcr(int mcr, u32 lo, u32 hi)
{
}

#define mtrr_ap_init() do {} while (0)
#define mtrr_bp_init() do {} while (0)
#define set_mtrr_aps_delayed_init() do {} while (0)
#define mtrr_aps_init() do {} while (0)
#define mtrr_bp_restore() do {} while (0)
#  endif

#ifdef CONFIG_COMPAT
#include <linux/compat.h>

struct mtrr_sentry32 {
    compat_ulong_t base;    
    compat_uint_t size;    
    compat_uint_t type;     
};

struct mtrr_gentry32 {
    compat_ulong_t regnum;   
    compat_uint_t base;    
    compat_uint_t size;    
    compat_uint_t type;     
};

#define MTRR_IOCTL_BASE 'M'

#define MTRRIOC32_ADD_ENTRY      _IOW(MTRR_IOCTL_BASE,  0, struct mtrr_sentry32)
#define MTRRIOC32_SET_ENTRY      _IOW(MTRR_IOCTL_BASE,  1, struct mtrr_sentry32)
#define MTRRIOC32_DEL_ENTRY      _IOW(MTRR_IOCTL_BASE,  2, struct mtrr_sentry32)
#define MTRRIOC32_GET_ENTRY      _IOWR(MTRR_IOCTL_BASE, 3, struct mtrr_gentry32)
#define MTRRIOC32_KILL_ENTRY     _IOW(MTRR_IOCTL_BASE,  4, struct mtrr_sentry32)
#define MTRRIOC32_ADD_PAGE_ENTRY _IOW(MTRR_IOCTL_BASE,  5, struct mtrr_sentry32)
#define MTRRIOC32_SET_PAGE_ENTRY _IOW(MTRR_IOCTL_BASE,  6, struct mtrr_sentry32)
#define MTRRIOC32_DEL_PAGE_ENTRY _IOW(MTRR_IOCTL_BASE,  7, struct mtrr_sentry32)
#define MTRRIOC32_GET_PAGE_ENTRY _IOWR(MTRR_IOCTL_BASE, 8, struct mtrr_gentry32)
#define MTRRIOC32_KILL_PAGE_ENTRY		\
				 _IOW(MTRR_IOCTL_BASE,  9, struct mtrr_sentry32)
#endif 

#endif 

#endif 
