#ifndef _ASM_X86_VISWS_PIIX4_H
#define _ASM_X86_VISWS_PIIX4_H


#define	PIIX_PM_START		0x0F80

#define	SIO_GPIO_START		0x0FC0

#define	SIO_PM_START		0x0FC8

#define	PMBASE			PIIX_PM_START
#define	GPIREG0			(PMBASE+0x30)
#define	GPIREG(x)		(GPIREG0+((x)/8))
#define	GPIBIT(x)		(1 << ((x)%8))

#define	PIIX_GPI_BD_ID1		18
#define	PIIX_GPI_BD_ID2		19
#define	PIIX_GPI_BD_ID3		20
#define	PIIX_GPI_BD_ID4		21
#define	PIIX_GPI_BD_REG		GPIREG(PIIX_GPI_BD_ID1)
#define	PIIX_GPI_BD_MASK	(GPIBIT(PIIX_GPI_BD_ID1) | \
				GPIBIT(PIIX_GPI_BD_ID2) | \
				GPIBIT(PIIX_GPI_BD_ID3) | \
				GPIBIT(PIIX_GPI_BD_ID4) )

#define	PIIX_GPI_BD_SHIFT	(PIIX_GPI_BD_ID1 % 8)

#define	SIO_INDEX		0x2e
#define	SIO_DATA		0x2f

#define	SIO_DEV_SEL		0x7
#define	SIO_DEV_ENB		0x30
#define	SIO_DEV_MSB		0x60
#define	SIO_DEV_LSB		0x61

#define	SIO_GP_DEV		0x7

#define	SIO_GP_BASE		SIO_GPIO_START
#define	SIO_GP_MSB		(SIO_GP_BASE>>8)
#define	SIO_GP_LSB		(SIO_GP_BASE&0xff)

#define	SIO_GP_DATA1		(SIO_GP_BASE+0)

#define	SIO_PM_DEV		0x8

#define	SIO_PM_BASE		SIO_PM_START
#define	SIO_PM_MSB		(SIO_PM_BASE>>8)
#define	SIO_PM_LSB		(SIO_PM_BASE&0xff)
#define	SIO_PM_INDEX		(SIO_PM_BASE+0)
#define	SIO_PM_DATA		(SIO_PM_BASE+1)

#define	SIO_PM_FER2		0x1

#define	SIO_PM_GP_EN		0x80



#define SPECIAL_DEV		0xff
#define SPECIAL_REG		0x00

#define PIIX_SPECIAL_STOP		0x00120002

#define PIIX4_RESET_PORT	0xcf9
#define PIIX4_RESET_VAL		0x6

#define PMSTS_PORT		0xf80	
#define PMEN_PORT		0xf82	
#define	PMCNTRL_PORT		0xf84	

#define PM_SUSPEND_ENABLE	0x2000	

#define PM_STS_RSM		(1<<15)	
#define PM_STS_PWRBTNOR		(1<<11)	
#define PM_STS_RTC		(1<<10)	
#define PM_STS_PWRBTN		(1<<8)	
#define PM_STS_GBL		(1<<5)	
#define PM_STS_BM		(1<<4)	
#define PM_STS_TMROF		(1<<0)	

#define PIIX_GPIREG0			(0xf80 + 0x30)

#define	PIIX_GPI_STPCLK		0x4	

#endif 
