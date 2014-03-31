
/*
 * Copyright (c) 1996-1999 Distributed Processing Technology Corporation
 * All rights reserved.
 *
 * Redistribution and use in source form, with or without modification, are
 * permitted provided that redistributions of source code must retain the
 * above copyright notice, this list of conditions and the following disclaimer.
 *
 * This software is provided `as is' by Distributed Processing Technology and
 * any express or implied warranties, including, but not limited to, the
 * implied warranties of merchantability and fitness for a particular purpose,
 * are disclaimed. In no event shall Distributed Processing Technology be
 * liable for any direct, indirect, incidental, special, exemplary or
 * consequential damages (including, but not limited to, procurement of
 * substitute goods or services; loss of use, data, or profits; or business
 * interruptions) however caused and on any theory of liability, whether in
 * contract, strict liability, or tort (including negligence or otherwise)
 * arising in any way out of the use of this driver software, even if advised
 * of the possibility of such damage.
 *
 */

#ifndef __DPTSIG_H_
#define __DPTSIG_H_
#ifdef _SINIX_ADDON
#include "dpt.h"
#endif

typedef unsigned char sigBYTE;
typedef unsigned short sigWORD;
typedef unsigned int sigINT;

#if (defined(_DPT_BIG_ENDIAN))
# define sigWORDLittleEndian(x) ((((x)&0xFF)<<8)|(((x)>>8)&0xFF))
# define sigLONGLittleEndian(x) \
        ((((x)&0xFF)<<24) |             \
         (((x)&0xFF00)<<8) |    \
         (((x)&0xFF0000L)>>8) | \
         (((x)&0xFF000000L)>>24))
#else
# define sigWORDLittleEndian(x) (x)
# define sigLONGLittleEndian(x) (x)
#endif


#ifndef NO_PACK
#if defined (_DPT_AIX)
#pragma options align=packed
#else
#pragma pack(1)
#endif  
#endif
#ifdef STRUCTALIGNMENTSUPPORTED
#pragma options align=mac68k
#endif


#define SIG_VERSION 1

#define PROC_INTEL      0x00    
#define PROC_MOTOROLA   0x01    
#define PROC_MIPS4000   0x02    
#define PROC_ALPHA      0x03    
#define PROC_POWERPC    0x04    
#define PROC_i960       0x05    
#define PROC_ULTRASPARC 0x06    


#define PROC_8086       0x01    
#define PROC_286        0x02    
#define PROC_386        0x04    
#define PROC_486        0x08    
#define PROC_PENTIUM    0x10    
#define PROC_SEXIUM	0x20	
#define PROC_IA64	0x40	

#define PROC_960RX      0x01    
#define PROC_960HX      0x02    

#define PROC_68000      0x01    
#define PROC_68010	0x02	
#define PROC_68020      0x04    
#define PROC_68030      0x08    
#define PROC_68040      0x10    

#define PROC_PPC601		0x01	
#define PROC_PPC603		0x02	
#define PROC_PPC604		0x04	

#define PROC_R4000      0x01    

#define FT_EXECUTABLE   0       
#define FT_SCRIPT       1       
#define FT_HBADRVR      2       
#define FT_OTHERDRVR    3       
#define FT_IFS          4       
#define FT_ENGINE       5       
#define FT_COMPDRVR     6       
#define FT_LANGUAGE     7       
#define FT_FIRMWARE     8       
#define FT_COMMMODL     9       
#define FT_INT13        10      
#define FT_HELPFILE     11      
#define FT_LOGGER       12      
#define FT_INSTALL      13      
#define FT_LIBRARY      14      
#define FT_RESOURCE	15	
#define FT_MODEM_DB	16	

#define FTF_DLL         0x01    
#define FTF_NLM         0x02    
#define FTF_OVERLAYS    0x04    
#define FTF_DEBUG       0x08    
#define FTF_TSR         0x10    
#define FTF_SYS         0x20    
#define FTF_PROTECTED   0x40    
#define FTF_APP_SPEC    0x80    
#define FTF_ROM		(FTF_SYS|FTF_TSR)	

#define OEM_DPT         0       
#define OEM_ATT         1       
#define OEM_NEC         2       
#define OEM_ALPHA       3       
#define OEM_AST         4       
#define OEM_OLIVETTI    5       
#define OEM_SNI         6       
#define OEM_SUN         7       

#define OS_DOS          0x00000001 
#define OS_WINDOWS      0x00000002 
#define OS_WINDOWS_NT   0x00000004 
#define OS_OS2M         0x00000008 
#define OS_OS2L         0x00000010 
#define OS_OS22x        0x00000020 
#define OS_NW286        0x00000040 
#define OS_NW386        0x00000080 
#define OS_GEN_UNIX     0x00000100 
#define OS_SCO_UNIX     0x00000200 
#define OS_ATT_UNIX     0x00000400 
#define OS_UNIXWARE     0x00000800 
#define OS_INT_UNIX     0x00001000 
#define OS_SOLARIS      0x00002000 
#define OS_QNX          0x00004000 
#define OS_NEXTSTEP     0x00008000 
#define OS_BANYAN       0x00010000 
#define OS_OLIVETTI_UNIX 0x00020000
#define OS_MAC_OS	0x00040000 
#define OS_WINDOWS_95	0x00080000 
#define OS_NW4x		0x00100000 
#define OS_BSDI_UNIX	0x00200000 
#define OS_AIX_UNIX     0x00400000 
#define OS_FREE_BSD	0x00800000 
#define OS_LINUX	0x01000000 
#define OS_DGUX_UNIX	0x02000000 
#define OS_SINIX_N      0x04000000 
#define OS_PLAN9	0x08000000 
#define OS_TSX		0x10000000 

