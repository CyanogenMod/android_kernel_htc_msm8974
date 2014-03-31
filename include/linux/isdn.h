/* $Id: isdn.h,v 1.125.2.3 2004/02/10 01:07:14 keil Exp $
 *
 * Main header for the Linux ISDN subsystem (linklevel).
 *
 * Copyright 1994,95,96 by Fritz Elfert (fritz@isdn4linux.de)
 * Copyright 1995,96    by Thinking Objects Software GmbH Wuerzburg
 * Copyright 1995,96    by Michael Hipp (Michael.Hipp@student.uni-tuebingen.de)
 * 
 * This software may be used and distributed according to the terms
 * of the GNU General Public License, incorporated herein by reference.
 *
 */

#ifndef __ISDN_H__
#define __ISDN_H__

#include <linux/ioctl.h>

#define ISDN_MAX_DRIVERS    32
#define ISDN_MAX_CHANNELS   64

#define IIOCNETAIF  _IO('I',1)
#define IIOCNETDIF  _IO('I',2)
#define IIOCNETSCF  _IO('I',3)
#define IIOCNETGCF  _IO('I',4)
#define IIOCNETANM  _IO('I',5)
#define IIOCNETDNM  _IO('I',6)
#define IIOCNETGNM  _IO('I',7)
#define IIOCGETSET  _IO('I',8) 
#define IIOCSETSET  _IO('I',9) 
#define IIOCSETVER  _IO('I',10)
#define IIOCNETHUP  _IO('I',11)
#define IIOCSETGST  _IO('I',12)
#define IIOCSETBRJ  _IO('I',13)
#define IIOCSIGPRF  _IO('I',14)
#define IIOCGETPRF  _IO('I',15)
#define IIOCSETPRF  _IO('I',16)
#define IIOCGETMAP  _IO('I',17)
#define IIOCSETMAP  _IO('I',18)
#define IIOCNETASL  _IO('I',19)
#define IIOCNETDIL  _IO('I',20)
#define IIOCGETCPS  _IO('I',21)
#define IIOCGETDVR  _IO('I',22)
#define IIOCNETLCR  _IO('I',23) 
#define IIOCNETDWRSET  _IO('I',24) 

#define IIOCNETALN  _IO('I',32)
#define IIOCNETDLN  _IO('I',33)

#define IIOCNETGPN  _IO('I',34)

#define IIOCDBGVAR  _IO('I',127)

#define IIOCDRVCTL  _IO('I',128)

#define SIOCGKEEPPERIOD	(SIOCDEVPRIVATE + 0)
#define SIOCSKEEPPERIOD	(SIOCDEVPRIVATE + 1)
#define SIOCGDEBSERINT	(SIOCDEVPRIVATE + 2)
#define SIOCSDEBSERINT	(SIOCDEVPRIVATE + 3)

#define ISDN_NET_ENCAP_ETHER      0
#define ISDN_NET_ENCAP_RAWIP      1
#define ISDN_NET_ENCAP_IPTYP      2
#define ISDN_NET_ENCAP_CISCOHDLC  3 
#define ISDN_NET_ENCAP_SYNCPPP    4
#define ISDN_NET_ENCAP_UIHDLC     5
#define ISDN_NET_ENCAP_CISCOHDLCK 6 
#define ISDN_NET_ENCAP_X25IFACE   7 
#define ISDN_NET_ENCAP_MAX_ENCAP  ISDN_NET_ENCAP_X25IFACE

#define ISDN_USAGE_NONE       0
#define ISDN_USAGE_RAW        1
#define ISDN_USAGE_MODEM      2
#define ISDN_USAGE_NET        3
#define ISDN_USAGE_VOICE      4
#define ISDN_USAGE_FAX        5
#define ISDN_USAGE_MASK       7 
#define ISDN_USAGE_DISABLED  32 
#define ISDN_USAGE_EXCLUSIVE 64 
#define ISDN_USAGE_OUTGOING 128 

#define ISDN_MODEM_NUMREG    24        
#define ISDN_LMSNLEN         255 
#define ISDN_CMSGLEN	     50	 

#define ISDN_MSNLEN          32
#define NET_DV 0x06  
#define TTY_DV 0x06  

#define INF_DV 0x01  

typedef struct {
  char drvid[25];
  unsigned long arg;
} isdn_ioctl_struct;

