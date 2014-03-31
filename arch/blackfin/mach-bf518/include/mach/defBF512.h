/*
 * Copyright 2008-2010 Analog Devices Inc.
 *
 * Licensed under the ADI BSD license or the GPL-2 (or later)
 */

#ifndef _DEF_BF512_H
#define _DEF_BF512_H


#define PLL_CTL				0xFFC00000	
#define PLL_DIV				0xFFC00004	
#define VR_CTL				0xFFC00008	
#define PLL_STAT			0xFFC0000C	
#define PLL_LOCKCNT			0xFFC00010	
#define CHIPID				0xFFC00014	

#define SWRST				0xFFC00100	
#define SYSCR				0xFFC00104	
#define SIC_RVECT			0xFFC00108	

#define SIC_IMASK0			0xFFC0010C	
#define SIC_IAR0			0xFFC00110	
#define SIC_IAR1			0xFFC00114	
#define SIC_IAR2			0xFFC00118	
#define SIC_IAR3			0xFFC0011C	
#define SIC_ISR0			0xFFC00120	
#define SIC_IWR0			0xFFC00124	

#define SIC_IMASK1                      0xFFC0014C     
#define SIC_IAR4                        0xFFC00150     
#define SIC_IAR5                        0xFFC00154     
#define SIC_IAR6                        0xFFC00158     
#define SIC_IAR7                        0xFFC0015C     
#define SIC_ISR1                        0xFFC00160     
#define SIC_IWR1                        0xFFC00164     


#define WDOG_CTL			0xFFC00200	
#define WDOG_CNT			0xFFC00204	
#define WDOG_STAT			0xFFC00208	


#define RTC_STAT			0xFFC00300	
#define RTC_ICTL			0xFFC00304	
#define RTC_ISTAT			0xFFC00308	
#define RTC_SWCNT			0xFFC0030C	
#define RTC_ALARM			0xFFC00310	
#define RTC_FAST			0xFFC00314	
#define RTC_PREN			0xFFC00314	


#define UART0_THR			0xFFC00400	
#define UART0_RBR			0xFFC00400	
#define UART0_DLL			0xFFC00400	
#define UART0_IER			0xFFC00404	
#define UART0_DLH			0xFFC00404	
#define UART0_IIR			0xFFC00408	
#define UART0_LCR			0xFFC0040C	
#define UART0_MCR			0xFFC00410	
#define UART0_LSR			0xFFC00414	
#define UART0_MSR			0xFFC00418	
#define UART0_SCR			0xFFC0041C	
#define UART0_GCTL			0xFFC00424	

#define SPI0_REGBASE			0xFFC00500
#define SPI0_CTL			0xFFC00500	
#define SPI0_FLG			0xFFC00504	
#define SPI0_STAT			0xFFC00508	
#define SPI0_TDBR			0xFFC0050C	
#define SPI0_RDBR			0xFFC00510	
#define SPI0_BAUD			0xFFC00514	
#define SPI0_SHADOW			0xFFC00518	

#define SPI1_REGBASE			0xFFC03400
#define SPI1_CTL			0xFFC03400	
#define SPI1_FLG			0xFFC03404	
#define SPI1_STAT			0xFFC03408	
#define SPI1_TDBR			0xFFC0340C	
#define SPI1_RDBR			0xFFC03410	
#define SPI1_BAUD			0xFFC03414	
#define SPI1_SHADOW			0xFFC03418	

#define TIMER0_CONFIG		0xFFC00600	
#define TIMER0_COUNTER		0xFFC00604	
#define TIMER0_PERIOD		0xFFC00608	
#define TIMER0_WIDTH		0xFFC0060C	

#define TIMER1_CONFIG		0xFFC00610	
#define TIMER1_COUNTER		0xFFC00614	
#define TIMER1_PERIOD		0xFFC00618	
#define TIMER1_WIDTH		0xFFC0061C	

#define TIMER2_CONFIG		0xFFC00620	
#define TIMER2_COUNTER		0xFFC00624	
#define TIMER2_PERIOD		0xFFC00628	
#define TIMER2_WIDTH		0xFFC0062C	

#define TIMER3_CONFIG		0xFFC00630	
#define TIMER3_COUNTER		0xFFC00634	
#define TIMER3_PERIOD		0xFFC00638	
#define TIMER3_WIDTH		0xFFC0063C	

#define TIMER4_CONFIG		0xFFC00640	
#define TIMER4_COUNTER		0xFFC00644	
#define TIMER4_PERIOD		0xFFC00648	
#define TIMER4_WIDTH		0xFFC0064C	

#define TIMER5_CONFIG		0xFFC00650	
#define TIMER5_COUNTER		0xFFC00654	
#define TIMER5_PERIOD		0xFFC00658	
#define TIMER5_WIDTH		0xFFC0065C	

#define TIMER6_CONFIG		0xFFC00660	
#define TIMER6_COUNTER		0xFFC00664	
#define TIMER6_PERIOD		0xFFC00668	
#define TIMER6_WIDTH		0xFFC0066C	

#define TIMER7_CONFIG		0xFFC00670	
#define TIMER7_COUNTER		0xFFC00674	
#define TIMER7_PERIOD		0xFFC00678	
#define TIMER7_WIDTH		0xFFC0067C	

#define TIMER_ENABLE		0xFFC00680	
#define TIMER_DISABLE		0xFFC00684	
#define TIMER_STATUS		0xFFC00688	

#define PORTFIO					0xFFC00700	
#define PORTFIO_CLEAR			0xFFC00704	
#define PORTFIO_SET				0xFFC00708	
#define PORTFIO_TOGGLE			0xFFC0070C	
#define PORTFIO_MASKA			0xFFC00710	
#define PORTFIO_MASKA_CLEAR		0xFFC00714	
#define PORTFIO_MASKA_SET		0xFFC00718	
#define PORTFIO_MASKA_TOGGLE	0xFFC0071C	
#define PORTFIO_MASKB			0xFFC00720	
#define PORTFIO_MASKB_CLEAR		0xFFC00724	
#define PORTFIO_MASKB_SET		0xFFC00728	
#define PORTFIO_MASKB_TOGGLE	0xFFC0072C	
#define PORTFIO_DIR				0xFFC00730	
#define PORTFIO_POLAR			0xFFC00734	
#define PORTFIO_EDGE			0xFFC00738	
#define PORTFIO_BOTH			0xFFC0073C	
#define PORTFIO_INEN			0xFFC00740	

