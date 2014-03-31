#ifndef _IOP13XX_IRQS_H_
#define _IOP13XX_IRQS_H_

#ifndef __ASSEMBLER__
#include <linux/types.h>

static inline u32 read_intpnd_0(void)
{
	u32 val;
	asm volatile("mrc p6, 0, %0, c0, c3, 0":"=r" (val));
	return val;
}

static inline u32 read_intpnd_1(void)
{
	u32 val;
	asm volatile("mrc p6, 0, %0, c1, c3, 0":"=r" (val));
	return val;
}

static inline u32 read_intpnd_2(void)
{
	u32 val;
	asm volatile("mrc p6, 0, %0, c2, c3, 0":"=r" (val));
	return val;
}

static inline u32 read_intpnd_3(void)
{
	u32 val;
	asm volatile("mrc p6, 0, %0, c3, c3, 0":"=r" (val));
	return val;
}
#endif

#define INTBASE 0
#define INTSIZE_4 1

#define IOP13XX_IRQ(x)		(IOP13XX_IRQ_OFS + (x))

#define IRQ_IOP13XX_ADMA0_EOT	(0)
#define IRQ_IOP13XX_ADMA0_EOC	(1)
#define IRQ_IOP13XX_ADMA1_EOT	(2)
#define IRQ_IOP13XX_ADMA1_EOC	(3)
#define IRQ_IOP13XX_ADMA2_EOT	(4)
#define IRQ_IOP13XX_ADMA2_EOC	(5)
#define IRQ_IOP134_WATCHDOG	(6)
#define IRQ_IOP13XX_RSVD_7	(7)
#define IRQ_IOP13XX_TIMER0	(8)
#define IRQ_IOP13XX_TIMER1	(9)
#define IRQ_IOP13XX_I2C_0	(10)
#define IRQ_IOP13XX_I2C_1	(11)
#define IRQ_IOP13XX_MSG	(12)
#define IRQ_IOP13XX_MSGIBQ	(13)
#define IRQ_IOP13XX_ATU_IM	(14)
#define IRQ_IOP13XX_ATU_BIST	(15)
#define IRQ_IOP13XX_PPMU	(16)
#define IRQ_IOP13XX_COREPMU	(17)
#define IRQ_IOP13XX_CORECACHE	(18)
#define IRQ_IOP13XX_RSVD_19	(19)
#define IRQ_IOP13XX_RSVD_20	(20)
#define IRQ_IOP13XX_RSVD_21	(21)
#define IRQ_IOP13XX_RSVD_22	(22)
#define IRQ_IOP13XX_RSVD_23	(23)
#define IRQ_IOP13XX_XINT0	(24)
#define IRQ_IOP13XX_XINT1	(25)
#define IRQ_IOP13XX_XINT2	(26)
#define IRQ_IOP13XX_XINT3	(27)
#define IRQ_IOP13XX_XINT4	(28)
#define IRQ_IOP13XX_XINT5	(29)
#define IRQ_IOP13XX_XINT6	(30)
#define IRQ_IOP13XX_XINT7	(31)
				      
#define IRQ_IOP13XX_XINT8	(32)  
#define IRQ_IOP13XX_XINT9	(33)  
#define IRQ_IOP13XX_XINT10	(34)  
#define IRQ_IOP13XX_XINT11	(35)  
#define IRQ_IOP13XX_XINT12	(36)  
#define IRQ_IOP13XX_XINT13	(37)  
#define IRQ_IOP13XX_XINT14	(38)  
#define IRQ_IOP13XX_XINT15	(39)  
#define IRQ_IOP13XX_RSVD_40	(40)  
#define IRQ_IOP13XX_RSVD_41	(41)  
#define IRQ_IOP13XX_RSVD_42	(42)  
#define IRQ_IOP13XX_RSVD_43	(43)  
#define IRQ_IOP13XX_RSVD_44	(44)  
#define IRQ_IOP13XX_RSVD_45	(45)  
#define IRQ_IOP13XX_RSVD_46	(46)  
#define IRQ_IOP13XX_RSVD_47	(47)  
#define IRQ_IOP13XX_RSVD_48	(48)  
#define IRQ_IOP13XX_RSVD_49	(49)  
#define IRQ_IOP13XX_RSVD_50	(50)  
#define IRQ_IOP13XX_UART0	(51)  
#define IRQ_IOP13XX_UART1	(52)  
#define IRQ_IOP13XX_PBIE	(53)  
#define IRQ_IOP13XX_ATU_CRW	(54)  
#define IRQ_IOP13XX_ATU_ERR	(55)  
#define IRQ_IOP13XX_MCU_ERR	(56)  
#define IRQ_IOP13XX_ADMA0_ERR	(57)  
#define IRQ_IOP13XX_ADMA1_ERR	(58)  
#define IRQ_IOP13XX_ADMA2_ERR	(59)  
#define IRQ_IOP13XX_RSVD_60	(60)  
#define IRQ_IOP13XX_RSVD_61	(61)  
#define IRQ_IOP13XX_MSG_ERR	(62)  
#define IRQ_IOP13XX_RSVD_63	(63)  
				      
