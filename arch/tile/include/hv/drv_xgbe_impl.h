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


#ifndef __DRV_XGBE_IMPL_H__
#define __DRV_XGBE_IMPL_H__

#include <hv/netio_errors.h>
#include <hv/netio_intf.h>
#include <hv/drv_xgbe_intf.h>


#define LOG2_NUM_GROUPS (12)
#define NUM_GROUPS (1 << LOG2_NUM_GROUPS)

#define EPP_REQS_PER_TILE (32)

#define EDMA_WDS_NO_CSUM      8
#define EDMA_WDS_CSUM        10
#define EDMA_WDS_TOTAL      128


#define SIZE_SMALL (1)       
#define SIZE_LARGE (2)       
#define SIZE_JUMBO (0)       

#define NETIO_NUM_SIZES 3



#define NETIO_DEFAULT_SMALL_PACKETS 2750
#define NETIO_DEFAULT_LARGE_PACKETS 2500
#define NETIO_DEFAULT_JUMBO_PACKETS 250


#define NETIO_ARENA_SHIFT      24      
#define NETIO_ARENA_SIZE       (1 << NETIO_ARENA_SHIFT)


/** A queue of packets.
 *
 * This structure partially defines a queue of packets waiting to be
 * processed.  The queue as a whole is written to by an interrupt handler and
 * read by non-interrupt code; this data structure is what's touched by the
 * interrupt handler.  The other part of the queue state, the read offset, is
 * kept in user space, not in hypervisor space, so it is in a separate data
 * structure.
 *
 * The read offset (__packet_receive_read in the user part of the queue
 * structure) points to the next packet to be read. When the read offset is
 * equal to the write offset, the queue is empty; therefore the queue must
 * contain one more slot than the required maximum queue size.
 *
 * Here's an example of all 3 state variables and what they mean.  All
 * pointers move left to right.
 *
 * @code
 *   I   I   V   V   V   V   I   I   I   I
 *   0   1   2   3   4   5   6   7   8   9  10
 *           ^       ^       ^               ^
 *           |               |               |
 *           |               |               __last_packet_plus_one
 *           |               __buffer_write
 *           __packet_receive_read
 * @endcode
 *
 * This queue has 10 slots, and thus can hold 9 packets (_last_packet_plus_one
 * = 10).  The read pointer is at 2, and the write pointer is at 6; thus,
 * there are valid, unread packets in slots 2, 3, 4, and 5.  The remaining
 * slots are invalid (do not contain a packet).
 */
typedef struct {
  /** Byte offset of the next notify packet to be written: zero for the first
   *  packet on the queue, sizeof (netio_pkt_t) for the second packet on the
   *  queue, etc. */
  volatile uint32_t __packet_write;

  uint32_t __last_packet_plus_one;
}
__netio_packet_queue_t;


/** A queue of buffers.
 *
 * This structure partially defines a queue of empty buffers which have been
 * obtained via requests to the IPP.  (The elements of the queue are packet
 * handles, which are transformed into a full netio_pkt_t when the buffer is
 * retrieved.)  The queue as a whole is written to by an interrupt handler and
 * read by non-interrupt code; this data structure is what's touched by the
 * interrupt handler.  The other parts of the queue state, the read offset and
 * requested write offset, are kept in user space, not in hypervisor space, so
 * they are in a separate data structure.
 *
 * The read offset (__buffer_read in the user part of the queue structure)
 * points to the next buffer to be read. When the read offset is equal to the
 * write offset, the queue is empty; therefore the queue must contain one more
 * slot than the required maximum queue size.
 *
 * The requested write offset (__buffer_requested_write in the user part of
 * the queue structure) points to the slot which will hold the next buffer we
 * request from the IPP, once we get around to sending such a request.  When
 * the requested write offset is equal to the write offset, no requests for
 * new buffers are outstanding; when the requested write offset is one greater
 * than the read offset, no more requests may be sent.
 *
 * Note that, unlike the packet_queue, the buffer_queue places incoming
 * buffers at decreasing addresses.  This makes the check for "is it time to
 * wrap the buffer pointer" cheaper in the assembly code which receives new
 * buffers, and means that the value which defines the queue size,
 * __last_buffer, is different than in the packet queue.  Also, the offset
 * used in the packet_queue is already scaled by the size of a packet; here we
 * use unscaled slot indices for the offsets.  (These differences are
 * historical, and in the future it's possible that the packet_queue will look
 * more like this queue.)
 *
 * @code
 * Here's an example of all 4 state variables and what they mean.  Remember:
 * all pointers move right to left.
 *
 *   V   V   V   I   I   R   R   V   V   V
 *   0   1   2   3   4   5   6   7   8   9
 *           ^       ^       ^           ^
 *           |       |       |           |
 *           |       |       |           __last_buffer
 *           |       |       __buffer_write
 *           |       __buffer_requested_write
 *           __buffer_read
 * @endcode
 *
 * This queue has 10 slots, and thus can hold 9 buffers (_last_buffer = 9).
 * The read pointer is at 2, and the write pointer is at 6; thus, there are
 * valid, unread buffers in slots 2, 1, 0, 9, 8, and 7.  The requested write
 * pointer is at 4; thus, requests have been made to the IPP for buffers which
 * will be placed in slots 6 and 5 when they arrive.  Finally, the remaining
 * slots are invalid (do not contain a buffer).
 */
typedef struct
{
  /** Ordinal number of the next buffer to be written: 0 for the first slot in
   *  the queue, 1 for the second slot in the queue, etc. */
  volatile uint32_t __buffer_write;

  uint32_t __last_buffer;
}
__netio_buffer_queue_t;


typedef struct __netio_queue_impl_t
{
  
  __netio_packet_queue_t __packet_receive_queue;
  
  unsigned int __intr_id;
  
  uint32_t __buffer_queue[NETIO_NUM_SIZES];
  
  
  uint32_t __epp_location;
  
  unsigned int __queue_id;
  
  volatile uint32_t __acks_received;
  
  volatile uint32_t __last_completion_rcv;
  
  uint32_t __max_outstanding;
  
  void* __va_0;
  
  void* __va_1;
  
  uint32_t __padding[3];
  
  netio_pkt_t __packets[0];
}
netio_queue_impl_t;


typedef struct __netio_queue_user_impl_t
{
  
  uint32_t __packet_receive_read;
  
  uint8_t __buffer_read[NETIO_NUM_SIZES];
  uint8_t __buffer_requested_write[NETIO_NUM_SIZES];
  
  uint8_t __pcie;
  
  uint32_t __receive_credit_remaining;
  
  uint32_t __receive_credit_interval;
  
  uint32_t __fastio_index;
  
  uint32_t __acks_outstanding;
  
  uint32_t __last_completion_req;
  
  int __fd;
}
netio_queue_user_impl_t;


#define NETIO_GROUP_CHUNK_SIZE   64   
#define NETIO_BUCKET_CHUNK_SIZE  64   


typedef struct
{
  uint16_t flags;              
  uint16_t transfer_size;      
  uint32_t va;                 
  __netio_pkt_handle_t handle; 
  uint32_t csum0;              
  uint32_t csum1;              
}
__netio_send_cmd_t;



#define __NETIO_SEND_FLG_ACK    0x1

#define __NETIO_SEND_FLG_CSUM   0x2

#define __NETIO_SEND_FLG_COMPLETION 0x4

#define __NETIO_SEND_FLG_XSEG_SHIFT 3

#define __NETIO_SEND_FLG_XSEG_WIDTH 2

#endif 
