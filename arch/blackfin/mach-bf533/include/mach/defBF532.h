/*
 * System & MMR bit and Address definitions for ADSP-BF532
 *
 * Copyright 2005-2010 Analog Devices Inc.
 *
 * Licensed under the ADI BSD license or the GPL-2 (or later)
 */

#ifndef _DEF_BF532_H
#define _DEF_BF532_H


#define PLL_CTL                0xFFC00000	
#define PLL_DIV			 0xFFC00004	
#define VR_CTL			 0xFFC00008	
#define PLL_STAT               0xFFC0000C	
#define PLL_LOCKCNT            0xFFC00010	
#define CHIPID                 0xFFC00014       

#define SWRST			0xFFC00100  
#define SYSCR			0xFFC00104  
#define SIC_RVECT             		0xFFC00108	
#define SIC_IMASK             		0xFFC0010C	
#define SIC_IAR0               		0xFFC00110	
#define SIC_IAR1               		0xFFC00114	
#define SIC_IAR2              		0xFFC00118	
#define SIC_ISR                		0xFFC00120	
#define SIC_IWR                		0xFFC00124	

#define WDOG_CTL                	0xFFC00200	
#define WDOG_CNT                	0xFFC00204	
#define WDOG_STAT               	0xFFC00208	

#define RTC_STAT                	0xFFC00300	
#define RTC_ICTL                	0xFFC00304	
#define RTC_ISTAT               	0xFFC00308	
#define RTC_SWCNT               	0xFFC0030C	
#define RTC_ALARM               	0xFFC00310	
#define RTC_FAST                	0xFFC00314	
#define RTC_PREN			0xFFC00314	


#define BFIN_UART_THR			0xFFC00400	
#define BFIN_UART_RBR			0xFFC00400	
#define BFIN_UART_DLL			0xFFC00400	
#define BFIN_UART_IER			0xFFC00404	
#define BFIN_UART_DLH			0xFFC00404	
#define BFIN_UART_IIR			0xFFC00408	
#define BFIN_UART_LCR			0xFFC0040C	
#define BFIN_UART_MCR			0xFFC00410	
#define BFIN_UART_LSR			0xFFC00414	
#if 0
#define BFIN_UART_MSR			0xFFC00418	
#endif
#define BFIN_UART_SCR			0xFFC0041C	
#define BFIN_UART_GCTL			0xFFC00424	

#define SPI0_REGBASE          		0xFFC00500
#define SPI_CTL               		0xFFC00500	
#define SPI_FLG               		0xFFC00504	
#define SPI_STAT              		0xFFC00508	
#define SPI_TDBR              		0xFFC0050C	
#define SPI_RDBR              		0xFFC00510	
#define SPI_BAUD              		0xFFC00514	
#define SPI_SHADOW            		0xFFC00518	


#define TIMER0_CONFIG          		0xFFC00600	
#define TIMER0_COUNTER			0xFFC00604	
#define TIMER0_PERIOD       		0xFFC00608	
#define TIMER0_WIDTH        		0xFFC0060C	

#define TIMER1_CONFIG          		0xFFC00610	
#define TIMER1_COUNTER         		0xFFC00614	
#define TIMER1_PERIOD          		0xFFC00618	
#define TIMER1_WIDTH           		0xFFC0061C	

#define TIMER2_CONFIG          		0xFFC00620	
#define TIMER2_COUNTER         		0xFFC00624	
#define TIMER2_PERIOD          		0xFFC00628	
#define TIMER2_WIDTH           		0xFFC0062C	

#define TIMER_ENABLE			0xFFC00640	
#define TIMER_DISABLE			0xFFC00644	
#define TIMER_STATUS			0xFFC00648	


#define FIO_FLAG_D	       		0xFFC00700	
#define FIO_FLAG_C             		0xFFC00704	
#define FIO_FLAG_S             		0xFFC00708	
#define FIO_FLAG_T			0xFFC0070C	
#define FIO_MASKA_D            		0xFFC00710	
#define FIO_MASKA_C            		0xFFC00714	
#define FIO_MASKA_S            		0xFFC00718	
#define FIO_MASKA_T            		0xFFC0071C	
#define FIO_MASKB_D            		0xFFC00720	
#define FIO_MASKB_C            		0xFFC00724	
#define FIO_MASKB_S            		0xFFC00728	
#define FIO_MASKB_T            		0xFFC0072C	
#define FIO_DIR                		0xFFC00730	
#define FIO_POLAR              		0xFFC00734	
#define FIO_EDGE               		0xFFC00738	
#define FIO_BOTH               		0xFFC0073C	
#define FIO_INEN					0xFFC00740	

