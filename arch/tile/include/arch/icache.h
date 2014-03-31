/*
 * Copyright 2010 Tilera Corporation. All Rights Reserved.
 *
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation, version 2.
 *
 *   This program is distributed in the hope that it will be useful, but
 *   WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE, GOOD TITLE or
 *   NON INFRINGEMENT.  See the GNU General Public License for
 *   more details.
 *
 */


#ifndef __ARCH_ICACHE_H__
#define __ARCH_ICACHE_H__

#include <arch/chip.h>


static __inline void
invalidate_icache(const void* addr, unsigned long size,
                  unsigned long page_size)
{
  const unsigned long cache_way_size =
    CHIP_L1I_CACHE_SIZE() / CHIP_L1I_ASSOC();
  unsigned long max_useful_size;
  const char* start, *end;
  long num_passes;

  if (__builtin_expect(size == 0, 0))
    return;

#ifdef __tilegx__
  
  max_useful_size = (page_size < cache_way_size) ? page_size : cache_way_size;

  
  num_passes = 1;
#else
  
  max_useful_size = cache_way_size;

  num_passes = cache_way_size >> __builtin_ctzl(page_size);
#endif

  if (__builtin_expect(size > max_useful_size, 0))
    size = max_useful_size;

  
  start = (const char *)((unsigned long)addr & -CHIP_L1I_LINE_SIZE());
  end = (const char*)addr + size - 1;

  __insn_mf();

  do
  {
    const char* p;

    for (p = start; p <= end; p += CHIP_L1I_LINE_SIZE())
      __insn_icoh(p);

    start += page_size;
    end += page_size;
  }
  while (--num_passes > 0);

  __insn_drain();
}


#endif 
