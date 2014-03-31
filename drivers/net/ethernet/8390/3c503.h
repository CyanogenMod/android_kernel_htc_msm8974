/* This file is distributed under the GPL.
   Many of these names and comments are directly from the Crynwr packet
   drivers, which are released under the GPL. */

#define EL2H (dev->base_addr + 0x400)
#define EL2L (dev->base_addr)


#define OLD_3COM_ID	0x02608c
#define NEW_3COM_ID	0x0020af


#define EL2_MB0_START_PG	(0x00)	
#define EL2_MB1_START_PG	(0x20)	
#define EL2_MB1_STOP_PG		(0x40)	

#define E33G_STARTPG	(EL2H+0)	
#define E33G_STOPPG	(EL2H+1)	
#define E33G_DRQCNT	(EL2H+2)	
#define E33G_IOBASE	(EL2H+3)	
	
#define E33G_ROMBASE	(EL2H+4)	
#define E33G_GACFR	(EL2H+5)	
#define E33G_CNTRL	(EL2H+6)	
#define E33G_STATUS	(EL2H+7)	
#define E33G_IDCFR	(EL2H+8)	
				
#define E33G_DMAAH	(EL2H+9)	
#define E33G_DMAAL	(EL2H+10)	
#define E33G_VP2	(EL2H+11)
#define E33G_VP1	(EL2H+12)
#define E33G_VP0	(EL2H+13)
#define E33G_FIFOH	(EL2H+14)	
#define E33G_FIFOL	(EL2H+15)	


#define ECNTRL_RESET	(0x01)	
#define ECNTRL_THIN	(0x02)	
#define ECNTRL_AUI	(0x00)	
#define ECNTRL_SAPROM	(0x04)	
#define ECNTRL_DBLBFR	(0x20)	
#define ECNTRL_OUTPUT	(0x40)	
#define ECNTRL_INPUT	(0x00)	
#define ECNTRL_START	(0x80)	


#define ESTAT_DPRDY	(0x80)	
#define ESTAT_UFLW	(0x40)	
#define ESTAT_OFLW	(0x20)	
#define ESTAT_DTC	(0x10)	
#define ESTAT_DIP	(0x08)	


#define EGACFR_NIM	(0x80)	
#define EGACFR_TCM	(0x40)	
#define EGACFR_RSEL	(0x08)	
#define EGACFR_MBS2	(0x04)	
#define EGACFR_MBS1	(0x02)	
#define EGACFR_MBS0	(0x01)	

#define EGACFR_NORM	(0x49)	
#define EGACFR_IRQOFF	(0xc9)	



