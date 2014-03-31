/*
 * Copyright 2007-2010 Analog Devices Inc.
 *
 * Licensed under the ADI BSD license or the GPL-2 (or later)
 */

#ifndef _DEF_BF54X_H
#define _DEF_BF54X_H




#define                          PLL_CTL  0xffc00000   
#define                          PLL_DIV  0xffc00004   
#define                           VR_CTL  0xffc00008   
#define                         PLL_STAT  0xffc0000c   
#define                      PLL_LOCKCNT  0xffc00010   


#define                           CHIPID  0xffc00014
#define                   CHIPID_VERSION  0xF0000000
#define                    CHIPID_FAMILY  0x0FFFF000
#define               CHIPID_MANUFACTURE  0x00000FFE


#define                            SWRST  0xffc00100   
#define                            SYSCR  0xffc00104   


#define                        SIC_RVECT  0xffc00108
#define                       SIC_IMASK0  0xffc0010c   
#define                       SIC_IMASK1  0xffc00110   
#define                       SIC_IMASK2  0xffc00114   
#define                         SIC_ISR0  0xffc00118   
#define                         SIC_ISR1  0xffc0011c   
#define                         SIC_ISR2  0xffc00120   
#define                         SIC_IWR0  0xffc00124   
#define                         SIC_IWR1  0xffc00128   
#define                         SIC_IWR2  0xffc0012c   
#define                         SIC_IAR0  0xffc00130   
#define                         SIC_IAR1  0xffc00134   
#define                         SIC_IAR2  0xffc00138   
#define                         SIC_IAR3  0xffc0013c   
#define                         SIC_IAR4  0xffc00140   
#define                         SIC_IAR5  0xffc00144   
#define                         SIC_IAR6  0xffc00148   
#define                         SIC_IAR7  0xffc0014c   
#define                         SIC_IAR8  0xffc00150   
#define                         SIC_IAR9  0xffc00154   
#define                        SIC_IAR10  0xffc00158   
#define                        SIC_IAR11  0xffc0015c   


#define                         WDOG_CTL  0xffc00200   
#define                         WDOG_CNT  0xffc00204   
#define                        WDOG_STAT  0xffc00208   


#define                         RTC_STAT  0xffc00300   
#define                         RTC_ICTL  0xffc00304   
#define                        RTC_ISTAT  0xffc00308   
#define                        RTC_SWCNT  0xffc0030c   
#define                        RTC_ALARM  0xffc00310   
#define                         RTC_PREN  0xffc00314   


#define                        UART0_DLL  0xffc00400   
#define                        UART0_DLH  0xffc00404   
#define                       UART0_GCTL  0xffc00408   
#define                        UART0_LCR  0xffc0040c   
#define                        UART0_MCR  0xffc00410   
#define                        UART0_LSR  0xffc00414   
#define                        UART0_MSR  0xffc00418   
#define                        UART0_SCR  0xffc0041c   
#define                    UART0_IER_SET  0xffc00420   
#define                  UART0_IER_CLEAR  0xffc00424   
#define                        UART0_THR  0xffc00428   
#define                        UART0_RBR  0xffc0042c   


#define                     SPI0_REGBASE  0xffc00500
#define                         SPI0_CTL  0xffc00500   
#define                         SPI0_FLG  0xffc00504   
#define                        SPI0_STAT  0xffc00508   
#define                        SPI0_TDBR  0xffc0050c   
#define                        SPI0_RDBR  0xffc00510   
#define                        SPI0_BAUD  0xffc00514   
#define                      SPI0_SHADOW  0xffc00518   



#define                     TWI0_REGBASE  0xffc00700
#define                      TWI0_CLKDIV  0xffc00700   
#define                     TWI0_CONTROL  0xffc00704   
#define                   TWI0_SLAVE_CTL  0xffc00708   
#define                  TWI0_SLAVE_STAT  0xffc0070c   
#define                  TWI0_SLAVE_ADDR  0xffc00710   
#define                  TWI0_MASTER_CTL  0xffc00714   
#define                 TWI0_MASTER_STAT  0xffc00718   
#define                 TWI0_MASTER_ADDR  0xffc0071c   
#define                    TWI0_INT_STAT  0xffc00720   
#define                    TWI0_INT_MASK  0xffc00724   
#define                    TWI0_FIFO_CTL  0xffc00728   
#define                   TWI0_FIFO_STAT  0xffc0072c   
#define                   TWI0_XMT_DATA8  0xffc00780   
#define                  TWI0_XMT_DATA16  0xffc00784   
#define                   TWI0_RCV_DATA8  0xffc00788   
#define                  TWI0_RCV_DATA16  0xffc0078c   



#define                      SPORT1_TCR1  0xffc00900   
#define                      SPORT1_TCR2  0xffc00904   
#define                   SPORT1_TCLKDIV  0xffc00908   
#define                    SPORT1_TFSDIV  0xffc0090c   
#define                        SPORT1_TX  0xffc00910   
#define                        SPORT1_RX  0xffc00918   
#define                      SPORT1_RCR1  0xffc00920   
#define                      SPORT1_RCR2  0xffc00924   
#define                   SPORT1_RCLKDIV  0xffc00928   
#define                    SPORT1_RFSDIV  0xffc0092c   
#define                      SPORT1_STAT  0xffc00930   
#define                      SPORT1_CHNL  0xffc00934   
#define                     SPORT1_MCMC1  0xffc00938   
#define                     SPORT1_MCMC2  0xffc0093c   
#define                     SPORT1_MTCS0  0xffc00940   
#define                     SPORT1_MTCS1  0xffc00944   
#define                     SPORT1_MTCS2  0xffc00948   
#define                     SPORT1_MTCS3  0xffc0094c   
#define                     SPORT1_MRCS0  0xffc00950   
#define                     SPORT1_MRCS1  0xffc00954   
#define                     SPORT1_MRCS2  0xffc00958   
#define                     SPORT1_MRCS3  0xffc0095c   


#define                      EBIU_AMGCTL  0xffc00a00   
#define                    EBIU_AMBCTL0   0xffc00a04   
#define                    EBIU_AMBCTL1   0xffc00a08   
#define                      EBIU_MBSCTL  0xffc00a0c   
#define                     EBIU_ARBSTAT  0xffc00a10   
#define                        EBIU_MODE  0xffc00a14   
#define                        EBIU_FCTL  0xffc00a18   


#define                     EBIU_DDRCTL0  0xffc00a20   
#define                     EBIU_DDRCTL1  0xffc00a24   
#define                     EBIU_DDRCTL2  0xffc00a28   
#define                     EBIU_DDRCTL3  0xffc00a2c   
#define                      EBIU_DDRQUE  0xffc00a30   
#define                      EBIU_ERRADD  0xffc00a34   
#define                      EBIU_ERRMST  0xffc00a38   
#define                      EBIU_RSTCTL  0xffc00a3c   


#define                     EBIU_DDRBRC0  0xffc00a60   
#define                     EBIU_DDRBRC1  0xffc00a64   
#define                     EBIU_DDRBRC2  0xffc00a68   
#define                     EBIU_DDRBRC3  0xffc00a6c   
#define                     EBIU_DDRBRC4  0xffc00a70   
#define                     EBIU_DDRBRC5  0xffc00a74   
#define                     EBIU_DDRBRC6  0xffc00a78   
#define                     EBIU_DDRBRC7  0xffc00a7c   
#define                     EBIU_DDRBWC0  0xffc00a80   
#define                     EBIU_DDRBWC1  0xffc00a84   
#define                     EBIU_DDRBWC2  0xffc00a88   
#define                     EBIU_DDRBWC3  0xffc00a8c   
#define                     EBIU_DDRBWC4  0xffc00a90   
#define                     EBIU_DDRBWC5  0xffc00a94   
#define                     EBIU_DDRBWC6  0xffc00a98   
#define                     EBIU_DDRBWC7  0xffc00a9c   
#define                     EBIU_DDRACCT  0xffc00aa0   
#define                     EBIU_DDRTACT  0xffc00aa8   
#define                     EBIU_DDRARCT  0xffc00aac   
#define                      EBIU_DDRGC0  0xffc00ab0   
#define                      EBIU_DDRGC1  0xffc00ab4   
#define                      EBIU_DDRGC2  0xffc00ab8   
#define                      EBIU_DDRGC3  0xffc00abc   
#define                     EBIU_DDRMCEN  0xffc00ac0   
#define                     EBIU_DDRMCCL  0xffc00ac4   


#define                     DMAC0_TC_PER  0xffc00b0c   
#define                     DMAC0_TC_CNT  0xffc00b10   


#define               DMA0_NEXT_DESC_PTR  0xffc00c00   
#define                  DMA0_START_ADDR  0xffc00c04   
#define                      DMA0_CONFIG  0xffc00c08   
#define                     DMA0_X_COUNT  0xffc00c10   
#define                    DMA0_X_MODIFY  0xffc00c14   
#define                     DMA0_Y_COUNT  0xffc00c18   
#define                    DMA0_Y_MODIFY  0xffc00c1c   
#define               DMA0_CURR_DESC_PTR  0xffc00c20   
#define                   DMA0_CURR_ADDR  0xffc00c24   
#define                  DMA0_IRQ_STATUS  0xffc00c28   
#define              DMA0_PERIPHERAL_MAP  0xffc00c2c   
#define                DMA0_CURR_X_COUNT  0xffc00c30   
#define                DMA0_CURR_Y_COUNT  0xffc00c38   


#define               DMA1_NEXT_DESC_PTR  0xffc00c40   
#define                  DMA1_START_ADDR  0xffc00c44   
#define                      DMA1_CONFIG  0xffc00c48   
#define                     DMA1_X_COUNT  0xffc00c50   
#define                    DMA1_X_MODIFY  0xffc00c54   
#define                     DMA1_Y_COUNT  0xffc00c58   
#define                    DMA1_Y_MODIFY  0xffc00c5c   
#define               DMA1_CURR_DESC_PTR  0xffc00c60   
#define                   DMA1_CURR_ADDR  0xffc00c64   
#define                  DMA1_IRQ_STATUS  0xffc00c68   
#define              DMA1_PERIPHERAL_MAP  0xffc00c6c   
#define                DMA1_CURR_X_COUNT  0xffc00c70   
#define                DMA1_CURR_Y_COUNT  0xffc00c78   


#define               DMA2_NEXT_DESC_PTR  0xffc00c80   
#define                  DMA2_START_ADDR  0xffc00c84   
#define                      DMA2_CONFIG  0xffc00c88   
#define                     DMA2_X_COUNT  0xffc00c90   
#define                    DMA2_X_MODIFY  0xffc00c94   
#define                     DMA2_Y_COUNT  0xffc00c98   
#define                    DMA2_Y_MODIFY  0xffc00c9c   
#define               DMA2_CURR_DESC_PTR  0xffc00ca0   
#define                   DMA2_CURR_ADDR  0xffc00ca4   
#define                  DMA2_IRQ_STATUS  0xffc00ca8   
#define              DMA2_PERIPHERAL_MAP  0xffc00cac   
#define                DMA2_CURR_X_COUNT  0xffc00cb0   
#define                DMA2_CURR_Y_COUNT  0xffc00cb8   


#define               DMA3_NEXT_DESC_PTR  0xffc00cc0   
#define                  DMA3_START_ADDR  0xffc00cc4   
#define                      DMA3_CONFIG  0xffc00cc8   
#define                     DMA3_X_COUNT  0xffc00cd0   
#define                    DMA3_X_MODIFY  0xffc00cd4   
#define                     DMA3_Y_COUNT  0xffc00cd8   
#define                    DMA3_Y_MODIFY  0xffc00cdc   
#define               DMA3_CURR_DESC_PTR  0xffc00ce0   
#define                   DMA3_CURR_ADDR  0xffc00ce4   
#define                  DMA3_IRQ_STATUS  0xffc00ce8   
#define              DMA3_PERIPHERAL_MAP  0xffc00cec   
#define                DMA3_CURR_X_COUNT  0xffc00cf0   
#define                DMA3_CURR_Y_COUNT  0xffc00cf8   


#define               DMA4_NEXT_DESC_PTR  0xffc00d00   
#define                  DMA4_START_ADDR  0xffc00d04   
#define                      DMA4_CONFIG  0xffc00d08   
#define                     DMA4_X_COUNT  0xffc00d10   
#define                    DMA4_X_MODIFY  0xffc00d14   
#define                     DMA4_Y_COUNT  0xffc00d18   
#define                    DMA4_Y_MODIFY  0xffc00d1c   
#define               DMA4_CURR_DESC_PTR  0xffc00d20   
#define                   DMA4_CURR_ADDR  0xffc00d24   
#define                  DMA4_IRQ_STATUS  0xffc00d28   
#define              DMA4_PERIPHERAL_MAP  0xffc00d2c   
#define                DMA4_CURR_X_COUNT  0xffc00d30   
#define                DMA4_CURR_Y_COUNT  0xffc00d38   


