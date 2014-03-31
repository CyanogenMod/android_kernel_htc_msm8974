/*
 * Copyright 2010 Tilera Corporation. All Rights Reserved.
 *
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation, version 2.
 *
 *   This program is distributed in the hope that it will be useful, but
 *   WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE, GOOD TITLE or
 *   NON INFRINGEMENT.  See the GNU General Public License for
 *   more details.
 */


#ifndef __DRV_XGBE_INTF_H__
#define __DRV_XGBE_INTF_H__

typedef struct
{
  
  HV_PhysAddr pa;
  HV_PTE pte;
  
  HV_VirtAddr va;
  
  int size;

}
netio_ipp_address_t;

typedef enum
{
  NETIO_FIXED_ADDR               = 0x5000000000000000ULL,

  NETIO_FIXED_SIZE               = 0x5100000000000000ULL,

  NETIO_IPP_INPUT_REGISTER_OFF   = 0x6000000000000000ULL,

  
  NETIO_IPP_INPUT_UNREGISTER_OFF = 0x6100000000000000ULL,

  
  NETIO_IPP_INPUT_INIT_OFF       = 0x6200000000000000ULL,

  
  NETIO_IPP_INPUT_UNINIT_OFF     = 0x6300000000000000ULL,

  NETIO_IPP_INPUT_GROUP_CFG_OFF  = 0x6400000000000000ULL,

  NETIO_IPP_INPUT_BUCKET_CFG_OFF = 0x6500000000000000ULL,

  NETIO_IPP_PARAM_OFF            = 0x6600000000000000ULL,

  
  NETIO_IPP_GET_FASTIO_OFF       = 0x6700000000000000ULL,

  NETIO_IPP_INPUT_HIJACK_CFG_OFF  = 0x6800000000000000ULL,

  NETIO_IPP_USER_MAX_OFF         = 0x6FFFFFFFFFFFFFFFULL,

  
  NETIO_IPP_IOMEM_REGISTER_OFF   = 0x7000000000000000ULL,

  
  NETIO_IPP_IOMEM_UNREGISTER_OFF = 0x7100000000000000ULL,


  
  NETIO_IPP_DRAIN_OFF              = 0xFA00000000000000ULL,

  NETIO_EPP_SHM_OFF              = 0xFB00000000000000ULL,

  

  
  NETIO_IPP_STOP_SHIM_OFF        = 0xFD00000000000000ULL,

  
  NETIO_IPP_START_SHIM_OFF       = 0xFE00000000000000ULL,

  NETIO_IPP_ADDRESS_OFF          = 0xFF00000000000000ULL,
} netio_hv_offset_t;

#define NETIO_BASE_OFFSET(off)    ((off) & 0xFF00000000000000ULL)
#define NETIO_LOCAL_OFFSET(off)   ((off) & 0x00FFFFFFFFFFFFFFULL)


typedef union
{
  struct
  {
    uint64_t addr:48;        
    unsigned int class:8;    
    unsigned int opcode:8;   
  }
  bits;                      
  uint64_t word;             
}
__netio_getset_offset_t;

typedef enum
{
  NETIO_FASTIO_ALLOCATE         = 0, 
  NETIO_FASTIO_FREE_BUFFER      = 1, 
  NETIO_FASTIO_RETURN_CREDITS   = 2, 
  NETIO_FASTIO_SEND_PKT_NOCK    = 3, 
  NETIO_FASTIO_SEND_PKT_CK      = 4, 
  NETIO_FASTIO_SEND_PKT_VEC     = 5, 
  NETIO_FASTIO_SENDV_PKT        = 6, 
  NETIO_FASTIO_NUM_INDEX        = 7, 
} netio_fastio_index_t;

typedef struct
{
  int err;            
  uint32_t val0;      
  uint32_t val1;      
} netio_fastio_rv3_t;

int __netio_fastio0(uint32_t fastio_index);
int __netio_fastio1(uint32_t fastio_index, uint32_t arg0);
netio_fastio_rv3_t __netio_fastio3_rv3(uint32_t fastio_index, uint32_t arg0,
                                       uint32_t arg1, uint32_t arg2);
int __netio_fastio4(uint32_t fastio_index, uint32_t arg0, uint32_t arg1,
                    uint32_t arg2, uint32_t arg3);
