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



#include "pvtcp.h"

#if defined(CONFIG_NET_NS)
#include <linux/nsproxy.h>
#include <linux/un.h>
#endif

#include <net/ipv6.h>
#include <linux/kobject.h>
#include <linux/netfilter_ipv4.h>
#include <linux/netfilter_ipv6.h>
#include <linux/cred.h>


#define PVTCP_PVSOCK_ADDR 0x7fee0001
#define PVTCP_PVSOCK_NET  0x7fee0000
#define PVTCP_PVSOCK_MASK 0x000000ff
#define PVTCP_PVSOCK_NETMASK 0xffffff00

extern uid_t Mvpkm_vmwareUid;
extern gid_t Mvpkm_vmwareGid;

static struct cred _cred;
static struct file _file = {
   .f_cred = &_cred,
};

extern CommOSAtomic PvtcpOutputAIOSection;
extern void PvtcpOffLargeDgramBufInit(void);

static const unsigned short portRangeBase = 7000;
static const unsigned int   portRangeSize = 31;
static int hooksRegistered;

static inline int PvtcpTestPortIndexBit(unsigned int addr,
                                        unsigned int portIdx);

static inline unsigned int
PvsockNfHook(struct sk_buff *skb, int inet6)
{
   uid_t uid;
   unsigned int port;
   struct socket *sock;
   unsigned int addr = inet6 ?
                       ntohl(ipv6_hdr(skb)->daddr.s6_addr32[3]) :
                       ntohl(ip_hdr(skb)->daddr);

   if (likely((addr ^ PVTCP_PVSOCK_NET) & ~PVTCP_PVSOCK_MASK)) {
      
      return NF_ACCEPT;
   }

   sock = skb->sk->sk_socket;
   if (unlikely(!sock)) {
      return NF_ACCEPT;
   }

   uid = (sock->file ? sock->file->f_cred->uid : 0);
   if (uid == 0 || (sock->type != SOCK_STREAM && sock->type != SOCK_DGRAM)) {
      return NF_ACCEPT;
   }

   if (likely(uid == Mvpkm_vmwareUid)) {
      if (sock->type == SOCK_DGRAM) {
         port = ntohs(udp_hdr(skb)->dest) - portRangeBase;
         if ((port < portRangeSize) &&
              PvtcpTestPortIndexBit(htonl(addr), port)) {
            return NF_ACCEPT;
         }
         return NF_DROP;
      }
      return NF_ACCEPT;
   }

   return NF_DROP;
}


static unsigned int
Inet4NfHook(unsigned int hooknum,
            struct sk_buff *skb,
            const struct net_device *in,
            const struct net_device *out,
            int (*okfn)(struct sk_buff *))
{
   return PvsockNfHook(skb, 0);
}

static unsigned int
Inet6NfHook(unsigned int hooknum,
            struct sk_buff *skb,
            const struct net_device *in,
            const struct net_device *out,
            int (*okfn)(struct sk_buff *))
{
   if (!ipv6_addr_v4mapped(&ipv6_hdr(skb)->daddr)) {
      
      return NF_ACCEPT;
   }

   return PvsockNfHook(skb, 1);
}


static struct nf_hook_ops netfilterHooks[] = {
   {
      .hook = Inet4NfHook,
      .owner = THIS_MODULE,
      .pf = PF_INET,
      .hooknum = NF_INET_LOCAL_OUT,
      .priority = NF_IP_PRI_SECURITY
   },
   {
      .hook = Inet6NfHook,
      .owner = THIS_MODULE,
      .pf = PF_INET6,
      .hooknum = NF_INET_LOCAL_OUT,
      .priority = NF_IP6_PRI_SECURITY
   }
};


#if !defined(CONFIG_SYSFS)
#error "The pvTCP offload module requires sysfs!"
#endif


typedef struct PvtcpStateKObj {
   struct kobject kobj;
   CommTranspInitArgs transpArgs;
   unsigned int pvsockAddr;
   int useNS;
   int haveNS;
} PvtcpStateKObj;


typedef struct  PvtcpStateKObjAttr {
   struct attribute attr;
   ssize_t (*show)(PvtcpStateKObj *stateKObj, char *buf);
   ssize_t (*store)(PvtcpStateKObj *stateKObj, const char *buf, size_t count);
} PvtcpStateKObjAttr;



static void
StateKObjRelease(struct kobject *kobj)
{
   kfree(container_of(kobj, PvtcpStateKObj, kobj));
}


/**
 * @brief Sysfs show function for all pvtcp attributes.
 * @param kobj (embedded) state kobject.
 * @param attr pvtcp attribute to show.
 * @param buf output buffer.
 * @return number of bytes written or negative error code.
 */

static ssize_t
StateKObjShow(struct kobject *kobj,
              struct attribute *attr,
              char *buf)
{
   PvtcpStateKObjAttr *stateAttr = container_of(attr, PvtcpStateKObjAttr, attr);
   PvtcpStateKObj *stateKObj = container_of(kobj, PvtcpStateKObj, kobj);

   if (stateAttr->show) {
      return stateAttr->show(stateKObj, buf);
   }

   return -EIO;
}



static ssize_t
StateKObjStore(struct kobject *kobj,
               struct attribute *attr,
               const char *buf,
               size_t count)
{
   PvtcpStateKObjAttr *stateAttr = container_of(attr, PvtcpStateKObjAttr, attr);
   PvtcpStateKObj *stateKObj = container_of(kobj, PvtcpStateKObj, kobj);

   if (stateAttr->store) {
      return stateAttr->store(stateKObj, buf, count);
   }

   return -EIO;
}


static const struct sysfs_ops StateKObjSysfsOps = {
   .show = StateKObjShow,
   .store = StateKObjStore
};


/**
 * @brief Show function for the comm_info pvtcp attribute.
 * @param stateKObj state kobject.
 * @param buf output buffer.
 * @return number of bytes written or negative error code.
 */

static ssize_t
StateKObjCommInfoShow(PvtcpStateKObj *stateKObj,
                      char *buf)
{
   unsigned int typeHash;


   typeHash = CommTransp_GetType(pvtcpVersions[stateKObj->transpArgs.type]);

   return snprintf(buf, PAGE_SIZE, "ID=%u,%u\nCAPACITY=%u\nTYPE=0x%0x\n",
                   stateKObj->transpArgs.id.d32[0],
                   stateKObj->transpArgs.id.d32[1],
                   stateKObj->transpArgs.capacity,
                   typeHash);
}


/**
 * @brief Show function for the pvsock_addr pvtcp attribute.
 * @param stateKObj state kobject.
 * @param buf output buffer.
 * @return number of bytes written or negative error code.
 */

static ssize_t
StateKObjPvsockAddrShow(PvtcpStateKObj *stateKObj,
                        char *buf)
{
   union {
      unsigned int raw;
      unsigned char bytes[4];
   } addr;

   addr.raw = stateKObj->pvsockAddr;
   return snprintf(buf, PAGE_SIZE, "%u.%u.%u.%u\n",
                   (unsigned int)addr.bytes[0], (unsigned int)addr.bytes[1],
                   (unsigned int)addr.bytes[2], (unsigned int)addr.bytes[3]);
}


/**
 * @brief Show function for the use_ns pvtcp attribute.
 * @param stateKObj state kobject.
 * @param buf output buffer.
 * @return number of bytes written or negative error code.
 */

static ssize_t
StateKObjUseNSShow(PvtcpStateKObj *stateKObj,
                   char *buf)
{
   return snprintf(buf, PAGE_SIZE, "%d\n", stateKObj->useNS);
}



static ssize_t
StateKObjUseNSStore(PvtcpStateKObj *stateKObj,
                    const char *buf,
                    size_t count)
{
   int rc = -EINVAL;

   
   if (stateKObj->haveNS && (sscanf(buf, "%d", &stateKObj->useNS) == 1)) {
      stateKObj->useNS = !!stateKObj->useNS;
      rc = count;
   }

   return rc;
}


static PvtcpStateKObjAttr stateKObjCommInfoAttr =
   __ATTR(comm_info, 0444, StateKObjCommInfoShow, NULL);

static PvtcpStateKObjAttr stateKObjPvsockAddrAttr =
   __ATTR(pvsock_addr, 0444, StateKObjPvsockAddrShow, NULL);

static PvtcpStateKObjAttr stateKObjUseNSAttr =
   __ATTR(use_ns, 0644, StateKObjUseNSShow, StateKObjUseNSStore);


static struct attribute *stateKObjDefaultAttrs[] = {
   &stateKObjCommInfoAttr.attr,
   &stateKObjPvsockAddrAttr.attr,
   &stateKObjUseNSAttr.attr,
   NULL
};


static struct kobj_type stateKType = {
   .sysfs_ops = &StateKObjSysfsOps,
   .release = StateKObjRelease,
   .default_attrs = stateKObjDefaultAttrs
};



static int Init(void *args);
static void Exit(void);

COMM_OS_MOD_INIT(Init, Exit);



static CommOSMutex globalLock;
static char perCpuBuf[NR_CPUS][PVTCP_SOCK_BUF_SIZE];

