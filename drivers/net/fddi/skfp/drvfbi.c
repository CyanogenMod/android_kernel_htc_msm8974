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


#include "h/types.h"
#include "h/fddi.h"
#include "h/smc.h"
#include "h/supern_2.h"
#include "h/skfbiinc.h"
#include <linux/bitrev.h>

#ifndef	lint
static const char ID_sccs[] = "@(#)drvfbi.c	1.63 99/02/11 (C) SK " ;
#endif

#define PC8_ACTIVE	8

#define	LED_Y_ON	0x11	
#define	LED_Y_OFF	0x10


#define MS2BCLK(x)	((x)*12500L)


#ifndef MULT_OEM
#ifndef	OEM_CONCEPT
const u_char oem_id[] = "xPOS_ID:xxxx" ;
#else	
const u_char oem_id[] = OEM_ID ;
#endif	
#define	ID_BYTE0	8
#define	OEMID(smc,i)	oem_id[ID_BYTE0 + i]
#else	
const struct s_oem_ids oem_ids[] = {
#include "oemids.h"
{0}
};
#define	OEMID(smc,i)	smc->hw.oem_id->oi_id[i]
#endif	

#ifdef AIX
extern int AIX_vpdReadByte() ;
#endif


static void smt_stop_watchdog(struct s_smc *smc);

static void card_start(struct s_smc *smc)
{
	int i ;
#ifdef	PCI
	u_char	rev_id ;
	u_short word;
#endif

	smt_stop_watchdog(smc) ;

#ifdef	PCI
	outpw(FM_A(FM_MDREG1),FM_MINIT) ;
	outp(ADDR(B0_CTRL), CTRL_HPI_SET) ;
	hwt_wait_time(smc,hwt_quick_read(smc),MS2BCLK(10)) ;
	outp(ADDR(B0_CTRL),CTRL_RST_SET) ;	
	i = (int) inp(ADDR(B0_CTRL)) ;		
	SK_UNUSED(i) ;				
	outp(ADDR(B0_CTRL), CTRL_RST_CLR) ;

	outp(ADDR(B0_TST_CTRL), TST_CFG_WRITE_ON) ;	
	word = inpw(PCI_C(PCI_STATUS)) ;
	outpw(PCI_C(PCI_STATUS), word | PCI_ERRBITS) ;
	outp(ADDR(B0_TST_CTRL), TST_CFG_WRITE_OFF) ;	

	outp(ADDR(B0_CTRL), CTRL_MRST_CLR|CTRL_HPI_CLR) ;

	rev_id = inp(PCI_C(PCI_REV_ID)) ;
	if ((rev_id & 0xf0) == SK_ML_ID_1 || (rev_id & 0xf0) == SK_ML_ID_2) {
		smc->hw.hw_is_64bit = TRUE ;
	} else {
		smc->hw.hw_is_64bit = FALSE ;
	}

	if (!smc->hw.hw_is_64bit) {
		outpd(ADDR(B4_R1_F), RX_WATERMARK) ;
		outpd(ADDR(B5_XA_F), TX_WATERMARK) ;
		outpd(ADDR(B5_XS_F), TX_WATERMARK) ;
	}

	outp(ADDR(B0_CTRL),CTRL_RST_CLR) ;	
	outp(ADDR(B0_LED),LED_GA_OFF|LED_MY_ON|LED_GB_OFF) ; 

	
	outpd(ADDR(B2_WDOG_INI),0x6FC23AC0) ;

	
	smc->hw.is_imask = ISR_MASK ;
	smc->hw.hw_state = STOPPED ;
#endif
	GET_PAGE(0) ;		
}

void card_stop(struct s_smc *smc)
{
	smt_stop_watchdog(smc) ;
	smc->hw.mac_ring_is_up = 0 ;		

#ifdef	PCI
	outpw(FM_A(FM_MDREG1),FM_MINIT) ;
	outp(ADDR(B0_CTRL), CTRL_HPI_SET) ;
	hwt_wait_time(smc,hwt_quick_read(smc),MS2BCLK(10)) ;
	outp(ADDR(B0_CTRL),CTRL_RST_SET) ;	
	outp(ADDR(B0_CTRL),CTRL_RST_CLR) ;	
	outp(ADDR(B0_LED),LED_GA_OFF|LED_MY_OFF|LED_GB_OFF) ; 
	smc->hw.hw_state = STOPPED ;
#endif
}

