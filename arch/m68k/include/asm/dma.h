#ifndef _M68K_DMA_H
#define _M68K_DMA_H 1

#ifdef CONFIG_COLDFIRE

#include <asm/coldfire.h>
#include <asm/mcfsim.h>
#include <asm/mcfdma.h>

#if defined(CONFIG_M5249) || defined(CONFIG_M5307) || defined(CONFIG_M5407) || \
	defined(CONFIG_M523x) || defined(CONFIG_M527x) || defined(CONFIG_M528x)
#define MAX_M68K_DMA_CHANNELS 4
#elif defined(CONFIG_M5272)
#define MAX_M68K_DMA_CHANNELS 1
#elif defined(CONFIG_M532x)
#define MAX_M68K_DMA_CHANNELS 0
#else
#define MAX_M68K_DMA_CHANNELS 2
#endif

extern unsigned int dma_base_addr[MAX_M68K_DMA_CHANNELS];
extern unsigned int dma_device_address[MAX_M68K_DMA_CHANNELS];

#if !defined(CONFIG_M5272)
#define DMA_MODE_WRITE_BIT  0x01  
#define DMA_MODE_WORD_BIT   0x02  
#define DMA_MODE_LONG_BIT   0x04  
#define DMA_MODE_SINGLE_BIT 0x08  

#define DMA_MODE_READ	            0
#define DMA_MODE_WRITE	            1
#define DMA_MODE_READ_WORD          2
#define DMA_MODE_WRITE_WORD         3
#define DMA_MODE_READ_LONG          4
#define DMA_MODE_WRITE_LONG         5
#define DMA_MODE_READ_SINGLE        8
#define DMA_MODE_WRITE_SINGLE       9
#define DMA_MODE_READ_WORD_SINGLE  10
#define DMA_MODE_WRITE_WORD_SINGLE 11
#define DMA_MODE_READ_LONG_SINGLE  12
#define DMA_MODE_WRITE_LONG_SINGLE 13

#else 

#define DMA_MODE_SRC_SA_BIT 0x01
#define DMA_MODE_SSIZE_MASK 0x06
#define DMA_MODE_SSIZE_OFF  0x01
#define DMA_MODE_DES_SA_BIT 0x10
#define DMA_MODE_DSIZE_MASK 0x60
#define DMA_MODE_DSIZE_OFF  0x05
#define DMA_MODE_SIZE_LONG  0x00
#define DMA_MODE_SIZE_BYTE  0x01
#define DMA_MODE_SIZE_WORD  0x02
#define DMA_MODE_SIZE_LINE  0x03


#define DMA_MODE_READ	              ((DMA_MODE_SIZE_BYTE << DMA_MODE_DSIZE_OFF) | (DMA_MODE_SIZE_BYTE << DMA_MODE_SSIZE_OFF) | DMA_SRC_SA_BIT)
#define DMA_MODE_WRITE	            ((DMA_MODE_SIZE_BYTE << DMA_MODE_DSIZE_OFF) | (DMA_MODE_SIZE_BYTE << DMA_MODE_SSIZE_OFF) | DMA_DES_SA_BIT)
#define DMA_MODE_READ_WORD	        ((DMA_MODE_SIZE_WORD << DMA_MODE_DSIZE_OFF) | (DMA_MODE_SIZE_WORD << DMA_MODE_SSIZE_OFF) | DMA_SRC_SA_BIT)
#define DMA_MODE_WRITE_WORD         ((DMA_MODE_SIZE_WORD << DMA_MODE_DSIZE_OFF) | (DMA_MODE_SIZE_WORD << DMA_MODE_SSIZE_OFF) | DMA_DES_SA_BIT)
#define DMA_MODE_READ_LONG	        ((DMA_MODE_SIZE_LONG << DMA_MODE_DSIZE_OFF) | (DMA_MODE_SIZE_LONG << DMA_MODE_SSIZE_OFF) | DMA_SRC_SA_BIT)
#define DMA_MODE_WRITE_LONG         ((DMA_MODE_SIZE_LONG << DMA_MODE_DSIZE_OFF) | (DMA_MODE_SIZE_LONG << DMA_MODE_SSIZE_OFF) | DMA_DES_SA_BIT)

