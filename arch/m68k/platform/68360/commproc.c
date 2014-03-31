/*
 * General Purpose functions for the global management of the
 * Communication Processor Module.
 *
 * Copyright (c) 2000 Michael Leslie <mleslie@lineo.com>
 * Copyright (c) 1997 Dan Malek (dmalek@jlc.net)
 *
 * In addition to the individual control of the communication
 * channels, there are a few functions that globally affect the
 * communication processor.
 *
 * Buffer descriptors must be allocated from the dual ported memory
 * space.  The allocator for that is here.  When the communication
 * process is reset, we reclaim the memory available.  There is
 * currently no deallocator for this memory.
 * The amount of space available is platform dependent.  On the
 * MBX, the EPPC software loads additional microcode into the
 * communication processor, and uses some of the DP ram for this
 * purpose.  Current, the first 512 bytes and the last 256 bytes of
 * memory are used.  Right now I am conservative and only use the
 * memory that can never be used for microcode.  If there are
 * applications that require more DP ram, we can expand the boundaries
 * but then we have to be careful of any downloaded microcode.
 *
 */


#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/param.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <asm/irq.h>
#include <asm/m68360.h>
#include <asm/commproc.h>

extern void *_quicc_base;
extern unsigned int system_clock;


static uint dp_alloc_base;	
static uint dp_alloc_top;	

#if 0
static	void	*host_buffer;	
static	void	*host_end;	    
#endif

         

QUICC  *pquicc;
 


struct	cpm_action {
	void	(*handler)(void *);
	void	*dev_id;
};
static	struct	cpm_action cpm_vecs[CPMVEC_NR];
static	void	cpm_interrupt(int irq, void * dev, struct pt_regs * regs);
static	void	cpm_error_interrupt(void *);

void cpm_install_handler(int vec, void (*handler)(), void *dev_id);
void m360_cpm_reset(void);




void m360_cpm_reset()
{

	pquicc = (struct quicc *)(_quicc_base); 

	
	pquicc->cp_cr = (SOFTWARE_RESET | CMD_FLAG);

	
	while (pquicc->cp_cr & CMD_FLAG);

	pquicc->sdma_sdcr = 0x0740;


	dp_alloc_base = CPM_DATAONLY_BASE;
	dp_alloc_top = dp_alloc_base + CPM_DATAONLY_SIZE;


	
	

	
	
	

}


void
cpm_interrupt_init(void)
{
	pquicc->intr_cicr =
		(CICR_SCD_SCC4 | CICR_SCC_SCC3 | CICR_SCB_SCC2 | CICR_SCA_SCC1) |
		(CPM_INTERRUPT << 13) |
		CICR_HP_MASK |
		(CPM_VECTOR_BASE << 5) |
		CICR_SPS;

	
	pquicc->intr_cimr = 0;



	

	
	

	
	 
}



static	void
cpm_interrupt(int irq, void * dev, struct pt_regs * regs)
{
	


	
	
	

#if 0 
	((volatile immap_t *)IMAP_ADDR)->im_cpic.cpic_civr = 1;
	vec = ((volatile immap_t *)IMAP_ADDR)->im_cpic.cpic_civr;
	vec >>= 11;


	if (cpm_vecs[vec].handler != 0)
		(*cpm_vecs[vec].handler)(cpm_vecs[vec].dev_id);
	else
		((immap_t *)IMAP_ADDR)->im_cpic.cpic_cimr &= ~(1 << vec);

	((immap_t *)IMAP_ADDR)->im_cpic.cpic_cisr |= (1 << vec);
#endif

}

static	void
cpm_error_interrupt(void *dev)
{
}

void
cpm_install_handler(int vec, void (*handler)(), void *dev_id)
{

	request_irq(vec, handler, 0, "timer", dev_id);


	

}

void
cpm_free_handler(int vec)
{
	cpm_vecs[vec].handler = NULL;
	cpm_vecs[vec].dev_id = NULL;
	
	pquicc->intr_cimr &= ~(1 << vec);
}




uint
m360_cpm_dpalloc(uint size)
{
        uint    retloc;

        if ((dp_alloc_base + size) >= dp_alloc_top)
                return(CPM_DP_NOSPACE);

        retloc = dp_alloc_base;
        dp_alloc_base += size;

        return(retloc);
}


#if 0 
uint
m360_cpm_hostalloc(uint size)
{
	uint	retloc;

	if ((host_buffer + size) >= host_end)
		return(0);

	retloc = host_buffer;
	host_buffer += size;

	return(retloc);
}
#endif


#define BRG_INT_CLK		system_clock
#define BRG_UART_CLK	(BRG_INT_CLK/16)

void
m360_cpm_setbrg(uint brg, uint rate)
{
	volatile uint	*bp;

	
	bp = (volatile uint *)(&pquicc->brgc[0].l);
	bp += brg;
	*bp = ((BRG_UART_CLK / rate - 1) << 1) | CPM_BRG_EN;
}


