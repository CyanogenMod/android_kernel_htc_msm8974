/* $Id: hysdn_pof.h,v 1.2.6.1 2001/09/23 22:24:54 kai Exp $
 *
 * Linux driver for HYSDN cards, definitions used for handling pof-files.
 *
 * Author    Werner Cornelius (werner@titro.de) for Hypercope GmbH
 * Copyright 1999 by Werner Cornelius (werner@titro.de)
 *
 * This software may be used and distributed according to the terms
 * of the GNU General Public License, incorporated herein by reference.
 *
 */

#define BOOT_BUF_SIZE   0x1000	
#define CRYPT_FEEDTERM  0x8142
#define CRYPT_STARTTERM 0x81a5
#define POF_READY_TIME_OUT_SEC  10



#define POF_BOOT_LOADER_PAGE_SIZE   0x4000	
#define POF_BOOT_LOADER_TOTAL_SIZE  (2U * POF_BOOT_LOADER_PAGE_SIZE)

#define POF_BOOT_LOADER_CODE_SIZE   0x0800	

#define POF_BOOT_LOADER_OFF_IN_PAGE (POF_BOOT_LOADER_PAGE_SIZE-POF_BOOT_LOADER_CODE_SIZE)


typedef struct PofFileHdr_tag {	
	 unsigned long Magic __attribute__((packed));
	 unsigned long N_PofRecs __attribute__((packed));
} tPofFileHdr;

typedef struct PofRecHdr_tag {	
	 unsigned short PofRecId __attribute__((packed));
	 unsigned long PofRecDataLen __attribute__((packed));
} tPofRecHdr;

typedef struct PofTimeStamp_tag {
	 unsigned long UnixTime __attribute__((packed));
	 unsigned char DateTimeText[0x28];
	
} tPofTimeStamp;

#define TAGFILEMAGIC 0x464F501AUL
#define TAG_ABSDATA  0x1000	
#define TAG_BOOTDTA  0x1001	
#define TAG_COMMENT  0x0020
#define TAG_SYSCALL  0x0021
#define TAG_FLOWCTRL 0x0022
#define TAG_TIMESTMP 0x0010	
#define TAG_CABSDATA 0x1100	
#define TAG_CBOOTDTA 0x1101	
