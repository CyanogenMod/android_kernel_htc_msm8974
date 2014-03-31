#ifndef _ASM_GENERIC_BITOPS_FIND_H_
#define _ASM_GENERIC_BITOPS_FIND_H_

#ifndef find_next_bit
extern unsigned long find_next_bit(const unsigned long *addr, unsigned long
		size, unsigned long offset);
#endif

#ifndef find_next_zero_bit
extern unsigned long find_next_zero_bit(const unsigned long *addr, unsigned
		long size, unsigned long offset);
#endif

#ifdef CONFIG_GENERIC_FIND_FIRST_BIT

extern unsigned long find_first_bit(const unsigned long *addr,
				    unsigned long size);

extern unsigned long find_first_zero_bit(const unsigned long *addr,
					 unsigned long size);
#else 

#define find_first_bit(addr, size) find_next_bit((addr), (size), 0)
#define find_first_zero_bit(addr, size) find_next_zero_bit((addr), (size), 0)

#endif 

#endif 
