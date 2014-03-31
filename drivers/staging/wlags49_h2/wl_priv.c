/*******************************************************************************
 * Agere Systems Inc.
 * Wireless device driver for Linux (wlags49).
 *
 * Copyright (c) 1998-2003 Agere Systems Inc.
 * All rights reserved.
 *   http://www.agere.com
 *
 * Initially developed by TriplePoint, Inc.
 *   http://www.triplepoint.com
 *
 *------------------------------------------------------------------------------
 *
 *   This file defines handling routines for the private IOCTLs
 *
 *------------------------------------------------------------------------------
 *
 * SOFTWARE LICENSE
 *
 * This software is provided subject to the following terms and conditions,
 * which you should read carefully before using the software.  Using this
 * software indicates your acceptance of these terms and conditions.  If you do
 * not agree with these terms and conditions, do not use the software.
 *
 * Copyright © 2003 Agere Systems Inc.
 * All rights reserved.
 *
 * Redistribution and use in source or binary forms, with or without
 * modifications, are permitted provided that the following conditions are met:
 *
 * . Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following Disclaimer as comments in the code as
 *    well as in the documentation and/or other materials provided with the
 *    distribution.
 *
 * . Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following Disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * . Neither the name of Agere Systems Inc. nor the names of the contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * Disclaimer
 *
 * THIS SOFTWARE IS PROVIDED AS IS AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, INFRINGEMENT AND THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  ANY
 * USE, MODIFICATION OR DISTRIBUTION OF THIS SOFTWARE IS SOLELY AT THE USERS OWN
 * RISK. IN NO EVENT SHALL AGERE SYSTEMS INC. OR CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, INCLUDING, BUT NOT LIMITED TO, CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 *
 ******************************************************************************/

#include <wl_version.h>

#include <linux/if_arp.h>
#include <linux/ioport.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <asm/uaccess.h>

#include <debug.h>
#include <hcf.h>
#include <hcfdef.h>

#include <wl_if.h>
#include <wl_internal.h>
#include <wl_enc.h>
#include <wl_main.h>
#include <wl_priv.h>
#include <wl_util.h>
#include <wl_netdev.h>

int wvlan_uil_connect( struct uilreq *urq, struct wl_private *lp );
int wvlan_uil_disconnect( struct uilreq *urq, struct wl_private *lp );
int wvlan_uil_action( struct uilreq *urq, struct wl_private *lp );
int wvlan_uil_block( struct uilreq *urq, struct wl_private *lp );
int wvlan_uil_unblock( struct uilreq *urq, struct wl_private *lp );
int wvlan_uil_send_diag_msg( struct uilreq *urq, struct wl_private *lp );
int wvlan_uil_put_info( struct uilreq *urq, struct wl_private *lp );
int wvlan_uil_get_info( struct uilreq *urq, struct wl_private *lp );

int cfg_driver_info( struct uilreq *urq, struct wl_private *lp );
int cfg_driver_identity( struct uilreq *urq, struct wl_private *lp );


#if DBG
extern dbg_info_t *DbgInfo;
#endif  




#ifdef USE_UIL

int wvlan_uil( struct uilreq *urq, struct wl_private *lp )
{
	int ioctl_ret = 0;
	

	DBG_FUNC( "wvlan_uil" );
	DBG_ENTER( DbgInfo );

	switch( urq->command ) {
	  case UIL_FUN_CONNECT:
		DBG_TRACE(DbgInfo, "IOCTL: WVLAN2_IOCTL_UIL -- WVLAN2_UIL_CONNECT\n");
		ioctl_ret = wvlan_uil_connect( urq, lp );
		break;
	  case UIL_FUN_DISCONNECT:
		DBG_TRACE(DbgInfo, "IOCTL: WVLAN2_IOCTL_UIL -- WVLAN2_UIL_DISCONNECT\n");
		ioctl_ret = wvlan_uil_disconnect( urq, lp );
		break;
	  case UIL_FUN_ACTION:
		DBG_TRACE(DbgInfo, "IOCTL: WVLAN2_IOCTL_UIL -- WVLAN2_UIL_ACTION\n" );
		ioctl_ret = wvlan_uil_action( urq, lp );
		break;
	  case UIL_FUN_SEND_DIAG_MSG:
		DBG_TRACE(DbgInfo, "IOCTL: WVLAN2_IOCTL_UIL -- WVLAN2_UIL_SEND_DIAG_MSG\n");
		ioctl_ret = wvlan_uil_send_diag_msg( urq, lp );
		break;
	  case UIL_FUN_GET_INFO:
		DBG_TRACE(DbgInfo, "IOCTL: WVLAN2_IOCTL_UIL -- WVLAN2_UIL_GET_INFO\n");
		ioctl_ret = wvlan_uil_get_info( urq, lp );
		break;
	  case UIL_FUN_PUT_INFO:
		DBG_TRACE(DbgInfo, "IOCTL: WVLAN2_IOCTL_UIL -- WVLAN2_UIL_PUT_INFO\n");
		ioctl_ret = wvlan_uil_put_info( urq, lp );
		break;
	default:
		DBG_TRACE(DbgInfo, "IOCTL: WVLAN2_IOCTL_UIL -- UNSUPPORTED UIL CODE: 0x%X", urq->command );
		ioctl_ret = -EOPNOTSUPP;
		break;
	}
	DBG_LEAVE( DbgInfo );
	return ioctl_ret;
} 




int wvlan_uil_connect( struct uilreq *urq, struct wl_private *lp )
{
	int result = 0;
	


	DBG_FUNC( "wvlan_uil_connect" );
	DBG_ENTER( DbgInfo );


	if( !( lp->flags & WVLAN2_UIL_CONNECTED )) {
		lp->flags |= WVLAN2_UIL_CONNECTED;
		urq->hcfCtx = &( lp->hcfCtx );
		urq->result = UIL_SUCCESS;
	} else {
		DBG_WARNING( DbgInfo, "UIL_ERR_IN_USE\n" );
		urq->result = UIL_ERR_IN_USE;
	}

	DBG_LEAVE( DbgInfo );
	return result;
} 




int wvlan_uil_disconnect( struct uilreq *urq, struct wl_private *lp )
{
	int result = 0;
	


	DBG_FUNC( "wvlan_uil_disconnect" );
	DBG_ENTER( DbgInfo );


	if( urq->hcfCtx == &( lp->hcfCtx )) {
		if (lp->flags & WVLAN2_UIL_CONNECTED) {
			lp->flags &= ~WVLAN2_UIL_CONNECTED;
		}

		urq->hcfCtx = NULL;
		urq->result = UIL_SUCCESS;
	} else {
		DBG_ERROR( DbgInfo, "UIL_ERR_WRONG_IFB\n" );
		urq->result = UIL_ERR_WRONG_IFB;
	}

	DBG_LEAVE( DbgInfo );
	return result;
} 




