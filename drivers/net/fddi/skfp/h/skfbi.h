/******************************************************************************
 *
 *	(C)Copyright 1998,1999 SysKonnect,
 *	a business unit of Schneider & Koch & Co. Datensysteme GmbH.
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 *
 *	The information in this file is provided "AS IS" without warranty.
 *
 ******************************************************************************/

#ifndef	_SKFBI_H_
#define	_SKFBI_H_


#ifdef	PCI


#define	PCI_VENDOR_ID	0x00	
#define	PCI_DEVICE_ID	0x02	
#define	PCI_COMMAND	0x04	
#define	PCI_STATUS	0x06	
#define	PCI_REV_ID	0x08	
#define	PCI_CLASS_CODE	0x09	
#define	PCI_CACHE_LSZ	0x0c	
#define	PCI_LAT_TIM	0x0d	
#define	PCI_HEADER_T	0x0e	
#define	PCI_BIST	0x0f	
#define	PCI_BASE_1ST	0x10	
#define	PCI_BASE_2ND	0x14	
#define	PCI_SUB_VID	0x2c	
#define	PCI_SUB_ID	0x2e	
#define	PCI_BASE_ROM	0x30	
#define PCI_CAP_PTR	0x34	
#define	PCI_IRQ_LINE	0x3c	
#define	PCI_IRQ_PIN	0x3d	
#define	PCI_MIN_GNT	0x3e	
#define	PCI_MAX_LAT	0x3f	
#define	PCI_OUR_REG	0x40	
#define	PCI_OUR_REG_1	0x40	
#define	PCI_OUR_REG_2	0x44	
#define PCI_PM_CAP_ID	0x48	
#define PCI_PM_NITEM	0x49	
#define PCI_PM_CAP_REG	0x4a	
#define PCI_PM_CTL_STS	0x4c	
#define PCI_PM_DAT_REG	0x4f	
#define	PCI_VPD_CAP_ID	0x50	
#define PCI_VPD_NITEM	0x51	
#define PCI_VPD_ADR_REG	0x52	
#define PCI_VPD_DAT_REG	0x54	

#define I2C_ADDR_VPD	0xA0	 

#define	PCI_FBTEN	0x0200	
#define	PCI_SERREN	0x0100	
#define	PCI_ADSTEP	0x0080	
#define	PCI_PERREN	0x0040	
#define	PCI_VGA_SNOOP	0x0020	
#define	PCI_MWIEN	0x0010	
#define	PCI_SCYCEN	0x0008	
#define	PCI_BMEN	0x0004	
#define	PCI_MEMEN	0x0002	
#define	PCI_IOEN	0x0001	

#define	PCI_PERR	0x8000	
#define	PCI_SERR	0x4000	
#define	PCI_RMABORT	0x2000	
#define	PCI_RTABORT	0x1000	
#define	PCI_STABORT	0x0800	
#define	PCI_DEVSEL	0x0600	
#define	PCI_DEV_FAST	(0<<9)	
#define	PCI_DEV_MEDIUM	(1<<9)	
#define	PCI_DEV_SLOW	(2<<9)	
#define	PCI_DATAPERR	0x0100	
#define	PCI_FB2BCAP	0x0080	
#define	PCI_UDF		0x0040	
#define PCI_66MHZCAP	0x0020	
#define PCI_NEWCAP	0x0010	

#define PCI_ERRBITS	(PCI_PERR|PCI_SERR|PCI_RMABORT|PCI_STABORT|PCI_DATAPERR)




#define	PCI_HD_MF_DEV	0x80	
#define	PCI_HD_TYPE	0x7f	

#define	PCI_BIST_CAP	0x80	
#define	PCI_BIST_ST	0x40	
#define	PCI_BIST_RET	0x0f	

#define	PCI_MEMSIZE	0x800L       
#define	PCI_MEMBASE_BITS 0xfffff800L 
#define	PCI_MEMSIZE_BIIS 0x000007f0L 
#define	PCI_PREFEN	0x00000008L  
#define	PCI_MEM_TYP	0x00000006L  
#define	PCI_MEM32BIT	(0<<1)	     
#define	PCI_MEM1M	(1<<1)	     
#define	PCI_MEM64BIT	(2<<1)	     
#define	PCI_MEMSPACE	0x00000001L  

#define	PCI_IOBASE	0xffffff00L  
#define	PCI_IOSIZE	0x000000fcL  
#define	PCI_IOSPACE	0x00000001L  


