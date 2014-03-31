/*
 * auxio.h:  Definitions and code for the Auxiliary I/O register.
 *
 * Copyright (C) 1995 David S. Miller (davem@caip.rutgers.edu)
 */
#ifndef _SPARC_AUXIO_H
#define _SPARC_AUXIO_H

#include <asm/vaddrs.h>

#define AUXIO_ORMEIN      0xf0    
#define AUXIO_ORMEIN4M    0xc0    
#define AUXIO_FLPY_DENS   0x20    
#define AUXIO_FLPY_DCHG   0x10    
#define AUXIO_EDGE_ON     0x10    
#define AUXIO_FLPY_DSEL   0x08    
#define AUXIO_LINK_TEST   0x08    

#define AUXIO_FLPY_TCNT   0x04    

#define AUXIO_FLPY_EJCT   0x02    
#define AUXIO_LED         0x01    

#ifndef __ASSEMBLY__

extern void set_auxio(unsigned char bits_on, unsigned char bits_off);
extern unsigned char get_auxio(void); 


#define AUXIO_LTE_ON    1
#define AUXIO_LTE_OFF   0

#define auxio_set_lte(on) \
do { \
	if(on) { \
		set_auxio(AUXIO_LINK_TEST, 0); \
	} else { \
		set_auxio(0, AUXIO_LINK_TEST); \
	} \
} while (0)

#define AUXIO_LED_ON    1
#define AUXIO_LED_OFF   0

#define auxio_set_led(on) \
do { \
	if(on) { \
		set_auxio(AUXIO_LED, 0); \
	} else { \
		set_auxio(0, AUXIO_LED); \
	} \
} while (0)

#endif 


extern __volatile__ unsigned char * auxio_power_register;

#define	AUXIO_POWER_DETECT_FAILURE	32
#define	AUXIO_POWER_CLEAR_FAILURE	2
#define	AUXIO_POWER_OFF			1


#endif 