#define SPORT0_TCR1			0xFFC00800	
#define SPORT0_TCR2			0xFFC00804	
#define SPORT0_TCLKDIV		0xFFC00808	
#define SPORT0_TFSDIV		0xFFC0080C	
#define SPORT0_TX			0xFFC00810	
#define SPORT0_RX			0xFFC00818	
#define SPORT0_RCR1			0xFFC00820	
#define SPORT0_RCR2			0xFFC00824	
#define SPORT0_RCLKDIV		0xFFC00828	
#define SPORT0_RFSDIV		0xFFC0082C	
#define SPORT0_STAT			0xFFC00830	
#define SPORT0_CHNL			0xFFC00834	
#define SPORT0_MCMC1		0xFFC00838	
#define SPORT0_MCMC2		0xFFC0083C	
#define SPORT0_MTCS0		0xFFC00840	
#define SPORT0_MTCS1		0xFFC00844	
#define SPORT0_MTCS2		0xFFC00848	
#define SPORT0_MTCS3		0xFFC0084C	
#define SPORT0_MRCS0		0xFFC00850	
#define SPORT0_MRCS1		0xFFC00854	
#define SPORT0_MRCS2		0xFFC00858	
#define SPORT0_MRCS3		0xFFC0085C	

#define SPORT1_TCR1			0xFFC00900	
#define SPORT1_TCR2			0xFFC00904	
#define SPORT1_TCLKDIV		0xFFC00908	
#define SPORT1_TFSDIV		0xFFC0090C	
#define SPORT1_TX			0xFFC00910	
#define SPORT1_RX			0xFFC00918	
#define SPORT1_RCR1			0xFFC00920	
#define SPORT1_RCR2			0xFFC00924	
#define SPORT1_RCLKDIV		0xFFC00928	
#define SPORT1_RFSDIV		0xFFC0092C	
#define SPORT1_STAT			0xFFC00930	
#define SPORT1_CHNL			0xFFC00934	
#define SPORT1_MCMC1		0xFFC00938	
#define SPORT1_MCMC2		0xFFC0093C	
#define SPORT1_MTCS0		0xFFC00940	
#define SPORT1_MTCS1		0xFFC00944	
#define SPORT1_MTCS2		0xFFC00948	
#define SPORT1_MTCS3		0xFFC0094C	
#define SPORT1_MRCS0		0xFFC00950	
#define SPORT1_MRCS1		0xFFC00954	
#define SPORT1_MRCS2		0xFFC00958	
#define SPORT1_MRCS3		0xFFC0095C	

#define EBIU_AMGCTL			0xFFC00A00	
#define EBIU_AMBCTL0		0xFFC00A04	
#define EBIU_AMBCTL1		0xFFC00A08	
#define EBIU_SDGCTL			0xFFC00A10	
#define EBIU_SDBCTL			0xFFC00A14	
#define EBIU_SDRRC			0xFFC00A18	
#define EBIU_SDSTAT			0xFFC00A1C	

#define DMAC_TC_PER			0xFFC00B0C	
#define DMAC_TC_CNT			0xFFC00B10	

#define DMA0_NEXT_DESC_PTR		0xFFC00C00	
#define DMA0_START_ADDR			0xFFC00C04	
#define DMA0_CONFIG				0xFFC00C08	
#define DMA0_X_COUNT			0xFFC00C10	
#define DMA0_X_MODIFY			0xFFC00C14	
#define DMA0_Y_COUNT			0xFFC00C18	
#define DMA0_Y_MODIFY			0xFFC00C1C	
#define DMA0_CURR_DESC_PTR		0xFFC00C20	
#define DMA0_CURR_ADDR			0xFFC00C24	
#define DMA0_IRQ_STATUS			0xFFC00C28	
#define DMA0_PERIPHERAL_MAP		0xFFC00C2C	
#define DMA0_CURR_X_COUNT		0xFFC00C30	
#define DMA0_CURR_Y_COUNT		0xFFC00C38	

#define DMA1_NEXT_DESC_PTR		0xFFC00C40	
#define DMA1_START_ADDR			0xFFC00C44	
#define DMA1_CONFIG				0xFFC00C48	
#define DMA1_X_COUNT			0xFFC00C50	
#define DMA1_X_MODIFY			0xFFC00C54	
#define DMA1_Y_COUNT			0xFFC00C58	
#define DMA1_Y_MODIFY			0xFFC00C5C	
#define DMA1_CURR_DESC_PTR		0xFFC00C60	
#define DMA1_CURR_ADDR			0xFFC00C64	
#define DMA1_IRQ_STATUS			0xFFC00C68	
#define DMA1_PERIPHERAL_MAP		0xFFC00C6C	
#define DMA1_CURR_X_COUNT		0xFFC00C70	
#define DMA1_CURR_Y_COUNT		0xFFC00C78	

#define DMA2_NEXT_DESC_PTR		0xFFC00C80	
#define DMA2_START_ADDR			0xFFC00C84	
#define DMA2_CONFIG				0xFFC00C88	
#define DMA2_X_COUNT			0xFFC00C90	
#define DMA2_X_MODIFY			0xFFC00C94	
#define DMA2_Y_COUNT			0xFFC00C98	
#define DMA2_Y_MODIFY			0xFFC00C9C	
#define DMA2_CURR_DESC_PTR		0xFFC00CA0	
#define DMA2_CURR_ADDR			0xFFC00CA4	
#define DMA2_IRQ_STATUS			0xFFC00CA8	
#define DMA2_PERIPHERAL_MAP		0xFFC00CAC	
#define DMA2_CURR_X_COUNT		0xFFC00CB0	
#define DMA2_CURR_Y_COUNT		0xFFC00CB8	

#define DMA3_NEXT_DESC_PTR		0xFFC00CC0	
#define DMA3_START_ADDR			0xFFC00CC4	
#define DMA3_CONFIG				0xFFC00CC8	
#define DMA3_X_COUNT			0xFFC00CD0	
#define DMA3_X_MODIFY			0xFFC00CD4	
#define DMA3_Y_COUNT			0xFFC00CD8	
#define DMA3_Y_MODIFY			0xFFC00CDC	
#define DMA3_CURR_DESC_PTR		0xFFC00CE0	
#define DMA3_CURR_ADDR			0xFFC00CE4	
#define DMA3_IRQ_STATUS			0xFFC00CE8	
#define DMA3_PERIPHERAL_MAP		0xFFC00CEC	
#define DMA3_CURR_X_COUNT		0xFFC00CF0	
#define DMA3_CURR_Y_COUNT		0xFFC00CF8	

