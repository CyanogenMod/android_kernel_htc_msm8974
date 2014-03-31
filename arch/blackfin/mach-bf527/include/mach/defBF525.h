/*
 * Copyright 2007-2010 Analog Devices Inc.
 *
 * Licensed under the ADI BSD license or the GPL-2 (or later)
 */

#ifndef _DEF_BF525_H
#define _DEF_BF525_H

#include "defBF522.h"


#define                        USB_FADDR  0xffc03800   
#define                        USB_POWER  0xffc03804   
#define                       USB_INTRTX  0xffc03808   
#define                       USB_INTRRX  0xffc0380c   
#define                      USB_INTRTXE  0xffc03810   
#define                      USB_INTRRXE  0xffc03814   
#define                      USB_INTRUSB  0xffc03818   
#define                     USB_INTRUSBE  0xffc0381c   
#define                        USB_FRAME  0xffc03820   
#define                        USB_INDEX  0xffc03824   
#define                     USB_TESTMODE  0xffc03828   
#define                     USB_GLOBINTR  0xffc0382c   
#define                   USB_GLOBAL_CTL  0xffc03830   


#define                USB_TX_MAX_PACKET  0xffc03840   
#define                         USB_CSR0  0xffc03844   
#define                        USB_TXCSR  0xffc03844   
#define                USB_RX_MAX_PACKET  0xffc03848   
#define                        USB_RXCSR  0xffc0384c   
#define                       USB_COUNT0  0xffc03850   
#define                      USB_RXCOUNT  0xffc03850   
#define                       USB_TXTYPE  0xffc03854   
#define                    USB_NAKLIMIT0  0xffc03858   
#define                   USB_TXINTERVAL  0xffc03858   
#define                       USB_RXTYPE  0xffc0385c   
#define                   USB_RXINTERVAL  0xffc03860   
#define                      USB_TXCOUNT  0xffc03868   /* Number of bytes to be written to the selected endpoint Tx FIFO */


#define                     USB_EP0_FIFO  0xffc03880   
#define                     USB_EP1_FIFO  0xffc03888   
#define                     USB_EP2_FIFO  0xffc03890   
#define                     USB_EP3_FIFO  0xffc03898   
#define                     USB_EP4_FIFO  0xffc038a0   
#define                     USB_EP5_FIFO  0xffc038a8   
#define                     USB_EP6_FIFO  0xffc038b0   
#define                     USB_EP7_FIFO  0xffc038b8   


#define                  USB_OTG_DEV_CTL  0xffc03900   
#define                 USB_OTG_VBUS_IRQ  0xffc03904   
#define                USB_OTG_VBUS_MASK  0xffc03908   


#define                     USB_LINKINFO  0xffc03948   
#define                        USB_VPLEN  0xffc0394c   
#define                      USB_HS_EOF1  0xffc03950   
#define                      USB_FS_EOF1  0xffc03954   
#define                      USB_LS_EOF1  0xffc03958   


#define                   USB_APHY_CNTRL  0xffc039e0   


#define                   USB_APHY_CALIB  0xffc039e4   

#define                  USB_APHY_CNTRL2  0xffc039e8   


#define                     USB_PHY_TEST  0xffc039ec   

#define                  USB_PLLOSC_CTRL  0xffc039f0   
#define                   USB_SRP_CLKDIV  0xffc039f4   


#define                USB_EP_NI0_TXMAXP  0xffc03a00   
#define                 USB_EP_NI0_TXCSR  0xffc03a04   
#define                USB_EP_NI0_RXMAXP  0xffc03a08   
#define                 USB_EP_NI0_RXCSR  0xffc03a0c   
#define               USB_EP_NI0_RXCOUNT  0xffc03a10   
#define                USB_EP_NI0_TXTYPE  0xffc03a14   
#define            USB_EP_NI0_TXINTERVAL  0xffc03a18   
#define                USB_EP_NI0_RXTYPE  0xffc03a1c   
#define            USB_EP_NI0_RXINTERVAL  0xffc03a20   
#define               USB_EP_NI0_TXCOUNT  0xffc03a28   /* Number of bytes to be written to the endpoint0 Tx FIFO */