#define PVTCP_OFF_MAX_LB_ADDRS 255
static unsigned int loopbackAddrs[PVTCP_OFF_MAX_LB_ADDRS] = {
   0xffffffff, 
   0x7fffffff  
               
};

static const unsigned int loopbackReserved = 0x00000001 << 31;


#define PvtcpTestLoopbackBit(entry, mask) \
   ((entry) & (mask))

#define PvtcpSetLoopbackBit(entry, mask) \
   ((entry) |= (mask))

#define PvtcpResetLoopbackBit(entry, mask) \
   ((entry) &= ~(mask))


static inline int
PvtcpTestPortIndexBit(unsigned int addr,
                      unsigned int portIdx)
{
   return PvtcpTestLoopbackBit(loopbackAddrs[*((unsigned char *)&addr + 3)],
                               BIT(portIdx));
}


static inline void
PvtcpSetPortIndexBit(unsigned int addr,
                     unsigned int portIdx)
{
   PvtcpSetLoopbackBit(loopbackAddrs[*((unsigned char *)&addr + 3)],
                       BIT(portIdx));
}


static inline void
PvtcpResetPortIndexBit(unsigned int addr,
                       unsigned int portIdx)
{
   PvtcpResetLoopbackBit(loopbackAddrs[*((unsigned char *)&addr + 3)],
                         BIT(portIdx));
}


unsigned int pvtcpLoopbackOffAddr;

unsigned long long pvtcpOffDgramAllocations;


extern void asmDestructorShim(struct sock *);



static void
SockReleaseWrapper(struct socket *sock)
{
   sock->file = NULL;
   sock_release(sock);
}


static unsigned int
GetLoopbackAddr(void)
{
   static unsigned char addrTempl[4] = { 127, 238, 0, 0 };
   unsigned int rc = -1U;
   unsigned int idx;
   struct socket *sock;

   CommOS_MutexLock(&globalLock);
   for (idx = 1; idx < PVTCP_OFF_MAX_LB_ADDRS; idx++) {
      if (!PvtcpTestLoopbackBit(loopbackAddrs[idx], loopbackReserved)) {
         addrTempl[3] = (unsigned char)idx;
         memcpy(&rc, addrTempl, sizeof(rc));

         

         if (!sock_create_kern(AF_INET, SOCK_DGRAM, 0, &sock)) {
            int err;
            struct sockaddr_in sin = {
               .sin_family = AF_INET,
               .sin_addr = { .s_addr = rc }
            };
            struct ifreq ifr = {
               .ifr_flags = IFF_UP
            };

            snprintf(ifr.ifr_name, sizeof(ifr.ifr_name), "lo:%u", idx);
            memcpy(&ifr.ifr_addr, &sin, sizeof(ifr.ifr_addr));
            err = kernel_sock_ioctl(sock, SIOCSIFADDR, (unsigned long)&ifr);
            sock_release(sock);
            if (err) {
               CommOS_Log(("%s: Could not set loopback address (ioctl)!\n",
                           __func__));
               rc = -1U;
               continue; 
            } else {
               PvtcpSetLoopbackBit(loopbackAddrs[idx], loopbackReserved);
               CommOS_Debug(("%s: Allocated loopback address [%u.%u.%u.%u].\n",
                             __func__,
                             addrTempl[0], addrTempl[1],
                             addrTempl[2], addrTempl[3]));
               break;
            }
         } else {
            CommOS_Log(("%s: Could not set loopback address (create)!\n",
                        __func__));
            rc = -1U;
            break;
         }
      }
   }
   if (idx == PVTCP_OFF_MAX_LB_ADDRS) {
      CommOS_Log(("%s: loopback address range exceeded!\n", __func__));
   }

   CommOS_MutexUnlock(&globalLock);
   return rc;
}



static void
PutLoopbackAddr(unsigned int uaddr)
{
   const unsigned char addrTempl[3] = { 127, 238, 0 };
   unsigned char addr[4];
   unsigned int idx;
   struct socket *sock;

   memcpy(addr, &uaddr, sizeof(uaddr));
   if (memcmp(addrTempl, addr, sizeof(addrTempl))) {
      return;
   }

   idx = addr[3];
   if ((idx == 0) || (idx >= PVTCP_OFF_MAX_LB_ADDRS)) {
      return;
   }

   CommOS_MutexLock(&globalLock);
   if (!PvtcpTestLoopbackBit(loopbackAddrs[idx], loopbackReserved)) {
      CommOS_Debug(("%s: loopback entry [%u] already freed.\n",
                    __func__, idx));
      goto out;
   }

   if (!sock_create_kern(AF_INET, SOCK_DGRAM, 0, &sock)) {
      struct sockaddr_in sin = {
         .sin_family = AF_INET,
         .sin_addr = { .s_addr = uaddr }
      };
      struct ifreq ifr = {
         .ifr_flags = 0
      };

      snprintf(ifr.ifr_name, sizeof(ifr.ifr_name), "lo:%u", idx);
      memcpy(&ifr.ifr_addr, &sin, sizeof(ifr.ifr_addr));
      kernel_sock_ioctl(sock, SIOCSIFFLAGS, (unsigned long)&ifr);
      sock_release(sock);
      loopbackAddrs[idx] = 0; 
      CommOS_Debug(("%s: Deallocated loopback address [%u.%u.%u.%u].\n",
                    __func__, addr[0], addr[1], addr[2], addr[3]));
   } else {
      CommOS_Log(("%s: Could not delete loopback address!\n",
                  __func__));
   }

out:
   CommOS_MutexUnlock(&globalLock);
}



static void
GetNetNamespace(PvtcpState *state)
{
#if defined(CONFIG_NET_NS) && !defined(PVTCP_NET_NS_DISABLE)
   CommTranspInitArgs args;
   pid_t pidn;
   struct pid *pid;
   struct task_struct *tsk;
   struct nsproxy *nsproxy;
   struct net *ns;
   struct socket *sock;
   struct sockaddr_un addr __attribute__ ((aligned(4))) = {
      .sun_family = AF_UNIX
   };
   struct timeval timeout = {
      .tv_sec = 3000,
      .tv_usec = 0
   };
   const int passcred = 1;
   char buf[64];
   struct kvec vec;
   const char *sockname = "pvtcp-vpn"; 
   const size_t socknamelen = strlen(sockname);

   struct msghdr msg = {
      .msg_name = (struct sockaddr *)&addr,
      .msg_namelen = 1 + offsetof(struct sockaddr_un, sun_path) + socknamelen
   };


   if (!state) {
      return;
   }

   args = CommSvc_GetTranspInitArgs(state->channel);
   ns = NULL;
   pidn = 0;

   if (sock_create_kern(AF_UNIX, SOCK_DGRAM, 0, &sock)) {
      CommOS_Debug(("%s: Can't create config socket!\n", __func__));
      goto out;
   }
   if (kernel_setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO,
                         (char *)&timeout, sizeof(timeout))) {
      sock_release(sock);
      CommOS_Debug(("%s: Can't set timeout on config socket!\n", __func__));
      goto out;
   }
   if (kernel_setsockopt(sock, SOL_SOCKET, SO_PASSCRED,
                         (char *)&passcred, sizeof(passcred))) {
      sock_release(sock);
      CommOS_Debug(("%s: Can't set passcred on config socket!\n",
                    __func__));
      goto out;
   }


   memset(buf, 0, sizeof(buf));
   snprintf(buf, sizeof(buf), "%u\n", args.id.d32[0]);
   buf[sizeof(buf) - 1] = '\0';
   vec.iov_base = buf;
   vec.iov_len = strlen(buf);

   
   addr.sun_path[0] = 0;
   memcpy(addr.sun_path+1, sockname, socknamelen);

   if (kernel_sendmsg(sock, &msg, &vec, 1, vec.iov_len) <= 0) {
      sock_release(sock);
      CommOS_Debug(("%s: Could not send config request for vm [%u]!\n",
                    __func__, args.id.d32[0]));
      goto out;
   }

   memset(buf, 0, sizeof(buf));
   vec.iov_base = buf;
   vec.iov_len = sizeof(buf);
   if (kernel_recvmsg(sock, &msg, &vec, 1, vec.iov_len, 0) <= 0) {
      CommOS_Debug(("%s: Could not receive config reply for vm [%u]!\n",
                    __func__, args.id.d32[0]));
   } else {
      buf[sizeof(buf) - 1] = '\0';
      
      sscanf(buf, "%d", &pidn);
   }
   sock_release(sock);

   if (!pidn) {
      goto out;
   }

   pid = find_get_pid(pidn);
   if (pid) {
      tsk = pid_task(pid, PIDTYPE_PID);
      if (tsk) {
         rcu_read_lock();
         nsproxy = task_nsproxy(tsk);
         if (nsproxy && nsproxy->net_ns) {
            ns = maybe_get_net(nsproxy->net_ns);
         }
         rcu_read_unlock();
      }
      put_pid(pid);
   }

out:
   if (!ns) {
      CommOS_Debug(("%s: Not using a namespace  for vm [%u].\n",
                    __func__, args.id.d32[0]));
      ns = &init_net;
   } else {
      CommOS_Debug(("%s: Found the net namespace for vm [%u].\n",
                    __func__, args.id.d32[0]));
   }