int wvlan_uil_action( struct uilreq *urq, struct wl_private *lp )
{
	int     result = 0;
	ltv_t   *ltv;
	


	DBG_FUNC( "wvlan_uil_action" );
	DBG_ENTER( DbgInfo );


	if( urq->hcfCtx == &( lp->hcfCtx )) {
		
		ltv = (ltv_t *)urq->data;
		if( ltv != NULL ) {
			switch( ltv->typ ) {
			case UIL_ACT_BLOCK:
				DBG_TRACE( DbgInfo, "UIL_ACT_BLOCK\n" );
				result = wvlan_uil_block( urq, lp );
				break;
			case UIL_ACT_UNBLOCK:
				DBG_TRACE( DbgInfo, "UIL_ACT_UNBLOCK\n" );
				result = wvlan_uil_unblock( urq, lp );
				break;
			case UIL_ACT_SCAN:
				DBG_TRACE( DbgInfo, "UIL_ACT_SCAN\n" );
				urq->result = hcf_action( &( lp->hcfCtx ), MDD_ACT_SCAN );
				break;
			case UIL_ACT_APPLY:
				DBG_TRACE( DbgInfo, "UIL_ACT_APPLY\n" );
				urq->result = wl_apply( lp );
				break;
			case UIL_ACT_RESET:
				DBG_TRACE( DbgInfo, "UIL_ACT_RESET\n" );
				urq->result = wl_go( lp );
				break;
			default:
				DBG_WARNING( DbgInfo, "Unknown action code: 0x%x\n", ltv->typ );
				break;
			}
		} else {
			DBG_ERROR( DbgInfo, "Bad LTV for this action\n" );
			urq->result = UIL_ERR_LEN;
		}
	} else {
		DBG_ERROR( DbgInfo, "UIL_ERR_WRONG_IFB\n" );
		urq->result = UIL_ERR_WRONG_IFB;
	}

	DBG_LEAVE( DbgInfo );
	return result;
} 





int wvlan_uil_block( struct uilreq *urq, struct wl_private *lp )
{
	int result = 0;
	


	DBG_FUNC( "wvlan_uil_block" );
	DBG_ENTER( DbgInfo );

	if( urq->hcfCtx == &( lp->hcfCtx )) {
		if( capable( CAP_NET_ADMIN )) {
			lp->flags |= WVLAN2_UIL_BUSY;
			netif_stop_queue(lp->dev);
			WL_WDS_NETIF_STOP_QUEUE( lp );
			urq->result = UIL_SUCCESS;
		} else {
			DBG_ERROR( DbgInfo, "EPERM\n" );
			urq->result = UIL_FAILURE;
			result = -EPERM;
		}
	} else {
		DBG_ERROR( DbgInfo, "UIL_ERR_WRONG_IFB\n" );
		urq->result = UIL_ERR_WRONG_IFB;
	}

	DBG_LEAVE( DbgInfo );
	return result;
} 




int wvlan_uil_unblock( struct uilreq *urq, struct wl_private *lp )
{
	int result = 0;
	


	DBG_FUNC( "wvlan_uil_unblock" );
	DBG_ENTER( DbgInfo );

	if( urq->hcfCtx == &( lp->hcfCtx )) {
		if( capable( CAP_NET_ADMIN )) {
			if (lp->flags & WVLAN2_UIL_BUSY) {
				lp->flags &= ~WVLAN2_UIL_BUSY;
				netif_wake_queue(lp->dev);
				WL_WDS_NETIF_WAKE_QUEUE( lp );
			}
		} else {
			DBG_ERROR( DbgInfo, "EPERM\n" );
			urq->result = UIL_FAILURE;
			result = -EPERM;
		}
	} else {
		DBG_ERROR( DbgInfo, "UIL_ERR_WRONG_IFB\n" );
		urq->result = UIL_ERR_WRONG_IFB;
	}

	DBG_LEAVE( DbgInfo );
	return result;
} 




int wvlan_uil_send_diag_msg( struct uilreq *urq, struct wl_private *lp )
{
	int         result = 0;
	DESC_STRCT  Descp[1];
	


	DBG_FUNC( "wvlan_uil_send_diag_msg" );
	DBG_ENTER( DbgInfo );

	if( urq->hcfCtx == &( lp->hcfCtx )) {
		if( capable( CAP_NET_ADMIN )) {
			if ((urq->data != NULL) && (urq->len != 0)) {
				if (lp->hcfCtx.IFB_RscInd != 0) {
					u_char *data;

					
					result = verify_area(VERIFY_READ, urq->data, urq->len);
					if (result != 0) {
						DBG_ERROR( DbgInfo, "verify_area failed, result: %d\n", result );
						urq->result = UIL_FAILURE;
						DBG_LEAVE( DbgInfo );
						return result;
					}

					data = kmalloc(urq->len, GFP_KERNEL);
					if (data != NULL) {
						memset( Descp, 0, sizeof( DESC_STRCT ));
						memcpy( data, urq->data, urq->len );

						Descp[0].buf_addr       = (wci_bufp)data;
						Descp[0].BUF_CNT        = urq->len;
						Descp[0].next_desc_addr = 0;    

						hcf_send_msg( &(lp->hcfCtx),  &Descp[0], HCF_PORT_0 );
						kfree( data );
					} else {
						DBG_ERROR( DbgInfo, "ENOMEM\n" );
						urq->result = UIL_FAILURE;
						result = -ENOMEM;
						DBG_LEAVE( DbgInfo );
						return result;
					}

				} else {
					urq->result = UIL_ERR_BUSY;
				}

			} else {
				urq->result = UIL_FAILURE;
			}
		} else {
			DBG_ERROR( DbgInfo, "EPERM\n" );
			urq->result = UIL_FAILURE;
			result = -EPERM;
		}
	} else {
		DBG_ERROR( DbgInfo, "UIL_ERR_WRONG_IFB\n" );
		urq->result = UIL_ERR_WRONG_IFB;
	}

	DBG_LEAVE( DbgInfo );
	return result;
} 


