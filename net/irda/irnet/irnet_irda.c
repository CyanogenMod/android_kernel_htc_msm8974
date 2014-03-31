
#include "irnet_irda.h"		
#include <linux/sched.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <asm/unaligned.h>

static void irnet_ppp_disconnect(struct work_struct *work)
{
	irnet_socket * self =
		container_of(work, irnet_socket, disconnect_work);

	if (self == NULL)
		return;
	if (self->ppp_open && !self->ttp_open && !self->ttp_connect) {
		ppp_unregister_channel(&self->chan);
		self->ppp_open = 0;
	}
}


static void
irnet_post_event(irnet_socket *	ap,
		 irnet_event	event,
		 __u32		saddr,
		 __u32		daddr,
		 char *		name,
		 __u16		hints)
{
  int			index;		

  DENTER(CTRL_TRACE, "(ap=0x%p, event=%d, daddr=%08x, name=``%s'')\n",
	 ap, event, daddr, name);

  spin_lock_bh(&irnet_events.spinlock);

  
  index = irnet_events.index;
  irnet_events.log[index].event = event;
  irnet_events.log[index].daddr = daddr;
  irnet_events.log[index].saddr = saddr;
  
  if(name)
    strcpy(irnet_events.log[index].name, name);
  else
    irnet_events.log[index].name[0] = '\0';
  
  irnet_events.log[index].hints.word = hints;
  
  if((ap != (irnet_socket *) NULL) && (ap->ppp_open))
    irnet_events.log[index].unit = ppp_unit_number(&ap->chan);
  else
    irnet_events.log[index].unit = -1;

  /* Increment the index
   * Note that we increment the index only after the event is written,
   * to make sure that the readers don't get garbage... */
  irnet_events.index = (index + 1) % IRNET_MAX_EVENTS;

  DEBUG(CTRL_INFO, "New event index is %d\n", irnet_events.index);

  
  spin_unlock_bh(&irnet_events.spinlock);

  
  wake_up_interruptible_all(&irnet_events.rwait);

  DEXIT(CTRL_TRACE, "\n");
}


static inline int
irnet_open_tsap(irnet_socket *	self)
{
  notify_t	notify;		

  DENTER(IRDA_SR_TRACE, "(self=0x%p)\n", self);

  DABORT(self->tsap != NULL, -EBUSY, IRDA_SR_ERROR, "Already busy !\n");

  
  irda_notify_init(&notify);
  notify.connect_confirm	= irnet_connect_confirm;
  notify.connect_indication	= irnet_connect_indication;
  notify.disconnect_indication	= irnet_disconnect_indication;
  notify.data_indication	= irnet_data_indication;
  
  notify.flow_indication	= irnet_flow_indication;
  notify.status_indication	= irnet_status_indication;
  notify.instance		= self;
  strlcpy(notify.name, IRNET_NOTIFY_NAME, sizeof(notify.name));

  
  self->tsap = irttp_open_tsap(LSAP_ANY, DEFAULT_INITIAL_CREDIT,
			       &notify);
  DABORT(self->tsap == NULL, -ENOMEM,
	 IRDA_SR_ERROR, "Unable to allocate TSAP !\n");

  
  self->stsap_sel = self->tsap->stsap_sel;

  DEXIT(IRDA_SR_TRACE, " - tsap=0x%p, sel=0x%X\n",
	self->tsap, self->stsap_sel);
  return 0;
}

static inline __u8
irnet_ias_to_tsap(irnet_socket *	self,
		  int			result,
		  struct ias_value *	value)
{
  __u8	dtsap_sel = 0;		

  DENTER(IRDA_SR_TRACE, "(self=0x%p)\n", self);

  
  self->errno = 0;

  
  switch(result)
    {
      
    case IAS_CLASS_UNKNOWN:
    case IAS_ATTRIB_UNKNOWN:
      DEBUG(IRDA_SR_INFO, "IAS object doesn't exist ! (%d)\n", result);
      self->errno = -EADDRNOTAVAIL;
      break;

      
    default :
      DEBUG(IRDA_SR_INFO, "IAS query failed ! (%d)\n", result);
      self->errno = -EHOSTUNREACH;
      break;

      
    case IAS_SUCCESS:
      break;
    }

  
  if(value != NULL)
    {
      
      switch(value->type)
	{
	case IAS_INTEGER:
	  DEBUG(IRDA_SR_INFO, "result=%d\n", value->t.integer);
	  if(value->t.integer != -1)
	    
	    dtsap_sel = value->t.integer;
	  else
	    self->errno = -EADDRNOTAVAIL;
	  break;
	default:
	  self->errno = -EADDRNOTAVAIL;
	  DERROR(IRDA_SR_ERROR, "bad type ! (0x%X)\n", value->type);
	  break;
	}

      
      irias_delete_value(value);
    }
  else	
    {
      
      if(!(self->errno))
	{
	  DERROR(IRDA_SR_ERROR,
		 "IrDA bug : result == SUCCESS && value == NULL\n");
	  self->errno = -EHOSTUNREACH;
	}
    }
  DEXIT(IRDA_SR_TRACE, "\n");

  
  return dtsap_sel;
}