#define	PCI_ROMBASE	0xfffe0000L  
#define	PCI_ROMBASZ	0x0001c000L  
#define	PCI_ROMSIZE	0x00003800L  
#define	PCI_ROMEN	0x00000001L  

				  
#define	PCI_PATCH_DIR	(3L<<27)  
#define PCI_PATCH_DIR_0	(1L<<27)  
#define PCI_PATCH_DIR_1 (1L<<28)  
				  
#define	PCI_EXT_PATCHS	(3L<<25)  
#define PCI_EXT_PATCH_0 (1L<<25)  
#define PCI_EXT_PATCH_1 (1L<<26)  
#define PCI_VIO		(1L<<25)  
#define	PCI_EN_BOOT	(1L<<24)  
				  
				  
#define	PCI_EN_IO	(1L<<23)  
#define	PCI_EN_FPROM	(1L<<22)  
				  
			  	  
#define	PCI_PAGESIZE	(3L<<20)  
#define	PCI_PAGE_16	(0L<<20)  
#define	PCI_PAGE_32K	(1L<<20)  
#define	PCI_PAGE_64K	(2L<<20)  
#define	PCI_PAGE_128K	(3L<<20)  
				  
#define	PCI_PAGEREG	(7L<<16)  
				  
#define	PCI_FORCE_BE	(1L<<14)  
#define	PCI_DIS_MRL	(1L<<13)  
#define	PCI_DIS_MRM	(1L<<12)  
#define	PCI_DIS_MWI	(1L<<11)  
#define	PCI_DISC_CLS	(1L<<10)  
#define	PCI_BURST_DIS	(1L<<9)	  
#define	PCI_BYTE_SWAP	(1L<<8)	  
#define	PCI_SKEW_DAS	(0xfL<<4) 
#define	PCI_SKEW_BASE	(0xfL<<0) 

#define PCI_VPD_WR_TH	(0xffL<<24)	
#define	PCI_DEV_SEL	(0x7fL<<17)	
#define	PCI_VPD_ROM_SZ	(7L<<14)	
					
#define	PCI_PATCH_DIR2	(0xfL<<8)	
#define	PCI_PATCH_DIR_2	(1L<<8)		
#define	PCI_PATCH_DIR_3	(1L<<9)
#define	PCI_PATCH_DIR_4	(1L<<10)
#define	PCI_PATCH_DIR_5	(1L<<11)
#define PCI_EXT_PATCHS2 (0xfL<<4)	
#define	PCI_EXT_PATCH_2	(1L<<4)		
#define	PCI_EXT_PATCH_3	(1L<<5)
#define	PCI_EXT_PATCH_4	(1L<<6)
#define	PCI_EXT_PATCH_5	(1L<<7)
#define	PCI_EN_DUMMY_RD	(1L<<3)		
#define PCI_REV_DESC	(1L<<2)		
#define PCI_USEADDR64	(1L<<1)		
#define PCI_USEDATA64	(1L<<0)		

#define	PCI_PME_SUP	(0x1f<<11)	
#define PCI_PM_D2_SUB	(1<<10)		
#define PCI_PM_D1_SUB	(1<<9)		
					
#define PCI_PM_DSI	(1<<5)		
#define PCI_PM_APS	(1<<4)		
#define PCI_PME_CLOCK	(1<<3)		
#define PCI_PM_VER	(7<<0)		

#define	PCI_PME_STATUS	(1<<15)		
#define PCI_PM_DAT_SCL	(3<<13)		
#define PCI_PM_DAT_SEL	(0xf<<9)	
					
#define PCI_PM_STATE	(3<<0)		
#define PCI_PM_STATE_D0	(0<<0)		
#define	PCI_PM_STATE_D1	(1<<0)		
#define PCI_PM_STATE_D2	(2<<0)		
#define PCI_PM_STATE_D3 (3<<0)		

#define	PCI_VPD_FLAG	(1<<15)		


#define	B0_RAP		0x0000	
	
#define	B0_CTRL		0x0004	
#define	B0_DAS		0x0005	
#define	B0_LED		0x0006	
#define	B0_TST_CTRL	0x0007	
#define	B0_ISRC		0x0008	
#define	B0_IMSK		0x000c	

#define B0_CMDREG1	0x0010	
#define B0_CMDREG2	0x0014	
#define B0_ST1U		0x0010	
#define B0_ST1L		0x0014	
#define B0_ST2U		0x0018	
#define B0_ST2L		0x001c	