#else
   void *ns = NULL;
#endif

   state->namespace = ns;
}



static void
PutNetNamespace(void *namespace)
{
#if defined(CONFIG_NET_NS) && !defined(PVTCP_NET_NS_DISABLE)
   if (namespace && (namespace != &init_net)) {
      put_net((struct net *)namespace);
   }
#endif
}



static void *
StateAlloc(CommChannel channel)
{
   extern struct kset *Mvpkm_FindVMNamedKSet(int, const char *);
   PvtcpState *state = NULL;
   PvtcpIf *loopbackNetif = NULL;
   PvtcpStateKObj *stateKObj = NULL;
   struct kset *kset = NULL;
   int rc;
   CommTranspInitArgs transpArgs;

   transpArgs = CommSvc_GetTranspInitArgs(channel);


   kset = Mvpkm_FindVMNamedKSet((int)transpArgs.id.d32[0], "devices");
   if (!kset) {
      CommOS_Debug(("%s: Could not find sysfs '.../vm/N/devices' kset!\n",
                    __func__));
      goto error;
   }

   state = PvtcpStateAlloc(channel);
   if (!state) {
      CommOS_Debug(("%s: Could not allocate state!\n", __func__));
      goto error;
   }

   
   stateKObj = kzalloc(sizeof(*stateKObj), GFP_KERNEL);
   if (!stateKObj) {
      CommOS_Debug(("%s: Could not allocate state kobject!\n", __func__));
      goto error;
   }

   stateKObj->kobj.kset = kset;
   
   rc = kobject_init_and_add(&stateKObj->kobj, &stateKType, NULL, "pvtcp");
   if (rc) {
      CommOS_Debug(("%s: Could not add state kobject to parent kset [%d]!\n",
                    __func__, rc));
      goto error;
   }

   loopbackNetif = PvtcpStateFindIf(state, pvtcpIfLoopbackInet4);
   BUG_ON(loopbackNetif == NULL);
   loopbackNetif->conf.addr.in.s_addr = GetLoopbackAddr();
   if (loopbackNetif->conf.addr.in.s_addr == -1U) {
      CommOS_Log(("%s: Could not allocate loopback address!\n", __func__));
      goto error;
   }

   GetNetNamespace(state);

   stateKObj->transpArgs = transpArgs;
   stateKObj->pvsockAddr = loopbackNetif->conf.addr.in.s_addr;
#if defined(CONFIG_NET_NS)
   stateKObj->haveNS = (state->namespace != &init_net);
   stateKObj->useNS = stateKObj->haveNS;
#endif
   state->extra = stateKObj;

   _cred.uid = _cred.suid = _cred.euid = _cred.fsuid = Mvpkm_vmwareUid;
   _cred.gid = _cred.sgid = _cred.egid = _cred.fsgid = Mvpkm_vmwareGid;


out:
   if (kset) {
      kset_put(kset);
   }
   return state;

error:
   if (stateKObj) {
      kobject_del(&stateKObj->kobj);
      kobject_put(&stateKObj->kobj);
   }
   if (loopbackNetif && (loopbackNetif->conf.addr.in.s_addr != -1U)) {
      PutLoopbackAddr(loopbackNetif->conf.addr.in.s_addr);
   }
   if (state) {
      PvtcpStateFree(state);
      state = NULL;
   }
   goto out;
}



static void
StateFree(void *arg)
{
   PvtcpState *state = arg;
   PvtcpIf *loopbackNetif;
   void *namespace;

   if (!state) {
      return;
   }

   if (state->extra) {
      PvtcpStateKObj *stateKObj = state->extra;

      kobject_del(&stateKObj->kobj);
      kobject_put(&stateKObj->kobj);
   }

   namespace = state->namespace;
   loopbackNetif = PvtcpStateFindIf(state, pvtcpIfLoopbackInet4);
   BUG_ON(loopbackNetif == NULL);
   PutLoopbackAddr(loopbackNetif->conf.addr.in.s_addr);
   PvtcpStateFree(state);
   PutNetNamespace(namespace);
}



void
PvtcpReleaseSocket(PvtcpSock *pvsk)
{
   struct socket *sock = SkFromPvsk(pvsk)->sk_socket;

   PvtcpHoldSock(pvsk);
   SOCK_IN_LOCK(pvsk);
   SOCK_OUT_LOCK(pvsk);
   pvsk->peerSockSet = 0;
   SockReleaseWrapper(sock);
   SOCK_OUT_UNLOCK(pvsk);
   SOCK_IN_UNLOCK(pvsk);
   PvtcpPutSock(pvsk);
   CommOS_Debug(("%s: [0x%p].\n", __func__, pvsk));
}



static inline int
TestLoopbackInet4(PvtcpSock *pvsk,
                  unsigned int addr)
{
   if (!ipv4_is_loopback(addr)) {
      return 0;
   }

   if (addr != htonl(PVTCP_PVSOCK_ADDR)) {
      if (addr != htonl(INADDR_LOOPBACK)) {
         return -EADDRNOTAVAIL;
      }
      if (PvtcpHasSockNamespace(pvsk)) {
         

         return 0;
      }
      return 2;
   }

   return 1;
}



int
PvtcpTestAndBindLoopbackInet4(PvtcpSock *pvsk,
                              unsigned int *addr,
                              unsigned short port)
{
   int rc;
   struct sockaddr_in sin;
   unsigned int morphedAddr;
   int propagate = 0;

   rc = TestLoopbackInet4(pvsk, *addr);
   switch (rc) {
   case 2:
      propagate = 1; 
   case 1:
      break; 
   case 0:
      return 1; 
   default:
      return rc;
   }

   if (pvsk->netif->conf.family == PVTCP_PF_LOOPBACK_INET4) {
      

      morphedAddr = pvsk->netif->conf.addr.in.s_addr;
      rc = 0;
      goto out;
   }


   PvtcpSwitchSock(pvsk, PVTCP_SOCK_NAMESPACE_INITIAL);
   PvtcpStateAddSocket(pvsk->channel, pvtcpIfLoopbackInet4, pvsk);
   morphedAddr = pvsk->netif->conf.addr.in.s_addr;
   memset(&sin, 0, sizeof(sin));
   sin.sin_family = AF_INET;
   sin.sin_port = port;
   sin.sin_addr.s_addr = morphedAddr;

   

   rc = kernel_bind(SkFromPvsk(pvsk)->sk_socket,
                    (struct sockaddr *)&sin, sizeof(sin));
   if (rc) {
      PvtcpSwitchSock(pvsk, PVTCP_SOCK_NAMESPACE_CHANNEL);
      PvtcpStateAddSocket(pvsk->channel, pvtcpIfUnbound, pvsk);
   } else {

      port = ntohs(port) - portRangeBase;
      if ((SkFromPvsk(pvsk)->sk_socket->type == SOCK_DGRAM) &&
          (port < portRangeSize)) {
         CommOS_MutexLock(&globalLock);
         PvtcpSetPortIndexBit(pvsk->netif->conf.addr.in.s_addr, port);
         CommOS_MutexUnlock(&globalLock);
      }

      SkFromPvsk(pvsk)->sk_socket->file = NULL;
   }

out:
   if (propagate) {
      *addr = morphedAddr;
   }
   return rc;
}



int
PvtcpTestAndBindLoopbackInet6(PvtcpSock *pvsk,
                              unsigned long long *addr0,
                              unsigned long long *addr1,
                              unsigned short port)
{
   int rc;
   struct sockaddr_in6 sin6;
   union {
      unsigned long long halves[2];
      struct in6_addr in6;
   } in6Addr = {
      .halves = { *addr0, *addr1 }
   };
   int propagate = 0;
   const int ipv6Only = 0;

   if (ipv6_addr_loopback(&in6Addr.in6)) {
      if (PvtcpHasSockNamespace(pvsk)) {
         return 1;
      }

      

      PvskSetFlag(pvsk, PVTCP_OFF_PVSKF_IPV6_LOOP, 1);
      ipv6_addr_set_v4mapped(htonl(INADDR_LOOPBACK), &in6Addr.in6);
   }

   if (!ipv6_addr_v4mapped(&in6Addr.in6)) {
      

      return 1;
   }

   rc = TestLoopbackInet4(pvsk, in6Addr.in6.s6_addr32[3]);
   switch (rc) {
   case 2:
      propagate = 1; 
   case 1:
      break; 
   case 0:
      return 1; 
   default:
      return rc;
   }

   if (pvsk->netif->conf.family == PVTCP_PF_LOOPBACK_INET4) {
      

      ipv6_addr_set_v4mapped(pvsk->netif->conf.addr.in.s_addr, &in6Addr.in6);
      rc = 0;
      goto out;
   }


   PvtcpSwitchSock(pvsk, PVTCP_SOCK_NAMESPACE_INITIAL);
   PvtcpStateAddSocket(pvsk->channel, pvtcpIfLoopbackInet4, pvsk);
   ipv6_addr_set_v4mapped(pvsk->netif->conf.addr.in.s_addr, &in6Addr.in6);
   memset(&sin6, 0, sizeof(sin6));
   sin6.sin6_family = AF_INET6;
   sin6.sin6_port = port;
   sin6.sin6_addr = in6Addr.in6;


   (void)kernel_setsockopt(SkFromPvsk(pvsk)->sk_socket, IPPROTO_IPV6,
                           IPV6_V6ONLY, (char *)&ipv6Only, sizeof(ipv6Only));
   rc = kernel_bind(SkFromPvsk(pvsk)->sk_socket,
                    (struct sockaddr *)&sin6, sizeof(sin6));
   if (rc) {
      PvtcpSwitchSock(pvsk, PVTCP_SOCK_NAMESPACE_CHANNEL);
      PvtcpStateAddSocket(pvsk->channel, pvtcpIfUnbound, pvsk);
   } else {

      port = ntohs(port) - portRangeBase;
      if ((SkFromPvsk(pvsk)->sk_socket->type == SOCK_DGRAM) &&
          (port < portRangeSize)) {
         CommOS_MutexLock(&globalLock);
         PvtcpSetPortIndexBit(pvsk->netif->conf.addr.in.s_addr, port);
         CommOS_MutexUnlock(&globalLock);
      }

      SkFromPvsk(pvsk)->sk_socket->file = NULL;
   }

out:
   if (propagate) {
      *addr0 = in6Addr.halves[0];
      *addr1 = in6Addr.halves[1];
   }
   return rc;
}



