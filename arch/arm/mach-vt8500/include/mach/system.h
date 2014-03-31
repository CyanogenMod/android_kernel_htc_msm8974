#include <asm/io.h>

#define VT8500_PMSR_VIRT	0xf8130060

static inline void arch_reset(char mode, const char *cmd)
{
	writel(1, VT8500_PMSR_VIRT);
}