#define B0_MARR		0x0020	
#define B0_MARW		0x0024	
#define B0_MDRU		0x0028	
#define B0_MDRL		0x002c	

#define	B0_MDREG3	0x0030	
#define	B0_ST3U		0x0034	
#define	B0_ST3L		0x0038	
#define	B0_IMSK3U	0x003c	
#define	B0_IMSK3L	0x0040	
#define	B0_IVR		0x0044	
#define	B0_IMR		0x0048	

#define B0_CNTRL_A	0x0050	
#define B0_CNTRL_B	0x0054	
#define B0_INTR_MASK	0x0058	
#define B0_XMIT_VECTOR	0x005c	

#define B0_STATUS_A	0x0060	
#define B0_STATUS_B	0x0064	
#define B0_CNTRL_C	0x0068	
#define	B0_MDREG1	0x006c	

#define	B0_R1_CSR	0x0070	
#define	B0_R2_CSR	0x0074	
#define	B0_XA_CSR	0x0078	
#define	B0_XS_CSR	0x007c	


#define	B2_MAC_0	0x0100	
#define	B2_MAC_1	0x0101	
#define	B2_MAC_2	0x0102	
#define	B2_MAC_3	0x0103	
#define	B2_MAC_4	0x0104	
#define	B2_MAC_5	0x0105	
#define	B2_MAC_6	0x0106	
#define	B2_MAC_7	0x0107	

#define B2_CONN_TYP	0x0108	
#define B2_PMD_TYP	0x0109	
				
	
#define B2_E_0		0x010c	
#define B2_E_1		0x010d	
#define B2_E_2		0x010e	
#define B2_E_3		0x010f	
#define B2_FAR		0x0110	
#define B2_FDP		0x0114	
				
#define B2_LD_CRTL	0x0118	
#define B2_LD_TEST	0x0119	
				
#define B2_TI_INI	0x0120	
#define B2_TI_VAL	0x0124	
#define B2_TI_CRTL	0x0128	
#define B2_TI_TEST	0x0129	
				
#define B2_WDOG_INI	0x0130	
#define B2_WDOG_VAL	0x0134	
#define B2_WDOG_CRTL	0x0138	
#define B2_WDOG_TEST	0x0139	
				
#define B2_RTM_INI	0x0140	
#define B2_RTM_VAL	0x0144	
#define B2_RTM_CRTL	0x0148	
#define B2_RTM_TEST	0x0149	

#define B2_TOK_COUNT	0x014c	
#define B2_DESC_ADDR_H	0x0150	
#define B2_CTRL_2	0x0154	
#define B2_IFACE_REG	0x0155	
				
#define B2_TST_CTRL_2	0x0157	
#define B2_I2C_CTRL	0x0158	
#define B2_I2C_DATA	0x015c	

#define B2_IRQ_MOD_INI	0x0160	
#define B2_IRQ_MOD_VAL	0x0164	
#define B2_IRQ_MOD_CTRL	0x0168	
#define B2_IRQ_MOD_TEST	0x0169	
				

#define B3_CFG_SPC	0x180

#define B4_R1_D		0x0200	
#define B4_R1_DA	0x0210	
#define B4_R1_AC	0x0214	
#define B4_R1_BC	0x0218	
#define B4_R1_CSR	0x021c	
#define B4_R1_F		0x0220	
#define B4_R1_T1	0x0224	
#define B4_R1_T1_TR	0x0224	
#define B4_R1_T1_WR	0x0225	
#define B4_R1_T1_RD	0x0226	
#define B4_R1_T1_SV	0x0227	
#define B4_R1_T2	0x0228	
#define B4_R1_T3	0x022c	
#define B4_R1_DA_H	0x0230	
#define B4_R1_AC_H	0x0234	
				
				
#define B4_R2_D		0x0240	
#define B4_R2_DA	0x0250	
#define B4_R2_AC	0x0254	
#define B4_R2_BC	0x0258	
#define B4_R2_CSR	0x025c	
#define B4_R2_F		0x0260	
#define B4_R2_T1	0x0264	
#define B4_R2_T1_TR	0x0264	
#define B4_R2_T1_WR	0x0265	
#define B4_R2_T1_RD	0x0266	
#define B4_R2_T1_SV	0x0267	
#define B4_R2_T2	0x0268	
#define B4_R2_T3	0x026c	
				

