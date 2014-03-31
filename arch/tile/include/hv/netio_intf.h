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


#ifndef __NETIO_INTF_H__
#define __NETIO_INTF_H__

#include <hv/netio_errors.h>

#ifdef __KERNEL__
#include <linux/types.h>
#else
#include <stdint.h>
#endif

#if !defined(__HV__) && !defined(__BOGUX__) && !defined(__KERNEL__)
#include <assert.h>
#define netio_assert assert  
#else
#define netio_assert(...) ((void)(0))  
#endif

#if !defined(__HV__) && !defined(__BOGUX__) && !defined(__KERNEL__) && \
    !defined(__NEWLIB__)
#define _NETIO_PTHREAD       

#ifndef NETIO_UNLOCKED

#include <sched.h>
typedef int _netio_percpu_mutex_t;

static __inline int
_netio_percpu_mutex_init(_netio_percpu_mutex_t* lock)
{
  *lock = 0;
  return 0;
}

static __inline int
_netio_percpu_mutex_lock(_netio_percpu_mutex_t* lock)
{
  while (__builtin_expect(__insn_tns(lock), 0))
    sched_yield();
  return 0;
}

static __inline int
_netio_percpu_mutex_unlock(_netio_percpu_mutex_t* lock)
{
  *lock = 0;
  return 0;
}

#else 

typedef int _netio_percpu_mutex_t;
#define _netio_percpu_mutex_init(L)
#define _netio_percpu_mutex_lock(L)
#define _netio_percpu_mutex_unlock(L)

#endif 
#endif 

#define NETIO_MAX_TILES_PER_QUEUE  64


#define NETIO_MAX_QUEUE_ID        255


#ifndef __DOXYGEN__


#define _NETIO_PKT_NO_L4_CSUM_SHIFT           0
#define _NETIO_PKT_NO_L4_CSUM_RMASK           1
#define _NETIO_PKT_NO_L4_CSUM_MASK \
         (_NETIO_PKT_NO_L4_CSUM_RMASK << _NETIO_PKT_NO_L4_CSUM_SHIFT)

#define _NETIO_PKT_NO_L3_CSUM_SHIFT           1
#define _NETIO_PKT_NO_L3_CSUM_RMASK           1
#define _NETIO_PKT_NO_L3_CSUM_MASK \
         (_NETIO_PKT_NO_L3_CSUM_RMASK << _NETIO_PKT_NO_L3_CSUM_SHIFT)

#define _NETIO_PKT_BAD_L3_CSUM_SHIFT          2
#define _NETIO_PKT_BAD_L3_CSUM_RMASK          1
#define _NETIO_PKT_BAD_L3_CSUM_MASK \
         (_NETIO_PKT_BAD_L3_CSUM_RMASK << _NETIO_PKT_BAD_L3_CSUM_SHIFT)

#define _NETIO_PKT_TYPE_UNRECOGNIZED_SHIFT    3
#define _NETIO_PKT_TYPE_UNRECOGNIZED_RMASK    1
#define _NETIO_PKT_TYPE_UNRECOGNIZED_MASK \
         (_NETIO_PKT_TYPE_UNRECOGNIZED_RMASK << \
          _NETIO_PKT_TYPE_UNRECOGNIZED_SHIFT)


#define _NETIO_PKT_TYPE_SHIFT        4
#define _NETIO_PKT_TYPE_RMASK        0x3F

#define _NETIO_PKT_VLAN_SHIFT        4
#define _NETIO_PKT_VLAN_RMASK        0x3
#define _NETIO_PKT_VLAN_MASK \
         (_NETIO_PKT_VLAN_RMASK << _NETIO_PKT_VLAN_SHIFT)
#define _NETIO_PKT_VLAN_NONE         0   
#define _NETIO_PKT_VLAN_ONE          1   
#define _NETIO_PKT_VLAN_TWO_OUTER    2   
#define _NETIO_PKT_VLAN_TWO_INNER    3   

#define _NETIO_PKT_TAG_SHIFT         6
#define _NETIO_PKT_TAG_RMASK         0x3
#define _NETIO_PKT_TAG_MASK \
          (_NETIO_PKT_TAG_RMASK << _NETIO_PKT_TAG_SHIFT)
#define _NETIO_PKT_TAG_NONE          0   
#define _NETIO_PKT_TAG_MRVL          1   
#define _NETIO_PKT_TAG_MRVL_EXT      2   
#define _NETIO_PKT_TAG_BRCM          3   

#define _NETIO_PKT_SNAP_SHIFT        8
#define _NETIO_PKT_SNAP_RMASK        0x1
#define _NETIO_PKT_SNAP_MASK \
          (_NETIO_PKT_SNAP_RMASK << _NETIO_PKT_SNAP_SHIFT)


#define _NETIO_PKT_CUSTOM_LEN_SHIFT  11
#define _NETIO_PKT_CUSTOM_LEN_RMASK  0x1F
#define _NETIO_PKT_CUSTOM_LEN_MASK \
          (_NETIO_PKT_CUSTOM_LEN_RMASK << _NETIO_PKT_CUSTOM_LEN_SHIFT)

#define _NETIO_PKT_BAD_L4_CSUM_SHIFT 16
#define _NETIO_PKT_BAD_L4_CSUM_RMASK 0x1
#define _NETIO_PKT_BAD_L4_CSUM_MASK \
          (_NETIO_PKT_BAD_L4_CSUM_RMASK << _NETIO_PKT_BAD_L4_CSUM_SHIFT)

#define _NETIO_PKT_L2_LEN_SHIFT  17
#define _NETIO_PKT_L2_LEN_RMASK  0x1F
#define _NETIO_PKT_L2_LEN_MASK \
          (_NETIO_PKT_L2_LEN_RMASK << _NETIO_PKT_L2_LEN_SHIFT)



#define _NETIO_PKT_NEED_EDMA_CSUM_SHIFT            0
#define _NETIO_PKT_NEED_EDMA_CSUM_RMASK            1
#define _NETIO_PKT_NEED_EDMA_CSUM_MASK \
         (_NETIO_PKT_NEED_EDMA_CSUM_RMASK << _NETIO_PKT_NEED_EDMA_CSUM_SHIFT)



