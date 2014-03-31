/*
  Madge Ambassador ATM Adapter driver.
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

#ifndef AMBASSADOR_H
#define AMBASSADOR_H


#ifdef CONFIG_ATM_AMBASSADOR_DEBUG
#define DEBUG_AMBASSADOR
#endif

#define DEV_LABEL                          "amb"

#ifndef PCI_VENDOR_ID_MADGE
#define PCI_VENDOR_ID_MADGE                0x10B6
#endif
#ifndef PCI_VENDOR_ID_MADGE_AMBASSADOR
#define PCI_DEVICE_ID_MADGE_AMBASSADOR     0x1001
#endif
#ifndef PCI_VENDOR_ID_MADGE_AMBASSADOR_BAD
#define PCI_DEVICE_ID_MADGE_AMBASSADOR_BAD 0x1002
#endif


#define PRINTK(severity,format,args...) \
  printk(severity DEV_LABEL ": " format "\n" , ## args)

#ifdef DEBUG_AMBASSADOR

#define DBG_ERR  0x0001
#define DBG_WARN 0x0002
#define DBG_INFO 0x0004
#define DBG_INIT 0x0008
#define DBG_LOAD 0x0010
#define DBG_VCC  0x0020
#define DBG_QOS  0x0040
#define DBG_CMD  0x0080
#define DBG_TX   0x0100
#define DBG_RX   0x0200
#define DBG_SKB  0x0400
#define DBG_POOL 0x0800
#define DBG_IRQ  0x1000
#define DBG_FLOW 0x2000
#define DBG_REGS 0x4000
#define DBG_DATA 0x8000
#define DBG_MASK 0xffff

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

#define PRINTDD(bits,format,args...)
#define PRINTDDB(sec,fmt,args...)
#define PRINTDDM(sec,fmt,args...)
#define PRINTDDE(sec,fmt,args...)


#define COM_Q_ENTRIES        8
#define TX_Q_ENTRIES        32
#define RX_Q_ENTRIES        64


#define AMB_EXTENT         0x80

#define MIN_QUEUE_SIZE     2

#define NUM_RX_POOLS	   4

#define MIN_RX_BUFFERS	   1

#define MIN_PCI_LATENCY   64 

#define NUM_VPI_BITS       0
#define NUM_VCI_BITS      10
#define NUM_VCS         1024

#define RX_ERR		0x8000 
#define CRC_ERR		0x4000 
#define LEN_ERR		0x2000 
#define ABORT_ERR	0x1000 
#define UNUSED_ERR	0x0800 


#define SRB_OPEN_VC		0
 




#define	SRB_CLOSE_VC		1

#define	SRB_GET_BIA		2

#define	SRB_GET_SUNI_STATS	3

#define	SRB_SET_BITS_8		4
#define	SRB_SET_BITS_16		5
#define	SRB_SET_BITS_32		6
#define	SRB_CLEAR_BITS_8	7
#define	SRB_CLEAR_BITS_16	8
#define	SRB_CLEAR_BITS_32	9

#define	SRB_SET_8		10
#define	SRB_SET_16		11
#define	SRB_SET_32		12

#define	SRB_GET_32		13

#define SRB_GET_VERSION		14

#define SRB_FLUSH_BUFFER_Q	15
 

#define	SRB_GET_DMA_SPEEDS	16

#define SRB_MODIFY_VC_RATE	17

#define SRB_MODIFY_VC_FLAGS	18
 




#define SRB_RATE_SHIFT          16
#define SRB_POOL_SHIFT          (SRB_FLAGS_SHIFT+5)
#define SRB_FLAGS_SHIFT         16

#define	SRB_STOP_TASKING	19
#define	SRB_START_TASKING	20
#define SRB_SHUT_DOWN		21
#define MAX_SRB			21

#define SRB_COMPLETE		0xffffffff

#define TX_FRAME          	0x80000000

#define NUM_OF_SRB	32

#define MAX_RATE_BITS	6

#define TX_UBR          0x0000
#define TX_UBR_CAPPED   0x0008
#define TX_ABR          0x0018
#define TX_FRAME_NOTCAP 0x0000
#define TX_FRAME_CAPPED 0x8000

#define FP_155_RATE	0x24b1
#define FP_25_RATE	0x1f9d


#define VERSION_NUMBER 0x01050025 

#define DMA_VALID 0xb728e149 

#define FLASH_BASE 0xa0c00000
#define FLASH_SIZE 0x00020000			
#define BIA_BASE (FLASH_BASE+0x0001c000)	
#define BIA_ADDRESS ((void *)0xa0c1c000)
#define PLX_BASE 0xe0000000

typedef enum {
  host_memory_test = 1,
  read_adapter_memory,
  write_adapter_memory,
  adapter_start,
  get_version_number,
  interrupt_host,
  flash_erase_sector,
  adap_download_block = 0x20,
  adap_erase_flash,
  adap_run_in_iram,
  adap_end_download
} loader_command;

#define BAD_COMMAND                     (-1)
#define COMMAND_IN_PROGRESS             1
#define COMMAND_PASSED_TEST             2
#define COMMAND_FAILED_TEST             3
#define COMMAND_READ_DATA_OK            4
#define COMMAND_READ_BAD_ADDRESS        5
#define COMMAND_WRITE_DATA_OK           6
#define COMMAND_WRITE_BAD_ADDRESS       7
#define COMMAND_WRITE_FLASH_FAILURE     8
#define COMMAND_COMPLETE                9
#define COMMAND_FLASH_ERASE_FAILURE	10
#define COMMAND_WRITE_BAD_DATA		11


#define GPINT_TST_FAILURE               0x00000001      
#define SUNI_DATA_PATTERN_FAILURE       0x00000002
#define SUNI_DATA_BITS_FAILURE          0x00000004
#define SUNI_UTOPIA_FAILURE             0x00000008
#define SUNI_FIFO_FAILURE               0x00000010
#define SRAM_FAILURE                    0x00000020
#define SELF_TEST_FAILURE               0x0000003f




#define UNUSED_LOADER_MAILBOXES 6

typedef struct {
  u32 stuff[16];
  union {
    struct {
      u32 result;
      u32 ready;
      u32 stuff[UNUSED_LOADER_MAILBOXES];
    } loader;
    struct {
      u32 cmd_address;
      u32 tx_address;
      u32 rx_address[NUM_RX_POOLS];
      u32 gen_counter;
      u32 spare;
    } adapter;
  } mb;
  u32 doorbell;
  u32 interrupt;
  u32 interrupt_control;
  u32 reset_control;
} amb_mem;

#define AMB_RESET_BITS	   0x40000000
#define AMB_INTERRUPT_BITS 0x00000300
#define AMB_DOORBELL_BITS  0x00030000


#define MAX_COMMAND_DATA 13
#define MAX_TRANSFER_DATA 11

typedef struct {
  __be32 address;
  __be32 count;
  __be32 data[MAX_TRANSFER_DATA];
} transfer_block;

typedef struct {
  __be32 result;
  __be32 command;
  union {
    transfer_block transfer;
    __be32 version;
    __be32 start;
    __be32 data[MAX_COMMAND_DATA];
  } payload;
  __be32 valid;
} loader_block;



typedef	struct {
  union {
    struct {
      __be32 vc;
      __be32 flags;
      __be32 rate;
    } open;
    struct {
      __be32 vc;
      __be32 rate;
    } modify_rate;
    struct {
      __be32 vc;
      __be32 flags;
    } modify_flags;
    struct {
      __be32 vc;
    } close;
    struct {
      __be32 lower4;
      __be32 upper2;
    } bia;
    struct {
      __be32 address;
    } suni;
    struct {
      __be32 major;
      __be32 minor;
    } version;
    struct {
      __be32 read;
      __be32 write;
    } speed;
    struct {
      __be32 flags;
    } flush;
    struct {
      __be32 address;
      __be32 data;
    } memory;
    __be32 par[3];
  } args;
  __be32 request;
} command;




typedef struct {
  __be32 bytes;
  __be32 address;
} tx_frag;


typedef struct {
  u32	handle;
  u16	vc;
  u16	next_descriptor_length;
  u32	next_descriptor;
#ifdef AMB_NEW_MICROCODE
  u8    cpcs_uu;
  u8    cpi;
  u16   pad;
#endif
} tx_frag_end;

typedef struct {
  tx_frag tx_frag;
  tx_frag_end tx_frag_end;
  struct sk_buff * skb;
} tx_simple;

#if 0
typedef union {
  tx_frag	fragment;
  tx_frag_end	end_of_list;
} tx_descr;
#endif


typedef	struct {
  __be16	vc;
  __be16	tx_descr_length;
  __be32	tx_descr_addr;
} tx_in;


typedef	struct {
  u32 handle;
} tx_out;



typedef struct {
  u32  handle;
  __be16  vc;
  __be16  lec_id; 
  __be16  status;
  __be16  length;
} rx_out;


typedef	struct {
  u32 handle;
  __be32 host_address;
} rx_in;


typedef struct {
  __be32 command_start;		
  __be32 command_end;		
  __be32 tx_start;
  __be32 tx_end;
  __be32 txcom_start;		
  __be32 txcom_end;		
  struct {
    __be32 buffer_start;
    __be32 buffer_end;
    u32 buffer_q_get;
    u32 buffer_q_end;
    u32 buffer_aptr;
    __be32 rx_start;		
    __be32 rx_end;
    u32 rx_ptr;
    __be32 buffer_size;		
  } rec_struct[NUM_RX_POOLS];
#ifdef AMB_NEW_MICROCODE
  u16 init_flags;
  u16 talk_block_spare;
#endif
} adap_talk_block;


typedef struct {
  u8	racp_chcs;
  u8	racp_uhcs;
  u16	spare;
  u32	racp_rcell;
  u32	tacp_tcell;
  u32	flags;
  u32	dropped_cells;
  u32	dropped_frames;
} suni_stats;

typedef enum {
  dead
} amb_flags;

#define NEXTQ(current,start,limit) \
  ( (current)+1 < (limit) ? (current)+1 : (start) ) 

typedef struct {
  command * start;
  command * in;
  command * out;
  command * limit;
} amb_cq_ptrs;

typedef struct {
  spinlock_t lock;
  unsigned int pending;
  unsigned int high;
  unsigned int filled;
  unsigned int maximum; 
  amb_cq_ptrs ptrs;
} amb_cq;

typedef struct {
  spinlock_t lock;
  unsigned int pending;
  unsigned int high;
  unsigned int filled;
  unsigned int maximum; 
  struct {
    tx_in * start;
    tx_in * ptr;
    tx_in * limit;
  } in;
  struct {
    tx_out * start;
    tx_out * ptr;
    tx_out * limit;
  } out;
} amb_txq;

typedef struct {
  spinlock_t lock;
  unsigned int pending;
  unsigned int low;
  unsigned int emptied;
  unsigned int maximum; 
  struct {
    rx_in * start;
    rx_in * ptr;
    rx_in * limit;
  } in;
  struct {
    rx_out * start;
    rx_out * ptr;
    rx_out * limit;
  } out;
  unsigned int buffers_wanted;
  unsigned int buffer_size;
} amb_rxq;

typedef struct {
  unsigned long tx_ok;
  struct {
    unsigned long ok;
    unsigned long error;
    unsigned long badcrc;
    unsigned long toolong;
    unsigned long aborted;
    unsigned long unused;
  } rx;
} amb_stats;


typedef struct {
  u8               tx_vc_bits:7;
  u8               tx_present:1;
} amb_tx_info;

typedef struct {
  unsigned char    pool;
} amb_rx_info;

typedef struct {
  amb_rx_info      rx_info;
  u16              tx_frame_bits;
  unsigned int     tx_rate;
  unsigned int     rx_rate;
} amb_vcc;

struct amb_dev {
  u8               irq;
  unsigned long	   flags;
  u32              iobase;
  u32 *            membase;

  amb_cq           cq;
  amb_txq          txq;
  amb_rxq          rxq[NUM_RX_POOLS];
  
  struct mutex     vcc_sf;
  amb_tx_info      txer[NUM_VCS];
  struct atm_vcc * rxer[NUM_VCS];
  unsigned int     tx_avail;
  unsigned int     rx_avail;
  
  amb_stats        stats;
  
  struct atm_dev * atm_dev;
  struct pci_dev * pci_dev;
  struct timer_list housekeeping;
};

typedef struct amb_dev amb_dev;

#define AMB_DEV(atm_dev) ((amb_dev *) (atm_dev)->dev_data)
#define AMB_VCC(atm_vcc) ((amb_vcc *) (atm_vcc)->dev_data)


typedef enum {
  round_up,
  round_down,
  round_nearest
} rounding;

#endif