#define               DMA5_NEXT_DESC_PTR  0xffc00d40   
#define                  DMA5_START_ADDR  0xffc00d44   
#define                      DMA5_CONFIG  0xffc00d48   
#define                     DMA5_X_COUNT  0xffc00d50   
#define                    DMA5_X_MODIFY  0xffc00d54   
#define                     DMA5_Y_COUNT  0xffc00d58   
#define                    DMA5_Y_MODIFY  0xffc00d5c   
#define               DMA5_CURR_DESC_PTR  0xffc00d60   
#define                   DMA5_CURR_ADDR  0xffc00d64   
#define                  DMA5_IRQ_STATUS  0xffc00d68   
#define              DMA5_PERIPHERAL_MAP  0xffc00d6c   
#define                DMA5_CURR_X_COUNT  0xffc00d70   
#define                DMA5_CURR_Y_COUNT  0xffc00d78   


#define               DMA6_NEXT_DESC_PTR  0xffc00d80   
#define                  DMA6_START_ADDR  0xffc00d84   
#define                      DMA6_CONFIG  0xffc00d88   
#define                     DMA6_X_COUNT  0xffc00d90   
#define                    DMA6_X_MODIFY  0xffc00d94   
#define                     DMA6_Y_COUNT  0xffc00d98   
#define                    DMA6_Y_MODIFY  0xffc00d9c   
#define               DMA6_CURR_DESC_PTR  0xffc00da0   
#define                   DMA6_CURR_ADDR  0xffc00da4   
#define                  DMA6_IRQ_STATUS  0xffc00da8   
#define              DMA6_PERIPHERAL_MAP  0xffc00dac   
#define                DMA6_CURR_X_COUNT  0xffc00db0   
#define                DMA6_CURR_Y_COUNT  0xffc00db8   


#define               DMA7_NEXT_DESC_PTR  0xffc00dc0   
#define                  DMA7_START_ADDR  0xffc00dc4   
#define                      DMA7_CONFIG  0xffc00dc8   
#define                     DMA7_X_COUNT  0xffc00dd0   
#define                    DMA7_X_MODIFY  0xffc00dd4   
#define                     DMA7_Y_COUNT  0xffc00dd8   
#define                    DMA7_Y_MODIFY  0xffc00ddc   
#define               DMA7_CURR_DESC_PTR  0xffc00de0   
#define                   DMA7_CURR_ADDR  0xffc00de4   
#define                  DMA7_IRQ_STATUS  0xffc00de8   
#define              DMA7_PERIPHERAL_MAP  0xffc00dec   
#define                DMA7_CURR_X_COUNT  0xffc00df0   
#define                DMA7_CURR_Y_COUNT  0xffc00df8   


#define               DMA8_NEXT_DESC_PTR  0xffc00e00   
#define                  DMA8_START_ADDR  0xffc00e04   
#define                      DMA8_CONFIG  0xffc00e08   
#define                     DMA8_X_COUNT  0xffc00e10   
#define                    DMA8_X_MODIFY  0xffc00e14   
#define                     DMA8_Y_COUNT  0xffc00e18   
#define                    DMA8_Y_MODIFY  0xffc00e1c   
#define               DMA8_CURR_DESC_PTR  0xffc00e20   
#define                   DMA8_CURR_ADDR  0xffc00e24   
#define                  DMA8_IRQ_STATUS  0xffc00e28   
#define              DMA8_PERIPHERAL_MAP  0xffc00e2c   
#define                DMA8_CURR_X_COUNT  0xffc00e30   
#define                DMA8_CURR_Y_COUNT  0xffc00e38   


#define               DMA9_NEXT_DESC_PTR  0xffc00e40   
#define                  DMA9_START_ADDR  0xffc00e44   
#define                      DMA9_CONFIG  0xffc00e48   
#define                     DMA9_X_COUNT  0xffc00e50   
#define                    DMA9_X_MODIFY  0xffc00e54   
#define                     DMA9_Y_COUNT  0xffc00e58   
#define                    DMA9_Y_MODIFY  0xffc00e5c   
#define               DMA9_CURR_DESC_PTR  0xffc00e60   
#define                   DMA9_CURR_ADDR  0xffc00e64   
#define                  DMA9_IRQ_STATUS  0xffc00e68   
#define              DMA9_PERIPHERAL_MAP  0xffc00e6c   
#define                DMA9_CURR_X_COUNT  0xffc00e70   
#define                DMA9_CURR_Y_COUNT  0xffc00e78   


#define              DMA10_NEXT_DESC_PTR  0xffc00e80   
#define                 DMA10_START_ADDR  0xffc00e84   
#define                     DMA10_CONFIG  0xffc00e88   
#define                    DMA10_X_COUNT  0xffc00e90   
#define                   DMA10_X_MODIFY  0xffc00e94   
#define                    DMA10_Y_COUNT  0xffc00e98   
#define                   DMA10_Y_MODIFY  0xffc00e9c   
#define              DMA10_CURR_DESC_PTR  0xffc00ea0   
#define                  DMA10_CURR_ADDR  0xffc00ea4   
#define                 DMA10_IRQ_STATUS  0xffc00ea8   
#define             DMA10_PERIPHERAL_MAP  0xffc00eac   
#define               DMA10_CURR_X_COUNT  0xffc00eb0   
#define               DMA10_CURR_Y_COUNT  0xffc00eb8   


#define              DMA11_NEXT_DESC_PTR  0xffc00ec0   
#define                 DMA11_START_ADDR  0xffc00ec4   
#define                     DMA11_CONFIG  0xffc00ec8   
#define                    DMA11_X_COUNT  0xffc00ed0   
#define                   DMA11_X_MODIFY  0xffc00ed4   
#define                    DMA11_Y_COUNT  0xffc00ed8   
#define                   DMA11_Y_MODIFY  0xffc00edc   
#define              DMA11_CURR_DESC_PTR  0xffc00ee0   
#define                  DMA11_CURR_ADDR  0xffc00ee4   
#define                 DMA11_IRQ_STATUS  0xffc00ee8   
#define             DMA11_PERIPHERAL_MAP  0xffc00eec   
#define               DMA11_CURR_X_COUNT  0xffc00ef0   
#define               DMA11_CURR_Y_COUNT  0xffc00ef8   


#define            MDMA_D0_NEXT_DESC_PTR  0xffc00f00   
#define               MDMA_D0_START_ADDR  0xffc00f04   
#define                   MDMA_D0_CONFIG  0xffc00f08   
#define                  MDMA_D0_X_COUNT  0xffc00f10   
#define                 MDMA_D0_X_MODIFY  0xffc00f14   
#define                  MDMA_D0_Y_COUNT  0xffc00f18   
#define                 MDMA_D0_Y_MODIFY  0xffc00f1c   
#define            MDMA_D0_CURR_DESC_PTR  0xffc00f20   
#define                MDMA_D0_CURR_ADDR  0xffc00f24   
#define               MDMA_D0_IRQ_STATUS  0xffc00f28   
#define           MDMA_D0_PERIPHERAL_MAP  0xffc00f2c   
#define             MDMA_D0_CURR_X_COUNT  0xffc00f30   
#define             MDMA_D0_CURR_Y_COUNT  0xffc00f38   
#define            MDMA_S0_NEXT_DESC_PTR  0xffc00f40   
#define               MDMA_S0_START_ADDR  0xffc00f44   
#define                   MDMA_S0_CONFIG  0xffc00f48   
#define                  MDMA_S0_X_COUNT  0xffc00f50   
#define                 MDMA_S0_X_MODIFY  0xffc00f54   
#define                  MDMA_S0_Y_COUNT  0xffc00f58   
#define                 MDMA_S0_Y_MODIFY  0xffc00f5c   
#define            MDMA_S0_CURR_DESC_PTR  0xffc00f60   
#define                MDMA_S0_CURR_ADDR  0xffc00f64   
#define               MDMA_S0_IRQ_STATUS  0xffc00f68   
#define           MDMA_S0_PERIPHERAL_MAP  0xffc00f6c   
#define             MDMA_S0_CURR_X_COUNT  0xffc00f70   
#define             MDMA_S0_CURR_Y_COUNT  0xffc00f78   


#define            MDMA_D1_NEXT_DESC_PTR  0xffc00f80   
#define               MDMA_D1_START_ADDR  0xffc00f84   
#define                   MDMA_D1_CONFIG  0xffc00f88   
#define                  MDMA_D1_X_COUNT  0xffc00f90   
#define                 MDMA_D1_X_MODIFY  0xffc00f94   
#define                  MDMA_D1_Y_COUNT  0xffc00f98   
#define                 MDMA_D1_Y_MODIFY  0xffc00f9c   
#define            MDMA_D1_CURR_DESC_PTR  0xffc00fa0   
#define                MDMA_D1_CURR_ADDR  0xffc00fa4   
#define               MDMA_D1_IRQ_STATUS  0xffc00fa8   
#define           MDMA_D1_PERIPHERAL_MAP  0xffc00fac   
#define             MDMA_D1_CURR_X_COUNT  0xffc00fb0   
#define             MDMA_D1_CURR_Y_COUNT  0xffc00fb8   
#define            MDMA_S1_NEXT_DESC_PTR  0xffc00fc0   
#define               MDMA_S1_START_ADDR  0xffc00fc4   
#define                   MDMA_S1_CONFIG  0xffc00fc8   
#define                  MDMA_S1_X_COUNT  0xffc00fd0   
#define                 MDMA_S1_X_MODIFY  0xffc00fd4   
#define                  MDMA_S1_Y_COUNT  0xffc00fd8   
#define                 MDMA_S1_Y_MODIFY  0xffc00fdc   
#define            MDMA_S1_CURR_DESC_PTR  0xffc00fe0   
#define                MDMA_S1_CURR_ADDR  0xffc00fe4   
#define               MDMA_S1_IRQ_STATUS  0xffc00fe8   
#define           MDMA_S1_PERIPHERAL_MAP  0xffc00fec   
#define             MDMA_S1_CURR_X_COUNT  0xffc00ff0   
#define             MDMA_S1_CURR_Y_COUNT  0xffc00ff8   


#define                        UART3_DLL  0xffc03100   
#define                        UART3_DLH  0xffc03104   
#define                       UART3_GCTL  0xffc03108   
#define                        UART3_LCR  0xffc0310c   
#define                        UART3_MCR  0xffc03110   
#define                        UART3_LSR  0xffc03114   
#define                        UART3_MSR  0xffc03118   
#define                        UART3_SCR  0xffc0311c   
#define                    UART3_IER_SET  0xffc03120   
#define                  UART3_IER_CLEAR  0xffc03124   
#define                        UART3_THR  0xffc03128   
#define                        UART3_RBR  0xffc0312c   


#define                     EPPI1_STATUS  0xffc01300   
#define                     EPPI1_HCOUNT  0xffc01304   
#define                     EPPI1_HDELAY  0xffc01308   
#define                     EPPI1_VCOUNT  0xffc0130c   
#define                     EPPI1_VDELAY  0xffc01310   
#define                      EPPI1_FRAME  0xffc01314   
#define                       EPPI1_LINE  0xffc01318   
#define                     EPPI1_CLKDIV  0xffc0131c   
#define                    EPPI1_CONTROL  0xffc01320   
#define                   EPPI1_FS1W_HBL  0xffc01324   
#define                  EPPI1_FS1P_AVPL  0xffc01328   
#define                   EPPI1_FS2W_LVB  0xffc0132c   
#define                  EPPI1_FS2P_LAVF  0xffc01330   
#define                       EPPI1_CLIP  0xffc01334   


#define                   PINT0_MASK_SET  0xffc01400   
#define                 PINT0_MASK_CLEAR  0xffc01404   
#define                    PINT0_REQUEST  0xffc01408   
#define                     PINT0_ASSIGN  0xffc0140c   
#define                   PINT0_EDGE_SET  0xffc01410   
#define                 PINT0_EDGE_CLEAR  0xffc01414   
#define                 PINT0_INVERT_SET  0xffc01418   
#define               PINT0_INVERT_CLEAR  0xffc0141c   
#define                   PINT0_PINSTATE  0xffc01420   
#define                      PINT0_LATCH  0xffc01424   


#define                   PINT1_MASK_SET  0xffc01430   
#define                 PINT1_MASK_CLEAR  0xffc01434   
#define                    PINT1_REQUEST  0xffc01438   
#define                     PINT1_ASSIGN  0xffc0143c   
#define                   PINT1_EDGE_SET  0xffc01440   
#define                 PINT1_EDGE_CLEAR  0xffc01444   
#define                 PINT1_INVERT_SET  0xffc01448   
#define               PINT1_INVERT_CLEAR  0xffc0144c   
#define                   PINT1_PINSTATE  0xffc01450   
#define                      PINT1_LATCH  0xffc01454   


#define                   PINT2_MASK_SET  0xffc01460   
#define                 PINT2_MASK_CLEAR  0xffc01464   
#define                    PINT2_REQUEST  0xffc01468   
#define                     PINT2_ASSIGN  0xffc0146c   
#define                   PINT2_EDGE_SET  0xffc01470   
#define                 PINT2_EDGE_CLEAR  0xffc01474   
#define                 PINT2_INVERT_SET  0xffc01478   
#define               PINT2_INVERT_CLEAR  0xffc0147c   
#define                   PINT2_PINSTATE  0xffc01480   
#define                      PINT2_LATCH  0xffc01484   