int wvlan_uil_put_info( struct uilreq *urq, struct wl_private *lp )
{
	int                     result = 0;
	ltv_t                   *pLtv;
	bool_t                  ltvAllocated = FALSE;
	ENCSTRCT                sEncryption;

#ifdef USE_WDS
	hcf_16                  hcfPort  = HCF_PORT_0;
#endif  
	
	DBG_FUNC( "wvlan_uil_put_info" );
	DBG_ENTER( DbgInfo );


	if( urq->hcfCtx == &( lp->hcfCtx )) {
		if( capable( CAP_NET_ADMIN )) {
			if(( urq->data != NULL ) && ( urq->len != 0 )) {
				
				if( urq->len < ( sizeof( hcf_16 ) * 2 )) {
					urq->len = sizeof( lp->ltvRecord );
					urq->result = UIL_ERR_LEN;
					DBG_ERROR( DbgInfo, "No Length/Type in LTV!!!\n" );
					DBG_ERROR( DbgInfo, "UIL_ERR_LEN\n" );
					DBG_LEAVE( DbgInfo );
					return result;
				}

				
				result = verify_area( VERIFY_READ, urq->data, urq->len );
				if( result != 0 ) {
					urq->result = UIL_FAILURE;
					DBG_ERROR( DbgInfo, "verify_area(), VERIFY_READ FAILED\n" );
					DBG_LEAVE( DbgInfo );
					return result;
				}

				
				copy_from_user( &( lp->ltvRecord ), urq->data, sizeof( hcf_16 ) * 2 );

				if((( lp->ltvRecord.len + 1 ) * sizeof( hcf_16 )) > urq->len ) {
					urq->len = sizeof( lp->ltvRecord );
					urq->result = UIL_ERR_LEN;
					DBG_ERROR( DbgInfo, "UIL_ERR_LEN\n" );
					DBG_LEAVE( DbgInfo );
					return result;
				}

				if( urq->len > sizeof( lp->ltvRecord )) {
					pLtv = kmalloc(urq->len, GFP_KERNEL);
					if (pLtv != NULL) {
						ltvAllocated = TRUE;
					} else {
						DBG_ERROR( DbgInfo, "Alloc FAILED\n" );
						urq->len = sizeof( lp->ltvRecord );
						urq->result = UIL_ERR_LEN;
						result = -ENOMEM;
						DBG_LEAVE( DbgInfo );
						return result;
					}
				} else {
					pLtv = &( lp->ltvRecord );
				}

				copy_from_user( pLtv, urq->data, urq->len );


				switch( pLtv->typ ) {
				case CFG_CNF_PORT_TYPE:
					lp->PortType    = pLtv->u.u16[0];
					pLtv->u.u16[0]  = CNV_INT_TO_LITTLE( pLtv->u.u16[0] );
					break;
				case CFG_CNF_OWN_MAC_ADDR:
					
					break;
				case CFG_CNF_OWN_CHANNEL:
					lp->Channel     = pLtv->u.u16[0];
					pLtv->u.u16[0]  = CNV_INT_TO_LITTLE( pLtv->u.u16[0] );
					break;
				
				case CFG_CNF_OWN_ATIM_WINDOW:
					lp->atimWindow  = pLtv->u.u16[0];
					pLtv->u.u16[0]  = CNV_INT_TO_LITTLE( pLtv->u.u16[0] );
					break;
				case CFG_CNF_SYSTEM_SCALE:
					lp->DistanceBetweenAPs  = pLtv->u.u16[0];
					pLtv->u.u16[0]          = CNV_INT_TO_LITTLE( pLtv->u.u16[0] );

				case CFG_CNF_MAX_DATA_LEN:
					break;
				case CFG_CNF_PM_ENABLED:
					lp->PMEnabled   = pLtv->u.u16[0];
					pLtv->u.u16[0]  = CNV_INT_TO_LITTLE( pLtv->u.u16[0] );
					break;
				case CFG_CNF_MCAST_RX:
					lp->MulticastReceive    = pLtv->u.u16[0];
					pLtv->u.u16[0]          = CNV_INT_TO_LITTLE( pLtv->u.u16[0] );
					break;
				case CFG_CNF_MAX_SLEEP_DURATION:
					lp->MaxSleepDuration    = pLtv->u.u16[0];
					pLtv->u.u16[0]          = CNV_INT_TO_LITTLE( pLtv->u.u16[0] );
					break;
				case CFG_CNF_HOLDOVER_DURATION:
					lp->holdoverDuration    = pLtv->u.u16[0];
					pLtv->u.u16[0]          = CNV_INT_TO_LITTLE( pLtv->u.u16[0] );
					break;
				case CFG_CNF_OWN_NAME:
					memset( lp->StationName, 0, sizeof( lp->StationName ));
					memcpy( (void *)lp->StationName, (void *)&pLtv->u.u8[2], (size_t)pLtv->u.u16[0]);
					pLtv->u.u16[0] = CNV_INT_TO_LITTLE( pLtv->u.u16[0] );
					break;
				case CFG_CNF_LOAD_BALANCING:
					lp->loadBalancing       = pLtv->u.u16[0];
					pLtv->u.u16[0]          = CNV_INT_TO_LITTLE( pLtv->u.u16[0] );
					break;
				case CFG_CNF_MEDIUM_DISTRIBUTION:
					lp->mediumDistribution  = pLtv->u.u16[0];
					pLtv->u.u16[0]          = CNV_INT_TO_LITTLE( pLtv->u.u16[0] );
					break;
#ifdef WARP
				case CFG_CNF_TX_POW_LVL:
					lp->txPowLevel          = pLtv->u.u16[0];
					pLtv->u.u16[0]          = CNV_INT_TO_LITTLE( pLtv->u.u16[0] );
					break;
				
				
				case CFG_SUPPORTED_RATE_SET_CNTL:        
					lp->srsc[0]             = pLtv->u.u16[0];
					lp->srsc[1]             = pLtv->u.u16[1];
					pLtv->u.u16[0]          = CNV_INT_TO_LITTLE( pLtv->u.u16[0] );
					pLtv->u.u16[1]          = CNV_INT_TO_LITTLE( pLtv->u.u16[1] );
					break;
				case CFG_BASIC_RATE_SET_CNTL:        
					lp->brsc[0]             = pLtv->u.u16[0];
					lp->brsc[1]             = pLtv->u.u16[1];
					pLtv->u.u16[0]          = CNV_INT_TO_LITTLE( pLtv->u.u16[0] );
					pLtv->u.u16[1]          = CNV_INT_TO_LITTLE( pLtv->u.u16[1] );
					break;
				case CFG_CNF_CONNECTION_CNTL:
					lp->connectionControl   = pLtv->u.u16[0];
					pLtv->u.u16[0]          = CNV_INT_TO_LITTLE( pLtv->u.u16[0] );
					break;
				
#endif  

#if 1 
		

				case CFG_CNF_OWN_DTIM_PERIOD:
					lp->DTIMPeriod  = pLtv->u.u16[0];
					pLtv->u.u16[0]  = CNV_INT_TO_LITTLE( pLtv->u.u16[0] );
					break;
#ifdef WARP
				case CFG_CNF_OWN_BEACON_INTERVAL:        
					lp->ownBeaconInterval   = pLtv->u.u16[0];
					pLtv->u.u16[0]          = CNV_INT_TO_LITTLE( pLtv->u.u16[0] );
					break;
#endif 
				case CFG_COEXISTENSE_BEHAVIOUR:         
					lp->coexistence         = pLtv->u.u16[0];
					pLtv->u.u16[0]          = CNV_INT_TO_LITTLE( pLtv->u.u16[0] );
					break;
#ifdef USE_WDS
				case CFG_CNF_WDS_ADDR1:
					memcpy( &lp->wds_port[0].wdsAddress, &pLtv->u.u8[0], ETH_ALEN );
					hcfPort = HCF_PORT_1;
					break;
				case CFG_CNF_WDS_ADDR2:
					memcpy( &lp->wds_port[1].wdsAddress, &pLtv->u.u8[0], ETH_ALEN );
					hcfPort = HCF_PORT_2;
					break;
				case CFG_CNF_WDS_ADDR3:
					memcpy( &lp->wds_port[2].wdsAddress, &pLtv->u.u8[0], ETH_ALEN );
					hcfPort = HCF_PORT_3;
					break;
				case CFG_CNF_WDS_ADDR4:
					memcpy( &lp->wds_port[3].wdsAddress, &pLtv->u.u8[0], ETH_ALEN );
					hcfPort = HCF_PORT_4;
					break;
				case CFG_CNF_WDS_ADDR5:
					memcpy( &lp->wds_port[4].wdsAddress, &pLtv->u.u8[0], ETH_ALEN );
					hcfPort = HCF_PORT_5;
					break;
				case CFG_CNF_WDS_ADDR6:
					memcpy( &lp->wds_port[5].wdsAddress, &pLtv->u.u8[0], ETH_ALEN );
					hcfPort = HCF_PORT_6;
					break;
#endif  

				case CFG_CNF_MCAST_PM_BUF:
					lp->multicastPMBuffering    = pLtv->u.u16[0];
					pLtv->u.u16[0]              = CNV_INT_TO_LITTLE( pLtv->u.u16[0] );
					break;
				case CFG_CNF_REJECT_ANY:
					lp->RejectAny   = pLtv->u.u16[0];
					pLtv->u.u16[0]  = CNV_INT_TO_LITTLE( pLtv->u.u16[0] );
					break;
#endif

				case CFG_CNF_ENCRYPTION:
					lp->EnableEncryption    = pLtv->u.u16[0];
					pLtv->u.u16[0]          = CNV_INT_TO_LITTLE( pLtv->u.u16[0] );
					break;
				case CFG_CNF_AUTHENTICATION:
					lp->authentication  = pLtv->u.u16[0];
					pLtv->u.u16[0]      = CNV_INT_TO_LITTLE( pLtv->u.u16[0] );
					break;
#if 1 
		

				
					
					
					
				case CFG_CNF_MCAST_RATE:
					
					break;
				case CFG_CNF_INTRA_BSS_RELAY:
					lp->intraBSSRelay   = pLtv->u.u16[0];
					pLtv->u.u16[0]      = CNV_INT_TO_LITTLE( pLtv->u.u16[0] );
					break;
#endif

				case CFG_CNF_MICRO_WAVE:
					
					break;
				
					
					
				
					
					
				
					
					
				
					
					
				case CFG_CNF_OWN_SSID:
				
				case CFG_DESIRED_SSID:
					memset( lp->NetworkName, 0, sizeof( lp->NetworkName ));
					memcpy( (void *)lp->NetworkName, (void *)&pLtv->u.u8[2], (size_t)pLtv->u.u16[0] );
					pLtv->u.u16[0] = CNV_INT_TO_LITTLE( pLtv->u.u16[0] );

					
					if(( strlen( &pLtv->u.u8[2]        ) == 0 ) ||
					   ( strcmp( &pLtv->u.u8[2], "ANY" ) == 0 ) ||
					   ( strcmp( &pLtv->u.u8[2], "any" ) == 0 )) {
						pLtv->u.u16[0] = 0;
						pLtv->u.u8[2]  = 0;
					}
					break;
				case CFG_GROUP_ADDR:
					
					break;
				case CFG_CREATE_IBSS:
					lp->CreateIBSS  = pLtv->u.u16[0];
					pLtv->u.u16[0]  = CNV_INT_TO_LITTLE( pLtv->u.u16[0] );
					break;
				case CFG_RTS_THRH:
					lp->RTSThreshold    = pLtv->u.u16[0];
					pLtv->u.u16[0]      = CNV_INT_TO_LITTLE( pLtv->u.u16[0] );
					break;
				case CFG_TX_RATE_CNTL:
					lp->TxRateControl[0]    = pLtv->u.u16[0];
					lp->TxRateControl[1]    = pLtv->u.u16[1];
					pLtv->u.u16[0]          = CNV_INT_TO_LITTLE( pLtv->u.u16[0] );
					pLtv->u.u16[1]          = CNV_INT_TO_LITTLE( pLtv->u.u16[1] );
					break;
				case CFG_PROMISCUOUS_MODE:
					
					break;
				
					
					
#if 1 
		
				case CFG_RTS_THRH0:
					lp->RTSThreshold    = pLtv->u.u16[0];
					pLtv->u.u16[0]      = CNV_INT_TO_LITTLE( pLtv->u.u16[0] );
					break;
				case CFG_TX_RATE_CNTL0:
					pLtv->u.u16[0]      = CNV_INT_TO_LITTLE( pLtv->u.u16[0] );
					break;
#ifdef USE_WDS
				case CFG_RTS_THRH1:
					lp->wds_port[0].rtsThreshold    = pLtv->u.u16[0];
					pLtv->u.u16[0]                  = CNV_INT_TO_LITTLE( pLtv->u.u16[0] );
					hcfPort                         = HCF_PORT_1;
					break;
				case CFG_RTS_THRH2:
					lp->wds_port[1].rtsThreshold    = pLtv->u.u16[0];
					pLtv->u.u16[0]                  = CNV_INT_TO_LITTLE( pLtv->u.u16[0] );
					hcfPort                         = HCF_PORT_2;
					break;
				case CFG_RTS_THRH3:
					lp->wds_port[2].rtsThreshold    = pLtv->u.u16[0];
					pLtv->u.u16[0]                  = CNV_INT_TO_LITTLE( pLtv->u.u16[0] );
					hcfPort                         = HCF_PORT_3;
					break;
				case CFG_RTS_THRH4:
					lp->wds_port[3].rtsThreshold    = pLtv->u.u16[0];
					pLtv->u.u16[0]                  = CNV_INT_TO_LITTLE( pLtv->u.u16[0] );
					hcfPort                         = HCF_PORT_4;
					break;
				case CFG_RTS_THRH5:
					lp->wds_port[4].rtsThreshold    = pLtv->u.u16[0];
					pLtv->u.u16[0]                  = CNV_INT_TO_LITTLE( pLtv->u.u16[0] );
					hcfPort                         = HCF_PORT_5;
					break;
				case CFG_RTS_THRH6:
					lp->wds_port[5].rtsThreshold    = pLtv->u.u16[0];
					pLtv->u.u16[0]                  = CNV_INT_TO_LITTLE( pLtv->u.u16[0] );
					hcfPort                         = HCF_PORT_6;
					break;
				case CFG_TX_RATE_CNTL1:
					lp->wds_port[0].txRateCntl  = pLtv->u.u16[0];
					pLtv->u.u16[0]              = CNV_INT_TO_LITTLE( pLtv->u.u16[0] );
					hcfPort                     = HCF_PORT_1;
					break;
				case CFG_TX_RATE_CNTL2:
					lp->wds_port[1].txRateCntl  = pLtv->u.u16[0];
					pLtv->u.u16[0]              = CNV_INT_TO_LITTLE( pLtv->u.u16[0] );
					hcfPort                     = HCF_PORT_2;
					break;
				case CFG_TX_RATE_CNTL3:
					lp->wds_port[2].txRateCntl  = pLtv->u.u16[0];
					pLtv->u.u16[0]              = CNV_INT_TO_LITTLE( pLtv->u.u16[0] );
					hcfPort                     = HCF_PORT_3;
					break;
				case CFG_TX_RATE_CNTL4:
					lp->wds_port[3].txRateCntl  = pLtv->u.u16[0];
					pLtv->u.u16[0]              = CNV_INT_TO_LITTLE( pLtv->u.u16[0] );
					hcfPort                     = HCF_PORT_4;
					break;
				case CFG_TX_RATE_CNTL5:
					lp->wds_port[4].txRateCntl  = pLtv->u.u16[0];
					pLtv->u.u16[0]              = CNV_INT_TO_LITTLE( pLtv->u.u16[0] );
					hcfPort                     = HCF_PORT_5;
					break;
				case CFG_TX_RATE_CNTL6:
					lp->wds_port[5].txRateCntl  = pLtv->u.u16[0];
					pLtv->u.u16[0]              = CNV_INT_TO_LITTLE( pLtv->u.u16[0] );
					hcfPort                     = HCF_PORT_6;
					break;
#endif  
#endif  

				case CFG_DEFAULT_KEYS:
					{
						CFG_DEFAULT_KEYS_STRCT *pKeys = (CFG_DEFAULT_KEYS_STRCT *)pLtv;

						pKeys->key[0].len = CNV_INT_TO_LITTLE( pKeys->key[0].len );
						pKeys->key[1].len = CNV_INT_TO_LITTLE( pKeys->key[1].len );
						pKeys->key[2].len = CNV_INT_TO_LITTLE( pKeys->key[2].len );
						pKeys->key[3].len = CNV_INT_TO_LITTLE( pKeys->key[3].len );

						memcpy( (void *)&(lp->DefaultKeys), (void *)pKeys,
								sizeof( CFG_DEFAULT_KEYS_STRCT ));
					}
					break;
				case CFG_TX_KEY_ID:
					lp->TransmitKeyID   = pLtv->u.u16[0];
					pLtv->u.u16[0]      = CNV_INT_TO_LITTLE( pLtv->u.u16[0] );
					break;
				case CFG_SCAN_SSID:
					
					break;
				case CFG_TICK_TIME:
					
					break;
				
				case CFG_MAX_LOAD_TIME:
				case CFG_DL_BUF:
				
				case CFG_NIC_SERIAL_NUMBER:
				case CFG_NIC_IDENTITY:
				case CFG_NIC_MFI_SUP_RANGE:
				case CFG_NIC_CFI_SUP_RANGE:
				case CFG_NIC_TEMP_TYPE:
				case CFG_NIC_PROFILE:
				case CFG_FW_IDENTITY:
				case CFG_FW_SUP_RANGE:
				case CFG_MFI_ACT_RANGES_STA:
				case CFG_CFI_ACT_RANGES_STA:
				case CFG_PORT_STAT:
				case CFG_CUR_SSID:
				case CFG_CUR_BSSID:
				case CFG_COMMS_QUALITY:
				case CFG_CUR_TX_RATE:
				case CFG_CUR_BEACON_INTERVAL:
				case CFG_CUR_SCALE_THRH:
				case CFG_PROTOCOL_RSP_TIME:
				case CFG_CUR_SHORT_RETRY_LIMIT:
				case CFG_CUR_LONG_RETRY_LIMIT:
				case CFG_MAX_TX_LIFETIME:
				case CFG_MAX_RX_LIFETIME:
				case CFG_CF_POLLABLE:
				case CFG_AUTHENTICATION_ALGORITHMS:
				case CFG_PRIVACY_OPT_IMPLEMENTED:
				
				
				
				
				
				
				
				
				
				case CFG_NIC_MAC_ADDR:
				case CFG_PCF_INFO:
				
				case CFG_PHY_TYPE:
				case CFG_CUR_CHANNEL:
				
				
				case CFG_SUPPORTED_DATA_RATES:
					break;
				case CFG_AP_MODE:
					DBG_ERROR( DbgInfo, "set CFG_AP_MODE no longer supported\n" );
					break;
				case CFG_ENCRYPT_STRING:
					
					memset( lp->szEncryption, 0, sizeof( lp->szEncryption ));
					memcpy( (void *)lp->szEncryption,  (void *)&pLtv->u.u8[0],
							( pLtv->len * sizeof( hcf_16 )) );
					wl_wep_decode( CRYPT_CODE, &sEncryption,
								    lp->szEncryption );

					lp->TransmitKeyID    = sEncryption.wTxKeyID + 1;
					lp->EnableEncryption = sEncryption.wEnabled;

					memcpy( &lp->DefaultKeys, &sEncryption.EncStr,
							sizeof( CFG_DEFAULT_KEYS_STRCT ));
					break;

				case CFG_DRIVER_ENABLE:
					lp->driverEnable    = pLtv->u.u16[0];
					pLtv->u.u16[0]      = CNV_INT_TO_LITTLE( pLtv->u.u16[0] );
					break;
				case CFG_WOLAS_ENABLE:
					lp->wolasEnable = pLtv->u.u16[0];
					pLtv->u.u16[0]  = CNV_INT_TO_LITTLE( pLtv->u.u16[0] );
					break;
				case CFG_SET_WPA_AUTH_KEY_MGMT_SUITE:
					lp->AuthKeyMgmtSuite = pLtv->u.u16[0];
					pLtv->u.u16[0]  = CNV_INT_TO_LITTLE( pLtv->u.u16[0] );
					break;
				case CFG_DISASSOCIATE_ADDR:
					pLtv->u.u16[ETH_ALEN / 2] = CNV_INT_TO_LITTLE( pLtv->u.u16[ETH_ALEN / 2] );
					break;
				case CFG_ADD_TKIP_DEFAULT_KEY:
				case CFG_REMOVE_TKIP_DEFAULT_KEY:
					
					pLtv->u.u16[0] = CNV_INT_TO_LITTLE( pLtv->u.u16[0] );
					break;
				case CFG_ADD_TKIP_MAPPED_KEY:
					break;
				case CFG_REMOVE_TKIP_MAPPED_KEY:
					break;
				
				case CFG_MB_INFO:
				case CFG_IFB:
				default:
					break;
				}

				switch( pLtv->typ ) {
				case CFG_CNF_PORT_TYPE:
				case CFG_CNF_OWN_MAC_ADDR:
				case CFG_CNF_OWN_CHANNEL:
				case CFG_CNF_OWN_SSID:
				case CFG_CNF_OWN_ATIM_WINDOW:
				case CFG_CNF_SYSTEM_SCALE:
				case CFG_CNF_MAX_DATA_LEN:
				case CFG_CNF_PM_ENABLED:
				case CFG_CNF_MCAST_RX:
				case CFG_CNF_MAX_SLEEP_DURATION:
				case CFG_CNF_HOLDOVER_DURATION:
				case CFG_CNF_OWN_NAME:
				case CFG_CNF_LOAD_BALANCING:
				case CFG_CNF_MEDIUM_DISTRIBUTION:
#ifdef WARP
				case CFG_CNF_TX_POW_LVL:
				case CFG_CNF_CONNECTION_CNTL:
				
#endif 
#if 1 
		
				case CFG_CNF_OWN_DTIM_PERIOD:
#ifdef WARP
				case CFG_CNF_OWN_BEACON_INTERVAL:                    
#endif 
#ifdef USE_WDS
				case CFG_CNF_WDS_ADDR1:
				case CFG_CNF_WDS_ADDR2:
				case CFG_CNF_WDS_ADDR3:
				case CFG_CNF_WDS_ADDR4:
				case CFG_CNF_WDS_ADDR5:
				case CFG_CNF_WDS_ADDR6:
#endif
				case CFG_CNF_MCAST_PM_BUF:
				case CFG_CNF_REJECT_ANY:
#endif

				case CFG_CNF_ENCRYPTION:
				case CFG_CNF_AUTHENTICATION:
#if 1 
		

				case CFG_CNF_EXCL_UNENCRYPTED:
				case CFG_CNF_MCAST_RATE:
				case CFG_CNF_INTRA_BSS_RELAY:
#endif

				case CFG_CNF_MICRO_WAVE:
				
				
				
				
				
				case CFG_AP_MODE:
				case CFG_ENCRYPT_STRING:
				
				case CFG_WOLAS_ENABLE:
				case CFG_MB_INFO:
				case CFG_IFB:
					break;
				
				case CFG_DRIVER_ENABLE:
					if( lp->driverEnable ) {
						
						
						
						
						
						

						hcf_cntl( &( lp->hcfCtx ), HCF_CNTL_ENABLE | HCF_PORT_0 );
						hcf_cntl( &( lp->hcfCtx ), HCF_CNTL_CONNECT );
					} else {
						
						
						
						
						
						

						hcf_cntl( &( lp->hcfCtx ), HCF_CNTL_DISABLE | HCF_PORT_0 );
						hcf_cntl( &( lp->hcfCtx ), HCF_CNTL_DISCONNECT );
					}
					break;
				default:
    					wl_act_int_off( lp );
					urq->result = hcf_put_info(&(lp->hcfCtx), (LTVP) pLtv);
    					wl_act_int_on( lp );
					break;
				}

				if( ltvAllocated ) {
					kfree( pLtv );
				}
			} else {
				urq->result = UIL_FAILURE;
			}
		} else {
			DBG_ERROR( DbgInfo, "EPERM\n" );
			urq->result = UIL_FAILURE;
			result = -EPERM;
		}
	} else {
		DBG_ERROR( DbgInfo, "UIL_ERR_WRONG_IFB\n" );
		urq->result = UIL_ERR_WRONG_IFB;
	}

	DBG_LEAVE( DbgInfo );
	return result;
} 

