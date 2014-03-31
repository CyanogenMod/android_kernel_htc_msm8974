/*
 * Linux driver for HYSDN cards
 * common definitions for both sides of the bus:
 * - conventions both spoolers must know
 * - channel numbers agreed upon
 *
 * Author    M. Steinkopf
 * Copyright 1999 by M. Steinkopf
 *
 * This software may be used and distributed according to the terms
 * of the GNU General Public License, incorporated herein by reference.
 *
 */

#ifndef __INCE1PC_H__
#define __INCE1PC_H__


#define CHAN_SYSTEM     0x0001      
#define CHAN_ERRLOG     0x0005      
#define CHAN_CAPI       0x0064      
#define CHAN_NDIS_DATA  0x1001      

#define RDY_MAGIC       0x52535953UL    
#define RDY_MAGIC_SIZE  4               

#define MAX_N_TOK_BYTES 255

#define MIN_RDY_MSG_SIZE    RDY_MAGIC_SIZE
#define MAX_RDY_MSG_SIZE    (RDY_MAGIC_SIZE + MAX_N_TOK_BYTES)

#define SYSR_TOK_END            0
#define SYSR_TOK_B_CHAN         1   
#define SYSR_TOK_FAX_CHAN       2   
#define SYSR_TOK_MAC_ADDR       3   
#define SYSR_TOK_ESC            255 
#define SYSR_TOK_B_CHAN_DEF     2   
#define SYSR_TOK_FAX_CHAN_DEF   1   


#define ERRLOG_CMD_REQ          "ERRLOG ON"
#define ERRLOG_CMD_REQ_SIZE     10              
#define ERRLOG_CMD_STOP         "ERRLOG OFF"
#define ERRLOG_CMD_STOP_SIZE    11              

#define ERRLOG_ENTRY_SIZE       64      
					
#define ERRLOG_TEXT_SIZE    (ERRLOG_ENTRY_SIZE - 2 * 4 - 1)

typedef struct ErrLogEntry_tag {

	 unsigned long ulErrType;

	 unsigned long ulErrSubtype;

	 unsigned char ucTextSize;

	 unsigned char ucText[ERRLOG_TEXT_SIZE];
	

} tErrLogEntry;


#if defined(__TURBOC__)
#if sizeof(tErrLogEntry) != ERRLOG_ENTRY_SIZE
#error size of tErrLogEntry != ERRLOG_ENTRY_SIZE
#endif				
#endif				

#define DPRAM_SPOOLER_DATA_SIZE 0x20
typedef struct DpramBootSpooler_tag {

	 unsigned char Len;

	 volatile unsigned char RdPtr;

	 unsigned char WrPtr;

	 unsigned char Data[DPRAM_SPOOLER_DATA_SIZE];

} tDpramBootSpooler;


#define DPRAM_SPOOLER_MIN_SIZE  5       
#define DPRAM_SPOOLER_DEF_SIZE  0x23    

#define SIZE_RSV_SOFT_UART  0x1B0   


#endif	