#define SPORT0_TCR1     	 	0xFFC00800	
#define SPORT0_TCR2      	 	0xFFC00804	
#define SPORT0_TCLKDIV        		0xFFC00808	
#define SPORT0_TFSDIV          		0xFFC0080C	
#define SPORT0_TX	             	0xFFC00810	
#define SPORT0_RX	            	0xFFC00818	
#define SPORT0_RCR1      	 	0xFFC00820	
#define SPORT0_RCR2      	 	0xFFC00824	
#define SPORT0_RCLKDIV        		0xFFC00828	
#define SPORT0_RFSDIV          		0xFFC0082C	
#define SPORT0_STAT            		0xFFC00830	
#define SPORT0_CHNL            		0xFFC00834	
#define SPORT0_MCMC1           		0xFFC00838	
#define SPORT0_MCMC2           		0xFFC0083C	
#define SPORT0_MTCS0           		0xFFC00840	
#define SPORT0_MTCS1           		0xFFC00844	
#define SPORT0_MTCS2           		0xFFC00848	
#define SPORT0_MTCS3           		0xFFC0084C	
#define SPORT0_MRCS0           		0xFFC00850	
#define SPORT0_MRCS1           		0xFFC00854	
#define SPORT0_MRCS2           		0xFFC00858	
#define SPORT0_MRCS3           		0xFFC0085C	

#define SPORT1_TCR1     	 	0xFFC00900	
#define SPORT1_TCR2      	 	0xFFC00904	
#define SPORT1_TCLKDIV        		0xFFC00908	
#define SPORT1_TFSDIV          		0xFFC0090C	
#define SPORT1_TX	             	0xFFC00910	
#define SPORT1_RX	            	0xFFC00918	
#define SPORT1_RCR1      	 	0xFFC00920	
#define SPORT1_RCR2      	 	0xFFC00924	
#define SPORT1_RCLKDIV        		0xFFC00928	
#define SPORT1_RFSDIV          		0xFFC0092C	
#define SPORT1_STAT            		0xFFC00930	
#define SPORT1_CHNL            		0xFFC00934	
#define SPORT1_MCMC1           		0xFFC00938	
#define SPORT1_MCMC2           		0xFFC0093C	
#define SPORT1_MTCS0           		0xFFC00940	
#define SPORT1_MTCS1           		0xFFC00944	
#define SPORT1_MTCS2           		0xFFC00948	
#define SPORT1_MTCS3           		0xFFC0094C	
#define SPORT1_MRCS0           		0xFFC00950	
#define SPORT1_MRCS1           		0xFFC00954	
#define SPORT1_MRCS2           		0xFFC00958	
#define SPORT1_MRCS3           		0xFFC0095C	

#define EBIU_AMGCTL			0xFFC00A00	
#define EBIU_AMBCTL0			0xFFC00A04	
#define EBIU_AMBCTL1			0xFFC00A08	


#define EBIU_SDGCTL			0xFFC00A10	
#define EBIU_SDBCTL			0xFFC00A14	
#define EBIU_SDRRC 			0xFFC00A18	
#define EBIU_SDSTAT			0xFFC00A1C	

#define DMAC_TC_PER 0xFFC00B0C	
#define DMAC_TC_CNT 0xFFC00B10	

#define DMA0_CONFIG		0xFFC00C08	
#define DMA0_NEXT_DESC_PTR	0xFFC00C00	
#define DMA0_START_ADDR		0xFFC00C04	
#define DMA0_X_COUNT		0xFFC00C10	
#define DMA0_Y_COUNT		0xFFC00C18	
#define DMA0_X_MODIFY		0xFFC00C14	
#define DMA0_Y_MODIFY		0xFFC00C1C	
#define DMA0_CURR_DESC_PTR	0xFFC00C20	
#define DMA0_CURR_ADDR		0xFFC00C24	
#define DMA0_CURR_X_COUNT	0xFFC00C30	
#define DMA0_CURR_Y_COUNT	0xFFC00C38	
#define DMA0_IRQ_STATUS		0xFFC00C28	
#define DMA0_PERIPHERAL_MAP	0xFFC00C2C	

