
#include <linux/sched.h>
#include <linux/slab.h>
#include "irnet_ppp.h"		

static const struct ppp_channel_ops irnet_ppp_ops = {
	.start_xmit = ppp_irnet_send,
	.ioctl = ppp_irnet_ioctl
};


static inline ssize_t
irnet_ctrl_write(irnet_socket *	ap,
		 const char __user *buf,
		 size_t		count)
{
  char		command[IRNET_MAX_COMMAND];
  char *	start;		
  char *	next;		
  int		length;		

  DENTER(CTRL_TRACE, "(ap=0x%p, count=%Zd)\n", ap, count);

  
  DABORT(count >= IRNET_MAX_COMMAND, -ENOMEM,
	 CTRL_ERROR, "Too much data !!!\n");

  
  if(copy_from_user(command, buf, count))
    {
      DERROR(CTRL_ERROR, "Invalid user space pointer.\n");
      return -EFAULT;
    }

  
  command[count] = '\0';
  DEBUG(CTRL_INFO, "Command line received is ``%s'' (%Zd).\n",
	command, count);

  
  next = command;
  while(next != NULL)
    {
      
      start = next;

	
	start = skip_spaces(start);

      
      next = strchr(start, ',');
      if(next)
	{
	  *next = '\0';			
	  length = next - start;	
	  next++;			
	}
      else
	length = strlen(start);

      DEBUG(CTRL_INFO, "Found command ``%s'' (%d).\n", start, length);


      
      if(!strncmp(start, "name", 4))
	{
	  
	  if((length > 5) && (strcmp(start + 5, "any")))
	    {
	      
	      while(isspace(start[length - 1]))
		length--;

	      DABORT(length < 5 || length > NICKNAME_MAX_LEN + 5,
		     -EINVAL, CTRL_ERROR, "Invalid nickname.\n");

	      
	      memcpy(ap->rname, start + 5, length - 5);
	      ap->rname[length - 5] = '\0';
	    }
	  else
	    ap->rname[0] = '\0';
	  DEBUG(CTRL_INFO, "Got rname = ``%s''\n", ap->rname);

	  
	  continue;
	}

      if((!strncmp(start, "addr", 4)) ||
	 (!strncmp(start, "daddr", 5)) ||
	 (!strncmp(start, "saddr", 5)))
	{
	  __u32		addr = DEV_ADDR_ANY;

	  
	  if((length > 5) && (strcmp(start + 5, "any")))
	    {
	      char *	begp = start + 5;
	      char *	endp;

	      
	      begp = skip_spaces(begp);

	      
	      addr = simple_strtoul(begp, &endp, 16);
	      
	      DABORT(endp <= (start + 5), -EINVAL,
		     CTRL_ERROR, "Invalid address.\n");
	    }
	  
	  if(start[0] == 's')
	    {
	      
	      ap->rsaddr = addr;
	      DEBUG(CTRL_INFO, "Got rsaddr = %08x\n", ap->rsaddr);
	    }
	  else
	    {
	      
	      ap->rdaddr = addr;
	      DEBUG(CTRL_INFO, "Got rdaddr = %08x\n", ap->rdaddr);
	    }

	  
	  continue;
	}

      

      
      DABORT(1, -EINVAL, CTRL_ERROR, "Not a recognised IrNET command.\n");
    }

  
  return count;
}

#ifdef INITIAL_DISCOVERY
static void
irnet_get_discovery_log(irnet_socket *	ap)
{
  __u16		mask = irlmp_service_to_hint(S_LAN);

  
  ap->discoveries = irlmp_get_discoveries(&ap->disco_number, mask,
					  DISCOVERY_DEFAULT_SLOTS);

  
  if(ap->discoveries == NULL)
    ap->disco_number = -1;

  DEBUG(CTRL_INFO, "Got the log (0x%p), size is %d\n",
	ap->discoveries, ap->disco_number);
}