#define                USB_EP_NI1_TXMAXP  0xffc03a40   
#define                 USB_EP_NI1_TXCSR  0xffc03a44   
#define                USB_EP_NI1_RXMAXP  0xffc03a48   
#define                 USB_EP_NI1_RXCSR  0xffc03a4c   
#define               USB_EP_NI1_RXCOUNT  0xffc03a50   
#define                USB_EP_NI1_TXTYPE  0xffc03a54   
#define            USB_EP_NI1_TXINTERVAL  0xffc03a58   
#define                USB_EP_NI1_RXTYPE  0xffc03a5c   
#define            USB_EP_NI1_RXINTERVAL  0xffc03a60   
#define               USB_EP_NI1_TXCOUNT  0xffc03a68   /* Number of bytes to be written to the+H102 endpoint1 Tx FIFO */


#define                USB_EP_NI2_TXMAXP  0xffc03a80   
#define                 USB_EP_NI2_TXCSR  0xffc03a84   
#define                USB_EP_NI2_RXMAXP  0xffc03a88   
#define                 USB_EP_NI2_RXCSR  0xffc03a8c   
#define               USB_EP_NI2_RXCOUNT  0xffc03a90   
#define                USB_EP_NI2_TXTYPE  0xffc03a94   
#define            USB_EP_NI2_TXINTERVAL  0xffc03a98   
#define                USB_EP_NI2_RXTYPE  0xffc03a9c   
#define            USB_EP_NI2_RXINTERVAL  0xffc03aa0   
#define               USB_EP_NI2_TXCOUNT  0xffc03aa8   /* Number of bytes to be written to the endpoint2 Tx FIFO */


#define                USB_EP_NI3_TXMAXP  0xffc03ac0   
#define                 USB_EP_NI3_TXCSR  0xffc03ac4   
#define                USB_EP_NI3_RXMAXP  0xffc03ac8   
#define                 USB_EP_NI3_RXCSR  0xffc03acc   
#define               USB_EP_NI3_RXCOUNT  0xffc03ad0   
#define                USB_EP_NI3_TXTYPE  0xffc03ad4   
#define            USB_EP_NI3_TXINTERVAL  0xffc03ad8   
#define                USB_EP_NI3_RXTYPE  0xffc03adc   
#define            USB_EP_NI3_RXINTERVAL  0xffc03ae0   
#define               USB_EP_NI3_TXCOUNT  0xffc03ae8   /* Number of bytes to be written to the H124endpoint3 Tx FIFO */


#define                USB_EP_NI4_TXMAXP  0xffc03b00   
#define                 USB_EP_NI4_TXCSR  0xffc03b04   
#define                USB_EP_NI4_RXMAXP  0xffc03b08   
#define                 USB_EP_NI4_RXCSR  0xffc03b0c   
#define               USB_EP_NI4_RXCOUNT  0xffc03b10   
#define                USB_EP_NI4_TXTYPE  0xffc03b14   
#define            USB_EP_NI4_TXINTERVAL  0xffc03b18   
#define                USB_EP_NI4_RXTYPE  0xffc03b1c   
#define            USB_EP_NI4_RXINTERVAL  0xffc03b20   
#define               USB_EP_NI4_TXCOUNT  0xffc03b28   /* Number of bytes to be written to the endpoint4 Tx FIFO */


#define                USB_EP_NI5_TXMAXP  0xffc03b40   
#define                 USB_EP_NI5_TXCSR  0xffc03b44   
#define                USB_EP_NI5_RXMAXP  0xffc03b48   
#define                 USB_EP_NI5_RXCSR  0xffc03b4c   
#define               USB_EP_NI5_RXCOUNT  0xffc03b50   
#define                USB_EP_NI5_TXTYPE  0xffc03b54   
#define            USB_EP_NI5_TXINTERVAL  0xffc03b58   
#define                USB_EP_NI5_RXTYPE  0xffc03b5c   
#define            USB_EP_NI5_RXINTERVAL  0xffc03b60   
#define               USB_EP_NI5_TXCOUNT  0xffc03b68   /* Number of bytes to be written to the endpoint5 Tx FIFO */


