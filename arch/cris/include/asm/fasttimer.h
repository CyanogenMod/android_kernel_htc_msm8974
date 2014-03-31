/*
 * linux/include/asm-cris/fasttimer.h
 *
 * Fast timers for ETRAX100LX
 * Copyright (C) 2000-2007 Axis Communications AB
 */
#include <linux/time.h> 
#include <linux/timex.h>

#ifdef CONFIG_ETRAX_FAST_TIMER

typedef void fast_timer_function_type(unsigned long);

struct fasttime_t {
	unsigned long tv_jiff;  
	unsigned long tv_usec;  
};

struct fast_timer{ 
  struct fast_timer *next;
  struct fast_timer *prev;
	struct fasttime_t tv_set;
	struct fasttime_t tv_expires;
  unsigned long delay_us;
  fast_timer_function_type *function;
  unsigned long data;
  const char *name;
};

extern struct fast_timer *fast_timer_list;

void start_one_shot_timer(struct fast_timer *t,
                          fast_timer_function_type *function,
                          unsigned long data,
                          unsigned long delay_us,
                          const char *name);

int del_fast_timer(struct fast_timer * t);


void schedule_usleep(unsigned long us);


int fast_timer_init(void);

#endif