static inline int
irnet_read_discovery_log(irnet_socket *	ap,
			 char *		event)
{
  int		done_event = 0;

  DENTER(CTRL_TRACE, "(ap=0x%p, event=0x%p)\n",
	 ap, event);

  
  if(ap->disco_number == -1)
    {
      DEBUG(CTRL_INFO, "Already done\n");
      return 0;
    }

  
  if(ap->discoveries == NULL)
    irnet_get_discovery_log(ap);

  
  if(ap->disco_index < ap->disco_number)
    {
      
      sprintf(event, "Found %08x (%s) behind %08x {hints %02X-%02X}\n",
	      ap->discoveries[ap->disco_index].daddr,
	      ap->discoveries[ap->disco_index].info,
	      ap->discoveries[ap->disco_index].saddr,
	      ap->discoveries[ap->disco_index].hints[0],
	      ap->discoveries[ap->disco_index].hints[1]);
      DEBUG(CTRL_INFO, "Writing discovery %d : %s\n",
	    ap->disco_index, ap->discoveries[ap->disco_index].info);

      
      done_event = 1;
      
      ap->disco_index++;
    }

  
  if(ap->disco_index >= ap->disco_number)
    {
      
      DEBUG(CTRL_INFO, "Cleaning up log (0x%p)\n",
	    ap->discoveries);
      if(ap->discoveries != NULL)
	{
	  
	  kfree(ap->discoveries);
	  ap->discoveries = NULL;
	}
      ap->disco_number = -1;
    }

  return done_event;
}
#endif 

static inline ssize_t
irnet_ctrl_read(irnet_socket *	ap,
		struct file *	file,
		char __user *	buf,
		size_t		count)
{
  DECLARE_WAITQUEUE(wait, current);
  char		event[64];	
  ssize_t	ret = 0;

  DENTER(CTRL_TRACE, "(ap=0x%p, count=%Zd)\n", ap, count);

  
  DABORT(count < sizeof(event), -EOVERFLOW, CTRL_ERROR, "Buffer to small.\n");

#ifdef INITIAL_DISCOVERY
  
  if(irnet_read_discovery_log(ap, event))
    {
      
      if(copy_to_user(buf, event, strlen(event)))
	{
	  DERROR(CTRL_ERROR, "Invalid user space pointer.\n");
	  return -EFAULT;
	}

      DEXIT(CTRL_TRACE, "\n");
      return strlen(event);
    }
#endif 

  
  add_wait_queue(&irnet_events.rwait, &wait);
  current->state = TASK_INTERRUPTIBLE;
  for(;;)
    {
      
      ret = 0;
      if(ap->event_index != irnet_events.index)
	break;
      ret = -EAGAIN;
      if(file->f_flags & O_NONBLOCK)
	break;
      ret = -ERESTARTSYS;
      if(signal_pending(current))
	break;
      
      schedule();
    }
  current->state = TASK_RUNNING;
  remove_wait_queue(&irnet_events.rwait, &wait);

  
  if(ret != 0)
    {
      
      DEXIT(CTRL_TRACE, " - ret %Zd\n", ret);
      return ret;
    }

  
  switch(irnet_events.log[ap->event_index].event)
    {
    case IRNET_DISCOVER:
      sprintf(event, "Discovered %08x (%s) behind %08x {hints %02X-%02X}\n",
	      irnet_events.log[ap->event_index].daddr,
	      irnet_events.log[ap->event_index].name,
	      irnet_events.log[ap->event_index].saddr,
	      irnet_events.log[ap->event_index].hints.byte[0],
	      irnet_events.log[ap->event_index].hints.byte[1]);
      break;
    case IRNET_EXPIRE:
      sprintf(event, "Expired %08x (%s) behind %08x {hints %02X-%02X}\n",
	      irnet_events.log[ap->event_index].daddr,
	      irnet_events.log[ap->event_index].name,
	      irnet_events.log[ap->event_index].saddr,
	      irnet_events.log[ap->event_index].hints.byte[0],
	      irnet_events.log[ap->event_index].hints.byte[1]);
      break;
    case IRNET_CONNECT_TO:
      sprintf(event, "Connected to %08x (%s) on ppp%d\n",
	      irnet_events.log[ap->event_index].daddr,
	      irnet_events.log[ap->event_index].name,
	      irnet_events.log[ap->event_index].unit);
      break;
    case IRNET_CONNECT_FROM:
      sprintf(event, "Connection from %08x (%s) on ppp%d\n",
	      irnet_events.log[ap->event_index].daddr,
	      irnet_events.log[ap->event_index].name,
	      irnet_events.log[ap->event_index].unit);
      break;
    case IRNET_REQUEST_FROM:
      sprintf(event, "Request from %08x (%s) behind %08x\n",
	      irnet_events.log[ap->event_index].daddr,
	      irnet_events.log[ap->event_index].name,
	      irnet_events.log[ap->event_index].saddr);
      break;
    case IRNET_NOANSWER_FROM:
      sprintf(event, "No-answer from %08x (%s) on ppp%d\n",
	      irnet_events.log[ap->event_index].daddr,
	      irnet_events.log[ap->event_index].name,
	      irnet_events.log[ap->event_index].unit);
      break;
    case IRNET_BLOCKED_LINK:
      sprintf(event, "Blocked link with %08x (%s) on ppp%d\n",
	      irnet_events.log[ap->event_index].daddr,
	      irnet_events.log[ap->event_index].name,
	      irnet_events.log[ap->event_index].unit);
      break;
    case IRNET_DISCONNECT_FROM:
      sprintf(event, "Disconnection from %08x (%s) on ppp%d\n",
	      irnet_events.log[ap->event_index].daddr,
	      irnet_events.log[ap->event_index].name,
	      irnet_events.log[ap->event_index].unit);
      break;
    case IRNET_DISCONNECT_TO:
      sprintf(event, "Disconnected to %08x (%s)\n",
	      irnet_events.log[ap->event_index].daddr,
	      irnet_events.log[ap->event_index].name);
      break;
    default:
      sprintf(event, "Bug\n");
    }
  
  ap->event_index = (ap->event_index + 1) % IRNET_MAX_EVENTS;

  DEBUG(CTRL_INFO, "Event is :%s", event);

  
  if(copy_to_user(buf, event, strlen(event)))
    {
      DERROR(CTRL_ERROR, "Invalid user space pointer.\n");
      return -EFAULT;
    }

  DEXIT(CTRL_TRACE, "\n");
  return strlen(event);
}

