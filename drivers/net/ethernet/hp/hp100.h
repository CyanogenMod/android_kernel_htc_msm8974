/*
 * hp100.h: Hewlett Packard HP10/100VG ANY LAN ethernet driver for Linux.
 *
 * $Id: hp100.h,v 1.51 1997/04/08 14:26:42 floeff Exp floeff $
 *
 * Authors:  Jaroslav Kysela, <perex@pf.jcu.cz>
 *           Siegfried Loeffler <floeff@tunix.mathematik.uni-stuttgart.de>
 *
 * This driver is based on the 'hpfepkt' crynwr packet driver.
 *
 * This source/code is public free; you can distribute it and/or modify
 * it under terms of the GNU General Public License (published by the
 * Free Software Foundation) either version two of this License, or any
 * later version.
 */



#define HP100_PAGE_PERFORMANCE	0x0	
#define HP100_PAGE_MAC_ADDRESS	0x1	
#define HP100_PAGE_HW_MAP	0x2	
#define HP100_PAGE_EEPROM_CTRL	0x3	
#define HP100_PAGE_MAC_CTRL	0x4	
#define HP100_PAGE_MMU_CFG	0x5	
#define HP100_PAGE_ID_MAC_ADDR	0x6	
#define HP100_PAGE_MMU_POINTER	0x7	



#define HP100_REG_HW_ID		0x00	
#define HP100_REG_TRACE		0x00	
#define HP100_REG_PAGING	0x02	
					
#define HP100_REG_OPTION_LSW	0x04	
#define HP100_REG_OPTION_MSW	0x06	


#define HP100_REG_IRQ_STATUS	0x08	
#define HP100_REG_IRQ_MASK	0x0a	
#define HP100_REG_FRAGMENT_LEN	0x0c	
/*       at offset 0x28 and 0x2c, where they can be written as 32bit values. */
#define HP100_REG_OFFSET	0x0e	
#define HP100_REG_DATA32	0x10	
#define HP100_REG_DATA16	0x12	
#define HP100_REG_TX_MEM_FREE	0x14	
#define HP100_REG_TX_PDA_L      0x14	
#define HP100_REG_TX_PDA_H      0x1c	
#define HP100_REG_RX_PKT_CNT	0x18	
#define HP100_REG_TX_PKT_CNT	0x19	
#define HP100_REG_RX_PDL        0x1a	
#define HP100_REG_TX_PDL        0x1b	
#define HP100_REG_RX_PDA        0x18	
					
#define HP100_REG_SL_EARLY      0x1c	
#define HP100_REG_STAT_DROPPED  0x20	
#define HP100_REG_STAT_ERRORED  0x22	
#define HP100_REG_STAT_ABORT    0x23	
#define HP100_REG_RX_RING       0x24	
#define HP100_REG_32_FRAGMENT_LEN 0x28	
#define HP100_REG_32_OFFSET     0x2c	


#define HP100_REG_MAC_ADDR	0x08	
#define HP100_REG_HASH_BYTE0	0x10	


#define HP100_REG_MEM_MAP_LSW	0x08	
#define HP100_REG_MEM_MAP_MSW	0x0a	
#define HP100_REG_IO_MAP	0x0c	
#define HP100_REG_IRQ_CHANNEL	0x0d	
#define HP100_REG_SRAM		0x0e	
#define HP100_REG_BM		0x0f	

#define HP100_REG_MODECTRL1     0x10	
#define HP100_REG_MODECTRL2     0x11	
#define HP100_REG_PCICTRL1      0x12	
#define HP100_REG_PCICTRL2      0x13	
#define HP100_REG_PCIBUSMLAT    0x15	
#define HP100_REG_EARLYTXCFG    0x16	
#define HP100_REG_EARLYRXCFG    0x18	
#define HP100_REG_ISAPNPCFG1    0x1a	
#define HP100_REG_ISAPNPCFG2    0x1b	


#define HP100_REG_EEPROM_CTRL	0x08	
#define HP100_REG_BOOTROM_CTRL  0x0a


#define HP100_REG_10_LAN_CFG_1	0x08	
#define HP100_REG_10_LAN_CFG_2  0x09	
#define HP100_REG_VG_LAN_CFG_1	0x0a	
#define HP100_REG_VG_LAN_CFG_2  0x0b	
#define HP100_REG_MAC_CFG_1	0x0c	
#define HP100_REG_MAC_CFG_2	0x0d	
#define HP100_REG_MAC_CFG_3     0x0e	
#define HP100_REG_MAC_CFG_4     0x0f	
#define HP100_REG_DROPPED	0x10	
#define HP100_REG_CRC		0x12	
#define HP100_REG_ABORT		0x13	
#define HP100_REG_TRAIN_REQUEST 0x14	
#define HP100_REG_TRAIN_ALLOW   0x16	