#define _NETIO_PKT_INFO_ETYPE_SHIFT       6
#define _NETIO_PKT_INFO_ETYPE_RMASK    0x1F

#define _NETIO_PKT_INFO_VLAN_SHIFT       11
#define _NETIO_PKT_INFO_VLAN_RMASK     0x1F

#endif


#define SMALL_PACKET_SIZE 256

#define LARGE_PACKET_SIZE 2048

#define JUMBO_PACKET_SIZE (12 * 1024)


#define ETHERTYPE_IPv4 (0x0800)
#define ETHERTYPE_ARP (0x0806)
#define ETHERTYPE_VLAN (0x8100)
#define ETHERTYPE_Q_IN_Q (0x9100)
#define ETHERTYPE_IPv6 (0x86DD)
#define ETHERTYPE_MPLS (0x8847)


typedef enum
{
  
  NETIO_PKT_STATUS_OK,
  NETIO_PKT_STATUS_UNDERSIZE,
  NETIO_PKT_STATUS_OVERSIZE,
  NETIO_PKT_STATUS_BAD
} netio_pkt_status_t;


#define NETIO_LOG2_NUM_BUCKETS (10)

#define NETIO_NUM_BUCKETS (1 << NETIO_LOG2_NUM_BUCKETS)


typedef union {
  
  struct {
    
    unsigned int __balance_on_l4:1;
    
    unsigned int __balance_on_l3:1;
    
    unsigned int __balance_on_l2:1;
    
    unsigned int __reserved:1;
    
    unsigned int __bucket_base:NETIO_LOG2_NUM_BUCKETS;
    unsigned int __bucket_mask:NETIO_LOG2_NUM_BUCKETS;
    
    unsigned int __padding:(32 - 4 - 2 * NETIO_LOG2_NUM_BUCKETS);
  } bits;
  
  unsigned int word;
}
netio_group_t;


typedef netio_group_t netio_vlan_t;


typedef unsigned char netio_bucket_t;


typedef unsigned int netio_size_t;


typedef struct
{
#ifdef __DOXYGEN__
  
  unsigned char opaque[24];
#else
  
  unsigned int __packet_ordinal;
  
  unsigned int __group_ordinal;
  
  unsigned int __flow_hash;
  
  unsigned int __flags;
  
  unsigned int __user_data_0;
  
  unsigned int __user_data_1;
#endif
}
netio_pkt_metadata_t;


#define NETIO_PACKET_PADDING 2



typedef struct
{
#ifdef __DOXYGEN__
  
  unsigned char opaque[14];
#else
  
  unsigned short l2_offset;
  
  unsigned short l3_offset;
  
  unsigned char csum_location;
  
  unsigned char csum_start;
  
  unsigned short flags;
  
  unsigned short l2_length;
  
  unsigned short csum_seed;
  
  unsigned short csum_length;
#endif
}
netio_pkt_minimal_metadata_t;


#ifndef __DOXYGEN__

typedef union
{
  unsigned int word; 
  
  struct
  {
    unsigned int __channel:7;    
    unsigned int __type:4;       
    unsigned int __ack:1;        
    unsigned int __reserved:1;   
    unsigned int __protocol:1;   
    unsigned int __status:2;     
    unsigned int __framing:2;    
    unsigned int __transfer_size:14; 
  } bits;
}
__netio_pkt_notif_t;


#define _NETIO_PKT_HANDLE_BASE(p) \
  ((unsigned char*)((p).word & 0xFFFFFFC0))

#define _NETIO_PKT_BASE(p) \
  _NETIO_PKT_HANDLE_BASE(p->__packet)

typedef union
{
  unsigned int word; 
  
  struct
  {
    unsigned int __queue:2;

    
    unsigned int __ipp_handle:2;

    
    unsigned int __reserved:1;

    unsigned int __minimal:1;

    unsigned int __offset:2;

    
    unsigned int __addr:24;
  } bits;
}
__netio_pkt_handle_t;

#endif 


typedef struct
{
  unsigned int word; 
} netio_pkt_handle_t;

typedef struct
{
#ifdef __DOXYGEN__
  
  unsigned char opaque[32];
#else
  __netio_pkt_notif_t __notif_header;

  
  __netio_pkt_handle_t __packet;

  
  netio_pkt_metadata_t __metadata;
#endif
}
netio_pkt_t;


#ifndef __DOXYGEN__

#define __NETIO_PKT_NOTIF_HEADER(pkt) ((pkt)->__notif_header)
#define __NETIO_PKT_IPP_HANDLE(pkt) ((pkt)->__packet.bits.__ipp_handle)
#define __NETIO_PKT_QUEUE(pkt) ((pkt)->__packet.bits.__queue)
#define __NETIO_PKT_NOTIF_HEADER_M(mda, pkt) ((pkt)->__notif_header)
#define __NETIO_PKT_IPP_HANDLE_M(mda, pkt) ((pkt)->__packet.bits.__ipp_handle)
#define __NETIO_PKT_MINIMAL(pkt) ((pkt)->__packet.bits.__minimal)
#define __NETIO_PKT_QUEUE_M(mda, pkt) ((pkt)->__packet.bits.__queue)
#define __NETIO_PKT_FLAGS_M(mda, pkt) ((mda)->__flags)

extern const uint16_t _netio_pkt_info[];

#endif 


#ifndef __DOXYGEN__
#define NETIO_PKT_GOOD_CHECKSUM(pkt) \
  NETIO_PKT_L4_CSUM_CORRECT(pkt)
#define NETIO_PKT_GOOD_CHECKSUM_M(mda, pkt) \
  NETIO_PKT_L4_CSUM_CORRECT_M(mda, pkt)
#endif 



static __inline netio_pkt_metadata_t*
NETIO_PKT_METADATA(netio_pkt_t* pkt)
{
  netio_assert(!pkt->__packet.bits.__minimal);
  return &pkt->__metadata;
}


