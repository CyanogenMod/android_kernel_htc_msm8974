
#ifndef IRNET_IRDA_H
#define IRNET_IRDA_H


#include "irnet.h"		


#define IRNET_SERVICE_NAME	"IrNetv1"
#define IRNET_IAS_VALUE		"IrDA:TinyTP:LsapSel"
#define IRNET_NOTIFY_NAME	"IrNET socket"
#define IRNET_NOTIFY_NAME_SERV	"IrNET server"


typedef struct irnet_root
{
  irnet_socket		s;		

  
  int			magic;		
  int			running;	

  
  hashbin_t *		list;
  spinlock_t		spinlock;	

  
  void *		skey;

  
  struct ias_object *	ias_obj;	

} irnet_root;



static void
	irnet_post_event(irnet_socket *,
			 irnet_event,
			 __u32,
			 __u32,
			 char *,
			 __u16);
static inline int
	irnet_open_tsap(irnet_socket *);
static inline __u8
	irnet_ias_to_tsap(irnet_socket *,
			  int,
			  struct ias_value *);
static inline int
	irnet_find_lsap_sel(irnet_socket *);
static inline int
	irnet_connect_tsap(irnet_socket *);
static inline int
	irnet_discover_next_daddr(irnet_socket *);
static inline int
	irnet_discover_daddr_and_lsap_sel(irnet_socket *);
static inline int
	irnet_dname_to_daddr(irnet_socket *);
static inline int
	irnet_daddr_to_dname(irnet_socket *);
static inline irnet_socket *
	irnet_find_socket(irnet_socket *);
static inline int
	irnet_connect_socket(irnet_socket *,
			     irnet_socket *,
			     struct qos_info *,
			     __u32,
			     __u8);
static inline void
	irnet_disconnect_server(irnet_socket *,
				struct sk_buff *);
static inline int
	irnet_setup_server(void);
static inline void
	irnet_destroy_server(void);
static int
	irnet_data_indication(void *,		
			      void *,		
			      struct sk_buff *);
static void
	irnet_disconnect_indication(void *,
				    void *,
				    LM_REASON,
				    struct sk_buff *);
static void
	irnet_connect_confirm(void *,
			      void *,
			      struct qos_info *,
			      __u32,
			      __u8,
			      struct sk_buff *);
static void
	irnet_flow_indication(void *,
			      void *,
			      LOCAL_FLOW);
static void
	irnet_status_indication(void *,
				LINK_STATUS,
				LOCK_STATUS);
static void
	irnet_connect_indication(void *,
				 void *,
				 struct qos_info *,
				 __u32,
				 __u8,
				 struct sk_buff *);
static void
	irnet_getvalue_confirm(int,
			       __u16,
			       struct ias_value *,
			       void *);
static void
	irnet_discovervalue_confirm(int,
				    __u16,
				    struct ias_value *,
				    void *);
#ifdef DISCOVERY_EVENTS
static void
	irnet_discovery_indication(discinfo_t *,
				   DISCOVERY_MODE,
				   void *);
static void
	irnet_expiry_indication(discinfo_t *,
				DISCOVERY_MODE,
				void *);
#endif


static struct irnet_root	irnet_server;

struct irnet_ctrl_channel	irnet_events;

#ifdef CONFIG_PROC_FS
extern struct proc_dir_entry *proc_irda;
#endif 

#endif 