int wvlan_uil_get_info( struct uilreq *urq, struct wl_private *lp )
{
	int result = 0;
	int i;
	

	DBG_FUNC( "wvlan_uil_get_info" );
	DBG_ENTER( DbgInfo );

	if( urq->hcfCtx == &( lp->hcfCtx )) {
		if(( urq->data != NULL ) && ( urq->len != 0 )) {
			ltv_t      *pLtv;
			bool_t      ltvAllocated = FALSE;

			
			if( urq->len < ( sizeof( hcf_16 ) * 2 )) {
				urq->len = sizeof( lp->ltvRecord );
				DBG_ERROR( DbgInfo, "No Length/Type in LTV!!!\n" );
				DBG_ERROR( DbgInfo, "UIL_ERR_LEN\n" );
				urq->result = UIL_ERR_LEN;
				DBG_LEAVE( DbgInfo );
				return result;
			}

			
			result = verify_area( VERIFY_READ, urq->data, sizeof( hcf_16 ) * 2 );
			if( result != 0 ) {
				DBG_ERROR( DbgInfo, "verify_area(), VERIFY_READ FAILED\n" );
				urq->result = UIL_FAILURE;
				DBG_LEAVE( DbgInfo );
				return result;
			}

			
			result = copy_from_user( &( lp->ltvRecord ), urq->data, sizeof( hcf_16 ) * 2 );

			if((( lp->ltvRecord.len + 1 ) * sizeof( hcf_16 )) > urq->len ) {
				DBG_ERROR( DbgInfo, "Incoming LTV too big\n" );
				urq->len = sizeof( lp->ltvRecord );
				urq->result = UIL_ERR_LEN;
				DBG_LEAVE( DbgInfo );
				return result;
			}

			
			switch ( lp->ltvRecord.typ ) {
			case CFG_NIC_IDENTITY:
				memcpy( &lp->ltvRecord.u.u8[0], &lp->NICIdentity, sizeof( lp->NICIdentity ));
				break;
			case CFG_PRI_IDENTITY:
				memcpy( &lp->ltvRecord.u.u8[0], &lp->PrimaryIdentity, sizeof( lp->PrimaryIdentity ));
				break;
			case CFG_AP_MODE:
				DBG_ERROR( DbgInfo, "set CFG_AP_MODE no longer supported, so is get useful ????\n" );
				lp->ltvRecord.u.u16[0] =
					CNV_INT_TO_LITTLE( lp->hcfCtx.IFB_FWIdentity.comp_id ) == COMP_ID_FW_AP;
				break;
			
			case CFG_ENCRYPT_STRING:
			case CFG_COUNTRY_STRING:
			case CFG_DRIVER_ENABLE:
			case CFG_WOLAS_ENABLE:
				
				urq->result = UIL_FAILURE;
				break;
			case CFG_DRV_INFO:
				DBG_TRACE( DbgInfo, "Intercept CFG_DRV_INFO\n" );
				result = cfg_driver_info( urq, lp );
				break;
			case CFG_DRV_IDENTITY:
				DBG_TRACE( DbgInfo, "Intercept CFG_DRV_IDENTITY\n" );
				result = cfg_driver_identity( urq, lp );
				break;
			case CFG_IFB:
				
				if( !capable( CAP_NET_ADMIN )) {
					result = -EPERM;
					break;
				}

				

			case CFG_FW_IDENTITY:   
			default:

				
				result = verify_area( VERIFY_WRITE, urq->data, urq->len );
				if( result != 0 ) {
					DBG_ERROR( DbgInfo, "verify_area(), VERIFY_WRITE FAILED\n" );
					urq->result = UIL_FAILURE;
					break;
				}

				if( urq->len > sizeof( lp->ltvRecord )) {
					pLtv = kmalloc(urq->len, GFP_KERNEL);
					if (pLtv != NULL) {
						ltvAllocated = TRUE;

						
						memcpy( pLtv, &( lp->ltvRecord ), sizeof( hcf_16 ) * 2 );
					} else {
						urq->len = sizeof( lp->ltvRecord );
						urq->result = UIL_ERR_LEN;
						DBG_ERROR( DbgInfo, "kmalloc FAILED\n" );
						DBG_ERROR( DbgInfo, "UIL_ERR_LEN\n" );
						result = -ENOMEM;
						break;
					}
				} else {
					pLtv = &( lp->ltvRecord );
				}

    				wl_act_int_off( lp );
				urq->result = hcf_get_info( &( lp->hcfCtx ), (LTVP) pLtv );
    				wl_act_int_on( lp );

				
				

				
				
				
				

				
				break;
			}

			
			switch( lp->ltvRecord.typ ) {
			
			case CFG_CNF_PORT_TYPE:
			case CFG_CNF_OWN_CHANNEL:
			case CFG_CNF_OWN_ATIM_WINDOW:
			case CFG_CNF_SYSTEM_SCALE:
			case CFG_CNF_MAX_DATA_LEN:
			case CFG_CNF_PM_ENABLED:
			case CFG_CNF_MCAST_RX:
			case CFG_CNF_MAX_SLEEP_DURATION:
			case CFG_CNF_HOLDOVER_DURATION:
			case CFG_CNF_OWN_DTIM_PERIOD:
			case CFG_CNF_MCAST_PM_BUF:
			case CFG_CNF_REJECT_ANY:
			case CFG_CNF_ENCRYPTION:
			case CFG_CNF_AUTHENTICATION:
			case CFG_CNF_EXCL_UNENCRYPTED:
			case CFG_CNF_INTRA_BSS_RELAY:
			case CFG_CNF_MICRO_WAVE:
			case CFG_CNF_LOAD_BALANCING:
			case CFG_CNF_MEDIUM_DISTRIBUTION:
#ifdef WARP
			case CFG_CNF_TX_POW_LVL:
			case CFG_CNF_CONNECTION_CNTL:
			case CFG_CNF_OWN_BEACON_INTERVAL:                          
			case CFG_COEXISTENSE_BEHAVIOUR:                            
			
#endif 
			case CFG_CREATE_IBSS:
			case CFG_RTS_THRH:
			case CFG_PROMISCUOUS_MODE:
			
			case CFG_RTS_THRH0:
			case CFG_RTS_THRH1:
			case CFG_RTS_THRH2:
			case CFG_RTS_THRH3:
			case CFG_RTS_THRH4:
			case CFG_RTS_THRH5:
			case CFG_RTS_THRH6:
			case CFG_TX_RATE_CNTL0:
			case CFG_TX_RATE_CNTL1:
			case CFG_TX_RATE_CNTL2:
			case CFG_TX_RATE_CNTL3:
			case CFG_TX_RATE_CNTL4:
			case CFG_TX_RATE_CNTL5:
			case CFG_TX_RATE_CNTL6:
			case CFG_TX_KEY_ID:
			case CFG_TICK_TIME:
			case CFG_MAX_LOAD_TIME:
			case CFG_NIC_TEMP_TYPE:
			case CFG_PORT_STAT:
			case CFG_CUR_TX_RATE:
			case CFG_CUR_BEACON_INTERVAL:
			case CFG_PROTOCOL_RSP_TIME:
			case CFG_CUR_SHORT_RETRY_LIMIT:
			case CFG_CUR_LONG_RETRY_LIMIT:
			case CFG_MAX_TX_LIFETIME:
			case CFG_MAX_RX_LIFETIME:
			case CFG_CF_POLLABLE:
			case CFG_PRIVACY_OPT_IMPLEMENTED:
			
			
			
			
			
			
			
			
			
			case CFG_PHY_TYPE:
			case CFG_CUR_CHANNEL:
			
			
			
			
			
			case CFG_CNF_OWN_SSID:
			case CFG_CNF_OWN_NAME:
			
			case CFG_DESIRED_SSID:
			case CFG_SCAN_SSID:
			case CFG_CUR_SSID:
				lp->ltvRecord.u.u16[0] = CNV_INT_TO_LITTLE( lp->ltvRecord.u.u16[0] );
				break;
			
			case CFG_CNF_OWN_MAC_ADDR:
			
			case CFG_CNF_WDS_ADDR1:
			case CFG_CNF_WDS_ADDR2:
			case CFG_CNF_WDS_ADDR3:
			case CFG_CNF_WDS_ADDR4:
			case CFG_CNF_WDS_ADDR5:
			case CFG_CNF_WDS_ADDR6:
			case CFG_GROUP_ADDR:
			case CFG_NIC_SERIAL_NUMBER:
			case CFG_CUR_BSSID:
			case CFG_NIC_MAC_ADDR:
			case CFG_SUPPORTED_DATA_RATES:  
				break;
			
			

			case CFG_DEFAULT_KEYS:
				{
					CFG_DEFAULT_KEYS_STRCT *pKeys = (CFG_DEFAULT_KEYS_STRCT *)&lp->ltvRecord.u.u8[0];

					pKeys[0].len = CNV_INT_TO_LITTLE( pKeys[0].len );
					pKeys[1].len = CNV_INT_TO_LITTLE( pKeys[1].len );
					pKeys[2].len = CNV_INT_TO_LITTLE( pKeys[2].len );
					pKeys[3].len = CNV_INT_TO_LITTLE( pKeys[3].len );
				}
				break;
			case CFG_CNF_MCAST_RATE:
			case CFG_TX_RATE_CNTL:
			case CFG_SUPPORTED_RATE_SET_CNTL:    
			case CFG_BASIC_RATE_SET_CNTL:    
				lp->ltvRecord.u.u16[0] = CNV_INT_TO_LITTLE( lp->ltvRecord.u.u16[0] );
				lp->ltvRecord.u.u16[1] = CNV_INT_TO_LITTLE( lp->ltvRecord.u.u16[1] );
				break;
			case CFG_DL_BUF:
			case CFG_NIC_IDENTITY:
			case CFG_COMMS_QUALITY:
			case CFG_PCF_INFO:
				lp->ltvRecord.u.u16[0] = CNV_INT_TO_LITTLE( lp->ltvRecord.u.u16[0] );
				lp->ltvRecord.u.u16[1] = CNV_INT_TO_LITTLE( lp->ltvRecord.u.u16[1] );
				lp->ltvRecord.u.u16[2] = CNV_INT_TO_LITTLE( lp->ltvRecord.u.u16[2] );
				break;
			case CFG_FW_IDENTITY:
				lp->ltvRecord.u.u16[0] = CNV_INT_TO_LITTLE( lp->ltvRecord.u.u16[0] );
				lp->ltvRecord.u.u16[1] = CNV_INT_TO_LITTLE( lp->ltvRecord.u.u16[1] );
				lp->ltvRecord.u.u16[2] = CNV_INT_TO_LITTLE( lp->ltvRecord.u.u16[2] );
				lp->ltvRecord.u.u16[3] = CNV_INT_TO_LITTLE( lp->ltvRecord.u.u16[3] );
				break;
			
			case CFG_NIC_MFI_SUP_RANGE:
			case CFG_NIC_CFI_SUP_RANGE:
			case CFG_NIC_PROFILE:
			case CFG_FW_SUP_RANGE:
				lp->ltvRecord.u.u16[0] = CNV_INT_TO_LITTLE( lp->ltvRecord.u.u16[0] );
				lp->ltvRecord.u.u16[1] = CNV_INT_TO_LITTLE( lp->ltvRecord.u.u16[1] );
				lp->ltvRecord.u.u16[2] = CNV_INT_TO_LITTLE( lp->ltvRecord.u.u16[2] );
				lp->ltvRecord.u.u16[3] = CNV_INT_TO_LITTLE( lp->ltvRecord.u.u16[3] );
				lp->ltvRecord.u.u16[4] = CNV_INT_TO_LITTLE( lp->ltvRecord.u.u16[4] );
				break;
			case CFG_MFI_ACT_RANGES_STA:
			case CFG_CFI_ACT_RANGES_STA:
			case CFG_CUR_SCALE_THRH:
			case CFG_AUTHENTICATION_ALGORITHMS:
				for( i = 0; i < ( lp->ltvRecord.len - 1 ); i++ ) {
					lp->ltvRecord.u.u16[i] = CNV_INT_TO_LITTLE( lp->ltvRecord.u.u16[i] );
				}
				break;
			
			case CFG_PRI_IDENTITY:
				break;
			case CFG_MB_INFO:
				
				break;
			
			case CFG_IFB:
			case CFG_DRV_INFO:
			case CFG_AP_MODE:
			case CFG_ENCRYPT_STRING:
			case CFG_COUNTRY_STRING:
			case CFG_DRIVER_ENABLE:
			case CFG_WOLAS_ENABLE:
			default:
				break;
			}

			
			copy_to_user( urq->data, &( lp->ltvRecord ), urq->len );

			if( ltvAllocated ) {
				kfree( &( lp->ltvRecord ));
			}

			urq->result = UIL_SUCCESS;
		} else {
			urq->result = UIL_FAILURE;
		}
	} else {
		DBG_ERROR( DbgInfo, "UIL_ERR_WRONG_IFB\n" );
		urq->result = UIL_ERR_WRONG_IFB;
	}

	DBG_LEAVE( DbgInfo );
	return result;
} 