void
PvtcpResetLoopbackInet4(PvtcpSock *pvsk,
                        unsigned int *addr)
{
   if (!PvtcpHasSockNamespace(pvsk)) {
      unsigned int addrOnHost = ntohl(*addr);

      
      if ((addrOnHost & PVTCP_PVSOCK_NETMASK) == PVTCP_PVSOCK_NET &&
          addrOnHost != PVTCP_PVSOCK_ADDR) {

         *addr = htonl(INADDR_LOOPBACK);
      }
   }
}



void
PvtcpResetLoopbackInet6(PvtcpSock *pvsk,
                        struct in6_addr *in6)
{
   if (!PvtcpHasSockNamespace(pvsk) && ipv6_addr_v4mapped(in6)) {
      if (PvskTestFlag(pvsk, PVTCP_OFF_PVSKF_IPV6_LOOP)) {
         

         static const struct in6_addr in6Loopback = IN6ADDR_LOOPBACK_INIT;

         *in6 = in6Loopback;
      } else {
         PvtcpResetLoopbackInet4(pvsk, &in6->s6_addr32[3]);
      }
   }
}



static int
Init(void *args)
{
   int rc = -1;

#if !defined(PVTCP_DISABLE_NETFILTER)
   rc = nf_register_hooks(netfilterHooks, ARRAY_SIZE(netfilterHooks));
   if (rc) {
      CommOS_Log(("%s: Could not register netfilter hooks!\n", __func__));
      goto out;
   } else {
      CommOS_Debug(("%s: Registered netfilter hooks.\n", __func__));
   }
   hooksRegistered = 1;
#else
   CommOS_Log(("%s: Netfilter hooks disabled.\n", __func__));
#endif

   CommOS_MutexInit(&globalLock);
   CommOS_WriteAtomic(&PvtcpOutputAIOSection, 0);
   PvtcpOffLargeDgramBufInit();

   pvtcpImpl.owner = CommOS_ModuleSelf();
   pvtcpImpl.stateCtor = StateAlloc;
   pvtcpImpl.stateDtor = StateFree;
   if (CommSvc_RegisterImpl(&pvtcpImpl) == 0) {
      rc = 0;
      pvtcpLoopbackOffAddr = GetLoopbackAddr();
      if (pvtcpLoopbackOffAddr == -1U) {
         CommOS_Log(("%s: Could not allocate offload loopback address!\n",
                     __func__));
         rc = -1;
         CommSvc_UnregisterImpl(&pvtcpImpl);
      }
   }

out:
   if (rc) {
      if (hooksRegistered) {
         nf_unregister_hooks(netfilterHooks, ARRAY_SIZE(netfilterHooks));
      }
   }
   return rc;
}



static void
Exit(void)
{
   PutLoopbackAddr(pvtcpLoopbackOffAddr);
   CommSvc_UnregisterImpl(&pvtcpImpl);
#if !defined(PVTCP_DISABLE_NETFILTER)
   if (hooksRegistered) {
      nf_unregister_hooks(netfilterHooks, ARRAY_SIZE(netfilterHooks));
      CommOS_Debug(("%s: Netfilter hooks unregistered.\n", __func__));
   }
#endif
   CommOS_Log(("%s: Allocations of large datagrams: %llu.\n",
               __func__, pvtcpOffDgramAllocations));
}




int
DestructCB(struct sock *sk)
{
   PvtcpOffBuf *internalBuf;
   PvtcpOffBuf *tmp;
   PvtcpSock *pvsk = PvskFromSk(sk);

   if (!pvsk ||
       (SkFromPvsk(pvsk) != sk) ||
       (pvsk->destruct == asmDestructorShim)) {
      

      CommOS_Debug(("%s: pvsk / sk inconsistency. Ignored.\n", __func__));
      return -1;
   }

   CommOS_ListForEachSafe(&pvsk->queue, internalBuf, tmp, link) {
      CommOS_ListDel(&internalBuf->link);
      PvtcpBufFree(PvtcpOffBufFromInternal(internalBuf));
   }
   if (pvsk->destruct) {
      pvsk->destruct(sk);
   }

   if (pvsk->rpcReply) {
      CommOS_Kfree(pvsk->rpcReply);
   }

   CommOS_Kfree(pvsk);


   return 0;
}



static void
StateChangeCB(struct sock *sk)
{
   PvtcpSock *pvsk = PvskFromSk(sk);

   if (!pvsk ||
       (SkFromPvsk(pvsk) != sk) ||
       (pvsk->stateChange == StateChangeCB)) {
      CommOS_Debug(("%s: pvsk / sk inconsistency. Ignored.\n", __func__));
      return;
   }


   CommOS_Debug(("%s: [0x%p] sk_state [%u] sk_err [%d] sk_err_soft [%d].\n",
                 __func__, pvsk, sk->sk_state,
                 sk->sk_err, sk->sk_err_soft));
   if (pvsk->stateChange) {
      pvsk->stateChange(sk);
   }
   if (sk->sk_state == TCP_ESTABLISHED) {
      PvskSetOpFlag(pvsk, PVTCP_OP_CONNECT);
   }
   PvtcpSchedSock(pvsk);
}



static void
ErrorReportCB(struct sock *sk)
{
   PvtcpSock *pvsk = PvskFromSk(sk);

   if (!pvsk ||
       (SkFromPvsk(pvsk) != sk) ||
       (pvsk->errorReport == ErrorReportCB)) {
      CommOS_Debug(("%s: pvsk / sk inconsistency. Ignored\n", __func__));
      return;
   }


   CommOS_Debug(("%s: [0x%p] sk_err [%d] sk_err_soft [%d].\n",
                 __func__, pvsk, sk->sk_err, sk->sk_err_soft));
   if (pvsk->errorReport) {
      pvsk->errorReport(sk);
   }
   pvsk->err = sk->sk_err;
   PvtcpSchedSock(pvsk);
}



static void
DataReadyCB(struct sock *sk,
            int bytes)
{
   PvtcpSock *pvsk = PvskFromSk(sk);

   if (!pvsk ||
       (SkFromPvsk(pvsk) != sk) ||
       (pvsk->dataReady == DataReadyCB)) {
      CommOS_Debug(("%s: pvsk / sk inconsistency. Ignored.\n", __func__));
      return;
   }


   if (pvsk->dataReady) {
      pvsk->dataReady(sk, bytes);
   }
   if (sk->sk_state == TCP_LISTEN) {
      CommOS_Debug(("%s: Listen socket ready to accept [0x%p].\n",
                    __func__, pvsk));
   }
   PvtcpSchedSock(pvsk);
}



static void
WriteSpaceCB(struct sock *sk)
{
   PvtcpSock *pvsk = PvskFromSk(sk);

   if (!pvsk ||
       (SkFromPvsk(pvsk) != sk) ||
       (pvsk->writeSpace == WriteSpaceCB)) {
      CommOS_Debug(("%s: pvsk / sk inconsistency. Ignored.\n", __func__));
      return;
   }


   if (pvsk->writeSpace) {
      pvsk->writeSpace(sk);
   }
   PvtcpSchedSock(pvsk);
}