#define HP100_REG_RX_MEM_STOP	0x0c	
#define HP100_REG_TX_MEM_STOP	0x0e	
#define HP100_REG_PDL_MEM_STOP  0x10	
#define HP100_REG_ECB_MEM_STOP  0x14	


#define HP100_REG_BOARD_ID	0x08	
#define HP100_REG_BOARD_IO_CHCK 0x0c	
#define HP100_REG_SOFT_MODEL	0x0d	
#define HP100_REG_LAN_ADDR	0x10	
#define HP100_REG_LAN_ADDR_CHCK 0x16	


#define HP100_REG_PTR_RXSTART	0x08	
#define HP100_REG_PTR_RXEND	0x0a	
#define HP100_REG_PTR_TXSTART	0x0c	
#define HP100_REG_PTR_TXEND	0x0e	
#define HP100_REG_PTR_RPDLSTART 0x10
#define HP100_REG_PTR_RPDLEND   0x12
#define HP100_REG_PTR_RINGPTRS  0x14
#define HP100_REG_PTR_MEMDEBUG  0x1a


#define HP100_HW_ID_CASCADE     0x4850	

#define HP100_CHIPID_MASK        0xFFF0
#define HP100_CHIPID_SHASTA      0x5350	
					 
#define HP100_CHIPID_RAINIER     0x5360	
					 
#define HP100_CHIPID_LASSEN      0x5370	
					 

#define HP100_DEBUG_EN		0x8000	
#define HP100_RX_HDR		0x4000	
					
#define HP100_MMAP_DIS		0x2000	
					
					
					
#define HP100_EE_EN		0x1000	
#define HP100_BM_WRITE		0x0800	
#define HP100_BM_READ		0x0400	
#define HP100_TRI_INT		0x0200	
#define HP100_MEM_EN		0x0040	
					
					
#define HP100_IO_EN		0x0020	
#define HP100_BOOT_EN		0x0010	
#define HP100_FAKE_INT		0x0008	
#define HP100_INT_EN		0x0004	
#define HP100_HW_RST		0x0002	
					

#define HP100_PRIORITY_TX	0x0080	
#define HP100_EE_LOAD		0x0040	
#define HP100_ADV_NXT_PKT	0x0004	
					
#define HP100_TX_CMD		0x0002	
					

/*
 * Interrupt Status Registers I and II
 * (Page PERFORMANCE, IRQ_STATUS, Offset 0x08-0x09)
 * Note: With old chips, these Registers will clear when 1 is written to them
 *       with new chips this depends on setting of CLR_ISMODE
 */
#define HP100_RX_EARLY_INT      0x2000
#define HP100_RX_PDA_ZERO       0x1000
#define HP100_RX_PDL_FILL_COMPL 0x0800
#define HP100_RX_PACKET		0x0400	
#define HP100_RX_ERROR		0x0200	
#define HP100_TX_PDA_ZERO       0x0020	
#define HP100_TX_SPACE_AVAIL	0x0010	
#define HP100_TX_COMPLETE	0x0008	
#define HP100_MISC_ERROR        0x0004	
#define HP100_TX_ERROR		0x0002	

#define HP100_AUTO_COMPARE	0x80000000	
#define HP100_FREE_SPACE	0x7fffffe0	

#define HP100_ZERO_WAIT_EN	0x80	
#define HP100_IRQ_SCRAMBLE      0x40
#define HP100_BOND_HP           0x20
#define HP100_LEVEL_IRQ		0x10	
					
#define HP100_IRQMASK		0x0F	

#define HP100_RAM_SIZE_MASK	0xe0	
#define HP100_RAM_SIZE_SHIFT	0x05	

#define HP100_BM_BURST_RD       0x01	
					
#define HP100_BM_BURST_WR       0x02	
					
#define HP100_BM_MASTER		0x04	
#define HP100_BM_PAGE_CK        0x08	
					
#define HP100_BM_PCI_8CLK       0x40	


#define HP100_TX_DUALQ          0x10
   
#define HP100_ISR_CLRMODE       0x02	
				       
#define HP100_EE_NOLOAD         0x04	
				       
#define HP100_TX_CNT_FLG        0x08	
#define HP100_PDL_USE3          0x10	
				       
				       
#define HP100_BUSTYPE_MASK      0xe0	

#define HP100_EE_MASK           0x0f	
				       
#define HP100_DIS_CANCEL        0x20	
#define HP100_EN_PDL_WB         0x40	
				       /* written back to system mem */
#define HP100_EN_BUS_FAIL       0x80	
				       

#define HP100_LO_MEM            0x01	
#define HP100_NO_MEM            0x02	
				       