#define                   PINT3_MASK_SET  0xffc01490   
#define                 PINT3_MASK_CLEAR  0xffc01494   
#define                    PINT3_REQUEST  0xffc01498   
#define                     PINT3_ASSIGN  0xffc0149c   
#define                   PINT3_EDGE_SET  0xffc014a0   
#define                 PINT3_EDGE_CLEAR  0xffc014a4   
#define                 PINT3_INVERT_SET  0xffc014a8   
#define               PINT3_INVERT_CLEAR  0xffc014ac   
#define                   PINT3_PINSTATE  0xffc014b0   
#define                      PINT3_LATCH  0xffc014b4   


#define                        PORTA_FER  0xffc014c0   
#define                            PORTA  0xffc014c4   
#define                        PORTA_SET  0xffc014c8   
#define                      PORTA_CLEAR  0xffc014cc   
#define                    PORTA_DIR_SET  0xffc014d0   
#define                  PORTA_DIR_CLEAR  0xffc014d4   
#define                       PORTA_INEN  0xffc014d8   
#define                        PORTA_MUX  0xffc014dc   


#define                        PORTB_FER  0xffc014e0   
#define                            PORTB  0xffc014e4   
#define                        PORTB_SET  0xffc014e8   
#define                      PORTB_CLEAR  0xffc014ec   
#define                    PORTB_DIR_SET  0xffc014f0   
#define                  PORTB_DIR_CLEAR  0xffc014f4   
#define                       PORTB_INEN  0xffc014f8   
#define                        PORTB_MUX  0xffc014fc   


#define                        PORTC_FER  0xffc01500   
#define                            PORTC  0xffc01504   
#define                        PORTC_SET  0xffc01508   
#define                      PORTC_CLEAR  0xffc0150c   
#define                    PORTC_DIR_SET  0xffc01510   
#define                  PORTC_DIR_CLEAR  0xffc01514   
#define                       PORTC_INEN  0xffc01518   
#define                        PORTC_MUX  0xffc0151c   


#define                        PORTD_FER  0xffc01520   
#define                            PORTD  0xffc01524   
#define                        PORTD_SET  0xffc01528   
#define                      PORTD_CLEAR  0xffc0152c   
#define                    PORTD_DIR_SET  0xffc01530   
#define                  PORTD_DIR_CLEAR  0xffc01534   
#define                       PORTD_INEN  0xffc01538   
#define                        PORTD_MUX  0xffc0153c   


#define                        PORTE_FER  0xffc01540   
#define                            PORTE  0xffc01544   
#define                        PORTE_SET  0xffc01548   
#define                      PORTE_CLEAR  0xffc0154c   
#define                    PORTE_DIR_SET  0xffc01550   
#define                  PORTE_DIR_CLEAR  0xffc01554   
#define                       PORTE_INEN  0xffc01558   
#define                        PORTE_MUX  0xffc0155c   


#define                        PORTF_FER  0xffc01560   
#define                            PORTF  0xffc01564   
#define                        PORTF_SET  0xffc01568   
#define                      PORTF_CLEAR  0xffc0156c   
#define                    PORTF_DIR_SET  0xffc01570   
#define                  PORTF_DIR_CLEAR  0xffc01574   
#define                       PORTF_INEN  0xffc01578   
#define                        PORTF_MUX  0xffc0157c   


#define                        PORTG_FER  0xffc01580   
#define                            PORTG  0xffc01584   
#define                        PORTG_SET  0xffc01588   
#define                      PORTG_CLEAR  0xffc0158c   
#define                    PORTG_DIR_SET  0xffc01590   
#define                  PORTG_DIR_CLEAR  0xffc01594   
#define                       PORTG_INEN  0xffc01598   
#define                        PORTG_MUX  0xffc0159c   


#define                        PORTH_FER  0xffc015a0   
#define                            PORTH  0xffc015a4   
#define                        PORTH_SET  0xffc015a8   
#define                      PORTH_CLEAR  0xffc015ac   
#define                    PORTH_DIR_SET  0xffc015b0   
#define                  PORTH_DIR_CLEAR  0xffc015b4   
#define                       PORTH_INEN  0xffc015b8   
#define                        PORTH_MUX  0xffc015bc   


#define                        PORTI_FER  0xffc015c0   
#define                            PORTI  0xffc015c4   
#define                        PORTI_SET  0xffc015c8   
#define                      PORTI_CLEAR  0xffc015cc   
#define                    PORTI_DIR_SET  0xffc015d0   
#define                  PORTI_DIR_CLEAR  0xffc015d4   
#define                       PORTI_INEN  0xffc015d8   
#define                        PORTI_MUX  0xffc015dc   


#define                        PORTJ_FER  0xffc015e0   
#define                            PORTJ  0xffc015e4   
#define                        PORTJ_SET  0xffc015e8   
#define                      PORTJ_CLEAR  0xffc015ec   
#define                    PORTJ_DIR_SET  0xffc015f0   
#define                  PORTJ_DIR_CLEAR  0xffc015f4   
#define                       PORTJ_INEN  0xffc015f8   
#define                        PORTJ_MUX  0xffc015fc   


#define                    TIMER0_CONFIG  0xffc01600   
#define                   TIMER0_COUNTER  0xffc01604   
#define                    TIMER0_PERIOD  0xffc01608   
#define                     TIMER0_WIDTH  0xffc0160c   
#define                    TIMER1_CONFIG  0xffc01610   
#define                   TIMER1_COUNTER  0xffc01614   
#define                    TIMER1_PERIOD  0xffc01618   
#define                     TIMER1_WIDTH  0xffc0161c   
#define                    TIMER2_CONFIG  0xffc01620   
#define                   TIMER2_COUNTER  0xffc01624   
#define                    TIMER2_PERIOD  0xffc01628   
#define                     TIMER2_WIDTH  0xffc0162c   
#define                    TIMER3_CONFIG  0xffc01630   
#define                   TIMER3_COUNTER  0xffc01634   
#define                    TIMER3_PERIOD  0xffc01638   
#define                     TIMER3_WIDTH  0xffc0163c   
#define                    TIMER4_CONFIG  0xffc01640   
#define                   TIMER4_COUNTER  0xffc01644   
#define                    TIMER4_PERIOD  0xffc01648   
#define                     TIMER4_WIDTH  0xffc0164c   
#define                    TIMER5_CONFIG  0xffc01650   
#define                   TIMER5_COUNTER  0xffc01654   
#define                    TIMER5_PERIOD  0xffc01658   
#define                     TIMER5_WIDTH  0xffc0165c   
#define                    TIMER6_CONFIG  0xffc01660   
#define                   TIMER6_COUNTER  0xffc01664   
#define                    TIMER6_PERIOD  0xffc01668   
#define                     TIMER6_WIDTH  0xffc0166c   
#define                    TIMER7_CONFIG  0xffc01670   
#define                   TIMER7_COUNTER  0xffc01674   
#define                    TIMER7_PERIOD  0xffc01678   
#define                     TIMER7_WIDTH  0xffc0167c   


#define                    TIMER_ENABLE0  0xffc01680   
#define                   TIMER_DISABLE0  0xffc01684   
#define                    TIMER_STATUS0  0xffc01688   


#define                     DMAC1_TC_PER  0xffc01b0c   
#define                     DMAC1_TC_CNT  0xffc01b10   


#define              DMA12_NEXT_DESC_PTR  0xffc01c00   
#define                 DMA12_START_ADDR  0xffc01c04   
#define                     DMA12_CONFIG  0xffc01c08   
#define                    DMA12_X_COUNT  0xffc01c10   
#define                   DMA12_X_MODIFY  0xffc01c14   
#define                    DMA12_Y_COUNT  0xffc01c18   
#define                   DMA12_Y_MODIFY  0xffc01c1c   
#define              DMA12_CURR_DESC_PTR  0xffc01c20   
#define                  DMA12_CURR_ADDR  0xffc01c24   
#define                 DMA12_IRQ_STATUS  0xffc01c28   
#define             DMA12_PERIPHERAL_MAP  0xffc01c2c   
#define               DMA12_CURR_X_COUNT  0xffc01c30   
#define               DMA12_CURR_Y_COUNT  0xffc01c38   


#define              DMA13_NEXT_DESC_PTR  0xffc01c40   
#define                 DMA13_START_ADDR  0xffc01c44   
#define                     DMA13_CONFIG  0xffc01c48   
#define                    DMA13_X_COUNT  0xffc01c50   
#define                   DMA13_X_MODIFY  0xffc01c54   
#define                    DMA13_Y_COUNT  0xffc01c58   
#define                   DMA13_Y_MODIFY  0xffc01c5c   
#define              DMA13_CURR_DESC_PTR  0xffc01c60   
#define                  DMA13_CURR_ADDR  0xffc01c64   
#define                 DMA13_IRQ_STATUS  0xffc01c68   
#define             DMA13_PERIPHERAL_MAP  0xffc01c6c   
#define               DMA13_CURR_X_COUNT  0xffc01c70   
#define               DMA13_CURR_Y_COUNT  0xffc01c78   


#define              DMA14_NEXT_DESC_PTR  0xffc01c80   
#define                 DMA14_START_ADDR  0xffc01c84   
#define                     DMA14_CONFIG  0xffc01c88   
#define                    DMA14_X_COUNT  0xffc01c90   
#define                   DMA14_X_MODIFY  0xffc01c94   
#define                    DMA14_Y_COUNT  0xffc01c98   
#define                   DMA14_Y_MODIFY  0xffc01c9c   
#define              DMA14_CURR_DESC_PTR  0xffc01ca0   
#define                  DMA14_CURR_ADDR  0xffc01ca4   
#define                 DMA14_IRQ_STATUS  0xffc01ca8   
#define             DMA14_PERIPHERAL_MAP  0xffc01cac   
#define               DMA14_CURR_X_COUNT  0xffc01cb0   
#define               DMA14_CURR_Y_COUNT  0xffc01cb8   


#define              DMA15_NEXT_DESC_PTR  0xffc01cc0   
#define                 DMA15_START_ADDR  0xffc01cc4   
#define                     DMA15_CONFIG  0xffc01cc8   
#define                    DMA15_X_COUNT  0xffc01cd0   
#define                   DMA15_X_MODIFY  0xffc01cd4   
#define                    DMA15_Y_COUNT  0xffc01cd8   
#define                   DMA15_Y_MODIFY  0xffc01cdc   
#define              DMA15_CURR_DESC_PTR  0xffc01ce0   
#define                  DMA15_CURR_ADDR  0xffc01ce4   
#define                 DMA15_IRQ_STATUS  0xffc01ce8   
#define             DMA15_PERIPHERAL_MAP  0xffc01cec   
#define               DMA15_CURR_X_COUNT  0xffc01cf0   
#define               DMA15_CURR_Y_COUNT  0xffc01cf8   


#define              DMA16_NEXT_DESC_PTR  0xffc01d00   
#define                 DMA16_START_ADDR  0xffc01d04   
#define                     DMA16_CONFIG  0xffc01d08   
#define                    DMA16_X_COUNT  0xffc01d10   
#define                   DMA16_X_MODIFY  0xffc01d14   
#define                    DMA16_Y_COUNT  0xffc01d18   
#define                   DMA16_Y_MODIFY  0xffc01d1c   
#define              DMA16_CURR_DESC_PTR  0xffc01d20   
#define                  DMA16_CURR_ADDR  0xffc01d24   
#define                 DMA16_IRQ_STATUS  0xffc01d28   
#define             DMA16_PERIPHERAL_MAP  0xffc01d2c   
#define               DMA16_CURR_X_COUNT  0xffc01d30   
#define               DMA16_CURR_Y_COUNT  0xffc01d38   


#define              DMA17_NEXT_DESC_PTR  0xffc01d40   
#define                 DMA17_START_ADDR  0xffc01d44   
#define                     DMA17_CONFIG  0xffc01d48   
#define                    DMA17_X_COUNT  0xffc01d50   
#define                   DMA17_X_MODIFY  0xffc01d54   
#define                    DMA17_Y_COUNT  0xffc01d58   
#define                   DMA17_Y_MODIFY  0xffc01d5c   
#define              DMA17_CURR_DESC_PTR  0xffc01d60   
#define                  DMA17_CURR_ADDR  0xffc01d64   
#define                 DMA17_IRQ_STATUS  0xffc01d68   
#define             DMA17_PERIPHERAL_MAP  0xffc01d6c   
#define               DMA17_CURR_X_COUNT  0xffc01d70   
#define               DMA17_CURR_Y_COUNT  0xffc01d78   


#define              DMA18_NEXT_DESC_PTR  0xffc01d80   
#define                 DMA18_START_ADDR  0xffc01d84   
#define                     DMA18_CONFIG  0xffc01d88   
#define                    DMA18_X_COUNT  0xffc01d90   
#define                   DMA18_X_MODIFY  0xffc01d94   
#define                    DMA18_Y_COUNT  0xffc01d98   
#define                   DMA18_Y_MODIFY  0xffc01d9c   
#define              DMA18_CURR_DESC_PTR  0xffc01da0   
#define                  DMA18_CURR_ADDR  0xffc01da4   
#define                 DMA18_IRQ_STATUS  0xffc01da8   
#define             DMA18_PERIPHERAL_MAP  0xffc01dac   
#define               DMA18_CURR_X_COUNT  0xffc01db0   
#define               DMA18_CURR_Y_COUNT  0xffc01db8   


