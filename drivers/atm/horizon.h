/*
  Madge Horizon ATM Adapter driver.
  Copyright (C) 1995-1999  Madge Networks Ltd.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

  The GNU GPL is contained in /usr/doc/copyright/GPL on a Debian
  system and in the file COPYING in the Linux kernel source.
*/



#ifndef DRIVER_ATM_HORIZON_H
#define DRIVER_ATM_HORIZON_H


#ifdef CONFIG_ATM_HORIZON_DEBUG
#define DEBUG_HORIZON
#endif

#define DEV_LABEL                         "hrz"

#ifndef PCI_VENDOR_ID_MADGE
#define PCI_VENDOR_ID_MADGE               0x10B6
#endif
#ifndef PCI_DEVICE_ID_MADGE_HORIZON
#define PCI_DEVICE_ID_MADGE_HORIZON       0x1000
#endif


#define PRINTK(severity,format,args...) \
  printk(severity DEV_LABEL ": " format "\n" , ## args)

#ifdef DEBUG_HORIZON

#define DBG_ERR  0x0001
#define DBG_WARN 0x0002
#define DBG_INFO 0x0004
#define DBG_VCC  0x0008
#define DBG_QOS  0x0010
#define DBG_TX   0x0020
#define DBG_RX   0x0040
#define DBG_SKB  0x0080
#define DBG_IRQ  0x0100
#define DBG_FLOW 0x0200
#define DBG_BUS  0x0400
#define DBG_REGS 0x0800
#define DBG_DATA 0x1000
#define DBG_MASK 0x1fff

#define PRINTDB(bits,format,args...) \
  ( (debug & (bits)) ? printk (KERN_INFO DEV_LABEL ": " format , ## args) : 1 )
#define PRINTDM(bits,format,args...) \
  ( (debug & (bits)) ? printk (format , ## args) : 1 )
#define PRINTDE(bits,format,args...) \
  ( (debug & (bits)) ? printk (format "\n" , ## args) : 1 )
#define PRINTD(bits,format,args...) \
  ( (debug & (bits)) ? printk (KERN_INFO DEV_LABEL ": " format "\n" , ## args) : 1 )

#else

#define PRINTD(bits,format,args...)
#define PRINTDB(bits,format,args...)
#define PRINTDM(bits,format,args...)
#define PRINTDE(bits,format,args...)

#endif

#define PRINTDD(sec,fmt,args...)
#define PRINTDDB(sec,fmt,args...)
#define PRINTDDM(sec,fmt,args...)
#define PRINTDDE(sec,fmt,args...)


#define SPARE_BUFFER_POOL_SIZE            MAX_VCS
#define HRZ_MAX_VPI                       4
#define MIN_PCI_LATENCY                   48 


#define HRZ_IO_EXTENT                     0x80

#define DATA_PORT_OFF                     0x00
#define TX_CHANNEL_PORT_OFF               0x04
#define TX_DESCRIPTOR_PORT_OFF            0x08
#define MEMORY_PORT_OFF                   0x0C
#define MEM_WR_ADDR_REG_OFF               0x14
#define MEM_RD_ADDR_REG_OFF               0x18
#define CONTROL_0_REG                     0x1C
#define INT_SOURCE_REG_OFF                0x20
#define INT_ENABLE_REG_OFF                0x24
#define MASTER_RX_ADDR_REG_OFF            0x28
#define MASTER_RX_COUNT_REG_OFF           0x2C
#define MASTER_TX_ADDR_REG_OFF            0x30
#define MASTER_TX_COUNT_REG_OFF           0x34
#define TX_DESCRIPTOR_REG_OFF             0x38
#define TX_CHANNEL_CONFIG_COMMAND_OFF     0x40
#define TX_CHANNEL_CONFIG_DATA_OFF        0x44
#define TX_FREE_BUFFER_COUNT_OFF          0x48
#define RX_FREE_BUFFER_COUNT_OFF          0x4C
#define TX_CONFIG_OFF                     0x50
#define TX_STATUS_OFF                     0x54
#define RX_CONFIG_OFF                     0x58
#define RX_LINE_CONFIG_OFF                0x5C
#define RX_QUEUE_RD_PTR_OFF               0x60
#define RX_QUEUE_WR_PTR_OFF               0x64
#define MAX_AAL5_CELL_COUNT_OFF           0x68
#define RX_CHANNEL_PORT_OFF               0x6C
#define TX_CELL_COUNT_OFF                 0x70
#define RX_CELL_COUNT_OFF                 0x74
#define HEC_ERROR_COUNT_OFF               0x78
#define UNASSIGNED_CELL_COUNT_OFF         0x7C



#define SEEPROM_DO                        0x00000001
#define SEEPROM_DI                        0x00000002
#define SEEPROM_SK                        0x00000004
#define SEEPROM_CS                        0x00000008
#define DEBUG_BIT_0                       0x00000010
#define DEBUG_BIT_1                       0x00000020
#define DEBUG_BIT_2                       0x00000040
#define DEBUG_BIT_0_OE                    0x00000100
#define DEBUG_BIT_1_OE                    0x00000200
#define DEBUG_BIT_2_OE                    0x00000400
#define DEBUG_BIT_0_STATE                 0x00001000
#define DEBUG_BIT_1_STATE                 0x00002000
#define DEBUG_BIT_2_STATE                 0x00004000
#define GENERAL_BIT_0                     0x00010000
#define GENERAL_BIT_1                     0x00020000
#define GENERAL_BIT_2                     0x00040000
#define GENERAL_BIT_3                     0x00080000
#define RESET_HORIZON                     0x00100000
#define RESET_ATM                         0x00200000
#define RESET_RX                          0x00400000
#define RESET_TX                          0x00800000
#define RESET_HOST                        0x01000000
#define TARGET_RETRY_DISABLE              0x04000000
#define ATM_LAYER_SELECT                  0x08000000
#define ATM_LAYER_STATUS                  0x10000000


#define RX_DATA_AV                        0x00000001
#define RX_DISABLED                       0x00000002
#define TIMING_MARKER                     0x00000004
#define FORCED                            0x00000008
#define RX_BUS_MASTER_COMPLETE            0x00000010
#define TX_BUS_MASTER_COMPLETE            0x00000020
#define ABR_TX_CELL_COUNT_INT             0x00000040
#define DEBUG_INT                         0x00000080


#define MAX_PIO_COUNT                     0x000000ff 
#define MAX_TRANSFER_COUNT                0x00001ffc 
#define MASTER_TX_AUTO_APPEND_DESC        0x80000000


#define PCR_TIMER_ACCESS                      0x0000
#define SCR_TIMER_ACCESS                      0x0001
#define BUCKET_CAPACITY_ACCESS                0x0002
#define BUCKET_FULLNESS_ACCESS                0x0003
#define RATE_TYPE_ACCESS                      0x0004
#define TX_CHANNEL_CONFIG_MULT                0x0100
#define BUCKET_MAX_SIZE                       0x003f


#define CLOCK_SELECT_SHIFT                    4
#define CLOCK_DISABLE                         0x00ff

#define IDLE_RATE_TYPE                       0x0
#define ABR_RATE_TYPE                        0x1
#define VBR_RATE_TYPE                        0x2
#define CBR_RATE_TYPE                        0x3


#define DRVR_DRVRBAR_ENABLE                   0x0001
#define TXCLK_MUX_SELECT_RCLK                 0x0002
#define TRANSMIT_TIMING_MARKER                0x0004
#define LOOPBACK_TIMING_MARKER                0x0008
#define TX_TEST_MODE_16MHz                    0x0000
#define TX_TEST_MODE_8MHz                     0x0010
#define TX_TEST_MODE_5_33MHz                  0x0020
#define TX_TEST_MODE_4MHz                     0x0030
#define TX_TEST_MODE_3_2MHz                   0x0040
#define TX_TEST_MODE_2_66MHz                  0x0050
#define TX_TEST_MODE_2_29MHz                  0x0060
#define TX_NORMAL_OPERATION                   0x0070
#define ABR_ROUND_ROBIN                       0x0080


#define IDLE_CHANNELS_MASK                    0x00FF
#define ABR_CELL_COUNT_REACHED_MULT           0x0100 
#define ABR_CELL_COUNT_REACHED_MASK           0xFF


#define NON_USER_CELLS_IN_ONE_CHANNEL         0x0008
#define RX_ENABLE                             0x0010
#define IGNORE_UNUSED_VPI_VCI_BITS_SET        0x0000
#define NON_USER_UNUSED_VPI_VCI_BITS_SET      0x0020
#define DISCARD_UNUSED_VPI_VCI_BITS_SET       0x0040


#define SIGNAL_LOSS                           0x0001
#define FREQUENCY_DETECT_ERROR                0x0002
#define LOCK_DETECT_ERROR                     0x0004
#define SELECT_INTERNAL_LOOPBACK              0x0008
#define LOCK_DETECT_ENABLE                    0x0010
#define FREQUENCY_DETECT_ENABLE               0x0020
#define USER_FRAQ                             0x0040
#define GXTALOUT_SELECT_DIV4                  0x0080
#define GXTALOUT_SELECT_NO_GATING             0x0100
#define TIMING_MARKER_RECEIVED                0x0200


#define RX_CHANNEL_MASK                       0x03FF
#define FLUSH_CHANNEL                         0x4000
#define RX_CHANNEL_UPDATE_IN_PROGRESS         0x8000


#define RX_Q_ENTRY_LENGTH_MASK            0x0000FFFF
#define RX_Q_ENTRY_CHANNEL_SHIFT          16
#define SIMONS_DODGEY_MARKER              0x08000000
#define RX_CONGESTION_EXPERIENCED         0x10000000
#define RX_CRC_10_OK                      0x20000000
#define RX_CRC_32_OK                      0x40000000
#define RX_COMPLETE_FRAME                 0x80000000



#define BUFFER_PTR_MASK                   0x0000FFFF
#define RX_INT_THRESHOLD_MULT             0x00010000
#define RX_INT_THRESHOLD_MASK             0x07FF
#define INT_EVERY_N_CELLS                 0x08000000
#define CONGESTION_EXPERIENCED            0x10000000
#define FIRST_CELL_OF_AAL5_FRAME          0x20000000
#define CHANNEL_TYPE_AAL5                 0x00000000
#define CHANNEL_TYPE_RAW_CELLS            0x40000000
#define CHANNEL_TYPE_AAL3_4               0x80000000


#define BUFF_STATUS_MASK                  0x00030000
#define BUFF_STATUS_EMPTY                 0x00000000
#define BUFF_STATUS_CELL_AV               0x00010000
#define BUFF_STATUS_LAST_CELL_AV          0x00020000



#define RX_CHANNEL_DISABLED               0x00000000
#define RX_CHANNEL_IDLE                   0x00000001


#define INITIAL_CRC                       0xFFFFFFFF


typedef u8 HDW;

typedef struct cell_buf {
  HDW payload[12];
  HDW next;
  HDW cell_count;               
  HDW res;
  union {
    HDW partial_crc;            
    HDW cell_header;            
  } u;
} cell_buf;

typedef struct tx_ch_desc {
  HDW rd_buf_type;
  HDW wr_buf_type;
  HDW partial_crc;
  HDW cell_header;
} tx_ch_desc;

typedef struct rx_ch_desc {
  HDW wr_buf_type;
  HDW rd_buf_type;
} rx_ch_desc;

typedef struct rx_q_entry {
  HDW entry;
} rx_q_entry;

#define TX_CHANS 8
#define RX_CHANS 1024
#define RX_QS 1024
#define MAX_VCS RX_CHANS


#define BUFN1_SIZE              118 
#define BUFN2_SIZE              124
#define BUFN3_SIZE              192
#define BUFN4_SIZE             1408


typedef struct MEMMAP {
  tx_ch_desc  tx_descs[TX_CHANS];     
  cell_buf    inittxbufs[TX_CHANS];   
  cell_buf    bufn1[BUFN1_SIZE];      
  cell_buf    txfreebufstart;
  cell_buf    txfreebufend;
  cell_buf    rxfreebufstart;
  cell_buf    rxfreebufend;           
  cell_buf    bufn2[BUFN2_SIZE];      
  rx_q_entry  rx_q_entries[RX_QS];    
  cell_buf    bufn3[BUFN3_SIZE];      
  rx_ch_desc  rx_descs[MAX_VCS];      
  cell_buf    bufn4[BUFN4_SIZE];      
} MEMMAP;

#define memmap ((MEMMAP *)0)


typedef enum {
  aal0,
  aal34,
  aal5
} hrz_aal;

typedef enum {
  tx_busy,
  rx_busy,
  ultra
} hrz_flags;


typedef struct {
  unsigned int        tx_rate;
  unsigned int        rx_rate;
  u16                 channel;
  u16                 tx_xbr_bits;
  u16                 tx_pcr_bits;
#if 0
  u16                 tx_scr_bits;
  u16                 tx_bucket_bits;
#endif
  hrz_aal             aal;
} hrz_vcc;

struct hrz_dev {
  
  u32                 iobase;
  u32 *               membase;

  struct sk_buff *    rx_skb;     
  unsigned int        rx_bytes;   
  void *              rx_addr;    
  unsigned int        rx_channel; 

  struct sk_buff *    tx_skb;     
  unsigned int        tx_bytes;   
  void *              tx_addr;    
  struct iovec *      tx_iovec;   
  unsigned int        tx_regions; 

  spinlock_t          mem_lock;
  wait_queue_head_t   tx_queue;

  u8                  irq;
  unsigned long	      flags;
  u8                  tx_last;
  u8                  tx_idle;

  rx_q_entry *        rx_q_reset;
  rx_q_entry *        rx_q_entry;
  rx_q_entry *        rx_q_wrap;

  struct atm_dev *    atm_dev;

  u32                 last_vc;
  
  int                 noof_spare_buffers;
  u16                 spare_buffers[SPARE_BUFFER_POOL_SIZE];

  u16                 tx_channel_record[TX_CHANS];

  
  u32              txer[MAX_VCS/32];
  struct atm_vcc * rxer[MAX_VCS];

  
  spinlock_t       rate_lock;
  unsigned int     rx_avail;
  unsigned int     tx_avail;
  
  
  unsigned long    tx_cell_count;
  unsigned long    rx_cell_count;
  unsigned long    hec_error_count;
  unsigned long    unassigned_cell_count;

  struct pci_dev * pci_dev;
  struct timer_list housekeeping;
};

typedef struct hrz_dev hrz_dev;


#define BUF_PTR(cbptr) ((cbptr) - (cell_buf *) 0)

#define INTERESTING_INTERRUPTS \
  (RX_DATA_AV | RX_DISABLED | TX_BUS_MASTER_COMPLETE | RX_BUS_MASTER_COMPLETE)

#define TX_AAL5_LIMIT (190*ATM_CELL_PAYLOAD-ATM_AAL5_TRAILER) 

#define RX_AAL5_LIMIT ATM_MAX_AAL5_PDU

#define DW(x) do{ x } while(0)

#define HRZ_DEV(atm_dev) ((hrz_dev *) (atm_dev)->dev_data)
#define HRZ_VCC(atm_vcc) ((hrz_vcc *) (atm_vcc)->dev_data)


#define YELLOW_LED    DEBUG_BIT_0
#define GREEN_LED     DEBUG_BIT_1
#define YELLOW_LED_OE DEBUG_BIT_0_OE
#define GREEN_LED_OE  DEBUG_BIT_1_OE

#define GREEN_LED_OFF(dev)                      \
  wr_regl (dev, CONTROL_0_REG, rd_regl (dev, CONTROL_0_REG) | GREEN_LED)
#define GREEN_LED_ON(dev)                       \
  wr_regl (dev, CONTROL_0_REG, rd_regl (dev, CONTROL_0_REG) &~ GREEN_LED)
#define YELLOW_LED_OFF(dev)                     \
  wr_regl (dev, CONTROL_0_REG, rd_regl (dev, CONTROL_0_REG) | YELLOW_LED)
#define YELLOW_LED_ON(dev)                      \
  wr_regl (dev, CONTROL_0_REG, rd_regl (dev, CONTROL_0_REG) &~ YELLOW_LED)

typedef enum {
  round_up,
  round_down,
  round_nearest
} rounding;

#endif 
