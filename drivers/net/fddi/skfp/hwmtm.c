/******************************************************************************
 *
 *	(C)Copyright 1998,1999 SysKonnect,
 *	a business unit of Schneider & Koch & Co. Datensysteme GmbH.
 *
 *	See the file "skfddi.c" for further information.
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 *
 *	The information in this file is provided "AS IS" without warranty.
 *
 ******************************************************************************/

#ifndef	lint
static char const ID_sccs[] = "@(#)hwmtm.c	1.40 99/05/31 (C) SK" ;
#endif

#define	HWMTM

#ifndef FDDI
#define	FDDI
#endif

#include "h/types.h"
#include "h/fddi.h"
#include "h/smc.h"
#include "h/supern_2.h"
#include "h/skfbiinc.h"

#ifdef COMMON_MB_POOL
static	SMbuf *mb_start = 0 ;
static	SMbuf *mb_free = 0 ;
static	int mb_init = FALSE ;
static	int call_count = 0 ;
#endif


#ifdef	DEBUG
#ifndef	DEBUG_BRD
extern	struct smt_debug	debug ;
#endif
#endif

#ifdef	NDIS_OS2
extern	u_char	offDepth ;
extern	u_char	force_irq_pending ;
#endif


static void queue_llc_rx(struct s_smc *smc, SMbuf *mb);
static void smt_to_llc(struct s_smc *smc, SMbuf *mb);
static void init_txd_ring(struct s_smc *smc);
static void init_rxd_ring(struct s_smc *smc);
static void queue_txd_mb(struct s_smc *smc, SMbuf *mb);
static u_long init_descr_ring(struct s_smc *smc, union s_fp_descr volatile *start,
			      int count);
static u_long repair_txd_ring(struct s_smc *smc, struct s_smt_tx_queue *queue);
static u_long repair_rxd_ring(struct s_smc *smc, struct s_smt_rx_queue *queue);
static SMbuf* get_llc_rx(struct s_smc *smc);
static SMbuf* get_txd_mb(struct s_smc *smc);
static void mac_drv_clear_txd(struct s_smc *smc);


extern void* mac_drv_get_space(struct s_smc *smc, unsigned int size);
extern void* mac_drv_get_desc_mem(struct s_smc *smc, unsigned int size);
extern void mac_drv_fill_rxd(struct s_smc *smc);
extern void mac_drv_tx_complete(struct s_smc *smc,
				volatile struct s_smt_fp_txd *txd);
extern void mac_drv_rx_complete(struct s_smc *smc,
				volatile struct s_smt_fp_rxd *rxd,
				int frag_count, int len);
extern void mac_drv_requeue_rxd(struct s_smc *smc, 
				volatile struct s_smt_fp_rxd *rxd,
				int frag_count);
extern void mac_drv_clear_rxd(struct s_smc *smc,
			      volatile struct s_smt_fp_rxd *rxd, int frag_count);

#ifdef	USE_OS_CPY
extern void hwm_cpy_rxd2mb(void);
extern void hwm_cpy_txd2mb(void);
#endif

#ifdef	ALL_RX_COMPLETE
extern void mac_drv_all_receives_complete(void);
#endif

extern u_long mac_drv_virt2phys(struct s_smc *smc, void *virt);
extern u_long dma_master(struct s_smc *smc, void *virt, int len, int flag);

#ifdef	NDIS_OS2
extern void post_proc(void);
#else
extern void dma_complete(struct s_smc *smc, volatile union s_fp_descr *descr,
			 int flag);
#endif

extern int mac_drv_rx_init(struct s_smc *smc, int len, int fc, char *look_ahead,
			   int la_len);

void process_receive(struct s_smc *smc);
void fddi_isr(struct s_smc *smc);
void smt_free_mbuf(struct s_smc *smc, SMbuf *mb);
void init_driver_fplus(struct s_smc *smc);
void mac_drv_rx_mode(struct s_smc *smc, int mode);
void init_fddi_driver(struct s_smc *smc, u_char *mac_addr);
void mac_drv_clear_tx_queue(struct s_smc *smc);
void mac_drv_clear_rx_queue(struct s_smc *smc);
void hwm_tx_frag(struct s_smc *smc, char far *virt, u_long phys, int len,
		 int frame_status);
void hwm_rx_frag(struct s_smc *smc, char far *virt, u_long phys, int len,
		 int frame_status);

int mac_drv_init(struct s_smc *smc);
int hwm_tx_init(struct s_smc *smc, u_char fc, int frag_count, int frame_len,
		int frame_status);

u_int mac_drv_check_space(void);

SMbuf* smt_get_mbuf(struct s_smc *smc);

#ifdef DEBUG
	void mac_drv_debug_lev(void);
#endif

#ifndef	UNUSED
#ifdef	lint
#define UNUSED(x)	(x) = (x)
#else
#define UNUSED(x)
#endif
#endif

#ifdef	USE_CAN_ADDR
#define MA		smc->hw.fddi_canon_addr.a
#define	GROUP_ADDR_BIT	0x01
#else
#define	MA		smc->hw.fddi_home_addr.a
#define	GROUP_ADDR_BIT	0x80
#endif

#define RXD_TXD_COUNT	(HWM_ASYNC_TXD_COUNT+HWM_SYNC_TXD_COUNT+\
			SMT_R1_RXD_COUNT+SMT_R2_RXD_COUNT)

#ifdef	MB_OUTSIDE_SMC
#define	EXT_VIRT_MEM	((RXD_TXD_COUNT+1)*sizeof(struct s_smt_fp_txd) +\
			MAX_MBUF*sizeof(SMbuf))
#define	EXT_VIRT_MEM_2	((RXD_TXD_COUNT+1)*sizeof(struct s_smt_fp_txd))
#else
#define	EXT_VIRT_MEM	((RXD_TXD_COUNT+1)*sizeof(struct s_smt_fp_txd))
#endif

#if	defined(NDIS_OS2) || defined(ODI2)
#define CR_READ(var)	((var) & 0xffff0000 | ((var) & 0xffff))
#else
#define CR_READ(var)	(__le32)(var)
#endif

#define IMASK_SLOW	(IS_PLINT1 | IS_PLINT2 | IS_TIMINT | IS_TOKEN | \
			 IS_MINTR1 | IS_MINTR2 | IS_MINTR3 | IS_R1_P | \
			 IS_R1_C | IS_XA_C | IS_XS_C)



u_int mac_drv_check_space(void)
{
#ifdef	MB_OUTSIDE_SMC
#ifdef	COMMON_MB_POOL
	call_count++ ;
	if (call_count == 1) {
		return EXT_VIRT_MEM;
	}
	else {
		return EXT_VIRT_MEM_2;
	}
#else
	return EXT_VIRT_MEM;
#endif
#else
	return 0;
#endif
}

int mac_drv_init(struct s_smc *smc)
{
	if (sizeof(struct s_smt_fp_rxd) % 16) {
		SMT_PANIC(smc,HWM_E0001,HWM_E0001_MSG) ;
	}
	if (sizeof(struct s_smt_fp_txd) % 16) {
		SMT_PANIC(smc,HWM_E0002,HWM_E0002_MSG) ;
	}

	if (!(smc->os.hwm.descr_p = (union s_fp_descr volatile *)
		mac_drv_get_desc_mem(smc,(u_int)
		(RXD_TXD_COUNT+1)*sizeof(struct s_smt_fp_txd)))) {
		return 1;	
	}

#ifndef	MB_OUTSIDE_SMC
	smc->os.hwm.mbuf_pool.mb_start=(SMbuf *)(&smc->os.hwm.mbuf_pool.mb[0]) ;
#else
#ifndef	COMMON_MB_POOL
	if (!(smc->os.hwm.mbuf_pool.mb_start = (SMbuf *) mac_drv_get_space(smc,
		MAX_MBUF*sizeof(SMbuf)))) {
		return 1;	
	}
#else
	if (!mb_start) {
		if (!(mb_start = (SMbuf *) mac_drv_get_space(smc,
			MAX_MBUF*sizeof(SMbuf)))) {
			return 1;	
		}
	}
#endif
#endif
	return 0;
}