typedef struct {
  char name[10];
  char phone[ISDN_MSNLEN];
  int  outgoing;
} isdn_net_ioctl_phone;

typedef struct {
  char name[10];     
  char master[10];   
  char slave[10];    
  char eaz[256];     
  char drvid[25];    
  int  onhtime;      
  int  charge;       
  int  l2_proto;     
  int  l3_proto;     
  int  p_encap;      
  int  exclusive;    
  int  dialmax;      
  int  slavedelay;   
  int  cbdelay;      
  int  chargehup;    
  int  ihup;         
  int  secure;       
  int  callback;     
  int  cbhup;        
  int  pppbind;      
  int  chargeint;    
  int  triggercps;   
  int  dialtimeout;  
  int  dialwait;     
  int  dialmode;     
} isdn_net_ioctl_cfg;

#define ISDN_NET_DIALMODE_MASK  0xC0    
#define ISDN_NET_DM_OFF	        0x00    
#define ISDN_NET_DM_MANUAL	0x40    
#define ISDN_NET_DM_AUTO	0x80    
#define ISDN_NET_DIALMODE(x) ((&(x))->flags & ISDN_NET_DIALMODE_MASK)

#ifdef __KERNEL__

#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/major.h>
#include <asm/io.h>
#include <linux/kernel.h>
#include <linux/signal.h>
#include <linux/slab.h>
#include <linux/timer.h>
#include <linux/wait.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/serial_reg.h>
#include <linux/fcntl.h>
#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/ip.h>
#include <linux/in.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/tcp.h>
#include <linux/mutex.h>

#define ISDN_TTY_MAJOR    43
#define ISDN_TTYAUX_MAJOR 44
#define ISDN_MAJOR        45


#define ISDN_MINOR_B        0
#define ISDN_MINOR_BMAX     (ISDN_MAX_CHANNELS-1)
#define ISDN_MINOR_CTRL     64
#define ISDN_MINOR_CTRLMAX  (64 + (ISDN_MAX_CHANNELS-1))
#define ISDN_MINOR_PPP      128
#define ISDN_MINOR_PPPMAX   (128 + (ISDN_MAX_CHANNELS-1))
#define ISDN_MINOR_STATUS   255

#ifdef CONFIG_ISDN_PPP

#ifdef CONFIG_ISDN_PPP_VJ
#  include <net/slhc_vj.h>
#endif

#include <linux/ppp_defs.h>
#include <linux/ppp-ioctl.h>

#include <linux/isdn_ppp.h>
#endif

#ifdef CONFIG_ISDN_X25
#  include <linux/concap.h>
#endif

#include <linux/isdnif.h>

#define ISDN_DRVIOCTL_MASK       0x7f  

#define ISDN_SERVICE_VOICE 1
#define ISDN_SERVICE_AB    1<<1 
#define ISDN_SERVICE_X21   1<<2
#define ISDN_SERVICE_G4    1<<3
#define ISDN_SERVICE_BTX   1<<4
#define ISDN_SERVICE_DFUE  1<<5
#define ISDN_SERVICE_X25   1<<6
#define ISDN_SERVICE_TTX   1<<7
#define ISDN_SERVICE_MIXED 1<<8
#define ISDN_SERVICE_FW    1<<9
#define ISDN_SERVICE_GTEL  1<<10
#define ISDN_SERVICE_BTXN  1<<11
#define ISDN_SERVICE_BTEL  1<<12

#define USG_NONE(x)         ((x & ISDN_USAGE_MASK)==ISDN_USAGE_NONE)
#define USG_RAW(x)          ((x & ISDN_USAGE_MASK)==ISDN_USAGE_RAW)
#define USG_MODEM(x)        ((x & ISDN_USAGE_MASK)==ISDN_USAGE_MODEM)
#define USG_VOICE(x)        ((x & ISDN_USAGE_MASK)==ISDN_USAGE_VOICE)
#define USG_NET(x)          ((x & ISDN_USAGE_MASK)==ISDN_USAGE_NET)
#define USG_FAX(x)          ((x & ISDN_USAGE_MASK)==ISDN_USAGE_FAX)
#define USG_OUTGOING(x)     ((x & ISDN_USAGE_OUTGOING)==ISDN_USAGE_OUTGOING)
#define USG_MODEMORVOICE(x) (((x & ISDN_USAGE_MASK)==ISDN_USAGE_MODEM) || \
                             ((x & ISDN_USAGE_MASK)==ISDN_USAGE_VOICE)     )

