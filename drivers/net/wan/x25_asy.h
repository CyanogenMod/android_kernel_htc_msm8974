#ifndef _LINUX_X25_ASY_H
#define _LINUX_X25_ASY_H

#define SL_NRUNIT	256		
#define SL_MTU		256	

#define X25_END         0x7E		
#define X25_ESC         0x7D		
#define X25_ESCAPE(x)	((x)^0x20)
#define X25_UNESCAPE(x)	((x)^0x20)


struct x25_asy {
  int			magic;

  
  spinlock_t		lock;
  struct tty_struct	*tty;		
  struct net_device	*dev;		

  
  unsigned char		*rbuff;		
  int                   rcount;         
  unsigned char		*xbuff;		
  unsigned char         *xhead;         
  int                   xleft;          
  int                   buffsize;       

  unsigned long		flags;		
#define SLF_INUSE	0		
#define SLF_ESCAPE	1               
#define SLF_ERROR	2               
#define SLF_OUTWAIT	4		
};



#define X25_ASY_MAGIC 0x5303

extern int x25_asy_init(struct net_device *dev);

#endif	