#define B5_XA_D		0x0280	
#define B5_XA_DA	0x0290	
#define B5_XA_AC	0x0294	
#define B5_XA_BC	0x0298	
#define B5_XA_CSR	0x029c	
#define B5_XA_F		0x02a0	
#define B5_XA_T1	0x02a4	
#define B5_XA_T1_TR	0x02a4	
#define B5_XA_T1_WR	0x02a5	
#define B5_XA_T1_RD	0x02a6	
#define B5_XA_T1_SV	0x02a7	
#define B5_XA_T2	0x02a8	
#define B5_XA_T3	0x02ac	
#define B5_XA_DA_H	0x02b0	
#define B5_XA_AC_H	0x02b4	
				
#define B5_XS_D		0x02c0	
#define B5_XS_DA	0x02d0	
#define B5_XS_AC	0x02d4	
#define B5_XS_BC	0x02d8	
#define B5_XS_CSR	0x02dc	
#define B5_XS_F		0x02e0	
#define B5_XS_T1	0x02e4	
#define B5_XS_T1_TR	0x02e4	
#define B5_XS_T1_WR	0x02e5	
#define B5_XS_T1_RD	0x02e6	
#define B5_XS_T1_SV	0x02e7	
#define B5_XS_T2	0x02e8	
#define B5_XS_T3	0x02ec	
#define B5_XS_DA_H	0x02f0	
#define B5_XS_AC_H	0x02f4	
				

#define B6_EXT_REG	0x300




#define	RAP_RAP		0x0f	

#define CTRL_FDDI_CLR	(1<<7)	
#define CTRL_FDDI_SET	(1<<6)	
#define	CTRL_HPI_CLR	(1<<5)	
#define	CTRL_HPI_SET	(1<<4)	
#define	CTRL_MRST_CLR	(1<<3)	
#define	CTRL_MRST_SET	(1<<2)	
#define	CTRL_RST_CLR	(1<<1)	
#define	CTRL_RST_SET	(1<<0)	

#define BUS_CLOCK	(1<<7)	
#define BUS_SLOT_SZ	(1<<6)	
				
#define	DAS_AVAIL	(1<<3)	
#define DAS_BYP_ST	(1<<2)	
#define DAS_BYP_INS	(1<<1)	
#define DAS_BYP_RMV	(1<<0)	

				
#define LED_2_ON	(1<<5)	
#define LED_2_OFF	(1<<4)	
#define LED_1_ON	(1<<3)	
#define LED_1_OFF	(1<<2)	
#define LED_0_ON	(1<<1)	
#define LED_0_OFF	(1<<0)	

#define LED_GA_ON	LED_2_ON	
#define LED_GA_OFF	LED_2_OFF	
#define LED_MY_ON	LED_1_ON
#define LED_MY_OFF	LED_1_OFF
#define LED_GB_ON	LED_0_ON
#define LED_GB_OFF	LED_0_OFF

#define	TST_FRC_DPERR_MR	(1<<7)	
#define	TST_FRC_DPERR_MW	(1<<6)	
#define	TST_FRC_DPERR_TR	(1<<5)	
#define	TST_FRC_DPERR_TW	(1<<4)	
#define	TST_FRC_APERR_M		(1<<3)	
#define	TST_FRC_APERR_T		(1<<2)	
#define	TST_CFG_WRITE_ON	(1<<1)	
#define	TST_CFG_WRITE_OFF	(1<<0)	

					
#define IS_I2C_READY	(1L<<27)	
#define IS_IRQ_SW	(1L<<26)	
#define IS_EXT_REG	(1L<<25)	
#define	IS_IRQ_STAT	(1L<<24)	
					
#define	IS_IRQ_MST_ERR	(1L<<23)	
					
#define	IS_TIMINT	(1L<<22)	
#define	IS_TOKEN	(1L<<21)	
#define	IS_PLINT1	(1L<<20)	
#define	IS_PLINT2	(1L<<19)	
#define	IS_MINTR3	(1L<<18)	
#define	IS_MINTR2	(1L<<17)	
#define	IS_MINTR1	(1L<<16)	
#define	IS_R1_P		(1L<<15)	
#define	IS_R1_B		(1L<<14)	
#define	IS_R1_F		(1L<<13)	
#define	IS_R1_C		(1L<<12)	
#define	IS_R2_P		(1L<<11)	
#define	IS_R2_B		(1L<<10)	
#define	IS_R2_F		(1L<<9)		
#define	IS_R2_C		(1L<<8)		
					