static __inline netio_pkt_minimal_metadata_t*
NETIO_PKT_MINIMAL_METADATA(netio_pkt_t* pkt)
{
  netio_assert(pkt->__packet.bits.__minimal);
  return (netio_pkt_minimal_metadata_t*) &pkt->__metadata;
}


static __inline unsigned int
NETIO_PKT_IS_MINIMAL(netio_pkt_t* pkt)
{
  return pkt->__packet.bits.__minimal;
}


static __inline netio_pkt_handle_t
NETIO_PKT_HANDLE(netio_pkt_t* pkt)
{
  netio_pkt_handle_t h;
  h.word = pkt->__packet.word;
  return h;
}


#define NETIO_PKT_HANDLE_NONE ((netio_pkt_handle_t) { 0 })


static __inline unsigned int
NETIO_PKT_HANDLE_IS_VALID(netio_pkt_handle_t handle)
{
  return handle.word != 0;
}



static __inline unsigned char*
NETIO_PKT_CUSTOM_DATA_H(netio_pkt_handle_t handle)
{
  return _NETIO_PKT_HANDLE_BASE(handle) + NETIO_PACKET_PADDING;
}


static __inline netio_size_t
NETIO_PKT_CUSTOM_HEADER_LENGTH_M(netio_pkt_metadata_t* mda, netio_pkt_t* pkt)
{
  return ((mda->__flags >> (_NETIO_PKT_CUSTOM_LEN_SHIFT - 2)) &
          (_NETIO_PKT_CUSTOM_LEN_RMASK << 2));
}


static __inline netio_size_t
NETIO_PKT_CUSTOM_LENGTH_M(netio_pkt_metadata_t* mda, netio_pkt_t* pkt)
{
  return (__NETIO_PKT_NOTIF_HEADER(pkt).bits.__transfer_size -
          NETIO_PACKET_PADDING);
}


static __inline unsigned char*
NETIO_PKT_CUSTOM_DATA_M(netio_pkt_metadata_t* mda, netio_pkt_t* pkt)
{
  return NETIO_PKT_CUSTOM_DATA_H(NETIO_PKT_HANDLE(pkt));
}


static __inline netio_size_t
NETIO_PKT_L2_HEADER_LENGTH_M(netio_pkt_metadata_t* mda, netio_pkt_t* pkt)
{
  return ((mda->__flags >> (_NETIO_PKT_L2_LEN_SHIFT - 2)) &
          (_NETIO_PKT_L2_LEN_RMASK << 2)) + 2;
}


static __inline netio_size_t
NETIO_PKT_L2_LENGTH_M(netio_pkt_metadata_t* mda, netio_pkt_t* pkt)
{
  return (NETIO_PKT_CUSTOM_LENGTH_M(mda, pkt) -
          NETIO_PKT_CUSTOM_HEADER_LENGTH_M(mda,pkt));
}


static __inline unsigned char*
NETIO_PKT_L2_DATA_M(netio_pkt_metadata_t* mda, netio_pkt_t* pkt)
{
  return (NETIO_PKT_CUSTOM_DATA_M(mda, pkt) +
          NETIO_PKT_CUSTOM_HEADER_LENGTH_M(mda, pkt));
}


static __inline netio_size_t
NETIO_PKT_L3_LENGTH_M(netio_pkt_metadata_t* mda, netio_pkt_t* pkt)
{
  return (NETIO_PKT_L2_LENGTH_M(mda, pkt) -
          NETIO_PKT_L2_HEADER_LENGTH_M(mda,pkt));
}


static __inline unsigned char*
NETIO_PKT_L3_DATA_M(netio_pkt_metadata_t* mda, netio_pkt_t* pkt)
{
  return (NETIO_PKT_L2_DATA_M(mda, pkt) +
          NETIO_PKT_L2_HEADER_LENGTH_M(mda, pkt));
}


static __inline unsigned int
NETIO_PKT_ORDINAL_M(netio_pkt_metadata_t* mda, netio_pkt_t* pkt)
{
  return mda->__packet_ordinal;
}


static __inline unsigned int
NETIO_PKT_GROUP_ORDINAL_M(netio_pkt_metadata_t* mda, netio_pkt_t* pkt)
{
  return mda->__group_ordinal;
}


static __inline unsigned short
NETIO_PKT_VLAN_ID_M(netio_pkt_metadata_t* mda, netio_pkt_t* pkt)
{
  int vl = (mda->__flags >> _NETIO_PKT_VLAN_SHIFT) & _NETIO_PKT_VLAN_RMASK;
  unsigned short* pkt_p;
  int index;
  unsigned short val;

  if (vl == _NETIO_PKT_VLAN_NONE)
    return 0;

  pkt_p = (unsigned short*) NETIO_PKT_L2_DATA_M(mda, pkt);
  index = (mda->__flags >> _NETIO_PKT_TYPE_SHIFT) & _NETIO_PKT_TYPE_RMASK;

  val = pkt_p[(_netio_pkt_info[index] >> _NETIO_PKT_INFO_VLAN_SHIFT) &
              _NETIO_PKT_INFO_VLAN_RMASK];

#ifdef __TILECC__
  return (__insn_bytex(val) >> 16) & 0xFFF;
#else
  return (__builtin_bswap32(val) >> 16) & 0xFFF;
#endif
}


static __inline unsigned short
NETIO_PKT_ETHERTYPE_M(netio_pkt_metadata_t* mda, netio_pkt_t* pkt)
{
  unsigned short* pkt_p = (unsigned short*) NETIO_PKT_L2_DATA_M(mda, pkt);
  int index = (mda->__flags >> _NETIO_PKT_TYPE_SHIFT) & _NETIO_PKT_TYPE_RMASK;

  unsigned short val =
    pkt_p[(_netio_pkt_info[index] >> _NETIO_PKT_INFO_ETYPE_SHIFT) &
          _NETIO_PKT_INFO_ETYPE_RMASK];

  return __builtin_bswap32(val) >> 16;
}


