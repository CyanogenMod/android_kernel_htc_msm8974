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


#ifndef _PVTCP_OFF_H_
#define _PVTCP_OFF_H_


#define PVTCP_OFF_SOCK_COMMON_FIELDS                                \
   volatile unsigned int opFlags;  \
   volatile unsigned int flags    



enum PvtcpOffPvskFlags {
   PVTCP_OFF_PVSKF_IPV6_LOOP = 0, 
   PVTCP_OFF_PVSKF_SHUT_RD,       
   PVTCP_OFF_PVSKF_SHUT_WR,       
   PVTCP_OFF_PVSKF_TCP_NODELAY,   
   PVTCP_OFF_PVSKF_TCP_CORK,      
   PVTCP_OFF_PVSKF_DISCONNECT,    
   PVTCP_OFF_PVSKF_INVALID = 32
};



#if defined(__linux__)
#include "pvtcp_off_linux.h"
#else
#error "Unsupported OS."
#endif



typedef struct PvtcpOffBuf {
   CommOSList link;    
   unsigned short len;
   unsigned short off;
   char data[1];
} PvtcpOffBuf;



static inline void *
PvtcpOffBufFromInternalOff(PvtcpOffBuf *arg)
{
   return arg ?
          &arg->data[arg->off] :
          NULL;
}



static inline void *
PvtcpOffBufFromInternal(PvtcpOffBuf *arg)
{
   return arg ?
          &arg->data[0] :
          NULL;
}



static inline PvtcpOffBuf *
PvtcpOffInternalFromBuf(void *arg)
{
   return arg ?
          (PvtcpOffBuf *)((char *)arg - offsetof(PvtcpOffBuf, data)) :
          NULL;
}



static inline int
PvskTestOpFlag(struct PvtcpSock *pvsk,
               int op)
{
   return pvsk->opFlags & (1 << op);
}



static inline void
PvskSetOpFlag(struct PvtcpSock *pvsk,
              int op)
{
   unsigned int ops;

   SOCK_STATE_LOCK(pvsk);
   ops = pvsk->opFlags | (1 << op);
   pvsk->opFlags = ops;
   SOCK_STATE_UNLOCK(pvsk);
}



static inline void
PvskResetOpFlag(struct PvtcpSock *pvsk,
                int op)
{
   unsigned int ops;

   SOCK_STATE_LOCK(pvsk);
   ops = pvsk->opFlags & ~(1 << op);
   pvsk->opFlags = ops;
   SOCK_STATE_UNLOCK(pvsk);
}



static inline int
PvskTestFlag(struct PvtcpSock *pvsk,
             int flag)
{
   return (flag < PVTCP_OFF_PVSKF_INVALID) && (pvsk->flags & (1 << flag));
}



static inline void
PvskSetFlag(struct PvtcpSock *pvsk,
            int flag,
            int onOff)
{
   unsigned int flags;

   SOCK_STATE_LOCK(pvsk);
   if (flag < PVTCP_OFF_PVSKF_INVALID) {
      if (onOff) {
         flags = pvsk->flags | (1 << flag);
      } else {
         flags = pvsk->flags & ~(1 << flag);
      }
      pvsk->flags = flags;
   }
   SOCK_STATE_UNLOCK(pvsk);
}


int PvtcpOffSockInit(PvtcpSock *pvsk, CommChannel channel);

#endif 