#define DMA4_NEXT_DESC_PTR		0xFFC00D00	
#define DMA4_START_ADDR			0xFFC00D04	
#define DMA4_CONFIG				0xFFC00D08	
#define DMA4_X_COUNT			0xFFC00D10	
#define DMA4_X_MODIFY			0xFFC00D14	
#define DMA4_Y_COUNT			0xFFC00D18	
#define DMA4_Y_MODIFY			0xFFC00D1C	
#define DMA4_CURR_DESC_PTR		0xFFC00D20	
#define DMA4_CURR_ADDR			0xFFC00D24	
#define DMA4_IRQ_STATUS			0xFFC00D28	
#define DMA4_PERIPHERAL_MAP		0xFFC00D2C	
#define DMA4_CURR_X_COUNT		0xFFC00D30	
#define DMA4_CURR_Y_COUNT		0xFFC00D38	

#define DMA5_NEXT_DESC_PTR		0xFFC00D40	
#define DMA5_START_ADDR			0xFFC00D44	
#define DMA5_CONFIG				0xFFC00D48	
#define DMA5_X_COUNT			0xFFC00D50	
#define DMA5_X_MODIFY			0xFFC00D54	
#define DMA5_Y_COUNT			0xFFC00D58	
#define DMA5_Y_MODIFY			0xFFC00D5C	
#define DMA5_CURR_DESC_PTR		0xFFC00D60	
#define DMA5_CURR_ADDR			0xFFC00D64	
#define DMA5_IRQ_STATUS			0xFFC00D68	
#define DMA5_PERIPHERAL_MAP		0xFFC00D6C	
#define DMA5_CURR_X_COUNT		0xFFC00D70	
#define DMA5_CURR_Y_COUNT		0xFFC00D78	

#define DMA6_NEXT_DESC_PTR		0xFFC00D80	
#define DMA6_START_ADDR			0xFFC00D84	
#define DMA6_CONFIG				0xFFC00D88	
#define DMA6_X_COUNT			0xFFC00D90	
#define DMA6_X_MODIFY			0xFFC00D94	
#define DMA6_Y_COUNT			0xFFC00D98	
#define DMA6_Y_MODIFY			0xFFC00D9C	
#define DMA6_CURR_DESC_PTR		0xFFC00DA0	
#define DMA6_CURR_ADDR			0xFFC00DA4	
#define DMA6_IRQ_STATUS			0xFFC00DA8	
#define DMA6_PERIPHERAL_MAP		0xFFC00DAC	
#define DMA6_CURR_X_COUNT		0xFFC00DB0	
#define DMA6_CURR_Y_COUNT		0xFFC00DB8	

#define DMA7_NEXT_DESC_PTR		0xFFC00DC0	
#define DMA7_START_ADDR			0xFFC00DC4	
#define DMA7_CONFIG				0xFFC00DC8	
#define DMA7_X_COUNT			0xFFC00DD0	
#define DMA7_X_MODIFY			0xFFC00DD4	
#define DMA7_Y_COUNT			0xFFC00DD8	
#define DMA7_Y_MODIFY			0xFFC00DDC	
#define DMA7_CURR_DESC_PTR		0xFFC00DE0	
#define DMA7_CURR_ADDR			0xFFC00DE4	
#define DMA7_IRQ_STATUS			0xFFC00DE8	
#define DMA7_PERIPHERAL_MAP		0xFFC00DEC	
#define DMA7_CURR_X_COUNT		0xFFC00DF0	
#define DMA7_CURR_Y_COUNT		0xFFC00DF8	

#define DMA8_NEXT_DESC_PTR		0xFFC00E00	
#define DMA8_START_ADDR			0xFFC00E04	
#define DMA8_CONFIG				0xFFC00E08	
#define DMA8_X_COUNT			0xFFC00E10	
#define DMA8_X_MODIFY			0xFFC00E14	
#define DMA8_Y_COUNT			0xFFC00E18	
#define DMA8_Y_MODIFY			0xFFC00E1C	
#define DMA8_CURR_DESC_PTR		0xFFC00E20	
#define DMA8_CURR_ADDR			0xFFC00E24	
#define DMA8_IRQ_STATUS			0xFFC00E28	
#define DMA8_PERIPHERAL_MAP		0xFFC00E2C	
#define DMA8_CURR_X_COUNT		0xFFC00E30	
#define DMA8_CURR_Y_COUNT		0xFFC00E38	

#define DMA9_NEXT_DESC_PTR		0xFFC00E40	
#define DMA9_START_ADDR			0xFFC00E44	
#define DMA9_CONFIG				0xFFC00E48	
#define DMA9_X_COUNT			0xFFC00E50	
#define DMA9_X_MODIFY			0xFFC00E54	
#define DMA9_Y_COUNT			0xFFC00E58	
#define DMA9_Y_MODIFY			0xFFC00E5C	
#define DMA9_CURR_DESC_PTR		0xFFC00E60	
#define DMA9_CURR_ADDR			0xFFC00E64	
#define DMA9_IRQ_STATUS			0xFFC00E68	
#define DMA9_PERIPHERAL_MAP		0xFFC00E6C	
#define DMA9_CURR_X_COUNT		0xFFC00E70	
#define DMA9_CURR_Y_COUNT		0xFFC00E78	

#define DMA10_NEXT_DESC_PTR		0xFFC00E80	
#define DMA10_START_ADDR		0xFFC00E84	
#define DMA10_CONFIG			0xFFC00E88	
#define DMA10_X_COUNT			0xFFC00E90	
#define DMA10_X_MODIFY			0xFFC00E94	
#define DMA10_Y_COUNT			0xFFC00E98	
#define DMA10_Y_MODIFY			0xFFC00E9C	
#define DMA10_CURR_DESC_PTR		0xFFC00EA0	
#define DMA10_CURR_ADDR			0xFFC00EA4	
#define DMA10_IRQ_STATUS		0xFFC00EA8	
#define DMA10_PERIPHERAL_MAP	0xFFC00EAC	
#define DMA10_CURR_X_COUNT		0xFFC00EB0	
#define DMA10_CURR_Y_COUNT		0xFFC00EB8	