static __inline unsigned int
NETIO_PKT_FLOW_HASH_M(netio_pkt_metadata_t* mda, netio_pkt_t* pkt)
{
  return mda->__flow_hash;
}


static __inline unsigned int
NETIO_PKT_USER_DATA_0_M(netio_pkt_metadata_t* mda, netio_pkt_t* pkt)
{
  return mda->__user_data_0;
}


static __inline unsigned int
NETIO_PKT_USER_DATA_1_M(netio_pkt_metadata_t* mda, netio_pkt_t* pkt)
{
  return mda->__user_data_1;
}


static __inline unsigned int
NETIO_PKT_L4_CSUM_CALCULATED_M(netio_pkt_metadata_t* mda, netio_pkt_t* pkt)
{
  return !(mda->__flags & _NETIO_PKT_NO_L4_CSUM_MASK);
}


static __inline unsigned int
NETIO_PKT_L4_CSUM_CORRECT_M(netio_pkt_metadata_t* mda, netio_pkt_t* pkt)
{
  return !(mda->__flags &
           (_NETIO_PKT_BAD_L4_CSUM_MASK | _NETIO_PKT_NO_L4_CSUM_MASK));
}


static __inline unsigned int
NETIO_PKT_L3_CSUM_CALCULATED_M(netio_pkt_metadata_t* mda, netio_pkt_t* pkt)
{
  return !(mda->__flags & _NETIO_PKT_NO_L3_CSUM_MASK);
}


static __inline unsigned int
NETIO_PKT_L3_CSUM_CORRECT_M(netio_pkt_metadata_t* mda, netio_pkt_t* pkt)
{
  return !(mda->__flags &
           (_NETIO_PKT_BAD_L3_CSUM_MASK | _NETIO_PKT_NO_L3_CSUM_MASK));
}


static __inline unsigned int
NETIO_PKT_ETHERTYPE_RECOGNIZED_M(netio_pkt_metadata_t* mda, netio_pkt_t* pkt)
{
  return !(mda->__flags & _NETIO_PKT_TYPE_UNRECOGNIZED_MASK);
}


static __inline netio_pkt_status_t
NETIO_PKT_STATUS_M(netio_pkt_metadata_t* mda, netio_pkt_t* pkt)
{
  return (netio_pkt_status_t) __NETIO_PKT_NOTIF_HEADER(pkt).bits.__status;
}


static __inline unsigned int
NETIO_PKT_BAD_M(netio_pkt_metadata_t* mda, netio_pkt_t* pkt)
{
  return ((NETIO_PKT_STATUS_M(mda, pkt) & 1) &&
          (NETIO_PKT_ETHERTYPE_RECOGNIZED_M(mda, pkt) ||
           NETIO_PKT_STATUS_M(mda, pkt) == NETIO_PKT_STATUS_BAD));
}


static __inline netio_size_t
NETIO_PKT_L2_LENGTH_MM(netio_pkt_minimal_metadata_t* mmd, netio_pkt_t* pkt)
{
  return mmd->l2_length;
}


static __inline netio_size_t
NETIO_PKT_L2_HEADER_LENGTH_MM(netio_pkt_minimal_metadata_t* mmd,
                              netio_pkt_t* pkt)
{
  return mmd->l3_offset - mmd->l2_offset;
}


static __inline netio_size_t
NETIO_PKT_L3_LENGTH_MM(netio_pkt_minimal_metadata_t* mmd, netio_pkt_t* pkt)
{
  return (NETIO_PKT_L2_LENGTH_MM(mmd, pkt) -
          NETIO_PKT_L2_HEADER_LENGTH_MM(mmd, pkt));
}


static __inline unsigned char*
NETIO_PKT_L3_DATA_MM(netio_pkt_minimal_metadata_t* mmd, netio_pkt_t* pkt)
{
  return _NETIO_PKT_BASE(pkt) + mmd->l3_offset;
}


static __inline unsigned char*
NETIO_PKT_L2_DATA_MM(netio_pkt_minimal_metadata_t* mmd, netio_pkt_t* pkt)
{
  return _NETIO_PKT_BASE(pkt) + mmd->l2_offset;
}


static __inline netio_pkt_status_t
NETIO_PKT_STATUS(netio_pkt_t* pkt)
{
  netio_assert(!pkt->__packet.bits.__minimal);

  return (netio_pkt_status_t) __NETIO_PKT_NOTIF_HEADER(pkt).bits.__status;
}


static __inline unsigned int
NETIO_PKT_BAD(netio_pkt_t* pkt)
{
  netio_pkt_metadata_t* mda = NETIO_PKT_METADATA(pkt);

  return NETIO_PKT_BAD_M(mda, pkt);
}


static __inline netio_size_t
NETIO_PKT_CUSTOM_HEADER_LENGTH(netio_pkt_t* pkt)
{
  netio_pkt_metadata_t* mda = NETIO_PKT_METADATA(pkt);

  return NETIO_PKT_CUSTOM_HEADER_LENGTH_M(mda, pkt);
}


static __inline netio_size_t
NETIO_PKT_CUSTOM_LENGTH(netio_pkt_t* pkt)
{
  netio_pkt_metadata_t* mda = NETIO_PKT_METADATA(pkt);

  return NETIO_PKT_CUSTOM_LENGTH_M(mda, pkt);
}


static __inline unsigned char*
NETIO_PKT_CUSTOM_DATA(netio_pkt_t* pkt)
{
  netio_pkt_metadata_t* mda = NETIO_PKT_METADATA(pkt);

  return NETIO_PKT_CUSTOM_DATA_M(mda, pkt);
}


static __inline netio_size_t
NETIO_PKT_L2_HEADER_LENGTH(netio_pkt_t* pkt)
{
  if (NETIO_PKT_IS_MINIMAL(pkt))
  {
    netio_pkt_minimal_metadata_t* mmd = NETIO_PKT_MINIMAL_METADATA(pkt);

    return NETIO_PKT_L2_HEADER_LENGTH_MM(mmd, pkt);
  }
  else
  {
    netio_pkt_metadata_t* mda = NETIO_PKT_METADATA(pkt);

    return NETIO_PKT_L2_HEADER_LENGTH_M(mda, pkt);
  }
}


