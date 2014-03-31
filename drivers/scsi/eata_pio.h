

#ifndef _EATA_PIO_H
#define _EATA_PIO_H

#define VER_MAJOR 0
#define VER_MINOR 0
#define VER_SUB	  "1b"


#define VERBOSE_SETUP		
#define ALLOW_DMA_BOARDS 1

#define DEBUG_EATA	1	
#define DPT_DEBUG	0	
#define DBG_DELAY	0	
#define DBG_PROBE	0	
#define DBG_ISA		0	
#define DBG_EISA	0	
#define DBG_PCI		0	
#define DBG_PIO		0	
#define DBG_COM		0	
#define DBG_QUEUE	0	
#define DBG_INTR	0	
#define DBG_INTR2	0	
#define DBG_PROC	0	
#define DBG_PROC_WRITE	0
#define DBG_REGISTER	0	
#define DBG_ABNORM	1	

#if DEBUG_EATA
#define DBG(x, y)   if ((x)) {y;}
#else
#define DBG(x, y)
#endif

#endif				
