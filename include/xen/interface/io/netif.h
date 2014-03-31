/******************************************************************************
 * netif.h
 *
 * Unified network-device I/O interface for Xen guest OSes.
 *
 * Copyright (c) 2003-2004, Keir Fraser
 */

#ifndef __XEN_PUBLIC_IO_NETIF_H__
#define __XEN_PUBLIC_IO_NETIF_H__

#include "ring.h"
#include "../grant_table.h"



#define _XEN_NETTXF_csum_blank		(0)
#define  XEN_NETTXF_csum_blank		(1U<<_XEN_NETTXF_csum_blank)

#define _XEN_NETTXF_data_validated	(1)
#define  XEN_NETTXF_data_validated	(1U<<_XEN_NETTXF_data_validated)

#define _XEN_NETTXF_more_data		(2)
#define  XEN_NETTXF_more_data		(1U<<_XEN_NETTXF_more_data)

#define _XEN_NETTXF_extra_info		(3)
#define  XEN_NETTXF_extra_info		(1U<<_XEN_NETTXF_extra_info)

struct xen_netif_tx_request {
    grant_ref_t gref;      
    uint16_t offset;       
    uint16_t flags;        
    uint16_t id;           
    uint16_t size;         
};

#define XEN_NETIF_EXTRA_TYPE_NONE	(0)  
#define XEN_NETIF_EXTRA_TYPE_GSO	(1)  
#define XEN_NETIF_EXTRA_TYPE_MAX	(2)

#define _XEN_NETIF_EXTRA_FLAG_MORE	(0)
#define  XEN_NETIF_EXTRA_FLAG_MORE	(1U<<_XEN_NETIF_EXTRA_FLAG_MORE)

#define XEN_NETIF_GSO_TYPE_TCPV4	(1)

struct xen_netif_extra_info {
	uint8_t type;  
	uint8_t flags; 

	union {
		struct {
			uint16_t size;

			uint8_t type; 

			
			uint8_t pad;

			uint16_t features; 
		} gso;

		uint16_t pad[3];
	} u;
};

struct xen_netif_tx_response {
	uint16_t id;
	int16_t  status;       
};

struct xen_netif_rx_request {
	uint16_t    id;        
	grant_ref_t gref;      
};

#define _XEN_NETRXF_data_validated	(0)
#define  XEN_NETRXF_data_validated	(1U<<_XEN_NETRXF_data_validated)

#define _XEN_NETRXF_csum_blank		(1)
#define  XEN_NETRXF_csum_blank		(1U<<_XEN_NETRXF_csum_blank)

#define _XEN_NETRXF_more_data		(2)
#define  XEN_NETRXF_more_data		(1U<<_XEN_NETRXF_more_data)

#define _XEN_NETRXF_extra_info		(3)
#define  XEN_NETRXF_extra_info		(1U<<_XEN_NETRXF_extra_info)

#define _XEN_NETRXF_gso_prefix		(4)
#define  XEN_NETRXF_gso_prefix		(1U<<_XEN_NETRXF_gso_prefix)

struct xen_netif_rx_response {
    uint16_t id;
    uint16_t offset;       
    uint16_t flags;        
    int16_t  status;       
};


DEFINE_RING_TYPES(xen_netif_tx,
		  struct xen_netif_tx_request,
		  struct xen_netif_tx_response);
DEFINE_RING_TYPES(xen_netif_rx,
		  struct xen_netif_rx_request,
		  struct xen_netif_rx_response);

#define XEN_NETIF_RSP_DROPPED	-2
#define XEN_NETIF_RSP_ERROR	-1
#define XEN_NETIF_RSP_OKAY	 0
#define XEN_NETIF_RSP_NULL	 1

#endif