static __inline netio_size_t
NETIO_PKT_L2_LENGTH(netio_pkt_t* pkt)
{
  if (NETIO_PKT_IS_MINIMAL(pkt))
  {
    netio_pkt_minimal_metadata_t* mmd = NETIO_PKT_MINIMAL_METADATA(pkt);

    return NETIO_PKT_L2_LENGTH_MM(mmd, pkt);
  }
  else
  {
    netio_pkt_metadata_t* mda = NETIO_PKT_METADATA(pkt);

    return NETIO_PKT_L2_LENGTH_M(mda, pkt);
  }
}


static __inline unsigned char*
NETIO_PKT_L2_DATA(netio_pkt_t* pkt)
{
  if (NETIO_PKT_IS_MINIMAL(pkt))
  {
    netio_pkt_minimal_metadata_t* mmd = NETIO_PKT_MINIMAL_METADATA(pkt);

    return NETIO_PKT_L2_DATA_MM(mmd, pkt);
  }
  else
  {
    netio_pkt_metadata_t* mda = NETIO_PKT_METADATA(pkt);

    return NETIO_PKT_L2_DATA_M(mda, pkt);
  }
}


static __inline netio_size_t
NETIO_PKT_L3_LENGTH(netio_pkt_t* pkt)
{
  if (NETIO_PKT_IS_MINIMAL(pkt))
  {
    netio_pkt_minimal_metadata_t* mmd = NETIO_PKT_MINIMAL_METADATA(pkt);

    return NETIO_PKT_L3_LENGTH_MM(mmd, pkt);
  }
  else
  {
    netio_pkt_metadata_t* mda = NETIO_PKT_METADATA(pkt);

    return NETIO_PKT_L3_LENGTH_M(mda, pkt);
  }
}


static __inline unsigned char*
NETIO_PKT_L3_DATA(netio_pkt_t* pkt)
{
  if (NETIO_PKT_IS_MINIMAL(pkt))
  {
    netio_pkt_minimal_metadata_t* mmd = NETIO_PKT_MINIMAL_METADATA(pkt);

    return NETIO_PKT_L3_DATA_MM(mmd, pkt);
  }
  else
  {
    netio_pkt_metadata_t* mda = NETIO_PKT_METADATA(pkt);

    return NETIO_PKT_L3_DATA_M(mda, pkt);
  }
}


static __inline unsigned int
NETIO_PKT_ORDINAL(netio_pkt_t* pkt)
{
  netio_pkt_metadata_t* mda = NETIO_PKT_METADATA(pkt);

  return NETIO_PKT_ORDINAL_M(mda, pkt);
}


static __inline unsigned int
NETIO_PKT_GROUP_ORDINAL(netio_pkt_t* pkt)
{
  netio_pkt_metadata_t* mda = NETIO_PKT_METADATA(pkt);

  return NETIO_PKT_GROUP_ORDINAL_M(mda, pkt);
}


static __inline unsigned short
NETIO_PKT_VLAN_ID(netio_pkt_t* pkt)
{
  netio_pkt_metadata_t* mda = NETIO_PKT_METADATA(pkt);

  return NETIO_PKT_VLAN_ID_M(mda, pkt);
}


static __inline unsigned short
NETIO_PKT_ETHERTYPE(netio_pkt_t* pkt)
{
  netio_pkt_metadata_t* mda = NETIO_PKT_METADATA(pkt);

  return NETIO_PKT_ETHERTYPE_M(mda, pkt);
}


static __inline unsigned int
NETIO_PKT_FLOW_HASH(netio_pkt_t* pkt)
{
  netio_pkt_metadata_t* mda = NETIO_PKT_METADATA(pkt);

  return NETIO_PKT_FLOW_HASH_M(mda, pkt);
}


static __inline unsigned int
NETIO_PKT_USER_DATA_0(netio_pkt_t* pkt)
{
  netio_pkt_metadata_t* mda = NETIO_PKT_METADATA(pkt);

  return NETIO_PKT_USER_DATA_0_M(mda, pkt);
}


static __inline unsigned int
NETIO_PKT_USER_DATA_1(netio_pkt_t* pkt)
{
  netio_pkt_metadata_t* mda = NETIO_PKT_METADATA(pkt);

  return NETIO_PKT_USER_DATA_1_M(mda, pkt);
}


static __inline unsigned int
NETIO_PKT_L4_CSUM_CALCULATED(netio_pkt_t* pkt)
{
  netio_pkt_metadata_t* mda = NETIO_PKT_METADATA(pkt);

  return NETIO_PKT_L4_CSUM_CALCULATED_M(mda, pkt);
}


static __inline unsigned int
NETIO_PKT_L4_CSUM_CORRECT(netio_pkt_t* pkt)
{
  netio_pkt_metadata_t* mda = NETIO_PKT_METADATA(pkt);

  return NETIO_PKT_L4_CSUM_CORRECT_M(mda, pkt);
}


static __inline unsigned int
NETIO_PKT_L3_CSUM_CALCULATED(netio_pkt_t* pkt)
{
  netio_pkt_metadata_t* mda = NETIO_PKT_METADATA(pkt);

  return NETIO_PKT_L3_CSUM_CALCULATED_M(mda, pkt);
}


static __inline unsigned int
NETIO_PKT_L3_CSUM_CORRECT(netio_pkt_t* pkt)
{
  netio_pkt_metadata_t* mda = NETIO_PKT_METADATA(pkt);

  return NETIO_PKT_L3_CSUM_CORRECT_M(mda, pkt);
}


static __inline unsigned int
NETIO_PKT_ETHERTYPE_RECOGNIZED(netio_pkt_t* pkt)
{
  netio_pkt_metadata_t* mda = NETIO_PKT_METADATA(pkt);

  return NETIO_PKT_ETHERTYPE_RECOGNIZED_M(mda, pkt);
}


static __inline void
NETIO_PKT_SET_L2_LENGTH_MM(netio_pkt_minimal_metadata_t* mmd, netio_pkt_t* pkt,
                           int len)
{
  mmd->l2_length = len;
}