#define DMA11_NEXT_DESC_PTR		0xFFC00EC0	
#define DMA11_START_ADDR		0xFFC00EC4	
#define DMA11_CONFIG			0xFFC00EC8	
#define DMA11_X_COUNT			0xFFC00ED0	
#define DMA11_X_MODIFY			0xFFC00ED4	
#define DMA11_Y_COUNT			0xFFC00ED8	
#define DMA11_Y_MODIFY			0xFFC00EDC	
#define DMA11_CURR_DESC_PTR		0xFFC00EE0	
#define DMA11_CURR_ADDR			0xFFC00EE4	
#define DMA11_IRQ_STATUS		0xFFC00EE8	
#define DMA11_PERIPHERAL_MAP	0xFFC00EEC	
#define DMA11_CURR_X_COUNT		0xFFC00EF0	
#define DMA11_CURR_Y_COUNT		0xFFC00EF8	

#define MDMA_D0_NEXT_DESC_PTR	0xFFC00F00	
#define MDMA_D0_START_ADDR		0xFFC00F04	
#define MDMA_D0_CONFIG			0xFFC00F08	
#define MDMA_D0_X_COUNT			0xFFC00F10	
#define MDMA_D0_X_MODIFY		0xFFC00F14	
#define MDMA_D0_Y_COUNT			0xFFC00F18	
#define MDMA_D0_Y_MODIFY		0xFFC00F1C	
#define MDMA_D0_CURR_DESC_PTR	0xFFC00F20	
#define MDMA_D0_CURR_ADDR		0xFFC00F24	
#define MDMA_D0_IRQ_STATUS		0xFFC00F28	
#define MDMA_D0_PERIPHERAL_MAP	0xFFC00F2C	
#define MDMA_D0_CURR_X_COUNT	0xFFC00F30	
#define MDMA_D0_CURR_Y_COUNT	0xFFC00F38	

#define MDMA_S0_NEXT_DESC_PTR	0xFFC00F40	
#define MDMA_S0_START_ADDR		0xFFC00F44	
#define MDMA_S0_CONFIG			0xFFC00F48	
#define MDMA_S0_X_COUNT			0xFFC00F50	
#define MDMA_S0_X_MODIFY		0xFFC00F54	
#define MDMA_S0_Y_COUNT			0xFFC00F58	
#define MDMA_S0_Y_MODIFY		0xFFC00F5C	
#define MDMA_S0_CURR_DESC_PTR	0xFFC00F60	
#define MDMA_S0_CURR_ADDR		0xFFC00F64	
#define MDMA_S0_IRQ_STATUS		0xFFC00F68	
#define MDMA_S0_PERIPHERAL_MAP	0xFFC00F6C	
#define MDMA_S0_CURR_X_COUNT	0xFFC00F70	
#define MDMA_S0_CURR_Y_COUNT	0xFFC00F78	

#define MDMA_D1_NEXT_DESC_PTR	0xFFC00F80	
#define MDMA_D1_START_ADDR		0xFFC00F84	
#define MDMA_D1_CONFIG			0xFFC00F88	
#define MDMA_D1_X_COUNT			0xFFC00F90	
#define MDMA_D1_X_MODIFY		0xFFC00F94	
#define MDMA_D1_Y_COUNT			0xFFC00F98	
#define MDMA_D1_Y_MODIFY		0xFFC00F9C	
#define MDMA_D1_CURR_DESC_PTR	0xFFC00FA0	
#define MDMA_D1_CURR_ADDR		0xFFC00FA4	
#define MDMA_D1_IRQ_STATUS		0xFFC00FA8	
#define MDMA_D1_PERIPHERAL_MAP	0xFFC00FAC	
#define MDMA_D1_CURR_X_COUNT	0xFFC00FB0	
#define MDMA_D1_CURR_Y_COUNT	0xFFC00FB8	

#define MDMA_S1_NEXT_DESC_PTR	0xFFC00FC0	
#define MDMA_S1_START_ADDR		0xFFC00FC4	
#define MDMA_S1_CONFIG			0xFFC00FC8	
#define MDMA_S1_X_COUNT			0xFFC00FD0	
#define MDMA_S1_X_MODIFY		0xFFC00FD4	
#define MDMA_S1_Y_COUNT			0xFFC00FD8	
#define MDMA_S1_Y_MODIFY		0xFFC00FDC	
#define MDMA_S1_CURR_DESC_PTR	0xFFC00FE0	
#define MDMA_S1_CURR_ADDR		0xFFC00FE4	
#define MDMA_S1_IRQ_STATUS		0xFFC00FE8	
#define MDMA_S1_PERIPHERAL_MAP	0xFFC00FEC	
#define MDMA_S1_CURR_X_COUNT	0xFFC00FF0	
#define MDMA_S1_CURR_Y_COUNT	0xFFC00FF8	


#define PPI_CONTROL			0xFFC01000	
#define PPI_STATUS			0xFFC01004	
#define PPI_COUNT			0xFFC01008	
#define PPI_DELAY			0xFFC0100C	
#define PPI_FRAME			0xFFC01010	


#define TWI0_REGBASE			0xFFC01400
#define TWI0_CLKDIV			0xFFC01400	
#define TWI0_CONTROL			0xFFC01404	
#define TWI0_SLAVE_CTL		0xFFC01408	
#define TWI0_SLAVE_STAT		0xFFC0140C	
#define TWI0_SLAVE_ADDR		0xFFC01410	
#define TWI0_MASTER_CTL		0xFFC01414	
#define TWI0_MASTER_STAT		0xFFC01418	
#define TWI0_MASTER_ADDR		0xFFC0141C	
#define TWI0_INT_STAT		0xFFC01420	
#define TWI0_INT_MASK		0xFFC01424	
#define TWI0_FIFO_CTL		0xFFC01428	
#define TWI0_FIFO_STAT		0xFFC0142C	
#define TWI0_XMT_DATA8		0xFFC01480	
#define TWI0_XMT_DATA16		0xFFC01484	
#define TWI0_RCV_DATA8		0xFFC01488	
#define TWI0_RCV_DATA16		0xFFC0148C	


