/*
*
* Definitions for H3600 Handheld Computer
*
* Copyright 2001 Compaq Computer Corporation.
*
* Use consistent with the GNU GPL is permitted,
* provided that this copyright notice is
* preserved in its entirety in all copies and derived works.
*
* COMPAQ COMPUTER CORPORATION MAKES NO WARRANTIES, EXPRESSED OR IMPLIED,
* AS TO THE USEFULNESS OR CORRECTNESS OF THIS CODE OR ITS
* FITNESS FOR ANY PARTICULAR PURPOSE.
*
* Author: Jamey Hicks.
*
*/


#define LINKUP_PRS_S1	(1 << 0) 
#define LINKUP_PRS_S2	(1 << 1)
#define LINKUP_PRS_S3	(1 << 2)
#define LINKUP_PRS_S4	(1 << 3)
#define LINKUP_PRS_BVD1	(1 << 4)
#define LINKUP_PRS_BVD2	(1 << 5)
#define LINKUP_PRS_VS1	(1 << 6)
#define LINKUP_PRS_VS2	(1 << 7)
#define LINKUP_PRS_RDY	(1 << 8)
#define LINKUP_PRS_CD1	(1 << 9)
#define LINKUP_PRS_CD2	(1 << 10)

#define LINKUP_PRC_S1	(1 << 0)
#define LINKUP_PRC_S2	(1 << 1)
#define LINKUP_PRC_S3	(1 << 2)
#define LINKUP_PRC_S4	(1 << 3)
#define LINKUP_PRC_RESET (1 << 4)
#define LINKUP_PRC_APOE	(1 << 5) 
#define LINKUP_PRC_CFE	(1 << 6) 
#define LINKUP_PRC_SOE	(1 << 7) 
#define LINKUP_PRC_SSP	(1 << 8) 
#define LINKUP_PRC_MBZ	(1 << 15) 

struct linkup_l1110 {
	volatile short prc;
};