void init_driver_fplus(struct s_smc *smc)
{
	smc->hw.fp.mdr2init = FM_LSB | FM_BMMODE | FM_ENNPRQ | FM_ENHSRQ | 3 ;

#ifdef	PCI
	smc->hw.fp.mdr2init |= FM_CHKPAR | FM_PARITY ;
#endif
	smc->hw.fp.mdr3init = FM_MENRQAUNLCK | FM_MENRS ;

#ifdef	USE_CAN_ADDR
	
	smc->hw.fp.frselreg_init = FM_ENXMTADSWAP | FM_ENRCVADSWAP ;
#endif
}

static u_long init_descr_ring(struct s_smc *smc,
			      union s_fp_descr volatile *start,
			      int count)
{
	int i ;
	union s_fp_descr volatile *d1 ;
	union s_fp_descr volatile *d2 ;
	u_long	phys ;

	DB_GEN("descr ring starts at = %x ",(void *)start,0,3) ;
	for (i=count-1, d1=start; i ; i--) {
		d2 = d1 ;
		d1++ ;		
		d2->r.rxd_rbctrl = cpu_to_le32(BMU_CHECK) ;
		d2->r.rxd_next = &d1->r ;
		phys = mac_drv_virt2phys(smc,(void *)d1) ;
		d2->r.rxd_nrdadr = cpu_to_le32(phys) ;
	}
	DB_GEN("descr ring ends at = %x ",(void *)d1,0,3) ;
	d1->r.rxd_rbctrl = cpu_to_le32(BMU_CHECK) ;
	d1->r.rxd_next = &start->r ;
	phys = mac_drv_virt2phys(smc,(void *)start) ;
	d1->r.rxd_nrdadr = cpu_to_le32(phys) ;

	for (i=count, d1=start; i ; i--) {
		DRV_BUF_FLUSH(&d1->r,DDI_DMA_SYNC_FORDEV) ;
		d1++;
	}
	return phys;
}

static void init_txd_ring(struct s_smc *smc)
{
	struct s_smt_fp_txd volatile *ds ;
	struct s_smt_tx_queue *queue ;
	u_long	phys ;

	ds = (struct s_smt_fp_txd volatile *) ((char *)smc->os.hwm.descr_p +
		SMT_R1_RXD_COUNT*sizeof(struct s_smt_fp_rxd)) ;
	queue = smc->hw.fp.tx[QUEUE_A0] ;
	DB_GEN("Init async TxD ring, %d TxDs ",HWM_ASYNC_TXD_COUNT,0,3) ;
	(void)init_descr_ring(smc,(union s_fp_descr volatile *)ds,
		HWM_ASYNC_TXD_COUNT) ;
	phys = le32_to_cpu(ds->txd_ntdadr) ;
	ds++ ;
	queue->tx_curr_put = queue->tx_curr_get = ds ;
	ds-- ;
	queue->tx_free = HWM_ASYNC_TXD_COUNT ;
	queue->tx_used = 0 ;
	outpd(ADDR(B5_XA_DA),phys) ;

	ds = (struct s_smt_fp_txd volatile *) ((char *)ds +
		HWM_ASYNC_TXD_COUNT*sizeof(struct s_smt_fp_txd)) ;
	queue = smc->hw.fp.tx[QUEUE_S] ;
	DB_GEN("Init sync TxD ring, %d TxDs ",HWM_SYNC_TXD_COUNT,0,3) ;
	(void)init_descr_ring(smc,(union s_fp_descr volatile *)ds,
		HWM_SYNC_TXD_COUNT) ;
	phys = le32_to_cpu(ds->txd_ntdadr) ;
	ds++ ;
	queue->tx_curr_put = queue->tx_curr_get = ds ;
	queue->tx_free = HWM_SYNC_TXD_COUNT ;
	queue->tx_used = 0 ;
	outpd(ADDR(B5_XS_DA),phys) ;
}

static void init_rxd_ring(struct s_smc *smc)
{
	struct s_smt_fp_rxd volatile *ds ;
	struct s_smt_rx_queue *queue ;
	u_long	phys ;

	ds = (struct s_smt_fp_rxd volatile *) smc->os.hwm.descr_p ;
	queue = smc->hw.fp.rx[QUEUE_R1] ;
	DB_GEN("Init RxD ring, %d RxDs ",SMT_R1_RXD_COUNT,0,3) ;
	(void)init_descr_ring(smc,(union s_fp_descr volatile *)ds,
		SMT_R1_RXD_COUNT) ;
	phys = le32_to_cpu(ds->rxd_nrdadr) ;
	ds++ ;
	queue->rx_curr_put = queue->rx_curr_get = ds ;
	queue->rx_free = SMT_R1_RXD_COUNT ;
	queue->rx_used = 0 ;
	outpd(ADDR(B4_R1_DA),phys) ;
}

void init_fddi_driver(struct s_smc *smc, u_char *mac_addr)
{
	SMbuf	*mb ;
	int	i ;

	init_board(smc,mac_addr) ;
	(void)init_fplus(smc) ;

#ifndef	COMMON_MB_POOL
	mb = smc->os.hwm.mbuf_pool.mb_start ;
	smc->os.hwm.mbuf_pool.mb_free = (SMbuf *)NULL ;
	for (i = 0; i < MAX_MBUF; i++) {
		mb->sm_use_count = 1 ;
		smt_free_mbuf(smc,mb)	;
		mb++ ;
	}
#else
	mb = mb_start ;
	if (!mb_init) {
		mb_free = 0 ;
		for (i = 0; i < MAX_MBUF; i++) {
			mb->sm_use_count = 1 ;
			smt_free_mbuf(smc,mb)	;
			mb++ ;
		}
		mb_init = TRUE ;
	}
#endif

	smc->os.hwm.llc_rx_pipe = smc->os.hwm.llc_rx_tail = (SMbuf *)NULL ;
	smc->os.hwm.txd_tx_pipe = smc->os.hwm.txd_tx_tail = NULL ;
	smc->os.hwm.pass_SMT = smc->os.hwm.pass_NSA = smc->os.hwm.pass_DB = 0 ;
	smc->os.hwm.pass_llc_promisc = TRUE ;
	smc->os.hwm.queued_rx_frames = smc->os.hwm.queued_txd_mb = 0 ;
	smc->os.hwm.detec_count = 0 ;
	smc->os.hwm.rx_break = 0 ;
	smc->os.hwm.rx_len_error = 0 ;
	smc->os.hwm.isr_flag = FALSE ;

	i = 16 - ((long)smc->os.hwm.descr_p & 0xf) ;
	if (i != 16) {
		DB_GEN("i = %d",i,0,3) ;
		smc->os.hwm.descr_p = (union s_fp_descr volatile *)
			((char *)smc->os.hwm.descr_p+i) ;
	}
	DB_GEN("pt to descr area = %x",(void *)smc->os.hwm.descr_p,0,3) ;

	init_txd_ring(smc) ;
	init_rxd_ring(smc) ;
	mac_drv_fill_rxd(smc) ;

	init_plc(smc) ;
}


SMbuf *smt_get_mbuf(struct s_smc *smc)
{
	register SMbuf	*mb ;

#ifndef	COMMON_MB_POOL
	mb = smc->os.hwm.mbuf_pool.mb_free ;
#else
	mb = mb_free ;
#endif
	if (mb) {
#ifndef	COMMON_MB_POOL
		smc->os.hwm.mbuf_pool.mb_free = mb->sm_next ;
#else
		mb_free = mb->sm_next ;
#endif
		mb->sm_off = 8 ;
		mb->sm_use_count = 1 ;
	}
	DB_GEN("get SMbuf: mb = %x",(void *)mb,0,3) ;
	return mb;	
}

