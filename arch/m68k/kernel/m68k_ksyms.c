#include <linux/module.h>

asmlinkage long long __ashldi3 (long long, int);
asmlinkage long long __ashrdi3 (long long, int);
asmlinkage long long __lshrdi3 (long long, int);
asmlinkage long long __muldi3 (long long, long long);

EXPORT_SYMBOL(__ashldi3);
EXPORT_SYMBOL(__ashrdi3);
EXPORT_SYMBOL(__lshrdi3);
EXPORT_SYMBOL(__muldi3);

#if defined(CONFIG_CPU_HAS_NO_MULDIV64)
extern long long __divsi3(long long, long long);
extern long long __modsi3(long long, long long);
extern long long __mulsi3(long long, long long);
extern long long __udivsi3(long long, long long);
extern long long __umodsi3(long long, long long);

EXPORT_SYMBOL(__divsi3);
EXPORT_SYMBOL(__modsi3);
EXPORT_SYMBOL(__mulsi3);
EXPORT_SYMBOL(__udivsi3);
EXPORT_SYMBOL(__umodsi3);
#endif