static __inline void
NETIO_PKT_SET_L2_LENGTH(netio_pkt_t* pkt, int len)
{
  netio_pkt_minimal_metadata_t* mmd = NETIO_PKT_MINIMAL_METADATA(pkt);

  NETIO_PKT_SET_L2_LENGTH_MM(mmd, pkt, len);
}


static __inline void
NETIO_PKT_SET_L2_HEADER_LENGTH_MM(netio_pkt_minimal_metadata_t* mmd,
                                  netio_pkt_t* pkt, int len)
{
  mmd->l3_offset = mmd->l2_offset + len;
}


static __inline void
NETIO_PKT_SET_L2_HEADER_LENGTH(netio_pkt_t* pkt, int len)
{
  netio_pkt_minimal_metadata_t* mmd = NETIO_PKT_MINIMAL_METADATA(pkt);

  NETIO_PKT_SET_L2_HEADER_LENGTH_MM(mmd, pkt, len);
}


/** Set up an egress packet for hardware checksum computation, using a
 *  metadata pointer to speed the operation.
 * @ingroup egress
 *
 *  NetIO provides the ability to automatically calculate a standard
 *  16-bit Internet checksum on transmitted packets.  The application
 *  may specify the point in the packet where the checksum starts, the
 *  number of bytes to be checksummed, and the two bytes in the packet
 *  which will be replaced with the completed checksum.  (If the range
 *  of bytes to be checksummed includes the bytes to be replaced, the
 *  initial values of those bytes will be included in the checksum.)
 *
 *  For some protocols, the packet checksum covers data which is not present
 *  in the packet, or is at least not contiguous to the main data payload.
 *  For instance, the TCP checksum includes a "pseudo-header" which includes
 *  the source and destination IP addresses of the packet.  To accommodate
 *  this, the checksum engine may be "seeded" with an initial value, which
 *  the application would need to compute based on the specific protocol's
 *  requirements.  Note that the seed is given in host byte order (little-
 *  endian), not network byte order (big-endian); code written to compute a
 *  pseudo-header checksum in network byte order will need to byte-swap it
 *  before use as the seed.
 *
 *  Note that the checksum is computed as part of the transmission process,
 *  so it will not be present in the packet upon completion of this routine.
 *
 * @param[in,out] mmd Pointer to packet's minimal metadata.
 * @param[in] pkt Packet on which to operate.
 * @param[in] start Offset within L2 packet of the first byte to include in
 *   the checksum.
 * @param[in] length Number of bytes to include in the checksum.
 *   the checksum.
 * @param[in] location Offset within L2 packet of the first of the two bytes
 *   to be replaced with the calculated checksum.
 * @param[in] seed Initial value of the running checksum before any of the
 *   packet data is added.
 */
static __inline void
NETIO_PKT_DO_EGRESS_CSUM_MM(netio_pkt_minimal_metadata_t* mmd,
                            netio_pkt_t* pkt, int start, int length,
                            int location, uint16_t seed)
{
  mmd->csum_start = start;
  mmd->csum_length = length;
  mmd->csum_location = location;
  mmd->csum_seed = seed;
  mmd->flags |= _NETIO_PKT_NEED_EDMA_CSUM_MASK;
}


/** Set up an egress packet for hardware checksum computation.
 * @ingroup egress
 *
 *  NetIO provides the ability to automatically calculate a standard
 *  16-bit Internet checksum on transmitted packets.  The application
 *  may specify the point in the packet where the checksum starts, the
 *  number of bytes to be checksummed, and the two bytes in the packet
 *  which will be replaced with the completed checksum.  (If the range
 *  of bytes to be checksummed includes the bytes to be replaced, the
 *  initial values of those bytes will be included in the checksum.)
 *
 *  For some protocols, the packet checksum covers data which is not present
 *  in the packet, or is at least not contiguous to the main data payload.
 *  For instance, the TCP checksum includes a "pseudo-header" which includes
 *  the source and destination IP addresses of the packet.  To accommodate
 *  this, the checksum engine may be "seeded" with an initial value, which
 *  the application would need to compute based on the specific protocol's
 *  requirements.  Note that the seed is given in host byte order (little-
 *  endian), not network byte order (big-endian); code written to compute a
 *  pseudo-header checksum in network byte order will need to byte-swap it
 *  before use as the seed.
 *
 *  Note that the checksum is computed as part of the transmission process,
 *  so it will not be present in the packet upon completion of this routine.
 *
 * @param[in,out] pkt Packet on which to operate.
 * @param[in] start Offset within L2 packet of the first byte to include in
 *   the checksum.
 * @param[in] length Number of bytes to include in the checksum.
 *   the checksum.
 * @param[in] location Offset within L2 packet of the first of the two bytes
 *   to be replaced with the calculated checksum.
 * @param[in] seed Initial value of the running checksum before any of the
 *   packet data is added.
 */
static __inline void
NETIO_PKT_DO_EGRESS_CSUM(netio_pkt_t* pkt, int start, int length,
                         int location, uint16_t seed)
{
  netio_pkt_minimal_metadata_t* mmd = NETIO_PKT_MINIMAL_METADATA(pkt);

  NETIO_PKT_DO_EGRESS_CSUM_MM(mmd, pkt, start, length, location, seed);
}


static __inline int
NETIO_PKT_PREPEND_AVAIL_M(netio_pkt_metadata_t* mda, netio_pkt_t* pkt)
{
  return (pkt->__packet.bits.__offset << 6) +
         NETIO_PKT_CUSTOM_HEADER_LENGTH_M(mda, pkt);
}


static __inline int
NETIO_PKT_PREPEND_AVAIL_MM(netio_pkt_minimal_metadata_t* mmd, netio_pkt_t* pkt)
{
  return (pkt->__packet.bits.__offset << 6) + mmd->l2_offset;
}