static inline int
irnet_find_lsap_sel(irnet_socket *	self)
{
  DENTER(IRDA_SR_TRACE, "(self=0x%p)\n", self);

  
  DABORT(self->iriap, -EBUSY, IRDA_SR_ERROR, "busy with a previous query.\n");

  
  self->iriap = iriap_open(LSAP_ANY, IAS_CLIENT, self,
			   irnet_getvalue_confirm);

  
  self->errno = -EHOSTUNREACH;

  
  iriap_getvaluebyclass_request(self->iriap, self->rsaddr, self->daddr,
				IRNET_SERVICE_NAME, IRNET_IAS_VALUE);


  DEXIT(IRDA_SR_TRACE, "\n");
  return 0;
}

static inline int
irnet_connect_tsap(irnet_socket *	self)
{
  int		err;

  DENTER(IRDA_SR_TRACE, "(self=0x%p)\n", self);

  
  err = irnet_open_tsap(self);
  if(err != 0)
    {
      clear_bit(0, &self->ttp_connect);
      DERROR(IRDA_SR_ERROR, "connect aborted!\n");
      return err;
    }

  
  err = irttp_connect_request(self->tsap, self->dtsap_sel,
			      self->rsaddr, self->daddr, NULL,
			      self->max_sdu_size_rx, NULL);
  if(err != 0)
    {
      clear_bit(0, &self->ttp_connect);
      DERROR(IRDA_SR_ERROR, "connect aborted!\n");
      return err;
    }


  DEXIT(IRDA_SR_TRACE, "\n");
  return err;
}

static inline int
irnet_discover_next_daddr(irnet_socket *	self)
{
  if(self->iriap)
    {
      iriap_close(self->iriap);
      self->iriap = NULL;
    }
  
  self->iriap = iriap_open(LSAP_ANY, IAS_CLIENT, self,
			   irnet_discovervalue_confirm);
  if(self->iriap == NULL)
    return -ENOMEM;

  
  self->disco_index++;

  
  if(self->disco_index < self->disco_number)
    {
      
      iriap_getvaluebyclass_request(self->iriap,
				    self->discoveries[self->disco_index].saddr,
				    self->discoveries[self->disco_index].daddr,
				    IRNET_SERVICE_NAME, IRNET_IAS_VALUE);
      return 0;
    }
  else
    return 1;
}

static inline int
irnet_discover_daddr_and_lsap_sel(irnet_socket *	self)
{
  int	ret;

  DENTER(IRDA_SR_TRACE, "(self=0x%p)\n", self);

  
  self->discoveries = irlmp_get_discoveries(&self->disco_number, self->mask,
					    DISCOVERY_DEFAULT_SLOTS);

  
  if(self->discoveries == NULL)
    {
      self->disco_number = -1;
      clear_bit(0, &self->ttp_connect);
      DRETURN(-ENETUNREACH, IRDA_SR_INFO, "No Cachelog...\n");
    }
  DEBUG(IRDA_SR_INFO, "Got the log (0x%p), size is %d\n",
	self->discoveries, self->disco_number);

  
  self->disco_index = -1;
  self->daddr = DEV_ADDR_ANY;

  
  ret = irnet_discover_next_daddr(self);
  if(ret)
    {
      
      if(self->iriap)
	iriap_close(self->iriap);
      self->iriap = NULL;

      
      kfree(self->discoveries);
      self->discoveries = NULL;

      clear_bit(0, &self->ttp_connect);
      DRETURN(-ENETUNREACH, IRDA_SR_INFO, "Cachelog empty...\n");
    }

  

  DEXIT(IRDA_SR_TRACE, "\n");
  return 0;
}

static inline int
irnet_dname_to_daddr(irnet_socket *	self)
{
  struct irda_device_info *discoveries;	
  int	number;			
  int	i;

  DENTER(IRDA_SR_TRACE, "(self=0x%p)\n", self);

  
  discoveries = irlmp_get_discoveries(&number, 0xffff,
				      DISCOVERY_DEFAULT_SLOTS);
  
  if(discoveries == NULL)
    DRETURN(-ENETUNREACH, IRDA_SR_INFO, "Cachelog empty...\n");

  for(i = 0; i < number; i++)
    {
      
      if(!strncmp(discoveries[i].info, self->rname, NICKNAME_MAX_LEN))
	{
	  
	  self->daddr = discoveries[i].daddr;
	  DEBUG(IRDA_SR_INFO, "discovered device ``%s'' at address 0x%08x.\n",
		self->rname, self->daddr);
	  kfree(discoveries);
	  DEXIT(IRDA_SR_TRACE, "\n");
	  return 0;
	}
    }
  
  DEBUG(IRDA_SR_INFO, "cannot discover device ``%s'' !!!\n", self->rname);
  kfree(discoveries);
  return -EADDRNOTAVAIL;
}