#define                USB_EP_NI6_TXMAXP  0xffc03b80   
#define                 USB_EP_NI6_TXCSR  0xffc03b84   
#define                USB_EP_NI6_RXMAXP  0xffc03b88   
#define                 USB_EP_NI6_RXCSR  0xffc03b8c   
#define               USB_EP_NI6_RXCOUNT  0xffc03b90   
#define                USB_EP_NI6_TXTYPE  0xffc03b94   
#define            USB_EP_NI6_TXINTERVAL  0xffc03b98   
#define                USB_EP_NI6_RXTYPE  0xffc03b9c   
#define            USB_EP_NI6_RXINTERVAL  0xffc03ba0   
#define               USB_EP_NI6_TXCOUNT  0xffc03ba8   /* Number of bytes to be written to the endpoint6 Tx FIFO */


#define                USB_EP_NI7_TXMAXP  0xffc03bc0   
#define                 USB_EP_NI7_TXCSR  0xffc03bc4   
#define                USB_EP_NI7_RXMAXP  0xffc03bc8   
#define                 USB_EP_NI7_RXCSR  0xffc03bcc   
#define               USB_EP_NI7_RXCOUNT  0xffc03bd0   
#define                USB_EP_NI7_TXTYPE  0xffc03bd4   
#define            USB_EP_NI7_TXINTERVAL  0xffc03bd8   
#define                USB_EP_NI7_RXTYPE  0xffc03bdc   
#define            USB_EP_NI7_RXINTERVAL  0xffc03be0   
#define               USB_EP_NI7_TXCOUNT  0xffc03be8   /* Number of bytes to be written to the endpoint7 Tx FIFO */

#define                USB_DMA_INTERRUPT  0xffc03c00   


#define                  USB_DMA0CONTROL  0xffc03c04   
#define                  USB_DMA0ADDRLOW  0xffc03c08   
#define                 USB_DMA0ADDRHIGH  0xffc03c0c   
#define                 USB_DMA0COUNTLOW  0xffc03c10   
#define                USB_DMA0COUNTHIGH  0xffc03c14   


#define                  USB_DMA1CONTROL  0xffc03c24   
#define                  USB_DMA1ADDRLOW  0xffc03c28   
#define                 USB_DMA1ADDRHIGH  0xffc03c2c   
#define                 USB_DMA1COUNTLOW  0xffc03c30   
#define                USB_DMA1COUNTHIGH  0xffc03c34   


#define                  USB_DMA2CONTROL  0xffc03c44   
#define                  USB_DMA2ADDRLOW  0xffc03c48   
#define                 USB_DMA2ADDRHIGH  0xffc03c4c   
#define                 USB_DMA2COUNTLOW  0xffc03c50   
#define                USB_DMA2COUNTHIGH  0xffc03c54   


#define                  USB_DMA3CONTROL  0xffc03c64   
#define                  USB_DMA3ADDRLOW  0xffc03c68   
#define                 USB_DMA3ADDRHIGH  0xffc03c6c   
#define                 USB_DMA3COUNTLOW  0xffc03c70   
#define                USB_DMA3COUNTHIGH  0xffc03c74   


#define                  USB_DMA4CONTROL  0xffc03c84   
#define                  USB_DMA4ADDRLOW  0xffc03c88   
#define                 USB_DMA4ADDRHIGH  0xffc03c8c   
#define                 USB_DMA4COUNTLOW  0xffc03c90   
#define                USB_DMA4COUNTHIGH  0xffc03c94   


#define                  USB_DMA5CONTROL  0xffc03ca4   
#define                  USB_DMA5ADDRLOW  0xffc03ca8   
#define                 USB_DMA5ADDRHIGH  0xffc03cac   
#define                 USB_DMA5COUNTLOW  0xffc03cb0   
#define                USB_DMA5COUNTHIGH  0xffc03cb4   