static int
SockAllocInit(struct socket *sock,
              CommChannel channel,
              unsigned long long peerSock,
              PvtcpSock *parentPvsk)
{
   struct sock *sk;
   struct inode *inode;
   PvtcpSock *pvsk;
   int sndBuf = PVTCP_SOCK_RCVSIZE * 4;

   if (!sock || !channel || !peerSock) {
      return -EINVAL;
   }

   sk = sock->sk;
   sk->sk_user_data = NULL;

   pvsk = CommOS_Kmalloc(sizeof(*pvsk));
   if (!pvsk) {
      return -ENOMEM;
   }

   if (PvtcpOffSockInit(pvsk, channel)) {
      CommOS_Kfree(pvsk);
      return -ENOMEM;
   }

   sk->sk_socket->file = &_file;

   inode = SOCK_INODE(sock);
   inode->i_uid = _cred.fsuid;
   inode->i_gid = _cred.fsgid;

   
   pvsk->peerSock = peerSock;
   pvsk->peerSockSet = 1;

   
   pvsk->sk = sk;

   
   if (PvtcpStateAddSocket(channel, pvtcpIfUnbound, pvsk) != 0) {
      CommOS_Kfree(pvsk);
      return -ENOMEM;
   }

   CommOS_ModuleGet(pvtcpImpl.owner);

   if (!parentPvsk) {
      pvsk->destruct = sk->sk_destruct;
      sk->sk_destruct = asmDestructorShim;
      pvsk->stateChange = sk->sk_state_change;
      sk->sk_state_change = StateChangeCB;
      pvsk->errorReport = sk->sk_error_report;
      sk->sk_error_report = ErrorReportCB;
      pvsk->dataReady = sk->sk_data_ready;
      sk->sk_data_ready = DataReadyCB;
      pvsk->writeSpace = sk->sk_write_space;
      sk->sk_write_space = WriteSpaceCB;
   } else {

      pvsk->destruct = parentPvsk->destruct;
      sk->sk_destruct = asmDestructorShim;
      pvsk->stateChange = parentPvsk->stateChange;
      sk->sk_state_change = StateChangeCB;
      pvsk->errorReport = parentPvsk->errorReport;
      sk->sk_error_report = ErrorReportCB;
      pvsk->dataReady = parentPvsk->dataReady;
      sk->sk_data_ready = DataReadyCB;
      pvsk->writeSpace = parentPvsk->writeSpace;
      sk->sk_write_space = WriteSpaceCB;

      if (parentPvsk->netif->conf.family == PVTCP_PF_LOOPBACK_INET4) {
         

         PvtcpSwitchSock(pvsk, PVTCP_SOCK_NAMESPACE_INITIAL);
         PvtcpStateAddSocket(pvsk->channel, pvtcpIfLoopbackInet4, pvsk);
      }
   }

   
   sk->sk_user_data = pvsk;


   kernel_setsockopt(sock, SOL_SOCKET, SO_SNDBUFFORCE,
                     (void *)&sndBuf, sizeof(sndBuf));

   return 0;
}



static PvtcpSock *
SockAllocErrInit(int err,
                 CommChannel channel,
                 unsigned long long peerSock)
{
   PvtcpSock *pvsk;

   if (!channel || !peerSock) {
      return NULL;
   }

   pvsk = CommOS_Kmalloc(sizeof(*pvsk));
   if (!pvsk) {
      return NULL;
   }

   if (PvtcpOffSockInit(pvsk, channel)) {
      CommOS_Kfree(pvsk);
      return NULL;
   }

   
   pvsk->peerSock = peerSock;
   pvsk->peerSockSet = 1;
   pvsk->err = err;

   
   pvsk->sk = NULL;
   return pvsk;
}




void
PvtcpCreateOp(CommChannel channel,
              void *upperLayerState,
              CommPacket *packet,
              struct kvec *vec,
              unsigned int vecLen)
{
   int rc;
   struct socket *sock;
   PvtcpSock *pvsk;
   PvtcpState *state = (PvtcpState *)upperLayerState;
   const int enable = 1;

   PVTCP_UNLOCK_DISP_DISCARD_VEC();

#if defined(PVTCP_IPV6_DISABLE)
   if (packet->data16 == AF_INET6) {
      CommOS_Debug(("%s: AF_INET6 support is disabled.\n", __func__));
      rc = -EAFNOSUPPORT;
   } else
#endif
   {
      rc = sock_create_kern(packet->data16, packet->data32,
                            packet->data32ex, &sock);
   }

   if (!rc) {
      rc = SockAllocInit(sock, channel, packet->data64, NULL);
      if (rc) {
         SockReleaseWrapper(sock);
         goto fail;
      }
      kernel_setsockopt(sock, SOL_SOCKET, SO_REUSEADDR,
                        (void *)&enable, sizeof(enable));
      pvsk = PvskFromSk(sock->sk);
      if (state->extra &&
          ((PvtcpStateKObj *)(state->extra))->useNS) {
         PvtcpSwitchSock(pvsk, PVTCP_SOCK_NAMESPACE_CHANNEL);
      } else {
         PvtcpSwitchSock(pvsk, PVTCP_SOCK_NAMESPACE_INITIAL);
      }
      PvtcpStateAddSocket(pvsk->channel, pvtcpIfUnbound, pvsk);
      PvskSetOpFlag(pvsk, PVTCP_OP_CREATE);
   } else {
      CommOS_Debug(("%s: Error creating offload socket: %d\n",
                    __func__, rc));
      pvsk = SockAllocErrInit(-rc, channel, packet->data64);
      if (!pvsk) {
         goto fail;
      }
   }

   PvtcpSchedSock(pvsk);
   return;

fail:
   CommOS_Log(("%s: BOOG ** FAILED TO CREATE OFFLOAD SOCKET [%d] "
               "_AND_ ERROR REPORTING SOCKET!\n"
               " PV SIDE MAY BE LOCKED UP UNTIL CREATE RPC TIMES OUT!",
               __func__, rc));
}



void
PvtcpReleaseOp(CommChannel channel,
               void *upperLayerState,
               CommPacket *packet,
               struct kvec *vec,
               unsigned int vecLen)
{
   PvtcpSock *pvsk = PvtcpGetPvskOrReturn(packet->data64, upperLayerState);
   struct sock *sk = SkFromPvsk(pvsk);


   if ((sk->sk_socket->type == SOCK_DGRAM) &&
       (pvsk->netif->conf.family == PVTCP_PF_LOOPBACK_INET4)) {
      unsigned short port = 0;

      if (sk->sk_family == AF_INET) {
         struct sockaddr_in sin = { .sin_family = AF_INET };
         int addrLen = sizeof(sin);

         if (!kernel_getsockname(sk->sk_socket,
                                 (struct sockaddr *)&sin, &addrLen)) {
            port = sin.sin_port;
         }
      } else { 
         struct sockaddr_in6 sin = { .sin6_family = AF_INET6 };
         int addrLen = sizeof(sin);

         if (!kernel_getsockname(sk->sk_socket,
                                 (struct sockaddr *)&sin, &addrLen)) {
            port = sin.sin6_port;
         }
      }

      port = ntohs(port) - portRangeBase;
      if (port < portRangeSize) {
         CommOS_MutexLock(&globalLock);
         PvtcpResetPortIndexBit(pvsk->netif->conf.addr.in.s_addr, port);
         CommOS_MutexUnlock(&globalLock);
      }
   }


   PvtcpStateRemoveSocket(pvsk->channel, pvsk);
   PvtcpHoldSock(pvsk);
   SOCK_STATE_LOCK(pvsk);
   pvsk->peerSockSet = 0;
   SOCK_STATE_UNLOCK(pvsk);
   PvskSetOpFlag(pvsk, PVTCP_OP_RELEASE);
   PvtcpSchedSock(pvsk);
   PvtcpPutSock(pvsk);
   PVTCP_UNLOCK_DISP_DISCARD_VEC();
}



void
PvtcpBindOp(CommChannel channel,
            void *upperLayerState,
            CommPacket *packet,
            struct kvec *vec,
            unsigned int vecLen)
{
   PvtcpSock *pvsk = PvtcpGetPvskOrReturn(packet->data64, upperLayerState);
   struct sock *sk = SkFromPvsk(pvsk);
   struct sockaddr *addr;
   struct sockaddr_in sin;
   struct sockaddr_in6 sin6;
   int reuseAddr;
   int addrLen;
   int rc;

   PvtcpHoldSock(pvsk);
   PVTCP_UNLOCK_DISP_DISCARD_VEC();


   reuseAddr = COMM_OPF_GET_VAL(packet->flags);
   if ((reuseAddr == 1) || (reuseAddr == 2)) {
      

      reuseAddr--;
      kernel_setsockopt(sk->sk_socket, SOL_SOCKET, SO_REUSEADDR,
                        (void *)&reuseAddr, sizeof(reuseAddr));
   }

   if (sk->sk_family == AF_INET) {
      memset(&sin, 0, sizeof(sin));
      sin.sin_family = AF_INET;
      sin.sin_port = packet->data16;
      sin.sin_addr.s_addr = (unsigned int)packet->data64ex;
      addr = (struct sockaddr *)&sin;
      addrLen = sizeof(sin);

      rc = PvtcpTestAndBindLoopbackInet4(pvsk, &sin.sin_addr.s_addr,
                                         sin.sin_port);
      if (rc <= 0) {
         

         pvsk->err = -rc;
         goto out;
      }
   } else { 
      memset(&sin6, 0, sizeof(sin6));
      sin6.sin6_family = AF_INET6;
      sin6.sin6_port = packet->data16;
      addr = (struct sockaddr *)&sin6;
      addrLen = sizeof(sin6);

      rc = PvtcpTestAndBindLoopbackInet6(pvsk, &packet->data64ex,
                                         &packet->data64ex2, sin6.sin6_port);
      if (rc <= 0) {
         

         pvsk->err = -rc;
         goto out;
      }
      PvtcpI6AddrUnpack(&sin6.sin6_addr.s6_addr32[0],
                        packet->data64ex, packet->data64ex2);
   }

   
   pvsk->err = -kernel_bind(sk->sk_socket, addr, addrLen);

out:
   PvskSetOpFlag(pvsk, PVTCP_OP_BIND);
   PvtcpSchedSock(pvsk);
   PvtcpPutSock(pvsk);
}