void smt_free_mbuf(struct s_smc *smc, SMbuf *mb)
{

	if (mb) {
		mb->sm_use_count-- ;
		DB_GEN("free_mbuf: sm_use_count = %d",mb->sm_use_count,0,3) ;
		if (!mb->sm_use_count) {
			DB_GEN("free SMbuf: mb = %x",(void *)mb,0,3) ;
#ifndef	COMMON_MB_POOL
			mb->sm_next = smc->os.hwm.mbuf_pool.mb_free ;
			smc->os.hwm.mbuf_pool.mb_free = mb ;
#else
			mb->sm_next = mb_free ;
			mb_free = mb ;
#endif
		}
	}
	else
		SMT_PANIC(smc,HWM_E0003,HWM_E0003_MSG) ;
}


void mac_drv_repair_descr(struct s_smc *smc)
{
	u_long	phys ;

	if (smc->hw.hw_state != STOPPED) {
		SK_BREAK() ;
		SMT_PANIC(smc,HWM_E0013,HWM_E0013_MSG) ;
		return ;
	}

	phys = repair_txd_ring(smc,smc->hw.fp.tx[QUEUE_A0]) ;
	outpd(ADDR(B5_XA_DA),phys) ;
	if (smc->hw.fp.tx_q[QUEUE_A0].tx_used) {
		outpd(ADDR(B0_XA_CSR),CSR_START) ;
	}
	phys = repair_txd_ring(smc,smc->hw.fp.tx[QUEUE_S]) ;
	outpd(ADDR(B5_XS_DA),phys) ;
	if (smc->hw.fp.tx_q[QUEUE_S].tx_used) {
		outpd(ADDR(B0_XS_CSR),CSR_START) ;
	}

	phys = repair_rxd_ring(smc,smc->hw.fp.rx[QUEUE_R1]) ;
	outpd(ADDR(B4_R1_DA),phys) ;
	outpd(ADDR(B0_R1_CSR),CSR_START) ;
}

static u_long repair_txd_ring(struct s_smc *smc, struct s_smt_tx_queue *queue)
{
	int i ;
	int tx_used ;
	u_long phys ;
	u_long tbctrl ;
	struct s_smt_fp_txd volatile *t ;

	SK_UNUSED(smc) ;

	t = queue->tx_curr_get ;
	tx_used = queue->tx_used ;
	for (i = tx_used+queue->tx_free-1 ; i ; i-- ) {
		t = t->txd_next ;
	}
	phys = le32_to_cpu(t->txd_ntdadr) ;

	t = queue->tx_curr_get ;
	while (tx_used) {
		DRV_BUF_FLUSH(t,DDI_DMA_SYNC_FORCPU) ;
		tbctrl = le32_to_cpu(t->txd_tbctrl) ;

		if (tbctrl & BMU_OWN) {
			if (tbctrl & BMU_STF) {
				break ;		
			}
			else {
				t->txd_tbctrl &= ~cpu_to_le32(BMU_OWN) ;
			}
		}
		phys = le32_to_cpu(t->txd_ntdadr) ;
		DRV_BUF_FLUSH(t,DDI_DMA_SYNC_FORDEV) ;
		t = t->txd_next ;
		tx_used-- ;
	}
	return phys;
}

static u_long repair_rxd_ring(struct s_smc *smc, struct s_smt_rx_queue *queue)
{
	int i ;
	int rx_used ;
	u_long phys ;
	u_long rbctrl ;
	struct s_smt_fp_rxd volatile *r ;

	SK_UNUSED(smc) ;

	r = queue->rx_curr_get ;
	rx_used = queue->rx_used ;
	for (i = SMT_R1_RXD_COUNT-1 ; i ; i-- ) {
		r = r->rxd_next ;
	}
	phys = le32_to_cpu(r->rxd_nrdadr) ;

	r = queue->rx_curr_get ;
	while (rx_used) {
		DRV_BUF_FLUSH(r,DDI_DMA_SYNC_FORCPU) ;
		rbctrl = le32_to_cpu(r->rxd_rbctrl) ;

		if (rbctrl & BMU_OWN) {
			if (rbctrl & BMU_STF) {
				break ;		
			}
			else {
				r->rxd_rbctrl &= ~cpu_to_le32(BMU_OWN) ;
			}
		}
		phys = le32_to_cpu(r->rxd_nrdadr) ;
		DRV_BUF_FLUSH(r,DDI_DMA_SYNC_FORDEV) ;
		r = r->rxd_next ;
		rx_used-- ;
	}
	return phys;
}



void fddi_isr(struct s_smc *smc)
{
	u_long		is ;		
	u_short		stu, stl ;
	SMbuf		*mb ;

#ifdef	USE_BREAK_ISR
	int	force_irq ;
#endif

#ifdef	ODI2
	if (smc->os.hwm.rx_break) {
		mac_drv_fill_rxd(smc) ;
		if (smc->hw.fp.rx_q[QUEUE_R1].rx_used > 0) {
			smc->os.hwm.rx_break = 0 ;
			process_receive(smc) ;
		}
		else {
			smc->os.hwm.detec_count = 0 ;
			smt_force_irq(smc) ;
		}
	}
#endif
	smc->os.hwm.isr_flag = TRUE ;

#ifdef	USE_BREAK_ISR
	force_irq = TRUE ;
	if (smc->os.hwm.leave_isr) {
		smc->os.hwm.leave_isr = FALSE ;
		process_receive(smc) ;
	}
#endif

	while ((is = GET_ISR() & ISR_MASK)) {
		NDD_TRACE("CH0B",is,0,0) ;
		DB_GEN("ISA = 0x%x",is,0,7) ;

		if (is & IMASK_SLOW) {
			NDD_TRACE("CH1b",is,0,0) ;
			if (is & IS_PLINT1) {	
				plc1_irq(smc) ;
			}
			if (is & IS_PLINT2) {	
				plc2_irq(smc) ;
			}
			if (is & IS_MINTR1) {	
				stu = inpw(FM_A(FM_ST1U)) ;
				stl = inpw(FM_A(FM_ST1L)) ;
				DB_GEN("Slow transmit complete",0,0,6) ;
				mac1_irq(smc,stu,stl) ;
			}
			if (is & IS_MINTR2) {	
				stu= inpw(FM_A(FM_ST2U)) ;
				stl= inpw(FM_A(FM_ST2L)) ;
				DB_GEN("Slow receive complete",0,0,6) ;
				DB_GEN("stl = %x : stu = %x",stl,stu,7) ;
				mac2_irq(smc,stu,stl) ;
			}
			if (is & IS_MINTR3) {	
				stu= inpw(FM_A(FM_ST3U)) ;
				stl= inpw(FM_A(FM_ST3L)) ;
				DB_GEN("FORMAC Mode Register 3",0,0,6) ;
				mac3_irq(smc,stu,stl) ;
			}
			if (is & IS_TIMINT) {	
				timer_irq(smc) ;
#ifdef	NDIS_OS2
				force_irq_pending = 0 ;
#endif
				if (++smc->os.hwm.detec_count > 4) {
					 process_receive(smc) ;
				}
			}
			if (is & IS_TOKEN) {	
				rtm_irq(smc) ;
			}
			if (is & IS_R1_P) {	
				
				outpd(ADDR(B4_R1_CSR),CSR_IRQ_CL_P) ;
				SMT_PANIC(smc,HWM_E0004,HWM_E0004_MSG) ;
			}
			if (is & IS_R1_C) {	
				
				outpd(ADDR(B4_R1_CSR),CSR_IRQ_CL_C) ;
				SMT_PANIC(smc,HWM_E0005,HWM_E0005_MSG) ;
			}
			if (is & IS_XA_C) {	
				
				outpd(ADDR(B5_XA_CSR),CSR_IRQ_CL_C) ;
				SMT_PANIC(smc,HWM_E0006,HWM_E0006_MSG) ;
			}
			if (is & IS_XS_C) {	
				
				outpd(ADDR(B5_XS_CSR),CSR_IRQ_CL_C) ;
				SMT_PANIC(smc,HWM_E0007,HWM_E0007_MSG) ;
			}
		}

		if (is & (IS_XS_F|IS_XA_F)) {
			DB_GEN("Fast tx complete queue",0,0,6) ;
			outpd(ADDR(B5_XS_CSR),CSR_IRQ_CL_F) ;
			outpd(ADDR(B5_XA_CSR),CSR_IRQ_CL_F) ;
			mac_drv_clear_txd(smc) ;
			llc_restart_tx(smc) ;
		}

		if (is & IS_R1_F) {
			DB_GEN("Fast receive complete",0,0,6) ;
			
#ifndef USE_BREAK_ISR
			outpd(ADDR(B4_R1_CSR),CSR_IRQ_CL_F) ;
			process_receive(smc) ;
#else
			process_receive(smc) ;
			if (smc->os.hwm.leave_isr) {
				force_irq = FALSE ;
			} else {
				outpd(ADDR(B4_R1_CSR),CSR_IRQ_CL_F) ;
				process_receive(smc) ;
			}
#endif
		}

#ifndef	NDIS_OS2
		while ((mb = get_llc_rx(smc))) {
			smt_to_llc(smc,mb) ;
		}
#else
		if (offDepth)
			post_proc() ;

		while (!offDepth && (mb = get_llc_rx(smc))) {
			smt_to_llc(smc,mb) ;
		}

		if (!offDepth && smc->os.hwm.rx_break) {
			process_receive(smc) ;
		}
#endif
		if (smc->q.ev_get != smc->q.ev_put) {
			NDD_TRACE("CH2a",0,0,0) ;
			ev_dispatcher(smc) ;
		}
#ifdef	NDIS_OS2
		post_proc() ;
		if (offDepth) {		
			break ;		
		}
#endif
#ifdef	USE_BREAK_ISR
		if (smc->os.hwm.leave_isr) {
			break ;		
		}
#endif

		
	}	

#ifdef	USE_BREAK_ISR
	if (smc->os.hwm.leave_isr && force_irq) {
		smt_force_irq(smc) ;
	}
#endif
	smc->os.hwm.isr_flag = FALSE ;
	NDD_TRACE("CH0E",0,0,0) ;
}