#define                  USB_DMA6CONTROL  0xffc03cc4   
#define                  USB_DMA6ADDRLOW  0xffc03cc8   
#define                 USB_DMA6ADDRHIGH  0xffc03ccc   
#define                 USB_DMA6COUNTLOW  0xffc03cd0   
#define                USB_DMA6COUNTHIGH  0xffc03cd4   


#define                  USB_DMA7CONTROL  0xffc03ce4   
#define                  USB_DMA7ADDRLOW  0xffc03ce8   
#define                 USB_DMA7ADDRHIGH  0xffc03cec   
#define                 USB_DMA7COUNTLOW  0xffc03cf0   
#define                USB_DMA7COUNTHIGH  0xffc03cf4   


#define          FUNCTION_ADDRESS  0x7f       


#define           ENABLE_SUSPENDM  0x1        
#define          nENABLE_SUSPENDM  0x0       
#define              SUSPEND_MODE  0x2        
#define             nSUSPEND_MODE  0x0       
#define               RESUME_MODE  0x4        
#define              nRESUME_MODE  0x0       
#define                     RESET  0x8        
#define                    nRESET  0x0       
#define                   HS_MODE  0x10       
#define                  nHS_MODE  0x0       
#define                 HS_ENABLE  0x20       
#define                nHS_ENABLE  0x0       
#define                 SOFT_CONN  0x40       
#define                nSOFT_CONN  0x0       
#define                ISO_UPDATE  0x80       
#define               nISO_UPDATE  0x0       


#define                    EP0_TX  0x1        
#define                   nEP0_TX  0x0       
#define                    EP1_TX  0x2        
#define                   nEP1_TX  0x0       
#define                    EP2_TX  0x4        
#define                   nEP2_TX  0x0       
#define                    EP3_TX  0x8        
#define                   nEP3_TX  0x0       
#define                    EP4_TX  0x10       
#define                   nEP4_TX  0x0       
#define                    EP5_TX  0x20       
#define                   nEP5_TX  0x0       
#define                    EP6_TX  0x40       
#define                   nEP6_TX  0x0       
#define                    EP7_TX  0x80       
#define                   nEP7_TX  0x0       


#define                    EP1_RX  0x2        
#define                   nEP1_RX  0x0       
#define                    EP2_RX  0x4        
#define                   nEP2_RX  0x0       
#define                    EP3_RX  0x8        
#define                   nEP3_RX  0x0       
#define                    EP4_RX  0x10       
#define                   nEP4_RX  0x0       
#define                    EP5_RX  0x20       
#define                   nEP5_RX  0x0       
#define                    EP6_RX  0x40       
#define                   nEP6_RX  0x0       
#define                    EP7_RX  0x80       
#define                   nEP7_RX  0x0       


#define                  EP0_TX_E  0x1        
#define                 nEP0_TX_E  0x0       
#define                  EP1_TX_E  0x2        
#define                 nEP1_TX_E  0x0       
#define                  EP2_TX_E  0x4        
#define                 nEP2_TX_E  0x0       
#define                  EP3_TX_E  0x8        
#define                 nEP3_TX_E  0x0       
#define                  EP4_TX_E  0x10       
#define                 nEP4_TX_E  0x0       
#define                  EP5_TX_E  0x20       
#define                 nEP5_TX_E  0x0       
#define                  EP6_TX_E  0x40       
#define                 nEP6_TX_E  0x0       
#define                  EP7_TX_E  0x80       
#define                 nEP7_TX_E  0x0       


#define                  EP1_RX_E  0x2        
#define                 nEP1_RX_E  0x0       
#define                  EP2_RX_E  0x4        
#define                 nEP2_RX_E  0x0       
#define                  EP3_RX_E  0x8        
#define                 nEP3_RX_E  0x0       
#define                  EP4_RX_E  0x10       
#define                 nEP4_RX_E  0x0       
#define                  EP5_RX_E  0x20       
#define                 nEP5_RX_E  0x0       
#define                  EP6_RX_E  0x40       
#define                 nEP6_RX_E  0x0       
#define                  EP7_RX_E  0x80       
#define                 nEP7_RX_E  0x0       


