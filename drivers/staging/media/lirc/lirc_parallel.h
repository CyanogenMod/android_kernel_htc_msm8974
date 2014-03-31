
#ifndef _LIRC_PARALLEL_H
#define _LIRC_PARALLEL_H

#include <linux/lp.h>

#define LIRC_PORT_LEN 3

#define LIRC_LP_BASE    0
#define LIRC_LP_STATUS  1
#define LIRC_LP_CONTROL 2

#define LIRC_PORT_DATA           LIRC_LP_BASE    
#define LIRC_PORT_TIMER        LIRC_LP_STATUS    
#define LIRC_PORT_TIMER_BIT          LP_PBUSY    
#define LIRC_PORT_SIGNAL       LIRC_LP_STATUS    
#define LIRC_PORT_SIGNAL_BIT          LP_PACK    
#define LIRC_PORT_IRQ         LIRC_LP_CONTROL    

#define LIRC_SFH506_DELAY 0             

#define LIRC_PARALLEL_MAX_TRANSMITTERS 8
#define LIRC_PARALLEL_TRANSMITTER_MASK ((1<<LIRC_PARALLEL_MAX_TRANSMITTERS) - 1)

#endif