int cfg_driver_info( struct uilreq *urq, struct wl_private *lp )
{
	int result = 0;
	


	DBG_FUNC( "cfg_driver_info" );
	DBG_ENTER( DbgInfo );


	
	if( urq->len < sizeof( lp->driverInfo )) {
		urq->len = sizeof( lp->driverInfo );
		urq->result = UIL_ERR_LEN;
		DBG_LEAVE( DbgInfo );
		return result;
	}

	
	result = verify_area( VERIFY_WRITE, urq->data, sizeof( lp->driverInfo ));
	if( result != 0 ) {
		urq->result = UIL_FAILURE;
		DBG_LEAVE( DbgInfo );
		return result;
	}

	lp->driverInfo.card_stat = lp->hcfCtx.IFB_CardStat;

	
	urq->result = UIL_SUCCESS;
	copy_to_user( urq->data, &( lp->driverInfo ), sizeof( lp->driverInfo ));

	DBG_LEAVE( DbgInfo );
	return result;
} 




int cfg_driver_identity( struct uilreq *urq, struct wl_private *lp )
{
	int result = 0;
	


	DBG_FUNC( "wvlan_driver_identity" );
	DBG_ENTER( DbgInfo );


	
	if( urq->len < sizeof( lp->driverIdentity )) {
		urq->len = sizeof( lp->driverIdentity );
		urq->result = UIL_ERR_LEN;
		DBG_LEAVE( DbgInfo );
		return result;
	}

	
	result = verify_area( VERIFY_WRITE, urq->data, sizeof( lp->driverIdentity ));
	if( result != 0 ) {
		urq->result = UIL_FAILURE;
		DBG_LEAVE( DbgInfo );
		return result;
	}

	
	urq->result = UIL_SUCCESS;
	copy_to_user( urq->data, &( lp->driverIdentity ), sizeof( lp->driverIdentity ));

	DBG_LEAVE( DbgInfo );
	return result;
} 