#define DMA1_CONFIG		0xFFC00C48	
#define DMA1_NEXT_DESC_PTR	0xFFC00C40	
#define DMA1_START_ADDR		0xFFC00C44	
#define DMA1_X_COUNT		0xFFC00C50	
#define DMA1_Y_COUNT		0xFFC00C58	
#define DMA1_X_MODIFY		0xFFC00C54	
#define DMA1_Y_MODIFY		0xFFC00C5C	
#define DMA1_CURR_DESC_PTR	0xFFC00C60	
#define DMA1_CURR_ADDR		0xFFC00C64	
#define DMA1_CURR_X_COUNT	0xFFC00C70	
#define DMA1_CURR_Y_COUNT	0xFFC00C78	
#define DMA1_IRQ_STATUS		0xFFC00C68	
#define DMA1_PERIPHERAL_MAP	0xFFC00C6C	

#define DMA2_CONFIG		0xFFC00C88	
#define DMA2_NEXT_DESC_PTR	0xFFC00C80	
#define DMA2_START_ADDR		0xFFC00C84	
#define DMA2_X_COUNT		0xFFC00C90	
#define DMA2_Y_COUNT		0xFFC00C98	
#define DMA2_X_MODIFY		0xFFC00C94	
#define DMA2_Y_MODIFY		0xFFC00C9C	
#define DMA2_CURR_DESC_PTR	0xFFC00CA0	
#define DMA2_CURR_ADDR		0xFFC00CA4	
#define DMA2_CURR_X_COUNT	0xFFC00CB0	
#define DMA2_CURR_Y_COUNT	0xFFC00CB8	
#define DMA2_IRQ_STATUS		0xFFC00CA8	
#define DMA2_PERIPHERAL_MAP	0xFFC00CAC	

#define DMA3_CONFIG		0xFFC00CC8	
#define DMA3_NEXT_DESC_PTR	0xFFC00CC0	
#define DMA3_START_ADDR		0xFFC00CC4	
#define DMA3_X_COUNT		0xFFC00CD0	
#define DMA3_Y_COUNT		0xFFC00CD8	
#define DMA3_X_MODIFY		0xFFC00CD4	
#define DMA3_Y_MODIFY		0xFFC00CDC	
#define DMA3_CURR_DESC_PTR	0xFFC00CE0	
#define DMA3_CURR_ADDR		0xFFC00CE4	
#define DMA3_CURR_X_COUNT	0xFFC00CF0	
#define DMA3_CURR_Y_COUNT	0xFFC00CF8	
#define DMA3_IRQ_STATUS		0xFFC00CE8	
#define DMA3_PERIPHERAL_MAP	0xFFC00CEC	

#define DMA4_CONFIG		0xFFC00D08	
#define DMA4_NEXT_DESC_PTR	0xFFC00D00	
#define DMA4_START_ADDR		0xFFC00D04	
#define DMA4_X_COUNT		0xFFC00D10	
#define DMA4_Y_COUNT		0xFFC00D18	
#define DMA4_X_MODIFY		0xFFC00D14	
#define DMA4_Y_MODIFY		0xFFC00D1C	
#define DMA4_CURR_DESC_PTR	0xFFC00D20	
#define DMA4_CURR_ADDR		0xFFC00D24	
#define DMA4_CURR_X_COUNT	0xFFC00D30	
#define DMA4_CURR_Y_COUNT	0xFFC00D38	
#define DMA4_IRQ_STATUS		0xFFC00D28	
#define DMA4_PERIPHERAL_MAP	0xFFC00D2C	

#define DMA5_CONFIG		0xFFC00D48	
#define DMA5_NEXT_DESC_PTR	0xFFC00D40	
#define DMA5_START_ADDR		0xFFC00D44	
#define DMA5_X_COUNT		0xFFC00D50	
#define DMA5_Y_COUNT		0xFFC00D58	
#define DMA5_X_MODIFY		0xFFC00D54	
#define DMA5_Y_MODIFY		0xFFC00D5C	
#define DMA5_CURR_DESC_PTR	0xFFC00D60	
#define DMA5_CURR_ADDR		0xFFC00D64	
#define DMA5_CURR_X_COUNT	0xFFC00D70	
#define DMA5_CURR_Y_COUNT	0xFFC00D78	
#define DMA5_IRQ_STATUS		0xFFC00D68	
#define DMA5_PERIPHERAL_MAP	0xFFC00D6C	

#define DMA6_CONFIG		0xFFC00D88	
#define DMA6_NEXT_DESC_PTR	0xFFC00D80	
#define DMA6_START_ADDR		0xFFC00D84	
#define DMA6_X_COUNT		0xFFC00D90	
#define DMA6_Y_COUNT		0xFFC00D98	
#define DMA6_X_MODIFY		0xFFC00D94	
#define DMA6_Y_MODIFY		0xFFC00D9C	
#define DMA6_CURR_DESC_PTR	0xFFC00DA0	
#define DMA6_CURR_ADDR		0xFFC00DA4	
#define DMA6_CURR_X_COUNT	0xFFC00DB0	
#define DMA6_CURR_Y_COUNT	0xFFC00DB8	
#define DMA6_IRQ_STATUS		0xFFC00DA8	
#define DMA6_PERIPHERAL_MAP	0xFFC00DAC	

