/*
 * Linux 2.6.32 and later Kernel module for VMware MVP PVTCP Server
 *
 * Copyright (C) 2010-2013 VMware, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; see the file COPYING.  If not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */
#line 5


#ifndef _PVTCP_H_
#define _PVTCP_H_


#if defined(__linux__)
#include <linux/in.h>
#include <linux/in6.h>
#else
#error "Unsupported OS."
#endif

#include "comm_svc.h"

#define PVTCP_CHANNEL_OPEN_TIMEOUT 2000

#define PVTCP_SOCK_BUF_SIZE (8 << 10) 

#define PVTCP_SOCK_DGRAM_BUF_SIZE PVTCP_SOCK_BUF_SIZE
#define PVTCP_SOCK_STREAM_BUF_SIZE PVTCP_SOCK_BUF_SIZE

typedef struct PvtcpDgramPseudoHeader {
   unsigned long long d0;
   unsigned long long d1;
   unsigned long long d2;
   unsigned long long d3;
} PvtcpDgramPseudoHeader;



#if !defined(PVTCP_SOCK_RCVSIZE_MODEL)
   #define PVTCP_SOCK_RCVSIZE_MODEL 1
#endif

#if PVTCP_SOCK_RCVSIZE_MODEL == 1
   #define PVTCP_SOCK_LARGE_ACK_WM (64 << 10) 
   #define PVTCP_SOCK_LARGE_ACK_ORDER 15
   #define PVTCP_SOCK_SMALL_ACK_ORDER 11
   #define PVTCP_SOCK_SAFE_RCVSIZE (128 << 10) 
#elif PVTCP_SOCK_RCVSIZE_MODEL == 2
   #define PVTCP_SOCK_LARGE_ACK_WM (128 << 10) 
   #define PVTCP_SOCK_LARGE_ACK_ORDER 16
   #define PVTCP_SOCK_SMALL_ACK_ORDER 12
   #define PVTCP_SOCK_SAFE_RCVSIZE (256 << 10) 
#elif PVTCP_SOCK_RCVSIZE_MODEL == 3
   #define PVTCP_SOCK_LARGE_ACK_WM (128 << 10) 
   #define PVTCP_SOCK_LARGE_ACK_ORDER 16
   #define PVTCP_SOCK_SMALL_ACK_ORDER 12
   #define PVTCP_SOCK_SAFE_RCVSIZE (512 << 10) 
#else
   #error "Invalid PVTCP_SOCK_RCVSIZE_MODEL (one of 1, 2, 3)"
#endif

#define PVTCP_SOCK_RCVSIZE    \
   (PVTCP_SOCK_SAFE_RCVSIZE + \
    PVTCP_SOCK_BUF_SIZE + sizeof(PvtcpDgramPseudoHeader))



enum PvtcpOpCodes {
   PVTCP_OP_FLOW = 0,
   PVTCP_OP_IO,
   PVTCP_OP_CREATE,
   PVTCP_OP_RELEASE,
   PVTCP_OP_BIND,
   PVTCP_OP_LISTEN,
   PVTCP_OP_ACCEPT,
   PVTCP_OP_CONNECT,
   PVTCP_OP_SHUTDOWN,
   PVTCP_OP_SETSOCKOPT,
   PVTCP_OP_GETSOCKOPT,
   PVTCP_OP_IOCTL,
   PVTCP_OP_INVALID
};

#define PVTCP_FLOW_OP_INVALID_SIZE 0xffffffff



COMM_DEFINE_OP(PvtcpFlowOp);
COMM_DEFINE_OP(PvtcpIoOp);
COMM_DEFINE_OP(PvtcpCreateOp);
COMM_DEFINE_OP(PvtcpReleaseOp);
COMM_DEFINE_OP(PvtcpBindOp);
COMM_DEFINE_OP(PvtcpListenOp);
COMM_DEFINE_OP(PvtcpAcceptOp);
COMM_DEFINE_OP(PvtcpConnectOp);
COMM_DEFINE_OP(PvtcpShutdownOp);
COMM_DEFINE_OP(PvtcpSetSockOptOp);
COMM_DEFINE_OP(PvtcpGetSockOptOp);
COMM_DEFINE_OP(PvtcpIoctlOp);



