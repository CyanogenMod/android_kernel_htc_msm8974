/*
 * Copyright (C) 2002-2003 Hewlett-Packard Co
 *               Stephane Eranian <eranian@hpl.hp.com>
 *
 * This file implements the default sampling buffer format
 * for Linux/ia64 perfmon subsystem.
 */
#ifndef __PERFMON_DEFAULT_SMPL_H__
#define __PERFMON_DEFAULT_SMPL_H__ 1

#define PFM_DEFAULT_SMPL_UUID { \
		0x4d, 0x72, 0xbe, 0xc0, 0x06, 0x64, 0x41, 0x43, 0x82, 0xb4, 0xd3, 0xfd, 0x27, 0x24, 0x3c, 0x97}

typedef struct {
	unsigned long buf_size;		
	unsigned int  flags;		
	unsigned int  res1;		
	unsigned long reserved[2];	
} pfm_default_smpl_arg_t;

typedef struct {
	pfarg_context_t		ctx_arg;
	pfm_default_smpl_arg_t	buf_arg;
} pfm_default_smpl_ctx_arg_t;

typedef struct {
	unsigned long	hdr_count;		
	unsigned long	hdr_cur_offs;		
	unsigned long	hdr_reserved2;		

	unsigned long	hdr_overflows;		
	unsigned long   hdr_buf_size;		

	unsigned int	hdr_version;		
	unsigned int	hdr_reserved1;		
	unsigned long	hdr_reserved[10];	
} pfm_default_smpl_hdr_t;

/*
 * Entry header in the sampling buffer.  The header is directly followed
 * with the values of the PMD registers of interest saved in increasing 
 * index order: PMD4, PMD5, and so on. How many PMDs are present depends 
 * on how the session was programmed.
 *
 * In the case where multiple counters overflow at the same time, multiple
 * entries are written consecutively.
 *
 * last_reset_value member indicates the initial value of the overflowed PMD. 
 */
typedef struct {
        int             pid;                    
        unsigned char   reserved1[3];           
        unsigned char   ovfl_pmd;               

        unsigned long   last_reset_val;         
        unsigned long   ip;                     
        unsigned long   tstamp;                 

        unsigned short  cpu;                    
        unsigned short  set;                    
        int    		tgid;              	
} pfm_default_smpl_entry_t;

#define PFM_DEFAULT_MAX_PMDS		64 
#define PFM_DEFAULT_MAX_ENTRY_SIZE	(sizeof(pfm_default_smpl_entry_t)+(sizeof(unsigned long)*PFM_DEFAULT_MAX_PMDS))
#define PFM_DEFAULT_SMPL_MIN_BUF_SIZE	(sizeof(pfm_default_smpl_hdr_t)+PFM_DEFAULT_MAX_ENTRY_SIZE)

#define PFM_DEFAULT_SMPL_VERSION_MAJ	2U
#define PFM_DEFAULT_SMPL_VERSION_MIN	0U
#define PFM_DEFAULT_SMPL_VERSION	(((PFM_DEFAULT_SMPL_VERSION_MAJ&0xffff)<<16)|(PFM_DEFAULT_SMPL_VERSION_MIN & 0xffff))

#endif 
