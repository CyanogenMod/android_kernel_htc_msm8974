#ifndef _SPARC_MEMREG_H
#define _SPARC_MEMREG_H
/* memreg.h:  Definitions of the values found in the synchronous
 *            and asynchronous memory error registers when a fault
 *            occurs on the sun4c.
 *
 * Copyright (C) 1995 David S. Miller (davem@caip.rutgers.edu)
 */


#define SUN4C_SYNC_WDRESET   0x0001  
#define SUN4C_SYNC_SIZE      0x0002  
#define SUN4C_SYNC_PARITY    0x0008  
#define SUN4C_SYNC_SBUS      0x0010  
#define SUN4C_SYNC_NOMEM     0x0020  
#define SUN4C_SYNC_PROT      0x0040  
#define SUN4C_SYNC_NPRESENT  0x0080  
#define SUN4C_SYNC_BADWRITE  0x8000  

#define SUN4C_SYNC_BOLIXED  \
        (SUN4C_SYNC_WDRESET | SUN4C_SYNC_SIZE | SUN4C_SYNC_SBUS | \
         SUN4C_SYNC_NOMEM | SUN4C_SYNC_PARITY)


#define SUN4C_ASYNC_BADDVMA 0x0010  
#define SUN4C_ASYNC_NOMEM   0x0020  
#define SUN4C_ASYNC_BADWB   0x0080  

#ifndef __ASSEMBLY__
extern __volatile__ unsigned long __iomem *sun4c_memerr_reg;
#endif

#define	SUN4C_MPE_ERROR	0x80	
#define	SUN4C_MPE_MULTI	0x40	
#define	SUN4C_MPE_TEST	0x20	
#define	SUN4C_MPE_CHECK	0x10	
#define	SUN4C_MPE_ERR00	0x08	
#define	SUN4C_MPE_ERR08	0x04	
#define	SUN4C_MPE_ERR16	0x02	
#define	SUN4C_MPE_ERR24	0x01	
#define	SUN4C_MPE_ERRS	0x0F	

#endif 
