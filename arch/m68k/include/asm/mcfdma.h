
/*
 *	mcfdma.h -- Coldfire internal DMA support defines.
 *
 *	(C) Copyright 1999, Rob Scott (rscott@mtrob.ml.org)
 */

#ifndef	mcfdma_h
#define	mcfdma_h

#if !defined(CONFIG_M5272)

#define	MCFDMA_SAR		0x00		
#define	MCFDMA_DAR		0x01		
#define	MCFDMA_DCR		0x04		
#define	MCFDMA_BCR		0x06		
#define	MCFDMA_DSR		0x10		
#define	MCFDMA_DIVR		0x14		

#define	MCFDMA_DCR_INT	        0x8000		
#define	MCFDMA_DCR_EEXT	        0x4000		
#define	MCFDMA_DCR_CS 	        0x2000		
#define	MCFDMA_DCR_AA   	0x1000		
#define	MCFDMA_DCR_BWC_MASK  	0x0E00		
#define MCFDMA_DCR_BWC_512      0x0200          
#define MCFDMA_DCR_BWC_1024     0x0400          
#define MCFDMA_DCR_BWC_2048     0x0600          
#define MCFDMA_DCR_BWC_4096     0x0800          
#define MCFDMA_DCR_BWC_8192     0x0a00          
#define MCFDMA_DCR_BWC_16384    0x0c00          
#define MCFDMA_DCR_BWC_32768    0x0e00          
#define	MCFDMA_DCR_SAA         	0x0100		
#define	MCFDMA_DCR_S_RW        	0x0080		
#define	MCFDMA_DCR_SINC        	0x0040		
#define	MCFDMA_DCR_SSIZE_MASK  	0x0030		
#define	MCFDMA_DCR_SSIZE_LONG  	0x0000		
#define	MCFDMA_DCR_SSIZE_BYTE  	0x0010		
#define	MCFDMA_DCR_SSIZE_WORD  	0x0020		
#define	MCFDMA_DCR_SSIZE_LINE  	0x0030		
#define	MCFDMA_DCR_DINC        	0x0008		
#define	MCFDMA_DCR_DSIZE_MASK  	0x0006		
#define	MCFDMA_DCR_DSIZE_LONG  	0x0000		
#define	MCFDMA_DCR_DSIZE_BYTE  	0x0002		
#define	MCFDMA_DCR_DSIZE_WORD  	0x0004		
#define	MCFDMA_DCR_DSIZE_LINE  	0x0006		
#define	MCFDMA_DCR_START       	0x0001		

#define	MCFDMA_DSR_CE	        0x40		
#define	MCFDMA_DSR_BES	        0x20		
#define	MCFDMA_DSR_BED 	        0x10		
#define	MCFDMA_DSR_REQ   	0x04		
#define	MCFDMA_DSR_BSY  	0x02		
#define	MCFDMA_DSR_DONE        	0x01		

#else 

#define MCFDMA_DMR        0x00    
#define MCFDMA_DIR        0x03    
#define MCFDMA_DSAR       0x03    
#define MCFDMA_DDAR       0x04    
#define MCFDMA_DBCR       0x02    

#define MCFDMA_DMR_RESET     0x80000000L 
#define MCFDMA_DMR_EN        0x40000000L 
#define MCFDMA_DMR_RQM       0x000C0000L 
#define MCFDMA_DMR_RQM_DUAL  0x000C0000L 
#define MCFDMA_DMR_DSTM      0x00002000L 
#define MCFDMA_DMR_DSTM_SA   0x00000000L 
#define MCFDMA_DMR_DSTM_IA   0x00002000L 
#define MCFDMA_DMR_DSTT_UD   0x00000400L 
#define MCFDMA_DMR_DSTT_UC   0x00000800L 
#define MCFDMA_DMR_DSTT_SD   0x00001400L 
#define MCFDMA_DMR_DSTT_SC   0x00001800L 
#define MCFDMA_DMR_DSTS_OFF  0x8         
#define MCFDMA_DMR_DSTS_LONG 0x00000000L 
#define MCFDMA_DMR_DSTS_BYTE 0x00000100L 
#define MCFDMA_DMR_DSTS_WORD 0x00000200L 
#define MCFDMA_DMR_DSTS_LINE 0x00000300L 
#define MCFDMA_DMR_SRCM      0x00000020L 
#define MCFDMA_DMR_SRCM_SA   0x00000000L 
#define MCFDMA_DMR_SRCM_IA   0x00000020L 
#define MCFDMA_DMR_SRCT_UD   0x00000004L 
#define MCFDMA_DMR_SRCT_UC   0x00000008L 
#define MCFDMA_DMR_SRCT_SD   0x00000014L 
#define MCFDMA_DMR_SRCT_SC   0x00000018L 
#define MCFDMA_DMR_SRCS_OFF  0x0         
#define MCFDMA_DMR_SRCS_LONG 0x00000000L 
#define MCFDMA_DMR_SRCS_BYTE 0x00000001L 
#define MCFDMA_DMR_SRCS_WORD 0x00000002L 
#define MCFDMA_DMR_SRCS_LINE 0x00000003L 

#define MCFDMA_DIR_INVEN     0x1000 
#define MCFDMA_DIR_ASCEN     0x0800 
#define MCFDMA_DIR_TEEN      0x0200 
#define MCFDMA_DIR_TCEN      0x0100 
#define MCFDMA_DIR_INV       0x0010 
#define MCFDMA_DIR_ASC       0x0008 
#define MCFDMA_DIR_TE        0x0002 
#define MCFDMA_DIR_TC        0x0001 

#endif  

#endif	
