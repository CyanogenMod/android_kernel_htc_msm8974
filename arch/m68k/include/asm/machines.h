/*
 * machines.h:  Defines for taking apart the machine type value in the
 *              idprom and determining the kind of machine we are on.
 *
 * Copyright (C) 1995 David S. Miller (davem@caip.rutgers.edu)
 * Sun3/3x models added by David Monro (davidm@psrg.cs.usyd.edu.au)
 */
#ifndef _SPARC_MACHINES_H
#define _SPARC_MACHINES_H

struct Sun_Machine_Models {
	char *name;
	unsigned char id_machtype;
};

#define NUM_SUN_MACHINES  8


#define SM_ARCH_MASK  0xf0
#define SM_SUN3       0x10
#define SM_SUN4       0x20
#define SM_SUN3X      0x40
#define SM_SUN4C      0x50
#define SM_SUN4M      0x70
#define SM_SUN4M_OBP  0x80

#define SM_TYP_MASK   0x0f
#define SM_3_160      0x01    
#define SM_3_50       0x02    
#define SM_3_260      0x03    
#define SM_3_110      0x04    
#define SM_3_60       0x07    
#define SM_3_E        0x08    

#define SM_3_460      0x01    
#define SM_3_80       0x02    

#define SM_4_260      0x01    
#define SM_4_110      0x02    
#define SM_4_330      0x03    
#define SM_4_470      0x04    

#define SM_4C_SS1     0x01    
#define SM_4C_IPC     0x02    
#define SM_4C_SS1PLUS 0x03    
#define SM_4C_SLC     0x04    
#define SM_4C_SS2     0x05    
#define SM_4C_ELC     0x06    
#define SM_4C_IPX     0x07    

#define SM_4M_SS60    0x01    
#define SM_4M_SS50    0x02    
#define SM_4M_SS40    0x03    


#endif 
