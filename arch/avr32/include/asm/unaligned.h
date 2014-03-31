#ifndef _ASM_AVR32_UNALIGNED_H
#define _ASM_AVR32_UNALIGNED_H


#include <linux/unaligned/be_struct.h>
#include <linux/unaligned/le_byteshift.h>
#include <linux/unaligned/generic.h>

#define get_unaligned	__get_unaligned_be
#define put_unaligned	__put_unaligned_be

#endif 