int __netio_fastio6(uint32_t fastio_index, uint32_t arg0, uint32_t arg1,
                    uint32_t arg2, uint32_t arg3, uint32_t arg4, uint32_t arg5);
int __netio_fastio9(uint32_t fastio_index, uint32_t arg0, uint32_t arg1,
                    uint32_t arg2, uint32_t arg3, uint32_t arg4, uint32_t arg5,
                    uint32_t arg6, uint32_t arg7, uint32_t arg8);

#define __netio_fastio_allocate(fastio_index, size) \
  __netio_fastio1((fastio_index) + NETIO_FASTIO_ALLOCATE, size)

#define __netio_fastio_free_buffer(fastio_index, handle) \
  __netio_fastio1((fastio_index) + NETIO_FASTIO_FREE_BUFFER, handle)

#define __netio_fastio_return_credits(fastio_index, credits) \
  __netio_fastio1((fastio_index) + NETIO_FASTIO_RETURN_CREDITS, credits)

#define __netio_fastio_send_pkt_nock(fastio_index, ackflag, size, va, handle) \
  __netio_fastio4((fastio_index) + NETIO_FASTIO_SEND_PKT_NOCK, ackflag, \
                  size, va, handle)

#define __netio_fastio_send_pkt_ck(fastio_index, ackflag, size, va, handle, \
                                   csum0, csum1) \
  __netio_fastio6((fastio_index) + NETIO_FASTIO_SEND_PKT_CK, ackflag, \
                  size, va, handle, csum0, csum1)


typedef union
{
  struct
  {
    unsigned int start_byte:7;       
    unsigned int count:14;           
    unsigned int destination_byte:7; 
    unsigned int reserved:4;         
  } bits;                            
  unsigned int word;                 
} __netio_checksum_header_t;


#define __netio_fastio_sendv_pkt_1_2(fastio_index, flags, confno, csum0, \
                                     va_F, va_L, len_F_L) \
  __netio_fastio6((fastio_index) + NETIO_FASTIO_SENDV_PKT, flags, confno, \
                  csum0, va_F, va_L, len_F_L)

#define __netio_fastio_send_pcie_pkt(fastio_index, flags, confno, csum0, \
                                     va_F, va_L, len_F_L) \
  __netio_fastio6((fastio_index) + PCIE_FASTIO_SENDV_PKT, flags, confno, \
                  csum0, va_F, va_L, len_F_L)

#define __netio_fastio_sendv_pkt_3_4(fastio_index, flags, confno, csum0, va_F, \
                                     va_L, len_F_L, va_M0, va_M1, len_M0_M1) \
  __netio_fastio9((fastio_index) + NETIO_FASTIO_SENDV_PKT, flags, confno, \
                  csum0, va_F, va_L, len_F_L, va_M0, va_M1, len_M0_M1)

#define __netio_fastio_send_pkt_vec(fastio_index, seqno, nentries, va) \
  __netio_fastio3_rv3((fastio_index) + NETIO_FASTIO_SEND_PKT_VEC, seqno, \
                      nentries, va)


typedef struct
{
  uint8_t tso               : 1;

  
  uint8_t _unused           : 3;

  uint8_t hash_for_home     : 1;

  
  uint8_t compute_checksum  : 1;

  uint8_t end_of_packet     : 1;

  
  uint8_t send_completion   : 1;

  uint8_t cpa_hi;

  
  uint16_t length;

  uint32_t cpa_lo;

  
  __netio_checksum_header_t checksum_data;

} lepp_cmd_t;


typedef struct
{
  
  uint32_t cpa_lo;
  
  uint16_t cpa_hi		: 15;
  uint16_t hash_for_home	: 1;
  
  uint16_t length;
} lepp_frag_t;


typedef struct
{
  uint8_t tso             : 1;

  
  uint8_t _unused         : 7;

  uint8_t header_size;

  
  uint8_t ip_offset;

  
  uint8_t tcp_offset;

  uint16_t payload_size;

  
  uint16_t num_frags;

  
  lepp_frag_t frags[0 ];


} lepp_tso_cmd_t;


typedef void* lepp_comp_t;


#define LEPP_MAX_FRAGS (65536 / HV_PAGE_SIZE_SMALL + 2 + 1)