#define              DMA19_NEXT_DESC_PTR  0xffc01dc0   
#define                 DMA19_START_ADDR  0xffc01dc4   
#define                     DMA19_CONFIG  0xffc01dc8   
#define                    DMA19_X_COUNT  0xffc01dd0   
#define                   DMA19_X_MODIFY  0xffc01dd4   
#define                    DMA19_Y_COUNT  0xffc01dd8   
#define                   DMA19_Y_MODIFY  0xffc01ddc   
#define              DMA19_CURR_DESC_PTR  0xffc01de0   
#define                  DMA19_CURR_ADDR  0xffc01de4   
#define                 DMA19_IRQ_STATUS  0xffc01de8   
#define             DMA19_PERIPHERAL_MAP  0xffc01dec   
#define               DMA19_CURR_X_COUNT  0xffc01df0   
#define               DMA19_CURR_Y_COUNT  0xffc01df8   


#define              DMA20_NEXT_DESC_PTR  0xffc01e00   
#define                 DMA20_START_ADDR  0xffc01e04   
#define                     DMA20_CONFIG  0xffc01e08   
#define                    DMA20_X_COUNT  0xffc01e10   
#define                   DMA20_X_MODIFY  0xffc01e14   
#define                    DMA20_Y_COUNT  0xffc01e18   
#define                   DMA20_Y_MODIFY  0xffc01e1c   
#define              DMA20_CURR_DESC_PTR  0xffc01e20   
#define                  DMA20_CURR_ADDR  0xffc01e24   
#define                 DMA20_IRQ_STATUS  0xffc01e28   
#define             DMA20_PERIPHERAL_MAP  0xffc01e2c   
#define               DMA20_CURR_X_COUNT  0xffc01e30   
#define               DMA20_CURR_Y_COUNT  0xffc01e38   


#define              DMA21_NEXT_DESC_PTR  0xffc01e40   
#define                 DMA21_START_ADDR  0xffc01e44   
#define                     DMA21_CONFIG  0xffc01e48   
#define                    DMA21_X_COUNT  0xffc01e50   
#define                   DMA21_X_MODIFY  0xffc01e54   
#define                    DMA21_Y_COUNT  0xffc01e58   
#define                   DMA21_Y_MODIFY  0xffc01e5c   
#define              DMA21_CURR_DESC_PTR  0xffc01e60   
#define                  DMA21_CURR_ADDR  0xffc01e64   
#define                 DMA21_IRQ_STATUS  0xffc01e68   
#define             DMA21_PERIPHERAL_MAP  0xffc01e6c   
#define               DMA21_CURR_X_COUNT  0xffc01e70   
#define               DMA21_CURR_Y_COUNT  0xffc01e78   


#define              DMA22_NEXT_DESC_PTR  0xffc01e80   
#define                 DMA22_START_ADDR  0xffc01e84   
#define                     DMA22_CONFIG  0xffc01e88   
#define                    DMA22_X_COUNT  0xffc01e90   
#define                   DMA22_X_MODIFY  0xffc01e94   
#define                    DMA22_Y_COUNT  0xffc01e98   
#define                   DMA22_Y_MODIFY  0xffc01e9c   
#define              DMA22_CURR_DESC_PTR  0xffc01ea0   
#define                  DMA22_CURR_ADDR  0xffc01ea4   
#define                 DMA22_IRQ_STATUS  0xffc01ea8   
#define             DMA22_PERIPHERAL_MAP  0xffc01eac   
#define               DMA22_CURR_X_COUNT  0xffc01eb0   
#define               DMA22_CURR_Y_COUNT  0xffc01eb8   


#define              DMA23_NEXT_DESC_PTR  0xffc01ec0   
#define                 DMA23_START_ADDR  0xffc01ec4   
#define                     DMA23_CONFIG  0xffc01ec8   
#define                    DMA23_X_COUNT  0xffc01ed0   
#define                   DMA23_X_MODIFY  0xffc01ed4   
#define                    DMA23_Y_COUNT  0xffc01ed8   
#define                   DMA23_Y_MODIFY  0xffc01edc   
#define              DMA23_CURR_DESC_PTR  0xffc01ee0   
#define                  DMA23_CURR_ADDR  0xffc01ee4   
#define                 DMA23_IRQ_STATUS  0xffc01ee8   
#define             DMA23_PERIPHERAL_MAP  0xffc01eec   
#define               DMA23_CURR_X_COUNT  0xffc01ef0   
#define               DMA23_CURR_Y_COUNT  0xffc01ef8   


#define            MDMA_D2_NEXT_DESC_PTR  0xffc01f00   
#define               MDMA_D2_START_ADDR  0xffc01f04   
#define                   MDMA_D2_CONFIG  0xffc01f08   
#define                  MDMA_D2_X_COUNT  0xffc01f10   
#define                 MDMA_D2_X_MODIFY  0xffc01f14   
#define                  MDMA_D2_Y_COUNT  0xffc01f18   
#define                 MDMA_D2_Y_MODIFY  0xffc01f1c   
#define            MDMA_D2_CURR_DESC_PTR  0xffc01f20   
#define                MDMA_D2_CURR_ADDR  0xffc01f24   
#define               MDMA_D2_IRQ_STATUS  0xffc01f28   
#define           MDMA_D2_PERIPHERAL_MAP  0xffc01f2c   
#define             MDMA_D2_CURR_X_COUNT  0xffc01f30   
#define             MDMA_D2_CURR_Y_COUNT  0xffc01f38   
#define            MDMA_S2_NEXT_DESC_PTR  0xffc01f40   
#define               MDMA_S2_START_ADDR  0xffc01f44   
#define                   MDMA_S2_CONFIG  0xffc01f48   
#define                  MDMA_S2_X_COUNT  0xffc01f50   
#define                 MDMA_S2_X_MODIFY  0xffc01f54   
#define                  MDMA_S2_Y_COUNT  0xffc01f58   
#define                 MDMA_S2_Y_MODIFY  0xffc01f5c   
#define            MDMA_S2_CURR_DESC_PTR  0xffc01f60   
#define                MDMA_S2_CURR_ADDR  0xffc01f64   
#define               MDMA_S2_IRQ_STATUS  0xffc01f68   
#define           MDMA_S2_PERIPHERAL_MAP  0xffc01f6c   
#define             MDMA_S2_CURR_X_COUNT  0xffc01f70   
#define             MDMA_S2_CURR_Y_COUNT  0xffc01f78   


#define            MDMA_D3_NEXT_DESC_PTR  0xffc01f80   
#define               MDMA_D3_START_ADDR  0xffc01f84   
#define                   MDMA_D3_CONFIG  0xffc01f88   
#define                  MDMA_D3_X_COUNT  0xffc01f90   
#define                 MDMA_D3_X_MODIFY  0xffc01f94   
#define                  MDMA_D3_Y_COUNT  0xffc01f98   
#define                 MDMA_D3_Y_MODIFY  0xffc01f9c   
#define            MDMA_D3_CURR_DESC_PTR  0xffc01fa0   
#define                MDMA_D3_CURR_ADDR  0xffc01fa4   
#define               MDMA_D3_IRQ_STATUS  0xffc01fa8   
#define           MDMA_D3_PERIPHERAL_MAP  0xffc01fac   
#define             MDMA_D3_CURR_X_COUNT  0xffc01fb0   
#define             MDMA_D3_CURR_Y_COUNT  0xffc01fb8   
#define            MDMA_S3_NEXT_DESC_PTR  0xffc01fc0   
#define               MDMA_S3_START_ADDR  0xffc01fc4   
#define                   MDMA_S3_CONFIG  0xffc01fc8   
#define                  MDMA_S3_X_COUNT  0xffc01fd0   
#define                 MDMA_S3_X_MODIFY  0xffc01fd4   
#define                  MDMA_S3_Y_COUNT  0xffc01fd8   
#define                 MDMA_S3_Y_MODIFY  0xffc01fdc   
#define            MDMA_S3_CURR_DESC_PTR  0xffc01fe0   
#define                MDMA_S3_CURR_ADDR  0xffc01fe4   
#define               MDMA_S3_IRQ_STATUS  0xffc01fe8   
#define           MDMA_S3_PERIPHERAL_MAP  0xffc01fec   
#define             MDMA_S3_CURR_X_COUNT  0xffc01ff0   
#define             MDMA_S3_CURR_Y_COUNT  0xffc01ff8   


#define                        UART1_DLL  0xffc02000   
#define                        UART1_DLH  0xffc02004   
#define                       UART1_GCTL  0xffc02008   
#define                        UART1_LCR  0xffc0200c   
#define                        UART1_MCR  0xffc02010   
#define                        UART1_LSR  0xffc02014   
#define                        UART1_MSR  0xffc02018   
#define                        UART1_SCR  0xffc0201c   
#define                    UART1_IER_SET  0xffc02020   
#define                  UART1_IER_CLEAR  0xffc02024   
#define                        UART1_THR  0xffc02028   
#define                        UART1_RBR  0xffc0202c   



#define                     SPI1_REGBASE  0xffc02300
#define                         SPI1_CTL  0xffc02300   
#define                         SPI1_FLG  0xffc02304   
#define                        SPI1_STAT  0xffc02308   
#define                        SPI1_TDBR  0xffc0230c   
#define                        SPI1_RDBR  0xffc02310   
#define                        SPI1_BAUD  0xffc02314   
#define                      SPI1_SHADOW  0xffc02318   


#define                      SPORT2_TCR1  0xffc02500   
#define                      SPORT2_TCR2  0xffc02504   
#define                   SPORT2_TCLKDIV  0xffc02508   
#define                    SPORT2_TFSDIV  0xffc0250c   
#define                        SPORT2_TX  0xffc02510   
#define                        SPORT2_RX  0xffc02518   
#define                      SPORT2_RCR1  0xffc02520   
#define                      SPORT2_RCR2  0xffc02524   
#define                   SPORT2_RCLKDIV  0xffc02528   
#define                    SPORT2_RFSDIV  0xffc0252c   
#define                      SPORT2_STAT  0xffc02530   
#define                      SPORT2_CHNL  0xffc02534   
#define                     SPORT2_MCMC1  0xffc02538   
#define                     SPORT2_MCMC2  0xffc0253c   
#define                     SPORT2_MTCS0  0xffc02540   
#define                     SPORT2_MTCS1  0xffc02544   
#define                     SPORT2_MTCS2  0xffc02548   
#define                     SPORT2_MTCS3  0xffc0254c   
#define                     SPORT2_MRCS0  0xffc02550   
#define                     SPORT2_MRCS1  0xffc02554   
#define                     SPORT2_MRCS2  0xffc02558   
#define                     SPORT2_MRCS3  0xffc0255c   


#define                      SPORT3_TCR1  0xffc02600   
#define                      SPORT3_TCR2  0xffc02604   
#define                   SPORT3_TCLKDIV  0xffc02608   
#define                    SPORT3_TFSDIV  0xffc0260c   
#define                        SPORT3_TX  0xffc02610   
#define                        SPORT3_RX  0xffc02618   
#define                      SPORT3_RCR1  0xffc02620   
#define                      SPORT3_RCR2  0xffc02624   
#define                   SPORT3_RCLKDIV  0xffc02628   
#define                    SPORT3_RFSDIV  0xffc0262c   
#define                      SPORT3_STAT  0xffc02630   
#define                      SPORT3_CHNL  0xffc02634   
#define                     SPORT3_MCMC1  0xffc02638   
#define                     SPORT3_MCMC2  0xffc0263c   
#define                     SPORT3_MTCS0  0xffc02640   
#define                     SPORT3_MTCS1  0xffc02644   
#define                     SPORT3_MTCS2  0xffc02648   
#define                     SPORT3_MTCS3  0xffc0264c   
#define                     SPORT3_MRCS0  0xffc02650   
#define                     SPORT3_MRCS1  0xffc02654   
#define                     SPORT3_MRCS2  0xffc02658   
#define                     SPORT3_MRCS3  0xffc0265c   


#define                     EPPI2_STATUS  0xffc02900   
#define                     EPPI2_HCOUNT  0xffc02904   
#define                     EPPI2_HDELAY  0xffc02908   
#define                     EPPI2_VCOUNT  0xffc0290c   
#define                     EPPI2_VDELAY  0xffc02910   
#define                      EPPI2_FRAME  0xffc02914   
#define                       EPPI2_LINE  0xffc02918   
#define                     EPPI2_CLKDIV  0xffc0291c   
#define                    EPPI2_CONTROL  0xffc02920   
#define                   EPPI2_FS1W_HBL  0xffc02924   
#define                  EPPI2_FS1P_AVPL  0xffc02928   
#define                   EPPI2_FS2W_LVB  0xffc0292c   
#define                  EPPI2_FS2P_LAVF  0xffc02930   
#define                       EPPI2_CLIP  0xffc02934   


#define                         CAN0_MC1  0xffc02a00   
#define                         CAN0_MD1  0xffc02a04   
#define                        CAN0_TRS1  0xffc02a08   
#define                        CAN0_TRR1  0xffc02a0c   
#define                         CAN0_TA1  0xffc02a10   
#define                         CAN0_AA1  0xffc02a14   
#define                        CAN0_RMP1  0xffc02a18   
#define                        CAN0_RML1  0xffc02a1c   
#define                      CAN0_MBTIF1  0xffc02a20   
#define                      CAN0_MBRIF1  0xffc02a24   
#define                       CAN0_MBIM1  0xffc02a28   
#define                        CAN0_RFH1  0xffc02a2c   
#define                       CAN0_OPSS1  0xffc02a30   