#define PVTCP_COMM_IMPL_TYPE "com.vmware.comm.protocol.pvTCP@"

#define PVTCP_COMM_IMPL_VERS_1_0 (PVTCP_COMM_IMPL_TYPE "1.0")
#define PVTCP_COMM_IMPL_VERS_1_1 (PVTCP_COMM_IMPL_TYPE "1.1")

typedef enum {
   PVTCP_VERS_1_0 = 0,
   PVTCP_VERS_1_1
} PvtcpVersion;

extern const char *pvtcpVersions[];
extern const unsigned int pvtcpVersionsSize;



#define PVTCP_PF_UNBOUND   0x0
#define PVTCP_PF_DEATH_ROW 0xffffffff
#define PVTCP_PF_LOOPBACK_INET4 (PVTCP_PF_DEATH_ROW - 1)



typedef struct PvtcpIfConf {
   int family;

   union {
      struct in_addr in;
      struct in6_addr in6;
   } addr;                          
   union {
      struct in_addr in;
      struct in6_addr in6;
   } mask;                          
} PvtcpIfConf;


struct PvtcpState;

typedef struct PvtcpIf {
   CommOSList sockList;       
   CommOSList stateLink;      
   struct PvtcpState *state;  
   PvtcpIfConf conf;          
} PvtcpIf;



typedef struct PvtcpState {
   unsigned long long id;     
   CommOSList ifList;         
   CommChannel channel;       
   PvtcpIf ifDeathRow;        
   PvtcpIf ifUnbound;         
   PvtcpIf ifLoopbackInet4;   
   void *namespace;           
   void *extra;               
   unsigned int mask;         
} PvtcpState;



#define PVTCP_SOCK_COMMON_FIELDS                                           \
   CommOSMutex inLock;           \
   CommOSMutex outLock;          \
   CommOSSpinlock stateLock;     \
   CommOSList ifLink;            \
   CommOSWork work;              \
   PvtcpIf *netif;               \
   PvtcpState *state;            \
   unsigned long long stateID;   \
   CommChannel channel;          \
   unsigned long long peerSock;  \
   volatile int peerSockSet;     \
   CommOSAtomic deltaAckSize;    \
   CommOSAtomic rcvdSize;        \
   CommOSAtomic sentSize;        \
   CommOSAtomic queueSize;       \
   CommOSList queue;             \
   void *rpcReply;               \
   int rpcStatus;                \
   int err                      

#define PVTCP_PEER_SOCK_NULL ((unsigned long long)0)



#define SOCK_STATE_LOCK(pvsk)   CommOS_SpinLock(&(pvsk)->stateLock)
#define SOCK_STATE_UNLOCK(pvsk) CommOS_SpinUnlock(&(pvsk)->stateLock)

#define SOCK_IN_TRYLOCK(pvsk)   CommOS_MutexTrylock(&(pvsk)->inLock)
#define SOCK_IN_LOCK(pvsk)      CommOS_MutexLock(&(pvsk)->inLock)
#define SOCK_IN_UNLOCK(pvsk)    CommOS_MutexUnlock(&(pvsk)->inLock)

#define SOCK_OUT_TRYLOCK(pvsk)  CommOS_MutexTrylock(&(pvsk)->outLock)
#define SOCK_OUT_LOCK(pvsk)     CommOS_MutexLock(&(pvsk)->outLock)
#define SOCK_OUT_LOCK_UNINT(pvsk) \
   CommOS_MutexLockUninterruptible(&(pvsk)->outLock)
#define SOCK_OUT_UNLOCK(pvsk)   CommOS_MutexUnlock(&(pvsk)->outLock)

#define PVTCP_UNLOCK_DISP_DISCARD_VEC()         \
   do {                                         \
      CommSvc_DispatchUnlock(channel);          \
      while (vecLen) {                          \
         PvtcpBufFree(vec[--vecLen].iov_base);  \
      }                                         \
   } while (0)