#define PORTGIO					0xFFC01500	
#define PORTGIO_CLEAR			0xFFC01504	
#define PORTGIO_SET				0xFFC01508	
#define PORTGIO_TOGGLE			0xFFC0150C	
#define PORTGIO_MASKA			0xFFC01510	
#define PORTGIO_MASKA_CLEAR		0xFFC01514	
#define PORTGIO_MASKA_SET		0xFFC01518	
#define PORTGIO_MASKA_TOGGLE	0xFFC0151C	
#define PORTGIO_MASKB			0xFFC01520	
#define PORTGIO_MASKB_CLEAR		0xFFC01524	
#define PORTGIO_MASKB_SET		0xFFC01528	
#define PORTGIO_MASKB_TOGGLE	0xFFC0152C	
#define PORTGIO_DIR				0xFFC01530	
#define PORTGIO_POLAR			0xFFC01534	
#define PORTGIO_EDGE			0xFFC01538	
#define PORTGIO_BOTH			0xFFC0153C	
#define PORTGIO_INEN			0xFFC01540	


#define PORTHIO					0xFFC01700	
#define PORTHIO_CLEAR			0xFFC01704	
#define PORTHIO_SET				0xFFC01708	
#define PORTHIO_TOGGLE			0xFFC0170C	
#define PORTHIO_MASKA			0xFFC01710	
#define PORTHIO_MASKA_CLEAR		0xFFC01714	
#define PORTHIO_MASKA_SET		0xFFC01718	
#define PORTHIO_MASKA_TOGGLE	0xFFC0171C	
#define PORTHIO_MASKB			0xFFC01720	
#define PORTHIO_MASKB_CLEAR		0xFFC01724	
#define PORTHIO_MASKB_SET		0xFFC01728	
#define PORTHIO_MASKB_TOGGLE	0xFFC0172C	
#define PORTHIO_DIR				0xFFC01730	
#define PORTHIO_POLAR			0xFFC01734	
#define PORTHIO_EDGE			0xFFC01738	
#define PORTHIO_BOTH			0xFFC0173C	
#define PORTHIO_INEN			0xFFC01740	


#define UART1_THR			0xFFC02000	
#define UART1_RBR			0xFFC02000	
#define UART1_DLL			0xFFC02000	
#define UART1_IER			0xFFC02004	
#define UART1_DLH			0xFFC02004	
#define UART1_IIR			0xFFC02008	
#define UART1_LCR			0xFFC0200C	
#define UART1_MCR			0xFFC02010	
#define UART1_LSR			0xFFC02014	
#define UART1_MSR			0xFFC02018	
#define UART1_SCR			0xFFC0201C	
#define UART1_GCTL			0xFFC02024	


#define PORTF_FER			0xFFC03200	
#define PORTG_FER			0xFFC03204	
#define PORTH_FER			0xFFC03208	
#define BFIN_PORT_MUX			0xFFC0320C	


#define HMDMA0_CONTROL		0xFFC03300	
#define HMDMA0_ECINIT		0xFFC03304	
#define HMDMA0_BCINIT		0xFFC03308	
#define HMDMA0_ECURGENT		0xFFC0330C	
#define HMDMA0_ECOVERFLOW	0xFFC03310	
#define HMDMA0_ECOUNT		0xFFC03314	
#define HMDMA0_BCOUNT		0xFFC03318	

#define HMDMA1_CONTROL		0xFFC03340	
#define HMDMA1_ECINIT		0xFFC03344	
#define HMDMA1_BCINIT		0xFFC03348	
#define HMDMA1_ECURGENT		0xFFC0334C	
#define HMDMA1_ECOVERFLOW	0xFFC03350	
#define HMDMA1_ECOUNT		0xFFC03354	
#define HMDMA1_BCOUNT		0xFFC03358	


#define PORTF_MUX               0xFFC03210      
#define PORTG_MUX               0xFFC03214      
#define PORTH_MUX               0xFFC03218      
#define PORTF_DRIVE             0xFFC03220      
#define PORTG_DRIVE             0xFFC03224      
#define PORTH_DRIVE             0xFFC03228      
#define PORTF_SLEW              0xFFC03230      
#define PORTG_SLEW              0xFFC03234      
#define PORTH_SLEW              0xFFC03238      
#define PORTF_HYSTERESIS        0xFFC03240      
#define PORTG_HYSTERESIS        0xFFC03244      
#define PORTH_HYSTERESIS        0xFFC03248      
#define MISCPORT_DRIVE          0xFFC03280      
#define MISCPORT_SLEW           0xFFC03284      
#define MISCPORT_HYSTERESIS     0xFFC03288      



#define CHIPID_VERSION         0xF0000000
#define CHIPID_FAMILY          0x0FFFF000
#define CHIPID_MANUFACTURE     0x00000FFE

#define SYSTEM_RESET		0x0007	
#define	DOUBLE_FAULT		0x0008	
#define RESET_DOUBLE		0x2000	
#define RESET_WDOG			0x4000	
#define RESET_SOFTWARE		0x8000	

#define BMODE				0x0007	
#define	NOBOOT				0x0010	



#if 0
#define IRQ_PLL_WAKEUP	0x00000001	

#define IRQ_ERROR1      0x00000002  
#define IRQ_ERROR2      0x00000004  
#define IRQ_RTC			0x00000008	
#define IRQ_DMA0		0x00000010	
#define IRQ_DMA3		0x00000020	
#define IRQ_DMA4		0x00000040	
#define IRQ_DMA5		0x00000080	

#define IRQ_DMA6		0x00000100	
#define IRQ_TWI			0x00000200	
#define IRQ_DMA7		0x00000400	
#define IRQ_DMA8		0x00000800	
#define IRQ_DMA9		0x00001000	
#define IRQ_DMA10		0x00002000	
#define IRQ_DMA11		0x00004000	
#define IRQ_CAN_RX		0x00008000	

#define IRQ_CAN_TX		0x00010000	
#define IRQ_DMA1		0x00020000	
#define IRQ_PFA_PORTH	0x00020000	
#define IRQ_DMA2		0x00040000	
#define IRQ_PFB_PORTH	0x00040000	
#define IRQ_TIMER0		0x00080000	
#define IRQ_TIMER1		0x00100000	
#define IRQ_TIMER2		0x00200000	
#define IRQ_TIMER3		0x00400000	
#define IRQ_TIMER4		0x00800000	

#define IRQ_TIMER5		0x01000000	
#define IRQ_TIMER6		0x02000000	
#define IRQ_TIMER7		0x04000000	
#define IRQ_PFA_PORTFG	0x08000000	
#define IRQ_PFB_PORTF	0x80000000	
#define IRQ_DMA12		0x20000000	
#define IRQ_DMA13		0x20000000	
#define IRQ_DMA14		0x40000000	
#define IRQ_DMA15		0x40000000	
#define IRQ_WDOG		0x80000000	
#define IRQ_PFB_PORTG	0x10000000	
#endif