#endif 

#if !defined(CONFIG_M5272)
static __inline__ void enable_dma(unsigned int dmanr)
{
  volatile unsigned short *dmawp;

#ifdef DMA_DEBUG
  printk("enable_dma(dmanr=%d)\n", dmanr);
#endif

  dmawp = (unsigned short *) dma_base_addr[dmanr];
  dmawp[MCFDMA_DCR] |= MCFDMA_DCR_EEXT;
}

static __inline__ void disable_dma(unsigned int dmanr)
{
  volatile unsigned short *dmawp;
  volatile unsigned char  *dmapb;

#ifdef DMA_DEBUG
  printk("disable_dma(dmanr=%d)\n", dmanr);
#endif

  dmawp = (unsigned short *) dma_base_addr[dmanr];
  dmapb = (unsigned char *) dma_base_addr[dmanr];

  
  dmawp[MCFDMA_DCR] &= ~MCFDMA_DCR_EEXT;
  dmapb[MCFDMA_DSR] = MCFDMA_DSR_DONE;
}

static __inline__ void clear_dma_ff(unsigned int dmanr)
{
}

static __inline__ void set_dma_mode(unsigned int dmanr, char mode)
{

  volatile unsigned char  *dmabp;
  volatile unsigned short *dmawp;

#ifdef DMA_DEBUG
  printk("set_dma_mode(dmanr=%d,mode=%d)\n", dmanr, mode);
#endif

  dmabp = (unsigned char *) dma_base_addr[dmanr];
  dmawp = (unsigned short *) dma_base_addr[dmanr];

  
  dmabp[MCFDMA_DSR] = MCFDMA_DSR_DONE;

  
  dmawp[MCFDMA_DCR] =
    MCFDMA_DCR_INT |         
    MCFDMA_DCR_CS |          
    MCFDMA_DCR_AA |          
    
    ((mode & DMA_MODE_SINGLE_BIT) ? MCFDMA_DCR_SAA : 0) |
    
    ((mode & DMA_MODE_WRITE_BIT) ? MCFDMA_DCR_S_RW : 0) |
    
    ((mode & DMA_MODE_WRITE_BIT) ? MCFDMA_DCR_SINC : MCFDMA_DCR_DINC) |
    
    ((mode & DMA_MODE_WORD_BIT)  ? MCFDMA_DCR_SSIZE_WORD :
     ((mode & DMA_MODE_LONG_BIT) ? MCFDMA_DCR_SSIZE_LONG :
                                   MCFDMA_DCR_SSIZE_BYTE)) |
    ((mode & DMA_MODE_WORD_BIT)  ? MCFDMA_DCR_DSIZE_WORD :
     ((mode & DMA_MODE_LONG_BIT) ? MCFDMA_DCR_DSIZE_LONG :
                                   MCFDMA_DCR_DSIZE_BYTE));

#ifdef DEBUG_DMA
  printk("%s(%d): dmanr=%d DSR[%x]=%x DCR[%x]=%x\n", __FILE__, __LINE__,
         dmanr, (int) &dmabp[MCFDMA_DSR], dmabp[MCFDMA_DSR],
	 (int) &dmawp[MCFDMA_DCR], dmawp[MCFDMA_DCR]);
#endif
}

