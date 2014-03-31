#ifndef __ASM_ARCH_ASSABET_H
#define __ASM_ARCH_ASSABET_H



#define ASSABET_SCR_SDRAM_LOW	(1<<2)	
#define ASSABET_SCR_SDRAM_HIGH	(1<<3)	
#define ASSABET_SCR_FLASH_LOW	(1<<4)	
#define ASSABET_SCR_FLASH_HIGH	(1<<5)	
#define ASSABET_SCR_GFX		(1<<8)	
#define ASSABET_SCR_SA1111	(1<<9)	

#define ASSABET_SCR_INIT	-1

extern unsigned long SCR_value;

#ifdef CONFIG_ASSABET_NEPONSET
#define machine_has_neponset()  ((SCR_value & ASSABET_SCR_SA1111) == 0)
#else
#define machine_has_neponset()	(0)
#endif


#define ASSABET_BCR_BASE  0xf1000000
#define ASSABET_BCR (*(volatile unsigned int *)(ASSABET_BCR_BASE))

#define ASSABET_BCR_CF_PWR	(1<<0)	
#define ASSABET_BCR_CF_RST	(1<<1)	
#define ASSABET_BCR_GFX_RST	(1<<1)	
#define ASSABET_BCR_CODEC_RST	(1<<2)	
#define ASSABET_BCR_IRDA_FSEL	(1<<3)	
#define ASSABET_BCR_IRDA_MD0	(1<<4)	
#define ASSABET_BCR_IRDA_MD1	(1<<5)	
#define ASSABET_BCR_STEREO_LB	(1<<6)	
#define ASSABET_BCR_CF_BUS_OFF	(1<<7)	
#define ASSABET_BCR_AUDIO_ON	(1<<8)	
#define ASSABET_BCR_LIGHT_ON	(1<<9)	
#define ASSABET_BCR_LCD_12RGB	(1<<10)	
#define ASSABET_BCR_LCD_ON	(1<<11)	
#define ASSABET_BCR_RS232EN	(1<<12)	
#define ASSABET_BCR_LED_RED	(1<<13)	
#define ASSABET_BCR_LED_GREEN	(1<<14)	
#define ASSABET_BCR_VIB_ON	(1<<15)	
#define ASSABET_BCR_COM_DTR	(1<<16)	
#define ASSABET_BCR_COM_RTS	(1<<17)	
#define ASSABET_BCR_RAD_WU	(1<<18)	
#define ASSABET_BCR_SMB_EN	(1<<19)	
#define ASSABET_BCR_TV_IR_DEC	(1<<20)	
#define ASSABET_BCR_QMUTE	(1<<21)	
#define ASSABET_BCR_RAD_ON	(1<<22)	
#define ASSABET_BCR_SPK_OFF	(1<<23)	

#ifdef CONFIG_SA1100_ASSABET
extern void ASSABET_BCR_frob(unsigned int mask, unsigned int set);
#else
#define ASSABET_BCR_frob(x,y)	do { } while (0)
#endif

#define ASSABET_BCR_set(x)	ASSABET_BCR_frob((x), (x))
#define ASSABET_BCR_clear(x)	ASSABET_BCR_frob((x), 0)

#define ASSABET_BSR_BASE	0xf1000000
#define ASSABET_BSR (*(volatile unsigned int*)(ASSABET_BSR_BASE))

#define ASSABET_BSR_RS232_VALID	(1 << 24)
#define ASSABET_BSR_COM_DCD	(1 << 25)
#define ASSABET_BSR_COM_CTS	(1 << 26)
#define ASSABET_BSR_COM_DSR	(1 << 27)
#define ASSABET_BSR_RAD_CTS	(1 << 28)
#define ASSABET_BSR_RAD_DSR	(1 << 29)
#define ASSABET_BSR_RAD_DCD	(1 << 30)
#define ASSABET_BSR_RAD_RI	(1 << 31)


#define ASSABET_GPIO_RADIO_IRQ		GPIO_GPIO (14)	
#define ASSABET_GPIO_PS_MODE_SYNC	GPIO_GPIO (16)	
#define ASSABET_GPIO_STEREO_64FS_CLK	GPIO_GPIO (19)	
#define ASSABET_GPIO_GFX_IRQ		GPIO_GPIO (24)	
#define ASSABET_GPIO_BATT_LOW		GPIO_GPIO (26)	
#define ASSABET_GPIO_RCLK		GPIO_GPIO (26)	

#define ASSABET_GPIO_CF_IRQ		21	
#define ASSABET_GPIO_CF_CD		22	
#define ASSABET_GPIO_CF_BVD2		24	
#define ASSABET_GPIO_CF_BVD1		25	

#endif