#define P0_IVG(x)		(((x)&0xF)-7)			
#define P1_IVG(x)		(((x)&0xF)-7) << 0x4	
#define P2_IVG(x)		(((x)&0xF)-7) << 0x8	
#define P3_IVG(x)		(((x)&0xF)-7) << 0xC	
#define P4_IVG(x)		(((x)&0xF)-7) << 0x10	
#define P5_IVG(x)		(((x)&0xF)-7) << 0x14	
#define P6_IVG(x)		(((x)&0xF)-7) << 0x18	
#define P7_IVG(x)		(((x)&0xF)-7) << 0x1C	

#define P8_IVG(x)		(((x)&0xF)-7)			
#define P9_IVG(x)		(((x)&0xF)-7) << 0x4	
#define P10_IVG(x)		(((x)&0xF)-7) << 0x8	
#define P11_IVG(x)		(((x)&0xF)-7) << 0xC	
#define P12_IVG(x)		(((x)&0xF)-7) << 0x10	
#define P13_IVG(x)		(((x)&0xF)-7) << 0x14	
#define P14_IVG(x)		(((x)&0xF)-7) << 0x18	
#define P15_IVG(x)		(((x)&0xF)-7) << 0x1C	

#define P16_IVG(x)		(((x)&0xF)-7)			
#define P17_IVG(x)		(((x)&0xF)-7) << 0x4	
#define P18_IVG(x)		(((x)&0xF)-7) << 0x8	
#define P19_IVG(x)		(((x)&0xF)-7) << 0xC	
#define P20_IVG(x)		(((x)&0xF)-7) << 0x10	
#define P21_IVG(x)		(((x)&0xF)-7) << 0x14	
#define P22_IVG(x)		(((x)&0xF)-7) << 0x18	
#define P23_IVG(x)		(((x)&0xF)-7) << 0x1C	

#define P24_IVG(x)		(((x)&0xF)-7)			
#define P25_IVG(x)		(((x)&0xF)-7) << 0x4	
#define P26_IVG(x)		(((x)&0xF)-7) << 0x8	
#define P27_IVG(x)		(((x)&0xF)-7) << 0xC	
#define P28_IVG(x)		(((x)&0xF)-7) << 0x10	
#define P29_IVG(x)		(((x)&0xF)-7) << 0x14	
#define P30_IVG(x)		(((x)&0xF)-7) << 0x18	
#define P31_IVG(x)		(((x)&0xF)-7) << 0x1C	


#define SIC_UNMASK_ALL	0x00000000					
#define SIC_MASK_ALL	0xFFFFFFFF					
#define SIC_MASK(x)		(1 << ((x)&0x1F))					
#define SIC_UNMASK(x)	(0xFFFFFFFF ^ (1 << ((x)&0x1F)))	

#define IWR_DISABLE_ALL	0x00000000					
#define IWR_ENABLE_ALL	0xFFFFFFFF					
#define IWR_ENABLE(x)	(1 << ((x)&0x1F))					
#define IWR_DISABLE(x)	(0xFFFFFFFF ^ (1 << ((x)&0x1F))) 	

#define TIMEN0			0x0001		
#define TIMEN1			0x0002		
#define TIMEN2			0x0004		
#define TIMEN3			0x0008		
#define TIMEN4			0x0010		
#define TIMEN5			0x0020		
#define TIMEN6			0x0040		
#define TIMEN7			0x0080		

#define TIMDIS0			TIMEN0		
#define TIMDIS1			TIMEN1		
#define TIMDIS2			TIMEN2		
#define TIMDIS3			TIMEN3		
#define TIMDIS4			TIMEN4		
#define TIMDIS5			TIMEN5		
#define TIMDIS6			TIMEN6		
#define TIMDIS7			TIMEN7		

#define TIMIL0			0x00000001	
#define TIMIL1			0x00000002	
#define TIMIL2			0x00000004	
#define TIMIL3			0x00000008	
#define TOVF_ERR0		0x00000010	
#define TOVF_ERR1		0x00000020	
#define TOVF_ERR2		0x00000040	
#define TOVF_ERR3		0x00000080	
#define TRUN0			0x00001000	
#define TRUN1			0x00002000	
#define TRUN2			0x00004000	
#define TRUN3			0x00008000	
#define TIMIL4			0x00010000	
#define TIMIL5			0x00020000	
#define TIMIL6			0x00040000	
#define TIMIL7			0x00080000	
#define TOVF_ERR4		0x00100000	
#define TOVF_ERR5		0x00200000	
#define TOVF_ERR6		0x00400000	
#define TOVF_ERR7		0x00800000	
#define TRUN4			0x10000000	
#define TRUN5			0x20000000	
#define TRUN6			0x40000000	
#define TRUN7			0x80000000	

#define TOVL_ERR0 TOVF_ERR0
#define TOVL_ERR1 TOVF_ERR1
#define TOVL_ERR2 TOVF_ERR2
#define TOVL_ERR3 TOVF_ERR3
#define TOVL_ERR4 TOVF_ERR4
#define TOVL_ERR5 TOVF_ERR5
#define TOVL_ERR6 TOVF_ERR6
#define TOVL_ERR7 TOVF_ERR7

#define PWM_OUT			0x0001	
#define WDTH_CAP		0x0002	
#define EXT_CLK			0x0003	
#define PULSE_HI		0x0004	
#define PERIOD_CNT		0x0008	
#define IRQ_ENA			0x0010	
#define TIN_SEL			0x0020	
#define OUT_DIS			0x0040	
#define CLK_SEL			0x0080	
#define TOGGLE_HI		0x0100	
#define EMU_RUN			0x0200	
#define ERR_TYP			0xC000	

#define AMCKEN			0x0001		
#define	AMBEN_NONE		0x0000		
#define AMBEN_B0		0x0002		
#define AMBEN_B0_B1		0x0004		
#define AMBEN_B0_B1_B2	0x0006		
#define AMBEN_ALL		0x0008		

