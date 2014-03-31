
#ifndef IRNET_PPP_H
#define IRNET_PPP_H


#include "irnet.h"		


#define IRNET_MAJOR	10	
#define IRNET_MINOR	187	

#define IRNET_MAX_COMMAND	256	


#define SC_RCV_BITS	(SC_RCV_B7_1|SC_RCV_B7_0|SC_RCV_ODDP|SC_RCV_EVNP)

#define XMIT_BUSY	0
#define RECV_BUSY	1
#define XMIT_WAKEUP	2
#define XMIT_FULL	3

#define PPPSYNC_MAX_RQLEN	32	




static inline ssize_t
	irnet_ctrl_write(irnet_socket *,
			 const char *,
			 size_t);
static inline ssize_t
	irnet_ctrl_read(irnet_socket *,
			struct file *,
			char *,
			size_t);
static inline unsigned int
	irnet_ctrl_poll(irnet_socket *,
			struct file *,
			poll_table *);
static int
	dev_irnet_open(struct inode *,	
		       struct file *),
	dev_irnet_close(struct inode *,
			struct file *);
static ssize_t
	dev_irnet_write(struct file *,
			const char __user *,
			size_t,
			loff_t *),
	dev_irnet_read(struct file *,
		       char __user *,
		       size_t,
		       loff_t *);
static unsigned int
	dev_irnet_poll(struct file *,
		       poll_table *);
static long
	dev_irnet_ioctl(struct file *,
			unsigned int,
			unsigned long);
static inline struct sk_buff *
	irnet_prepare_skb(irnet_socket *,
			  struct sk_buff *);
static int
	ppp_irnet_send(struct ppp_channel *,
		      struct sk_buff *);
static int
	ppp_irnet_ioctl(struct ppp_channel *,
			unsigned int,
			unsigned long);


static const struct file_operations irnet_device_fops =
{
	.owner		= THIS_MODULE,
	.read		= dev_irnet_read,
	.write		= dev_irnet_write,
	.poll		= dev_irnet_poll,
	.unlocked_ioctl	= dev_irnet_ioctl,
	.open		= dev_irnet_open,
	.release	= dev_irnet_close,
	.llseek		= noop_llseek,
  
};

static struct miscdevice irnet_misc_device =
{
	IRNET_MINOR,
	"irnet",
	&irnet_device_fops
};

#endif 