static inline unsigned int
irnet_ctrl_poll(irnet_socket *	ap,
		struct file *	file,
		poll_table *	wait)
{
  unsigned int mask;

  DENTER(CTRL_TRACE, "(ap=0x%p)\n", ap);

  poll_wait(file, &irnet_events.rwait, wait);
  mask = POLLOUT | POLLWRNORM;
  
  if(ap->event_index != irnet_events.index)
    mask |= POLLIN | POLLRDNORM;
#ifdef INITIAL_DISCOVERY
  if(ap->disco_number != -1)
    {
      
      if(ap->discoveries == NULL)
	irnet_get_discovery_log(ap);
      
      if(ap->disco_number != -1)
	mask |= POLLIN | POLLRDNORM;
    }
#endif 

  DEXIT(CTRL_TRACE, " - mask=0x%X\n", mask);
  return mask;
}



static int
dev_irnet_open(struct inode *	inode,
	       struct file *	file)
{
  struct irnet_socket *	ap;
  int			err;

  DENTER(FS_TRACE, "(file=0x%p)\n", file);

#ifdef SECURE_DEVIRNET
  
  if(!capable(CAP_NET_ADMIN))
    return -EPERM;
#endif 

  
  ap = kzalloc(sizeof(*ap), GFP_KERNEL);
  DABORT(ap == NULL, -ENOMEM, FS_ERROR, "Can't allocate struct irnet...\n");

  
  ap->file = file;

  
  ap->ppp_open = 0;
  ap->chan.private = ap;
  ap->chan.ops = &irnet_ppp_ops;
  ap->chan.mtu = (2048 - TTP_MAX_HEADER - 2 - PPP_HDRLEN);
  ap->chan.hdrlen = 2 + TTP_MAX_HEADER;		
  
