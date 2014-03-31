#ifndef _ASM_DMI_H
#define _ASM_DMI_H 1

#include <linux/slab.h>
#include <asm/io.h>

#define dmi_ioremap ioremap
#define dmi_iounmap(x,l) iounmap(x)
#define dmi_alloc(l) kmalloc(l, GFP_ATOMIC)

#endif