#define	IS_XA_B		(1L<<6)		
#define	IS_XA_F		(1L<<5)		
#define	IS_XA_C		(1L<<4)		
					
#define	IS_XS_B		(1L<<2)		
#define	IS_XS_F		(1L<<1)		
#define	IS_XS_C		(1L<<0)		

#define	ALL_IRSR	0x01ffff77L	
#define	ALL_IRSR_ML	0x0ffff077L	


					
#define IRQ_I2C_READY	(1L<<27)	
#define IRQ_SW		(1L<<26)	
#define IRQ_EXT_REG	(1L<<25)	
#define	IRQ_STAT	(1L<<24)	
					
#define	IRQ_MST_ERR	(1L<<23)	
					
#define	IRQ_TIMER	(1L<<22)	
#define	IRQ_RTM		(1L<<21)	
#define	IRQ_DAS		(1L<<20)	
#define	IRQ_IFCP_4	(1L<<19)	
#define	IRQ_IFCP_3	(1L<<18)	
#define	IRQ_IFCP_2	(1L<<17)	
#define	IRQ_IFCP_1	(1L<<16)	
#define	IRQ_R1_P	(1L<<15)	
#define	IRQ_R1_B	(1L<<14)	
#define	IRQ_R1_F	(1L<<13)	
#define	IRQ_R1_C	(1L<<12)	
#define	IRQ_R2_P	(1L<<11)	
#define	IRQ_R2_B	(1L<<10)	
#define	IRQ_R2_F	(1L<<9)		
#define	IRQ_R2_C	(1L<<8)		
					
#define	IRQ_XA_B	(1L<<6)		
#define	IRQ_XA_F	(1L<<5)		
#define	IRQ_XA_C	(1L<<4)		
					
#define	IRQ_XS_B	(1L<<2)		
#define	IRQ_XS_F	(1L<<1)		
#define	IRQ_XS_C	(1L<<0)		





#define	FAR_ADDR	0x1ffffL	



#define	LD_T_ON		(1<<3)	
#define	LD_T_OFF	(1<<2)	
#define	LD_T_STEP	(1<<1)	
#define	LD_START	(1<<0)	

#define GET_TOK_CT	(1<<4)	
#define TIM_RES_TOK	(1<<3)	
#define TIM_ALARM	(1<<3)	
#define TIM_START	(1<<2)	
#define TIM_STOP	(1<<1)	
#define TIM_CL_IRQ	(1<<0)	
#define	TIM_T_ON	(1<<2)	
#define	TIM_T_OFF	(1<<1)	
#define	TIM_T_STEP	(1<<0)	

				
#define CTRL_CL_I2C_IRQ (1<<4)	
#define CTRL_ST_SW_IRQ	(1<<3)	
#define CTRL_CL_SW_IRQ	(1<<2)	
#define CTRL_STOP_DONE	(1<<1)	
#define	CTRL_STOP_MAST	(1<<0)	

				
#define	IF_I2C_DATA_DIR	(1<<2)	
#define IF_I2C_DATA	(1<<1)	
#define	IF_I2C_CLK	(1<<0)	

				
					
					
					
#define TST_FRC_DPERR_MR64	(1<<3)	
#define TST_FRC_DPERR_MW64	(1<<2)	
#define TST_FRC_APERR_1M64	(1<<1)	
#define TST_FRC_APERR_2M64	(1<<0)	

#define	I2C_FLAG	(1L<<31)	
#define I2C_ADDR	(0x7fffL<<16)	/* Bit 30..16:	Addr to be read/written*/
#define	I2C_DEV_SEL	(0x7fL<<9)	
					
#define I2C_BURST_LEN	(1L<<4)		
#define I2C_DEV_SIZE	(7L<<1)		
#define I2C_025K_DEV	(0L<<1)		
#define I2C_05K_DEV	(1L<<1)		
#define	I2C_1K_DEV	(2L<<1)		
#define I2C_2K_DEV	(3L<<1)		
#define	I2C_4K_DEV	(4L<<1)		
#define	I2C_8K_DEV	(5L<<1)		
#define	I2C_16K_DEV	(6L<<1)		
#define	I2C_32K_DEV	(7L<<1)		
#define I2C_STOP_BIT	(1<<0)		

#define	I2C_ADDR_TEMP	0x90	


