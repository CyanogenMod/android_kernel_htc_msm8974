/*
 *  arch/arm/mach-pxa/include/mach/hardware.h
 *
 *  Author:	Nicolas Pitre
 *  Created:	Jun 15, 2001
 *  Copyright:	MontaVista Software Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __ASM_ARCH_HARDWARE_H
#define __ASM_ARCH_HARDWARE_H

#include <mach/addr-map.h>

#define UNCACHED_PHYS_0		0xff000000
#define UNCACHED_ADDR		UNCACHED_PHYS_0

#define io_v2p(x) (0x3c000000 + ((x) & 0x01ffffff) + (((x) & 0x0e000000) << 1))
#define io_p2v(x) IOMEM(0xf2000000 + ((x) & 0x01ffffff) + (((x) & 0x1c000000) >> 1))

#ifndef __ASSEMBLY__
# define __REG(x)	(*((volatile u32 __iomem *)io_p2v(x)))

# define __REG2(x,y)	\
	(*(volatile u32 __iomem*)((u32)&__REG(x) + (y)))

# define __PREG(x)	(io_v2p((u32)&(x)))

#else

# define __REG(x)	io_p2v(x)
# define __PREG(x)	io_v2p(x)

#endif

#ifndef __ASSEMBLY__

#include <asm/cputype.h>

#ifdef CONFIG_PXA25x
#define __cpu_is_pxa210(id)				\
	({						\
		unsigned int _id = (id) & 0xf3f0;	\
		_id == 0x2120;				\
	})

#define __cpu_is_pxa250(id)				\
	({						\
		unsigned int _id = (id) & 0xf3ff;	\
		_id <= 0x2105;				\
	})

#define __cpu_is_pxa255(id)				\
	({						\
		unsigned int _id = (id) & 0xffff;	\
		_id == 0x2d06;				\
	})

#define __cpu_is_pxa25x(id)				\
	({						\
		unsigned int _id = (id) & 0xf300;	\
		_id == 0x2100;				\
	})
#else
#define __cpu_is_pxa210(id)	(0)
#define __cpu_is_pxa250(id)	(0)
#define __cpu_is_pxa255(id)	(0)
#define __cpu_is_pxa25x(id)	(0)
#endif

#ifdef CONFIG_PXA27x
#define __cpu_is_pxa27x(id)				\
	({						\
		unsigned int _id = (id) >> 4 & 0xfff;	\
		_id == 0x411;				\
	})
#else
#define __cpu_is_pxa27x(id)	(0)
#endif

#ifdef CONFIG_CPU_PXA300
#define __cpu_is_pxa300(id)				\
	({						\
		unsigned int _id = (id) >> 4 & 0xfff;	\
		_id == 0x688;				\
	 })
#else
#define __cpu_is_pxa300(id)	(0)
#endif

#ifdef CONFIG_CPU_PXA310
#define __cpu_is_pxa310(id)				\
	({						\
		unsigned int _id = (id) >> 4 & 0xfff;	\
		_id == 0x689;				\
	 })
#else
#define __cpu_is_pxa310(id)	(0)
#endif

#ifdef CONFIG_CPU_PXA320
#define __cpu_is_pxa320(id)				\
	({						\
		unsigned int _id = (id) >> 4 & 0xfff;	\
		_id == 0x603 || _id == 0x682;		\
	 })
#else
#define __cpu_is_pxa320(id)	(0)
#endif

#ifdef CONFIG_CPU_PXA930
#define __cpu_is_pxa930(id)				\
	({						\
		unsigned int _id = (id) >> 4 & 0xfff;	\
		_id == 0x683;				\
	 })
#else
#define __cpu_is_pxa930(id)	(0)
#endif

#ifdef CONFIG_CPU_PXA935
#define __cpu_is_pxa935(id)				\
	({						\
		unsigned int _id = (id) >> 4 & 0xfff;	\
		_id == 0x693;				\
	 })
#else
#define __cpu_is_pxa935(id)	(0)
#endif

#ifdef CONFIG_CPU_PXA955
#define __cpu_is_pxa955(id)				\
	({						\
		unsigned int _id = (id) >> 4 & 0xfff;	\
		_id == 0x581 || _id == 0xc08		\
			|| _id == 0xb76;		\
	})
#else
#define __cpu_is_pxa955(id)	(0)
#endif

#define cpu_is_pxa210()					\
	({						\
		__cpu_is_pxa210(read_cpuid_id());	\
	})

#define cpu_is_pxa250()					\
	({						\
		__cpu_is_pxa250(read_cpuid_id());	\
	})

#define cpu_is_pxa255()                                 \
	({                                              \
		__cpu_is_pxa255(read_cpuid_id());       \
	})

#define cpu_is_pxa25x()					\
	({						\
		__cpu_is_pxa25x(read_cpuid_id());	\
	})

#define cpu_is_pxa27x()					\
	({						\
		__cpu_is_pxa27x(read_cpuid_id());	\
	})

#define cpu_is_pxa300()					\
	({						\
		__cpu_is_pxa300(read_cpuid_id());	\
	 })

#define cpu_is_pxa310()					\
	({						\
		__cpu_is_pxa310(read_cpuid_id());	\
	 })

#define cpu_is_pxa320()					\
	({						\
		__cpu_is_pxa320(read_cpuid_id());	\
	 })

#define cpu_is_pxa930()					\
	({						\
		__cpu_is_pxa930(read_cpuid_id());	\
	 })

#define cpu_is_pxa935()					\
	({						\
		__cpu_is_pxa935(read_cpuid_id());	\
	 })

#define cpu_is_pxa955()					\
	({						\
		__cpu_is_pxa955(read_cpuid_id());	\
	})


#if defined(CONFIG_PXA25x) || defined(CONFIG_PXA27x)
#define __cpu_is_pxa2xx(id)				\
	({						\
		unsigned int _id = (id) >> 13 & 0x7;	\
		_id <= 0x2;				\
	 })
#else
#define __cpu_is_pxa2xx(id)	(0)
#endif

#ifdef CONFIG_PXA3xx
#define __cpu_is_pxa3xx(id)				\
	({						\
		__cpu_is_pxa300(id)			\
			|| __cpu_is_pxa310(id)		\
			|| __cpu_is_pxa320(id)		\
			|| __cpu_is_pxa93x(id);		\
	 })
#else
#define __cpu_is_pxa3xx(id)	(0)
#endif

#if defined(CONFIG_CPU_PXA930) || defined(CONFIG_CPU_PXA935)
#define __cpu_is_pxa93x(id)				\
	({						\
		__cpu_is_pxa930(id)			\
			|| __cpu_is_pxa935(id);		\
	 })
#else
#define __cpu_is_pxa93x(id)	(0)
#endif

#ifdef CONFIG_PXA95x
#define __cpu_is_pxa95x(id)				\
	({						\
		__cpu_is_pxa955(id);			\
	})
#else
#define __cpu_is_pxa95x(id)	(0)
#endif

#define cpu_is_pxa2xx()					\
	({						\
		__cpu_is_pxa2xx(read_cpuid_id());	\
	 })

#define cpu_is_pxa3xx()					\
	({						\
		__cpu_is_pxa3xx(read_cpuid_id());	\
	 })

#define cpu_is_pxa93x()					\
	({						\
		__cpu_is_pxa93x(read_cpuid_id());	\
	 })

#define cpu_is_pxa95x()					\
	({						\
		__cpu_is_pxa95x(read_cpuid_id());	\
	})

extern unsigned int get_memclk_frequency_10khz(void);

extern unsigned long get_clock_tick_rate(void);
#endif

#endif  