#define DMA7_CONFIG		0xFFC00DC8	
#define DMA7_NEXT_DESC_PTR	0xFFC00DC0	
#define DMA7_START_ADDR		0xFFC00DC4	
#define DMA7_X_COUNT		0xFFC00DD0	
#define DMA7_Y_COUNT		0xFFC00DD8	
#define DMA7_X_MODIFY		0xFFC00DD4	
#define DMA7_Y_MODIFY		0xFFC00DDC	
#define DMA7_CURR_DESC_PTR	0xFFC00DE0	
#define DMA7_CURR_ADDR		0xFFC00DE4	
#define DMA7_CURR_X_COUNT	0xFFC00DF0	
#define DMA7_CURR_Y_COUNT	0xFFC00DF8	
#define DMA7_IRQ_STATUS		0xFFC00DE8	
#define DMA7_PERIPHERAL_MAP	0xFFC00DEC	

#define MDMA_D1_CONFIG		0xFFC00E88	
#define MDMA_D1_NEXT_DESC_PTR	0xFFC00E80	
#define MDMA_D1_START_ADDR	0xFFC00E84	
#define MDMA_D1_X_COUNT		0xFFC00E90	
#define MDMA_D1_Y_COUNT		0xFFC00E98	
#define MDMA_D1_X_MODIFY	0xFFC00E94	
#define MDMA_D1_Y_MODIFY	0xFFC00E9C	
#define MDMA_D1_CURR_DESC_PTR	0xFFC00EA0	
#define MDMA_D1_CURR_ADDR	0xFFC00EA4	
#define MDMA_D1_CURR_X_COUNT	0xFFC00EB0	
#define MDMA_D1_CURR_Y_COUNT	0xFFC00EB8	
#define MDMA_D1_IRQ_STATUS	0xFFC00EA8	
#define MDMA_D1_PERIPHERAL_MAP	0xFFC00EAC	

#define MDMA_S1_CONFIG		0xFFC00EC8	
#define MDMA_S1_NEXT_DESC_PTR	0xFFC00EC0	
#define MDMA_S1_START_ADDR	0xFFC00EC4	
#define MDMA_S1_X_COUNT		0xFFC00ED0	
#define MDMA_S1_Y_COUNT		0xFFC00ED8	
#define MDMA_S1_X_MODIFY	0xFFC00ED4	
#define MDMA_S1_Y_MODIFY	0xFFC00EDC	
#define MDMA_S1_CURR_DESC_PTR	0xFFC00EE0	
#define MDMA_S1_CURR_ADDR	0xFFC00EE4	
#define MDMA_S1_CURR_X_COUNT	0xFFC00EF0	
#define MDMA_S1_CURR_Y_COUNT	0xFFC00EF8	
#define MDMA_S1_IRQ_STATUS	0xFFC00EE8	
#define MDMA_S1_PERIPHERAL_MAP	0xFFC00EEC	

#define MDMA_D0_CONFIG		0xFFC00E08	
#define MDMA_D0_NEXT_DESC_PTR	0xFFC00E00	
#define MDMA_D0_START_ADDR	0xFFC00E04	
#define MDMA_D0_X_COUNT		0xFFC00E10	
#define MDMA_D0_Y_COUNT		0xFFC00E18	
#define MDMA_D0_X_MODIFY	0xFFC00E14	
#define MDMA_D0_Y_MODIFY	0xFFC00E1C	
#define MDMA_D0_CURR_DESC_PTR	0xFFC00E20	
#define MDMA_D0_CURR_ADDR	0xFFC00E24	
#define MDMA_D0_CURR_X_COUNT	0xFFC00E30	
#define MDMA_D0_CURR_Y_COUNT	0xFFC00E38	
#define MDMA_D0_IRQ_STATUS	0xFFC00E28	
#define MDMA_D0_PERIPHERAL_MAP	0xFFC00E2C	

