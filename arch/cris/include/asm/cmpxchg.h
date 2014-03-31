#ifndef __ASM_CRIS_CMPXCHG__
#define __ASM_CRIS_CMPXCHG__

#include <linux/irqflags.h>

static inline unsigned long __xchg(unsigned long x, volatile void * ptr, int size)
{
  unsigned long flags,temp;
  local_irq_save(flags); 
  switch (size) {
  case 1:
    *((unsigned char *)&temp) = x;
    x = *(unsigned char *)ptr;
    *(unsigned char *)ptr = *((unsigned char *)&temp);
    break;
  case 2:
    *((unsigned short *)&temp) = x;
    x = *(unsigned short *)ptr;
    *(unsigned short *)ptr = *((unsigned short *)&temp);
    break;
  case 4:
    temp = x;
    x = *(unsigned long *)ptr;
    *(unsigned long *)ptr = temp;
    break;
  }
  local_irq_restore(flags); 
  return x;
}

#define xchg(ptr,x) \
	((__typeof__(*(ptr)))__xchg((unsigned long)(x),(ptr),sizeof(*(ptr))))

#define tas(ptr) (xchg((ptr),1))

#include <asm-generic/cmpxchg-local.h>

#define cmpxchg_local(ptr, o, n)				 	       \
	((__typeof__(*(ptr)))__cmpxchg_local_generic((ptr), (unsigned long)(o),\
			(unsigned long)(n), sizeof(*(ptr))))
#define cmpxchg64_local(ptr, o, n) __cmpxchg64_local_generic((ptr), (o), (n))

#ifndef CONFIG_SMP
#include <asm-generic/cmpxchg.h>
#endif

#endif 