#define B0RDYEN			0x00000001  
#define B0RDYPOL		0x00000002  
#define B0TT_1			0x00000004  
#define B0TT_2			0x00000008  
#define B0TT_3			0x0000000C  
#define B0TT_4			0x00000000  
#define B0ST_1			0x00000010  
#define B0ST_2			0x00000020  
#define B0ST_3			0x00000030  
#define B0ST_4			0x00000000  
#define B0HT_1			0x00000040  
#define B0HT_2			0x00000080  
#define B0HT_3			0x000000C0  
#define B0HT_0			0x00000000  
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
#define PSM				0x00400000	
#define PSS				0x00800000	
#define SRFS			0x01000000	
#define EBUFE			0x02000000	
#define FBBRW			0x04000000	
#define EMREN			0x10000000	
#define TCSR			0x20000000	
#define CDDBG			0x40000000	

#define EBE				0x0001		
#define EBSZ_16			0x0000		
#define EBSZ_32			0x0002		
#define EBSZ_64			0x0004		
#define EBSZ_128		0x0006		
#define EBSZ_256		0x0008		
#define EBSZ_512		0x000A		
#define EBCAW_8			0x0000		
#define EBCAW_9			0x0010		
#define EBCAW_10		0x0020		
#define EBCAW_11		0x0030		

#define SDCI			0x0001		
#define SDSRA			0x0002		
#define SDPUA			0x0004		
#define SDRS			0x0008		
#define SDEASE			0x0010		
#define BGSTAT			0x0020		



#define CTYPE			0x0040	
#define PMAP			0xF000	
#define PMAP_PPI		0x0000	
#define	PMAP_EMACRX		0x1000	
#define PMAP_EMACTX		0x2000	
#define PMAP_SPORT0RX	0x3000	
#define PMAP_SPORT0TX	0x4000	
#define PMAP_SPORT1RX	0x5000	
#define PMAP_SPORT1TX	0x6000	
#define PMAP_SPI		0x7000	
#define PMAP_UART0RX	0x8000	
#define PMAP_UART0TX	0x9000	
#define	PMAP_UART1RX	0xA000	
#define	PMAP_UART1TX	0xB000	

#define PORT_EN			0x0001		
#define PORT_DIR		0x0002		
#define XFR_TYPE		0x000C		
#define PORT_CFG		0x0030		
#define FLD_SEL			0x0040		
#define PACK_EN			0x0080		
#define DMA32			0x0100		
#define SKIP_EN			0x0200		
#define SKIP_EO			0x0400		
#define DLEN_8			0x0000		
#define DLEN_10			0x0800		
#define DLEN_11			0x1000		
#define DLEN_12			0x1800		
#define DLEN_13			0x2000		
#define DLEN_14			0x2800		
#define DLEN_15			0x3000		
#define DLEN_16			0x3800		
#define DLENGTH			0x3800		
#define POLC			0x4000		
#define POLS			0x8000		

#define FLD				0x0400		
#define FT_ERR			0x0800		
#define OVR				0x1000		
#define UNDR			0x2000		
#define ERR_DET			0x4000		
#define ERR_NCOR		0x8000		


#define	CLKLOW(x)	((x) & 0xFF)		
#define CLKHI(y)	(((y)&0xFF)<<0x8)	

#define	PRESCALE	0x007F		
#define	TWI_ENA		0x0080		
#define	SCCB		0x0200		

#define	SEN			0x0001		
#define	SADD_LEN	0x0002		
#define	STDVAL		0x0004		
#define	NAK			0x0008		
#define	GEN			0x0010		

#define	SDIR		0x0001		
#define GCALL		0x0002		

#define	MEN			0x0001		
#define	MADD_LEN	0x0002		
#define	MDIR		0x0004		
#define	FAST		0x0008		
#define	STOP		0x0010		
#define	RSTART		0x0020		
#define	DCNT		0x3FC0		
#define	SDAOVR		0x4000		
#define	SCLOVR		0x8000		

#define	MPROG		0x0001		
#define	LOSTARB		0x0002		
#define	ANAK		0x0004		
#define	DNAK		0x0008		
#define	BUFRDERR	0x0010		
#define	BUFWRERR	0x0020		
#define	SDASEN		0x0040		
#define	SCLSEN		0x0080		
#define	BUSBUSY		0x0100		

#define	SINIT		0x0001		
#define	SCOMP		0x0002		
#define	SERR		0x0004		
#define	SOVF		0x0008		
#define	MCOMP		0x0010		
#define	MERR		0x0020		
#define	XMTSERV		0x0040		
#define	RCVSERV		0x0080		

#define	XMTFLUSH	0x0001		
#define	RCVFLUSH	0x0002		
#define	XMTINTLEN	0x0004		
#define	RCVINTLEN	0x0008		

#define	XMTSTAT		0x0003		
#define	XMT_EMPTY	0x0000		
#define	XMT_HALF	0x0001		
#define	XMT_FULL	0x0003		

#define	RCVSTAT		0x000C		
#define	RCV_EMPTY	0x0000		
#define	RCV_HALF	0x0004		
#define	RCV_FULL	0x000C		


#define	PJSE			0x0001			
#define	PJSE_SPORT		0x0000			
#define	PJSE_SPI		0x0001			

#define	PJCE(x)			(((x)&0x3)<<1)	
#define	PJCE_SPORT		0x0000			
#define	PJCE_CAN		0x0002			
#define	PJCE_SPI		0x0004			

#define	PFDE			0x0008			
#define	PFDE_UART		0x0000			
#define	PFDE_DMA		0x0008			

#define	PFTE			0x0010			
#define	PFTE_UART		0x0000			
#define	PFTE_TIMER		0x0010			

#define	PFS6E			0x0020			
#define	PFS6E_TIMER		0x0000			
#define	PFS6E_SPI		0x0020			

#define	PFS5E			0x0040			
#define	PFS5E_TIMER		0x0000			
#define	PFS5E_SPI		0x0040			

#define	PFS4E			0x0080			
#define	PFS4E_TIMER		0x0000			
#define	PFS4E_SPI		0x0080			

#define	PFFE			0x0100			
#define	PFFE_TIMER		0x0000			
#define	PFFE_PPI		0x0100			

#define	PGSE			0x0200			
#define	PGSE_PPI		0x0000			
#define	PGSE_SPORT		0x0200			

#define	PGRE			0x0400			
#define	PGRE_PPI		0x0000			
#define	PGRE_SPORT		0x0400			

#define	PGTE			0x0800			
#define	PGTE_PPI		0x0000			
#define	PGTE_SPORT		0x0800			