void
PvtcpSetSockOptOp(CommChannel channel,
                  void *upperLayerState,
                  CommPacket *packet,
                  struct kvec *vec,
                  unsigned int vecLen)
{
   PvtcpSock *pvsk = PvtcpGetPvskOrReturn(packet->data64, upperLayerState);
   struct sock *sk = SkFromPvsk(pvsk);
   struct socket *sock = sk->sk_socket;
   unsigned int optlen = packet->len - sizeof(*packet);

   PvtcpHoldSock(pvsk);

   if ((vecLen != 1) || (vec[0].iov_len != optlen) || (optlen < sizeof(int))) {
      pvsk->rpcStatus = -EINVAL;
      goto out;
   }

   if (packet->data32 == SOL_TCP) {

      int on;

      switch (packet->data32ex) {
      case TCP_NODELAY:
         memcpy(&on, vec[0].iov_base, sizeof(on));
         PvskSetFlag(pvsk, PVTCP_OFF_PVSKF_TCP_NODELAY, on);
         pvsk->rpcStatus = 0;
         goto out;
      case TCP_CORK:
         memcpy(&on, vec[0].iov_base, sizeof(on));
         PvskSetFlag(pvsk, PVTCP_OFF_PVSKF_TCP_CORK, on);
         pvsk->rpcStatus = 0;
         goto out;
      }
   }

   pvsk->rpcStatus = kernel_setsockopt(sock,
                                       packet->data32,
                                       packet->data32ex,
                                       vec[0].iov_base,
                                       optlen);

out:
   PVTCP_UNLOCK_DISP_DISCARD_VEC();
   PvskSetOpFlag(pvsk, PVTCP_OP_SETSOCKOPT);
   PvtcpSchedSock(pvsk);
   PvtcpPutSock(pvsk);
}


void
PvtcpGetSockOptOp(CommChannel channel,
                  void *upperLayerState,
                  CommPacket *packet,
                  struct kvec *vec,
                  unsigned int vecLen)
{
   PvtcpSock *pvsk = PvtcpGetPvskOrReturn(packet->data64, upperLayerState);
   struct sock *sk = SkFromPvsk(pvsk);
   struct socket *sock = sk->sk_socket;
   unsigned int optLen = (unsigned int)(packet->data64ex);
   char *optBuf;
   int rc = 0;

   PvtcpHoldSock(pvsk);

   if ((optLen < sizeof(int)) || (optLen > PVTCP_SOCK_SAFE_RCVSIZE)) {
      pvsk->rpcStatus = -EINVAL;
      goto out;
   }

   optBuf = CommOS_Kmalloc(optLen);
   if (!optBuf) {
      pvsk->rpcStatus = -EINVAL;
      goto out;
   }

   if (packet->data32 == SOL_TCP) {

      int on;

      switch (packet->data32ex) {
      case TCP_NODELAY:
         on = PvskTestFlag(pvsk, PVTCP_OFF_PVSKF_TCP_NODELAY);
         optLen = sizeof(on);
         memcpy(optBuf, &on, optLen);
         goto done;
      case TCP_CORK:
         on = PvskTestFlag(pvsk, PVTCP_OFF_PVSKF_TCP_CORK);
         optLen = sizeof(on);
         memcpy(optBuf, &on, optLen);
         goto done;
      }
   }

   rc = kernel_getsockopt(sock, packet->data32,
                          packet->data32ex, optBuf, &optLen);

done:
   if (!rc) {
      pvsk->rpcReply = optBuf;
      CommOS_MemBarrier();
      pvsk->rpcStatus = (int)optLen;
   } else {
      CommOS_Kfree(optBuf);
      pvsk->rpcStatus = rc;
   }

out:
   PVTCP_UNLOCK_DISP_DISCARD_VEC();
   PvskSetOpFlag(pvsk, PVTCP_OP_GETSOCKOPT);
   PvtcpSchedSock(pvsk);
   PvtcpPutSock(pvsk);
}



void
PvtcpIoctlOp(CommChannel channel,
           void *state,
           CommPacket *packet,
           struct kvec *vec,
           unsigned int vecLen)
{
   PvtcpSock *pvsk = PvtcpGetPvskOrReturn(packet->data64, state);
   struct sock *sk = SkFromPvsk(pvsk);
   struct socket *sock = sk->sk_socket;

   PvtcpHoldSock(pvsk);

   

   (void)sock;
   pvsk->rpcStatus = -ENOIOCTLCMD;

   PVTCP_UNLOCK_DISP_DISCARD_VEC();
   PvskSetOpFlag(pvsk, PVTCP_OP_IOCTL);
   PvtcpSchedSock(pvsk);
   PvtcpPutSock(pvsk);
}



void
PvtcpListenOp(CommChannel channel,
              void *upperLayerState,
              CommPacket *packet,
              struct kvec *vec,
              unsigned int vecLen)
{
   PvtcpSock *pvsk = PvtcpGetPvskOrReturn(packet->data64, upperLayerState);
   struct sock *sk = SkFromPvsk(pvsk);
   int backlog = (int)packet->data32;

   PvtcpHoldSock(pvsk);
   PVTCP_UNLOCK_DISP_DISCARD_VEC();

   pvsk->err = -kernel_listen(sk->sk_socket, backlog);
   PvskSetOpFlag(pvsk, PVTCP_OP_LISTEN);
   PvtcpSchedSock(pvsk);
   PvtcpPutSock(pvsk);
}



void
PvtcpAcceptOp(CommChannel channel,
              void *upperLayerState,
              CommPacket *packet,
              struct kvec *vec,
              unsigned int vecLen)
{
   int rc;
   PvtcpSock *pvsk = PvtcpGetPvskOrReturn(packet->data64, upperLayerState);
   struct sock *sk = SkFromPvsk(pvsk);
   struct socket *newsock = NULL;

   PvtcpHoldSock(pvsk);
   PVTCP_UNLOCK_DISP_DISCARD_VEC();

   rc = kernel_accept(sk->sk_socket, &newsock, O_NONBLOCK);
   if (rc == 0) {
      rc = SockAllocInit(newsock, channel, packet->data64ex, pvsk);
      if (rc) {
         SockReleaseWrapper(newsock);
      }
   }

   if (rc == 0) {
      struct sock *newsk = newsock->sk;
      PvtcpSock *newpvsk = PvskFromSk(newsk);

      

      newpvsk->state = (PvtcpState *)pvsk;
      PvskSetOpFlag(newpvsk, PVTCP_OP_ACCEPT);
      PvtcpSchedSock(newpvsk);
   } else {
      pvsk->err = -rc;
      PvskSetOpFlag(pvsk, PVTCP_OP_ACCEPT);
      PvtcpSchedSock(pvsk);
   }

   PvtcpPutSock(pvsk);
}



void
PvtcpConnectOp(CommChannel channel,
               void *upperLayerState,
               CommPacket *packet,
               struct kvec *vec,
               unsigned int vecLen)
{
   PvtcpSock *pvsk = PvtcpGetPvskOrReturn(packet->data64, upperLayerState);
   struct sock *sk = SkFromPvsk(pvsk);
   struct sockaddr *addr;
   struct sockaddr_in sin;
   struct sockaddr_in6 sin6;
   int addrLen;
   int flags = 0;
   int rc = 0;
   int disconnect = 0;

   PvtcpHoldSock(pvsk);
   PVTCP_UNLOCK_DISP_DISCARD_VEC();

   if (sk->sk_family == AF_INET) {
      addr = (struct sockaddr *)&sin;
      addrLen = sizeof(sin);
      memset(&sin, 0, sizeof(sin));
      sin.sin_port = packet->data16;
      sin.sin_addr.s_addr = (unsigned int)packet->data64ex;
      if (COMM_OPF_GET_VAL(packet->flags)) {
         sin.sin_family = AF_UNSPEC;
         disconnect = 1;
         goto connect;
      }
      sin.sin_family = AF_INET;
      PvtcpTestAndBindLoopbackInet4(pvsk, &sin.sin_addr.s_addr, 0);
   } else { 
      addr = (struct sockaddr *)&sin6;
      addrLen = sizeof(sin6);
      memset(&sin6, 0, sizeof(sin6));
      sin6.sin6_port = packet->data16;
      if (COMM_OPF_GET_VAL(packet->flags)) {
         sin6.sin6_family = AF_UNSPEC;
         PvtcpI6AddrUnpack(&sin6.sin6_addr.s6_addr32[0],
                           packet->data64ex, packet->data64ex2);
         disconnect = 1;
         goto connect;
      }
      sin6.sin6_family = AF_INET6;
      PvtcpTestAndBindLoopbackInet6(pvsk, &packet->data64ex,
                                    &packet->data64ex2, 0);
      PvtcpI6AddrUnpack(&sin6.sin6_addr.s6_addr32[0],
                        packet->data64ex, packet->data64ex2);
   }

connect:
   rc = kernel_connect(sk->sk_socket, addr, addrLen, flags | O_NONBLOCK);


   if (rc && (sk->sk_socket->type == SOCK_DGRAM)) {
      pvsk->err = -rc;
   }


   PvskSetFlag(pvsk, PVTCP_OFF_PVSKF_DISCONNECT, disconnect);
   PvskSetOpFlag(pvsk, PVTCP_OP_CONNECT);
   PvtcpSchedSock(pvsk);
   PvtcpPutSock(pvsk);
}