#ifndef	NDIS_OS2
void mac_drv_rx_mode(struct s_smc *smc, int mode)
{
	switch(mode) {
	case RX_ENABLE_PASS_SMT:
		smc->os.hwm.pass_SMT = TRUE ;
		break ;
	case RX_DISABLE_PASS_SMT:
		smc->os.hwm.pass_SMT = FALSE ;
		break ;
	case RX_ENABLE_PASS_NSA:
		smc->os.hwm.pass_NSA = TRUE ;
		break ;
	case RX_DISABLE_PASS_NSA:
		smc->os.hwm.pass_NSA = FALSE ;
		break ;
	case RX_ENABLE_PASS_DB:
		smc->os.hwm.pass_DB = TRUE ;
		break ;
	case RX_DISABLE_PASS_DB:
		smc->os.hwm.pass_DB = FALSE ;
		break ;
	case RX_DISABLE_PASS_ALL:
		smc->os.hwm.pass_SMT = smc->os.hwm.pass_NSA = FALSE ;
		smc->os.hwm.pass_DB = FALSE ;
		smc->os.hwm.pass_llc_promisc = TRUE ;
		mac_set_rx_mode(smc,RX_DISABLE_NSA) ;
		break ;
	case RX_DISABLE_LLC_PROMISC:
		smc->os.hwm.pass_llc_promisc = FALSE ;
		break ;
	case RX_ENABLE_LLC_PROMISC:
		smc->os.hwm.pass_llc_promisc = TRUE ;
		break ;
	case RX_ENABLE_ALLMULTI:
	case RX_DISABLE_ALLMULTI:
	case RX_ENABLE_PROMISC:
	case RX_DISABLE_PROMISC:
	case RX_ENABLE_NSA:
	case RX_DISABLE_NSA:
	default:
		mac_set_rx_mode(smc,mode) ;
		break ;
	}
}
#endif	

