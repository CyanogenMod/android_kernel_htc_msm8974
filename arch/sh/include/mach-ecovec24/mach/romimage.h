#ifdef __ASSEMBLY__


#include <asm/romimage-macros.h>
#include "partner-jet-setup.txt"

	
	mov.l	1f, r0
	icbi	@r0

	
	mova	2f, r0
	jmp	@r0
	nop

	.align 2
1 :	.long 0xa8000000
2 :

#else 

#define HIZCRA		0xa4050158
#define PGDR		0xa405012c

static inline void mmcif_update_progress(int nr)
{
	
	__raw_writew(__raw_readw(HIZCRA) & ~(1 << 1), HIZCRA);

	
	__raw_writeb(1 << (nr - 1), PGDR);
}

#endif 