void
PvtcpShutdownOp(CommChannel channel,
                void *upperLayerState,
                CommPacket *packet,
                struct kvec *vec,
                unsigned int vecLen)
{
   PvtcpSock *pvsk = PvtcpGetPvskOrReturn(packet->data64, upperLayerState);
   int how = (int)packet->data32;

   PvtcpHoldSock(pvsk);
   if ((how == SHUT_RD) || (how == SHUT_RDWR)) {
      kernel_sock_shutdown(SkFromPvsk(pvsk)->sk_socket, SHUT_RD);
      PvskSetFlag(pvsk, PVTCP_OFF_PVSKF_SHUT_RD, 1);
   }
   if ((how == SHUT_WR) || (how == SHUT_RDWR)) {
      PvskSetFlag(pvsk, PVTCP_OFF_PVSKF_SHUT_WR, 1);
   }
   PVTCP_UNLOCK_DISP_DISCARD_VEC();
   PvtcpSchedSock(pvsk);
   PvtcpPutSock(pvsk);
}




static inline void
ReleaseAIO(PvtcpSock *pvsk)
{
   struct sock *sk = SkFromPvsk(pvsk);
   struct socket *sock = sk->sk_socket;
   CommPacket packet = {
      .len = sizeof(packet),
      .flags = 0,
      .opCode = PVTCP_OP_RELEASE,
      .data64 = pvsk->peerSock,
      .data64ex = PvtcpGetHandle(pvsk)
   };
   unsigned long long timeout = COMM_MAX_TO;

   SOCK_OUT_LOCK(pvsk);
   CommSvc_Write(pvsk->channel, &packet, &timeout);
#if defined(PVTCP_FULL_DEBUG)
   CommOS_Debug(("%s: Sent 'Release' [0x%p] -> 0x%0x] reply.\n",
                 __func__, pvsk, (unsigned)(pvsk->peerSock)));
#endif
   SockReleaseWrapper(sock);
   SOCK_OUT_UNLOCK(pvsk);
}



static inline void
CreateAIO(PvtcpSock *pvsk)
{
   struct sock *sk;
   struct socket *sock;
   CommPacket packet = {
      .len = sizeof(packet),
      .flags = 0,
      .opCode = PVTCP_OP_CREATE,
      .data64 = pvsk->peerSock,
   };
   unsigned long long timeout = COMM_MAX_TO;
   int rc;

   sk = SkFromPvsk(pvsk);
   if (!sk) {

      return;
   }

   sock = sk->sk_socket;
   packet.data64ex = PvtcpGetHandle(pvsk);

   rc = CommSvc_Write(pvsk->channel, &packet, &timeout);
   if (rc != packet.len) {
      

      PvtcpStateRemoveSocket(pvsk->channel, pvsk);
      SockReleaseWrapper(sock);
      CommOS_Log(("%s: BOOG -- Couldn't send 'Create' reply [0x%p]!\n",
                  __func__, sk));
   } else {
#if defined(PVTCP_FULL_DEBUG)
      CommOS_Debug(("%s: Sent 'Create' [0x%p] reply [%d].\n",
                    __func__, pvsk, rc));
#endif
   }
}



static inline void
BindAIO(PvtcpSock *pvsk)
{
   struct sock *sk = SkFromPvsk(pvsk);
   struct socket *sock = sk->sk_socket;
   CommPacket packet = {
      .len = sizeof(packet),
      .flags = 0,
      .opCode = PVTCP_OP_BIND,
      .data64 = pvsk->peerSock
   };
   unsigned long long timeout = COMM_MAX_TO;
   int rc;

   if (pvsk->peerSockSet) {
      if (sk->sk_family == AF_INET) {
         struct sockaddr_in sin = { .sin_family = AF_INET };
         int addrLen = sizeof(sin);

         rc = kernel_getsockname(sock, (struct sockaddr *)&sin, &addrLen);
         if (rc == 0) {
            packet.data16 = sin.sin_port;
            PvtcpResetLoopbackInet4(pvsk, &sin.sin_addr.s_addr);
            packet.data64ex = (unsigned long long)sin.sin_addr.s_addr;
         }
      } else { 
         struct sockaddr_in6 sin = { .sin6_family = AF_INET6 };
         int addrLen = sizeof(sin);

         rc = kernel_getsockname(sock, (struct sockaddr *)&sin, &addrLen);
         if (rc == 0) {
            packet.data16 = sin.sin6_port;
            PvtcpResetLoopbackInet6(pvsk, &sin.sin6_addr);
            PvtcpI6AddrPack(&sin.sin6_addr.s6_addr32[0],
                            &packet.data64ex, &packet.data64ex2);
         }
      }

      if (rc) {
         COMM_OPF_SET_ERR(packet.flags);
         packet.data32ex = (unsigned int)(-rc);
         packet.opCode = PVTCP_OP_FLOW;
      }
      CommSvc_Write(pvsk->channel, &packet, &timeout);
#if defined(PVTCP_FULL_DEBUG)
      CommOS_Debug(("%s: Sent 'Bind' [0x%p, %d] reply.\n",
                    __func__, pvsk, rc));
#endif
   }
}



static inline void
SetSockOptAIO(PvtcpSock *pvsk)
{
   CommPacket packet;
   unsigned long long timeout;

   packet.len    = sizeof(packet);
   packet.flags  = 0;
   packet.opCode = PVTCP_OP_SETSOCKOPT;
   packet.data64 = pvsk->peerSock;
   packet.data32 = (unsigned int)(pvsk->rpcStatus);
   timeout = COMM_MAX_TO;
   CommSvc_Write(pvsk->channel, &packet, &timeout);
   pvsk->rpcStatus = 0;
}



static inline void
GetSockOptAIO(PvtcpSock *pvsk)
{
   CommPacket packet = {
      .len = sizeof(packet),
      .opCode = PVTCP_OP_GETSOCKOPT,
      .flags = 0
   };
   unsigned long long timeout = COMM_MAX_TO;

   struct kvec vec[1];
   struct kvec *inVec = vec;
   unsigned int vecLen = 1;
   unsigned int iovOffset = 0;

   if (pvsk->rpcStatus > 0) {
      packet.len = sizeof(packet) + pvsk->rpcStatus;
      vec[0].iov_base = pvsk->rpcReply;
      vec[0].iov_len = pvsk->rpcStatus;
   } else {
      vecLen = 0;
   }

   packet.data64 = pvsk->peerSock;
   packet.data32 = pvsk->rpcStatus;

   CommSvc_WriteVec(pvsk->channel, &packet, &inVec, &vecLen,
                    &timeout, &iovOffset, 1);

   if (pvsk->rpcReply) {
      CommOS_Kfree(pvsk->rpcReply);
      pvsk->rpcReply = NULL;
   }
   pvsk->rpcStatus = 0;
}



static inline void
IoctlAIO(PvtcpSock *pvsk)
{
   CommPacket packet = {
      .len = sizeof(packet),
      .opCode = PVTCP_OP_IOCTL,
      .flags = 0
   };
   unsigned long long timeout = COMM_MAX_TO;

   packet.data64 = pvsk->peerSock;
   packet.data32 = pvsk->rpcStatus;
   CommSvc_Write(pvsk->channel, &packet, &timeout);
   pvsk->rpcStatus = 0;
}



static inline void
ListenAIO(PvtcpSock *pvsk)
{
   struct sock *sk = SkFromPvsk(pvsk);
   CommPacket packet = {
      .len = sizeof(packet),
      .flags = 0,
      .opCode = PVTCP_OP_LISTEN,
      .data64 = pvsk->peerSock
   };
   unsigned long long timeout = COMM_MAX_TO;

   if (pvsk->peerSockSet) {
      if (sk->sk_state != TCP_LISTEN) {
         COMM_OPF_SET_ERR(packet.flags);
         packet.data32ex = (unsigned int)pvsk->err;
         packet.opCode = PVTCP_OP_FLOW;
      }

      CommSvc_Write(pvsk->channel, &packet, &timeout);
#if defined(PVTCP_FULL_DEBUG)
      CommOS_Debug(("%s: Sent 'Listen' [0x%p] reply.\n", __func__, pvsk));
#endif
   }
}



