/* $Id: bkm_ax.h,v 1.5.6.3 2001/09/23 22:24:46 kai Exp $
 *
 * low level decls for T-Berkom cards A4T and Scitel Quadro (4*S0, passive)
 *
 * Author       Roland Klabunde
 * Copyright    by Roland Klabunde   <R.Klabunde@Berkom.de>
 *
 * This software may be used and distributed according to the terms
 * of the GNU General Public License, incorporated herein by reference.
 *
 */

#ifndef	__BKM_AX_H__
#define	__BKM_AX_H__

#define SCT_1		1
#define	SCT_2		2
#define	SCT_3		3
#define	SCT_4		4
#define BKM_A4T		5

#define	PLX_ADDR_PLX		0x14	
#define	PLX_ADDR_ISAC		0x18	
#define	PLX_ADDR_HSCX		0x1C	
#define	PLX_ADDR_ALE		0x20	
#define	PLX_ADDR_ALEPLUS	0x24	

#define	PLX_SUBVEN		0x2C	
#define	PLX_SUBSYS		0x2E	


typedef	struct {
	
	volatile u_int	i20VFEHorzCfg;	
	
	volatile u_int	i20VFEVertCfg;	
	
	volatile u_int	i20VFEScaler;	
	
	volatile u_int	i20VDispTop;	
	
	volatile u_int	i20VDispBottom;	
	
	volatile u_int	i20VidFrameGrab;
	
	volatile u_int	i20VDispCfg;	
	
	volatile u_int	i20VMaskTop;	
	
	volatile u_int	i20VMaskBottom;	
	
	volatile u_int	i20OvlyControl;	
	
	volatile u_int	i20SysControl;	
#define	sysRESET		0x01000000	
	
#define	sysCFG			0x000000E0	
	
	volatile u_int	i20GuestControl;
#define	guestWAIT_CFG	0x00005555	
#define	guestISDN_INT_E	0x01000000	
#define	guestVID_INT_E	0x02000000	
#define	guestADI1_INT_R	0x04000000	
#define	guestADI2_INT_R	0x08000000	
#define	guestISDN_RES	0x10000000	
#define	guestADI1_INT_S	0x20000000	
#define	guestADI2_INT_S	0x40000000	
#define	guestISDN_INT_S	0x80000000	

#define	g_A4T_JADE_RES	0x01000000	
#define	g_A4T_ISAR_RES	0x02000000	
#define	g_A4T_ISAC_RES	0x04000000	
#define	g_A4T_JADE_BOOTR 0x08000000	
#define	g_A4T_ISAR_BOOTR 0x10000000	
#define	g_A4T_JADE_INT_S 0x20000000	
#define	g_A4T_ISAR_INT_S 0x40000000	
#define	g_A4T_ISAC_INT_S 0x80000000	

	volatile u_int	i20CodeSource;	
	volatile u_int	i20CodeXferCtrl;
	volatile u_int	i20CodeMemPtr;	

	volatile u_int	i20IntStatus;	
	volatile u_int	i20IntCtrl;	
#define	intISDN		0x40000000	
#define	intVID		0x20000000	
#define	intCOD		0x10000000	
#define	intPCI		0x01000000	

	volatile u_int	i20I2CCtrl;	
} I20_REGISTER_FILE, *PI20_REGISTER_FILE;

#define	PO_OFFSET	0x00000200	

#define	GCS_0		0x00000000	
#define	GCS_1		0x00100000
#define	GCS_2		0x00200000
#define	GCS_3		0x00300000

#define	PO_READ		0x00000000	
#define	PO_WRITE	0x00800000

#define	PO_PEND		0x02000000

#define POSTOFFICE(postoffice) *(volatile unsigned int *)(postoffice)

#define	__WAITI20__(postoffice)					\
	do {							\
		while ((POSTOFFICE(postoffice) & PO_PEND)) ;	\
	} while (0)

#endif	
