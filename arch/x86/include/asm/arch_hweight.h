#ifndef _ASM_X86_HWEIGHT_H
#define _ASM_X86_HWEIGHT_H

#ifdef CONFIG_64BIT
#define POPCNT32 ".byte 0xf3,0x40,0x0f,0xb8,0xc7"
#define POPCNT64 ".byte 0xf3,0x48,0x0f,0xb8,0xc7"
#define REG_IN "D"
#define REG_OUT "a"
#else
#define POPCNT32 ".byte 0xf3,0x0f,0xb8,0xc0"
#define REG_IN "a"
#define REG_OUT "a"
#endif

static inline unsigned int __arch_hweight32(unsigned int w)
{
	unsigned int res = 0;

	asm (ALTERNATIVE("call __sw_hweight32", POPCNT32, X86_FEATURE_POPCNT)
		     : "="REG_OUT (res)
		     : REG_IN (w));

	return res;
}

static inline unsigned int __arch_hweight16(unsigned int w)
{
	return __arch_hweight32(w & 0xffff);
}

static inline unsigned int __arch_hweight8(unsigned int w)
{
	return __arch_hweight32(w & 0xff);
}

static inline unsigned long __arch_hweight64(__u64 w)
{
	unsigned long res = 0;

#ifdef CONFIG_X86_32
	return  __arch_hweight32((u32)w) +
		__arch_hweight32((u32)(w >> 32));
#else
	asm (ALTERNATIVE("call __sw_hweight64", POPCNT64, X86_FEATURE_POPCNT)
		     : "="REG_OUT (res)
		     : REG_IN (w));
#endif 

	return res;
}

#endif