#define MDMA_S0_CONFIG		0xFFC00E48	
#define MDMA_S0_NEXT_DESC_PTR	0xFFC00E40	
#define MDMA_S0_START_ADDR	0xFFC00E44	
#define MDMA_S0_X_COUNT		0xFFC00E50	
#define MDMA_S0_Y_COUNT		0xFFC00E58	
#define MDMA_S0_X_MODIFY	0xFFC00E54	
#define MDMA_S0_Y_MODIFY	0xFFC00E5C	
#define MDMA_S0_CURR_DESC_PTR	0xFFC00E60	
#define MDMA_S0_CURR_ADDR	0xFFC00E64	
#define MDMA_S0_CURR_X_COUNT	0xFFC00E70	
#define MDMA_S0_CURR_Y_COUNT	0xFFC00E78	
#define MDMA_S0_IRQ_STATUS	0xFFC00E68	
#define MDMA_S0_PERIPHERAL_MAP	0xFFC00E6C	


#define PPI_CONTROL			0xFFC01000	
#define PPI_STATUS			0xFFC01004	
#define PPI_COUNT			0xFFC01008	
#define PPI_DELAY			0xFFC0100C	
#define PPI_FRAME			0xFFC01010	


#define CHIPID_VERSION         0xF0000000
#define CHIPID_FAMILY          0x0FFFF000
#define CHIPID_MANUFACTURE     0x00000FFE

#define SYSTEM_RESET	0x0007	
#define	DOUBLE_FAULT	0x0008	
#define RESET_DOUBLE	0x2000	
#define RESET_WDOG	0x4000	
#define RESET_SOFTWARE	0x8000	

#define BMODE			0x0006	
#define	NOBOOT			0x0010	


    

#define P0_IVG(x)    ((x)-7)	
#define P1_IVG(x)    ((x)-7) << 0x4	
#define P2_IVG(x)    ((x)-7) << 0x8	
#define P3_IVG(x)    ((x)-7) << 0xC	
#define P4_IVG(x)    ((x)-7) << 0x10	
#define P5_IVG(x)    ((x)-7) << 0x14	
#define P6_IVG(x)    ((x)-7) << 0x18	
#define P7_IVG(x)    ((x)-7) << 0x1C	


#define P8_IVG(x)     ((x)-7)	
#define P9_IVG(x)     ((x)-7) << 0x4	
#define P10_IVG(x)    ((x)-7) << 0x8	
#define P11_IVG(x)    ((x)-7) << 0xC	
#define P12_IVG(x)    ((x)-7) << 0x10	
#define P13_IVG(x)    ((x)-7) << 0x14	
#define P14_IVG(x)    ((x)-7) << 0x18	
#define P15_IVG(x)    ((x)-7) << 0x1C	

#define P16_IVG(x)    ((x)-7)	
#define P17_IVG(x)    ((x)-7) << 0x4	
#define P18_IVG(x)    ((x)-7) << 0x8	
#define P19_IVG(x)    ((x)-7) << 0xC	
#define P20_IVG(x)    ((x)-7) << 0x10	
#define P21_IVG(x)    ((x)-7) << 0x14	
#define P22_IVG(x)    ((x)-7) << 0x18	
#define P23_IVG(x)    ((x)-7) << 0x1C	

#define SIC_UNMASK_ALL         0x00000000	
#define SIC_MASK_ALL           0xFFFFFFFF	
#define SIC_MASK(x)	       (1 << (x))	
#define SIC_UNMASK(x) (0xFFFFFFFF ^ (1 << (x)))	

#define IWR_DISABLE_ALL        0x00000000	
#define IWR_ENABLE_ALL         0xFFFFFFFF	
#define IWR_ENABLE(x)	       (1 << (x))	
#define IWR_DISABLE(x) (0xFFFFFFFF ^ (1 << (x)))	


#define PORT_EN              0x00000001	
#define PORT_DIR             0x00000002	
#define XFR_TYPE             0x0000000C	
#define PORT_CFG             0x00000030	
#define FLD_SEL              0x00000040	
#define PACK_EN              0x00000080	
#define DMA32                0x00000100	
#define SKIP_EN              0x00000200	
#define SKIP_EO              0x00000400	
#define DLENGTH              0x00003800	
#define DLEN_8			0x0000	
#define DLEN_10			0x0800	
#define DLEN_11			0x1000	
#define DLEN_12			0x1800	
#define DLEN_13			0x2000	
#define DLEN_14			0x2800	
#define DLEN_15			0x3000	
#define DLEN_16			0x3800	
#define DLEN(x)	(((x-9) & 0x07) << 11)	
#define POL                  0x0000C000	
#define POLC		0x4000		
#define POLS		0x8000		

#define FLD	             0x00000400	
#define FT_ERR	             0x00000800	
#define OVR	             0x00001000	
#define UNDR	             0x00002000	
#define ERR_DET	      	     0x00004000	
#define ERR_NCOR	     0x00008000	



