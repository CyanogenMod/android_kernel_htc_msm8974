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

#ifndef	lint
static const char ID_sccs[] = "@(#)hwt.c	1.13 97/04/23 (C) SK " ;
#endif


#define	HWT_MAX	(65000)

void hwt_start(struct s_smc *smc, u_long time)
{
	u_short	cnt ;

	if (time > HWT_MAX)
		time = HWT_MAX ;

	smc->hw.t_start = time ;
	smc->hw.t_stop = 0L ;

	cnt = (u_short)time ;
	if (!cnt)
		cnt++ ;

	outpd(ADDR(B2_TI_INI), (u_long) cnt * 200) ;	
	outpw(ADDR(B2_TI_CRTL), TIM_START) ;		

	smc->hw.timer_activ = TRUE ;
}

void hwt_stop(struct s_smc *smc)
{
	outpw(ADDR(B2_TI_CRTL), TIM_STOP) ;
	outpw(ADDR(B2_TI_CRTL), TIM_CL_IRQ) ;

	smc->hw.timer_activ = FALSE ;
}

void hwt_init(struct s_smc *smc)
{
	smc->hw.t_start = 0 ;
	smc->hw.t_stop	= 0 ;
	smc->hw.timer_activ = FALSE ;

	hwt_restart(smc) ;
}

void hwt_restart(struct s_smc *smc)
{
	hwt_stop(smc) ;
}

u_long hwt_read(struct s_smc *smc)
{
	u_short	tr ;
	u_long	is ;

	if (smc->hw.timer_activ) {
		hwt_stop(smc) ;
		tr = (u_short)((inpd(ADDR(B2_TI_VAL))/200) & 0xffff) ;

		is = GET_ISR() ;
		
		if ((tr > smc->hw.t_start) || (is & IS_TIMINT)) {
			hwt_restart(smc) ;
			smc->hw.t_stop = smc->hw.t_start ;
		}
		else
			smc->hw.t_stop = smc->hw.t_start - tr ;
	}
	return smc->hw.t_stop;
}

#ifdef	PCI
u_long hwt_quick_read(struct s_smc *smc)
{
	u_long interval ;
	u_long time ;

	interval = inpd(ADDR(B2_TI_INI)) ;
	outpw(ADDR(B2_TI_CRTL), TIM_STOP) ;
	time = inpd(ADDR(B2_TI_VAL)) ;
	outpd(ADDR(B2_TI_INI),time) ;
	outpw(ADDR(B2_TI_CRTL), TIM_START) ;
	outpd(ADDR(B2_TI_INI),interval) ;

	return time;
}

void hwt_wait_time(struct s_smc *smc, u_long start, long int duration)
{
	long	diff ;
	long	interval ;
	int	wrapped ;

	if (smc->hw.timer_activ == FALSE ||
		hwt_quick_read(smc) == hwt_quick_read(smc)) {
		return ;
	}

	interval = inpd(ADDR(B2_TI_INI)) ;
	if (interval > duration) {
		do {
			diff = (long)(start - hwt_quick_read(smc)) ;
			if (diff < 0) {
				diff += interval ;
			}
		} while (diff <= duration) ;
	}
	else {
		diff = interval ;
		wrapped = 0 ;
		do {
			if (!wrapped) {
				if (hwt_quick_read(smc) >= start) {
					diff += interval ;
					wrapped = 1 ;
				}
			}
			else {
				if (hwt_quick_read(smc) < start) {
					wrapped = 0 ;
				}
			}
		} while (diff <= duration) ;
	}
}
#endif

