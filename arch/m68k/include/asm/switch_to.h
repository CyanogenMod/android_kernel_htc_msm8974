#ifndef _M68K_SWITCH_TO_H
#define _M68K_SWITCH_TO_H

asmlinkage void resume(void);
#define switch_to(prev,next,last) do { \
  register void *_prev __asm__ ("a0") = (prev); \
  register void *_next __asm__ ("a1") = (next); \
  register void *_last __asm__ ("d1"); \
  __asm__ __volatile__("jbsr resume" \
		       : "=a" (_prev), "=a" (_next), "=d" (_last) \
		       : "0" (_prev), "1" (_next) \
		       : "d0", "d2", "d3", "d4", "d5"); \
  (last) = _last; \
} while (0)

#endif 
