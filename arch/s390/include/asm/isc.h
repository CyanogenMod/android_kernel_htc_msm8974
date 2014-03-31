#ifndef _ASM_S390_ISC_H
#define _ASM_S390_ISC_H

#include <linux/types.h>

#define MAX_ISC 7

#define IO_SCH_ISC 3			
#define CONSOLE_ISC 1			
#define CHSC_SCH_ISC 7			
#define QDIO_AIRQ_ISC IO_SCH_ISC	
#define AP_ISC 6			

void isc_register(unsigned int isc);
void isc_unregister(unsigned int isc);

#endif 
