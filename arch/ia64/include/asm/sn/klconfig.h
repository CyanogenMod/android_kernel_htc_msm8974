/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Derived from IRIX <sys/SN/klconfig.h>.
 *
 * Copyright (C) 1992-1997,1999,2001-2004 Silicon Graphics, Inc.  All Rights Reserved.
 * Copyright (C) 1999 by Ralf Baechle
 */
#ifndef _ASM_IA64_SN_KLCONFIG_H
#define _ASM_IA64_SN_KLCONFIG_H


typedef s32 klconf_off_t;



typedef struct kl_config_hdr {
	char		pad[20];
	klconf_off_t	ch_board_info;	
	char		pad0[88];
} kl_config_hdr_t;


#define NODE_OFFSET_TO_LBOARD(nasid,off)        (lboard_t*)(GLOBAL_CAC_ADDR((nasid), (off)))




#define KLCLASS_MASK	0xf0   
#define KLCLASS_NONE	0x00
#define KLCLASS_NODE	0x10             
#define KLCLASS_CPU	KLCLASS_NODE	
#define KLCLASS_IO	0x20             
#define KLCLASS_ROUTER	0x30             
#define KLCLASS_MIDPLANE 0x40            
#define KLCLASS_IOBRICK	0x70		
#define KLCLASS_MAX	8		

#define KLCLASS(_x) ((_x) & KLCLASS_MASK)



#define KLTYPE_MASK	0x0f
#define KLTYPE(_x)      ((_x) & KLTYPE_MASK)

#define KLTYPE_SNIA	(KLCLASS_CPU | 0x1)
#define KLTYPE_TIO	(KLCLASS_CPU | 0x2)

#define KLTYPE_ROUTER     (KLCLASS_ROUTER | 0x1)
#define KLTYPE_META_ROUTER (KLCLASS_ROUTER | 0x3)
#define KLTYPE_REPEATER_ROUTER (KLCLASS_ROUTER | 0x4)

#define KLTYPE_IOBRICK_XBOW	(KLCLASS_MIDPLANE | 0x2)

#define KLTYPE_IOBRICK		(KLCLASS_IOBRICK | 0x0)
#define KLTYPE_NBRICK		(KLCLASS_IOBRICK | 0x4)
#define KLTYPE_PXBRICK		(KLCLASS_IOBRICK | 0x6)
#define KLTYPE_IXBRICK		(KLCLASS_IOBRICK | 0x7)
#define KLTYPE_CGBRICK		(KLCLASS_IOBRICK | 0x8)
#define KLTYPE_OPUSBRICK	(KLCLASS_IOBRICK | 0x9)
#define KLTYPE_SABRICK          (KLCLASS_IOBRICK | 0xa)
#define KLTYPE_IABRICK		(KLCLASS_IOBRICK | 0xb)
#define KLTYPE_PABRICK          (KLCLASS_IOBRICK | 0xc)
#define KLTYPE_GABRICK		(KLCLASS_IOBRICK | 0xd)



#define MAX_COMPTS_PER_BRD 24

typedef struct lboard_s {
	klconf_off_t 	brd_next_any;     
	unsigned char 	struct_type;      
	unsigned char 	brd_type;         
	unsigned char 	brd_sversion;     
        unsigned char 	brd_brevision;    
        unsigned char 	brd_promver;      
 	unsigned char 	brd_flags;        
	unsigned char 	brd_slot;         
	unsigned short	brd_debugsw;      
	geoid_t		brd_geoid;	  
	partid_t 	brd_partition;    
        unsigned short 	brd_diagval;      
        unsigned short 	brd_diagparm;     
        unsigned char 	brd_inventory;    
        unsigned char 	brd_numcompts;    
        nic_t         	brd_nic;          
	nasid_t		brd_nasid;        
	klconf_off_t 	brd_compts[MAX_COMPTS_PER_BRD]; 
	klconf_off_t 	brd_errinfo;      
	struct lboard_s *brd_parent;	  
	char            pad0[4];
	unsigned char	brd_confidence;	  
	nasid_t		brd_owner;        
	unsigned char 	brd_nic_flags;    
	char		pad1[24];	  
	char		brd_name[32];
	nasid_t		brd_next_same_host; 
	klconf_off_t	brd_next_same;    
} lboard_t;

 
typedef struct klinfo_s {                  
        unsigned char   struct_type;       
        unsigned char   struct_version;    
        unsigned char   flags;            
        unsigned char   revision;         
        unsigned short  diagval;          
        unsigned short  diagparm;         
        unsigned char   inventory;        
        unsigned short  partid;		   
	nic_t 		nic;              
        unsigned char   physid;           
        unsigned int    virtid;           
	unsigned char	widid;	          
	nasid_t		nasid;            
	char		pad1;		  
	char		pad2;		  
	void		*data;
        klconf_off_t	errinfo;          
        unsigned short  pad3;             
        unsigned short  pad4;             
} klinfo_t ;


static inline lboard_t *find_lboard_next(lboard_t * brd)
{
	if (brd && brd->brd_next_any)
		return NODE_OFFSET_TO_LBOARD(NASID_GET(brd), brd->brd_next_any);
        return NULL;
}

#endif 