#define	CSR_DESC_CLEAR	(1L<<21)    
#define	CSR_DESC_SET	(1L<<20)    
#define	CSR_FIFO_CLEAR	(1L<<19)    
#define	CSR_FIFO_SET	(1L<<18)    
#define	CSR_HPI_RUN	(1L<<17)    
#define	CSR_HPI_RST	(1L<<16)    
#define	CSR_SV_RUN	(1L<<15)    
#define	CSR_SV_RST	(1L<<14)    
#define	CSR_DREAD_RUN	(1L<<13)    
#define	CSR_DREAD_RST	(1L<<12)    
#define	CSR_DWRITE_RUN	(1L<<11)    
#define	CSR_DWRITE_RST	(1L<<10)    
#define	CSR_TRANS_RUN	(1L<<9)     
#define	CSR_TRANS_RST	(1L<<8)     
				    
#define	CSR_START	(1L<<4)     
#define	CSR_IRQ_CL_P	(1L<<3)     
#define	CSR_IRQ_CL_B	(1L<<2)     
#define	CSR_IRQ_CL_F	(1L<<1)     
#define	CSR_IRQ_CL_C	(1L<<0)     

#define CSR_SET_RESET	(CSR_DESC_SET|CSR_FIFO_SET|CSR_HPI_RST|CSR_SV_RST|\
			CSR_DREAD_RST|CSR_DWRITE_RST|CSR_TRANS_RST)
#define CSR_CLR_RESET	(CSR_DESC_CLEAR|CSR_FIFO_CLEAR|CSR_HPI_RUN|CSR_SV_RUN|\
			CSR_DREAD_RUN|CSR_DWRITE_RUN|CSR_TRANS_RUN)


					
#define F_ALM_FULL	(1L<<27)	
#define F_FIFO_EOF	(1L<<26)	
#define F_WM_REACHED	(1L<<25)	
#define F_UP_DW_USED	(1L<<24)	
					
#define F_FIFO_LEVEL	(0x1fL<<16)	
					
#define F_ML_WATER_M	0x0000ffL	
#define	FLAG_WATER	0x00001fL	

#define	SM_CRTL_SV	(0xffL<<24) 
#define	SM_CRTL_RD	(0xffL<<16) 
#define	SM_CRTL_WR	(0xffL<<8)  
#define	SM_CRTL_TR	(0xffL<<0)  

#define	SM_STATE	0xf0	
#define	SM_LOAD		0x08	
#define	SM_TEST_ON	0x04	
#define	SM_TEST_OFF	0x02	
#define	SM_STEP		0x01	

#define	SM_SV_IDLE	0x0	
#define	SM_SV_RES_START	0x1	
#define	SM_SV_GET_DESC	0x3	
#define	SM_SV_CHECK	0x2	
#define	SM_SV_MOV_DATA	0x6	
#define	SM_SV_PUT_DESC	0x7	
#define	SM_SV_SET_IRQ	0x5	

#define	SM_RD_IDLE	0x0	
#define	SM_RD_LOAD	0x1	
#define	SM_RD_WAIT_TC	0x3	
#define	SM_RD_RST_EOF	0x6	
#define	SM_RD_WDONE_R	0x2	
#define	SM_RD_WDONE_T	0x4	

#define	SM_TR_IDLE	0x0	
#define	SM_TR_LOAD	0x3	
#define	SM_TR_LOAD_R_ML	0x1	
#define	SM_TR_WAIT_TC	0x2	
#define	SM_TR_WDONE	0x4	

#define	SM_WR_IDLE	0x0	
#define	SM_WR_ABLEN	0x1	
#define	SM_WR_LD_A4	0x2	
#define	SM_WR_RES_OWN	0x2	
#define	SM_WR_WAIT_EOF	0x3	
#define	SM_WR_LD_N2C_R	0x4	
#define	SM_WR_WAIT_TC_R	0x5	
#define	SM_WR_WAIT_TC4	0x6	
#define	SM_WR_LD_A_T	0x6	
#define	SM_WR_LD_A_R	0x7	
#define	SM_WR_WAIT_TC_T	0x7	
#define	SM_WR_LD_N2C_T	0xc	
#define	SM_WR_WDONE_T	0x9	
#define	SM_WR_WDONE_R	0xc	
#define SM_WR_LD_D_AD	0xe	
#define SM_WR_WAIT_D_TC	0xf	

				
#define	AC_TEST_ON	(1<<7)	
#define	AC_TEST_OFF	(1<<6)	
#define	BC_TEST_ON	(1<<5)	
#define	BC_TEST_OFF	(1<<4)	
#define	TEST_STEP04	(1<<3)	
#define	TEST_STEP03	(1<<2)	
#define	TEST_STEP02	(1<<1)	
#define	TEST_STEP01	(1<<0)	

				
#define T3_MUX_2	(1<<7)	
#define T3_VRAM_2	(1<<6)	
#define	T3_LOOP		(1<<5)	
#define	T3_UNLOOP	(1<<4)	
#define	T3_MUX		(3<<2)	
#define	T3_VRAM		(3<<0)	