int
irda_irnet_create(irnet_socket *	self)
{
  DENTER(IRDA_SOCK_TRACE, "(self=0x%p)\n", self);

  self->magic = IRNET_MAGIC;	

  self->ttp_open = 0;		
  self->ttp_connect = 0;	
  self->rname[0] = '\0';	
  self->rdaddr = DEV_ADDR_ANY;	
  self->rsaddr = DEV_ADDR_ANY;	
  self->daddr = DEV_ADDR_ANY;	
  self->saddr = DEV_ADDR_ANY;	
  self->max_sdu_size_rx = TTP_SAR_UNBOUND;

  
  self->ckey = irlmp_register_client(0, NULL, NULL, NULL);
#ifdef DISCOVERY_NOMASK
  self->mask = 0xffff;		
#else 
  self->mask = irlmp_service_to_hint(S_LAN);
#endif 
  self->tx_flow = FLOW_START;	

  INIT_WORK(&self->disconnect_work, irnet_ppp_disconnect);

  DEXIT(IRDA_SOCK_TRACE, "\n");
  return 0;
}

int
irda_irnet_connect(irnet_socket *	self)
{
  int		err;

  DENTER(IRDA_SOCK_TRACE, "(self=0x%p)\n", self);

  if(test_and_set_bit(0, &self->ttp_connect))
    DRETURN(-EBUSY, IRDA_SOCK_INFO, "Already connecting...\n");
  if((self->iriap != NULL) || (self->tsap != NULL))
    DERROR(IRDA_SOCK_ERROR, "Socket not cleaned up...\n");

  if((irnet_server.running) && (self->q.q_next == NULL))
    {
      spin_lock_bh(&irnet_server.spinlock);
      hashbin_insert(irnet_server.list, (irda_queue_t *) self, 0, self->rname);
      spin_unlock_bh(&irnet_server.spinlock);
      DEBUG(IRDA_SOCK_INFO, "Inserted ``%s'' in hashbin...\n", self->rname);
    }

  
  if((self->rdaddr == DEV_ADDR_ANY) && (self->rname[0] == '\0'))
    {
      
      if((err = irnet_discover_daddr_and_lsap_sel(self)) != 0)
	DRETURN(err, IRDA_SOCK_INFO, "auto-connect failed!\n");
      
    }
  else
    {
      
      if(self->rdaddr == DEV_ADDR_ANY)
	{
	  if((err = irnet_dname_to_daddr(self)) != 0)
	    DRETURN(err, IRDA_SOCK_INFO, "name connect failed!\n");
	}
      else
	
	self->daddr = self->rdaddr;

      
      irnet_find_lsap_sel(self);
      
    }

  DEXIT(IRDA_SOCK_TRACE, "\n");
  return 0;
}

void
irda_irnet_destroy(irnet_socket *	self)
{
  DENTER(IRDA_SOCK_TRACE, "(self=0x%p)\n", self);
  if(self == NULL)
    return;

  if((irnet_server.running) && (self->q.q_next != NULL))
    {
      struct irnet_socket *	entry;
      DEBUG(IRDA_SOCK_INFO, "Removing from hash..\n");
      spin_lock_bh(&irnet_server.spinlock);
      entry = hashbin_remove_this(irnet_server.list, (irda_queue_t *) self);
      self->q.q_next = NULL;
      spin_unlock_bh(&irnet_server.spinlock);
      DASSERT(entry == self, , IRDA_SOCK_ERROR, "Can't remove from hash.\n");
    }

  
  if(test_bit(0, &self->ttp_open))
    {
      irnet_post_event(NULL, IRNET_DISCONNECT_TO,
		       self->saddr, self->daddr, self->rname, 0);
    }

  clear_bit(0, &self->ttp_connect);

  
  clear_bit(0, &self->ttp_open);

  
  irlmp_unregister_client(self->ckey);

  
  if(self->iriap)
    {
      iriap_close(self->iriap);
      self->iriap = NULL;
    }

  
  if(self->discoveries != NULL)
    {
      
      kfree(self->discoveries);
      self->discoveries = NULL;
    }

  
  if(self->tsap)
    {
      DEBUG(IRDA_SOCK_INFO, "Closing our TTP connection.\n");
      irttp_disconnect_request(self->tsap, NULL, P_NORMAL);
      irttp_close_tsap(self->tsap);
      self->tsap = NULL;
    }
  self->stsap_sel = 0;

  DEXIT(IRDA_SOCK_TRACE, "\n");
}



static inline int
irnet_daddr_to_dname(irnet_socket *	self)
{
  struct irda_device_info *discoveries;	
  int	number;			
  int	i;

  DENTER(IRDA_SERV_TRACE, "(self=0x%p)\n", self);

  
  discoveries = irlmp_get_discoveries(&number, 0xffff,
				      DISCOVERY_DEFAULT_SLOTS);
  
  if (discoveries == NULL)
    DRETURN(-ENETUNREACH, IRDA_SERV_INFO, "Cachelog empty...\n");

  
  for(i = 0; i < number; i++)
    {
      
      if(discoveries[i].daddr == self->daddr)
	{
	  
	  strlcpy(self->rname, discoveries[i].info, sizeof(self->rname));
	  self->rname[sizeof(self->rname) - 1] = '\0';
	  DEBUG(IRDA_SERV_INFO, "Device 0x%08x is in fact ``%s''.\n",
		self->daddr, self->rname);
	  kfree(discoveries);
	  DEXIT(IRDA_SERV_TRACE, "\n");
	  return 0;
	}
    }
  
  DEXIT(IRDA_SERV_INFO, ": cannot discover device 0x%08x !!!\n", self->daddr);
  kfree(discoveries);
  return -EADDRNOTAVAIL;
}

