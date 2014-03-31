#ifndef ASMARM_SPARSEMEM_H
#define ASMARM_SPARSEMEM_H

#include <asm/memory.h>

#if !defined(SECTION_SIZE_BITS) || !defined(MAX_PHYSMEM_BITS)
#error Sparsemem is not supported on this platform
#endif

#endif