#define	PCI_VEND_ID0	0x48		
#define	PCI_VEND_ID1	0x11		
					
#define	PCI_DEV_ID0	0x00		
#define	PCI_DEV_ID1	0x40		

		
#define PCI_NW_CLASS	0x02		
#define PCI_SUB_CLASS	0x02		
#define PCI_PROG_INTFC	0x00		

#define	FMA(a)	(0x0400|((a)<<2))	
#define	P1(a)	(0x0380|((a)<<2))	
#define	P2(a)	(0x0600|((a)<<2))	
#define PRA(a)	(B2_MAC_0 + (a))	

#define	MAX_PAGES	0x20000L	
#define	MAX_FADDR	1		

#define	BMU_OWN		(1UL<<31)	
#define	BMU_STF		(1L<<30)	
#define	BMU_EOF		(1L<<29)	
#define	BMU_EN_IRQ_EOB	(1L<<28)	
#define	BMU_EN_IRQ_EOF	(1L<<27)	
#define	BMU_DEV_0	(1L<<26)	
#define BMU_SMT_TX	(1L<<25)	
#define BMU_ST_BUF	(1L<<25)	
#define BMU_UNUSED	(1L<<24)	
#define BMU_SW		(3L<<24)	
#define	BMU_CHECK	0x00550000L	
#define	BMU_BBC		0x0000FFFFL	

#ifdef MEM_MAPPED_IO
#define	ADDR(a)		(char far *) smc->hw.iop+(a)
#define	ADDRS(smc,a)	(char far *) (smc)->hw.iop+(a)
#else
#define	ADDR(a)	(((a)>>7) ? (outp(smc->hw.iop+B0_RAP,(a)>>7), \
	(smc->hw.iop+(((a)&0x7F)|((a)>>7 ? 0x80:0)))) : \
	(smc->hw.iop+(((a)&0x7F)|((a)>>7 ? 0x80:0))))
#define	ADDRS(smc,a) (((a)>>7) ? (outp((smc)->hw.iop+B0_RAP,(a)>>7), \
	((smc)->hw.iop+(((a)&0x7F)|((a)>>7 ? 0x80:0)))) : \
	((smc)->hw.iop+(((a)&0x7F)|((a)>>7 ? 0x80:0))))
#endif

#define PCI_C(a)	ADDR(B3_CFG_SPC + (a))	

#define EXT_R(a)	ADDR(B6_EXT_REG + (a))	

#define	SA_MAC		(0)	
#define	PRA_OFF		(0)	

#define	SKFDDI_PSZ	8	

#define	FM_A(a)	ADDR(FMA(a))	
#define	P1_A(a)	ADDR(P1(a))	
#define	P2_A(a)	ADDR(P2(a))	
#define PR_A(a)	ADDR(PRA(a))	

#define	READ_PROM(a)	((u_char)inp(a))

#define	GET_PAGE(bank)	outpd(ADDR(B2_FAR),bank)
#define	VPP_ON()
#define	VPP_OFF()

#define ISR_A		ADDR(B0_ISRC)
#define	GET_ISR()		inpd(ISR_A)
#define GET_ISR_SMP(iop)	inpd((iop)+B0_ISRC)
#define	CHECK_ISR()		(inpd(ISR_A) & inpd(ADDR(B0_IMSK)))
#define CHECK_ISR_SMP(iop)	(inpd((iop)+B0_ISRC) & inpd((iop)+B0_IMSK))

#define	BUS_CHECK()

#ifndef UNIX
#define	CLI_FBI()	outpd(ADDR(B0_IMSK),0)
#else
#define	CLI_FBI(smc)	outpd(ADDRS((smc),B0_IMSK),0)
#endif