void mac1_irq(struct s_smc *smc, u_short stu, u_short stl)
{
	int	restart_tx = 0 ;
again:

	if (stl & (FM_SPCEPDS  |	
		   FM_SPCEPDA0 |	
		   FM_SPCEPDA1)) {	
		SMT_PANIC(smc,SMT_E0134, SMT_E0134_MSG) ;
	}
	if (stl & (FM_STBURS  |		
		   FM_STBURA0 |		
		   FM_STBURA1)) {	
		SMT_PANIC(smc,SMT_E0133, SMT_E0133_MSG) ;
	}

	if ( (stu & (FM_SXMTABT |		
		     FM_STXABRS |		
		     FM_STXABRA0)) ||		
	     (stl & (FM_SQLCKS |		
		     FM_SQLCKA0)) ) {		
		formac_tx_restart(smc) ;	
		restart_tx = 1 ;
		stu = inpw(FM_A(FM_ST1U)) ;
		stl = inpw(FM_A(FM_ST1L)) ;
		stu &= ~ (FM_STECFRMA0 | FM_STEFRMA0 | FM_STEFRMS) ;
		if (stu || stl)
			goto again ;
	}

	if (stu & (FM_STEFRMA0 |	
		    FM_STEFRMS)) {	
		restart_tx = 1 ;
	}

	if (restart_tx)
		llc_restart_tx(smc) ;
}

void plc1_irq(struct s_smc *smc)
{
	u_short	st = inpw(PLC(PB,PL_INTR_EVENT)) ;

	plc_irq(smc,PB,st) ;
}

void plc2_irq(struct s_smc *smc)
{
	u_short	st = inpw(PLC(PA,PL_INTR_EVENT)) ;

	plc_irq(smc,PA,st) ;
}


void timer_irq(struct s_smc *smc)
{
	hwt_restart(smc);
	smc->hw.t_stop = smc->hw.t_start;
	smt_timer_done(smc) ;
}

int pcm_get_s_port(struct s_smc *smc)
{
	SK_UNUSED(smc) ;
	return PS;
}

#define STATION_LABEL_CONNECTOR_OFFSET	5
#define STATION_LABEL_PMD_OFFSET	6
#define STATION_LABEL_PORT_OFFSET	7

void read_address(struct s_smc *smc, u_char *mac_addr)
{
	char ConnectorType ;
	char PmdType ;
	int	i ;

#ifdef	PCI
	for (i = 0; i < 6; i++) {	
		smc->hw.fddi_phys_addr.a[i] =
			bitrev8(inp(ADDR(B2_MAC_0+i)));
	}
#endif

	ConnectorType = inp(ADDR(B2_CONN_TYP)) ;
	PmdType = inp(ADDR(B2_PMD_TYP)) ;

	smc->y[PA].pmd_type[PMD_SK_CONN] =
	smc->y[PB].pmd_type[PMD_SK_CONN] = ConnectorType ;
	smc->y[PA].pmd_type[PMD_SK_PMD ] =
	smc->y[PB].pmd_type[PMD_SK_PMD ] = PmdType ;

	if (mac_addr) {
		for (i = 0; i < 6 ;i++) {
			smc->hw.fddi_canon_addr.a[i] = mac_addr[i] ;
			smc->hw.fddi_home_addr.a[i] = bitrev8(mac_addr[i]);
		}
		return ;
	}
	smc->hw.fddi_home_addr = smc->hw.fddi_phys_addr ;

	for (i = 0; i < 6 ;i++) {
		smc->hw.fddi_canon_addr.a[i] =
			bitrev8(smc->hw.fddi_phys_addr.a[i]);
	}
}

void init_board(struct s_smc *smc, u_char *mac_addr)
{
	card_start(smc) ;
	read_address(smc,mac_addr) ;

	if (!(inp(ADDR(B0_DAS)) & DAS_AVAIL))
		smc->s.sas = SMT_SAS ;	
	else
		smc->s.sas = SMT_DAS ;	

	if (!(inp(ADDR(B0_DAS)) & DAS_BYP_ST))
		smc->mib.fddiSMTBypassPresent = 0 ;
		
	else
		smc->mib.fddiSMTBypassPresent = 1 ;
		
}

void sm_pm_bypass_req(struct s_smc *smc, int mode)
{
	DB_ECMN(1,"ECM : sm_pm_bypass_req(%s)\n",(mode == BP_INSERT) ?
					"BP_INSERT" : "BP_DEINSERT",0) ;

	if (smc->s.sas != SMT_DAS)
		return ;

#ifdef	PCI
	switch(mode) {
	case BP_INSERT :
		outp(ADDR(B0_DAS),DAS_BYP_INS) ;	
		break ;
	case BP_DEINSERT :
		outp(ADDR(B0_DAS),DAS_BYP_RMV) ;	
		break ;
	}
#endif
}