#endif  


#ifdef WIRELESS_EXT


int wvlan_set_netname(struct net_device *dev,
		      struct iw_request_info *info,
		      union iwreq_data *wrqu,
		      char *extra)
{
        struct wl_private *lp = wl_priv(dev);
        unsigned long flags;
	int ret = 0;
	


	DBG_FUNC( "wvlan_set_netname" );
	DBG_ENTER( DbgInfo );

        wl_lock(lp, &flags);

        memset( lp->NetworkName, 0, sizeof( lp->NetworkName ));
        memcpy( lp->NetworkName, extra, wrqu->data.length);

	
	wl_apply(lp);
        wl_unlock(lp, &flags);

	DBG_LEAVE( DbgInfo );
	return ret;
} 




int wvlan_get_netname(struct net_device *dev,
		      struct iw_request_info *info,
		      union iwreq_data *wrqu,
		      char *extra)
{
        struct wl_private *lp = wl_priv(dev);
        unsigned long flags;
        int         ret = 0;
        int         status = -1;
        wvName_t   *pName;
	


        DBG_FUNC( "wvlan_get_netname" );
        DBG_ENTER( DbgInfo );

        wl_lock(lp, &flags);

        
        lp->ltvRecord.len = 1 + ( sizeof( *pName ) / sizeof( hcf_16 ));
        lp->ltvRecord.typ = CFG_CUR_SSID;

        status = hcf_get_info( &( lp->hcfCtx ), (LTVP)&( lp->ltvRecord ));

        if( status == HCF_SUCCESS ) {
                pName = (wvName_t *)&( lp->ltvRecord.u.u32 );

		memset(extra, '\0', HCF_MAX_NAME_LEN);
		wrqu->data.length = pName->length;

                memcpy(extra, pName->name, pName->length);
        } else {
                ret = -EFAULT;
	}

        wl_unlock(lp, &flags);

        DBG_LEAVE( DbgInfo );
        return ret;
} 




