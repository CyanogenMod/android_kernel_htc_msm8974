/*
 * Orb related data structures.
 *
 * Copyright IBM Corp. 2007, 2011
 *
 * Author(s): Cornelia Huck <cornelia.huck@de.ibm.com>
 *	      Peter Oberparleiter <peter.oberparleiter@de.ibm.com>
 *	      Sebastian Ott <sebott@linux.vnet.ibm.com>
 */

#ifndef S390_ORB_H
#define S390_ORB_H

struct cmd_orb {
	u32 intparm;	
	u32 key:4;	
	u32 spnd:1;	
	u32 res1:1;	
	u32 mod:1;	
	u32 sync:1;	
	u32 fmt:1;	
	u32 pfch:1;	
	u32 isic:1;	
	u32 alcc:1;	
	u32 ssic:1;	
	u32 res2:1;	
	u32 c64:1;	
	u32 i2k:1;	
	u32 lpm:8;	
	u32 ils:1;	
	u32 zero:6;	
	u32 orbx:1;	
	u32 cpa;	
}  __packed __aligned(4);

struct tm_orb {
	u32 intparm;
	u32 key:4;
	u32:9;
	u32 b:1;
	u32:2;
	u32 lpm:8;
	u32:7;
	u32 x:1;
	u32 tcw;
	u32 prio:8;
	u32:8;
	u32 rsvpgm:8;
	u32:8;
	u32:32;
	u32:32;
	u32:32;
	u32:32;
}  __packed __aligned(4);

union orb {
	struct cmd_orb cmd;
	struct tm_orb tm;
}  __packed __aligned(4);

#endif 