static inline void
AcceptAIO(PvtcpSock *pvsk)
{
   struct sock *sk = SkFromPvsk(pvsk);
   struct socket *sock = sk->sk_socket;
   CommPacket packet = {
      .len = sizeof(packet),
      .flags = 0,
      .opCode = PVTCP_OP_ACCEPT
   };
   unsigned long long timeout = COMM_MAX_TO;
   const int enable = 1;
   int rc;

   if (pvsk->peerSockSet) {
      unsigned long long payloadSocks[2] = { 0, 0 };
      struct kvec payloadVec[] = {
         { .iov_base = &payloadSocks, .iov_len = sizeof(payloadSocks) }
      };
      struct kvec *payload = payloadVec;
      unsigned int payloadLen = 1;
      unsigned int iovOffset = 0;

      packet.len = sizeof(packet) + sizeof(payloadSocks);


      packet.data64 = ((PvtcpSock *)pvsk->state)->peerSock;
      pvsk->state = CommSvc_GetState(pvsk->channel);

      payloadSocks[0] = pvsk->peerSock;
      payloadSocks[1] = PvtcpGetHandle(pvsk);

      rc = 0;
      if (sk->sk_family == AF_INET) {
         struct sockaddr_in sin = { .sin_family = AF_INET };
         int addrLen = sizeof(sin);

         rc = kernel_getpeername(sock, (struct sockaddr *)&sin, &addrLen);
         if (rc == 0) {
            packet.data16 = sin.sin_port;
            PvtcpResetLoopbackInet4(pvsk, &sin.sin_addr.s_addr);
            packet.data64ex = (unsigned long long)sin.sin_addr.s_addr;
         }
      } else { 
         struct sockaddr_in6 sin = { .sin6_family = AF_INET6 };
         int addrLen = sizeof(sin);

         rc = kernel_getpeername(sock, (struct sockaddr *)&sin, &addrLen);
         if (rc == 0) {
            packet.data16 = sin.sin6_port;
            PvtcpResetLoopbackInet6(pvsk, &sin.sin6_addr);
            PvtcpI6AddrPack(&sin.sin6_addr.s6_addr32[0],
                            &packet.data64ex, &packet.data64ex2);
         }
      }

      if (rc == 0) {
         kernel_setsockopt(sock, SOL_TCP, TCP_NODELAY,
                           (void *)&enable, sizeof(enable));
         kernel_setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE,
                           (void *)&enable, sizeof(enable));
         kernel_setsockopt(sock, SOL_SOCKET, SO_OOBINLINE,
                           (void *)&enable, sizeof(enable));
      } else {
         PvtcpStateRemoveSocket(pvsk->channel, pvsk);
         SockReleaseWrapper(sock);
         COMM_OPF_SET_ERR(packet.flags);
         packet.data32ex = (unsigned int)ECONNABORTED;
         packet.len = sizeof(packet);
         packet.opCode = PVTCP_OP_FLOW;
      }

      rc = CommSvc_WriteVec(pvsk->channel, &packet,
                            &payload, &payloadLen, &timeout, &iovOffset, 1);
      if ((rc != packet.len) && !COMM_OPF_TEST_ERR(packet.flags)) {
         

         PvtcpStateRemoveSocket(pvsk->channel, pvsk);
         SockReleaseWrapper(sock);
      }
#if defined(PVTCP_FULL_DEBUG)
      CommOS_Debug(("%s: Sent 'Accept' [0x%p] reply.\n", __func__, pvsk));
#endif
   }
}



static inline void
ConnectAIO(PvtcpSock *pvsk)
{
   struct sock *sk = SkFromPvsk(pvsk);
   struct socket *sock = sk->sk_socket;
   CommPacket packet = {
      .len = sizeof(packet),
      .flags = 0,
      .opCode = PVTCP_OP_CONNECT,
      .data64 = pvsk->peerSock
   };
   unsigned long long timeout = COMM_MAX_TO;
   const int enable = 1;
   int rc;

   if (!pvsk->peerSockSet ||
       (!PvskTestFlag(pvsk, PVTCP_OFF_PVSKF_DISCONNECT) &&
        (sk->sk_state != TCP_ESTABLISHED))) {
      return;
   }

   if (PvskTestFlag(pvsk, PVTCP_OFF_PVSKF_DISCONNECT)) {
      COMM_OPF_SET_VAL(packet.flags, 1);
      PvskSetFlag(pvsk, PVTCP_OFF_PVSKF_DISCONNECT, 0);
   } else if (sk->sk_state == TCP_ESTABLISHED) {
      if (sk->sk_family == AF_INET) {
         struct sockaddr_in sin = { .sin_family = AF_INET };
         int addrLen = sizeof(sin);

         rc = kernel_getsockname(sock, (struct sockaddr *)&sin, &addrLen);
         if (rc == 0) {
            packet.data16 = sin.sin_port;
            PvtcpResetLoopbackInet4(pvsk, &sin.sin_addr.s_addr);
            packet.data64ex = (unsigned long long)sin.sin_addr.s_addr;
         }
      } else { 
         struct sockaddr_in6 sin = { .sin6_family = AF_INET6 };
         int addrLen = sizeof(sin);

         rc = kernel_getsockname(sock, (struct sockaddr *)&sin, &addrLen);
         if (rc == 0) {
            packet.data16 = sin.sin6_port;
            PvtcpResetLoopbackInet6(pvsk, &sin.sin6_addr);
            PvtcpI6AddrPack(&sin.sin6_addr.s6_addr32[0],
                            &packet.data64ex, &packet.data64ex2);
         }
      }

      if (rc == 0) {
         kernel_setsockopt(sock, SOL_TCP, TCP_NODELAY,
                           (void *)&enable, sizeof(enable));
         kernel_setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE,
                           (void *)&enable, sizeof(enable));
         kernel_setsockopt(sock, SOL_SOCKET, SO_OOBINLINE,
                           (void *)&enable, sizeof(enable));
      } else {
         COMM_OPF_SET_ERR(packet.flags);
         packet.data32ex = ECONNABORTED;
         packet.opCode = PVTCP_OP_FLOW;
      }
   }

   CommSvc_Write(pvsk->channel, &packet, &timeout);
#if defined(PVTCP_FULL_DEBUG)
   CommOS_Debug(("%s: Sent 'Connect' [0x%p] reply.\n", __func__, pvsk));
#endif
}



void
PvtcpProcessAIO(CommOSWork *arg)
{
   PvtcpSock *pvsk = container_of(arg, PvtcpSock, work);
   struct sock *sk = SkFromPvsk(pvsk);

   if (!SOCK_OUT_TRYLOCK(pvsk)) {

      if (sk && sk->sk_socket) {
         PvtcpOutputAIO(pvsk);
      }
      SOCK_OUT_UNLOCK(pvsk);
   }

   

   if (!SOCK_IN_TRYLOCK(pvsk)) {

      if (sk && sk->sk_socket) {
         int err;

         

         err = PvtcpInputAIO(pvsk, perCpuBuf[smp_processor_id()]);

         

         PvtcpFlowAIO(pvsk, err);

         if (!pvsk->opFlags) {
            

            goto doneInUnlock;
         }

         if (PvskTestOpFlag(pvsk, PVTCP_OP_RELEASE)) {
            PvskResetOpFlag(pvsk, PVTCP_OP_RELEASE);
            ReleaseAIO(pvsk);

            
            goto doneInUnlock;
         }

         if (PvskTestOpFlag(pvsk, PVTCP_OP_CREATE)) {
            

            PvskResetOpFlag(pvsk, PVTCP_OP_CREATE);
            CreateAIO(pvsk);
         }

         if (PvskTestOpFlag(pvsk, PVTCP_OP_BIND)) {
            PvskResetOpFlag(pvsk, PVTCP_OP_BIND);
            BindAIO(pvsk);
         }

         if (PvskTestOpFlag(pvsk, PVTCP_OP_SETSOCKOPT)) {
            PvskResetOpFlag(pvsk, PVTCP_OP_SETSOCKOPT);
            SetSockOptAIO(pvsk);
         }

         if (PvskTestOpFlag(pvsk, PVTCP_OP_GETSOCKOPT)) {
            PvskResetOpFlag(pvsk, PVTCP_OP_GETSOCKOPT);
            GetSockOptAIO(pvsk);
         }

         if (PvskTestOpFlag(pvsk, PVTCP_OP_IOCTL)) {
            PvskResetOpFlag(pvsk, PVTCP_OP_IOCTL);
            IoctlAIO(pvsk);
         }

         if (PvskTestOpFlag(pvsk, PVTCP_OP_LISTEN)) {
            PvskResetOpFlag(pvsk, PVTCP_OP_LISTEN);
            ListenAIO(pvsk);
         }

         if (PvskTestOpFlag(pvsk, PVTCP_OP_ACCEPT)) {
            PvskResetOpFlag(pvsk, PVTCP_OP_ACCEPT);
            AcceptAIO(pvsk);
         }

         if (PvskTestOpFlag(pvsk, PVTCP_OP_CONNECT)) {
            PvskResetOpFlag(pvsk, PVTCP_OP_CONNECT);
            ConnectAIO(pvsk);
         }

doneInUnlock:
         SOCK_IN_UNLOCK(pvsk);
      } else {

         PvtcpFlowAIO(pvsk, -ENETDOWN);
      }
   } else {
      if ((pvsk->peerSockSet || PvskTestOpFlag(pvsk, PVTCP_OP_RELEASE)) &&
          sk && sk->sk_socket) {
         PvtcpSchedSock(pvsk);
      }
   }

   PvtcpPutSock(pvsk);
}