static __inline__ void set_dma_addr(unsigned int dmanr, unsigned int a)
{
  volatile unsigned short *dmawp;
  volatile unsigned int   *dmalp;

#ifdef DMA_DEBUG
  printk("set_dma_addr(dmanr=%d,a=%x)\n", dmanr, a);
#endif

  dmawp = (unsigned short *) dma_base_addr[dmanr];
  dmalp = (unsigned int *) dma_base_addr[dmanr];

  
  if (dmawp[MCFDMA_DCR] & MCFDMA_DCR_SINC) {
    
    dmalp[MCFDMA_SAR] = a;
    
    dmalp[MCFDMA_DAR] = dma_device_address[dmanr];
  } else {
    
    dmalp[MCFDMA_DAR] = a;
    
    dmalp[MCFDMA_SAR] = dma_device_address[dmanr];
  }

#ifdef DEBUG_DMA
  printk("%s(%d): dmanr=%d DCR[%x]=%x SAR[%x]=%08x DAR[%x]=%08x\n",
	__FILE__, __LINE__, dmanr, (int) &dmawp[MCFDMA_DCR], dmawp[MCFDMA_DCR],
	(int) &dmalp[MCFDMA_SAR], dmalp[MCFDMA_SAR],
	(int) &dmalp[MCFDMA_DAR], dmalp[MCFDMA_DAR]);
#endif
}

static __inline__ void set_dma_device_addr(unsigned int dmanr, unsigned int a)
{
#ifdef DMA_DEBUG
  printk("set_dma_device_addr(dmanr=%d,a=%x)\n", dmanr, a);
#endif

  dma_device_address[dmanr] = a;
}

static __inline__ void set_dma_count(unsigned int dmanr, unsigned int count)
{
  volatile unsigned short *dmawp;

#ifdef DMA_DEBUG
  printk("set_dma_count(dmanr=%d,count=%d)\n", dmanr, count);
#endif

  dmawp = (unsigned short *) dma_base_addr[dmanr];
  dmawp[MCFDMA_BCR] = (unsigned short)count;
}

static __inline__ int get_dma_residue(unsigned int dmanr)
{
  volatile unsigned short *dmawp;
  unsigned short count;

#ifdef DMA_DEBUG
  printk("get_dma_residue(dmanr=%d)\n", dmanr);
#endif

  dmawp = (unsigned short *) dma_base_addr[dmanr];
  count = dmawp[MCFDMA_BCR];
  return((int) count);
}
#else 


static __inline__ void enable_dma(unsigned int dmanr)
{
  volatile unsigned int  *dmalp;

#ifdef DMA_DEBUG
  printk("enable_dma(dmanr=%d)\n", dmanr);
#endif

  dmalp = (unsigned int *) dma_base_addr[dmanr];
  dmalp[MCFDMA_DMR] |= MCFDMA_DMR_EN;
}

static __inline__ void disable_dma(unsigned int dmanr)
{
  volatile unsigned int   *dmalp;

#ifdef DMA_DEBUG
  printk("disable_dma(dmanr=%d)\n", dmanr);
#endif

  dmalp = (unsigned int *) dma_base_addr[dmanr];

  
  dmalp[MCFDMA_DMR] &= ~MCFDMA_DMR_EN;
  dmalp[MCFDMA_DMR] |= MCFDMA_DMR_RESET;
}

static __inline__ void clear_dma_ff(unsigned int dmanr)
{
}