static inline irnet_socket *
irnet_find_socket(irnet_socket *	self)
{
  irnet_socket *	new = (irnet_socket *) NULL;
  int			err;

  DENTER(IRDA_SERV_TRACE, "(self=0x%p)\n", self);

  
  self->daddr = irttp_get_daddr(self->tsap);
  self->saddr = irttp_get_saddr(self->tsap);

  
  err = irnet_daddr_to_dname(self);

  
  spin_lock_bh(&irnet_server.spinlock);

  if(err == 0)
    {
      new = (irnet_socket *) hashbin_find(irnet_server.list,
					  0, self->rname);
      if(new)
	DEBUG(IRDA_SERV_INFO, "Socket 0x%p matches rname ``%s''.\n",
	      new, new->rname);
    }

  
  if(new == (irnet_socket *) NULL)
    {
      new = (irnet_socket *) hashbin_get_first(irnet_server.list);
      while(new !=(irnet_socket *) NULL)
	{
	  
	  if((new->rdaddr == self->daddr) || (new->daddr == self->daddr))
	    {
	      
	      DEBUG(IRDA_SERV_INFO, "Socket 0x%p matches daddr %#08x.\n",
		    new, self->daddr);
	      break;
	    }
	  new = (irnet_socket *) hashbin_get_next(irnet_server.list);
	}
    }

  
  if(new == (irnet_socket *) NULL)
    {
      new = (irnet_socket *) hashbin_get_first(irnet_server.list);
      while(new !=(irnet_socket *) NULL)
	{
	  
	  if(!(test_bit(0, &new->ttp_open)) && (new->rdaddr == DEV_ADDR_ANY) &&
	     (new->rname[0] == '\0') && (new->ppp_open))
	    {
	      
	      DEBUG(IRDA_SERV_INFO, "Socket 0x%p is free.\n",
		    new);
	      break;
	    }
	  new = (irnet_socket *) hashbin_get_next(irnet_server.list);
	}
    }

  
  spin_unlock_bh(&irnet_server.spinlock);

  DEXIT(IRDA_SERV_TRACE, " - new = 0x%p\n", new);
  return new;
}

static inline int
irnet_connect_socket(irnet_socket *	server,
		     irnet_socket *	new,
		     struct qos_info *	qos,
		     __u32		max_sdu_size,
		     __u8		max_header_size)
{
  DENTER(IRDA_SERV_TRACE, "(server=0x%p, new=0x%p)\n",
	 server, new);

  
  new->tsap = irttp_dup(server->tsap, new);
  DABORT(new->tsap == NULL, -1, IRDA_SERV_ERROR, "dup failed!\n");

  
  new->stsap_sel = new->tsap->stsap_sel;
  new->dtsap_sel = new->tsap->dtsap_sel;
  new->saddr = irttp_get_saddr(new->tsap);
  new->daddr = irttp_get_daddr(new->tsap);

  new->max_header_size = max_header_size;
  new->max_sdu_size_tx = max_sdu_size;
  new->max_data_size   = max_sdu_size;
#ifdef STREAM_COMPAT
  
  if(max_sdu_size == 0)
    new->max_data_size = irttp_get_max_seg_size(new->tsap);
#endif 

  
  irttp_listen(server->tsap);

  
  irttp_connect_response(new->tsap, new->max_sdu_size_rx, NULL);

  
  set_bit(0, &new->ttp_open);

  clear_bit(0, &new->ttp_connect);
  if(new->iriap)
    {
      iriap_close(new->iriap);
      new->iriap = NULL;
    }
  if(new->discoveries != NULL)
    {
      kfree(new->discoveries);
      new->discoveries = NULL;
    }

#ifdef CONNECT_INDIC_KICK
  ppp_output_wakeup(&new->chan);
#endif 

  
  irnet_post_event(new, IRNET_CONNECT_FROM,
		   new->saddr, new->daddr, server->rname, 0);

  DEXIT(IRDA_SERV_TRACE, "\n");
  return 0;
}

static inline void
irnet_disconnect_server(irnet_socket *	self,
			struct sk_buff *skb)
{
  DENTER(IRDA_SERV_TRACE, "(self=0x%p)\n", self);

  
  kfree_skb(skb);

#ifdef FAIL_SEND_DISCONNECT
  
  irttp_disconnect_request(self->tsap, NULL, P_NORMAL);
#endif 

  
  irnet_post_event(NULL, IRNET_REQUEST_FROM,
		   self->saddr, self->daddr, self->rname, 0);

  
  irttp_listen(self->tsap);