#define                         CAN0_MC2  0xffc02a40   
#define                         CAN0_MD2  0xffc02a44   
#define                        CAN0_TRS2  0xffc02a48   
#define                        CAN0_TRR2  0xffc02a4c   
#define                         CAN0_TA2  0xffc02a50   
#define                         CAN0_AA2  0xffc02a54   
#define                        CAN0_RMP2  0xffc02a58   
#define                        CAN0_RML2  0xffc02a5c   
#define                      CAN0_MBTIF2  0xffc02a60   
#define                      CAN0_MBRIF2  0xffc02a64   
#define                       CAN0_MBIM2  0xffc02a68   
#define                        CAN0_RFH2  0xffc02a6c   
#define                       CAN0_OPSS2  0xffc02a70   


#define                       CAN0_CLOCK  0xffc02a80   
#define                      CAN0_TIMING  0xffc02a84   
#define                       CAN0_DEBUG  0xffc02a88   
#define                      CAN0_STATUS  0xffc02a8c   
#define                         CAN0_CEC  0xffc02a90   
#define                         CAN0_GIS  0xffc02a94   
#define                         CAN0_GIM  0xffc02a98   
#define                         CAN0_GIF  0xffc02a9c   
#define                     CAN0_CONTROL  0xffc02aa0   
#define                        CAN0_INTR  0xffc02aa4   
#define                        CAN0_MBTD  0xffc02aac   
#define                         CAN0_EWR  0xffc02ab0   
#define                         CAN0_ESR  0xffc02ab4   
#define                       CAN0_UCCNT  0xffc02ac4   
#define                        CAN0_UCRC  0xffc02ac8   
#define                       CAN0_UCCNF  0xffc02acc   


#define                       CAN0_AM00L  0xffc02b00   
#define                       CAN0_AM00H  0xffc02b04   
#define                       CAN0_AM01L  0xffc02b08   
#define                       CAN0_AM01H  0xffc02b0c   
#define                       CAN0_AM02L  0xffc02b10   
#define                       CAN0_AM02H  0xffc02b14   
#define                       CAN0_AM03L  0xffc02b18   
#define                       CAN0_AM03H  0xffc02b1c   
#define                       CAN0_AM04L  0xffc02b20   
#define                       CAN0_AM04H  0xffc02b24   
#define                       CAN0_AM05L  0xffc02b28   
#define                       CAN0_AM05H  0xffc02b2c   
#define                       CAN0_AM06L  0xffc02b30   
#define                       CAN0_AM06H  0xffc02b34   
#define                       CAN0_AM07L  0xffc02b38   
#define                       CAN0_AM07H  0xffc02b3c   
#define                       CAN0_AM08L  0xffc02b40   
#define                       CAN0_AM08H  0xffc02b44   
#define                       CAN0_AM09L  0xffc02b48   
#define                       CAN0_AM09H  0xffc02b4c   
#define                       CAN0_AM10L  0xffc02b50   
#define                       CAN0_AM10H  0xffc02b54   
#define                       CAN0_AM11L  0xffc02b58   
#define                       CAN0_AM11H  0xffc02b5c   
#define                       CAN0_AM12L  0xffc02b60   
#define                       CAN0_AM12H  0xffc02b64   
#define                       CAN0_AM13L  0xffc02b68   
#define                       CAN0_AM13H  0xffc02b6c   
#define                       CAN0_AM14L  0xffc02b70   
#define                       CAN0_AM14H  0xffc02b74   
#define                       CAN0_AM15L  0xffc02b78   
#define                       CAN0_AM15H  0xffc02b7c   


#define                       CAN0_AM16L  0xffc02b80   
#define                       CAN0_AM16H  0xffc02b84   
#define                       CAN0_AM17L  0xffc02b88   
#define                       CAN0_AM17H  0xffc02b8c   
#define                       CAN0_AM18L  0xffc02b90   
#define                       CAN0_AM18H  0xffc02b94   
#define                       CAN0_AM19L  0xffc02b98   
#define                       CAN0_AM19H  0xffc02b9c   
#define                       CAN0_AM20L  0xffc02ba0   
#define                       CAN0_AM20H  0xffc02ba4   
#define                       CAN0_AM21L  0xffc02ba8   
#define                       CAN0_AM21H  0xffc02bac   
#define                       CAN0_AM22L  0xffc02bb0   
#define                       CAN0_AM22H  0xffc02bb4   
#define                       CAN0_AM23L  0xffc02bb8   
#define                       CAN0_AM23H  0xffc02bbc   
#define                       CAN0_AM24L  0xffc02bc0   
#define                       CAN0_AM24H  0xffc02bc4   
#define                       CAN0_AM25L  0xffc02bc8   
#define                       CAN0_AM25H  0xffc02bcc   
#define                       CAN0_AM26L  0xffc02bd0   
#define                       CAN0_AM26H  0xffc02bd4   
#define                       CAN0_AM27L  0xffc02bd8   
#define                       CAN0_AM27H  0xffc02bdc   
#define                       CAN0_AM28L  0xffc02be0   
#define                       CAN0_AM28H  0xffc02be4   
#define                       CAN0_AM29L  0xffc02be8   
#define                       CAN0_AM29H  0xffc02bec   
#define                       CAN0_AM30L  0xffc02bf0   
#define                       CAN0_AM30H  0xffc02bf4   
#define                       CAN0_AM31L  0xffc02bf8   
#define                       CAN0_AM31H  0xffc02bfc   


#define                  CAN0_MB00_DATA0  0xffc02c00   
#define                  CAN0_MB00_DATA1  0xffc02c04   
#define                  CAN0_MB00_DATA2  0xffc02c08   
#define                  CAN0_MB00_DATA3  0xffc02c0c   
#define                 CAN0_MB00_LENGTH  0xffc02c10   
#define              CAN0_MB00_TIMESTAMP  0xffc02c14   
#define                    CAN0_MB00_ID0  0xffc02c18   
#define                    CAN0_MB00_ID1  0xffc02c1c   
#define                  CAN0_MB01_DATA0  0xffc02c20   
#define                  CAN0_MB01_DATA1  0xffc02c24   
#define                  CAN0_MB01_DATA2  0xffc02c28   
#define                  CAN0_MB01_DATA3  0xffc02c2c   
#define                 CAN0_MB01_LENGTH  0xffc02c30   
#define              CAN0_MB01_TIMESTAMP  0xffc02c34   
#define                    CAN0_MB01_ID0  0xffc02c38   
#define                    CAN0_MB01_ID1  0xffc02c3c   
#define                  CAN0_MB02_DATA0  0xffc02c40   
#define                  CAN0_MB02_DATA1  0xffc02c44   
#define                  CAN0_MB02_DATA2  0xffc02c48   
#define                  CAN0_MB02_DATA3  0xffc02c4c   
#define                 CAN0_MB02_LENGTH  0xffc02c50   
#define              CAN0_MB02_TIMESTAMP  0xffc02c54   
#define                    CAN0_MB02_ID0  0xffc02c58   
#define                    CAN0_MB02_ID1  0xffc02c5c   
#define                  CAN0_MB03_DATA0  0xffc02c60   
#define                  CAN0_MB03_DATA1  0xffc02c64   
#define                  CAN0_MB03_DATA2  0xffc02c68   
#define                  CAN0_MB03_DATA3  0xffc02c6c   
#define                 CAN0_MB03_LENGTH  0xffc02c70   
#define              CAN0_MB03_TIMESTAMP  0xffc02c74   
#define                    CAN0_MB03_ID0  0xffc02c78   
#define                    CAN0_MB03_ID1  0xffc02c7c   
#define                  CAN0_MB04_DATA0  0xffc02c80   
#define                  CAN0_MB04_DATA1  0xffc02c84   
#define                  CAN0_MB04_DATA2  0xffc02c88   
#define                  CAN0_MB04_DATA3  0xffc02c8c   
#define                 CAN0_MB04_LENGTH  0xffc02c90   
#define              CAN0_MB04_TIMESTAMP  0xffc02c94   
#define                    CAN0_MB04_ID0  0xffc02c98   
#define                    CAN0_MB04_ID1  0xffc02c9c   
#define                  CAN0_MB05_DATA0  0xffc02ca0   
#define                  CAN0_MB05_DATA1  0xffc02ca4   
#define                  CAN0_MB05_DATA2  0xffc02ca8   
#define                  CAN0_MB05_DATA3  0xffc02cac   
#define                 CAN0_MB05_LENGTH  0xffc02cb0   
#define              CAN0_MB05_TIMESTAMP  0xffc02cb4   
#define                    CAN0_MB05_ID0  0xffc02cb8   
#define                    CAN0_MB05_ID1  0xffc02cbc   
#define                  CAN0_MB06_DATA0  0xffc02cc0   
#define                  CAN0_MB06_DATA1  0xffc02cc4   
#define                  CAN0_MB06_DATA2  0xffc02cc8   
#define                  CAN0_MB06_DATA3  0xffc02ccc   
#define                 CAN0_MB06_LENGTH  0xffc02cd0   
#define              CAN0_MB06_TIMESTAMP  0xffc02cd4   
#define                    CAN0_MB06_ID0  0xffc02cd8   
#define                    CAN0_MB06_ID1  0xffc02cdc   
#define                  CAN0_MB07_DATA0  0xffc02ce0   
#define                  CAN0_MB07_DATA1  0xffc02ce4   
#define                  CAN0_MB07_DATA2  0xffc02ce8   
#define                  CAN0_MB07_DATA3  0xffc02cec   
#define                 CAN0_MB07_LENGTH  0xffc02cf0   
#define              CAN0_MB07_TIMESTAMP  0xffc02cf4   
#define                    CAN0_MB07_ID0  0xffc02cf8   
#define                    CAN0_MB07_ID1  0xffc02cfc   
#define                  CAN0_MB08_DATA0  0xffc02d00   
#define                  CAN0_MB08_DATA1  0xffc02d04   
#define                  CAN0_MB08_DATA2  0xffc02d08   
#define                  CAN0_MB08_DATA3  0xffc02d0c   
#define                 CAN0_MB08_LENGTH  0xffc02d10   
#define              CAN0_MB08_TIMESTAMP  0xffc02d14   
#define                    CAN0_MB08_ID0  0xffc02d18   
#define                    CAN0_MB08_ID1  0xffc02d1c   
#define                  CAN0_MB09_DATA0  0xffc02d20   
#define                  CAN0_MB09_DATA1  0xffc02d24   
#define                  CAN0_MB09_DATA2  0xffc02d28   
#define                  CAN0_MB09_DATA3  0xffc02d2c   
#define                 CAN0_MB09_LENGTH  0xffc02d30   
#define              CAN0_MB09_TIMESTAMP  0xffc02d34   
#define                    CAN0_MB09_ID0  0xffc02d38   
#define                    CAN0_MB09_ID1  0xffc02d3c   
#define                  CAN0_MB10_DATA0  0xffc02d40   
#define                  CAN0_MB10_DATA1  0xffc02d44   
#define                  CAN0_MB10_DATA2  0xffc02d48   
#define                  CAN0_MB10_DATA3  0xffc02d4c   
#define                 CAN0_MB10_LENGTH  0xffc02d50   
#define              CAN0_MB10_TIMESTAMP  0xffc02d54   
#define                    CAN0_MB10_ID0  0xffc02d58   
#define                    CAN0_MB10_ID1  0xffc02d5c   
#define                  CAN0_MB11_DATA0  0xffc02d60   
#define                  CAN0_MB11_DATA1  0xffc02d64   
#define                  CAN0_MB11_DATA2  0xffc02d68   
#define                  CAN0_MB11_DATA3  0xffc02d6c   
#define                 CAN0_MB11_LENGTH  0xffc02d70   
#define              CAN0_MB11_TIMESTAMP  0xffc02d74   
#define                    CAN0_MB11_ID0  0xffc02d78   
#define                    CAN0_MB11_ID1  0xffc02d7c   
#define                  CAN0_MB12_DATA0  0xffc02d80   
#define                  CAN0_MB12_DATA1  0xffc02d84   
#define                  CAN0_MB12_DATA2  0xffc02d88   
#define                  CAN0_MB12_DATA3  0xffc02d8c   
#define                 CAN0_MB12_LENGTH  0xffc02d90   
#define              CAN0_MB12_TIMESTAMP  0xffc02d94   
#define                    CAN0_MB12_ID0  0xffc02d98   
#define                    CAN0_MB12_ID1  0xffc02d9c   
#define                  CAN0_MB13_DATA0  0xffc02da0   
#define                  CAN0_MB13_DATA1  0xffc02da4   
#define                  CAN0_MB13_DATA2  0xffc02da8   
#define                  CAN0_MB13_DATA3  0xffc02dac   
#define                 CAN0_MB13_LENGTH  0xffc02db0   
#define              CAN0_MB13_TIMESTAMP  0xffc02db4   
#define                    CAN0_MB13_ID0  0xffc02db8   
#define                    CAN0_MB13_ID1  0xffc02dbc   
#define                  CAN0_MB14_DATA0  0xffc02dc0   
#define                  CAN0_MB14_DATA1  0xffc02dc4   
#define                  CAN0_MB14_DATA2  0xffc02dc8   
#define                  CAN0_MB14_DATA3  0xffc02dcc   
#define                 CAN0_MB14_LENGTH  0xffc02dd0   
#define              CAN0_MB14_TIMESTAMP  0xffc02dd4   
#define                    CAN0_MB14_ID0  0xffc02dd8   
#define                    CAN0_MB14_ID1  0xffc02ddc   
#define                  CAN0_MB15_DATA0  0xffc02de0   
#define                  CAN0_MB15_DATA1  0xffc02de4   
#define                  CAN0_MB15_DATA2  0xffc02de8   
#define                  CAN0_MB15_DATA3  0xffc02dec   
#define                 CAN0_MB15_LENGTH  0xffc02df0   
#define              CAN0_MB15_TIMESTAMP  0xffc02df4   
#define                    CAN0_MB15_ID0  0xffc02df8   
#define                    CAN0_MB15_ID1  0xffc02dfc   