#if defined(PVTCP_BUILDING_SERVER)
#include "pvtcp_off.h"
#else
#include "pvtcp_pv.h"
#endif 



extern const PvtcpIfConf *pvtcpIfUnbound;
extern const PvtcpIfConf *pvtcpIfDeathRow;
extern const PvtcpIfConf *pvtcpIfLoopbackInet4;

extern CommImpl pvtcpImpl;
extern CommOperationFunc pvtcpOperations[];

extern CommChannel pvtcpClientChannel;



void *PvtcpStateAlloc(CommChannel channel);
void PvtcpStateFree(void *arg);

int PvtcpStateAddIf(CommChannel channel, const PvtcpIfConf *conf);
void PvtcpStateRemoveIf(CommChannel channel, const PvtcpIfConf *conf);
PvtcpIf *PvtcpStateFindIf(PvtcpState *state, const PvtcpIfConf *conf);

int
PvtcpStateAddSocket(CommChannel channel,
                    const PvtcpIfConf *conf,
                    PvtcpSock *sock);
int PvtcpStateRemoveSocket(CommChannel channel, PvtcpSock *sock);



int PvtcpCheckArgs(CommTranspInitArgs *transpArgs);

void
PvtcpCloseNtf(void *ntfData,
              const CommTranspInitArgs *transpArgs,
              int inBH);

void *
PvtcpBufAlloc(unsigned int size,
              CommChannel channel,
              CommPacket *header,
              int (*copy)(CommChannel channel,
                          void *dest,
                          unsigned int size,
                          int kern));
void PvtcpBufFree(void *buf);

void PvtcpReleaseSocket(PvtcpSock *pvsk);
int PvtcpSockInit(PvtcpSock *pvsk, CommChannel channel);

void PvtcpProcessAIO(CommOSWork *work);



static inline void
PvtcpI6AddrPack(const unsigned int addr[4],
                unsigned long long *d64_0,
                unsigned long long *d64_1)
{
   *d64_0 = *(unsigned long long *)&addr[0];
   *d64_1 = *(unsigned long long *)&addr[2];
}



static inline void
PvtcpI6AddrUnpack(unsigned int addr[4],
                  unsigned long long d64_0,
                  unsigned long long d64_1)
{
   *(unsigned long long *)&addr[0] = d64_0;
   *(unsigned long long *)&addr[2] = d64_1;
}



#if defined(__LP64__) || defined(__LLP64__)

#define PvtcpGetPvskOrReturn(handle, container)                                \
   ({                                                                          \
      PvtcpState *__state = (PvtcpState *)(container);                         \
      PvtcpSock *__pvsk =                                                      \
         (PvtcpSock *)((handle) ^ (unsigned long long)__state->mask);          \
                                                                               \
      if (__pvsk->stateID != __state->id) {                                    \
         PVTCP_UNLOCK_DISP_DISCARD_VEC();                                      \
         CommSvc_Zombify(__state->channel, 0);                                 \
         return;                                                               \
      }                                                                        \
      (__pvsk);                                                                \
   })

#else 

#define PvtcpGetPvskOrReturn(handle, container)                                \
   ({                                                                          \
      PvtcpState *__state = (PvtcpState *)(container);                         \
      PvtcpSock *__pvsk =                                                      \
         (PvtcpSock *)((unsigned int)(handle) ^ __state->mask);                \
                                                                               \
      if (__pvsk->stateID != __state->id) {                                    \
         PVTCP_UNLOCK_DISP_DISCARD_VEC();                                      \
         CommSvc_Zombify(__state->channel, 0);                                 \
         return;                                                               \
      }                                                                        \
      (__pvsk);                                                                \
   })

#endif 



#if defined(__LP64__) || defined(__LLP64__)

#define PvtcpGetHandle(pvsk)                                                   \
   ((unsigned long long)(pvsk) ^ (unsigned long long)(pvsk)->state->mask)

#else 

#define PvtcpGetHandle(pvsk)                                                   \
   ((unsigned int)(pvsk) ^ (pvsk)->state->mask)

#endif 

#endif 