  DEXIT(IRDA_SERV_TRACE, "\n");
}

static inline int
irnet_setup_server(void)
{
  __u16		hints;

  DENTER(IRDA_SERV_TRACE, "()\n");

  
  irda_irnet_create(&irnet_server.s);

  
  irnet_open_tsap(&irnet_server.s);

  
  irnet_server.s.ppp_open = 0;
  irnet_server.s.chan.private = NULL;
  irnet_server.s.file = NULL;

  
  hints = irlmp_service_to_hint(S_LAN);

#ifdef ADVERTISE_HINT
  
  irnet_server.skey = irlmp_register_service(hints);
#endif 

  
  irnet_server.ias_obj = irias_new_object(IRNET_SERVICE_NAME, jiffies);
  irias_add_integer_attrib(irnet_server.ias_obj, IRNET_IAS_VALUE,
			   irnet_server.s.stsap_sel, IAS_KERNEL_ATTR);
  irias_insert_object(irnet_server.ias_obj);

#ifdef DISCOVERY_EVENTS
  
  irlmp_update_client(irnet_server.s.ckey, hints,
		      irnet_discovery_indication, irnet_expiry_indication,
		      (void *) &irnet_server.s);
#endif

  DEXIT(IRDA_SERV_TRACE, " - self=0x%p\n", &irnet_server.s);
  return 0;
}

static inline void
irnet_destroy_server(void)
{
  DENTER(IRDA_SERV_TRACE, "()\n");

#ifdef ADVERTISE_HINT
  
  irlmp_unregister_service(irnet_server.skey);
#endif 

  
  if(irnet_server.ias_obj)
    irias_delete_object(irnet_server.ias_obj);

  
  irda_irnet_destroy(&irnet_server.s);

  DEXIT(IRDA_SERV_TRACE, "\n");
}



static int
irnet_data_indication(void *	instance,
		      void *	sap,
		      struct sk_buff *skb)
{
  irnet_socket *	ap = (irnet_socket *) instance;
  unsigned char *	p;
  int			code = 0;

  DENTER(IRDA_TCB_TRACE, "(self/ap=0x%p, skb=0x%p)\n",
	 ap, skb);
  DASSERT(skb != NULL, 0, IRDA_CB_ERROR, "skb is NULL !!!\n");

  
  if(!ap->ppp_open)
    {
      DERROR(IRDA_CB_ERROR, "PPP not ready, dropping packet...\n");
      return -ENOMEM;
    }

  
  p = skb->data;
  if((p[0] == PPP_ALLSTATIONS) && (p[1] == PPP_UI))
    {
      
      if(skb->len < 3)
	goto err_exit;
      p = skb_pull(skb, 2);
    }

  
  if(p[0] & 1)
    {
      
      skb_push(skb, 1)[0] = 0;
    }
  else
    if(skb->len < 2)
      goto err_exit;

  
  ppp_input(&ap->chan, skb);

  DEXIT(IRDA_TCB_TRACE, "\n");
  return 0;

 err_exit:
  DERROR(IRDA_CB_ERROR, "Packet too small, dropping...\n");
  kfree_skb(skb);
  ppp_input_error(&ap->chan, code);
  return 0;	
}

static void
irnet_disconnect_indication(void *	instance,
			    void *	sap,
			    LM_REASON	reason,
			    struct sk_buff *skb)
{
  irnet_socket *	self = (irnet_socket *) instance;
  int			test_open;
  int			test_connect;

  DENTER(IRDA_TCB_TRACE, "(self=0x%p)\n", self);
  DASSERT(self != NULL, , IRDA_CB_ERROR, "Self is NULL !!!\n");

  
  if(skb)
    dev_kfree_skb(skb);

  
  test_open = test_and_clear_bit(0, &self->ttp_open);
  test_connect = test_and_clear_bit(0, &self->ttp_connect);

  if(!(test_open || test_connect))
    {
      DERROR(IRDA_CB_ERROR, "Race condition detected...\n");
      return;
    }

  
  if(test_open)
    irnet_post_event(self, IRNET_DISCONNECT_FROM,
		     self->saddr, self->daddr, self->rname, 0);
  else
    
    if((self->tsap) && (self != &irnet_server.s))
      irnet_post_event(self, IRNET_NOANSWER_FROM,
		       self->saddr, self->daddr, self->rname, 0);

  
  if((self->tsap) && (self != &irnet_server.s))
    {
      DEBUG(IRDA_CB_INFO, "Closing our TTP connection.\n");
      irttp_close_tsap(self->tsap);
      self->tsap = NULL;
    }
  
  self->stsap_sel = 0;
  self->daddr = DEV_ADDR_ANY;
  self->tx_flow = FLOW_START;

  
  if(self->ppp_open)
    {
      if(test_open)
	{
	  
	  schedule_work(&self->disconnect_work);
	}
      else
	{
	  ppp_output_wakeup(&self->chan);
	}
    }