#ifndef UNIX
#define	STI_FBI()	outpd(ADDR(B0_IMSK),smc->hw.is_imask)
#else
#define	STI_FBI(smc)	outpd(ADDRS((smc),B0_IMSK),(smc)->hw.is_imask)
#endif

#define CLI_FBI_SMP(iop)	outpd((iop)+B0_IMSK,0)
#define	STI_FBI_SMP(smc,iop)	outpd((iop)+B0_IMSK,(smc)->hw.is_imask)

#endif	

#define	MAX_TRANS	(0x0fff)

#define	MST_8259 (0x20)
#define	SLV_8259 (0xA0)

#define TPS		(18)		

#define	TN		(4)	
#define	SNPPND_TIME	(5)	

#define	MAC_AD	0x405a0000

#define MODR1	FM_A(FM_MDREG1)	
#define MODR2	FM_A(FM_MDREG2)	

#define CMDR1	FM_A(FM_CMDREG1)	
#define CMDR2	FM_A(FM_CMDREG2)	


#define	CLEAR(io,mask)		outpw((io),inpw(io)&(~(mask)))
#define	SET(io,mask)		outpw((io),inpw(io)|(mask))
#define	GET(io,mask)		(inpw(io)&(mask))
#define	SETMASK(io,val,mask)	outpw((io),(inpw(io) & ~(mask)) | (val))

#define	PLC(np,reg)	(((np) == PA) ? P2_A(reg) : P1_A(reg))

#define	MARW(ma)	outpw(FM_A(FM_MARW),(unsigned int)(ma))
#define	MARR(ma)	outpw(FM_A(FM_MARR),(unsigned int)(ma))

#define	MDRW(dd)	outpw(FM_A(FM_MDRU),(unsigned int)((dd)>>16)) ;\
			outpw(FM_A(FM_MDRL),(unsigned int)(dd))

#ifndef WINNT
#define	MDRR()		(((long)inpw(FM_A(FM_MDRU))<<16) + inpw(FM_A(FM_MDRL)))

#define	GET_ST1()	(((long)inpw(FM_A(FM_ST1U))<<16) + inpw(FM_A(FM_ST1L)))
#define	GET_ST2()	(((long)inpw(FM_A(FM_ST2U))<<16) + inpw(FM_A(FM_ST2L)))
#ifdef	SUPERNET_3
#define	GET_ST3()	(((long)inpw(FM_A(FM_ST3U))<<16) + inpw(FM_A(FM_ST3L)))
#endif
#else
#define MDRR()		inp2w((FM_A(FM_MDRU)),(FM_A(FM_MDRL)))

#define GET_ST1()	inp2w((FM_A(FM_ST1U)),(FM_A(FM_ST1L)))
#define GET_ST2()	inp2w((FM_A(FM_ST2U)),(FM_A(FM_ST2L)))
#ifdef	SUPERNET_3
#define GET_ST3()	inp2w((FM_A(FM_ST3U)),(FM_A(FM_ST3L)))
#endif
#endif

				
#define	OUT_82c54_TIMER(port,val)	outpw(TI_A(port),(val)<<8)
#define	IN_82c54_TIMER(port)		((inpw(TI_A(port))>>8) & 0xff)


#ifdef	DEBUG
#define	DB_MAC(mac,st) {if (debug_mac & 0x1)\
				printf("M") ;\
			if (debug_mac & 0x2)\
				printf("\tMAC %d status 0x%08lx\n",mac,st) ;\
			if (debug_mac & 0x4)\
				dp_mac(mac,st) ;\
}

#define	DB_PLC(p,iev) {	if (debug_plc & 0x1)\
				printf("P") ;\
			if (debug_plc & 0x2)\
				printf("\tPLC %s Int 0x%04x\n", \
					(p == PA) ? "A" : "B", iev) ;\
			if (debug_plc & 0x4)\
				dp_plc(p,iev) ;\
}

#define	DB_TIMER() {	if (debug_timer & 0x1)\
				printf("T") ;\
			if (debug_timer & 0x2)\
				printf("\tTimer ISR\n") ;\
}

#else	

#define	DB_MAC(mac,st)
#define	DB_PLC(p,iev)
#define	DB_TIMER()

#endif	

#define	INC_PTR(sp,cp,ep)	if (++cp == ep) cp = sp
#define	COUNT(t)	((t)<<6)	
#define	RW_OP(o)	((o)<<4)	
#define	TMODE(m)	((m)<<1)	

#endif