int wvlan_set_station_nickname(struct net_device *dev,
		      struct iw_request_info *info,
		      union iwreq_data *wrqu,
		      char *extra)
{
        struct wl_private *lp = wl_priv(dev);
        unsigned long flags;
        int         ret = 0;
	


        DBG_FUNC( "wvlan_set_station_nickname" );
        DBG_ENTER( DbgInfo );

        wl_lock(lp, &flags);

        memset( lp->StationName, 0, sizeof( lp->StationName ));

        memcpy( lp->StationName, extra, wrqu->data.length);

        
        wl_apply( lp );
        wl_unlock(lp, &flags);

        DBG_LEAVE( DbgInfo );
        return ret;
} 




int wvlan_get_station_nickname(struct net_device *dev,
		      struct iw_request_info *info,
		      union iwreq_data *wrqu,
		      char *extra)
{
        struct wl_private *lp = wl_priv(dev);
        unsigned long flags;
	int         ret = 0;
	int         status = -1;
	wvName_t   *pName;
	


        DBG_FUNC( "wvlan_get_station_nickname" );
        DBG_ENTER( DbgInfo );

        wl_lock( lp, &flags );

        
        lp->ltvRecord.len = 1 + ( sizeof( *pName ) / sizeof( hcf_16 ));
        lp->ltvRecord.typ = CFG_CNF_OWN_NAME;

        status = hcf_get_info( &( lp->hcfCtx ), (LTVP)&( lp->ltvRecord ));

        if( status == HCF_SUCCESS ) {
                pName = (wvName_t *)&( lp->ltvRecord.u.u32 );

		memset(extra, '\0', HCF_MAX_NAME_LEN);
		wrqu->data.length = pName->length;
		memcpy(extra, pName->name, pName->length);
        } else {
                ret = -EFAULT;
        }

        wl_unlock(lp, &flags);

        DBG_LEAVE( DbgInfo );
	return ret;
} 