  DEXIT(IRDA_TCB_TRACE, "\n");
}

static void
irnet_connect_confirm(void *	instance,
		      void *	sap,
		      struct qos_info *qos,
		      __u32	max_sdu_size,
		      __u8	max_header_size,
		      struct sk_buff *skb)
{
  irnet_socket *	self = (irnet_socket *) instance;

  DENTER(IRDA_TCB_TRACE, "(self=0x%p)\n", self);

  
  if(! test_bit(0, &self->ttp_connect))
    {
      DERROR(IRDA_CB_ERROR, "Socket no longer connecting. Ouch !\n");
      return;
    }

  
  self->max_header_size = max_header_size;

  
  self->max_sdu_size_tx = max_sdu_size;
  self->max_data_size = max_sdu_size;
#ifdef STREAM_COMPAT
  if(max_sdu_size == 0)
    self->max_data_size = irttp_get_max_seg_size(self->tsap);
#endif 

  
  self->saddr = irttp_get_saddr(self->tsap);

  
  set_bit(0, &self->ttp_open);
  clear_bit(0, &self->ttp_connect);	
  
  ppp_output_wakeup(&self->chan);

  
  if(skb->len > 0)
    {
#ifdef PASS_CONNECT_PACKETS
      DEBUG(IRDA_CB_INFO, "Passing connect packet to PPP.\n");
      
      irnet_data_indication(instance, sap, skb);
#else 
      DERROR(IRDA_CB_ERROR, "Dropping non empty packet.\n");
      kfree_skb(skb);	
#endif 
    }
  else
    kfree_skb(skb);

  
  irnet_post_event(self, IRNET_CONNECT_TO,
		   self->saddr, self->daddr, self->rname, 0);

  DEXIT(IRDA_TCB_TRACE, "\n");
}

static void
irnet_flow_indication(void *	instance,
		      void *	sap,
		      LOCAL_FLOW flow)
{
  irnet_socket *	self = (irnet_socket *) instance;
  LOCAL_FLOW		oldflow = self->tx_flow;

  DENTER(IRDA_TCB_TRACE, "(self=0x%p, flow=%d)\n", self, flow);

  
  self->tx_flow = flow;

  
  switch(flow)
    {
    case FLOW_START:
      DEBUG(IRDA_CB_INFO, "IrTTP wants us to start again\n");
      
      if(oldflow == FLOW_STOP)
	ppp_output_wakeup(&self->chan);
      else
	DEBUG(IRDA_CB_INFO, "But we were already transmitting !!!\n");
      break;
    case FLOW_STOP:
      DEBUG(IRDA_CB_INFO, "IrTTP wants us to slow down\n");
      break;
    default:
      DEBUG(IRDA_CB_INFO, "Unknown flow command!\n");
      break;
    }

  DEXIT(IRDA_TCB_TRACE, "\n");
}

static void
irnet_status_indication(void *	instance,
			LINK_STATUS link,
			LOCK_STATUS lock)
{
  irnet_socket *	self = (irnet_socket *) instance;

  DENTER(IRDA_TCB_TRACE, "(self=0x%p)\n", self);
  DASSERT(self != NULL, , IRDA_CB_ERROR, "Self is NULL !!!\n");

  
  switch(link)
    {
    case STATUS_NO_ACTIVITY:
      irnet_post_event(self, IRNET_BLOCKED_LINK,
		       self->saddr, self->daddr, self->rname, 0);
      break;
    default:
      DEBUG(IRDA_CB_INFO, "Unknown status...\n");
    }

  DEXIT(IRDA_TCB_TRACE, "\n");
}

static void
irnet_connect_indication(void *		instance,
			 void *		sap,
			 struct qos_info *qos,
			 __u32		max_sdu_size,
			 __u8		max_header_size,
			 struct sk_buff *skb)
{
  irnet_socket *	server = &irnet_server.s;
  irnet_socket *	new = (irnet_socket *) NULL;

  DENTER(IRDA_TCB_TRACE, "(server=0x%p)\n", server);
  DASSERT(instance == &irnet_server, , IRDA_CB_ERROR,
	  "Invalid instance (0x%p) !!!\n", instance);
  DASSERT(sap == irnet_server.s.tsap, , IRDA_CB_ERROR, "Invalid sap !!!\n");

  
  new = irnet_find_socket(server);

  
  if(new == (irnet_socket *) NULL)
    {
      DEXIT(IRDA_CB_INFO, ": No socket waiting for this connection.\n");
      irnet_disconnect_server(server, skb);
      return;
    }

  
  if(test_bit(0, &new->ttp_open))
    {
      DEXIT(IRDA_CB_INFO, ": Socket already connected.\n");
      irnet_disconnect_server(server, skb);
      return;
    }


  
  if(0
#ifdef ALLOW_SIMULT_CONNECT
     || ((irttp_is_primary(server->tsap) == 1) &&	
	 (test_and_clear_bit(0, &new->ttp_connect)))
#endif 
     )
    {
      DERROR(IRDA_CB_ERROR, "Socket already connecting, but going to reuse it !\n");

      
      if(new->tsap != NULL)
	{
	  irttp_close_tsap(new->tsap);
	  new->tsap = NULL;
	}
    }
  else
    {
      if((test_bit(0, &new->ttp_connect)) || (new->tsap != NULL))
	{
	  
	  DERROR(IRDA_CB_ERROR, "Race condition detected, socket in use, abort connect...\n");
	  irnet_disconnect_server(server, skb);
	  return;
	}
    }

  
  irnet_connect_socket(server, new, qos, max_sdu_size, max_header_size);

  
  if(skb->len > 0)
    {
#ifdef PASS_CONNECT_PACKETS
      DEBUG(IRDA_CB_INFO, "Passing connect packet to PPP.\n");
      
      irnet_data_indication(new, new->tsap, skb);
#else 
      DERROR(IRDA_CB_ERROR, "Dropping non empty packet.\n");
      kfree_skb(skb);	
#endif 
    }
  else
    kfree_skb(skb);