  ap->mru = (2048 - TTP_MAX_HEADER - 2 - PPP_HDRLEN);
  ap->xaccm[0] = ~0U;
  ap->xaccm[3] = 0x60000000U;
  ap->raccm = ~0U;

  
  err = irda_irnet_create(ap);
  if(err)
    {
      DERROR(FS_ERROR, "Can't setup IrDA link...\n");
      kfree(ap);

      return err;
    }

  
  ap->event_index = irnet_events.index;	

  mutex_init(&ap->lock);

  
  file->private_data = ap;

  DEXIT(FS_TRACE, " - ap=0x%p\n", ap);

  return 0;
}


static int
dev_irnet_close(struct inode *	inode,
		struct file *	file)
{
  irnet_socket *	ap = file->private_data;

  DENTER(FS_TRACE, "(file=0x%p, ap=0x%p)\n",
	 file, ap);
  DABORT(ap == NULL, 0, FS_ERROR, "ap is NULL !!!\n");

  
  file->private_data = NULL;

  
  irda_irnet_destroy(ap);

  
  if(ap->ppp_open)
    {
      DERROR(FS_ERROR, "Channel still registered - deregistering !\n");
      ap->ppp_open = 0;
      ppp_unregister_channel(&ap->chan);
    }

  kfree(ap);

  DEXIT(FS_TRACE, "\n");
  return 0;
}

static ssize_t
dev_irnet_write(struct file *	file,
		const char __user *buf,
		size_t		count,
		loff_t *	ppos)
{
  irnet_socket *	ap = file->private_data;

  DPASS(FS_TRACE, "(file=0x%p, ap=0x%p, count=%Zd)\n",
	file, ap, count);
  DABORT(ap == NULL, -ENXIO, FS_ERROR, "ap is NULL !!!\n");

  
  if(ap->ppp_open)
    return -EAGAIN;
  else
    return irnet_ctrl_write(ap, buf, count);
}

static ssize_t
dev_irnet_read(struct file *	file,
	       char __user *	buf,
	       size_t		count,
	       loff_t *		ppos)
{
  irnet_socket *	ap = file->private_data;

  DPASS(FS_TRACE, "(file=0x%p, ap=0x%p, count=%Zd)\n",
	file, ap, count);
  DABORT(ap == NULL, -ENXIO, FS_ERROR, "ap is NULL !!!\n");

  
  if(ap->ppp_open)
    return -EAGAIN;
  else
    return irnet_ctrl_read(ap, file, buf, count);
}

static unsigned int
dev_irnet_poll(struct file *	file,
	       poll_table *	wait)
{
  irnet_socket *	ap = file->private_data;
  unsigned int		mask;

  DENTER(FS_TRACE, "(file=0x%p, ap=0x%p)\n",
	 file, ap);

  mask = POLLOUT | POLLWRNORM;
  DABORT(ap == NULL, mask, FS_ERROR, "ap is NULL !!!\n");

  
  if(!ap->ppp_open)
    mask |= irnet_ctrl_poll(ap, file, wait);

  DEXIT(FS_TRACE, " - mask=0x%X\n", mask);
  return mask;
}