#define                  CAN0_MB16_DATA0  0xffc02e00   
#define                  CAN0_MB16_DATA1  0xffc02e04   
#define                  CAN0_MB16_DATA2  0xffc02e08   
#define                  CAN0_MB16_DATA3  0xffc02e0c   
#define                 CAN0_MB16_LENGTH  0xffc02e10   
#define              CAN0_MB16_TIMESTAMP  0xffc02e14   
#define                    CAN0_MB16_ID0  0xffc02e18   
#define                    CAN0_MB16_ID1  0xffc02e1c   
#define                  CAN0_MB17_DATA0  0xffc02e20   
#define                  CAN0_MB17_DATA1  0xffc02e24   
#define                  CAN0_MB17_DATA2  0xffc02e28   
#define                  CAN0_MB17_DATA3  0xffc02e2c   
#define                 CAN0_MB17_LENGTH  0xffc02e30   
#define              CAN0_MB17_TIMESTAMP  0xffc02e34   
#define                    CAN0_MB17_ID0  0xffc02e38   
#define                    CAN0_MB17_ID1  0xffc02e3c   
#define                  CAN0_MB18_DATA0  0xffc02e40   
#define                  CAN0_MB18_DATA1  0xffc02e44   
#define                  CAN0_MB18_DATA2  0xffc02e48   
#define                  CAN0_MB18_DATA3  0xffc02e4c   
#define                 CAN0_MB18_LENGTH  0xffc02e50   
#define              CAN0_MB18_TIMESTAMP  0xffc02e54   
#define                    CAN0_MB18_ID0  0xffc02e58   
#define                    CAN0_MB18_ID1  0xffc02e5c   
#define                  CAN0_MB19_DATA0  0xffc02e60   
#define                  CAN0_MB19_DATA1  0xffc02e64   
#define                  CAN0_MB19_DATA2  0xffc02e68   
#define                  CAN0_MB19_DATA3  0xffc02e6c   
#define                 CAN0_MB19_LENGTH  0xffc02e70   
#define              CAN0_MB19_TIMESTAMP  0xffc02e74   
#define                    CAN0_MB19_ID0  0xffc02e78   
#define                    CAN0_MB19_ID1  0xffc02e7c   
#define                  CAN0_MB20_DATA0  0xffc02e80   
#define                  CAN0_MB20_DATA1  0xffc02e84   
#define                  CAN0_MB20_DATA2  0xffc02e88   
#define                  CAN0_MB20_DATA3  0xffc02e8c   
#define                 CAN0_MB20_LENGTH  0xffc02e90   
#define              CAN0_MB20_TIMESTAMP  0xffc02e94   
#define                    CAN0_MB20_ID0  0xffc02e98   
#define                    CAN0_MB20_ID1  0xffc02e9c   
#define                  CAN0_MB21_DATA0  0xffc02ea0   
#define                  CAN0_MB21_DATA1  0xffc02ea4   
#define                  CAN0_MB21_DATA2  0xffc02ea8   
#define                  CAN0_MB21_DATA3  0xffc02eac   
#define                 CAN0_MB21_LENGTH  0xffc02eb0   
#define              CAN0_MB21_TIMESTAMP  0xffc02eb4   
#define                    CAN0_MB21_ID0  0xffc02eb8   
#define                    CAN0_MB21_ID1  0xffc02ebc   
#define                  CAN0_MB22_DATA0  0xffc02ec0   
#define                  CAN0_MB22_DATA1  0xffc02ec4   
#define                  CAN0_MB22_DATA2  0xffc02ec8   
#define                  CAN0_MB22_DATA3  0xffc02ecc   
#define                 CAN0_MB22_LENGTH  0xffc02ed0   
#define              CAN0_MB22_TIMESTAMP  0xffc02ed4   
#define                    CAN0_MB22_ID0  0xffc02ed8   
#define                    CAN0_MB22_ID1  0xffc02edc   
#define                  CAN0_MB23_DATA0  0xffc02ee0   
#define                  CAN0_MB23_DATA1  0xffc02ee4   
#define                  CAN0_MB23_DATA2  0xffc02ee8   
#define                  CAN0_MB23_DATA3  0xffc02eec   
#define                 CAN0_MB23_LENGTH  0xffc02ef0   
#define              CAN0_MB23_TIMESTAMP  0xffc02ef4   
#define                    CAN0_MB23_ID0  0xffc02ef8   
#define                    CAN0_MB23_ID1  0xffc02efc   
#define                  CAN0_MB24_DATA0  0xffc02f00   
#define                  CAN0_MB24_DATA1  0xffc02f04   
#define                  CAN0_MB24_DATA2  0xffc02f08   
#define                  CAN0_MB24_DATA3  0xffc02f0c   
#define                 CAN0_MB24_LENGTH  0xffc02f10   
#define              CAN0_MB24_TIMESTAMP  0xffc02f14   
#define                    CAN0_MB24_ID0  0xffc02f18   
#define                    CAN0_MB24_ID1  0xffc02f1c   
#define                  CAN0_MB25_DATA0  0xffc02f20   
#define                  CAN0_MB25_DATA1  0xffc02f24   
#define                  CAN0_MB25_DATA2  0xffc02f28   
#define                  CAN0_MB25_DATA3  0xffc02f2c   
#define                 CAN0_MB25_LENGTH  0xffc02f30   
#define              CAN0_MB25_TIMESTAMP  0xffc02f34   
#define                    CAN0_MB25_ID0  0xffc02f38   
#define                    CAN0_MB25_ID1  0xffc02f3c   
#define                  CAN0_MB26_DATA0  0xffc02f40   
#define                  CAN0_MB26_DATA1  0xffc02f44   
#define                  CAN0_MB26_DATA2  0xffc02f48   
#define                  CAN0_MB26_DATA3  0xffc02f4c   
#define                 CAN0_MB26_LENGTH  0xffc02f50   
#define              CAN0_MB26_TIMESTAMP  0xffc02f54   
#define                    CAN0_MB26_ID0  0xffc02f58   
#define                    CAN0_MB26_ID1  0xffc02f5c   
#define                  CAN0_MB27_DATA0  0xffc02f60   
#define                  CAN0_MB27_DATA1  0xffc02f64   
#define                  CAN0_MB27_DATA2  0xffc02f68   
#define                  CAN0_MB27_DATA3  0xffc02f6c   
#define                 CAN0_MB27_LENGTH  0xffc02f70   
#define              CAN0_MB27_TIMESTAMP  0xffc02f74   
#define                    CAN0_MB27_ID0  0xffc02f78   
#define                    CAN0_MB27_ID1  0xffc02f7c   
#define                  CAN0_MB28_DATA0  0xffc02f80   
#define                  CAN0_MB28_DATA1  0xffc02f84   
#define                  CAN0_MB28_DATA2  0xffc02f88   
#define                  CAN0_MB28_DATA3  0xffc02f8c   
#define                 CAN0_MB28_LENGTH  0xffc02f90   
#define              CAN0_MB28_TIMESTAMP  0xffc02f94   
#define                    CAN0_MB28_ID0  0xffc02f98   
#define                    CAN0_MB28_ID1  0xffc02f9c   
#define                  CAN0_MB29_DATA0  0xffc02fa0   
#define                  CAN0_MB29_DATA1  0xffc02fa4   
#define                  CAN0_MB29_DATA2  0xffc02fa8   
#define                  CAN0_MB29_DATA3  0xffc02fac   
#define                 CAN0_MB29_LENGTH  0xffc02fb0   
#define              CAN0_MB29_TIMESTAMP  0xffc02fb4   
#define                    CAN0_MB29_ID0  0xffc02fb8   
#define                    CAN0_MB29_ID1  0xffc02fbc   
#define                  CAN0_MB30_DATA0  0xffc02fc0   
#define                  CAN0_MB30_DATA1  0xffc02fc4   
#define                  CAN0_MB30_DATA2  0xffc02fc8   
#define                  CAN0_MB30_DATA3  0xffc02fcc   
#define                 CAN0_MB30_LENGTH  0xffc02fd0   
#define              CAN0_MB30_TIMESTAMP  0xffc02fd4   
#define                    CAN0_MB30_ID0  0xffc02fd8   
#define                    CAN0_MB30_ID1  0xffc02fdc   
#define                  CAN0_MB31_DATA0  0xffc02fe0   
#define                  CAN0_MB31_DATA1  0xffc02fe4   
#define                  CAN0_MB31_DATA2  0xffc02fe8   
#define                  CAN0_MB31_DATA3  0xffc02fec   
#define                 CAN0_MB31_LENGTH  0xffc02ff0   
#define              CAN0_MB31_TIMESTAMP  0xffc02ff4   
#define                    CAN0_MB31_ID0  0xffc02ff8   
#define                    CAN0_MB31_ID1  0xffc02ffc   


#define                        UART3_DLL  0xffc03100   
#define                        UART3_DLH  0xffc03104   
#define                       UART3_GCTL  0xffc03108   
#define                        UART3_LCR  0xffc0310c   
#define                        UART3_MCR  0xffc03110   
#define                        UART3_LSR  0xffc03114   
#define                        UART3_MSR  0xffc03118   
#define                        UART3_SCR  0xffc0311c   
#define                    UART3_IER_SET  0xffc03120   
#define                  UART3_IER_CLEAR  0xffc03124   
#define                        UART3_THR  0xffc03128   
#define                        UART3_RBR  0xffc0312c   


#define                          NFC_CTL  0xffc03b00   
#define                         NFC_STAT  0xffc03b04   
#define                      NFC_IRQSTAT  0xffc03b08   
#define                      NFC_IRQMASK  0xffc03b0c   
#define                         NFC_ECC0  0xffc03b10   
#define                         NFC_ECC1  0xffc03b14   
#define                         NFC_ECC2  0xffc03b18   
#define                         NFC_ECC3  0xffc03b1c   
#define                        NFC_COUNT  0xffc03b20   
#define                          NFC_RST  0xffc03b24   
#define                        NFC_PGCTL  0xffc03b28   
#define                         NFC_READ  0xffc03b2c   
#define                         NFC_ADDR  0xffc03b40   
#define                          NFC_CMD  0xffc03b44   
#define                      NFC_DATA_WR  0xffc03b48   
#define                      NFC_DATA_RD  0xffc03b4c   


#define                       CNT_CONFIG  0xffc04200   
#define                        CNT_IMASK  0xffc04204   
#define                       CNT_STATUS  0xffc04208   
#define                      CNT_COMMAND  0xffc0420c   
#define                     CNT_DEBOUNCE  0xffc04210   
#define                      CNT_COUNTER  0xffc04214   
#define                          CNT_MAX  0xffc04218   
#define                          CNT_MIN  0xffc0421c   


#define                      OTP_CONTROL  0xffc04300   
#define                          OTP_BEN  0xffc04304   
#define                       OTP_STATUS  0xffc04308   
#define                       OTP_TIMING  0xffc0430c   


#define                    SECURE_SYSSWT  0xffc04320   
#define                   SECURE_CONTROL  0xffc04324   
#define                    SECURE_STATUS  0xffc04328   


#define                    DMAC1_PERIMUX  0xffc04340   


#define                        OTP_DATA0  0xffc04380   
#define                        OTP_DATA1  0xffc04384   
#define                        OTP_DATA2  0xffc04388   
#define                        OTP_DATA3  0xffc0438c   



#define SIC_UNMASK_ALL         0x00000000	
#define SIC_MASK_ALL           0xFFFFFFFF	
#define SIC_MASK(x)	       (1 << (x))	
#define SIC_UNMASK(x) (0xFFFFFFFF ^ (1 << (x)))	

#define IWR_DISABLE_ALL        0x00000000	
#define IWR_ENABLE_ALL         0xFFFFFFFF	
#define IWR_ENABLE(x)	       (1 << (x))	
#define IWR_DISABLE(x) (0xFFFFFFFF ^ (1 << (x)))	


#define            PLL_WAKEUP  0x1        


#define              DMA0_ERR  0x2        
#define             EPPI0_ERR  0x4        
#define            SPORT0_ERR  0x8        
#define            SPORT1_ERR  0x10       
#define              SPI0_ERR  0x20       
#define             UART0_ERR  0x40       
#define                   RTC  0x80       
#define                 DMA12  0x100      
#define                  DMA0  0x200      
#define                  DMA1  0x400      
#define                  DMA2  0x800      
#define                  DMA3  0x1000     
#define                  DMA4  0x2000     
#define                  DMA6  0x4000     
#define                  DMA7  0x8000     
#define                 PINT0  0x80000    
#define                 PINT1  0x100000   
#define                 MDMA0  0x200000   
#define                 MDMA1  0x400000   
#define                  WDOG  0x800000   
#define              DMA1_ERR  0x1000000  
#define            SPORT2_ERR  0x2000000  
#define            SPORT3_ERR  0x4000000  
#define               MXVR_SD  0x8000000  
#define              SPI1_ERR  0x10000000 
#define              SPI2_ERR  0x20000000 
#define             UART1_ERR  0x40000000 
#define             UART2_ERR  0x80000000 


