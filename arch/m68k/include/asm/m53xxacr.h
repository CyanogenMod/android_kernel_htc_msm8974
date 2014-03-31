
/*
 * m53xxacr.h -- ColdFire version 3 core cache support
 *
 * (C) Copyright 2010, Greg Ungerer <gerg@snapgear.com>
 */

#ifndef m53xxacr_h
#define m53xxacr_h


#define CACR_EC		0x80000000	
#define CACR_ESB	0x20000000	
#define CACR_DPI	0x10000000	
#define CACR_HLCK	0x08000000	
#define CACR_CINVA	0x01000000	
#define CACR_DNFB	0x00000400	
#define CACR_DCM_WT	0x00000000	
#define CACR_DCM_CB	0x00000100	
#define CACR_DCM_PRE	0x00000200	
#define CACR_DCM_IMPRE	0x00000300	
#define CACR_WPROTECT	0x00000020	
#define CACR_EUSP	0x00000010	

#define ACR_BASE_POS	24		
#define ACR_MASK_POS	16		
#define ACR_ENABLE	0x00008000	
#define ACR_USER	0x00000000	
#define ACR_SUPER	0x00002000	
#define ACR_ANY		0x00004000	
#define ACR_CM_WT	0x00000000	
#define ACR_CM_CB	0x00000020	
#define ACR_CM_PRE	0x00000040	
#define ACR_CM_IMPRE	0x00000060	
#define ACR_WPROTECT	0x00000004	

#if defined(CONFIG_M5307)
#define	CACHE_SIZE	0x2000		
#define	ICACHE_SIZE	CACHE_SIZE
#define	DCACHE_SIZE	CACHE_SIZE
#elif defined(CONFIG_M532x)
#define	CACHE_SIZE	0x4000		
#define	ICACHE_SIZE	CACHE_SIZE
#define	DCACHE_SIZE	CACHE_SIZE
#endif

#define	CACHE_LINE_SIZE	16		
#define	CACHE_WAYS	4		

#if defined(CONFIG_CACHE_COPYBACK)
#define CACHE_TYPE	ACR_CM_CB
#define CACHE_PUSH
#else
#define CACHE_TYPE	ACR_CM_WT
#endif

#ifdef CONFIG_COLDFIRE_SW_A7
#define CACHE_MODE	(CACR_EC + CACR_ESB + CACR_DCM_PRE)
#else
#define CACHE_MODE	(CACR_EC + CACR_ESB + CACR_DCM_PRE + CACR_EUSP)
#endif

#define CACHE_INIT	  CACR_CINVA
#define CACHE_INVALIDATE  CACR_CINVA
#define CACHE_INVALIDATED CACR_CINVA

#define ACR0_MODE	((CONFIG_RAMBASE & 0xff000000) + \
			 (0x000f0000) + \
			 (ACR_ENABLE + ACR_ANY + CACHE_TYPE))
#define ACR1_MODE	0

#endif  