#define ISDN_TIMER_RES         4                         
#define ISDN_TIMER_02SEC       (HZ/ISDN_TIMER_RES/5)     
#define ISDN_TIMER_1SEC        (HZ/ISDN_TIMER_RES)       
#define ISDN_TIMER_RINGING     5 
#define ISDN_TIMER_KEEPINT    10 
#define ISDN_TIMER_MODEMREAD   1
#define ISDN_TIMER_MODEMPLUS   2
#define ISDN_TIMER_MODEMRING   4
#define ISDN_TIMER_MODEMXMIT   8
#define ISDN_TIMER_NETDIAL    16 
#define ISDN_TIMER_NETHANGUP  32
#define ISDN_TIMER_CARRIER   256 
#define ISDN_TIMER_FAST      (ISDN_TIMER_MODEMREAD | ISDN_TIMER_MODEMPLUS | \
                              ISDN_TIMER_MODEMXMIT)
#define ISDN_TIMER_SLOW      (ISDN_TIMER_MODEMRING | ISDN_TIMER_NETHANGUP | \
                              ISDN_TIMER_NETDIAL | ISDN_TIMER_CARRIER)

#define ISDN_TIMER_DTIMEOUT10 (10*HZ/(ISDN_TIMER_02SEC*(ISDN_TIMER_RES+1)))
#define ISDN_TIMER_DTIMEOUT15 (15*HZ/(ISDN_TIMER_02SEC*(ISDN_TIMER_RES+1)))
#define ISDN_TIMER_DTIMEOUT60 (60*HZ/(ISDN_TIMER_02SEC*(ISDN_TIMER_RES+1)))

#define ISDN_GLOBAL_STOPPED 1


#define ISDN_NET_CONNECTED  0x01       
#define ISDN_NET_SECURE     0x02       
#define ISDN_NET_CALLBACK   0x04       
#define ISDN_NET_CBHUP      0x08       
#define ISDN_NET_CBOUT      0x10       

#define ISDN_NET_MAGIC      0x49344C02 

typedef struct {
  void *next;
  char num[ISDN_MSNLEN];
} isdn_net_phone;


typedef struct isdn_net_local_s {
  ulong                  magic;
  struct net_device_stats stats;       
  int                    isdn_device;  
  int                    isdn_channel; 
  int			 ppp_slot;     
  int                    pre_device;   
  int                    pre_channel;  
  int                    exclusive;    
  int                    flags;        
  int                    dialretry;    
  int                    dialmax;      
  int                    cbdelay;      
  int                    dtimer;       
  char                   msn[ISDN_MSNLEN]; 
  u_char                 cbhup;        
  u_char                 dialstate;    
  u_char                 p_encap;      
                                       
				       
                                       
  u_char                 l2_proto;     
				       
                                       
                                       
				       
				       
				       
  u_char                 l3_proto;     
				       
                                       
                                       
  int                    huptimer;     
  int                    charge;       
  ulong                  chargetime;   
  int                    hupflags;     
				       
				       
                                       
                                       
  int                    outgoing;     
  int                    onhtime;      
  int                    chargeint;    
  int                    onum;         
  int                    cps;          
  int                    transcount;   
  int                    sqfull;       
  ulong                  sqfull_stamp; 
  ulong                  slavedelay;   
  int                    triggercps;   
  isdn_net_phone         *phone[2];    
				       
				       
  isdn_net_phone         *dial;        
  struct net_device      *master;      
  struct net_device      *slave;       
  struct isdn_net_local_s *next;       
  struct isdn_net_local_s *last;       
  struct isdn_net_dev_s  *netdev;      
  struct sk_buff_head    super_tx_queue; 
	                               
  atomic_t frame_cnt;                  
                        	           
                                       
  spinlock_t             xmit_lock;    
                                       
                                       

  int  pppbind;                        
  int					dialtimeout;	
  int					dialwait;		
  ulong					dialstarted;	
  ulong					dialwait_timer;	
  int					huptimeout;		
#ifdef CONFIG_ISDN_X25
  struct concap_device_ops *dops;      
#endif
  
  ulong cisco_myseq;                   
  ulong cisco_mineseen;                
  ulong cisco_yourseq;                 
  int cisco_keepalive_period;		
  ulong cisco_last_slarp_in;		
  char cisco_line_state;		
  char cisco_debserint;			
  struct timer_list cisco_timer;
  struct work_struct tqueue;
} isdn_net_local;