#define OS_OTHER        0x80000000 

#define CAP_RAID0       0x0001  
#define CAP_RAID1       0x0002  
#define CAP_RAID3       0x0004  
#define CAP_RAID5       0x0008  
#define CAP_SPAN        0x0010  
#define CAP_PASS        0x0020  
#define CAP_OVERLAP     0x0040  
#define CAP_ASPI        0x0080  
#define CAP_ABOVE16MB   0x0100  
#define CAP_EXTEND      0x8000  
#ifdef SNI_MIPS
#define CAP_CACHEMODE   0x1000  
#endif

#define DEV_DASD        0x0001  
#define DEV_TAPE        0x0002  
#define DEV_PRINTER     0x0004  
#define DEV_PROC        0x0008  
#define DEV_WORM        0x0010  
#define DEV_CDROM       0x0020  
#define DEV_SCANNER     0x0040  
#define DEV_OPTICAL     0x0080  
#define DEV_JUKEBOX     0x0100  
#define DEV_COMM        0x0200  
#define DEV_OTHER       0x0400  
#define DEV_ALL         0xFFFF  

#define ADF_2001        0x0001  
#define ADF_2012A       0x0002  
#define ADF_PLUS_ISA    0x0004  
#define ADF_PLUS_EISA   0x0008  
#define ADF_SC3_ISA	0x0010  
#define ADF_SC3_EISA	0x0020  
#define ADF_SC3_PCI	0x0040  
#define ADF_SC4_ISA	0x0080  
#define ADF_SC4_EISA	0x0100  
#define ADF_SC4_PCI	0x0200	
#define ADF_SC5_PCI	0x0400	
#define ADF_ALL_2000	(ADF_2001|ADF_2012A)
#define ADF_ALL_PLUS	(ADF_PLUS_ISA|ADF_PLUS_EISA)
#define ADF_ALL_SC3	(ADF_SC3_ISA|ADF_SC3_EISA|ADF_SC3_PCI)
#define ADF_ALL_SC4	(ADF_SC4_ISA|ADF_SC4_EISA|ADF_SC4_PCI)
#define ADF_ALL_SC5	(ADF_SC5_PCI)
#define ADF_ALL_CACHE	(ADF_ALL_PLUS|ADF_ALL_SC3|ADF_ALL_SC4)
#define ADF_ALL_MASTER	(ADF_2012A|ADF_ALL_CACHE)
#define ADF_ALL_EATA	(ADF_2001|ADF_ALL_MASTER)
#define ADF_ALL		ADF_ALL_EATA

#define APP_DPTMGR      0x0001  
#define APP_ENGINE      0x0002  
#define APP_SYTOS       0x0004  
#define APP_CHEYENNE    0x0008  
#define APP_MSCDEX      0x0010  
#define APP_NOVABACK    0x0020  
#define APP_AIM         0x0040  

#define REQ_SMARTROM    0x01    
#define REQ_DPTDDL      0x02    
#define REQ_HBA_DRIVER  0x04    
#define REQ_ASPI_TRAN   0x08    
#define REQ_ENGINE      0x10    
#define REQ_COMM_ENG    0x20    

#if (!defined(dsDescription_size))
# define dsDescription_size 50
#endif

typedef struct dpt_sig {
    char    dsSignature[6];      
    sigBYTE dsSigVersion;        
    sigBYTE dsProcessorFamily;   
    sigBYTE dsProcessor;         
    sigBYTE dsFiletype;          
    sigBYTE dsFiletypeFlags;     
    sigBYTE dsOEM;               
    sigINT  dsOS;                
    sigWORD dsCapabilities;      
    sigWORD dsDeviceSupp;        
    sigWORD dsAdapterSupp;       
    sigWORD dsApplication;       
    sigBYTE dsRequirements;      
    sigBYTE dsVersion;           
    sigBYTE dsRevision;          
    sigBYTE dsSubRevision;       
    sigBYTE dsMonth;             
    sigBYTE dsDay;               
    sigBYTE dsYear;              
    
    char  dsDescription[dsDescription_size];
} dpt_sig_S;


#ifndef NO_UNPACK
#if defined (_DPT_AIX)
#pragma options align=reset
#elif defined (UNPACK_FOUR)
#pragma pack(4)
#else
#pragma pack()
#endif  
#endif
#ifdef STRUCTALIGNMENTSUPPORTED
#pragma options align=reset
#endif

#endif