#define              CAN0_ERR  0x1        
#define                 DMA18  0x2        
#define                 DMA19  0x4        
#define                 DMA20  0x8        
#define                 DMA21  0x10       
#define                 DMA13  0x20       
#define                 DMA14  0x40       
#define                  DMA5  0x80       
#define                 DMA23  0x100      
#define                  DMA8  0x200      
#define                  DMA9  0x400      
#define                 DMA10  0x800      
#define                 DMA11  0x1000     
#define                  TWI0  0x2000     
#define                  TWI1  0x4000     
#define               CAN0_RX  0x8000     
#define               CAN0_TX  0x10000    
#define                 MDMA2  0x20000    
#define                 MDMA3  0x40000    
#define             MXVR_STAT  0x80000    
#define               MXVR_CM  0x100000   
#define               MXVR_AP  0x200000   
#define             EPPI1_ERR  0x400000   
#define             EPPI2_ERR  0x800000   
#define             UART3_ERR  0x1000000  
#define              HOST_ERR  0x2000000  
#define               USB_ERR  0x4000000  
#define              PIXC_ERR  0x8000000  
#define               NFC_ERR  0x10000000 
#define             ATAPI_ERR  0x20000000 
#define              CAN1_ERR  0x40000000 
#define             DMAR0_ERR  0x80000000 
#define             DMAR1_ERR  0x80000000 
#define                 DMAR0  0x80000000 
#define                 DMAR1  0x80000000 


#define                 DMA15  0x1        
#define                 DMA16  0x2        
#define                 DMA17  0x4        
#define                 DMA22  0x8        
#define                   CNT  0x10       
#define                   KEY  0x20       
#define               CAN1_RX  0x40       
#define               CAN1_TX  0x80       
#define             SDH_INT_MASK0  0x100      
#define             SDH_INT_MASK1  0x200      
#define              USB_EINT  0x400      
#define              USB_INT0  0x800      
#define              USB_INT1  0x1000     
#define              USB_INT2  0x2000     
#define            USB_DMAINT  0x4000     
#define                OTPSEC  0x8000     
#define                TIMER0  0x400000   
#define                TIMER1  0x800000   
#define                TIMER2  0x1000000  
#define                TIMER3  0x2000000  
#define                TIMER4  0x4000000  
#define                TIMER5  0x8000000  
#define                TIMER6  0x10000000 
#define                TIMER7  0x20000000 
#define                 PINT2  0x40000000 
#define                 PINT3  0x80000000 


#define                     CTYPE  0x40       
#define                      PMAP  0xf000     


#define        DCB_TRAFFIC_PERIOD  0xf        
#define        DEB_TRAFFIC_PERIOD  0xf0       
#define        DAB_TRAFFIC_PERIOD  0x700      
#define   MDMA_ROUND_ROBIN_PERIOD  0xf800     


#define         DCB_TRAFFIC_COUNT  0xf        
#define         DEB_TRAFFIC_COUNT  0xf0       
#define         DAB_TRAFFIC_COUNT  0x700      
#define    MDMA_ROUND_ROBIN_COUNT  0xf800     


#define                   PMUXSDH  0x1        

#define AMCKEN			0x0001		
#define	AMBEN_NONE		0x0000		
#define AMBEN_B0		0x0002		
#define AMBEN_B0_B1		0x0004		
#define AMBEN_B0_B1_B2	0x0006		
#define AMBEN_ALL		0x0008		



#define                   B0RDYEN  0x1        
#define                  B0RDYPOL  0x2        
#define                      B0TT  0xc        
#define                      B0ST  0x30       
#define                      B0HT  0xc0       
#define                     B0RAT  0xf00      
#define                     B0WAT  0xf000     
#define                   B1RDYEN  0x10000    
#define                  B1RDYPOL  0x20000    
#define                      B1TT  0xc0000    
#define                      B1ST  0x300000   
#define                      B1HT  0xc00000   
#define                     B1RAT  0xf000000  
#define                     B1WAT  0xf0000000 


#define                   B2RDYEN  0x1        
#define                  B2RDYPOL  0x2        
#define                      B2TT  0xc        
#define                      B2ST  0x30       
#define                      B2HT  0xc0       
#define                     B2RAT  0xf00      
#define                     B2WAT  0xf000     
#define                   B3RDYEN  0x10000    
#define                  B3RDYPOL  0x20000    
#define                      B3TT  0xc0000    
#define                      B3ST  0x300000   
#define                      B3HT  0xc00000   
#define                     B3RAT  0xf000000  
#define                     B3WAT  0xf0000000 


#define                  AMSB0CTL  0x3        
#define                  AMSB1CTL  0xc        
#define                  AMSB2CTL  0x30       
#define                  AMSB3CTL  0xc0       


#define                    B0MODE  0x3        
#define                    B1MODE  0xc        
#define                    B2MODE  0x30       
#define                    B3MODE  0xc0       


#define               TESTSETLOCK  0x1        
#define                      BCLK  0x6        
#define                      PGWS  0x38       
#define                      PGSZ  0x40       
#define                      RDDL  0x380      


#define                   ARBSTAT  0x1        
#define                    BGSTAT  0x2        


#define                     TREFI  0x3fff     
#define                      TRFC  0x3c000    
#define                       TRP  0x3c0000   
#define                      TRAS  0x3c00000  
#define                       TRC  0x3c000000 
#define DDR_TRAS(x)		((x<<22)&TRAS)	
#define DDR_TRP(x)		((x<<18)&TRP)	
#define DDR_TRC(x)		((x<<26)&TRC)	
#define DDR_TRFC(x)		((x<<14)&TRFC)	
#define DDR_TREFI(x)		(x&TREFI)	


#define                      TRCD  0xf        
#define                      TMRD  0xf0       
#define                       TWR  0x300      
#define               DDRDATWIDTH  0x3000     
#define                  EXTBANKS  0xc000     
#define               DDRDEVWIDTH  0x30000    
#define                DDRDEVSIZE  0xc0000    
#define                      TWTR  0xf0000000 
#define DDR_TWTR(x)		((x<<28)&TWTR)	
#define DDR_TMRD(x)		((x<<4)&TMRD)	
#define DDR_TWR(x)		((x<<8)&TWR)	
#define DDR_TRCD(x)		(x&TRCD)	
#define DDR_DATWIDTH		0x2000		
#define EXTBANK_1		0		
#define EXTBANK_2		0x4000		
#define DEVSZ_64		0x40000		
#define DEVSZ_128		0x80000		
#define DEVSZ_256		0xc0000		
#define DEVSZ_512		0		
#define DEVWD_4			0		
#define DEVWD_8			0x10000		
#define DEVWD_16		0x20000		


#define               BURSTLENGTH  0x7        
#define                CASLATENCY  0x70       
#define                  DLLRESET  0x100      
#define                      REGE  0x1000     
#define CL_1_5			0x50		
#define CL_2			0x20		
#define CL_2_5			0x60		
#define CL_3			0x30		


#define                      PASR  0x7        


#define                DEB1_PFLEN  0x3        
#define                DEB2_PFLEN  0xc        
#define                DEB3_PFLEN  0x30       
#define          DEB_ARB_PRIORITY  0x700      
#define               DEB1_URGENT  0x1000     
#define               DEB2_URGENT  0x2000     
#define               DEB3_URGENT  0x4000     


#define                DEB1_ERROR  0x1        
#define                DEB2_ERROR  0x2        
#define                DEB3_ERROR  0x4        
#define                CORE_ERROR  0x8        
#define                DEB_MERROR  0x10       
#define               DEB2_MERROR  0x20       
#define               DEB3_MERROR  0x40       
#define               CORE_MERROR  0x80       


#define                 DDRSRESET  0x1        
#define               PFTCHSRESET  0x4        
#define                     SRREQ  0x8        
#define                     SRACK  0x10       
#define                MDDRENABLE  0x20       


#define                B0WCENABLE  0x1        
#define                B1WCENABLE  0x2        
#define                B2WCENABLE  0x4        
#define                B3WCENABLE  0x8        
#define                B4WCENABLE  0x10       
#define                B5WCENABLE  0x20       
#define                B6WCENABLE  0x40       
#define                B7WCENABLE  0x80       
#define                B0RCENABLE  0x100      
#define                B1RCENABLE  0x200      
#define                B2RCENABLE  0x400      
#define                B3RCENABLE  0x800      
#define                B4RCENABLE  0x1000     
#define                B5RCENABLE  0x2000     
#define                B6RCENABLE  0x4000     
#define                B7RCENABLE  0x8000     
#define             ROWACTCENABLE  0x10000    
#define                RWTCENABLE  0x20000    
#define                 ARCENABLE  0x40000    
#define                 GC0ENABLE  0x100000   
#define                 GC1ENABLE  0x200000   
#define                 GC2ENABLE  0x400000   
#define                 GC3ENABLE  0x800000   
#define                 GCCONTROL  0x3000000  


#define                 CB0WCOUNT  0x1        
#define                 CB1WCOUNT  0x2        
#define                 CB2WCOUNT  0x4        
#define                 CB3WCOUNT  0x8        
#define                 CB4WCOUNT  0x10       
#define                 CB5WCOUNT  0x20       
#define                 CB6WCOUNT  0x40       
#define                 CB7WCOUNT  0x80       
#define                  CBRCOUNT  0x100      
#define                 CB1RCOUNT  0x200      
#define                 CB2RCOUNT  0x400      
#define                 CB3RCOUNT  0x800      
#define                 CB4RCOUNT  0x1000     
#define                 CB5RCOUNT  0x2000     
#define                 CB6RCOUNT  0x4000     
#define                 CB7RCOUNT  0x8000     
#define                  CRACOUNT  0x10000    
#define                CRWTACOUNT  0x20000    
#define                  CARCOUNT  0x40000    
#define                  CG0COUNT  0x100000   
#define                  CG1COUNT  0x200000   
#define                  CG2COUNT  0x400000   
#define                  CG3COUNT  0x800000   


#define                       Px0  0x1        
#define                       Px1  0x2        
#define                       Px2  0x4        
#define                       Px3  0x8        
#define                       Px4  0x10       
#define                       Px5  0x20       
#define                       Px6  0x40       
#define                       Px7  0x80       
#define                       Px8  0x100      
#define                       Px9  0x200      
#define                      Px10  0x400      
#define                      Px11  0x800      
#define                      Px12  0x1000     
#define                      Px13  0x2000     
#define                      Px14  0x4000     
#define                      Px15  0x8000     


#define                      PxM0  0x3        
#define                      PxM1  0xc        
#define                      PxM2  0x30       
#define                      PxM3  0xc0       
#define                      PxM4  0x300      
#define                      PxM5  0xc00      
#define                      PxM6  0x3000     
#define                      PxM7  0xc000     
#define                      PxM8  0x30000    
#define                      PxM9  0xc0000    
#define                     PxM10  0x300000   
#define                     PxM11  0xc00000   
#define                     PxM12  0x3000000  
#define                     PxM13  0xc000000  
#define                     PxM14  0x30000000 
#define                     PxM15  0xc0000000 



#define                       IB0  0x1        
#define                       IB1  0x2        
#define                       IB2  0x4        
#define                       IB3  0x8        
#define                       IB4  0x10       
#define                       IB5  0x20       
#define                       IB6  0x40       
#define                       IB7  0x80       
#define                       IB8  0x100      
#define                       IB9  0x200      
#define                      IB10  0x400      
#define                      IB11  0x800      
#define                      IB12  0x1000     
#define                      IB13  0x2000     
#define                      IB14  0x4000     
#define                      IB15  0x8000     


#define                     TMODE  0x3        
#define                  PULSE_HI  0x4        
#define                PERIOD_CNT  0x8        
#define                   IRQ_ENA  0x10       
#define                   TIN_SEL  0x20       
#define                   OUT_DIS  0x40       
#define                   CLK_SEL  0x80       
#define                 TOGGLE_HI  0x100      
#define                   EMU_RUN  0x200      
#define                   ERR_TYP  0xc000     


#define                    TIMEN0  0x1        
#define                    TIMEN1  0x2        
#define                    TIMEN2  0x4        
#define                    TIMEN3  0x8        
#define                    TIMEN4  0x10       
#define                    TIMEN5  0x20       
#define                    TIMEN6  0x40       
#define                    TIMEN7  0x80       


#define                   TIMDIS0  0x1        
#define                   TIMDIS1  0x2        
#define                   TIMDIS2  0x4        
#define                   TIMDIS3  0x8        
#define                   TIMDIS4  0x10       
#define                   TIMDIS5  0x20       
#define                   TIMDIS6  0x40       
#define                   TIMDIS7  0x80       