void process_receive(struct s_smc *smc)
{
	int i ;
	int n ;
	int frag_count ;		
	int used_frags ;		
	struct s_smt_rx_queue *queue ;	
	struct s_smt_fp_rxd volatile *r ;	
	struct s_smt_fp_rxd volatile *rxd ;	
	u_long rbctrl ;			
	u_long rfsw ;			
	u_short rx_used ;
	u_char far *virt ;
	char far *data ;
	SMbuf *mb ;
	u_char fc ;			
	int len ;			

	smc->os.hwm.detec_count = 0 ;
	queue = smc->hw.fp.rx[QUEUE_R1] ;
	NDD_TRACE("RHxB",0,0,0) ;
	for ( ; ; ) {
		r = queue->rx_curr_get ;
		rx_used = queue->rx_used ;
		frag_count = 0 ;

#ifdef	USE_BREAK_ISR
		if (smc->os.hwm.leave_isr) {
			goto rx_end ;
		}
#endif
#ifdef	NDIS_OS2
		if (offDepth) {
			smc->os.hwm.rx_break = 1 ;
			goto rx_end ;
		}
		smc->os.hwm.rx_break = 0 ;
#endif
#ifdef	ODI2
		if (smc->os.hwm.rx_break) {
			goto rx_end ;
		}
#endif
		n = 0 ;
		do {
			DB_RX("Check RxD %x for OWN and EOF",(void *)r,0,5) ;
			DRV_BUF_FLUSH(r,DDI_DMA_SYNC_FORCPU) ;
			rbctrl = le32_to_cpu(CR_READ(r->rxd_rbctrl));

			if (rbctrl & BMU_OWN) {
				NDD_TRACE("RHxE",r,rfsw,rbctrl) ;
				DB_RX("End of RxDs",0,0,4) ;
				goto rx_end ;
			}
			if (!rx_used) {
				SK_BREAK() ;
				SMT_PANIC(smc,HWM_E0009,HWM_E0009_MSG) ;
				smc->hw.hw_state = STOPPED ;
				mac_drv_clear_rx_queue(smc) ;
				smc->hw.hw_state = STARTED ;
				mac_drv_fill_rxd(smc) ;
				smc->os.hwm.detec_count = 0 ;
				goto rx_end ;
			}
			rfsw = le32_to_cpu(r->rxd_rfsw) ;
			if ((rbctrl & BMU_STF) != ((rbctrl & BMU_ST_BUF) <<5)) {
				SK_BREAK() ;
				rfsw = 0 ;
				if (frag_count) {
					break ;
				}
			}
			n += rbctrl & 0xffff ;
			r = r->rxd_next ;
			frag_count++ ;
			rx_used-- ;
		} while (!(rbctrl & BMU_EOF)) ;
		used_frags = frag_count ;
		DB_RX("EOF set in RxD, used_frags = %d ",used_frags,0,5) ;

		
		
		DRV_BUF_FLUSH(r,DDI_DMA_SYNC_FORCPU) ;
		while (rx_used && !(r->rxd_rbctrl & cpu_to_le32(BMU_ST_BUF))) {
			DB_RX("Check STF bit in %x",(void *)r,0,5) ;
			r = r->rxd_next ;
			DRV_BUF_FLUSH(r,DDI_DMA_SYNC_FORCPU) ;
			frag_count++ ;
			rx_used-- ;
		}
		DB_RX("STF bit found",0,0,5) ;

		rxd = queue->rx_curr_get ;
		queue->rx_curr_get = r ;
		queue->rx_free += frag_count ;
		queue->rx_used = rx_used ;

		rxd->rxd_rbctrl &= cpu_to_le32(~BMU_STF) ;

		for (r=rxd, i=frag_count ; i ; r=r->rxd_next, i--){
			DB_RX("dma_complete for RxD %x",(void *)r,0,5) ;
			dma_complete(smc,(union s_fp_descr volatile *)r,DMA_WR);
		}
		smc->hw.fp.err_stats.err_valid++ ;
		smc->mib.m[MAC0].fddiMACCopied_Ct++ ;

		
		len = (rfsw & RD_LENGTH) - 4 ;

		DB_RX("frame length = %d",len,0,4) ;
		if (rfsw & (RX_MSRABT|RX_FS_E|RX_FS_CRC|RX_FS_IMPL)){
			if (rfsw & RD_S_MSRABT) {
				DB_RX("Frame aborted by the FORMAC",0,0,2) ;
				smc->hw.fp.err_stats.err_abort++ ;
			}
			if (rfsw & RD_S_SEAC2) {
				DB_RX("E-Indicator set",0,0,2) ;
				smc->hw.fp.err_stats.err_e_indicator++ ;
			}
			if (rfsw & RD_S_SFRMERR) {
				DB_RX("CRC error",0,0,2) ;
				smc->hw.fp.err_stats.err_crc++ ;
			}
			if (rfsw & RX_FS_IMPL) {
				DB_RX("Implementer frame",0,0,2) ;
				smc->hw.fp.err_stats.err_imp_frame++ ;
			}
			goto abort_frame ;
		}
		if (len > FDDI_RAW_MTU-4) {
			DB_RX("Frame too long error",0,0,2) ;
			smc->hw.fp.err_stats.err_too_long++ ;
			goto abort_frame ;
		}
		if (len <= 4) {
			DB_RX("Frame length = 0",0,0,2) ;
			goto abort_frame ;
		}

		if (len != (n-4)) {
			DB_RX("BMU: rx len differs: [%d:%d]",len,n,4);
			smc->os.hwm.rx_len_error++ ;
			goto abort_frame ;
		}

		virt = (u_char far *) rxd->rxd_virt ;
		DB_RX("FC = %x",*virt,0,2) ;
		if (virt[12] == MA[5] &&
		    virt[11] == MA[4] &&
		    virt[10] == MA[3] &&
		    virt[9] == MA[2] &&
		    virt[8] == MA[1] &&
		    (virt[7] & ~GROUP_ADDR_BIT) == MA[0]) {
			goto abort_frame ;
		}

		if (rfsw & RX_FS_LLC) {
			if (!smc->os.hwm.pass_llc_promisc) {
				if(!(virt[1] & GROUP_ADDR_BIT)) {
					if (virt[6] != MA[5] ||
					    virt[5] != MA[4] ||
					    virt[4] != MA[3] ||
					    virt[3] != MA[2] ||
					    virt[2] != MA[1] ||
					    virt[1] != MA[0]) {
						DB_RX("DA != MA and not multi- or broadcast",0,0,2) ;
						goto abort_frame ;
					}
				}
			}

			DB_RX("LLC - receive",0,0,4) ;
			mac_drv_rx_complete(smc,rxd,frag_count,len) ;
		}
		else {
			if (!(mb = smt_get_mbuf(smc))) {
				smc->hw.fp.err_stats.err_no_buf++ ;
				DB_RX("No SMbuf; receive terminated",0,0,4) ;
				goto abort_frame ;
			}
			data = smtod(mb,char *) - 1 ;

#ifdef USE_OS_CPY
			hwm_cpy_rxd2mb(rxd,data,len) ;
#else
			for (r=rxd, i=used_frags ; i ; r=r->rxd_next, i--){
				n = le32_to_cpu(r->rxd_rbctrl) & RD_LENGTH ;
				DB_RX("cp SMT frame to mb: len = %d",n,0,6) ;
				memcpy(data,r->rxd_virt,n) ;
				data += n ;
			}
			data = smtod(mb,char *) - 1 ;
#endif
			fc = *(char *)mb->sm_data = *data ;
			mb->sm_len = len - 1 ;		
			data++ ;

			switch(fc) {
			case FC_SMT_INFO :
				smc->hw.fp.err_stats.err_smt_frame++ ;
				DB_RX("SMT frame received ",0,0,5) ;

				if (smc->os.hwm.pass_SMT) {
					DB_RX("pass SMT frame ",0,0,5) ;
					mac_drv_rx_complete(smc, rxd,
						frag_count,len) ;
				}
				else {
					DB_RX("requeue RxD",0,0,5) ;
					mac_drv_requeue_rxd(smc,rxd,frag_count);
				}

				smt_received_pack(smc,mb,(int)(rfsw>>25)) ;
				break ;
			case FC_SMT_NSA :
				smc->hw.fp.err_stats.err_smt_frame++ ;
				DB_RX("SMT frame received ",0,0,5) ;

				
				
				
				if (smc->os.hwm.pass_NSA ||
					(smc->os.hwm.pass_SMT &&
					!(rfsw & A_INDIC))) {
					DB_RX("pass SMT frame ",0,0,5) ;
					mac_drv_rx_complete(smc, rxd,
						frag_count,len) ;
				}
				else {
					DB_RX("requeue RxD",0,0,5) ;
					mac_drv_requeue_rxd(smc,rxd,frag_count);
				}

				smt_received_pack(smc,mb,(int)(rfsw>>25)) ;
				break ;
			case FC_BEACON :
				if (smc->os.hwm.pass_DB) {
					DB_RX("pass DB frame ",0,0,5) ;
					mac_drv_rx_complete(smc, rxd,
						frag_count,len) ;
				}
				else {
					DB_RX("requeue RxD",0,0,5) ;
					mac_drv_requeue_rxd(smc,rxd,frag_count);
				}
				smt_free_mbuf(smc,mb) ;
				break ;
			default :
				DB_RX("unknown FC error",0,0,2) ;
				smt_free_mbuf(smc,mb) ;
				DB_RX("requeue RxD",0,0,5) ;
				mac_drv_requeue_rxd(smc,rxd,frag_count) ;
				if ((fc & 0xf0) == FC_MAC)
					smc->hw.fp.err_stats.err_mac_frame++ ;
				else
					smc->hw.fp.err_stats.err_imp_frame++ ;

				break ;
			}
		}

		DB_RX("next RxD is %x ",queue->rx_curr_get,0,3) ;
		NDD_TRACE("RHx1",queue->rx_curr_get,0,0) ;

		continue ;
	
abort_frame:
		DB_RX("requeue RxD",0,0,5) ;
		mac_drv_requeue_rxd(smc,rxd,frag_count) ;

		DB_RX("next RxD is %x ",queue->rx_curr_get,0,3) ;
		NDD_TRACE("RHx2",queue->rx_curr_get,0,0) ;
	}
rx_end:
#ifdef	ALL_RX_COMPLETE
	mac_drv_all_receives_complete(smc) ;
#endif
	return ;	
}

static void smt_to_llc(struct s_smc *smc, SMbuf *mb)
{
	u_char	fc ;

	DB_RX("send a queued frame to the llc layer",0,0,4) ;
	smc->os.hwm.r.len = mb->sm_len ;
	smc->os.hwm.r.mb_pos = smtod(mb,char *) ;
	fc = *smc->os.hwm.r.mb_pos ;
	(void)mac_drv_rx_init(smc,(int)mb->sm_len,(int)fc,
		smc->os.hwm.r.mb_pos,(int)mb->sm_len) ;
	smt_free_mbuf(smc,mb) ;
}

