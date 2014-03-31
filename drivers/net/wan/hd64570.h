#ifndef __HD64570_H
#define __HD64570_H




#define LPR    0x00		

#define PABR0  0x02		
#define PABR1  0x03		
#define WCRL   0x04		
#define WCRM   0x05		
#define WCRH   0x06		

#define PCR    0x08		
#define DMER   0x09		


#define ISR0   0x10		
#define ISR1   0x11		
#define ISR2   0x12		

#define IER0   0x14		
#define IER1   0x15		
#define IER2   0x16		

#define ITCR   0x18		
#define IVR    0x1A		
#define IMVR   0x1C		




#define MSCI0_OFFSET 0x20
#define MSCI1_OFFSET 0x40

#define TRBL   0x00		 
#define TRBH   0x01		 
#define ST0    0x02		
#define ST1    0x03		
#define ST2    0x04		
#define ST3    0x05		
#define FST    0x06		
#define IE0    0x08		
#define IE1    0x09		
#define IE2    0x0A		
#define FIE    0x0B		
#define CMD    0x0C		
#define MD0    0x0E		
#define MD1    0x0F		
#define MD2    0x10		
#define CTL    0x11		
#define SA0    0x12		
#define SA1    0x13		
#define IDL    0x14		
#define TMC    0x15		
#define RXS    0x16		
#define TXS    0x17		
#define TRC0   0x18		 
#define TRC1   0x19		 
#define RRC    0x1A		 
#define CST0   0x1C		
#define CST1   0x1D		



#define TIMER0RX_OFFSET 0x60
#define TIMER0TX_OFFSET 0x68
#define TIMER1RX_OFFSET 0x70
#define TIMER1TX_OFFSET 0x78

#define TCNTL  0x00		
#define TCNTH  0x01		
#define TCONRL 0x02		
#define TCONRH 0x03		
#define TCSR   0x04		
#define TEPR   0x05		




#define DMAC0RX_OFFSET 0x80
#define DMAC0TX_OFFSET 0xA0
#define DMAC1RX_OFFSET 0xC0
#define DMAC1TX_OFFSET 0xE0

#define BARL   0x00		
#define BARH   0x01		
#define BARB   0x02		

#define DARL   0x00		
#define DARH   0x01		
#define DARB   0x02		

#define SARL   0x04		
#define SARH   0x05		
#define SARB   0x06		

#define CPB    0x06		

#define CDAL   0x08		
#define CDAH   0x09		
#define EDAL   0x0A		
#define EDAH   0x0B		
#define BFLL   0x0C		
#define BFLH   0x0D		
#define BCRL   0x0E		
#define BCRH   0x0F		
#define DSR    0x10		
#define DSR_RX(node) (DSR + (node ? DMAC1RX_OFFSET : DMAC0RX_OFFSET))
#define DSR_TX(node) (DSR + (node ? DMAC1TX_OFFSET : DMAC0TX_OFFSET))
#define DMR    0x11		
#define DMR_RX(node) (DMR + (node ? DMAC1RX_OFFSET : DMAC0RX_OFFSET))
#define DMR_TX(node) (DMR + (node ? DMAC1TX_OFFSET : DMAC0TX_OFFSET))
#define FCT    0x13		
#define FCT_RX(node) (FCT + (node ? DMAC1RX_OFFSET : DMAC0RX_OFFSET))
#define FCT_TX(node) (FCT + (node ? DMAC1TX_OFFSET : DMAC0TX_OFFSET))
#define DIR    0x14		
#define DIR_RX(node) (DIR + (node ? DMAC1RX_OFFSET : DMAC0RX_OFFSET))
#define DIR_TX(node) (DIR + (node ? DMAC1TX_OFFSET : DMAC0TX_OFFSET))
#define DCR    0x15		
#define DCR_RX(node) (DCR + (node ? DMAC1RX_OFFSET : DMAC0RX_OFFSET))
#define DCR_TX(node) (DCR + (node ? DMAC1TX_OFFSET : DMAC0TX_OFFSET))





typedef struct {
	u16 cp;			
	u32 bp;			
	u16 len;		
	u8 stat;		
	u8 unused;		
}__packed pkt_desc;



#define ST_TX_EOM     0x80	
#define ST_TX_EOT     0x01	

#define ST_RX_EOM     0x80	
#define ST_RX_SHORT   0x40	
#define ST_RX_ABORT   0x20	
#define ST_RX_RESBIT  0x10	
#define ST_RX_OVERRUN 0x08	
#define ST_RX_CRC     0x04	

#define ST_ERROR_MASK 0x7C

#define DIR_EOTE      0x80      
#define DIR_EOME      0x40      
#define DIR_BOFE      0x20      
#define DIR_COFE      0x10      


#define DSR_EOT       0x80      
#define DSR_EOM       0x40      
#define DSR_BOF       0x20      
#define DSR_COF       0x10      
#define DSR_DE        0x02	
#define DSR_DWE       0x01      

#define DMER_DME      0x80	


#define CMD_RESET     0x21	
#define CMD_TX_ENABLE 0x02	
#define CMD_RX_ENABLE 0x12	

#define MD0_HDLC      0x80	
#define MD0_CRC_ENA   0x04	
#define MD0_CRC_CCITT 0x02	
#define MD0_CRC_PR1   0x01	

#define MD0_CRC_NONE  0x00
#define MD0_CRC_16_0  0x04
#define MD0_CRC_16    0x05
#define MD0_CRC_ITU_0 0x06
#define MD0_CRC_ITU   0x07

#define MD2_NRZ	      0x00
#define MD2_NRZI      0x20
#define MD2_MANCHESTER 0x80
#define MD2_FM_MARK   0xA0
#define MD2_FM_SPACE  0xC0
#define MD2_LOOPBACK  0x03      

#define CTL_NORTS     0x01
#define CTL_IDLE      0x10	
#define CTL_UDRNC     0x20	

#define ST0_TXRDY     0x02	
#define ST0_RXRDY     0x01	

#define ST1_UDRN      0x80	
#define ST1_CDCD      0x04	

#define ST3_CTS       0x08	
#define ST3_DCD       0x04	

#define IE0_TXINT     0x80	
#define IE0_RXINTA    0x40	
#define IE1_UDRN      0x80	
#define IE1_CDCD      0x04	

#define DCR_ABORT     0x01	
#define DCR_CLEAR_EOF 0x02	

#define CLK_BRG_MASK  0x0F
#define CLK_LINE_RX   0x00	
#define CLK_LINE_TX   0x00	
#define CLK_BRG_RX    0x40	
#define CLK_BRG_TX    0x40	
#define CLK_RXCLK_TX  0x60	

#endif
