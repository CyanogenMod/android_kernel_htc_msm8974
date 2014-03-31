#ifndef __ASM_ARCH_REGS_LCD_H
#define __ASM_ARCH_REGS_LCD_H

#include <mach/bitfield.h>

#define LCCR0		(0x000)	
#define LCCR1		(0x004)	
#define LCCR2		(0x008)	
#define LCCR3		(0x00C)	
#define LCCR4		(0x010)	
#define LCCR5		(0x014)	
#define LCSR		(0x038)	
#define LCSR1		(0x034)	
#define LIIDR		(0x03C)	
#define TMEDRGBR	(0x040)	
#define TMEDCR		(0x044)	

#define FBR0		(0x020)	
#define FBR1		(0x024)	
#define FBR2		(0x028) 
#define FBR3		(0x02C) 
#define FBR4		(0x030) 
#define FBR5		(0x110) 
#define FBR6		(0x114) 

#define OVL1C1		(0x050)	
#define OVL1C2		(0x060)	
#define OVL2C1		(0x070)	
#define OVL2C2		(0x080)	

#define CMDCR		(0x100)	
#define PRSR		(0x104)	

#define LCCR3_BPP(x)	((((x) & 0x7) << 24) | (((x) & 0x8) ? (1 << 29) : 0))

#define LCCR3_PDFOR_0	(0 << 30)
#define LCCR3_PDFOR_1	(1 << 30)
#define LCCR3_PDFOR_2	(2 << 30)
#define LCCR3_PDFOR_3	(3 << 30)

#define LCCR4_PAL_FOR_0	(0 << 15)
#define LCCR4_PAL_FOR_1	(1 << 15)
#define LCCR4_PAL_FOR_2	(2 << 15)
#define LCCR4_PAL_FOR_3	(3 << 15)
#define LCCR4_PAL_FOR_MASK	(3 << 15)

#define FDADR0		(0x200)	
#define FDADR1		(0x210)	
#define FDADR2		(0x220)	
#define FDADR3		(0x230)	
#define FDADR4		(0x240)	
#define FDADR5		(0x250)	
#define FDADR6		(0x260) 

#define LCCR0_ENB	(1 << 0)	
#define LCCR0_CMS	(1 << 1)	
#define LCCR0_Color	(LCCR0_CMS*0)	
#define LCCR0_Mono	(LCCR0_CMS*1)	
#define LCCR0_SDS	(1 << 2)	
#define LCCR0_Sngl	(LCCR0_SDS*0)	
#define LCCR0_Dual	(LCCR0_SDS*1)	

#define LCCR0_LDM	(1 << 3)	
#define LCCR0_SFM	(1 << 4)	
#define LCCR0_IUM	(1 << 5)	
#define LCCR0_EFM	(1 << 6)	
#define LCCR0_PAS	(1 << 7)	
#define LCCR0_Pas	(LCCR0_PAS*0)	
#define LCCR0_Act	(LCCR0_PAS*1)	
#define LCCR0_DPD	(1 << 9)	
#define LCCR0_4PixMono	(LCCR0_DPD*0)	
#define LCCR0_8PixMono	(LCCR0_DPD*1)	
#define LCCR0_DIS	(1 << 10)	
#define LCCR0_QDM	(1 << 11)	
#define LCCR0_PDD	(0xff << 12)	
#define LCCR0_PDD_S	12
#define LCCR0_BM	(1 << 20)	
#define LCCR0_OUM	(1 << 21)	
#define LCCR0_LCDT	(1 << 22)	
#define LCCR0_RDSTM	(1 << 23)	
#define LCCR0_CMDIM	(1 << 24)	
#define LCCR0_OUC	(1 << 25)	
#define LCCR0_LDDALT	(1 << 26)	

#define LCCR1_PPL	Fld (10, 0)	
#define LCCR1_DisWdth(Pixel)	(((Pixel) - 1) << FShft (LCCR1_PPL))

#define LCCR1_HSW	Fld (6, 10)	
#define LCCR1_HorSnchWdth(Tpix)	(((Tpix) - 1) << FShft (LCCR1_HSW))

#define LCCR1_ELW	Fld (8, 16)	
#define LCCR1_EndLnDel(Tpix)	(((Tpix) - 1) << FShft (LCCR1_ELW))

#define LCCR1_BLW	Fld (8, 24)	
#define LCCR1_BegLnDel(Tpix)	(((Tpix) - 1) << FShft (LCCR1_BLW))

#define LCCR2_LPP	Fld (10, 0)	
#define LCCR2_DisHght(Line)	(((Line) - 1) << FShft (LCCR2_LPP))

#define LCCR2_VSW	Fld (6, 10)	
#define LCCR2_VrtSnchWdth(Tln)	(((Tln) - 1) << FShft (LCCR2_VSW))