#define CTYPE	            0x00000040	
#define CTYPE_P             6	
#define PCAP8	            0x00000080	
#define PCAP16	            0x00000100	
#define PCAP32	            0x00000200	
#define PCAPWR	            0x00000400	
#define PCAPRD	            0x00000800	
#define PMAP	            0x00007000	

#define PMAP_PPI		0x0000	
#define	PMAP_SPORT0RX		0x1000	
#define PMAP_SPORT0TX		0x2000	
#define	PMAP_SPORT1RX		0x3000	
#define PMAP_SPORT1TX		0x4000	
#define PMAP_SPI		0x5000	
#define PMAP_UARTRX		0x6000	
#define PMAP_UARTTX		0x7000	



#define TIMEN0	0x0001
#define TIMEN1	0x0002
#define TIMEN2	0x0004

#define TIMEN0_P	0x00
#define TIMEN1_P	0x01
#define TIMEN2_P	0x02

#define TIMDIS0	0x0001
#define TIMDIS1	0x0002
#define TIMDIS2	0x0004

#define TIMDIS0_P	0x00
#define TIMDIS1_P	0x01
#define TIMDIS2_P	0x02

#define TIMIL0		0x0001
#define TIMIL1		0x0002
#define TIMIL2		0x0004
#define TOVF_ERR0		0x0010	
#define TOVF_ERR1		0x0020	
#define TOVF_ERR2		0x0040	
#define TRUN0		0x1000
#define TRUN1		0x2000
#define TRUN2		0x4000

#define TIMIL0_P	0x00
#define TIMIL1_P	0x01
#define TIMIL2_P	0x02
#define TOVF_ERR0_P		0x04
#define TOVF_ERR1_P		0x05
#define TOVF_ERR2_P		0x06
#define TRUN0_P		0x0C
#define TRUN1_P		0x0D
#define TRUN2_P		0x0E

#define TOVL_ERR0 		TOVF_ERR0
#define TOVL_ERR1 		TOVF_ERR1
#define TOVL_ERR2 		TOVF_ERR2
#define TOVL_ERR0_P		TOVF_ERR0_P
#define TOVL_ERR1_P 		TOVF_ERR1_P
#define TOVL_ERR2_P 		TOVF_ERR2_P

#define PWM_OUT		0x0001
#define WDTH_CAP	0x0002
#define EXT_CLK		0x0003
#define PULSE_HI	0x0004
#define PERIOD_CNT	0x0008
#define IRQ_ENA		0x0010
#define TIN_SEL		0x0020
#define OUT_DIS		0x0040
#define CLK_SEL		0x0080
#define TOGGLE_HI	0x0100
#define EMU_RUN		0x0200
#define ERR_TYP(x)	((x & 0x03) << 14)

#define TMODE_P0		0x00
#define TMODE_P1		0x01
#define PULSE_HI_P		0x02
#define PERIOD_CNT_P		0x03
#define IRQ_ENA_P		0x04
#define TIN_SEL_P		0x05
#define OUT_DIS_P		0x06
#define CLK_SEL_P		0x07
#define TOGGLE_HI_P		0x08
#define EMU_RUN_P		0x09
#define ERR_TYP_P0		0x0E
#define ERR_TYP_P1		0x0F


#define AMCKEN			0x00000001	
#define	AMBEN_NONE		0x00000000	
#define AMBEN_B0		0x00000002	
#define AMBEN_B0_B1		0x00000004	
#define AMBEN_B0_B1_B2		0x00000006	
#define AMBEN_ALL		0x00000008	

#define AMCKEN_P		0x00000000	
#define AMBEN_P0		0x00000001	
#define AMBEN_P1		0x00000002	
#define AMBEN_P2		0x00000003	