  DEXIT(IRDA_TCB_TRACE, "\n");
}



static void
irnet_getvalue_confirm(int	result,
		       __u16	obj_id,
		       struct ias_value *value,
		       void *	priv)
{
  irnet_socket *	self = (irnet_socket *) priv;

  DENTER(IRDA_OCB_TRACE, "(self=0x%p)\n", self);
  DASSERT(self != NULL, , IRDA_OCB_ERROR, "Self is NULL !!!\n");

  if(! test_bit(0, &self->ttp_connect))
    {
      DERROR(IRDA_OCB_ERROR, "Socket no longer connecting. Ouch !\n");
      return;
    }

  
  iriap_close(self->iriap);
  self->iriap = NULL;

  
  self->dtsap_sel = irnet_ias_to_tsap(self, result, value);

  
  if(self->errno)
    {
      clear_bit(0, &self->ttp_connect);
      DERROR(IRDA_OCB_ERROR, "IAS connect failed ! (0x%X)\n", self->errno);
      return;
    }

  DEBUG(IRDA_OCB_INFO, "daddr = %08x, lsap = %d, starting IrTTP connection\n",
	self->daddr, self->dtsap_sel);

  
  irnet_connect_tsap(self);

  DEXIT(IRDA_OCB_TRACE, "\n");
}

static void
irnet_discovervalue_confirm(int		result,
			    __u16	obj_id,
			    struct ias_value *value,
			    void *	priv)
{
  irnet_socket *	self = (irnet_socket *) priv;
  __u8			dtsap_sel;		

  DENTER(IRDA_OCB_TRACE, "(self=0x%p)\n", self);
  DASSERT(self != NULL, , IRDA_OCB_ERROR, "Self is NULL !!!\n");

  if(! test_bit(0, &self->ttp_connect))
    {
      DERROR(IRDA_OCB_ERROR, "Socket no longer connecting. Ouch !\n");
      return;
    }

  
  dtsap_sel = irnet_ias_to_tsap(self, result, value);

  
  if(self->errno == 0)
    {
      
      if(self->daddr != DEV_ADDR_ANY)
	{
	  DERROR(IRDA_OCB_ERROR, "More than one device in range supports IrNET...\n");
	}
      else
	{
	  
	  self->daddr = self->discoveries[self->disco_index].daddr;
	  self->dtsap_sel = dtsap_sel;
	}
    }

  
  if((self->errno == -EADDRNOTAVAIL) || (self->errno == 0))
    {
      int	ret;

      
      ret = irnet_discover_next_daddr(self);
      if(!ret)
	{
	  return;
	}
      
    }

  

  
  iriap_close(self->iriap);
  self->iriap = NULL;

  
  DEBUG(IRDA_OCB_INFO, "Cleaning up log (0x%p)\n",
	self->discoveries);
  if(self->discoveries != NULL)
    {
      
      kfree(self->discoveries);
      self->discoveries = NULL;
    }
  self->disco_number = -1;

  
  if(self->daddr == DEV_ADDR_ANY)
    {
      self->daddr = DEV_ADDR_ANY;
      clear_bit(0, &self->ttp_connect);
      DEXIT(IRDA_OCB_TRACE, ": cannot discover IrNET in any device !!!\n");
      return;
    }

  

  DEBUG(IRDA_OCB_INFO, "daddr = %08x, lsap = %d, starting IrTTP connection\n",
	self->daddr, self->dtsap_sel);

  
  irnet_connect_tsap(self);

  DEXIT(IRDA_OCB_TRACE, "\n");
}

#ifdef DISCOVERY_EVENTS
static void
irnet_discovery_indication(discinfo_t *		discovery,
			   DISCOVERY_MODE	mode,
			   void *		priv)
{
  irnet_socket *	self = &irnet_server.s;

  DENTER(IRDA_OCB_TRACE, "(self=0x%p)\n", self);
  DASSERT(priv == &irnet_server, , IRDA_OCB_ERROR,
	  "Invalid instance (0x%p) !!!\n", priv);

  DEBUG(IRDA_OCB_INFO, "Discovered new IrNET/IrLAN node %s...\n",
	discovery->info);

  
  irnet_post_event(NULL, IRNET_DISCOVER,
		   discovery->saddr, discovery->daddr, discovery->info,
		   get_unaligned((__u16 *)discovery->hints));

  DEXIT(IRDA_OCB_TRACE, "\n");
}