static long
dev_irnet_ioctl(
		struct file *	file,
		unsigned int	cmd,
		unsigned long	arg)
{
  irnet_socket *	ap = file->private_data;
  int			err;
  int			val;
  void __user *argp = (void __user *)arg;

  DENTER(FS_TRACE, "(file=0x%p, ap=0x%p, cmd=0x%X)\n",
	 file, ap, cmd);

  
  DASSERT(ap != NULL, -ENXIO, PPP_ERROR, "ap is NULL...\n");
#ifdef SECURE_DEVIRNET
  if(!capable(CAP_NET_ADMIN))
    return -EPERM;
#endif 

  err = -EFAULT;
  switch(cmd)
    {
      
    case TIOCSETD:
      if(get_user(val, (int __user *)argp))
	break;
      if((val == N_SYNC_PPP) || (val == N_PPP))
	{
	  DEBUG(FS_INFO, "Entering PPP discipline.\n");
	  
	  if (mutex_lock_interruptible(&ap->lock))
		  return -EINTR;

	  err = ppp_register_channel(&ap->chan);
	  if(err == 0)
	    {
	      
	      ap->ppp_open = 1;

	      DEBUG(FS_INFO, "Trying to establish a connection.\n");
	      
	      irda_irnet_connect(ap);
	    }
	  else
	    DERROR(FS_ERROR, "Can't setup PPP channel...\n");

          mutex_unlock(&ap->lock);
	}
      else
	{
	  
	  DEBUG(FS_INFO, "Exiting PPP discipline.\n");
	  
	  if (mutex_lock_interruptible(&ap->lock))
		  return -EINTR;

	  if(ap->ppp_open)
	    {
	      ap->ppp_open = 0;
	      ppp_unregister_channel(&ap->chan);
	    }
	  else
	    DERROR(FS_ERROR, "Channel not registered !\n");
	  err = 0;

	  mutex_unlock(&ap->lock);
	}
      break;

      
    case PPPIOCGCHAN:
      if (mutex_lock_interruptible(&ap->lock))
	      return -EINTR;

      if(ap->ppp_open && !put_user(ppp_channel_index(&ap->chan),
						(int __user *)argp))
	err = 0;

      mutex_unlock(&ap->lock);
      break;
    case PPPIOCGUNIT:
      if (mutex_lock_interruptible(&ap->lock))
	      return -EINTR;

      if(ap->ppp_open && !put_user(ppp_unit_number(&ap->chan),
						(int __user *)argp))
        err = 0;

      mutex_unlock(&ap->lock);
      break;

    case PPPIOCGFLAGS:
    case PPPIOCSFLAGS:
    case PPPIOCGASYNCMAP:
    case PPPIOCSASYNCMAP:
    case PPPIOCGRASYNCMAP:
    case PPPIOCSRASYNCMAP:
    case PPPIOCGXASYNCMAP:
    case PPPIOCSXASYNCMAP:
    case PPPIOCGMRU:
    case PPPIOCSMRU:
      DEBUG(FS_INFO, "Standard PPP ioctl.\n");
      if(!capable(CAP_NET_ADMIN))
	err = -EPERM;
      else {
	if (mutex_lock_interruptible(&ap->lock))
	      return -EINTR;

	err = ppp_irnet_ioctl(&ap->chan, cmd, arg);

	mutex_unlock(&ap->lock);
      }
      break;

      
      
    case TCGETS:
      DEBUG(FS_INFO, "Get termios.\n");
      if (mutex_lock_interruptible(&ap->lock))
	      return -EINTR;

#ifndef TCGETS2
      if(!kernel_termios_to_user_termios((struct termios __user *)argp, &ap->termios))
	err = 0;
#else
      if(kernel_termios_to_user_termios_1((struct termios __user *)argp, &ap->termios))
	err = 0;
#endif

      mutex_unlock(&ap->lock);
      break;
      
    case TCSETSF:
      DEBUG(FS_INFO, "Set termios.\n");
      if (mutex_lock_interruptible(&ap->lock))
	      return -EINTR;

#ifndef TCGETS2
      if(!user_termios_to_kernel_termios(&ap->termios, (struct termios __user *)argp))
	err = 0;
#else
      if(!user_termios_to_kernel_termios_1(&ap->termios, (struct termios __user *)argp))
	err = 0;
#endif

      mutex_unlock(&ap->lock);
      break;

      
    case TIOCMBIS:
    case TIOCMBIC:
      
    case TIOCEXCL:
    case TIOCNXCL:
      DEBUG(FS_INFO, "TTY compatibility.\n");
      err = 0;
      break;

    case TCGETA:
      DEBUG(FS_INFO, "TCGETA\n");
      break;

    case TCFLSH:
      DEBUG(FS_INFO, "TCFLSH\n");
#ifdef FLUSH_TO_PPP
      if (mutex_lock_interruptible(&ap->lock))
	      return -EINTR;
      ppp_output_wakeup(&ap->chan);
      mutex_unlock(&ap->lock);
#endif 
      err = 0;
      break;

    case FIONREAD:
      DEBUG(FS_INFO, "FIONREAD\n");
      val = 0;
      if(put_user(val, (int __user *)argp))
	break;
      err = 0;
      break;

    default:
      DERROR(FS_ERROR, "Unsupported ioctl (0x%X)\n", cmd);
      err = -ENOTTY;
    }

  DEXIT(FS_TRACE, " - err = 0x%X\n", err);
  return err;
}