#define B0RDYEN	0x00000001	
#define B0RDYPOL 0x00000002	
#define B0TT_1	0x00000004	
#define B0TT_2	0x00000008	
#define B0TT_3	0x0000000C	
#define B0TT_4	0x00000000	
#define B0ST_1	0x00000010	
#define B0ST_2	0x00000020	
#define B0ST_3	0x00000030	
#define B0ST_4	0x00000000	
#define B0HT_1	0x00000040	
#define B0HT_2	0x00000080	
#define B0HT_3	0x000000C0	
#define B0HT_0	0x00000000	
#define B0RAT_1			0x00000100	
#define B0RAT_2			0x00000200	
#define B0RAT_3			0x00000300	
#define B0RAT_4			0x00000400	
#define B0RAT_5			0x00000500	
#define B0RAT_6			0x00000600	
#define B0RAT_7			0x00000700	
#define B0RAT_8			0x00000800	
#define B0RAT_9			0x00000900	
#define B0RAT_10		0x00000A00	
#define B0RAT_11		0x00000B00	
#define B0RAT_12		0x00000C00	
#define B0RAT_13		0x00000D00	
#define B0RAT_14		0x00000E00	
#define B0RAT_15		0x00000F00	
#define B0WAT_1			0x00001000	
#define B0WAT_2			0x00002000	
#define B0WAT_3			0x00003000	
#define B0WAT_4			0x00004000	
#define B0WAT_5			0x00005000	
#define B0WAT_6			0x00006000	
#define B0WAT_7			0x00007000	
#define B0WAT_8			0x00008000	
#define B0WAT_9			0x00009000	
#define B0WAT_10		0x0000A000	
#define B0WAT_11		0x0000B000	
#define B0WAT_12		0x0000C000	
#define B0WAT_13		0x0000D000	
#define B0WAT_14		0x0000E000	
#define B0WAT_15		0x0000F000	
#define B1RDYEN			0x00010000	
#define B1RDYPOL		0x00020000	
#define B1TT_1			0x00040000	
#define B1TT_2			0x00080000	
#define B1TT_3			0x000C0000	
#define B1TT_4			0x00000000	
#define B1ST_1			0x00100000	
#define B1ST_2			0x00200000	
#define B1ST_3			0x00300000	
#define B1ST_4			0x00000000	
#define B1HT_1			0x00400000	
#define B1HT_2			0x00800000	
#define B1HT_3			0x00C00000	
#define B1HT_0			0x00000000	
#define B1RAT_1			0x01000000	
#define B1RAT_2			0x02000000	
#define B1RAT_3			0x03000000	
#define B1RAT_4			0x04000000	
#define B1RAT_5			0x05000000	
#define B1RAT_6			0x06000000	
#define B1RAT_7			0x07000000	
#define B1RAT_8			0x08000000	
#define B1RAT_9			0x09000000	
#define B1RAT_10		0x0A000000	
#define B1RAT_11		0x0B000000	
#define B1RAT_12		0x0C000000	
#define B1RAT_13		0x0D000000	
#define B1RAT_14		0x0E000000	
#define B1RAT_15		0x0F000000	
#define B1WAT_1			0x10000000	
#define B1WAT_2			0x20000000	
#define B1WAT_3			0x30000000	
#define B1WAT_4			0x40000000	
#define B1WAT_5			0x50000000	
#define B1WAT_6			0x60000000	
#define B1WAT_7			0x70000000	
#define B1WAT_8			0x80000000	
#define B1WAT_9			0x90000000	
#define B1WAT_10		0xA0000000	
#define B1WAT_11		0xB0000000	
#define B1WAT_12		0xC0000000	
#define B1WAT_13		0xD0000000	
#define B1WAT_14		0xE0000000	
#define B1WAT_15		0xF0000000	