#define LEPP_TSO_CMD_SIZE(num_frags, header_size) \
  (sizeof(lepp_tso_cmd_t) + \
   (num_frags) * sizeof(lepp_frag_t) + \
   (((header_size) + 3) & -4))

#define LEPP_CMD_QUEUE_BYTES \
 (((CHIP_L2_CACHE_SIZE() - 2 * CHIP_L2_LINE_SIZE()) / \
  (sizeof(lepp_cmd_t) + sizeof(lepp_comp_t))) * sizeof(lepp_cmd_t))

#define LEPP_MAX_CMD_SIZE LEPP_TSO_CMD_SIZE(LEPP_MAX_FRAGS, 128)

#define LEPP_CMD_LIMIT \
  (LEPP_CMD_QUEUE_BYTES - LEPP_MAX_CMD_SIZE)

#define LEPP_COMP_QUEUE_SIZE \
  ((LEPP_CMD_LIMIT + sizeof(lepp_cmd_t) - 1) / sizeof(lepp_cmd_t))

#define LEPP_QINC(var) \
  (var = __insn_mnz(var - (LEPP_COMP_QUEUE_SIZE - 1), var + 1))

typedef struct
{
  /** Index of first completion not yet processed by user code.
   *  If this is equal to comp_busy, there are no such completions.
   *
   *  NOTE: This is only read/written by the user.
   */
  unsigned int comp_head;

  /** Index of first completion record not yet completed.
   *  If this is equal to comp_tail, there are no such completions.
   *  This index gets advanced (modulo LEPP_QUEUE_SIZE) whenever
   *  a command with the 'completion' bit set is finished.
   *
   *  NOTE: This is only written by LEPP, only read by the user.
   */
  volatile unsigned int comp_busy;

  /** Index of the first empty slot in the completion ring.
   *  Entries from this up to but not including comp_head (in ring order)
   *  can be filled in with completion data.
   *
   *  NOTE: This is only read/written by the user.
   */
  unsigned int comp_tail;

  /** Byte index of first command enqueued for LEPP but not yet processed.
   *
   *  This is always divisible by sizeof(void*) and always <= LEPP_CMD_LIMIT.
   *
   *  NOTE: LEPP advances this counter as soon as it no longer needs
   *  the cmds[] storage for this entry, but the transfer is not actually
   *  complete (i.e. the buffer pointed to by the command is no longer
   *  needed) until comp_busy advances.
   *
   *  If this is equal to cmd_tail, the ring is empty.
   *
   *  NOTE: This is only written by LEPP, only read by the user.
   */
  volatile unsigned int cmd_head;

  /** Byte index of first empty slot in the command ring.  This field can
   *  be incremented up to but not equal to cmd_head (because that would
   *  mean the ring is empty).
   *
   *  This is always divisible by sizeof(void*) and always <= LEPP_CMD_LIMIT.
   *
   *  NOTE: This is read/written by the user, only read by LEPP.
   */
  volatile unsigned int cmd_tail;

  /** A ring of variable-sized egress DMA commands.
   *
   *  NOTE: Only written by the user, only read by LEPP.
   */
  char cmds[LEPP_CMD_QUEUE_BYTES]
    __attribute__((aligned(CHIP_L2_LINE_SIZE())));

  /** A ring of user completion data.
   *  NOTE: Only read/written by the user.
   */
  lepp_comp_t comps[LEPP_COMP_QUEUE_SIZE]
    __attribute__((aligned(CHIP_L2_LINE_SIZE())));
} lepp_queue_t;


static inline unsigned int
_lepp_num_free_slots(unsigned int head, unsigned int tail)
{
  return (head - tail - 1) + ((head <= tail) ? LEPP_COMP_QUEUE_SIZE : 0);
}


static inline unsigned int
lepp_num_free_comp_slots(const lepp_queue_t* q)
{
  return _lepp_num_free_slots(q->comp_head, q->comp_tail);
}

static inline int
lepp_qsub(int v1, int v2)
{
  int delta = v1 - v2;
  return delta + ((delta >> 31) & LEPP_COMP_QUEUE_SIZE);
}


#define LIPP_VERSION 1


#define LIPP_PACKET_PADDING 2

#define LIPP_SMALL_PACKET_SIZE 128


#define LIPP_SMALL_BUFFERS 6785

#define LIPP_LARGE_BUFFERS 6785

#endif 