#define                 SUSPEND_B  0x1        
#define                nSUSPEND_B  0x0       
#define                  RESUME_B  0x2        
#define                 nRESUME_B  0x0       
#define          RESET_OR_BABLE_B  0x4        
#define         nRESET_OR_BABLE_B  0x0       
#define                     SOF_B  0x8        
#define                    nSOF_B  0x0       
#define                    CONN_B  0x10       
#define                   nCONN_B  0x0       
#define                  DISCON_B  0x20       
#define                 nDISCON_B  0x0       
#define             SESSION_REQ_B  0x40       
#define            nSESSION_REQ_B  0x0       
#define              VBUS_ERROR_B  0x80       
#define             nVBUS_ERROR_B  0x0       


#define                SUSPEND_BE  0x1        
#define               nSUSPEND_BE  0x0       
#define                 RESUME_BE  0x2        
#define                nRESUME_BE  0x0       
#define         RESET_OR_BABLE_BE  0x4        
#define        nRESET_OR_BABLE_BE  0x0       
#define                    SOF_BE  0x8        
#define                   nSOF_BE  0x0       
#define                   CONN_BE  0x10       
#define                  nCONN_BE  0x0       
#define                 DISCON_BE  0x20       
#define                nDISCON_BE  0x0       
#define            SESSION_REQ_BE  0x40       
#define           nSESSION_REQ_BE  0x0       
#define             VBUS_ERROR_BE  0x80       
#define            nVBUS_ERROR_BE  0x0       


#define              FRAME_NUMBER  0x7ff      


#define         SELECTED_ENDPOINT  0xf        


#define                GLOBAL_ENA  0x1        
#define               nGLOBAL_ENA  0x0       
#define                EP1_TX_ENA  0x2        
#define               nEP1_TX_ENA  0x0       
#define                EP2_TX_ENA  0x4        
#define               nEP2_TX_ENA  0x0       
#define                EP3_TX_ENA  0x8        
#define               nEP3_TX_ENA  0x0       
#define                EP4_TX_ENA  0x10       
#define               nEP4_TX_ENA  0x0       
#define                EP5_TX_ENA  0x20       
#define               nEP5_TX_ENA  0x0       
#define                EP6_TX_ENA  0x40       
#define               nEP6_TX_ENA  0x0       
#define                EP7_TX_ENA  0x80       
#define               nEP7_TX_ENA  0x0       
#define                EP1_RX_ENA  0x100      
#define               nEP1_RX_ENA  0x0       
#define                EP2_RX_ENA  0x200      
#define               nEP2_RX_ENA  0x0       
#define                EP3_RX_ENA  0x400      
#define               nEP3_RX_ENA  0x0       
#define                EP4_RX_ENA  0x800      
#define               nEP4_RX_ENA  0x0       
#define                EP5_RX_ENA  0x1000     
#define               nEP5_RX_ENA  0x0       
#define                EP6_RX_ENA  0x2000     
#define               nEP6_RX_ENA  0x0       
#define                EP7_RX_ENA  0x4000     
#define               nEP7_RX_ENA  0x0       


#define                   SESSION  0x1        
#define                  nSESSION  0x0       
#define                  HOST_REQ  0x2        
#define                 nHOST_REQ  0x0       
#define                 HOST_MODE  0x4        
#define                nHOST_MODE  0x0       
#define                     VBUS0  0x8        
#define                    nVBUS0  0x0       
#define                     VBUS1  0x10       
#define                    nVBUS1  0x0       
#define                     LSDEV  0x20       
#define                    nLSDEV  0x0       
#define                     FSDEV  0x40       
#define                    nFSDEV  0x0       
#define                  B_DEVICE  0x80       
#define                 nB_DEVICE  0x0       


#define             DRIVE_VBUS_ON  0x1        
#define            nDRIVE_VBUS_ON  0x0       
#define            DRIVE_VBUS_OFF  0x2        
#define           nDRIVE_VBUS_OFF  0x0       
#define           CHRG_VBUS_START  0x4        
#define          nCHRG_VBUS_START  0x0       
#define             CHRG_VBUS_END  0x8        
#define            nCHRG_VBUS_END  0x0       
#define        DISCHRG_VBUS_START  0x10       
#define       nDISCHRG_VBUS_START  0x0       
#define          DISCHRG_VBUS_END  0x20       
#define         nDISCHRG_VBUS_END  0x0       