#define _BOOTROM_RESET 0xEF000000
#define _BOOTROM_FINAL_INIT 0xEF000002
#define _BOOTROM_DO_MEMORY_DMA 0xEF000006
#define _BOOTROM_BOOT_DXE_FLASH 0xEF000008
#define _BOOTROM_BOOT_DXE_SPI 0xEF00000A
#define _BOOTROM_BOOT_DXE_TWI 0xEF00000C
#define _BOOTROM_GET_DXE_ADDRESS_FLASH 0xEF000010
#define _BOOTROM_GET_DXE_ADDRESS_SPI 0xEF000012
#define _BOOTROM_GET_DXE_ADDRESS_TWI 0xEF000014

#define	PGDE_UART   PFDE_UART
#define	PGDE_DMA    PFDE_DMA
#define	CKELOW		SCKELOW


#define                     HOST_CONTROL  0xffc03400   
#define                      HOST_STATUS  0xffc03404   
#define                     HOST_TIMEOUT  0xffc03408   


#define                       CNT_CONFIG  0xffc03500   
#define                        CNT_IMASK  0xffc03504   
#define                       CNT_STATUS  0xffc03508   
#define                      CNT_COMMAND  0xffc0350c   
#define                     CNT_DEBOUNCE  0xffc03510   
#define                      CNT_COUNTER  0xffc03514   
#define                          CNT_MAX  0xffc03518   
#define                          CNT_MIN  0xffc0351c   


#define                      OTP_CONTROL  0xffc03600   
#define                          OTP_BEN  0xffc03604   
#define                       OTP_STATUS  0xffc03608   
#define                       OTP_TIMING  0xffc0360c   


#define                    SECURE_SYSSWT  0xffc03620   
#define                   SECURE_CONTROL  0xffc03624   
#define                    SECURE_STATUS  0xffc03628   


#define                        OTP_DATA0  0xffc03680   
#define                        OTP_DATA1  0xffc03684   
#define                        OTP_DATA2  0xffc03688   
#define                        OTP_DATA3  0xffc0368c   


#define                         PWM_CTRL  0xffc03700   
#define                         PWM_STAT  0xffc03704   
#define                           PWM_TM  0xffc03708   
#define                           PWM_DT  0xffc0370c   
#define                         PWM_GATE  0xffc03710   
#define                          PWM_CHA  0xffc03714   
#define                          PWM_CHB  0xffc03718   
#define                          PWM_CHC  0xffc0371c   
#define                          PWM_SEG  0xffc03720   
#define                       PWM_SYNCWT  0xffc03724   
#define                         PWM_CHAL  0xffc03728   
#define                         PWM_CHBL  0xffc0372c   
#define                         PWM_CHCL  0xffc03730   
#define                          PWM_LSI  0xffc03734   
#define                        PWM_STAT2  0xffc03738   




#define                   HOST_CNTR_HOST_EN  0x1        
#define                  HOST_CNTR_nHOST_EN  0x0
#define                  HOST_CNTR_HOST_END  0x2        
#define                 HOST_CNTR_nHOST_END  0x0
#define                 HOST_CNTR_DATA_SIZE  0x4        
#define                HOST_CNTR_nDATA_SIZE  0x0
#define                  HOST_CNTR_HOST_RST  0x8        
#define                 HOST_CNTR_nHOST_RST  0x0
#define                  HOST_CNTR_HRDY_OVR  0x20       
#define                 HOST_CNTR_nHRDY_OVR  0x0
#define                  HOST_CNTR_INT_MODE  0x40       
#define                 HOST_CNTR_nINT_MODE  0x0
#define                     HOST_CNTR_BT_EN  0x80       
#define                   HOST_CNTR_ nBT_EN  0x0
#define                       HOST_CNTR_EHW  0x100      
#define                      HOST_CNTR_nEHW  0x0
#define                       HOST_CNTR_EHR  0x200      
#define                      HOST_CNTR_nEHR  0x0
#define                       HOST_CNTR_BDR  0x400      
#define                      HOST_CNTR_nBDR  0x0


#define                     HOST_STAT_READY  0x1        
#define                    HOST_STAT_nREADY  0x0
#define                  HOST_STAT_FIFOFULL  0x2        
#define                 HOST_STAT_nFIFOFULL  0x0
#define                 HOST_STAT_FIFOEMPTY  0x4        
#define                HOST_STAT_nFIFOEMPTY  0x0
#define                  HOST_STAT_COMPLETE  0x8        
#define                 HOST_STAT_nCOMPLETE  0x0
#define                      HOST_STAT_HSHK  0x10       
#define                     HOST_STAT_nHSHK  0x0
#define                   HOST_STAT_TIMEOUT  0x20       
#define                  HOST_STAT_nTIMEOUT  0x0
#define                      HOST_STAT_HIRQ  0x40       
#define                     HOST_STAT_nHIRQ  0x0
#define                HOST_STAT_ALLOW_CNFG  0x80       
#define               HOST_STAT_nALLOW_CNFG  0x0
#define                   HOST_STAT_DMA_DIR  0x100      
#define                  HOST_STAT_nDMA_DIR  0x0
#define                       HOST_STAT_BTE  0x200      
#define                      HOST_STAT_nBTE  0x0
#define               HOST_STAT_HOSTRD_DONE  0x8000     
#define              HOST_STAT_nHOSTRD_DONE  0x0


#define             HOST_COUNT_TIMEOUT  0x7ff      


#define                   EMUDABL  0x1        
#define                  nEMUDABL  0x0
#define                   RSTDABL  0x2        
#define                  nRSTDABL  0x0
#define                   L1IDABL  0x1c       
#define                  L1DADABL  0xe0       
#define                  L1DBDABL  0x700      
#define                   DMA0OVR  0x800      
#define                  nDMA0OVR  0x0
#define                   DMA1OVR  0x1000     
#define                  nDMA1OVR  0x0
#define                    EMUOVR  0x4000     
#define                   nEMUOVR  0x0
#define                    OTPSEN  0x8000     
#define                   nOTPSEN  0x0
#define                    L2DABL  0x70000    


#define                   SECURE0  0x1        
#define                  nSECURE0  0x0
#define                   SECURE1  0x2        
#define                  nSECURE1  0x0
#define                   SECURE2  0x4        
#define                  nSECURE2  0x0
#define                   SECURE3  0x8        
#define                  nSECURE3  0x0


#define                   SECMODE  0x3        
#define                       NMI  0x4        
#define                      nNMI  0x0
#define                   AFVALID  0x8        
#define                  nAFVALID  0x0
#define                    AFEXIT  0x10       
#define                   nAFEXIT  0x0
#define                   SECSTAT  0xe0       

#endif 