int sm_pm_bypass_present(struct s_smc *smc)
{
	return (inp(ADDR(B0_DAS)) & DAS_BYP_ST) ? TRUE : FALSE;
}

void plc_clear_irq(struct s_smc *smc, int p)
{
	SK_UNUSED(p) ;

	SK_UNUSED(smc) ;
}


static void led_indication(struct s_smc *smc, int led_event)
{
	u_short			led_state ;
	struct s_phy		*phy ;
	struct fddi_mib_p	*mib_a ;
	struct fddi_mib_p	*mib_b ;

	phy = &smc->y[PA] ;
	mib_a = phy->mib ;
	phy = &smc->y[PB] ;
	mib_b = phy->mib ;

#ifdef	PCI
        led_state = 0 ;
	
	
	if (led_event == LED_Y_ON) {
		led_state |= LED_MY_ON ;
	}
	else if (led_event == LED_Y_OFF) {
		led_state |= LED_MY_OFF ;
	}
	else {	
		
		if (mib_a->fddiPORTPCMState == PC8_ACTIVE) {	
			led_state |= LED_GA_ON ;
		}
		else {
			led_state |= LED_GA_OFF ;
		}
		
		
		if (mib_b->fddiPORTPCMState == PC8_ACTIVE) {
			led_state |= LED_GB_ON ;
		}
		else {
			led_state |= LED_GB_OFF ;
		}
	}

        outp(ADDR(B0_LED), led_state) ;
#endif	

}


void pcm_state_change(struct s_smc *smc, int plc, int p_state)
{
	DRV_PCM_STATE_CHANGE(smc,plc,p_state) ;
	
	led_indication(smc,0) ;
}


void rmt_indication(struct s_smc *smc, int i)
{
	
	DRV_RMT_INDICATION(smc,i) ;

        led_indication(smc, i ? LED_Y_OFF : LED_Y_ON) ;
}


void llc_recover_tx(struct s_smc *smc)
{
#ifdef	LOAD_GEN
	extern	int load_gen_flag ;

	load_gen_flag = 0 ;
#endif
#ifndef	SYNC
	smc->hw.n_a_send= 0 ;
#else
	SK_UNUSED(smc) ;
#endif
}

#ifdef MULT_OEM
static int is_equal_num(char comp1[], char comp2[], int num)
{
	int i ;

	for (i = 0 ; i < num ; i++) {
		if (comp1[i] != comp2[i])
			return 0;
	}
		return 1;
}	


int set_oi_id_def(struct s_smc *smc)
{
	int sel_id ;
	int i ;
	int act_entries ;

	i = 0 ;
	sel_id = -1 ;
	act_entries = FALSE ;
	smc->hw.oem_id = 0 ;
	smc->hw.oem_min_status = OI_STAT_ACTIVE ;
	
	
	while (oem_ids[i].oi_status) {
		switch (oem_ids[i].oi_status) {
		case OI_STAT_ACTIVE:
			act_entries = TRUE ;	
			if (sel_id == -1)
				sel_id = i ;	
		case OI_STAT_VALID:
		case OI_STAT_PRESENT:
			i++ ;
			break ;			
		default:
			return 1;		
		}
	}

	if (i == 0)
		return 2;
	if (!act_entries)
		return 3;

	
	smc->hw.oem_id = (struct s_oem_ids *)  &oem_ids[sel_id] ;
	return 0;
}
#endif	

void driver_get_bia(struct s_smc *smc, struct fddi_addr *bia_addr)
{
	int i ;

	for (i = 0 ; i < 6 ; i++)
		bia_addr->a[i] = bitrev8(smc->hw.fddi_phys_addr.a[i]);
}

void smt_start_watchdog(struct s_smc *smc)
{
	SK_UNUSED(smc) ;	

#ifndef	DEBUG

#ifdef	PCI
	if (smc->hw.wdog_used) {
		outpw(ADDR(B2_WDOG_CRTL),TIM_START) ;	
	}
#endif

#endif	
}

static void smt_stop_watchdog(struct s_smc *smc)
{
	SK_UNUSED(smc) ;	
#ifndef	DEBUG

#ifdef	PCI
	if (smc->hw.wdog_used) {
		outpw(ADDR(B2_WDOG_CRTL),TIM_STOP) ;	
	}
#endif

#endif	
}

#ifdef	PCI

void mac_do_pci_fix(struct s_smc *smc)
{
	SK_UNUSED(smc) ;
}
#endif	

