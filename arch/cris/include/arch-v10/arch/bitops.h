
#ifndef _CRIS_ARCH_BITOPS_H
#define _CRIS_ARCH_BITOPS_H

static inline unsigned long cris_swapnwbrlz(unsigned long w)
{

	unsigned long res;
	__asm__ ("swapnwbr %0 \n\t"
		 "lz %0,%0"
		 : "=r" (res) : "0" (w));
	return res;
}

static inline unsigned long cris_swapwbrlz(unsigned long w)
{
	unsigned res;
	__asm__ ("swapwbr %0 \n\t"
		 "lz %0,%0"
		 : "=r" (res)
		 : "0" (w));
	return res;
}

static inline unsigned long ffz(unsigned long w)
{
	return cris_swapnwbrlz(w);
}

static inline unsigned long __ffs(unsigned long word)
{
	return cris_swapnwbrlz(~word);
}


static inline unsigned long kernel_ffs(unsigned long w)
{
	return w ? cris_swapwbrlz (w) + 1 : 0;
}

#endif