static inline struct sk_buff *
irnet_prepare_skb(irnet_socket *	ap,
		  struct sk_buff *	skb)
{
  unsigned char *	data;
  int			proto;		
  int			islcp;		
  int			needaddr;	

  DENTER(PPP_TRACE, "(ap=0x%p, skb=0x%p)\n",
	 ap, skb);

  
  data  = skb->data;
  proto = (data[0] << 8) + data[1];

  islcp = (proto == PPP_LCP) && (1 <= data[2]) && (data[2] <= 7);

  
  if((data[0] == 0) && (ap->flags & SC_COMP_PROT) && (!islcp))
    skb_pull(skb,1);

  
  needaddr = 2*((ap->flags & SC_COMP_AC) == 0 || islcp);

  
  if((skb_headroom(skb) < (ap->max_header_size + needaddr)) ||
      (skb_shared(skb)))
    {
      struct sk_buff *	new_skb;

      DEBUG(PPP_INFO, "Reallocating skb\n");

      
      new_skb = skb_realloc_headroom(skb, ap->max_header_size + needaddr);

      
      dev_kfree_skb(skb);

      
      DABORT(new_skb == NULL, NULL, PPP_ERROR, "Could not realloc skb\n");

      
      skb = new_skb;
    }

  
  if(needaddr)
    {
      skb_push(skb, 2);
      skb->data[0] = PPP_ALLSTATIONS;
      skb->data[1] = PPP_UI;
    }

  DEXIT(PPP_TRACE, "\n");

  return skb;
}

static int
ppp_irnet_send(struct ppp_channel *	chan,
	       struct sk_buff *		skb)
{
  irnet_socket *	self = (struct irnet_socket *) chan->private;
  int			ret;

  DENTER(PPP_TRACE, "(channel=0x%p, ap/self=0x%p)\n",
	 chan, self);

  
  DASSERT(self != NULL, 0, PPP_ERROR, "Self is NULL !!!\n");

  
  if(!(test_bit(0, &self->ttp_open)))
    {
#ifdef CONNECT_IN_SEND
      
      
      irda_irnet_connect(self);
#endif 

      DEBUG(PPP_INFO, "IrTTP not ready ! (%ld-%ld)\n",
	    self->ttp_open, self->ttp_connect);

#ifdef BLOCK_WHEN_CONNECT
      
      if(test_bit(0, &self->ttp_connect))
	{
	  
	  return 0;
	}
#endif 

      
      dev_kfree_skb(skb);
      return 1;
    }

  
  if(self->tx_flow != FLOW_START)
    DRETURN(0, PPP_INFO, "IrTTP queue full (%d skbs)...\n",
	    skb_queue_len(&self->tsap->tx_queue));

  
  skb = irnet_prepare_skb(self, skb);
  DABORT(skb == NULL, 1, PPP_ERROR, "Prepare skb for Tx failed.\n");

  
  ret = irttp_data_request(self->tsap, skb);
  if(ret < 0)
    {
      DERROR(PPP_ERROR, "IrTTP doesn't like this packet !!! (0x%X)\n", ret);
      
    }