#define B2RDYEN			0x00000001	
#define B2RDYPOL		0x00000002	
#define B2TT_1			0x00000004	
#define B2TT_2			0x00000008	
#define B2TT_3			0x0000000C	
#define B2TT_4			0x00000000	
#define B2ST_1			0x00000010	
#define B2ST_2			0x00000020	
#define B2ST_3			0x00000030	
#define B2ST_4			0x00000000	
#define B2HT_1			0x00000040	
#define B2HT_2			0x00000080	
#define B2HT_3			0x000000C0	
#define B2HT_0			0x00000000	
#define B2RAT_1			0x00000100	
#define B2RAT_2			0x00000200	
#define B2RAT_3			0x00000300	
#define B2RAT_4			0x00000400	
#define B2RAT_5			0x00000500	
#define B2RAT_6			0x00000600	
#define B2RAT_7			0x00000700	
#define B2RAT_8			0x00000800	
#define B2RAT_9			0x00000900	
#define B2RAT_10		0x00000A00	
#define B2RAT_11		0x00000B00	
#define B2RAT_12		0x00000C00	
#define B2RAT_13		0x00000D00	
#define B2RAT_14		0x00000E00	
#define B2RAT_15		0x00000F00	
#define B2WAT_1			0x00001000	
#define B2WAT_2			0x00002000	
#define B2WAT_3			0x00003000	
#define B2WAT_4			0x00004000	
#define B2WAT_5			0x00005000	
#define B2WAT_6			0x00006000	
#define B2WAT_7			0x00007000	
#define B2WAT_8			0x00008000	
#define B2WAT_9			0x00009000	
#define B2WAT_10		0x0000A000	
#define B2WAT_11		0x0000B000	
#define B2WAT_12		0x0000C000	
#define B2WAT_13		0x0000D000	
#define B2WAT_14		0x0000E000	
#define B2WAT_15		0x0000F000	
#define B3RDYEN			0x00010000	
#define B3RDYPOL		0x00020000	
#define B3TT_1			0x00040000	
#define B3TT_2			0x00080000	
#define B3TT_3			0x000C0000	
#define B3TT_4			0x00000000	
#define B3ST_1			0x00100000	
#define B3ST_2			0x00200000	
#define B3ST_3			0x00300000	
#define B3ST_4			0x00000000	
#define B3HT_1			0x00400000	
#define B3HT_2			0x00800000	
#define B3HT_3			0x00C00000	
#define B3HT_0			0x00000000	
#define B3RAT_1			0x01000000	
#define B3RAT_2			0x02000000	
#define B3RAT_3			0x03000000	
#define B3RAT_4			0x04000000	
#define B3RAT_5			0x05000000	
#define B3RAT_6			0x06000000	
#define B3RAT_7			0x07000000	
#define B3RAT_8			0x08000000	
#define B3RAT_9			0x09000000	
#define B3RAT_10		0x0A000000	
#define B3RAT_11		0x0B000000	
#define B3RAT_12		0x0C000000	
#define B3RAT_13		0x0D000000	
#define B3RAT_14		0x0E000000	
#define B3RAT_15		0x0F000000	
#define B3WAT_1			0x10000000	
#define B3WAT_2			0x20000000	
#define B3WAT_3			0x30000000	
#define B3WAT_4			0x40000000	
#define B3WAT_5			0x50000000	
#define B3WAT_6			0x60000000	
#define B3WAT_7			0x70000000	
#define B3WAT_8			0x80000000	
#define B3WAT_9			0x90000000	
#define B3WAT_10		0xA0000000	
#define B3WAT_11		0xB0000000	
#define B3WAT_12		0xC0000000	
#define B3WAT_13		0xD0000000	
#define B3WAT_14		0xE0000000	
#define B3WAT_15		0xF0000000	


#define SCTLE			0x00000001	
#define CL_2			0x00000008	
#define CL_3			0x0000000C	
#define PFE			0x00000010	
#define PFP			0x00000020	
#define PASR_ALL		0x00000000	
#define PASR_B0_B1		0x00000010	
#define PASR_B0			0x00000020	
#define TRAS_1			0x00000040	
#define TRAS_2			0x00000080	
#define TRAS_3			0x000000C0	
#define TRAS_4			0x00000100	
#define TRAS_5			0x00000140	
#define TRAS_6			0x00000180	
#define TRAS_7			0x000001C0	
#define TRAS_8			0x00000200	
#define TRAS_9			0x00000240	
#define TRAS_10			0x00000280	
#define TRAS_11			0x000002C0	
#define TRAS_12			0x00000300	
#define TRAS_13			0x00000340	
#define TRAS_14			0x00000380	
#define TRAS_15			0x000003C0	
#define TRP_1			0x00000800	
#define TRP_2			0x00001000	
#define TRP_3			0x00001800	
#define TRP_4			0x00002000	
#define TRP_5			0x00002800	
#define TRP_6			0x00003000	
#define TRP_7			0x00003800	
#define TRCD_1			0x00008000	
#define TRCD_2			0x00010000	
#define TRCD_3			0x00018000	
#define TRCD_4			0x00020000	
#define TRCD_5			0x00028000	
#define TRCD_6			0x00030000	
#define TRCD_7			0x00038000	
#define TWR_1			0x00080000	
#define TWR_2			0x00100000	
#define TWR_3			0x00180000	
#define PUPSD			0x00200000	
#define PSM			0x00400000	
#define PSS				0x00800000	
#define SRFS			0x01000000	
#define EBUFE			0x02000000	
#define FBBRW			0x04000000	
#define EMREN			0x10000000	
#define TCSR			0x20000000	
#define CDDBG			0x40000000	

#define EBE			0x00000001	
#define EBSZ_16			0x00000000	
#define EBSZ_32			0x00000002	
#define EBSZ_64			0x00000004	
#define EBSZ_128			0x00000006	
#define EBCAW_8			0x00000000	
#define EBCAW_9			0x00000010	
#define EBCAW_10			0x00000020	
#define EBCAW_11			0x00000030	

#define SDCI			0x00000001	
#define SDSRA			0x00000002	
#define SDPUA			0x00000004	
#define SDRS			0x00000008	
#define SDEASE		      0x00000010	
#define BGSTAT			0x00000020	


#endif				
