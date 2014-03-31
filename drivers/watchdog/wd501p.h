/*
 *	Industrial Computer Source WDT500/501 driver
 *
 *	(c) Copyright 1995	CymruNET Ltd
 *				Innovation Centre
 *				Singleton Park
 *				Swansea
 *				Wales
 *				UK
 *				SA2 8PP
 *
 *	http://www.cymru.net
 *
 *	This driver is provided under the GNU General Public License,
 *	incorporated herein by reference. The driver is provided without
 *	warranty or support.
 *
 *	Release 0.04.
 *
 */


#define WDT_COUNT0		(io+0)
#define WDT_COUNT1		(io+1)
#define WDT_COUNT2		(io+2)
#define WDT_CR			(io+3)
#define WDT_SR			(io+4)	
#define WDT_RT			(io+5)	
#define WDT_BUZZER		(io+6)	
#define WDT_DC			(io+7)

#define WDT_CLOCK		(io+12)	
#define WDT_OPTONOTRST		(io+13)	
#define WDT_OPTORST		(io+14)	
#define WDT_PROGOUT		(io+15)	

							 
#define WDC_SR_WCCR		1	 
#define WDC_SR_TGOOD		2			 
#define WDC_SR_ISOI0		4			 
#define WDC_SR_ISII1		8			 
#define WDC_SR_FANGOOD		16			 
#define WDC_SR_PSUOVER		32	 
#define WDC_SR_PSUUNDR		64	 
#define WDC_SR_IRQ		128	 

