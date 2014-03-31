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
 */



#ifndef __ARCH_SIM_H__
#define __ARCH_SIM_H__

#include <arch/sim_def.h>
#include <arch/abi.h>

#ifndef __ASSEMBLER__

#include <arch/spr_def.h>


static inline int
sim_is_simulator(void)
{
  return __insn_mfspr(SPR_SIM_CONTROL) != 0;
}


static __inline void
sim_checkpoint(void)
{
  __insn_mtspr(SPR_SIM_CONTROL, SIM_CONTROL_CHECKPOINT);
}


static __inline unsigned int
sim_get_tracing(void)
{
  return __insn_mfspr(SPR_SIM_CONTROL) & SIM_TRACE_FLAG_MASK;
}


static __inline void
sim_set_tracing(unsigned int mask)
{
  __insn_mtspr(SPR_SIM_CONTROL, SIM_TRACE_SPR_ARG(mask));
}


static __inline void
sim_dump(unsigned int mask)
{
  __insn_mtspr(SPR_SIM_CONTROL, SIM_DUMP_SPR_ARG(mask));
}


/**
 * Print a string to the simulator stdout.
 *
 * @param str The string to be written.
 */
static __inline void
sim_print(const char* str)
{
  for ( ; *str != '\0'; str++)
  {
    __insn_mtspr(SPR_SIM_CONTROL, SIM_CONTROL_PUTC |
                 (*str << _SIM_CONTROL_OPERATOR_BITS));
  }
  __insn_mtspr(SPR_SIM_CONTROL, SIM_CONTROL_PUTC |
               (SIM_PUTC_FLUSH_BINARY << _SIM_CONTROL_OPERATOR_BITS));
}


/**
 * Print a string to the simulator stdout.
 *
 * @param str The string to be written (a newline is automatically added).
 */
static __inline void
sim_print_string(const char* str)
{
  for ( ; *str != '\0'; str++)
  {
    __insn_mtspr(SPR_SIM_CONTROL, SIM_CONTROL_PUTC |
                 (*str << _SIM_CONTROL_OPERATOR_BITS));
  }
  __insn_mtspr(SPR_SIM_CONTROL, SIM_CONTROL_PUTC |
               (SIM_PUTC_FLUSH_STRING << _SIM_CONTROL_OPERATOR_BITS));
}


static __inline void
sim_command(const char* str)
{
  int c;
  do
  {
    c = *str++;
    __insn_mtspr(SPR_SIM_CONTROL, SIM_CONTROL_COMMAND |
                 (c << _SIM_CONTROL_OPERATOR_BITS));
  }
  while (c);
}



#ifndef __DOXYGEN__

static __inline long _sim_syscall0(int val)
{
  long result;
  __asm__ __volatile__ ("mtspr SIM_CONTROL, r0"
                        : "=R00" (result) : "R00" (val));
  return result;
}

static __inline long _sim_syscall1(int val, long arg1)
{
  long result;
  __asm__ __volatile__ ("{ and zero, r1, r1; mtspr SIM_CONTROL, r0 }"
                        : "=R00" (result) : "R00" (val), "R01" (arg1));
  return result;
}

static __inline long _sim_syscall2(int val, long arg1, long arg2)
{
  long result;
  __asm__ __volatile__ ("{ and zero, r1, r2; mtspr SIM_CONTROL, r0 }"
                        : "=R00" (result)
                        : "R00" (val), "R01" (arg1), "R02" (arg2));
  return result;
}


static __inline long _sim_syscall3(int val, long arg1, long arg2, long arg3)
{
  long result;
  __asm__ __volatile__ ("{ and zero, r3, r3 };"
                        "{ and zero, r1, r2; mtspr SIM_CONTROL, r0 }"
                        : "=R00" (result)
                        : "R00" (val), "R01" (arg1), "R02" (arg2),
                          "R03" (arg3));
  return result;
}

static __inline long _sim_syscall4(int val, long arg1, long arg2, long arg3,
                                  long arg4)
{
  long result;
  __asm__ __volatile__ ("{ and zero, r3, r4 };"
                        "{ and zero, r1, r2; mtspr SIM_CONTROL, r0 }"
                        : "=R00" (result)
                        : "R00" (val), "R01" (arg1), "R02" (arg2),
                          "R03" (arg3), "R04" (arg4));
  return result;
}

static __inline long _sim_syscall5(int val, long arg1, long arg2, long arg3,
                                  long arg4, long arg5)
{
  long result;
  __asm__ __volatile__ ("{ and zero, r3, r4; and zero, r5, r5 };"
                        "{ and zero, r1, r2; mtspr SIM_CONTROL, r0 }"
                        : "=R00" (result)
                        : "R00" (val), "R01" (arg1), "R02" (arg2),
                          "R03" (arg3), "R04" (arg4), "R05" (arg5));
  return result;
}

#define _sim_syscall(syscall_num, nr, args...) \
  _sim_syscall##nr( \
    ((syscall_num) << _SIM_CONTROL_OPERATOR_BITS) | SIM_CONTROL_SYSCALL, \
    ##args)


#define SIM_WATCHPOINT_READ    1
#define SIM_WATCHPOINT_WRITE   2
#define SIM_WATCHPOINT_EXECUTE 4


static __inline int
sim_add_watchpoint(unsigned int process_id,
                   unsigned long address,
                   unsigned long size,
                   unsigned int access_mask,
                   unsigned long user_data)
{
  return _sim_syscall(SIM_SYSCALL_ADD_WATCHPOINT, 5, process_id,
                     address, size, access_mask, user_data);
}