static __inline int
NETIO_PKT_PREPEND_AVAIL(netio_pkt_t* pkt)
{
  if (NETIO_PKT_IS_MINIMAL(pkt))
  {
    netio_pkt_minimal_metadata_t* mmd = NETIO_PKT_MINIMAL_METADATA(pkt);

    return NETIO_PKT_PREPEND_AVAIL_MM(mmd, pkt);
  }
  else
  {
    netio_pkt_metadata_t* mda = NETIO_PKT_METADATA(pkt);

    return NETIO_PKT_PREPEND_AVAIL_M(mda, pkt);
  }
}


static __inline void
NETIO_PKT_FLUSH_MINIMAL_METADATA_MM(netio_pkt_minimal_metadata_t* mmd,
                                    netio_pkt_t* pkt)
{
}


static __inline void
NETIO_PKT_INV_MINIMAL_METADATA_MM(netio_pkt_minimal_metadata_t* mmd,
                                  netio_pkt_t* pkt)
{
}


static __inline void
NETIO_PKT_FLUSH_INV_MINIMAL_METADATA_MM(netio_pkt_minimal_metadata_t* mmd,
                                        netio_pkt_t* pkt)
{
}


static __inline void
NETIO_PKT_FLUSH_METADATA_M(netio_pkt_metadata_t* mda, netio_pkt_t* pkt)
{
}


static __inline void
NETIO_PKT_INV_METADATA_M(netio_pkt_metadata_t* mda, netio_pkt_t* pkt)
{
}


static __inline void
NETIO_PKT_FLUSH_INV_METADATA_M(netio_pkt_metadata_t* mda, netio_pkt_t* pkt)
{
}


static __inline void
NETIO_PKT_FLUSH_MINIMAL_METADATA(netio_pkt_t* pkt)
{
}


static __inline void
NETIO_PKT_INV_MINIMAL_METADATA(netio_pkt_t* pkt)
{
}


static __inline void
NETIO_PKT_FLUSH_INV_MINIMAL_METADATA(netio_pkt_t* pkt)
{
}


static __inline void
NETIO_PKT_FLUSH_METADATA(netio_pkt_t* pkt)
{
}


static __inline void
NETIO_PKT_INV_METADATA(netio_pkt_t* pkt)
{
}


static __inline void
NETIO_PKT_FLUSH_INV_METADATA(netio_pkt_t* pkt)
{
}

#define NETIO_NUM_NODE_WEIGHTS  16

typedef struct
{
  /** Registration characteristics.

      This value determines several characteristics of the registration;
      flags for different types of behavior are ORed together to make the
      final flag value.  Generally applications should specify exactly
      one flag from each of the following categories:

      - Whether the application will be receiving packets on this queue
        (::NETIO_RECV or ::NETIO_NO_RECV).

      - Whether the application will be transmitting packets on this queue,
        and if so, whether it will request egress checksum calculation
        (::NETIO_XMIT, ::NETIO_XMIT_CSUM, or ::NETIO_NO_XMIT).  It is
        legal to call netio_get_buffer() without one of the XMIT flags,
        as long as ::NETIO_RECV is specified; in this case, the retrieved
        buffers must be passed to another tile for transmission.

      - Whether the application expects any vendor-specific tags in
        its packets' L2 headers (::NETIO_TAG_NONE, ::NETIO_TAG_BRCM,
        or ::NETIO_TAG_MRVL).  This must match the configuration of the
        target IPP.

      To accommodate applications written to previous versions of the NetIO
      interface, none of the flags above are currently required; if omitted,
      NetIO behaves more or less as if ::NETIO_RECV | ::NETIO_XMIT_CSUM |
      ::NETIO_TAG_NONE were used.  However, explicit specification of
      the relevant flags allows NetIO to do a better job of resource
      allocation, allows earlier detection of certain configuration errors,
      and may enable advanced features or higher performance in the future,
      so their use is strongly recommended.

      Note that specifying ::NETIO_NO_RECV along with ::NETIO_NO_XMIT
      is a special case, intended primarily for use by programs which
      retrieve network statistics or do link management operations.
      When these flags are both specified, the resulting queue may not
      be used with NetIO routines other than netio_get(), netio_set(),
      and netio_input_unregister().  See @ref link for more information
      on link management.

      Other flags are optional; their use is described below.
  */
  int flags;

  const char* interface;

  int num_receive_packets;

  unsigned int queue_id;

  int num_send_buffers_small_total;

  int num_send_buffers_small_prealloc;

  int num_send_buffers_large_total;

  int num_send_buffers_large_prealloc;

  int num_send_buffers_jumbo_total;

  int num_send_buffers_jumbo_prealloc;

  uint64_t total_buffer_size;

  uint8_t buffer_node_weights[NETIO_NUM_NODE_WEIGHTS];

  void* fixed_buffer_va;

  int num_sends_outstanding;
}
netio_input_config_t;



#define NETIO_STRICT_HOMING   0x00000002

#define NETIO_TAG_NONE        0x00000004

#define NETIO_TAG_MRVL        0x00000008

#define NETIO_TAG_BRCM        0x00000010

#define NETIO_RECV            0x00000020

#define NETIO_NO_RECV         0x00000040

#define NETIO_XMIT            0x00000080

#define NETIO_XMIT_CSUM       0x00000100

#define NETIO_NO_XMIT         0x00000200

#define NETIO_FIXED_BUFFER_VA 0x00000400

#define NETIO_REQUIRE_LINK_UP    0x00000800

#define NETIO_NOREQUIRE_LINK_UP  0x00001000

#ifndef __DOXYGEN__
#define _NETIO_AUTO_UP        0x00002000
#define _NETIO_AUTO_DN        0x00004000
#define _NETIO_AUTO_PRESENT   0x00008000
#endif

#define NETIO_AUTO_LINK_UP     (_NETIO_AUTO_PRESENT | _NETIO_AUTO_UP)

#define NETIO_AUTO_LINK_UPDN   (_NETIO_AUTO_PRESENT | _NETIO_AUTO_UP | \
                                _NETIO_AUTO_DN)

#define NETIO_AUTO_LINK_DN     (_NETIO_AUTO_PRESENT | _NETIO_AUTO_DN)