#define         DRIVE_VBUS_ON_ENA  0x1        
#define        nDRIVE_VBUS_ON_ENA  0x0       
#define        DRIVE_VBUS_OFF_ENA  0x2        
#define       nDRIVE_VBUS_OFF_ENA  0x0       
#define       CHRG_VBUS_START_ENA  0x4        
#define      nCHRG_VBUS_START_ENA  0x0       
#define         CHRG_VBUS_END_ENA  0x8        
#define        nCHRG_VBUS_END_ENA  0x0       
#define    DISCHRG_VBUS_START_ENA  0x10       
#define   nDISCHRG_VBUS_START_ENA  0x0       
#define      DISCHRG_VBUS_END_ENA  0x20       
#define     nDISCHRG_VBUS_END_ENA  0x0       


#define                  RXPKTRDY  0x1        
#define                 nRXPKTRDY  0x0       
#define                  TXPKTRDY  0x2        
#define                 nTXPKTRDY  0x0       
#define                STALL_SENT  0x4        
#define               nSTALL_SENT  0x0       
#define                   DATAEND  0x8        
#define                  nDATAEND  0x0       
#define                  SETUPEND  0x10       
#define                 nSETUPEND  0x0       
#define                 SENDSTALL  0x20       
#define                nSENDSTALL  0x0       
#define         SERVICED_RXPKTRDY  0x40       
#define        nSERVICED_RXPKTRDY  0x0       
#define         SERVICED_SETUPEND  0x80       
#define        nSERVICED_SETUPEND  0x0       
#define                 FLUSHFIFO  0x100      
#define                nFLUSHFIFO  0x0       
#define          STALL_RECEIVED_H  0x4        
#define         nSTALL_RECEIVED_H  0x0       
#define                SETUPPKT_H  0x8        
#define               nSETUPPKT_H  0x0       
#define                   ERROR_H  0x10       
#define                  nERROR_H  0x0       
#define                  REQPKT_H  0x20       
#define                 nREQPKT_H  0x0       
#define               STATUSPKT_H  0x40       
#define              nSTATUSPKT_H  0x0       
#define             NAK_TIMEOUT_H  0x80       
#define            nNAK_TIMEOUT_H  0x0       


#define              EP0_RX_COUNT  0x7f       


#define             EP0_NAK_LIMIT  0x1f       


#define         MAX_PACKET_SIZE_T  0x7ff      


#define         MAX_PACKET_SIZE_R  0x7ff      


#define                TXPKTRDY_T  0x1        
#define               nTXPKTRDY_T  0x0       
#define          FIFO_NOT_EMPTY_T  0x2        
#define         nFIFO_NOT_EMPTY_T  0x0       
#define                UNDERRUN_T  0x4        
#define               nUNDERRUN_T  0x0       
#define               FLUSHFIFO_T  0x8        
#define              nFLUSHFIFO_T  0x0       
#define              STALL_SEND_T  0x10       
#define             nSTALL_SEND_T  0x0       
#define              STALL_SENT_T  0x20       
#define             nSTALL_SENT_T  0x0       
#define        CLEAR_DATATOGGLE_T  0x40       
#define       nCLEAR_DATATOGGLE_T  0x0       
#define                INCOMPTX_T  0x80       
#define               nINCOMPTX_T  0x0       
#define              DMAREQMODE_T  0x400      
#define             nDMAREQMODE_T  0x0       
#define        FORCE_DATATOGGLE_T  0x800      
#define       nFORCE_DATATOGGLE_T  0x0       
#define              DMAREQ_ENA_T  0x1000     
#define             nDMAREQ_ENA_T  0x0       
#define                     ISO_T  0x4000     
#define                    nISO_T  0x0       
#define                 AUTOSET_T  0x8000     
#define                nAUTOSET_T  0x0       
#define                  ERROR_TH  0x4        
#define                 nERROR_TH  0x0       
#define         STALL_RECEIVED_TH  0x20       
#define        nSTALL_RECEIVED_TH  0x0       
#define            NAK_TIMEOUT_TH  0x80       
#define           nNAK_TIMEOUT_TH  0x0       