static __inline int
sim_remove_watchpoint(unsigned int process_id,
                      unsigned long address,
                      unsigned long size,
                      unsigned int access_mask,
                      unsigned long user_data)
{
  return _sim_syscall(SIM_SYSCALL_REMOVE_WATCHPOINT, 5, process_id,
                     address, size, access_mask, user_data);
}


struct SimQueryWatchpointStatus
{
  int syscall_status;

  unsigned long address;

  
  unsigned long user_data;
};


static __inline struct SimQueryWatchpointStatus
sim_query_watchpoint(unsigned int process_id)
{
  struct SimQueryWatchpointStatus status;
  long val = SIM_CONTROL_SYSCALL |
    (SIM_SYSCALL_QUERY_WATCHPOINT << _SIM_CONTROL_OPERATOR_BITS);
  __asm__ __volatile__ ("{ and zero, r1, r1; mtspr SIM_CONTROL, r0 }"
                        : "=R00" (status.syscall_status),
                          "=R01" (status.address),
                          "=R02" (status.user_data)
                        : "R00" (val), "R01" (process_id));
  return status;
}


static __inline void
sim_validate_lines_evicted(unsigned long long pa, unsigned long length)
{
#ifdef __LP64__
  _sim_syscall(SIM_SYSCALL_VALIDATE_LINES_EVICTED, 2, pa, length);
#else
  _sim_syscall(SIM_SYSCALL_VALIDATE_LINES_EVICTED, 4,
               0 , (long)(pa), (long)(pa >> 32), length);
#endif
}


static __inline long
sim_query_cpu_speed(void)
{
  return _sim_syscall(SIM_SYSCALL_QUERY_CPU_SPEED, 0);
}

#endif 




static __inline int
sim_set_shaping(unsigned shim,
                unsigned type,
                unsigned units,
                unsigned rate)
{
  if ((rate & ~((1 << SIM_CONTROL_SHAPING_RATE_BITS) - 1)) != 0)
    return 1;

  __insn_mtspr(SPR_SIM_CONTROL, SIM_SHAPING_SPR_ARG(shim, type, units, rate));
  return 0;
}

#ifdef __tilegx__

static __inline void
sim_enable_mpipe_links(unsigned mpipe, unsigned long link_mask)
{
  __insn_mtspr(SPR_SIM_CONTROL,
               (SIM_CONTROL_ENABLE_MPIPE_LINK_MAGIC_BYTE |
                (mpipe << 8) | (1 << 16) | ((uint_reg_t)link_mask << 32)));
}

static __inline void
sim_disable_mpipe_links(unsigned mpipe, unsigned long link_mask)
{
  __insn_mtspr(SPR_SIM_CONTROL,
               (SIM_CONTROL_ENABLE_MPIPE_LINK_MAGIC_BYTE |
                (mpipe << 8) | (0 << 16) | ((uint_reg_t)link_mask << 32)));
}

#endif 



#ifndef __DOXYGEN__

#define sim_enable_functional() \
  __insn_mtspr(SPR_SIM_CONTROL, SIM_CONTROL_ENABLE_FUNCTIONAL)

#define sim_disable_functional() \
  __insn_mtspr(SPR_SIM_CONTROL, SIM_CONTROL_DISABLE_FUNCTIONAL)

#endif 



static __inline void
sim_profiler_enable(void)
{
  __insn_mtspr(SPR_SIM_CONTROL, SIM_CONTROL_PROFILER_ENABLE);
}


static __inline void
sim_profiler_disable(void)
{
  __insn_mtspr(SPR_SIM_CONTROL, SIM_CONTROL_PROFILER_DISABLE);
}


static __inline void
sim_profiler_set_enabled(int enabled)
{
  int val =
    enabled ? SIM_CONTROL_PROFILER_ENABLE : SIM_CONTROL_PROFILER_DISABLE;
  __insn_mtspr(SPR_SIM_CONTROL, val);
}


static __inline int
sim_profiler_is_enabled(void)
{
  return ((__insn_mfspr(SPR_SIM_CONTROL) & SIM_PROFILER_ENABLED_MASK) != 0);
}


static __inline void
sim_profiler_clear(void)
{
  __insn_mtspr(SPR_SIM_CONTROL, SIM_CONTROL_PROFILER_CLEAR);
}


static __inline void
sim_profiler_chip_enable(unsigned int mask)
{
  __insn_mtspr(SPR_SIM_CONTROL, SIM_PROFILER_CHIP_ENABLE_SPR_ARG(mask));
}


static __inline void
sim_profiler_chip_disable(unsigned int mask)
{
  __insn_mtspr(SPR_SIM_CONTROL, SIM_PROFILER_CHIP_DISABLE_SPR_ARG(mask));
}


static __inline void
sim_profiler_chip_clear(unsigned int mask)
{
  __insn_mtspr(SPR_SIM_CONTROL, SIM_PROFILER_CHIP_CLEAR_SPR_ARG(mask));
}



#ifndef __DOXYGEN__

static __inline void
sim_event_begin(unsigned int x)
{
#if defined(__tile__) && !defined(__NO_EVENT_SPR__)
  __insn_mtspr(SPR_EVENT_BEGIN, x);
#endif
}

static __inline void
sim_event_end(unsigned int x)
{
#if defined(__tile__) && !defined(__NO_EVENT_SPR__)
  __insn_mtspr(SPR_EVENT_END, x);
#endif
}

#endif 

#endif 

#endif 