static void
irnet_expiry_indication(discinfo_t *	expiry,
			DISCOVERY_MODE	mode,
			void *		priv)
{
  irnet_socket *	self = &irnet_server.s;

  DENTER(IRDA_OCB_TRACE, "(self=0x%p)\n", self);
  DASSERT(priv == &irnet_server, , IRDA_OCB_ERROR,
	  "Invalid instance (0x%p) !!!\n", priv);

  DEBUG(IRDA_OCB_INFO, "IrNET/IrLAN node %s expired...\n",
	expiry->info);

  
  irnet_post_event(NULL, IRNET_EXPIRE,
		   expiry->saddr, expiry->daddr, expiry->info,
		   get_unaligned((__u16 *)expiry->hints));

  DEXIT(IRDA_OCB_TRACE, "\n");
}
#endif 



#ifdef CONFIG_PROC_FS
static int
irnet_proc_show(struct seq_file *m, void *v)
{
  irnet_socket *	self;
  char *		state;
  int			i = 0;

  
  seq_printf(m, "IrNET server - ");
  seq_printf(m, "IrDA state: %s, ",
		 (irnet_server.running ? "running" : "dead"));
  seq_printf(m, "stsap_sel: %02x, ", irnet_server.s.stsap_sel);
  seq_printf(m, "dtsap_sel: %02x\n", irnet_server.s.dtsap_sel);

  
  if(!irnet_server.running)
    return 0;

  
  spin_lock_bh(&irnet_server.spinlock);

  
  self = (irnet_socket *) hashbin_get_first(irnet_server.list);
  while(self != NULL)
    {
      
      seq_printf(m, "\nIrNET socket %d - ", i++);

      
      seq_printf(m, "Requested IrDA name: \"%s\", ", self->rname);
      seq_printf(m, "daddr: %08x, ", self->rdaddr);
      seq_printf(m, "saddr: %08x\n", self->rsaddr);

      
      seq_printf(m, "	PPP state: %s",
		 (self->ppp_open ? "registered" : "unregistered"));
      if(self->ppp_open)
	{
	  seq_printf(m, ", unit: ppp%d",
			 ppp_unit_number(&self->chan));
	  seq_printf(m, ", channel: %d",
			 ppp_channel_index(&self->chan));
	  seq_printf(m, ", mru: %d",
			 self->mru);
	  
	}

      
      if(self->ttp_open)
	state = "connected";
      else
	if(self->tsap != NULL)
	  state = "connecting";
	else
	  if(self->iriap != NULL)
	    state = "searching";
	  else
	    if(self->ttp_connect)
	      state = "weird";
	    else
	      state = "idle";
      seq_printf(m, "\n	IrDA state: %s, ", state);
      seq_printf(m, "daddr: %08x, ", self->daddr);
      seq_printf(m, "stsap_sel: %02x, ", self->stsap_sel);
      seq_printf(m, "dtsap_sel: %02x\n", self->dtsap_sel);

      
      self = (irnet_socket *) hashbin_get_next(irnet_server.list);
    }

  
  spin_unlock_bh(&irnet_server.spinlock);

  return 0;
}

static int irnet_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, irnet_proc_show, NULL);
}

static const struct file_operations irnet_proc_fops = {
	.owner		= THIS_MODULE,
	.open		= irnet_proc_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};
#endif 



int __init
irda_irnet_init(void)
{
  int		err = 0;

  DENTER(MODULE_TRACE, "()\n");

  
  memset(&irnet_server, 0, sizeof(struct irnet_root));

  
  irnet_server.list = hashbin_new(HB_NOLOCK);
  DABORT(irnet_server.list == NULL, -ENOMEM,
	 MODULE_ERROR, "Can't allocate hashbin!\n");
  
  spin_lock_init(&irnet_server.spinlock);

  
  init_waitqueue_head(&irnet_events.rwait);
  irnet_events.index = 0;
  
  spin_lock_init(&irnet_events.spinlock);

#ifdef CONFIG_PROC_FS
  
  proc_create("irnet", 0, proc_irda, &irnet_proc_fops);
#endif 

  
  err = irnet_setup_server();

  if(!err)
    
    irnet_server.running = 1;

  DEXIT(MODULE_TRACE, "\n");
  return err;
}

void __exit
irda_irnet_cleanup(void)
{
  DENTER(MODULE_TRACE, "()\n");

  
  irnet_server.running = 0;

#ifdef CONFIG_PROC_FS
  
  remove_proc_entry("irnet", proc_irda);
#endif 

  
  irnet_destroy_server();

  
  hashbin_delete(irnet_server.list, (FREE_FUNC) irda_irnet_destroy);

  DEXIT(MODULE_TRACE, "\n");
}