void hwm_rx_frag(struct s_smc *smc, char far *virt, u_long phys, int len,
		 int frame_status)
{
	struct s_smt_fp_rxd volatile *r ;
	__le32	rbctrl;

	NDD_TRACE("RHfB",virt,len,frame_status) ;
	DB_RX("hwm_rx_frag: len = %d, frame_status = %x\n",len,frame_status,2) ;
	r = smc->hw.fp.rx_q[QUEUE_R1].rx_curr_put ;
	r->rxd_virt = virt ;
	r->rxd_rbadr = cpu_to_le32(phys) ;
	rbctrl = cpu_to_le32( (((__u32)frame_status &
		(FIRST_FRAG|LAST_FRAG))<<26) |
		(((u_long) frame_status & FIRST_FRAG) << 21) |
		BMU_OWN | BMU_CHECK | BMU_EN_IRQ_EOF | len) ;
	r->rxd_rbctrl = rbctrl ;

	DRV_BUF_FLUSH(r,DDI_DMA_SYNC_FORDEV) ;
	outpd(ADDR(B0_R1_CSR),CSR_START) ;
	smc->hw.fp.rx_q[QUEUE_R1].rx_free-- ;
	smc->hw.fp.rx_q[QUEUE_R1].rx_used++ ;
	smc->hw.fp.rx_q[QUEUE_R1].rx_curr_put = r->rxd_next ;
	NDD_TRACE("RHfE",r,le32_to_cpu(r->rxd_rbadr),0) ;
}

void mac_drv_clear_rx_queue(struct s_smc *smc)
{
	struct s_smt_fp_rxd volatile *r ;
	struct s_smt_fp_rxd volatile *next_rxd ;
	struct s_smt_rx_queue *queue ;
	int frag_count ;
	int i ;

	if (smc->hw.hw_state != STOPPED) {
		SK_BREAK() ;
		SMT_PANIC(smc,HWM_E0012,HWM_E0012_MSG) ;
		return ;
	}

	queue = smc->hw.fp.rx[QUEUE_R1] ;
	DB_RX("clear_rx_queue",0,0,5) ;

	r = queue->rx_curr_get ;
	while (queue->rx_used) {
		DRV_BUF_FLUSH(r,DDI_DMA_SYNC_FORCPU) ;
		DB_RX("switch OWN bit of RxD 0x%x ",r,0,5) ;
		r->rxd_rbctrl &= ~cpu_to_le32(BMU_OWN) ;
		frag_count = 1 ;
		DRV_BUF_FLUSH(r,DDI_DMA_SYNC_FORDEV) ;
		r = r->rxd_next ;
		DRV_BUF_FLUSH(r,DDI_DMA_SYNC_FORCPU) ;
		while (r != queue->rx_curr_put &&
			!(r->rxd_rbctrl & cpu_to_le32(BMU_ST_BUF))) {
			DB_RX("Check STF bit in %x",(void *)r,0,5) ;
			r->rxd_rbctrl &= ~cpu_to_le32(BMU_OWN) ;
			DRV_BUF_FLUSH(r,DDI_DMA_SYNC_FORDEV) ;
			r = r->rxd_next ;
			DRV_BUF_FLUSH(r,DDI_DMA_SYNC_FORCPU) ;
			frag_count++ ;
		}
		DB_RX("STF bit found",0,0,5) ;
		next_rxd = r ;

		for (r=queue->rx_curr_get,i=frag_count; i ; r=r->rxd_next,i--){
			DB_RX("dma_complete for RxD %x",(void *)r,0,5) ;
			dma_complete(smc,(union s_fp_descr volatile *)r,DMA_WR);
		}

		DB_RX("mac_drv_clear_rxd: RxD %x frag_count %d ",
			(void *)queue->rx_curr_get,frag_count,5) ;
		mac_drv_clear_rxd(smc,queue->rx_curr_get,frag_count) ;

		queue->rx_curr_get = next_rxd ;
		queue->rx_used -= frag_count ;
		queue->rx_free += frag_count ;
	}
}



int hwm_tx_init(struct s_smc *smc, u_char fc, int frag_count, int frame_len,
		int frame_status)
{
	NDD_TRACE("THiB",fc,frag_count,frame_len) ;
	smc->os.hwm.tx_p = smc->hw.fp.tx[frame_status & QUEUE_A0] ;
	smc->os.hwm.tx_descr = TX_DESCRIPTOR | (((u_long)(frame_len-1)&3)<<27) ;
	smc->os.hwm.tx_len = frame_len ;
	DB_TX("hwm_tx_init: fc = %x, len = %d",fc,frame_len,3) ;
	if ((fc & ~(FC_SYNC_BIT|FC_LLC_PRIOR)) == FC_ASYNC_LLC) {
		frame_status |= LAN_TX ;
	}
	else {
		switch (fc) {
		case FC_SMT_INFO :
		case FC_SMT_NSA :
			frame_status |= LAN_TX ;
			break ;
		case FC_SMT_LOC :
			frame_status |= LOC_TX ;
			break ;
		case FC_SMT_LAN_LOC :
			frame_status |= LAN_TX | LOC_TX ;
			break ;
		default :
			SMT_PANIC(smc,HWM_E0010,HWM_E0010_MSG) ;
		}
	}
	if (!smc->hw.mac_ring_is_up) {
		frame_status &= ~LAN_TX ;
		frame_status |= RING_DOWN ;
		DB_TX("Ring is down: terminate LAN_TX",0,0,2) ;
	}
	if (frag_count > smc->os.hwm.tx_p->tx_free) {
#ifndef	NDIS_OS2
		mac_drv_clear_txd(smc) ;
		if (frag_count > smc->os.hwm.tx_p->tx_free) {
			DB_TX("Out of TxDs, terminate LAN_TX",0,0,2) ;
			frame_status &= ~LAN_TX ;
			frame_status |= OUT_OF_TXD ;
		}
#else
		DB_TX("Out of TxDs, terminate LAN_TX",0,0,2) ;
		frame_status &= ~LAN_TX ;
		frame_status |= OUT_OF_TXD ;
#endif
	}
	DB_TX("frame_status = %x",frame_status,0,3) ;
	NDD_TRACE("THiE",frame_status,smc->os.hwm.tx_p->tx_free,0) ;
	return frame_status;
}