  DEXIT(PPP_TRACE, "\n");
  return 1;	
}

static int
ppp_irnet_ioctl(struct ppp_channel *	chan,
		unsigned int		cmd,
		unsigned long		arg)
{
  irnet_socket *	ap = (struct irnet_socket *) chan->private;
  int			err;
  int			val;
  u32			accm[8];
  void __user *argp = (void __user *)arg;

  DENTER(PPP_TRACE, "(channel=0x%p, ap=0x%p, cmd=0x%X)\n",
	 chan, ap, cmd);

  
  DASSERT(ap != NULL, -ENXIO, PPP_ERROR, "ap is NULL...\n");

  err = -EFAULT;
  switch(cmd)
    {
      
    case PPPIOCGFLAGS:
      val = ap->flags | ap->rbits;
      if(put_user(val, (int __user *) argp))
	break;
      err = 0;
      break;
    case PPPIOCSFLAGS:
      if(get_user(val, (int __user *) argp))
	break;
      ap->flags = val & ~SC_RCV_BITS;
      ap->rbits = val & SC_RCV_BITS;
      err = 0;
      break;

      
    case PPPIOCGASYNCMAP:
      if(put_user(ap->xaccm[0], (u32 __user *) argp))
	break;
      err = 0;
      break;
    case PPPIOCSASYNCMAP:
      if(get_user(ap->xaccm[0], (u32 __user *) argp))
	break;
      err = 0;
      break;
    case PPPIOCGRASYNCMAP:
      if(put_user(ap->raccm, (u32 __user *) argp))
	break;
      err = 0;
      break;
    case PPPIOCSRASYNCMAP:
      if(get_user(ap->raccm, (u32 __user *) argp))
	break;
      err = 0;
      break;
    case PPPIOCGXASYNCMAP:
      if(copy_to_user(argp, ap->xaccm, sizeof(ap->xaccm)))
	break;
      err = 0;
      break;
    case PPPIOCSXASYNCMAP:
      if(copy_from_user(accm, argp, sizeof(accm)))
	break;
      accm[2] &= ~0x40000000U;		
      accm[3] |= 0x60000000U;		
      memcpy(ap->xaccm, accm, sizeof(ap->xaccm));
      err = 0;
      break;

      
    case PPPIOCGMRU:
      if(put_user(ap->mru, (int __user *) argp))
	break;
      err = 0;
      break;
    case PPPIOCSMRU:
      if(get_user(val, (int __user *) argp))
	break;
      if(val < PPP_MRU)
	val = PPP_MRU;
      ap->mru = val;
      err = 0;
      break;

    default:
      DEBUG(PPP_INFO, "Unsupported ioctl (0x%X)\n", cmd);
      err = -ENOIOCTLCMD;
    }

  DEXIT(PPP_TRACE, " - err = 0x%X\n", err);
  return err;
}


static inline int __init
ppp_irnet_init(void)
{
  int err = 0;

  DENTER(MODULE_TRACE, "()\n");

  
  err = misc_register(&irnet_misc_device);

  DEXIT(MODULE_TRACE, "\n");
  return err;
}

static inline void __exit
ppp_irnet_cleanup(void)
{
  DENTER(MODULE_TRACE, "()\n");

  
  misc_deregister(&irnet_misc_device);

  DEXIT(MODULE_TRACE, "\n");
}

static int __init
irnet_init(void)
{
  int err;

  
  err = irda_irnet_init();
  if(!err)
    err = ppp_irnet_init();
  return err;
}

static void __exit
irnet_cleanup(void)
{
  irda_irnet_cleanup();
  ppp_irnet_cleanup();
}

module_init(irnet_init);
module_exit(irnet_cleanup);
MODULE_AUTHOR("Jean Tourrilhes <jt@hpl.hp.com>");
MODULE_DESCRIPTION("IrNET : Synchronous PPP over IrDA");
MODULE_LICENSE("GPL");
MODULE_ALIAS_CHARDEV(10, 187);
