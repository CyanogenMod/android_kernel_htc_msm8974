
#ifndef __HARDWARE_H
#define __HARDWARE_H

#include <asm/types.h>


#ifndef __ASSEMBLY__
void iop33x_init_irq(void);

extern struct platform_device iop33x_uart0_device;
extern struct platform_device iop33x_uart1_device;
#endif


#include "iop33x.h"

#include "iq80331.h"
#include "iq80332.h"


#endif
