#ifndef _LINUX_FDREG_H
#define _LINUX_FDREG_H



#define FDCSELREG_STP   (0x80)   
#define FDCSELREG_TRA   (0x82)   
#define FDCSELREG_SEC   (0x84)   
#define FDCSELREG_DTA   (0x86)   


#define FDCREG_CMD		0
#define FDCREG_STATUS	0
#define FDCREG_TRACK	2
#define FDCREG_SECTOR	4
#define FDCREG_DATA		6


#define FDCCMD_RESTORE  (0x00)   
#define FDCCMD_SEEK     (0x10)   
#define FDCCMD_STEP     (0x20)   
#define FDCCMD_STIN     (0x40)   
#define FDCCMD_STOT     (0x60)   
#define FDCCMD_RDSEC    (0x80)   
#define FDCCMD_WRSEC    (0xa0)   
#define FDCCMD_RDADR    (0xc0)   
#define FDCCMD_RDTRA    (0xe0)   
#define FDCCMD_WRTRA    (0xf0)   
#define FDCCMD_FORCI    (0xd0)   


#define FDCCMDADD_SR6   (0x00)   
#define FDCCMDADD_SR12  (0x01)
#define FDCCMDADD_SR2   (0x02)
#define FDCCMDADD_SR3   (0x03)
#define FDCCMDADD_V     (0x04)   
#define FDCCMDADD_H     (0x08)   
#define FDCCMDADD_U     (0x10)   
#define FDCCMDADD_M     (0x10)   
#define FDCCMDADD_E     (0x04)   
#define FDCCMDADD_P     (0x02)   
#define FDCCMDADD_A0    (0x01)   


#define	FDCSTAT_MOTORON	(0x80)   
#define	FDCSTAT_WPROT	(0x40)   
#define	FDCSTAT_SPINUP	(0x20)   
#define	FDCSTAT_DELDAM	(0x20)   
#define	FDCSTAT_RECNF	(0x10)   
#define	FDCSTAT_CRC		(0x08)   
#define	FDCSTAT_TR00	(0x04)   
#define	FDCSTAT_LOST	(0x04)   
#define	FDCSTAT_IDX		(0x02)   
#define	FDCSTAT_DRQ		(0x02)   
#define	FDCSTAT_BUSY	(0x01)   


#define DSKSIDE     (0x01)

#define DSKDRVNONE  (0x06)
#define DSKDRV0     (0x02)
#define DSKDRV1     (0x04)

#define	FDCSTEP_6	0x00
#define	FDCSTEP_12	0x01
#define	FDCSTEP_2	0x02
#define	FDCSTEP_3	0x03

#endif