#define NETIO_AUTO_LINK_NONE   _NETIO_AUTO_PRESENT


#define NETIO_MIN_RECEIVE_PKTS            16

#define NETIO_MAX_RECEIVE_PKTS           128

#define NETIO_MAX_SEND_BUFFERS            16

#define NETIO_TOTAL_SENDS_OUTSTANDING   2015

#define NETIO_MIN_SENDS_OUTSTANDING       16



#ifndef __DOXYGEN__

struct __netio_queue_impl_t;

struct __netio_queue_user_impl_t;

#endif 


typedef struct
{
#ifdef __DOXYGEN__
  uint8_t opaque[8];                 
#else
  struct __netio_queue_impl_t* __system_part;    
  struct __netio_queue_user_impl_t* __user_part; 
#ifdef _NETIO_PTHREAD
  _netio_percpu_mutex_t lock;                    
#endif
#endif
}
netio_queue_t;


typedef struct
{
#ifdef __DOXYGEN__
  uint8_t opaque[44];   
#else
  uint8_t flags;        
  uint8_t datalen;      
  uint32_t request[9];  
  uint32_t *data;       
#endif
}
netio_send_pkt_context_t;


#ifndef __DOXYGEN__
#define SEND_PKT_CTX_USE_EPP   1  
#define SEND_PKT_CTX_SEND_CSUM 2  
#endif

typedef struct
#ifndef __DOXYGEN__
__attribute__((aligned(8)))
#endif
{
  uint8_t user_data;


  uint8_t buffer_address_low;

  
  uint16_t size;

  netio_pkt_handle_t handle;
}
netio_pkt_vector_entry_t;


/**
 * @brief Initialize fields in a packet vector entry.
 *
 * @ingroup egress
 *
 * @param[out] v Pointer to the vector entry to be initialized.
 * @param[in] pkt Packet to be transmitted when the vector entry is passed to
 *        netio_send_packet_vector().  Note that the packet's attributes
 *        (e.g., its L2 offset and length) are captured at the time this
 *        routine is called; subsequent changes in those attributes will not
 *        be reflected in the packet which is actually transmitted.
 *        Changes in the packet's contents, however, will be so reflected.
 *        If this is NULL, no packet will be transmitted.
 * @param[in] user_data User data to be set in the vector entry.
 *        This function guarantees that the "user_data" field will become
 *        visible to a reader only after all other fields have become visible.
 *        This allows a structure in a ring buffer to be written and read
 *        by a polling reader without any locks or other synchronization.
 */
static __inline void
netio_pkt_vector_set(volatile netio_pkt_vector_entry_t* v, netio_pkt_t* pkt,
                     uint8_t user_data)
{
  if (pkt)
  {
    if (NETIO_PKT_IS_MINIMAL(pkt))
    {
      netio_pkt_minimal_metadata_t* mmd =
        (netio_pkt_minimal_metadata_t*) &pkt->__metadata;
      v->buffer_address_low = (uintptr_t) NETIO_PKT_L2_DATA_MM(mmd, pkt) & 0xFF;
      v->size = NETIO_PKT_L2_LENGTH_MM(mmd, pkt);
    }
    else
    {
      netio_pkt_metadata_t* mda = &pkt->__metadata;
      v->buffer_address_low = (uintptr_t) NETIO_PKT_L2_DATA_M(mda, pkt) & 0xFF;
      v->size = NETIO_PKT_L2_LENGTH_M(mda, pkt);
    }
    v->handle.word = pkt->__packet.word;
  }
  else
  {
    v->handle.word = 0;   
  }

  __asm__("" : : : "memory");

  v->user_data = user_data;
}



#define NETIO_PARAM       0
#define NETIO_PARAM_MAC        0

#define NETIO_PARAM_PAUSE_IN   1

#define NETIO_PARAM_PAUSE_OUT  2

#define NETIO_PARAM_JUMBO      3

#define NETIO_PARAM_OVERFLOW   4

#define NETIO_PARAM_STAT 5

#define NETIO_PARAM_LINK_POSSIBLE_STATE 6

#define NETIO_PARAM_LINK_CONFIG NETIO_PARAM_LINK_POSSIBLE_STATE

#define NETIO_PARAM_LINK_CURRENT_STATE 7

#define NETIO_PARAM_LINK_STATUS NETIO_PARAM_LINK_CURRENT_STATE

#define NETIO_PARAM_COHERENT 8

#define NETIO_PARAM_LINK_DESIRED_STATE 9

typedef struct
{
  uint32_t packets_received;

  uint32_t packets_dropped;


  uint8_t drops_no_worker;
#ifndef __DOXYGEN__
#define NETIO_STAT_DROPS_NO_WORKER   0
#endif

  uint8_t drops_no_smallbuf;
#ifndef __DOXYGEN__
#define NETIO_STAT_DROPS_NO_SMALLBUF 1
#endif

  uint8_t drops_no_largebuf;
#ifndef __DOXYGEN__
#define NETIO_STAT_DROPS_NO_LARGEBUF 2
#endif

  uint8_t drops_no_jumbobuf;
#ifndef __DOXYGEN__
#define NETIO_STAT_DROPS_NO_JUMBOBUF 3
#endif
}
netio_stat_t;


#define NETIO_LINK_10M         0x01

#define NETIO_LINK_100M        0x02

#define NETIO_LINK_1G          0x04

#define NETIO_LINK_10G         0x08

#define NETIO_LINK_ANYSPEED    0x10

#define NETIO_LINK_SPEED  (NETIO_LINK_10M  | \
                           NETIO_LINK_100M | \
                           NETIO_LINK_1G   | \
                           NETIO_LINK_10G  | \
                           NETIO_LINK_ANYSPEED)


#define NETIO_MAC             1

#define NETIO_MDIO            2

#define NETIO_MDIO_CLAUSE45   3

typedef union
{
  struct
  {
    unsigned int reg:16;  
    unsigned int phy:5;   
    unsigned int dev:5;   
  }
  bits;                   
  uint64_t addr;          
}
netio_mdio_addr_t;


#endif 