int wvlan_set_porttype(struct net_device *dev,
		      struct iw_request_info *info,
		      union iwreq_data *wrqu,
		      char *extra)
{
        struct wl_private *lp = wl_priv(dev);
        unsigned long flags;
        int     ret = 0;
	hcf_16  portType;
	


        DBG_FUNC( "wvlan_set_porttype" );
        DBG_ENTER( DbgInfo );

        wl_lock(lp, &flags);

        
        portType = *((__u32 *)extra);

        if( !(( portType == 1 ) || ( portType == 3 ))) {
                ret = -EINVAL;
		goto out_unlock;
        }

        lp->PortType = portType;

        
        wl_apply( lp );

out_unlock:
        wl_unlock(lp, &flags);

        DBG_LEAVE( DbgInfo );
        return ret;
}



int wvlan_get_porttype(struct net_device *dev,
		      struct iw_request_info *info,
		      union iwreq_data *wrqu,
		      char *extra)
{
        struct wl_private *lp = wl_priv(dev);
        unsigned long flags;
        int     ret = 0;
        int     status = -1;
        hcf_16  *pPortType;
        __u32 *pData = (__u32 *)extra;
	


        DBG_FUNC( "wvlan_get_porttype" );
        DBG_ENTER( DbgInfo );

        wl_lock( lp, &flags );

        
        lp->ltvRecord.len = 1 + ( sizeof( *pPortType ) / sizeof( hcf_16 ));
        lp->ltvRecord.typ = CFG_CNF_PORT_TYPE;

        status = hcf_get_info( &( lp->hcfCtx ), (LTVP)&( lp->ltvRecord ));

        if( status == HCF_SUCCESS ) {
                pPortType = (hcf_16 *)&( lp->ltvRecord.u.u32 );

                *pData = CNV_LITTLE_TO_INT( *pPortType );
        } else {
            ret = -EFAULT;
	}

        wl_unlock(lp, &flags);

        DBG_LEAVE( DbgInfo );
        return ret;
} 

#endif  




#ifdef USE_RTS
int wvlan_rts( struct rtsreq *rrq, __u32 io_base )
{
	int ioctl_ret = 0;
	


	DBG_FUNC( "wvlan_rts" );
	DBG_ENTER( DbgInfo );


	DBG_PRINT( "io_base: 0x%08x\n", io_base );

	switch( rrq->typ ) {
	  case WL_IOCTL_RTS_READ:
		DBG_TRACE(DbgInfo, "IOCTL: WVLAN2_IOCTL_RTS -- WL_IOCTL_RTS_READ\n");
		rrq->data[0] = IN_PORT_WORD( io_base + rrq->reg );
		DBG_TRACE( DbgInfo, "  reg 0x%04x ==> 0x%04x\n", rrq->reg, CNV_LITTLE_TO_SHORT( rrq->data[0] ) );
		break;
	  case WL_IOCTL_RTS_WRITE:
		DBG_TRACE(DbgInfo, "IOCTL: WVLAN2_IOCTL_RTS -- WL_IOCTL_RTS_WRITE\n");
		OUT_PORT_WORD( io_base + rrq->reg, rrq->data[0] );
		DBG_TRACE( DbgInfo, "  reg 0x%04x <== 0x%04x\n", rrq->reg, CNV_LITTLE_TO_SHORT( rrq->data[0] ) );
		break;
	  case WL_IOCTL_RTS_BATCH_READ:
		DBG_TRACE(DbgInfo, "IOCTL: WVLAN2_IOCTL_RTS -- WL_IOCTL_RTS_BATCH_READ\n");
		IN_PORT_STRING_16( io_base + rrq->reg, rrq->data, rrq->len );
		DBG_TRACE( DbgInfo, "  reg 0x%04x ==> %d bytes\n", rrq->reg, rrq->len * sizeof (__u16 ) );
		break;
	  case WL_IOCTL_RTS_BATCH_WRITE:
		DBG_TRACE(DbgInfo, "IOCTL: WVLAN2_IOCTL_RTS -- WL_IOCTL_RTS_BATCH_WRITE\n");
		OUT_PORT_STRING_16( io_base + rrq->reg, rrq->data, rrq->len );
		DBG_TRACE( DbgInfo, "  reg 0x%04x <== %d bytes\n", rrq->reg, rrq->len * sizeof (__u16) );
		break;
	default:

		DBG_TRACE(DbgInfo, "IOCTL: WVLAN2_IOCTL_RTS -- UNSUPPORTED RTS CODE: 0x%X", rrq->typ );
		ioctl_ret = -EOPNOTSUPP;
		break;
	}

	DBG_LEAVE( DbgInfo );
	return ioctl_ret;
} 

#endif  