#define IRQ_IOP13XX_INTERPROC	(64)  
#define IRQ_IOP13XX_RSVD_65	(65)  
#define IRQ_IOP13XX_RSVD_66	(66)  
#define IRQ_IOP13XX_RSVD_67	(67)  
#define IRQ_IOP13XX_RSVD_68	(68)  
#define IRQ_IOP13XX_RSVD_69	(69)  
#define IRQ_IOP13XX_RSVD_70	(70)  
#define IRQ_IOP13XX_RSVD_71	(71)  
#define IRQ_IOP13XX_RSVD_72	(72)  
#define IRQ_IOP13XX_RSVD_73	(73)  
#define IRQ_IOP13XX_RSVD_74	(74)  
#define IRQ_IOP13XX_RSVD_75	(75)  
#define IRQ_IOP13XX_RSVD_76	(76)  
#define IRQ_IOP13XX_RSVD_77	(77)  
#define IRQ_IOP13XX_RSVD_78	(78)  
#define IRQ_IOP13XX_RSVD_79	(79)  
#define IRQ_IOP13XX_RSVD_80	(80)  
#define IRQ_IOP13XX_RSVD_81	(81)  
#define IRQ_IOP13XX_RSVD_82	(82)  
#define IRQ_IOP13XX_RSVD_83	(83)  
#define IRQ_IOP13XX_RSVD_84	(84)  
#define IRQ_IOP13XX_RSVD_85	(85)  
#define IRQ_IOP13XX_RSVD_86	(86)  
#define IRQ_IOP13XX_RSVD_87	(87)  
#define IRQ_IOP13XX_RSVD_88	(88)  
#define IRQ_IOP13XX_RSVD_89	(89)  
#define IRQ_IOP13XX_RSVD_90	(90)  
#define IRQ_IOP13XX_RSVD_91	(91)  
#define IRQ_IOP13XX_RSVD_92	(92)  
#define IRQ_IOP13XX_RSVD_93	(93)  
#define IRQ_IOP13XX_SIB_ERR	(94)  
#define IRQ_IOP13XX_SRAM_ERR	(95)  
				      
#define IRQ_IOP13XX_I2C_2	(96)  
#define IRQ_IOP13XX_ATUE_BIST	(97)  
#define IRQ_IOP13XX_ATUE_CRW	(98)  
#define IRQ_IOP13XX_ATUE_ERR	(99)  
#define IRQ_IOP13XX_IMU	(100) 
#define IRQ_IOP13XX_RSVD_101	(101) 
#define IRQ_IOP13XX_RSVD_102	(102) 
#define IRQ_IOP13XX_TPMI0_OUT	(103) 
#define IRQ_IOP13XX_TPMI1_OUT	(104) 
#define IRQ_IOP13XX_TPMI2_OUT	(105) 
#define IRQ_IOP13XX_TPMI3_OUT	(106) 
#define IRQ_IOP13XX_ATUE_IMA	(107) 
#define IRQ_IOP13XX_ATUE_IMB	(108) 
#define IRQ_IOP13XX_ATUE_IMC	(109) 
#define IRQ_IOP13XX_ATUE_IMD	(110) 
#define IRQ_IOP13XX_MU_MSI_TB	(111) 
#define IRQ_IOP13XX_RSVD_112	(112) 
#define IRQ_IOP13XX_INBD_MSI	(113) 
#define IRQ_IOP13XX_RSVD_114	(114) 
#define IRQ_IOP13XX_RSVD_115	(115) 
#define IRQ_IOP13XX_RSVD_116	(116) 
#define IRQ_IOP13XX_RSVD_117	(117) 
#define IRQ_IOP13XX_RSVD_118	(118) 
#define IRQ_IOP13XX_RSVD_119	(119) 
#define IRQ_IOP13XX_RSVD_120	(120) 
#define IRQ_IOP13XX_RSVD_121	(121) 
#define IRQ_IOP13XX_RSVD_122	(122) 
#define IRQ_IOP13XX_RSVD_123	(123) 
#define IRQ_IOP13XX_RSVD_124	(124) 
#define IRQ_IOP13XX_RSVD_125	(125) 
#define IRQ_IOP13XX_RSVD_126	(126) 
#define IRQ_IOP13XX_HPI	(127) 

#ifdef CONFIG_PCI_MSI
#define IRQ_IOP13XX_MSI_0	(IRQ_IOP13XX_HPI + 1)
#define NR_IOP13XX_IRQS 	(IRQ_IOP13XX_MSI_0 + 128)
#else
#define NR_IOP13XX_IRQS	(IRQ_IOP13XX_HPI + 1)
#endif

#define NR_IRQS		NR_IOP13XX_IRQS

#endif 
