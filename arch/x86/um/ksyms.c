#include <linux/module.h>
#include <asm/string.h>
#include <asm/checksum.h>

#ifndef CONFIG_X86_32
#if (__GNUC__ == 4 && __GNUC_MINOR__ >= 3) || __GNUC__ > 4
EXPORT_SYMBOL(memcpy);
#else
EXPORT_SYMBOL(__memcpy);
#endif
#endif
EXPORT_SYMBOL(csum_partial);
