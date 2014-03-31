/*
 * Racal-Interlan ni5010 Ethernet definitions
 *
 * This is an extension to the Linux operating system, and is covered by the
 * same GNU General Public License that covers that work.
 *
 * copyrights (c) 1996 by Jan-Pascal van Best (jvbest@wi.leidenuniv.nl)
 *
 * I have done a look in the following sources:
 *   crynwr-packet-driver by Russ Nelson
 */

#define NI5010_BUFSIZE	2048	

#define NI5010_MAGICVAL0 0x00  
#define NI5010_MAGICVAL1 0x55
#define NI5010_MAGICVAL2 0xAA

#define SA_ADDR0 0x02
#define SA_ADDR1 0x07
#define SA_ADDR2 0x01

#define NI5010_IO_EXTENT       32

#define PRINTK(x) if (NI5010_DEBUG) printk x
#define PRINTK2(x) if (NI5010_DEBUG>=2) printk x
#define PRINTK3(x) if (NI5010_DEBUG>=3) printk x

#define EDLC_XSTAT	(ioaddr + 0x00)	
#define EDLC_XCLR	(ioaddr + 0x00)	
#define EDLC_XMASK	(ioaddr + 0x01)	
#define EDLC_RSTAT	(ioaddr + 0x02)	
#define EDLC_RCLR	(ioaddr + 0x02)	
#define EDLC_RMASK	(ioaddr + 0x03)	
#define EDLC_XMODE	(ioaddr + 0x04)	
#define EDLC_RMODE	(ioaddr + 0x05)	
#define EDLC_RESET	(ioaddr + 0x06)	
#define EDLC_TDR1	(ioaddr + 0x07)	
#define EDLC_ADDR	(ioaddr + 0x08)	
	 			
#define EDLC_TDR2	(ioaddr + 0x0f)	
#define IE_GP		(ioaddr + 0x10)	
				
#define IE_RCNT		(ioaddr + 0x10)	
 				
#define IE_MMODE	(ioaddr + 0x12)	
#define IE_DMA_RST	(ioaddr + 0x13)	
#define IE_ISTAT	(ioaddr + 0x13)	
#define IE_RBUF		(ioaddr + 0x14)	
#define IE_XBUF		(ioaddr + 0x15)	
#define IE_SAPROM	(ioaddr + 0x16)	
#define IE_RESET	(ioaddr + 0x17)	

#define XS_TPOK		0x80	
#define XS_CS		0x40	
#define XS_RCVD		0x20	
#define XS_SHORT	0x10	
#define XS_UFLW		0x08	
#define XS_COLL		0x04	
#define XS_16COLL	0x02	
#define XS_PERR		0x01	

#define XS_CLR_UFLW	0x08	
#define XS_CLR_COLL	0x04	
#define XS_CLR_16COLL	0x02	
#define XS_CLR_PERR	0x01	

#define XM_TPOK		0x80	
#define XM_RCVD		0x20	
#define XM_UFLW		0x08	
#define XM_COLL		0x04	
#define XM_COLL16	0x02	
#define XM_PERR		0x01	
 				
#define XM_ALL		(XM_TPOK | XM_RCVD | XM_UFLW | XM_COLL | XM_COLL16)

#define RS_PKT_OK	0x80	
#define RS_RST_PKT	0x10	
#define RS_RUNT		0x08	
#define RS_ALIGN	0x04	
#define RS_CRC_ERR	0x02	
#define RS_OFLW		0x01	
#define RS_VALID_BITS	( RS_PKT_OK | RS_RST_PKT | RS_RUNT | RS_ALIGN | RS_CRC_ERR | RS_OFLW )
 				

#define RS_CLR_PKT_OK	0x80	
#define RS_CLR_RST_PKT	0x10	
#define RS_CLR_RUNT	0x08	
#define RS_CLR_ALIGN	0x04	
#define RS_CLR_CRC_ERR	0x02	
#define RS_CLR_OFLW	0x01	

#define RM_PKT_OK	0x80	
#define RM_RST_PKT	0x10	
#define RM_RUNT		0x08	
#define RM_ALIGN	0x04	
#define RM_CRC_ERR	0x02	
#define RM_OFLW		0x01	

#define RMD_TEST	0x80	
#define RMD_ADD_SIZ	0x10	
#define RMD_EN_RUNT	0x08	
#define RMD_EN_RST	0x04	

#define RMD_PROMISC	0x03	
#define RMD_MULTICAST	0x02	
#define RMD_BROADCAST	0x01	
#define RMD_NO_PACKETS	0x00	

#define XMD_COLL_CNT	0xf0	
#define XMD_IG_PAR	0x08	
#define XMD_T_MODE	0x04	
#define XMD_LBC		0x02	
#define XMD_DIS_C	0x01	

#define RS_RESET	0x80	

#define MM_EN_DMA	0x80	
#define MM_EN_RCV	0x40	
#define MM_EN_XMT	0x20	
#define MM_BUS_PAGE	0x18	
#define MM_NET_PAGE	0x06	
#define MM_MUX		0x01	
				

#define IS_TDIAG	0x80	
#define IS_EN_RCV	0x20	
#define IS_EN_XMT	0x10	
#define IS_EN_DMA	0x08	
#define IS_DMA_INT	0x04	
#define IS_R_INT	0x02	
#define IS_X_INT	0x01	

