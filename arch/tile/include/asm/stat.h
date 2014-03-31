#if defined(__KERNEL__) && defined(CONFIG_COMPAT)
#define __ARCH_WANT_STAT64	
#endif
#include <asm-generic/stat.h>