#define                  TX_COUNT  0x1fff     /* Number of bytes to be written to the selected endpoint Tx FIFO */


#define                RXPKTRDY_R  0x1        
#define               nRXPKTRDY_R  0x0       
#define               FIFO_FULL_R  0x2        
#define              nFIFO_FULL_R  0x0       
#define                 OVERRUN_R  0x4        
#define                nOVERRUN_R  0x0       
#define               DATAERROR_R  0x8        
#define              nDATAERROR_R  0x0       
#define               FLUSHFIFO_R  0x10       
#define              nFLUSHFIFO_R  0x0       
#define              STALL_SEND_R  0x20       
#define             nSTALL_SEND_R  0x0       
#define              STALL_SENT_R  0x40       
#define             nSTALL_SENT_R  0x0       
#define        CLEAR_DATATOGGLE_R  0x80       
#define       nCLEAR_DATATOGGLE_R  0x0       
#define                INCOMPRX_R  0x100      
#define               nINCOMPRX_R  0x0       
#define              DMAREQMODE_R  0x800      
#define             nDMAREQMODE_R  0x0       
#define                 DISNYET_R  0x1000     
#define                nDISNYET_R  0x0       
#define              DMAREQ_ENA_R  0x2000     
#define             nDMAREQ_ENA_R  0x0       
#define                     ISO_R  0x4000     
#define                    nISO_R  0x0       
#define               AUTOCLEAR_R  0x8000     
#define              nAUTOCLEAR_R  0x0       
#define                  ERROR_RH  0x4        
#define                 nERROR_RH  0x0       
#define                 REQPKT_RH  0x20       
#define                nREQPKT_RH  0x0       
#define         STALL_RECEIVED_RH  0x40       
#define        nSTALL_RECEIVED_RH  0x0       
#define               INCOMPRX_RH  0x100      
#define              nINCOMPRX_RH  0x0       
#define             DMAREQMODE_RH  0x800      
#define            nDMAREQMODE_RH  0x0       
#define                AUTOREQ_RH  0x4000     
#define               nAUTOREQ_RH  0x0       


#define                  RX_COUNT  0x1fff     


#define            TARGET_EP_NO_T  0xf        
#define                PROTOCOL_T  0xc        


#define          TX_POLL_INTERVAL  0xff       


#define            TARGET_EP_NO_R  0xf        
#define                PROTOCOL_R  0xc        


#define          RX_POLL_INTERVAL  0xff       


#define                  DMA0_INT  0x1        
#define                 nDMA0_INT  0x0       
#define                  DMA1_INT  0x2        
#define                 nDMA1_INT  0x0       
#define                  DMA2_INT  0x4        
#define                 nDMA2_INT  0x0       
#define                  DMA3_INT  0x8        
#define                 nDMA3_INT  0x0       
#define                  DMA4_INT  0x10       
#define                 nDMA4_INT  0x0       
#define                  DMA5_INT  0x20       
#define                 nDMA5_INT  0x0       
#define                  DMA6_INT  0x40       
#define                 nDMA6_INT  0x0       
#define                  DMA7_INT  0x80       
#define                 nDMA7_INT  0x0       


#define                   DMA_ENA  0x1        
#define                  nDMA_ENA  0x0       
#define                 DIRECTION  0x2        
#define                nDIRECTION  0x0       
#define                      MODE  0x4        
#define                     nMODE  0x0       
#define                   INT_ENA  0x8        
#define                  nINT_ENA  0x0       
#define                     EPNUM  0xf0       
#define                  BUSERROR  0x100      
#define                 nBUSERROR  0x0       


#define             DMA_ADDR_HIGH  0xffff     


#define              DMA_ADDR_LOW  0xffff     


#define            DMA_COUNT_HIGH  0xffff     


#define             DMA_COUNT_LOW  0xffff     

#endif 