#define LCCR2_EFW	Fld (8, 16)	
#define LCCR2_EndFrmDel(Tln)	((Tln) << FShft (LCCR2_EFW))

#define LCCR2_BFW	Fld (8, 24)	
#define LCCR2_BegFrmDel(Tln)	((Tln) << FShft (LCCR2_BFW))

#define LCCR3_API	(0xf << 16)	
#define LCCR3_API_S	16
#define LCCR3_VSP	(1 << 20)	
#define LCCR3_HSP	(1 << 21)	
#define LCCR3_PCP	(1 << 22)	
#define LCCR3_PixRsEdg	(LCCR3_PCP*0)	
#define LCCR3_PixFlEdg	(LCCR3_PCP*1)	

#define LCCR3_OEP	(1 << 23)	
#define LCCR3_OutEnH	(LCCR3_OEP*0)	
#define LCCR3_OutEnL	(LCCR3_OEP*1)	

#define LCCR3_DPC	(1 << 27)	
#define LCCR3_PCD	Fld (8, 0)	
#define LCCR3_PixClkDiv(Div)	(((Div) << FShft (LCCR3_PCD)))

#define LCCR3_ACB	Fld (8, 8)	
#define LCCR3_Acb(Acb)	(((Acb) << FShft (LCCR3_ACB)))

#define LCCR3_HorSnchH	(LCCR3_HSP*0)	
#define LCCR3_HorSnchL	(LCCR3_HSP*1)	

#define LCCR3_VrtSnchH	(LCCR3_VSP*0)	
#define LCCR3_VrtSnchL	(LCCR3_VSP*1)	

#define LCCR5_IUM(x)	(1 << ((x) + 23)) 
#define LCCR5_BSM(x)	(1 << ((x) + 15)) 
#define LCCR5_EOFM(x)	(1 << ((x) + 7))  
#define LCCR5_SOFM(x)	(1 << ((x) + 0))  

#define LCSR_LDD	(1 << 0)	
#define LCSR_SOF	(1 << 1)	
#define LCSR_BER	(1 << 2)	
#define LCSR_ABC	(1 << 3)	
#define LCSR_IUL	(1 << 4)	
#define LCSR_IUU	(1 << 5)	
#define LCSR_OU		(1 << 6)	
#define LCSR_QD		(1 << 7)	
#define LCSR_EOF	(1 << 8)	
#define LCSR_BS		(1 << 9)	
#define LCSR_SINT	(1 << 10)	
#define LCSR_RD_ST	(1 << 11)	
#define LCSR_CMD_INT	(1 << 12)	

#define LCSR1_IU(x)	(1 << ((x) + 23)) 
#define LCSR1_BS(x)	(1 << ((x) + 15)) 
#define LCSR1_EOF(x)	(1 << ((x) + 7))  
#define LCSR1_SOF(x)	(1 << ((x) - 1))  

#define LDCMD_PAL	(1 << 26)	

#define OVLxC1_PPL(x)	((((x) - 1) & 0x3ff) << 0)	
#define OVLxC1_LPO(x)	((((x) - 1) & 0x3ff) << 10)	
#define OVLxC1_BPP(x)	(((x) & 0xf) << 20)	
#define OVLxC1_OEN	(1 << 31)		
#define OVLxC2_XPOS(x)	(((x) & 0x3ff) << 0)	
#define OVLxC2_YPOS(x)	(((x) & 0x3ff) << 10)	
#define OVL2C2_PFOR(x)	(((x) & 0x7) << 20)	

#define PRSR_DATA(x)	((x) & 0xff)	
#define PRSR_A0		(1 << 8)	
#define PRSR_ST_OK	(1 << 9)	
#define PRSR_CON_NT	(1 << 10)	

#define SMART_CMD_A0			 (0x1 << 8)
#define SMART_CMD_READ_STATUS_REG	 (0x0 << 9)
#define SMART_CMD_READ_FRAME_BUFFER	((0x0 << 9) | SMART_CMD_A0)
#define SMART_CMD_WRITE_COMMAND		 (0x1 << 9)
#define SMART_CMD_WRITE_DATA		((0x1 << 9) | SMART_CMD_A0)
#define SMART_CMD_WRITE_FRAME		((0x2 << 9) | SMART_CMD_A0)
#define SMART_CMD_WAIT_FOR_VSYNC	 (0x3 << 9)
#define SMART_CMD_NOOP			 (0x4 << 9)
#define SMART_CMD_INTERRUPT		 (0x5 << 9)

#define SMART_CMD(x)	(SMART_CMD_WRITE_COMMAND | ((x) & 0xff))
#define SMART_DAT(x)	(SMART_CMD_WRITE_DATA | ((x) & 0xff))

#define SMART_CMD_DELAY		(0x6 << 9)
#define SMART_DELAY(ms)		(SMART_CMD_DELAY | ((ms) & 0xff))
#endif 
