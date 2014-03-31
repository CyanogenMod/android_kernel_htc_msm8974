/*
 * dz.h: Serial port driver for DECstations equipped
 *       with the DZ chipset.
 *
 * Copyright (C) 1998 Olivier A. D. Lebaillif 
 *             
 * Email: olivier.lebaillif@ifrsys.com
 *
 * Copyright (C) 2004, 2006  Maciej W. Rozycki
 */
#ifndef DZ_SERIAL_H
#define DZ_SERIAL_H

#define DZ_TRDY        0x8000                 
#define DZ_TIE         0x4000                 
#define DZ_TLINE       0x0300                 
#define DZ_RDONE       0x0080                 
#define DZ_RIE         0x0040                 
#define DZ_MSE         0x0020                 
#define DZ_CLR         0x0010                 
#define DZ_MAINT       0x0008                 

#define DZ_RBUF_MASK   0x00FF                 
#define DZ_LINE_MASK   0x0300                 
#define DZ_DVAL        0x8000                 
#define DZ_OERR        0x4000                 
#define DZ_FERR        0x2000                 
#define DZ_PERR        0x1000                 

#define DZ_BREAK       0x0800                 

#define LINE(x) ((x & DZ_LINE_MASK) >> 8)     
#define UCHAR(x) ((unsigned char)(x & DZ_RBUF_MASK))

#define DZ_LINE_KEYBOARD 0x0001
#define DZ_LINE_MOUSE    0x0002
#define DZ_LINE_MODEM    0x0004
#define DZ_LINE_PRINTER  0x0008

#define DZ_MODEM_RTS     0x0800               
#define DZ_MODEM_DTR     0x0400               
#define DZ_PRINT_RTS     0x0200               
#define DZ_PRINT_DTR     0x0100               
#define DZ_LNENB         0x000f               

#define DZ_MODEM_RI      0x0800               
#define DZ_MODEM_CD      0x0400               
#define DZ_MODEM_DSR     0x0200               
#define DZ_MODEM_CTS     0x0100               
#define DZ_PRINT_RI      0x0008               
#define DZ_PRINT_CD      0x0004               
#define DZ_PRINT_DSR     0x0002               
#define DZ_PRINT_CTS     0x0001               

#define DZ_BRK0          0x0100               
#define DZ_BRK1          0x0200               
#define DZ_BRK2          0x0400               
#define DZ_BRK3          0x0800               

#define DZ_KEYBOARD      0x0000               
#define DZ_MOUSE         0x0001               
#define DZ_MODEM         0x0002               
#define DZ_PRINTER       0x0003               

#define DZ_CSIZE         0x0018               
#define DZ_CS5           0x0000               
#define DZ_CS6           0x0008               
#define DZ_CS7           0x0010               
#define DZ_CS8           0x0018               

#define DZ_CSTOPB        0x0020                

#define DZ_PARENB        0x0040               
#define DZ_PARODD        0x0080               

#define DZ_CBAUD         0x0E00               
#define DZ_B50           0x0000
#define DZ_B75           0x0100
#define DZ_B110          0x0200
#define DZ_B134          0x0300
#define DZ_B150          0x0400
#define DZ_B300          0x0500
#define DZ_B600          0x0600
#define DZ_B1200         0x0700 
#define DZ_B1800         0x0800
#define DZ_B2000         0x0900
#define DZ_B2400         0x0A00
#define DZ_B3600         0x0B00
#define DZ_B4800         0x0C00
#define DZ_B7200         0x0D00
#define DZ_B9600         0x0E00

#define DZ_RXENAB        0x1000               

#define DZ_CSR       0x00            
#define DZ_RBUF      0x08            
#define DZ_LPR       0x08            
#define DZ_TCR       0x10            
#define DZ_MSR       0x18            
#define DZ_TDR       0x18            

#define DZ_NB_PORT 4

#define DZ_XMIT_SIZE   4096                 
#define DZ_WAKEUP_CHARS   DZ_XMIT_SIZE/4

#endif 