void hwm_tx_frag(struct s_smc *smc, char far *virt, u_long phys, int len,
		 int frame_status)
{
	struct s_smt_fp_txd volatile *t ;
	struct s_smt_tx_queue *queue ;
	__le32	tbctrl ;

	queue = smc->os.hwm.tx_p ;

	NDD_TRACE("THfB",virt,len,frame_status) ;
	t = queue->tx_curr_put ;

	DB_TX("hwm_tx_frag: len = %d, frame_status = %x ",len,frame_status,2) ;
	if (frame_status & LAN_TX) {
		
		DB_TX("LAN_TX: TxD = %x, virt = %x ",t,virt,3) ;
		t->txd_virt = virt ;
		t->txd_txdscr = cpu_to_le32(smc->os.hwm.tx_descr) ;
		t->txd_tbadr = cpu_to_le32(phys) ;
		tbctrl = cpu_to_le32((((__u32)frame_status &
			(FIRST_FRAG|LAST_FRAG|EN_IRQ_EOF))<< 26) |
			BMU_OWN|BMU_CHECK |len) ;
		t->txd_tbctrl = tbctrl ;

#ifndef	AIX
		DRV_BUF_FLUSH(t,DDI_DMA_SYNC_FORDEV) ;
		outpd(queue->tx_bmu_ctl,CSR_START) ;
#else	
		DRV_BUF_FLUSH(t,DDI_DMA_SYNC_FORDEV) ;
		if (frame_status & QUEUE_A0) {
			outpd(ADDR(B0_XA_CSR),CSR_START) ;
		}
		else {
			outpd(ADDR(B0_XS_CSR),CSR_START) ;
		}
#endif
		queue->tx_free-- ;
		queue->tx_used++ ;
		queue->tx_curr_put = t->txd_next ;
		if (frame_status & LAST_FRAG) {
			smc->mib.m[MAC0].fddiMACTransmit_Ct++ ;
		}
	}
	if (frame_status & LOC_TX) {
		DB_TX("LOC_TX: ",0,0,3) ;
		if (frame_status & FIRST_FRAG) {
			if(!(smc->os.hwm.tx_mb = smt_get_mbuf(smc))) {
				smc->hw.fp.err_stats.err_no_buf++ ;
				DB_TX("No SMbuf; transmit terminated",0,0,4) ;
			}
			else {
				smc->os.hwm.tx_data =
					smtod(smc->os.hwm.tx_mb,char *) - 1 ;
#ifdef USE_OS_CPY
#ifdef PASS_1ST_TXD_2_TX_COMP
				hwm_cpy_txd2mb(t,smc->os.hwm.tx_data,
					smc->os.hwm.tx_len) ;
#endif
#endif
			}
		}
		if (smc->os.hwm.tx_mb) {
#ifndef	USE_OS_CPY
			DB_TX("copy fragment into MBuf ",0,0,3) ;
			memcpy(smc->os.hwm.tx_data,virt,len) ;
			smc->os.hwm.tx_data += len ;
#endif
			if (frame_status & LAST_FRAG) {
#ifdef	USE_OS_CPY
#ifndef PASS_1ST_TXD_2_TX_COMP
				/*
				 * hwm_cpy_txd2mb(txd,data,len) copies 'len' 
				 * bytes from the virtual pointer in 'rxd'
				 * to 'data'. The virtual pointer of the 
				 * os-specific tx-buffer should be written
				 * in the LAST txd.
				 */ 
				hwm_cpy_txd2mb(t,smc->os.hwm.tx_data,
					smc->os.hwm.tx_len) ;
#endif	
#endif	
				smc->os.hwm.tx_data =
					smtod(smc->os.hwm.tx_mb,char *) - 1 ;
				*(char *)smc->os.hwm.tx_mb->sm_data =
					*smc->os.hwm.tx_data ;
				smc->os.hwm.tx_data++ ;
				smc->os.hwm.tx_mb->sm_len =
					smc->os.hwm.tx_len - 1 ;
				DB_TX("pass LLC frame to SMT ",0,0,3) ;
				smt_received_pack(smc,smc->os.hwm.tx_mb,
						RD_FS_LOCAL) ;
			}
		}
	}
	NDD_TRACE("THfE",t,queue->tx_free,0) ;
}


static void queue_llc_rx(struct s_smc *smc, SMbuf *mb)
{
	DB_GEN("queue_llc_rx: mb = %x",(void *)mb,0,4) ;
	smc->os.hwm.queued_rx_frames++ ;
	mb->sm_next = (SMbuf *)NULL ;
	if (smc->os.hwm.llc_rx_pipe == NULL) {
		smc->os.hwm.llc_rx_pipe = mb ;
	}
	else {
		smc->os.hwm.llc_rx_tail->sm_next = mb ;
	}
	smc->os.hwm.llc_rx_tail = mb ;

	if (!smc->os.hwm.isr_flag) {
		smt_force_irq(smc) ;
	}
}

static SMbuf *get_llc_rx(struct s_smc *smc)
{
	SMbuf	*mb ;

	if ((mb = smc->os.hwm.llc_rx_pipe)) {
		smc->os.hwm.queued_rx_frames-- ;
		smc->os.hwm.llc_rx_pipe = mb->sm_next ;
	}
	DB_GEN("get_llc_rx: mb = 0x%x",(void *)mb,0,4) ;
	return mb;
}

static void queue_txd_mb(struct s_smc *smc, SMbuf *mb)
{
	DB_GEN("_rx: queue_txd_mb = %x",(void *)mb,0,4) ;
	smc->os.hwm.queued_txd_mb++ ;
	mb->sm_next = (SMbuf *)NULL ;
	if (smc->os.hwm.txd_tx_pipe == NULL) {
		smc->os.hwm.txd_tx_pipe = mb ;
	}
	else {
		smc->os.hwm.txd_tx_tail->sm_next = mb ;
	}
	smc->os.hwm.txd_tx_tail = mb ;
}

static SMbuf *get_txd_mb(struct s_smc *smc)
{
	SMbuf *mb ;

	if ((mb = smc->os.hwm.txd_tx_pipe)) {
		smc->os.hwm.queued_txd_mb-- ;
		smc->os.hwm.txd_tx_pipe = mb->sm_next ;
	}
	DB_GEN("get_txd_mb: mb = 0x%x",(void *)mb,0,4) ;
	return mb;
}

void smt_send_mbuf(struct s_smc *smc, SMbuf *mb, int fc)
{
	char far *data ;
	int	len ;
	int	n ;
	int	i ;
	int	frag_count ;
	int	frame_status ;
	SK_LOC_DECL(char far,*virt[3]) ;
	int	frag_len[3] ;
	struct s_smt_tx_queue *queue ;
	struct s_smt_fp_txd volatile *t ;
	u_long	phys ;
	__le32	tbctrl;

	NDD_TRACE("THSB",mb,fc,0) ;
	DB_TX("smt_send_mbuf: mb = 0x%x, fc = 0x%x",mb,fc,4) ;

	mb->sm_off-- ;	
	mb->sm_len++ ;	
	data = smtod(mb,char *) ;
	*data = fc ;
	if (fc == FC_SMT_LOC)
		*data = FC_SMT_INFO ;

	frag_count = 0 ;
	len = mb->sm_len ;
	while (len) {
		n = SMT_PAGESIZE - ((long)data & (SMT_PAGESIZE-1)) ;
		if (n >= len) {
			n = len ;
		}
		DB_TX("frag: virt/len = 0x%x/%d ",(void *)data,n,5) ;
		virt[frag_count] = data ;
		frag_len[frag_count] = n ;
		frag_count++ ;
		len -= n ;
		data += n ;
	}

	queue = smc->hw.fp.tx[QUEUE_A0] ;
	if (fc == FC_BEACON || fc == FC_SMT_LOC) {
		frame_status = LOC_TX ;
	}
	else {
		frame_status = LAN_TX ;
		if ((smc->os.hwm.pass_NSA &&(fc == FC_SMT_NSA)) ||
		   (smc->os.hwm.pass_SMT &&(fc == FC_SMT_INFO)))
			frame_status |= LOC_TX ;
	}

	if (!smc->hw.mac_ring_is_up || frag_count > queue->tx_free) {
		frame_status &= ~LAN_TX;
		if (frame_status) {
			DB_TX("Ring is down: terminate LAN_TX",0,0,2) ;
		}
		else {
			DB_TX("Ring is down: terminate transmission",0,0,2) ;
			smt_free_mbuf(smc,mb) ;
			return ;
		}
	}
	DB_TX("frame_status = 0x%x ",frame_status,0,5) ;

	if ((frame_status & LAN_TX) && (frame_status & LOC_TX)) {
		mb->sm_use_count = 2 ;
	}

	if (frame_status & LAN_TX) {
		t = queue->tx_curr_put ;
		frame_status |= FIRST_FRAG ;
		for (i = 0; i < frag_count; i++) {
			DB_TX("init TxD = 0x%x",(void *)t,0,5) ;
			if (i == frag_count-1) {
				frame_status |= LAST_FRAG ;
				t->txd_txdscr = cpu_to_le32(TX_DESCRIPTOR |
					(((__u32)(mb->sm_len-1)&3) << 27)) ;
			}
			t->txd_virt = virt[i] ;
			phys = dma_master(smc, (void far *)virt[i],
				frag_len[i], DMA_RD|SMT_BUF) ;
			t->txd_tbadr = cpu_to_le32(phys) ;
			tbctrl = cpu_to_le32((((__u32)frame_status &
				(FIRST_FRAG|LAST_FRAG)) << 26) |
				BMU_OWN | BMU_CHECK | BMU_SMT_TX |frag_len[i]) ;
			t->txd_tbctrl = tbctrl ;
#ifndef	AIX
			DRV_BUF_FLUSH(t,DDI_DMA_SYNC_FORDEV) ;
			outpd(queue->tx_bmu_ctl,CSR_START) ;
#else
			DRV_BUF_FLUSH(t,DDI_DMA_SYNC_FORDEV) ;
			outpd(ADDR(B0_XA_CSR),CSR_START) ;
#endif
			frame_status &= ~FIRST_FRAG ;
			queue->tx_curr_put = t = t->txd_next ;
			queue->tx_free-- ;
			queue->tx_used++ ;
		}
		smc->mib.m[MAC0].fddiMACTransmit_Ct++ ;
		queue_txd_mb(smc,mb) ;
	}

	if (frame_status & LOC_TX) {
		DB_TX("pass Mbuf to LLC queue",0,0,5) ;
		queue_llc_rx(smc,mb) ;
	}

	mac_drv_clear_txd(smc) ;
	NDD_TRACE("THSE",t,queue->tx_free,frag_count) ;
}