#define HP100_USE_ISA           0x04	
				       
#define HP100_IRQ_HI_MASK       0xf0	
#define HP100_PCI_IRQ_HI_MASK   0x78	

#define HP100_RD_LINE_PDL       0x01	
#define HP100_RD_TX_DATA_MASK   0x06	
#define HP100_MWI               0x08	
#define HP100_ARB_MODE          0x10	
#define HP100_STOP_EN           0x20	
				       
#define HP100_IGNORE_PAR        0x40	
#define HP100_PCI_RESET         0x80	

#define HP100_EN_EARLY_TX       0x8000	
#define HP100_EN_ADAPTIVE       0x4000	
#define HP100_EN_TX_UR_IRQ      0x2000	
#define HP100_EN_LOW_TX         0x1000	
#define HP100_ET_CNT_MASK       0x0fff	

#define HP100_EN_EARLY_RX       0x80	
#define HP100_EN_LOW_RX         0x40	
#define HP100_RX_TRIP_MASK      0x1f	

#define HP100_EEPROM_LOAD	0x0001	
					
					

#define HP100_MAC10_SEL		0xc0	
#define HP100_AUI_SEL		0x20	
#define HP100_LOW_TH		0x10	
#define HP100_LINK_BEAT_DIS	0x08	
#define HP100_LINK_BEAT_ST	0x04	
#define HP100_R_ROL_ST		0x02	
					
#define HP100_AUI_ST		0x01	

#define HP100_SQU_ST		0x01	
					
#define HP100_FULLDUP           0x02	
#define HP100_DOT3_MAC          0x04	

#define HP100_AUTO_SEL_10	0x0	
#define HP100_XCVR_LXT901_10	0x1	
#define HP100_XCVR_7213		0x2	
#define HP100_XCVR_82503	0x3	

#define HP100_FRAME_FORMAT	0x08	
#define HP100_BRIDGE		0x04	
#define HP100_PROM_MODE		0x02	
					
#define HP100_REPEATER		0x01	
					

#define HP100_VG_SEL	        0x80	
#define HP100_LINK_UP_ST	0x40	
#define HP100_LINK_CABLE_ST	0x20	
					
#define HP100_LOAD_ADDR		0x10	
					
					
#define HP100_LINK_CMD		0x08	
					
					
#define HP100_TRN_DONE          0x04	
					
					
#define HP100_LINK_GOOD_ST	0x02	
#define HP100_VG_RESET		0x01	


#define HP100_RX_IDLE		0x80	
#define HP100_TX_IDLE		0x40	
#define HP100_RX_EN		0x20	
#define HP100_TX_EN		0x10	
#define HP100_ACC_ERRORED	0x08	
#define HP100_ACC_MC		0x04	
#define HP100_ACC_BC		0x02	
#define HP100_ACC_PHY		0x01	
#define HP100_MAC1MODEMASK	0xf0	
#define HP100_MAC1MODE1		0x00	
#define HP100_MAC1MODE2		0x00
#define HP100_MAC1MODE3		HP100_MAC1MODE2 | HP100_ACC_BC
#define HP100_MAC1MODE4		HP100_MAC1MODE3 | HP100_ACC_MC
#define HP100_MAC1MODE5		HP100_MAC1MODE4	
#define HP100_MAC1MODE6		HP100_MAC1MODE5 | HP100_ACC_PHY	
#define HP100_MAC1MODE7		HP100_MAC1MODE6 | HP100_ACC_ERRORED

#define HP100_TR_MODE		0x80	
#define HP100_TX_SAME		0x40	
#define HP100_LBK_XCVR		0x20	
					
#define HP100_LBK_MAC		0x10	
#define HP100_CRC_I		0x08	
#define HP100_ACCNA             0x04	
#define HP100_KEEP_CRC		0x02	
					
#define HP100_ACCFA             0x01	
#define HP100_MAC2MODEMASK	0x02
#define HP100_MAC2MODE1		0x00
#define HP100_MAC2MODE2		0x00
#define HP100_MAC2MODE3		0x00
#define HP100_MAC2MODE4		0x00
#define HP100_MAC2MODE5		0x00
#define HP100_MAC2MODE6		0x00
#define HP100_MAC2MODE7		KEEP_CRC

#define HP100_PACKET_PACE       0x03	
#define HP100_LRF_EN            0x04	
#define HP100_AUTO_MODE         0x10	

#define HP100_MAC_SEL_ST        0x01	
#define HP100_LINK_FAIL_ST      0x02	

