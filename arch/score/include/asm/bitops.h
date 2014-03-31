#ifndef _ASM_SCORE_BITOPS_H
#define _ASM_SCORE_BITOPS_H

#include <asm/byteorder.h> 

#define smp_mb__before_clear_bit()	barrier()
#define smp_mb__after_clear_bit()	barrier()

#include <asm-generic/bitops.h>
#include <asm-generic/bitops/__fls.h>

#endif 