typedef struct isdn_net_dev_s {
  isdn_net_local *local;
  isdn_net_local *queue;               
  spinlock_t queue_lock;               
  void *next;                          
  struct net_device *dev;              
#ifdef CONFIG_ISDN_PPP
  ippp_bundle * pb;		
#endif
#ifdef CONFIG_ISDN_X25
  struct concap_proto  *cprot; 
#endif

} isdn_net_dev;



#define ISDN_ASYNC_MAGIC          0x49344C01 
#define ISDN_ASYNC_INITIALIZED	  0x80000000 
#define ISDN_ASYNC_CALLOUT_ACTIVE 0x40000000 
#define ISDN_ASYNC_NORMAL_ACTIVE  0x20000000 
#define ISDN_ASYNC_CLOSING	  0x08000000 
#define ISDN_ASYNC_CTS_FLOW	  0x04000000 
#define ISDN_ASYNC_CHECK_CD	  0x02000000 
#define ISDN_ASYNC_HUP_NOTIFY         0x0001 
#define ISDN_ASYNC_SESSION_LOCKOUT    0x0100 
#define ISDN_ASYNC_PGRP_LOCKOUT       0x0200 
#define ISDN_ASYNC_CALLOUT_NOHUP      0x0400 
#define ISDN_ASYNC_SPLIT_TERMIOS      0x0008 
#define ISDN_SERIAL_XMIT_SIZE           1024 
#define ISDN_SERIAL_XMIT_MAX            4000 
#define ISDN_SERIAL_TYPE_NORMAL            1
#define ISDN_SERIAL_TYPE_CALLOUT           2

#ifdef CONFIG_ISDN_AUDIO
typedef struct _isdn_audio_data {
  unsigned short dle_count;
  unsigned char  lock;
} isdn_audio_data_t;

#define ISDN_AUDIO_SKB_DLECOUNT(skb)	(((isdn_audio_data_t *)&skb->cb[0])->dle_count)
#define ISDN_AUDIO_SKB_LOCK(skb)	(((isdn_audio_data_t *)&skb->cb[0])->lock)
#endif

typedef struct atemu {
	u_char       profile[ISDN_MODEM_NUMREG]; 
	u_char       mdmreg[ISDN_MODEM_NUMREG];  
	char         pmsn[ISDN_MSNLEN];          
	char         msn[ISDN_MSNLEN];           
	char         plmsn[ISDN_LMSNLEN];        
	char         lmsn[ISDN_LMSNLEN];         
	char         cpn[ISDN_MSNLEN];           
	char         connmsg[ISDN_CMSGLEN];	 
#ifdef CONFIG_ISDN_AUDIO
	u_char       vpar[10];                   
	int          lastDLE;                    
#endif
	int          mdmcmdl;                    
	int          pluscount;                  
	u_long       lastplus;                   
	int	     carrierwait;                
	char         mdmcmd[255];                
	unsigned int charge;                     
} atemu;

typedef struct modem_info {
  int			magic;
  struct module		*owner;
  int			flags;		 
  int			x_char;		 
  int			mcr;		 
  int                   msr;             
  int                   lsr;             
  int			line;
  int			count;		 
  int			blocked_open;	 
  long			session;	 
  long			pgrp;		 
  int                   online;          
					 
  int                   dialing;         
  int                   rcvsched;        
  int                   isdn_driver;	 
  int                   isdn_channel;    
  int                   drv_index;       
  int                   ncarrier;        
  unsigned char         last_cause[8];   
  unsigned char         last_num[ISDN_MSNLEN];
	                                 
  unsigned char         last_l2;         
  unsigned char         last_si;         
  unsigned char         last_lhup;       
  unsigned char         last_dir;        
  struct timer_list     nc_timer;        
  int                   send_outstanding;
  int                   xmit_size;       
  int                   xmit_count;      
  unsigned char         *xmit_buf;       
  struct sk_buff_head   xmit_queue;      
  atomic_t              xmit_lock;       
#ifdef CONFIG_ISDN_AUDIO
  int                   vonline;         
					 
					 
					 
  struct sk_buff_head   dtmf_queue;      
  void                  *adpcms;         
  void                  *adpcmr;         
  void                  *dtmf_state;     
  void                  *silence_state;  
#endif
#ifdef CONFIG_ISDN_TTY_FAX
  struct T30_s		*fax;		 
  int			faxonline;	 
#endif
  struct tty_struct 	*tty;            
  atemu                 emu;             
  struct ktermios	normal_termios;  
  struct ktermios	callout_termios;
  wait_queue_head_t	open_wait, close_wait;
  spinlock_t	        readlock;
} modem_info;