#define                    TIMIL0  0x1        
#define                    TIMIL1  0x2        
#define                    TIMIL2  0x4        
#define                    TIMIL3  0x8        
#define                 TOVF_ERR0  0x10       
#define                 TOVF_ERR1  0x20       
#define                 TOVF_ERR2  0x40       
#define                 TOVF_ERR3  0x80       
#define                     TRUN0  0x1000     
#define                     TRUN1  0x2000     
#define                     TRUN2  0x4000     
#define                     TRUN3  0x8000     
#define                    TIMIL4  0x10000    
#define                    TIMIL5  0x20000    
#define                    TIMIL6  0x40000    
#define                    TIMIL7  0x80000    
#define                 TOVF_ERR4  0x100000   
#define                 TOVF_ERR5  0x200000   
#define                 TOVF_ERR6  0x400000   
#define                 TOVF_ERR7  0x800000   
#define                     TRUN4  0x10000000 
#define                     TRUN5  0x20000000 
#define                     TRUN6  0x40000000 
#define                     TRUN7  0x80000000 


#define                   EMUDABL  0x1        
#define                   RSTDABL  0x2        
#define                   L1IDABL  0x1c       
#define                  L1DADABL  0xe0       
#define                  L1DBDABL  0x700      
#define                   DMA0OVR  0x800      
#define                   DMA1OVR  0x1000     
#define                    EMUOVR  0x4000     
#define                    OTPSEN  0x8000     
#define                    L2DABL  0x70000    


#define                   SECURE0  0x1        
#define                   SECURE1  0x2        
#define                   SECURE2  0x4        
#define                   SECURE3  0x8        


#define                   SECMODE  0x3        
#define                       NMI  0x4        
#define                   AFVALID  0x8        
#define                    AFEXIT  0x10       
#define                   SECSTAT  0xe0       

#define              SYSTEM_RESET 0x0007       
#define              DOUBLE_FAULT 0x0008       
#define              RESET_DOUBLE 0x2000       
#define                RESET_WDOG 0x4000       
#define            RESET_SOFTWARE 0x8000       


#define                 CFIFO_ERR  0x1        
#define                 YFIFO_ERR  0x2        
#define                 LTERR_OVR  0x4        
#define                LTERR_UNDR  0x8        
#define                 FTERR_OVR  0x10       
#define                FTERR_UNDR  0x20       
#define                  ERR_NCOR  0x40       
#define                   DMA1URQ  0x80       
#define                   DMA0URQ  0x100      
#define                   ERR_DET  0x4000     
#define                       FLD  0x8000     


#define                   EPPI_EN  0x1        
#define                  EPPI_DIR  0x2        
#define                  XFR_TYPE  0xc        
#define                    FS_CFG  0x30       
#define                   FLD_SEL  0x40       
#define                  ITU_TYPE  0x80       
#define                  BLANKGEN  0x100      
#define                   ICLKGEN  0x200      
#define                    IFSGEN  0x400      
#define                      POLC  0x1800     
#define                      POLS  0x6000     
#define                   DLENGTH  0x38000    
#define                   SKIP_EN  0x40000    
#define                   SKIP_EO  0x80000    
#define                    PACKEN  0x100000   
#define                    SWAPEN  0x200000   
#define                  SIGN_EXT  0x400000   
#define             SPLT_EVEN_ODD  0x800000   
#define               SUBSPLT_ODD  0x1000000  
#define                    DMACFG  0x2000000  
#define                RGB_FMT_EN  0x4000000  
#define                  FIFO_RWM  0x18000000 
#define                  FIFO_UWM  0x60000000 

#define DLEN_8		(0 << 15) 
#define DLEN_10		(1 << 15) 
#define DLEN_12		(2 << 15) 
#define DLEN_14		(3 << 15) 
#define DLEN_16		(4 << 15) 
#define DLEN_18		(5 << 15) 
#define DLEN_24		(6 << 15) 



#define                   F1VB_BD  0xff       
#define                   F1VB_AD  0xff00     
#define                   F2VB_BD  0xff0000   
#define                   F2VB_AD  0xff000000 


#define                    F1_ACT  0xffff     
#define                    F2_ACT  0xffff0000 


#define                   LOW_ODD  0xff       
#define                  HIGH_ODD  0xff00     
#define                  LOW_EVEN  0xff0000   
#define                 HIGH_EVEN  0xff000000 



#define                  PRESCALE  0x7f       
#define                   TWI_ENA  0x80       
#define                      SCCB  0x200      


#define                    CLKLOW  0xff       
#define                     CLKHI  0xff00     


#define                       SEN  0x1        
#define                    STDVAL  0x4        
#define                       NAK  0x8        
#define                       GEN  0x10       


#define                     SADDR  0x7f       


#define                      SDIR  0x1        
#define                     GCALL  0x2        


#define                       MEN  0x1        
#define                      MDIR  0x4        
#define                      FAST  0x8        
#define                      STOP  0x10       
#define                    RSTART  0x20       
#define                      DCNT  0x3fc0     
#define                    SDAOVR  0x4000     
#define                    SCLOVR  0x8000     


#define                     MADDR  0x7f       


#define                     MPROG  0x1        
#define                   LOSTARB  0x2        
#define                      ANAK  0x4        
#define                      DNAK  0x8        
#define                  BUFRDERR  0x10       
#define                  BUFWRERR  0x20       
#define                    SDASEN  0x40       
#define                    SCLSEN  0x80       
#define                   BUSBUSY  0x100      


#define                  XMTFLUSH  0x1        
#define                  RCVFLUSH  0x2        
#define                 XMTINTLEN  0x4        
#define                 RCVINTLEN  0x8        


#define                   XMTSTAT  0x3        
#define                   RCVSTAT  0xc        


#define                    SINITM  0x1        
#define                    SCOMPM  0x2        
#define                     SERRM  0x4        
#define                     SOVFM  0x8        
#define                    MCOMPM  0x10       
#define                     MERRM  0x20       
#define                  XMTSERVM  0x40       
#define                  RCVSERVM  0x80       


#define                     SINIT  0x1        
#define                     SCOMP  0x2        
#define                      SERR  0x4        
#define                      SOVF  0x8        
#define                     MCOMP  0x10       
#define                      MERR  0x20       
#define                   XMTSERV  0x40       
#define                   RCVSERV  0x80       


#define                  XMTDATA8  0xff       


#define                 XMTDATA16  0xffff     


#define                  RCVDATA8  0xff       


#define                 RCVDATA16  0xffff     



#define BCODE_WAKEUP    0x0000  
#define BCODE_FULLBOOT  0x0010  
#define BCODE_QUICKBOOT 0x0020  
#define BCODE_NOBOOT    0x0030  


#define PWM_OUT  0x0001
#define WDTH_CAP 0x0002
#define EXT_CLK  0x0003


#define PIQ0 0x00000001
#define PIQ1 0x00000002
#define PIQ2 0x00000004
#define PIQ3 0x00000008

#define PIQ4 0x00000010
#define PIQ5 0x00000020
#define PIQ6 0x00000040
#define PIQ7 0x00000080

#define PIQ8 0x00000100
#define PIQ9 0x00000200
#define PIQ10 0x00000400
#define PIQ11 0x00000800

#define PIQ12 0x00001000
#define PIQ13 0x00002000
#define PIQ14 0x00004000
#define PIQ15 0x00008000

#define PIQ16 0x00010000
#define PIQ17 0x00020000
#define PIQ18 0x00040000
#define PIQ19 0x00080000

#define PIQ20 0x00100000
#define PIQ21 0x00200000
#define PIQ22 0x00400000
#define PIQ23 0x00800000

#define PIQ24 0x01000000
#define PIQ25 0x02000000
#define PIQ26 0x04000000
#define PIQ27 0x08000000

#define PIQ28 0x10000000
#define PIQ29 0x20000000
#define PIQ30 0x40000000
#define PIQ31 0x80000000


#define MUX0 0x00000003
#define MUX0_0 0x00000000
#define MUX0_1 0x00000001
#define MUX0_2 0x00000002
#define MUX0_3 0x00000003

#define MUX1 0x0000000C
#define MUX1_0 0x00000000
#define MUX1_1 0x00000004
#define MUX1_2 0x00000008
#define MUX1_3 0x0000000C

#define MUX2 0x00000030
#define MUX2_0 0x00000000
#define MUX2_1 0x00000010
#define MUX2_2 0x00000020
#define MUX2_3 0x00000030

#define MUX3 0x000000C0
#define MUX3_0 0x00000000
#define MUX3_1 0x00000040
#define MUX3_2 0x00000080
#define MUX3_3 0x000000C0

#define MUX4 0x00000300
#define MUX4_0 0x00000000
#define MUX4_1 0x00000100
#define MUX4_2 0x00000200
#define MUX4_3 0x00000300

#define MUX5 0x00000C00
#define MUX5_0 0x00000000
#define MUX5_1 0x00000400
#define MUX5_2 0x00000800
#define MUX5_3 0x00000C00

#define MUX6 0x00003000
#define MUX6_0 0x00000000
#define MUX6_1 0x00001000
#define MUX6_2 0x00002000
#define MUX6_3 0x00003000

#define MUX7 0x0000C000
#define MUX7_0 0x00000000
#define MUX7_1 0x00004000
#define MUX7_2 0x00008000
#define MUX7_3 0x0000C000

#define MUX8 0x00030000
#define MUX8_0 0x00000000
#define MUX8_1 0x00010000
#define MUX8_2 0x00020000
#define MUX8_3 0x00030000

#define MUX9 0x000C0000
#define MUX9_0 0x00000000
#define MUX9_1 0x00040000
#define MUX9_2 0x00080000
#define MUX9_3 0x000C0000

#define MUX10 0x00300000
#define MUX10_0 0x00000000
#define MUX10_1 0x00100000
#define MUX10_2 0x00200000
#define MUX10_3 0x00300000

#define MUX11 0x00C00000
#define MUX11_0 0x00000000
#define MUX11_1 0x00400000
#define MUX11_2 0x00800000
#define MUX11_3 0x00C00000

#define MUX12 0x03000000
#define MUX12_0 0x00000000
#define MUX12_1 0x01000000
#define MUX12_2 0x02000000
#define MUX12_3 0x03000000

#define MUX13 0x0C000000
#define MUX13_0 0x00000000
#define MUX13_1 0x04000000
#define MUX13_2 0x08000000
#define MUX13_3 0x0C000000

#define MUX14 0x30000000
#define MUX14_0 0x00000000
#define MUX14_1 0x10000000
#define MUX14_2 0x20000000
#define MUX14_3 0x30000000

#define MUX15 0xC0000000
#define MUX15_0 0x00000000
#define MUX15_1 0x40000000
#define MUX15_2 0x80000000
#define MUX15_3 0xC0000000

#define MUX(b15,b14,b13,b12,b11,b10,b9,b8,b7,b6,b5,b4,b3,b2,b1,b0) \
    ((((b15)&3) << 30) | \
     (((b14)&3) << 28) | \
     (((b13)&3) << 26) | \
     (((b12)&3) << 24) | \
     (((b11)&3) << 22) | \
     (((b10)&3) << 20) | \
     (((b9) &3) << 18) | \
     (((b8) &3) << 16) | \
     (((b7) &3) << 14) | \
     (((b6) &3) << 12) | \
     (((b5) &3) << 10) | \
     (((b4) &3) << 8)  | \
     (((b3) &3) << 6)  | \
     (((b2) &3) << 4)  | \
     (((b1) &3) << 2)  | \
     (((b0) &3)))


#define B0MAP 0x000000FF     
#define B0MAP_PAL 0x00000000 
#define B0MAP_PBL 0x00000001 
#define B1MAP 0x0000FF00     
#define B1MAP_PAH 0x00000000 
#define B1MAP_PBH 0x00000100 
#define B2MAP 0x00FF0000     
#define B2MAP_PAL 0x00000000 
#define B2MAP_PBL 0x00010000 
#define B3MAP 0xFF000000     
#define B3MAP_PAH 0x00000000 
#define B3MAP_PBH 0x01000000 


#define B0MAP_PCL 0x00000000 
#define B0MAP_PDL 0x00000001 
#define B0MAP_PEL 0x00000002 
#define B0MAP_PFL 0x00000003 
#define B0MAP_PGL 0x00000004 
#define B0MAP_PHL 0x00000005 
#define B0MAP_PIL 0x00000006 
#define B0MAP_PJL 0x00000007 

#define B1MAP_PCH 0x00000000 
#define B1MAP_PDH 0x00000100 
#define B1MAP_PEH 0x00000200 
#define B1MAP_PFH 0x00000300 
#define B1MAP_PGH 0x00000400 
#define B1MAP_PHH 0x00000500 
#define B1MAP_PIH 0x00000600 
#define B1MAP_PJH 0x00000700 

#define B2MAP_PCL 0x00000000 
#define B2MAP_PDL 0x00010000 
#define B2MAP_PEL 0x00020000 
#define B2MAP_PFL 0x00030000 
#define B2MAP_PGL 0x00040000 
#define B2MAP_PHL 0x00050000 
#define B2MAP_PIL 0x00060000 
#define B2MAP_PJL 0x00070000 

#define B3MAP_PCH 0x00000000 
#define B3MAP_PDH 0x01000000 
#define B3MAP_PEH 0x02000000 
#define B3MAP_PFH 0x03000000 
#define B3MAP_PGH 0x04000000 
#define B3MAP_PHH 0x05000000 
#define B3MAP_PIH 0x06000000 
#define B3MAP_PJH 0x07000000 

#endif 
