/*******************************************************************************
  DWMAC DMA Header file.

  Copyright (C) 2007-2009  STMicroelectronics Ltd

  This program is free software; you can redistribute it and/or modify it
  under the terms and conditions of the GNU General Public License,
  version 2, as published by the Free Software Foundation.

  This program is distributed in the hope it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
  more details.

  You should have received a copy of the GNU General Public License along with
  this program; if not, write to the Free Software Foundation, Inc.,
  51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.

  The full GNU General Public License is included in this distribution in
  the file called "COPYING".

  Author: Giuseppe Cavallaro <peppe.cavallaro@st.com>
*******************************************************************************/

#define DMA_BUS_MODE		0x00001000	
#define DMA_XMT_POLL_DEMAND	0x00001004	
#define DMA_RCV_POLL_DEMAND	0x00001008	
#define DMA_RCV_BASE_ADDR	0x0000100c	
#define DMA_TX_BASE_ADDR	0x00001010	
#define DMA_STATUS		0x00001014	
#define DMA_CONTROL		0x00001018	
#define DMA_INTR_ENA		0x0000101c	
#define DMA_MISSED_FRAME_CTR	0x00001020	
#define DMA_CUR_TX_BUF_ADDR	0x00001050	
#define DMA_CUR_RX_BUF_ADDR	0x00001054	
#define DMA_HW_FEATURE		0x00001058	

#define DMA_CONTROL_ST		0x00002000	
#define DMA_CONTROL_SR		0x00000002	

#define DMA_INTR_ENA_NIE 0x00010000	
#define DMA_INTR_ENA_TIE 0x00000001	
#define DMA_INTR_ENA_TUE 0x00000004	
#define DMA_INTR_ENA_RIE 0x00000040	
#define DMA_INTR_ENA_ERE 0x00004000	

#define DMA_INTR_NORMAL	(DMA_INTR_ENA_NIE | DMA_INTR_ENA_RIE | \
			DMA_INTR_ENA_TIE)

#define DMA_INTR_ENA_AIE 0x00008000	
#define DMA_INTR_ENA_FBE 0x00002000	
#define DMA_INTR_ENA_ETE 0x00000400	
#define DMA_INTR_ENA_RWE 0x00000200	
#define DMA_INTR_ENA_RSE 0x00000100	
#define DMA_INTR_ENA_RUE 0x00000080	
#define DMA_INTR_ENA_UNE 0x00000020	
#define DMA_INTR_ENA_OVE 0x00000010	
#define DMA_INTR_ENA_TJE 0x00000008	
#define DMA_INTR_ENA_TSE 0x00000002	

#define DMA_INTR_ABNORMAL	(DMA_INTR_ENA_AIE | DMA_INTR_ENA_FBE | \
				DMA_INTR_ENA_UNE)

#define DMA_INTR_DEFAULT_MASK	(DMA_INTR_NORMAL | DMA_INTR_ABNORMAL)

#define DMA_STATUS_GPI		0x10000000	
#define DMA_STATUS_GMI		0x08000000	
#define DMA_STATUS_GLI		0x04000000	
#define DMA_STATUS_GMI		0x08000000
#define DMA_STATUS_GLI		0x04000000
#define DMA_STATUS_EB_MASK	0x00380000	
#define DMA_STATUS_EB_TX_ABORT	0x00080000	
#define DMA_STATUS_EB_RX_ABORT	0x00100000	
#define DMA_STATUS_TS_MASK	0x00700000	
#define DMA_STATUS_TS_SHIFT	20
#define DMA_STATUS_RS_MASK	0x000e0000	
#define DMA_STATUS_RS_SHIFT	17
#define DMA_STATUS_NIS	0x00010000	
#define DMA_STATUS_AIS	0x00008000	
#define DMA_STATUS_ERI	0x00004000	
#define DMA_STATUS_FBI	0x00002000	
#define DMA_STATUS_ETI	0x00000400	
#define DMA_STATUS_RWT	0x00000200	
#define DMA_STATUS_RPS	0x00000100	
#define DMA_STATUS_RU	0x00000080	
#define DMA_STATUS_RI	0x00000040	
#define DMA_STATUS_UNF	0x00000020	
#define DMA_STATUS_OVF	0x00000010	
#define DMA_STATUS_TJT	0x00000008	
#define DMA_STATUS_TU	0x00000004	
#define DMA_STATUS_TPS	0x00000002	
#define DMA_STATUS_TI	0x00000001	
#define DMA_CONTROL_FTF		0x00100000 

extern void dwmac_enable_dma_transmission(void __iomem *ioaddr);
extern void dwmac_enable_dma_irq(void __iomem *ioaddr);
extern void dwmac_disable_dma_irq(void __iomem *ioaddr);
extern void dwmac_dma_start_tx(void __iomem *ioaddr);
extern void dwmac_dma_stop_tx(void __iomem *ioaddr);
extern void dwmac_dma_start_rx(void __iomem *ioaddr);
extern void dwmac_dma_stop_rx(void __iomem *ioaddr);
extern int dwmac_dma_interrupt(void __iomem *ioaddr,
				struct stmmac_extra_stats *x);