#define ISDN_MODEM_WINSIZE 8

typedef struct _isdn_modem {
  int                refcount;				
  struct tty_driver  *tty_modem;			
  struct tty_struct  *modem_table[ISDN_MAX_CHANNELS];	
  struct ktermios     *modem_termios[ISDN_MAX_CHANNELS];
  struct ktermios     *modem_termios_locked[ISDN_MAX_CHANNELS];
  modem_info         info[ISDN_MAX_CHANNELS];	   
} isdn_modem_t;


#define V110_BUFSIZE 1024

typedef struct {
	int nbytes;                    
	int nbits;                     
	unsigned char key;             
	int decodelen;                 
	int SyncInit;                  
	unsigned char *OnlineFrame;    
	unsigned char *OfflineFrame;   
	int framelen;                  
	int skbuser;                   
	int skbidle;                   
	int introducer;                
	int dbit;
	unsigned char b;
	int skbres;                    
	int maxsize;                   
	unsigned char *encodebuf;      
	unsigned char decodebuf[V110_BUFSIZE]; 
} isdn_v110_stream;



typedef struct {
	char *next;
	char *private;
} infostruct;

#define DRV_FLAG_RUNNING 1
#define DRV_FLAG_REJBUS  2
#define DRV_FLAG_LOADED  4

typedef struct _isdn_driver {
	ulong               online;           
	ulong               flags;            
	int                 locks;            
	int                 channels;         
	wait_queue_head_t   st_waitq;         
	int                 maxbufsize;       
	unsigned long       pktcount;         
	int                 stavail;          
	isdn_if            *interface;        
	int                *rcverr;           
	int                *rcvcount;         
#ifdef CONFIG_ISDN_AUDIO
	unsigned long      DLEflag;           
#endif
	struct sk_buff_head *rpqueue;         
	wait_queue_head_t  *rcv_waitq;       
	wait_queue_head_t  *snd_waitq;       
	char               msn2eaz[10][ISDN_MSNLEN];  
} isdn_driver_t;

typedef struct isdn_devt {
	struct module     *owner;
	spinlock_t	  lock;
	unsigned short    flags;		      
	int               drivers;		      
	int               channels;		      
	int               net_verbose;                
	int               modempoll;		      
	spinlock_t	  timerlock;
	int               tflags;                     
	
	int               global_flags;
	infostruct        *infochain;                 
	wait_queue_head_t info_waitq;                 
	struct timer_list timer;		      
	int               chanmap[ISDN_MAX_CHANNELS]; 
	int               drvmap[ISDN_MAX_CHANNELS];  
	int               usage[ISDN_MAX_CHANNELS];   
	char              num[ISDN_MAX_CHANNELS][ISDN_MSNLEN];
	
	int               m_idx[ISDN_MAX_CHANNELS];   
	isdn_driver_t     *drv[ISDN_MAX_DRIVERS];     
	isdn_net_dev      *netdev;		      
	char              drvid[ISDN_MAX_DRIVERS][20];
	struct task_struct *profd;                    
	isdn_modem_t      mdm;			      
	isdn_net_dev      *rx_netdev[ISDN_MAX_CHANNELS]; 
	isdn_net_dev      *st_netdev[ISDN_MAX_CHANNELS]; 
	ulong             ibytes[ISDN_MAX_CHANNELS];  
	ulong             obytes[ISDN_MAX_CHANNELS];  
	int               v110emu[ISDN_MAX_CHANNELS]; 
	atomic_t          v110use[ISDN_MAX_CHANNELS]; 
	isdn_v110_stream  *v110[ISDN_MAX_CHANNELS];   
	struct mutex      mtx;                        
	unsigned long     global_features;
} isdn_dev;

extern isdn_dev *dev;


#endif 

#endif 