static void mac_drv_clear_txd(struct s_smc *smc)
{
	struct s_smt_tx_queue *queue ;
	struct s_smt_fp_txd volatile *t1 ;
	struct s_smt_fp_txd volatile *t2 = NULL ;
	SMbuf *mb ;
	u_long	tbctrl ;
	int i ;
	int frag_count ;
	int n ;

	NDD_TRACE("THcB",0,0,0) ;
	for (i = QUEUE_S; i <= QUEUE_A0; i++) {
		queue = smc->hw.fp.tx[i] ;
		t1 = queue->tx_curr_get ;
		DB_TX("clear_txd: QUEUE = %d (0=sync/1=async)",i,0,5) ;

		for ( ; ; ) {
			frag_count = 0 ;

			do {
				DRV_BUF_FLUSH(t1,DDI_DMA_SYNC_FORCPU) ;
				DB_TX("check OWN/EOF bit of TxD 0x%x",t1,0,5) ;
				tbctrl = le32_to_cpu(CR_READ(t1->txd_tbctrl));

				if (tbctrl & BMU_OWN || !queue->tx_used){
					DB_TX("End of TxDs queue %d",i,0,4) ;
					goto free_next_queue ;	
				}
				t1 = t1->txd_next ;
				frag_count++ ;
			} while (!(tbctrl & BMU_EOF)) ;

			t1 = queue->tx_curr_get ;
			for (n = frag_count; n; n--) {
				tbctrl = le32_to_cpu(t1->txd_tbctrl) ;
				dma_complete(smc,
					(union s_fp_descr volatile *) t1,
					(int) (DMA_RD |
					((tbctrl & BMU_SMT_TX) >> 18))) ;
				t2 = t1 ;
				t1 = t1->txd_next ;
			}

			if (tbctrl & BMU_SMT_TX) {
				mb = get_txd_mb(smc) ;
				smt_free_mbuf(smc,mb) ;
			}
			else {
#ifndef PASS_1ST_TXD_2_TX_COMP
				DB_TX("mac_drv_tx_comp for TxD 0x%x",t2,0,4) ;
				mac_drv_tx_complete(smc,t2) ;
#else
				DB_TX("mac_drv_tx_comp for TxD 0x%x",
					queue->tx_curr_get,0,4) ;
				mac_drv_tx_complete(smc,queue->tx_curr_get) ;
#endif
			}
			queue->tx_curr_get = t1 ;
			queue->tx_free += frag_count ;
			queue->tx_used -= frag_count ;
		}
free_next_queue: ;
	}
	NDD_TRACE("THcE",0,0,0) ;
}

void mac_drv_clear_tx_queue(struct s_smc *smc)
{
	struct s_smt_fp_txd volatile *t ;
	struct s_smt_tx_queue *queue ;
	int tx_used ;
	int i ;

	if (smc->hw.hw_state != STOPPED) {
		SK_BREAK() ;
		SMT_PANIC(smc,HWM_E0011,HWM_E0011_MSG) ;
		return ;
	}

	for (i = QUEUE_S; i <= QUEUE_A0; i++) {
		queue = smc->hw.fp.tx[i] ;
		DB_TX("clear_tx_queue: QUEUE = %d (0=sync/1=async)",i,0,5) ;

		t = queue->tx_curr_get ;
		tx_used = queue->tx_used ;
		while (tx_used) {
			DRV_BUF_FLUSH(t,DDI_DMA_SYNC_FORCPU) ;
			DB_TX("switch OWN bit of TxD 0x%x ",t,0,5) ;
			t->txd_tbctrl &= ~cpu_to_le32(BMU_OWN) ;
			DRV_BUF_FLUSH(t,DDI_DMA_SYNC_FORDEV) ;
			t = t->txd_next ;
			tx_used-- ;
		}
	}

	mac_drv_clear_txd(smc) ;

	for (i = QUEUE_S; i <= QUEUE_A0; i++) {
		queue = smc->hw.fp.tx[i] ;
		t = queue->tx_curr_get ;

		if (i == QUEUE_S) {
			outpd(ADDR(B5_XS_DA),le32_to_cpu(t->txd_ntdadr)) ;
		}
		else {
			outpd(ADDR(B5_XA_DA),le32_to_cpu(t->txd_ntdadr)) ;
		}

		queue->tx_curr_put = queue->tx_curr_get->txd_next ;
		queue->tx_curr_get = queue->tx_curr_put ;
	}
}



#ifdef	DEBUG
void mac_drv_debug_lev(struct s_smc *smc, int flag, int lev)
{
	switch(flag) {
	case (int)NULL:
		DB_P.d_smtf = DB_P.d_smt = DB_P.d_ecm = DB_P.d_rmt = 0 ;
		DB_P.d_cfm = 0 ;
		DB_P.d_os.hwm_rx = DB_P.d_os.hwm_tx = DB_P.d_os.hwm_gen = 0 ;
#ifdef	SBA
		DB_P.d_sba = 0 ;
#endif
#ifdef	ESS
		DB_P.d_ess = 0 ;
#endif
		break ;
	case DEBUG_SMTF:
		DB_P.d_smtf = lev ;
		break ;
	case DEBUG_SMT:
		DB_P.d_smt = lev ;
		break ;
	case DEBUG_ECM:
		DB_P.d_ecm = lev ;
		break ;
	case DEBUG_RMT:
		DB_P.d_rmt = lev ;
		break ;
	case DEBUG_CFM:
		DB_P.d_cfm = lev ;
		break ;
	case DEBUG_PCM:
		DB_P.d_pcm = lev ;
		break ;
	case DEBUG_SBA:
#ifdef	SBA
		DB_P.d_sba = lev ;
#endif
		break ;
	case DEBUG_ESS:
#ifdef	ESS
		DB_P.d_ess = lev ;
#endif
		break ;
	case DB_HWM_RX:
		DB_P.d_os.hwm_rx = lev ;
		break ;
	case DB_HWM_TX:
		DB_P.d_os.hwm_tx = lev ;
		break ;
	case DB_HWM_GEN:
		DB_P.d_os.hwm_gen = lev ;
		break ;
	default:
		break ;
	}
}
#endif