#define HP100_MACRQ_REPEATER         0x0001	
#define HP100_MACRQ_PROMSC           0x0006	
#define HP100_MACRQ_FRAMEFMT_EITHER  0x0018	
#define HP100_MACRQ_FRAMEFMT_802_3   0x0000	
#define HP100_MACRQ_FRAMEFMT_802_5   0x0010	
#define HP100_CARD_MACVER            0xe000	
#define HP100_MALLOW_REPEATER        0x0001	
#define HP100_MALLOW_PROMSC          0x0004	
#define HP100_MALLOW_FRAMEFMT        0x00e0	
#define HP100_MALLOW_ACCDENIED       0x0400	
#define HP100_MALLOW_CONFIGURE       0x0f00	
#define HP100_MALLOW_DUPADDR         0x1000	
#define HP100_HUB_MACVER             0xe000	
					     


#define HP100_SET_HB		0x0100	
#define HP100_SET_LB		0x0001	
#define HP100_RESET_HB		0x0000	
#define HP100_RESET_LB		0x0000	

#define HP100_LAN_100		100	
#define HP100_LAN_10		10	
#define HP100_LAN_COAX		9	
#define HP100_LAN_ERR		(-1)	


#define MAX_RX_PDL              30	
#define MAX_RX_FRAG             2	
#define MAX_TX_PDL              29
#define MAX_TX_FRAG             2	

#define MAX_RINGSIZE ((MAX_RX_FRAG*8+4+4)*MAX_RX_PDL+(MAX_TX_FRAG*8+4+4)*MAX_TX_PDL)+16

#define MIN_ETHER_SIZE          60
#define MAX_ETHER_SIZE          1514	
					

typedef struct hp100_ring {
	u_int *pdl;		
	u_int pdl_paddr;	
	struct sk_buff *skb;
	struct hp100_ring *next;
} hp100_ring_t;



#define HP100_PKT_LEN_MASK	0x1FFF	


#define HP100_RX_PRI		0x8000	
#define HP100_SDF_ERR		0x4000	
#define HP100_SKEW_ERR		0x2000	
#define HP100_BAD_SYMBOL_ERR	0x1000	
#define HP100_RCV_IPM_ERR	0x0800	
					
#define HP100_SYMBOL_BAL_ERR	0x0400	
#define HP100_VG_ALN_ERR	0x0200	
#define HP100_TRUNC_ERR		0x0100	
#define HP100_RUNT_ERR		0x0040	
					
#define HP100_ALN_ERR		0x0010	
#define HP100_CRC_ERR		0x0008	


#define HP100_MULTI_ADDR_HASH	0x0006	
#define HP100_BROADCAST_ADDR	0x0003	
#define HP100_MULTI_ADDR_NO_HASH 0x0002	
#define HP100_PHYS_ADDR_MATCH	0x0001	
#define HP100_PHYS_ADDR_NO_MATCH 0x0000	


#define hp100_inb( reg ) \
        inb( ioaddr + HP100_REG_##reg )
#define hp100_inw( reg ) \
	inw( ioaddr + HP100_REG_##reg )
#define hp100_inl( reg ) \
	inl( ioaddr + HP100_REG_##reg )
#define hp100_outb( data, reg ) \
	outb( data, ioaddr + HP100_REG_##reg )
#define hp100_outw( data, reg ) \
	outw( data, ioaddr + HP100_REG_##reg )
#define hp100_outl( data, reg ) \
	outl( data, ioaddr + HP100_REG_##reg )
#define hp100_orb( data, reg ) \
	outb( inb( ioaddr + HP100_REG_##reg ) | (data), ioaddr + HP100_REG_##reg )
#define hp100_orw( data, reg ) \
	outw( inw( ioaddr + HP100_REG_##reg ) | (data), ioaddr + HP100_REG_##reg )
#define hp100_andb( data, reg ) \
	outb( inb( ioaddr + HP100_REG_##reg ) & (data), ioaddr + HP100_REG_##reg )
#define hp100_andw( data, reg ) \
	outw( inw( ioaddr + HP100_REG_##reg ) & (data), ioaddr + HP100_REG_##reg )

#define hp100_page( page ) \
	outw( HP100_PAGE_##page, ioaddr + HP100_REG_PAGING )
#define hp100_ints_off() \
	outw( HP100_INT_EN | HP100_RESET_LB, ioaddr + HP100_REG_OPTION_LSW )
#define hp100_ints_on() \
	outw( HP100_INT_EN | HP100_SET_LB, ioaddr + HP100_REG_OPTION_LSW )
#define hp100_mem_map_enable() \
	outw( HP100_MMAP_DIS | HP100_RESET_HB, ioaddr + HP100_REG_OPTION_LSW )
#define hp100_mem_map_disable() \
	outw( HP100_MMAP_DIS | HP100_SET_HB, ioaddr + HP100_REG_OPTION_LSW )