static __inline__ void set_dma_mode(unsigned int dmanr, char mode)
{

  volatile unsigned int   *dmalp;
  volatile unsigned short *dmawp;

#ifdef DMA_DEBUG
  printk("set_dma_mode(dmanr=%d,mode=%d)\n", dmanr, mode);
#endif
  dmalp = (unsigned int *) dma_base_addr[dmanr];
  dmawp = (unsigned short *) dma_base_addr[dmanr];

  
  dmalp[MCFDMA_DMR] |= MCFDMA_DMR_RESET;

  
  dmalp[MCFDMA_DMR] =
    MCFDMA_DMR_RQM_DUAL |         
    MCFDMA_DMR_DSTT_SD  |         
    MCFDMA_DMR_SRCT_SD  |         
    
    ((mode & DMA_MODE_SRC_SA_BIT) ? MCFDMA_DMR_SRCM_SA : MCFDMA_DMR_SRCM_IA) |
    
    ((mode & DMA_MODE_DES_SA_BIT) ? MCFDMA_DMR_DSTM_SA : MCFDMA_DMR_DSTM_IA) |
    
    (((mode & DMA_MODE_SSIZE_MASK) >> DMA_MODE_SSIZE_OFF) << MCFDMA_DMR_DSTS_OFF) |
    (((mode & DMA_MODE_SSIZE_MASK) >> DMA_MODE_SSIZE_OFF) << MCFDMA_DMR_SRCS_OFF);

  dmawp[MCFDMA_DIR] |= MCFDMA_DIR_ASCEN;   

#ifdef DEBUG_DMA
  printk("%s(%d): dmanr=%d DMR[%x]=%x DIR[%x]=%x\n", __FILE__, __LINE__,
         dmanr, (int) &dmalp[MCFDMA_DMR], dmabp[MCFDMA_DMR],
	 (int) &dmawp[MCFDMA_DIR], dmawp[MCFDMA_DIR]);
#endif
}

static __inline__ void set_dma_addr(unsigned int dmanr, unsigned int a)
{
  volatile unsigned int   *dmalp;

#ifdef DMA_DEBUG
  printk("set_dma_addr(dmanr=%d,a=%x)\n", dmanr, a);
#endif

  dmalp = (unsigned int *) dma_base_addr[dmanr];

  
  if (dmalp[MCFDMA_DMR] & MCFDMA_DMR_SRCM) {
    
    dmalp[MCFDMA_DSAR] = a;
    
    dmalp[MCFDMA_DDAR] = dma_device_address[dmanr];
  } else {
    
    dmalp[MCFDMA_DDAR] = a;
    
    dmalp[MCFDMA_DSAR] = dma_device_address[dmanr];
  }

#ifdef DEBUG_DMA
  printk("%s(%d): dmanr=%d DMR[%x]=%x SAR[%x]=%08x DAR[%x]=%08x\n",
	__FILE__, __LINE__, dmanr, (int) &dmawp[MCFDMA_DMR], dmawp[MCFDMA_DMR],
	(int) &dmalp[MCFDMA_DSAR], dmalp[MCFDMA_DSAR],
	(int) &dmalp[MCFDMA_DDAR], dmalp[MCFDMA_DDAR]);
#endif
}

static __inline__ void set_dma_device_addr(unsigned int dmanr, unsigned int a)
{
#ifdef DMA_DEBUG
  printk("set_dma_device_addr(dmanr=%d,a=%x)\n", dmanr, a);
#endif

  dma_device_address[dmanr] = a;
}

static __inline__ void set_dma_count(unsigned int dmanr, unsigned int count)
{
  volatile unsigned int *dmalp;

#ifdef DMA_DEBUG
  printk("set_dma_count(dmanr=%d,count=%d)\n", dmanr, count);
#endif

  dmalp = (unsigned int *) dma_base_addr[dmanr];
  dmalp[MCFDMA_DBCR] = count;
}

static __inline__ int get_dma_residue(unsigned int dmanr)
{
  volatile unsigned int *dmalp;
  unsigned int count;

#ifdef DMA_DEBUG
  printk("get_dma_residue(dmanr=%d)\n", dmanr);
#endif

  dmalp = (unsigned int *) dma_base_addr[dmanr];
  count = dmalp[MCFDMA_DBCR];
  return(count);
}

#endif 
#endif 

#define MAX_DMA_ADDRESS PAGE_OFFSET

#define MAX_DMA_CHANNELS 8

extern int request_dma(unsigned int dmanr, const char * device_id);	
extern void free_dma(unsigned int dmanr);	

#define isa_dma_bridge_buggy    (0)

#endif 
