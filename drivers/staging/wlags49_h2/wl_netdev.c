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
 *   This file contains handler functions registered with the net_device
 *   structure.
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

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/kernel.h>

#include <linux/netdevice.h>
#include <linux/ethtool.h>
#include <linux/etherdevice.h>

#include <debug.h>

#include <hcf.h>
#include <dhf.h>

#include <wl_if.h>
#include <wl_internal.h>
#include <wl_util.h>
#include <wl_priv.h>
#include <wl_main.h>
#include <wl_netdev.h>
#include <wl_wext.h>

#ifdef USE_PROFILE
#include <wl_profile.h>
#endif  

#ifdef BUS_PCMCIA
#include <wl_cs.h>
#endif  

#ifdef BUS_PCI
#include <wl_pci.h>
#endif  


#if DBG
extern dbg_info_t *DbgInfo;
#endif  


#if HCF_ENCAP
#define MTU_MAX (HCF_MAX_MSG - ETH_HLEN - 8)
#else
#define MTU_MAX (HCF_MAX_MSG - ETH_HLEN)
#endif


#define BLOCK_INPUT(buf, len) \
    desc->buf_addr = buf; \
    desc->BUF_SIZE = len; \
    status = hcf_rcv_msg(&(lp->hcfCtx), desc, 0)

#define BLOCK_INPUT_DMA(buf, len) memcpy( buf, desc_next->buf_addr, pktlen )


int wl_init( struct net_device *dev )
{
    

    DBG_FUNC( "wl_init" );
    DBG_ENTER( DbgInfo );

    DBG_PARAM( DbgInfo, "dev", "%s (0x%p)", dev->name, dev );


    DBG_LEAVE( DbgInfo );
    return 0;
} 

int wl_config( struct net_device *dev, struct ifmap *map )
{
    DBG_FUNC( "wl_config" );
    DBG_ENTER( DbgInfo );

    DBG_PARAM( DbgInfo, "dev", "%s (0x%p)", dev->name, dev );
    DBG_PARAM( DbgInfo, "map", "0x%p", map );

    DBG_TRACE(DbgInfo, "%s: %s called.\n", dev->name, __func__);

    DBG_LEAVE( DbgInfo );
    return 0;
} 

struct net_device_stats *wl_stats( struct net_device *dev )
{
#ifdef USE_WDS
    int                         count;
#endif  
    unsigned long               flags;
    struct net_device_stats     *pStats;
    struct wl_private           *lp = wl_priv(dev);
    

    
    
    

    pStats = NULL;

    wl_lock( lp, &flags );

#ifdef USE_RTS
    if( lp->useRTS == 1 ) {
	wl_unlock( lp, &flags );

	
	return NULL;
    }
#endif  

    
#ifdef USE_WDS

    for( count = 0; count < NUM_WDS_PORTS; count++ ) {
	if( dev == lp->wds_port[count].dev ) {
	    pStats = &( lp->wds_port[count].stats );
	}
    }

#endif  

    
    if( pStats == NULL ) {
        pStats = &( lp->stats );
    }

    wl_unlock( lp, &flags );

    

    return pStats;
} 

int wl_open(struct net_device *dev)
{
    int                 status = HCF_SUCCESS;
    struct wl_private   *lp = wl_priv(dev);
    unsigned long       flags;
    

    DBG_FUNC( "wl_open" );
    DBG_ENTER( DbgInfo );

    wl_lock( lp, &flags );

#ifdef USE_RTS
    if( lp->useRTS == 1 ) {
	DBG_TRACE( DbgInfo, "Skipping device open, in RTS mode\n" );
	wl_unlock( lp, &flags );
	DBG_LEAVE( DbgInfo );
	return -EIO;
    }
#endif  

#ifdef USE_PROFILE
    parse_config( dev );
#endif

    if( lp->portState == WVLAN_PORT_STATE_DISABLED ) {
	DBG_TRACE( DbgInfo, "Enabling Port 0\n" );
	status = wl_enable( lp );

        if( status != HCF_SUCCESS ) {
            DBG_TRACE( DbgInfo, "Enable port 0 failed: 0x%x\n", status );
        }
    }

    
    wl_unlock(lp, &flags);
    wl_lock( lp, &flags );

    if ( strlen( lp->fw_image_filename ) ) {
	DBG_TRACE( DbgInfo, ";???? Kludgy way to force a download\n" );
	status = wl_go( lp );
    } else {
	status = wl_apply( lp );
    }

    
    wl_unlock(lp, &flags);
    wl_lock( lp, &flags );

    if( status != HCF_SUCCESS ) {
	
	status = wl_reset( dev );
    }

    
    wl_unlock(lp, &flags);
    wl_lock( lp, &flags );

    if( status == HCF_SUCCESS ) {
	netif_carrier_on( dev );
	WL_WDS_NETIF_CARRIER_ON( lp );

	lp->is_handling_int = WL_HANDLING_INT; 
        wl_act_int_on( lp );

	netif_start_queue( dev );
	WL_WDS_NETIF_START_QUEUE( lp );
    } else {
        wl_hcf_error( dev, status );		
        netif_device_detach( dev );		
    }

    wl_unlock( lp, &flags );

    DBG_LEAVE( DbgInfo );
    return status;
} 

int wl_close( struct net_device *dev )
{
    struct wl_private   *lp = wl_priv(dev);
    unsigned long   flags;
    

    DBG_FUNC("wl_close");
    DBG_ENTER(DbgInfo);
    DBG_PARAM(DbgInfo, "dev", "%s (0x%p)", dev->name, dev);

    
    netif_stop_queue( dev );
    WL_WDS_NETIF_STOP_QUEUE( lp );

    netif_carrier_off( dev );
    WL_WDS_NETIF_CARRIER_OFF( lp );


    wl_lock( lp, &flags );

    wl_act_int_off( lp );
    lp->is_handling_int = WL_NOT_HANDLING_INT; 

#ifdef USE_RTS
    if( lp->useRTS == 1 ) {
	DBG_TRACE( DbgInfo, "Skipping device close, in RTS mode\n" );
	wl_unlock( lp, &flags );
	DBG_LEAVE( DbgInfo );
	return -EIO;
    }
#endif  

    
    wl_disable( lp );

    wl_unlock( lp, &flags );

    DBG_LEAVE( DbgInfo );
    return 0;
} 

static void wl_get_drvinfo(struct net_device *dev, struct ethtool_drvinfo *info)
{
    strncpy(info->driver, DRIVER_NAME, sizeof(info->driver) - 1);
    strncpy(info->version, DRV_VERSION_STR, sizeof(info->version) - 1);

    if (dev->dev.parent) {
    	dev_set_name(dev->dev.parent, "%s", info->bus_info);
	
	
    } else {
	snprintf(info->bus_info, sizeof(info->bus_info) - 1,
		"PCMCIA FIXME");
    }
} 

static struct ethtool_ops wl_ethtool_ops = {
    .get_drvinfo = wl_get_drvinfo,
    .get_link = ethtool_op_get_link,
};


int wl_ioctl( struct net_device *dev, struct ifreq *rq, int cmd )
{
    struct wl_private  *lp = wl_priv(dev);
    unsigned long           flags;
    int                     ret = 0;
    

    DBG_FUNC( "wl_ioctl" );
    DBG_ENTER(DbgInfo);
    DBG_PARAM(DbgInfo, "dev", "%s (0x%p)", dev->name, dev);
    DBG_PARAM(DbgInfo, "rq", "0x%p", rq);
    DBG_PARAM(DbgInfo, "cmd", "0x%04x", cmd);

    wl_lock( lp, &flags );

    wl_act_int_off( lp );

#ifdef USE_RTS
    if( lp->useRTS == 1 ) {
	
	if( cmd == WL_IOCTL_RTS ) {
	    DBG_TRACE( DbgInfo, "IOCTL: WL_IOCTL_RTS\n" );
	    ret = wvlan_rts( (struct rtsreq *)rq, dev->base_addr );
	} else {
	    DBG_TRACE( DbgInfo, "IOCTL not supported in RTS mode: 0x%X\n", cmd );
	    ret = -EOPNOTSUPP;
	}

	goto out_act_int_on_unlock;
    }
#endif  

    
    if( !(( lp->flags & WVLAN2_UIL_BUSY ) && ( cmd != WVLAN2_IOCTL_UIL ))) {
#ifdef USE_UIL
	struct uilreq  *urq = (struct uilreq *)rq;
#endif 

	switch( cmd ) {
		
#ifdef USE_UIL
	case WVLAN2_IOCTL_UIL:
	     DBG_TRACE( DbgInfo, "IOCTL: WVLAN2_IOCTL_UIL\n" );
	     ret = wvlan_uil( urq, lp );
	     break;
#endif  

	default:
	     DBG_TRACE(DbgInfo, "IOCTL CODE NOT SUPPORTED: 0x%X\n", cmd );
	     ret = -EOPNOTSUPP;
	     break;
	}
    } else {
	DBG_WARNING( DbgInfo, "DEVICE IS BUSY, CANNOT PROCESS REQUEST\n" );
	ret = -EBUSY;
    }

#ifdef USE_RTS
out_act_int_on_unlock:
#endif  
    wl_act_int_on( lp );

    wl_unlock( lp, &flags );

    DBG_LEAVE( DbgInfo );
    return ret;
} 

#ifdef CONFIG_NET_POLL_CONTROLLER
void wl_poll(struct net_device *dev)
{
    struct wl_private *lp = wl_priv(dev);
    unsigned long flags;
    struct pt_regs regs;

    wl_lock( lp, &flags );
    wl_isr(dev->irq, dev, &regs);
    wl_unlock( lp, &flags );
}
#endif

void wl_tx_timeout( struct net_device *dev )
{
#ifdef USE_WDS
    int                     count;
#endif  
    unsigned long           flags;
    struct wl_private       *lp = wl_priv(dev);
    struct net_device_stats *pStats = NULL;
    

    DBG_FUNC( "wl_tx_timeout" );
    DBG_ENTER( DbgInfo );

    DBG_WARNING( DbgInfo, "%s: Transmit timeout.\n", dev->name );

    wl_lock( lp, &flags );

#ifdef USE_RTS
    if( lp->useRTS == 1 ) {
	DBG_TRACE( DbgInfo, "Skipping tx_timeout handler, in RTS mode\n" );
	wl_unlock( lp, &flags );

	DBG_LEAVE( DbgInfo );
	return;
    }
#endif  

#ifdef USE_WDS

    for( count = 0; count < NUM_WDS_PORTS; count++ ) {
	if( dev == lp->wds_port[count].dev ) {
	    pStats = &( lp->wds_port[count].stats );

	    break;
	}
    }

#endif  

    
    if( pStats == NULL ) {
	pStats = &( lp->stats );
    }

    
    pStats->tx_errors++;

    wl_unlock( lp, &flags );

    DBG_LEAVE( DbgInfo );
    return;
} 

int wl_send( struct wl_private *lp )
{

    int                 status;
    DESC_STRCT          *desc;
    WVLAN_LFRAME        *txF = NULL;
    struct list_head    *element;
    int                 len;
    

    DBG_FUNC( "wl_send" );

    if( lp == NULL ) {
        DBG_ERROR( DbgInfo, "Private adapter struct is NULL\n" );
        return FALSE;
    }
    if( lp->dev == NULL ) {
        DBG_ERROR( DbgInfo, "net_device struct in wl_private is NULL\n" );
        return FALSE;
    }

    if( lp->hcfCtx.IFB_RscInd == 0 ) {
        return FALSE;
    }

    
    if( !list_empty( &( lp->txQ[0] ))) {
        element = lp->txQ[0].next;

        txF = (WVLAN_LFRAME * )list_entry( element, WVLAN_LFRAME, node );
        if( txF != NULL ) {
            lp->txF.skb  = txF->frame.skb;
            lp->txF.port = txF->frame.port;

            txF->frame.skb  = NULL;
            txF->frame.port = 0;

            list_del( &( txF->node ));
            list_add( element, &( lp->txFree ));

            lp->txQ_count--;

            if( lp->txQ_count < TX_Q_LOW_WATER_MARK ) {
                if( lp->netif_queue_on == FALSE ) {
                    DBG_TX( DbgInfo, "Kickstarting Q: %d\n", lp->txQ_count );
                    netif_wake_queue( lp->dev );
                    WL_WDS_NETIF_WAKE_QUEUE( lp );
                    lp->netif_queue_on = TRUE;
                }
            }
        }
    }

    if( lp->txF.skb == NULL ) {
        return FALSE;
    }

    
    
    len = lp->txF.skb->len < ETH_ZLEN ? ETH_ZLEN : lp->txF.skb->len;

    desc                    = &( lp->desc_tx );
    desc->buf_addr          = lp->txF.skb->data;
    desc->BUF_CNT           = len;
    desc->next_desc_addr    = NULL;

    status = hcf_send_msg( &( lp->hcfCtx ), desc, lp->txF.port );

    if( status == HCF_SUCCESS ) {
        lp->dev->trans_start = jiffies;

        DBG_TX( DbgInfo, "Transmit...\n" );

        if( lp->txF.port == HCF_PORT_0 ) {
            lp->stats.tx_packets++;
            lp->stats.tx_bytes += lp->txF.skb->len;
        }

#ifdef USE_WDS
        else
        {
            lp->wds_port[(( lp->txF.port >> 8 ) - 1)].stats.tx_packets++;
            lp->wds_port[(( lp->txF.port >> 8 ) - 1)].stats.tx_bytes += lp->txF.skb->len;
        }

#endif  

        dev_kfree_skb( lp->txF.skb );

        lp->txF.skb = NULL;
        lp->txF.port = 0;
    }

    return TRUE;
} 

int wl_tx( struct sk_buff *skb, struct net_device *dev, int port )
{
    unsigned long           flags;
    struct wl_private       *lp = wl_priv(dev);
    WVLAN_LFRAME            *txF = NULL;
    struct list_head        *element;
    

    DBG_FUNC( "wl_tx" );

    
    wl_lock( lp, &flags );

    if( lp->flags & WVLAN2_UIL_BUSY ) {
        DBG_WARNING( DbgInfo, "UIL has device blocked\n" );
        
	wl_unlock( lp, &flags );
        return 1;
    }

#ifdef USE_RTS
    if( lp->useRTS == 1 ) {
        DBG_PRINT( "RTS: we're getting a Tx...\n" );
	wl_unlock( lp, &flags );
        return 1;
    }
#endif  

    if( !lp->use_dma ) {
        
        element = lp->txFree.next;
        txF = (WVLAN_LFRAME *)list_entry( element, WVLAN_LFRAME, node );
        if( txF == NULL ) {
            DBG_ERROR( DbgInfo, "Problem with list_entry\n" );
	    wl_unlock( lp, &flags );
            return 1;
        }
        
        txF->frame.skb = skb;
        txF->frame.port = port;
        
        
        list_del( &( txF->node ));
        list_add( &( txF->node ), &( lp->txQ[0] ));

        lp->txQ_count++;
        if( lp->txQ_count >= DEFAULT_NUM_TX_FRAMES ) {
            DBG_TX( DbgInfo, "Q Full: %d\n", lp->txQ_count );
            if( lp->netif_queue_on == TRUE ) {
                netif_stop_queue( lp->dev );
                WL_WDS_NETIF_STOP_QUEUE( lp );
                lp->netif_queue_on = FALSE;
            }
        }
    }
    wl_act_int_off( lp ); 

    
#ifdef ENABLE_DMA
    if( lp->use_dma ) {
        wl_send_dma( lp, skb, port );
    }
    else
#endif
    {
        wl_send( lp );
    }
    
    wl_act_int_on( lp );
    wl_unlock( lp, &flags );
    return 0;
} 

int wl_rx(struct net_device *dev)
{
    int                     port;
    struct sk_buff          *skb;
    struct wl_private       *lp = wl_priv(dev);
    int                     status;
    hcf_16                  pktlen;
    hcf_16                  hfs_stat;
    DESC_STRCT              *desc;
    

    DBG_FUNC("wl_rx")
    DBG_PARAM(DbgInfo, "dev", "%s (0x%p)", dev->name, dev);

    if(!( lp->flags & WVLAN2_UIL_BUSY )) {

#ifdef USE_RTS
        if( lp->useRTS == 1 ) {
            DBG_PRINT( "RTS: We're getting an Rx...\n" );
            return -EIO;
        }
#endif  

        
        hfs_stat = (hcf_16)(( lp->lookAheadBuf[HFS_STAT] ) |
                            ( lp->lookAheadBuf[HFS_STAT + 1] << 8 ));

        
        if(( hfs_stat & HFS_STAT_ERR ) != HCF_SUCCESS ) {
            DBG_WARNING( DbgInfo, "HFS_STAT_ERROR (0x%x) in Rx Packet\n",
                         lp->lookAheadBuf[HFS_STAT] );
            return -EIO;
        }

        
        port = ( hfs_stat >> 8 ) & 0x0007;
        DBG_RX( DbgInfo, "Rx frame for port %d\n", port );

        pktlen = lp->hcfCtx.IFB_RxLen;
        if (pktlen != 0) {
            skb = ALLOC_SKB(pktlen);
            if (skb != NULL) {
                
                switch( port ) {
#ifdef USE_WDS
                case 1:
                case 2:
                case 3:
                case 4:
                case 5:
                case 6:
                    skb->dev = lp->wds_port[port-1].dev;
                    break;
#endif  

                case 0:
                default:
                    skb->dev = dev;
                    break;
                }

                desc = &( lp->desc_rx );

                desc->next_desc_addr = NULL;


                GET_PACKET( skb->dev, skb, pktlen );

                if( status == HCF_SUCCESS ) {
                    netif_rx( skb );

                    if( port == 0 ) {
                        lp->stats.rx_packets++;
                        lp->stats.rx_bytes += pktlen;
                    }
#ifdef USE_WDS
                    else
                    {
                        lp->wds_port[port-1].stats.rx_packets++;
                        lp->wds_port[port-1].stats.rx_bytes += pktlen;
                    }
#endif  

                    dev->last_rx = jiffies;

#ifdef WIRELESS_EXT
#ifdef WIRELESS_SPY
                    if( lp->spydata.spy_number > 0 ) {
                        char *srcaddr = skb->mac.raw + MAC_ADDR_SIZE;

                        wl_spy_gather( dev, srcaddr );
                    }
#endif 
#endif 
                } else {
                    DBG_ERROR( DbgInfo, "Rx request to card FAILED\n" );

                    if( port == 0 ) {
                        lp->stats.rx_dropped++;
                    }
#ifdef USE_WDS
                    else
                    {
                        lp->wds_port[port-1].stats.rx_dropped++;
                    }
#endif  

                    dev_kfree_skb( skb );
                }
            } else {
                DBG_ERROR( DbgInfo, "Could not alloc skb\n" );

                if( port == 0 ) {
                    lp->stats.rx_dropped++;
                }
#ifdef USE_WDS
                else
                {
                    lp->wds_port[port-1].stats.rx_dropped++;
                }
#endif  
            }
        }
    }

    return 0;
} 

#ifdef NEW_MULTICAST

void wl_multicast( struct net_device *dev )
{
#if 1 

    int                 x;
    struct netdev_hw_addr *ha;
    struct wl_private   *lp = wl_priv(dev);
    unsigned long       flags;
    

    DBG_FUNC( "wl_multicast" );
    DBG_ENTER( DbgInfo );
    DBG_PARAM( DbgInfo, "dev", "%s (0x%p)", dev->name, dev );

    if( !wl_adapter_is_open( dev )) {
        DBG_LEAVE( DbgInfo );
        return;
    }

#if DBG
    if( DBG_FLAGS( DbgInfo ) & DBG_PARAM_ON ) {
        DBG_PRINT("  flags: %s%s%s\n",
            ( dev->flags & IFF_PROMISC ) ? "Promiscous " : "",
            ( dev->flags & IFF_MULTICAST ) ? "Multicast " : "",
            ( dev->flags & IFF_ALLMULTI ) ? "All-Multicast" : "" );

        DBG_PRINT( "  mc_count: %d\n", netdev_mc_count(dev));

	netdev_for_each_mc_addr(ha, dev)
	DBG_PRINT("    %pM (%d)\n", ha->addr, dev->addr_len);
    }
#endif 

    if(!( lp->flags & WVLAN2_UIL_BUSY )) {

#ifdef USE_RTS
        if( lp->useRTS == 1 ) {
            DBG_TRACE( DbgInfo, "Skipping multicast, in RTS mode\n" );

            DBG_LEAVE( DbgInfo );
            return;
        }
#endif  

        wl_lock( lp, &flags );
        wl_act_int_off( lp );

		if ( CNV_INT_TO_LITTLE( lp->hcfCtx.IFB_FWIdentity.comp_id ) == COMP_ID_FW_STA  ) {
            if( dev->flags & IFF_PROMISC ) {
                
                lp->ltvRecord.len       = 2;
                lp->ltvRecord.typ       = CFG_PROMISCUOUS_MODE;
                lp->ltvRecord.u.u16[0]  = CNV_INT_TO_LITTLE( 1 );
                DBG_PRINT( "Enabling Promiscuous mode (IFF_PROMISC)\n" );
                hcf_put_info( &( lp->hcfCtx ), (LTVP)&( lp->ltvRecord ));
            }
            else if ((netdev_mc_count(dev) > HCF_MAX_MULTICAST) ||
                    ( dev->flags & IFF_ALLMULTI )) {
                lp->ltvRecord.len       = 2;
                lp->ltvRecord.typ       = CFG_CNF_RX_ALL_GROUP_ADDR;
                lp->ltvRecord.u.u16[0]  = CNV_INT_TO_LITTLE( 0 );
                DBG_PRINT( "Enabling all multicast mode (IFF_ALLMULTI)\n" );
                hcf_put_info( &( lp->hcfCtx ), (LTVP)&( lp->ltvRecord ));
                wl_apply( lp );
            }
            else if (!netdev_mc_empty(dev)) {
                
                lp->ltvRecord.len = ( netdev_mc_count(dev) * 3 ) + 1;
                lp->ltvRecord.typ = CFG_GROUP_ADDR;

		x = 0;
		netdev_for_each_mc_addr(ha, dev)
                    memcpy(&(lp->ltvRecord.u.u8[x++ * ETH_ALEN]),
			   ha->addr, ETH_ALEN);
                DBG_PRINT( "Setting multicast list\n" );
                hcf_put_info( &( lp->hcfCtx ), (LTVP)&( lp->ltvRecord ));
            } else {
                
                lp->ltvRecord.len       = 2;
                lp->ltvRecord.typ       = CFG_PROMISCUOUS_MODE;
                lp->ltvRecord.u.u16[0]  = CNV_INT_TO_LITTLE( 0 );
                DBG_PRINT( "Disabling Promiscuous mode\n" );
                hcf_put_info( &( lp->hcfCtx ), (LTVP)&( lp->ltvRecord ));

                
                lp->ltvRecord.len = 2;
                lp->ltvRecord.typ = CFG_GROUP_ADDR;
                DBG_PRINT( "Disabling Multicast mode\n" );
                hcf_put_info( &( lp->hcfCtx ), (LTVP)&( lp->ltvRecord ));

                lp->ltvRecord.len       = 2;
                lp->ltvRecord.typ       = CFG_CNF_RX_ALL_GROUP_ADDR;
                lp->ltvRecord.u.u16[0]  = CNV_INT_TO_LITTLE( 1 );
                DBG_PRINT( "Disabling all multicast mode (IFF_ALLMULTI)\n" );
                hcf_put_info( &( lp->hcfCtx ), (LTVP)&( lp->ltvRecord ));
                wl_apply( lp );
            }
        }
        wl_act_int_on( lp );
	wl_unlock( lp, &flags );
    }
    DBG_LEAVE( DbgInfo );
#endif 
} 

#else 

void wl_multicast( struct net_device *dev, int num_addrs, void *addrs )
{
    DBG_FUNC( "wl_multicast");
    DBG_ENTER(DbgInfo);

    DBG_PARAM( DbgInfo, "dev", "%s (0x%p)", dev->name, dev );
    DBG_PARAM( DbgInfo, "num_addrs", "%d", num_addrs );
    DBG_PARAM( DbgInfo, "addrs", "0x%p", addrs );

#error Obsolete set multicast interface!

    DBG_LEAVE( DbgInfo );
} 

#endif 

static const struct net_device_ops wl_netdev_ops =
{
    .ndo_start_xmit         = &wl_tx_port0,

    .ndo_set_config         = &wl_config,
    .ndo_get_stats          = &wl_stats,
    .ndo_set_rx_mode        = &wl_multicast,

    .ndo_init               = &wl_insert,
    .ndo_open               = &wl_adapter_open,
    .ndo_stop               = &wl_adapter_close,
    .ndo_do_ioctl           = &wl_ioctl,

    .ndo_tx_timeout         = &wl_tx_timeout,

#ifdef CONFIG_NET_POLL_CONTROLLER
    .ndo_poll_controller    = wl_poll,
#endif
};

struct net_device * wl_device_alloc( void )
{
    struct net_device   *dev = NULL;
    struct wl_private   *lp = NULL;
    

    DBG_FUNC( "wl_device_alloc" );
    DBG_ENTER( DbgInfo );

    
    dev = alloc_etherdev(sizeof(struct wl_private));
    if (!dev)
        return NULL;

    lp = wl_priv(dev);


    
    if( dev->mtu > MTU_MAX )
    {
	    DBG_WARNING( DbgInfo, "%s: MTU set too high, limiting to %d.\n",
                        dev->name, MTU_MAX );
    	dev->mtu = MTU_MAX;
    }

    

    dev->wireless_handlers = (struct iw_handler_def *)&wl_iw_handler_def;
    lp->wireless_data.spy_data = &lp->spy_data;
    dev->wireless_data = &lp->wireless_data;

    dev->netdev_ops = &wl_netdev_ops;

    dev->watchdog_timeo     = TX_TIMEOUT;

    dev->ethtool_ops	    = &wl_ethtool_ops;

    netif_stop_queue( dev );

    
    WL_WDS_DEVICE_ALLOC( lp );

    DBG_LEAVE( DbgInfo );
    return dev;
} 

void wl_device_dealloc( struct net_device *dev )
{
    

    DBG_FUNC( "wl_device_dealloc" );
    DBG_ENTER( DbgInfo );

    
    WL_WDS_DEVICE_DEALLOC( lp );

    free_netdev( dev );

    DBG_LEAVE( DbgInfo );
    return;
} 

int wl_tx_port0( struct sk_buff *skb, struct net_device *dev )
{
    DBG_TX( DbgInfo, "Tx on Port 0\n" );

    return wl_tx( skb, dev, HCF_PORT_0 );
#ifdef ENABLE_DMA
    return wl_tx_dma( skb, dev, HCF_PORT_0 );
#endif
} 

#ifdef USE_WDS

int wl_tx_port1( struct sk_buff *skb, struct net_device *dev )
{
    DBG_TX( DbgInfo, "Tx on Port 1\n" );
    return wl_tx( skb, dev, HCF_PORT_1 );
} 

int wl_tx_port2( struct sk_buff *skb, struct net_device *dev )
{
    DBG_TX( DbgInfo, "Tx on Port 2\n" );
    return wl_tx( skb, dev, HCF_PORT_2 );
} 

int wl_tx_port3( struct sk_buff *skb, struct net_device *dev )
{
    DBG_TX( DbgInfo, "Tx on Port 3\n" );
    return wl_tx( skb, dev, HCF_PORT_3 );
} 

int wl_tx_port4( struct sk_buff *skb, struct net_device *dev )
{
    DBG_TX( DbgInfo, "Tx on Port 4\n" );
    return wl_tx( skb, dev, HCF_PORT_4 );
} 

int wl_tx_port5( struct sk_buff *skb, struct net_device *dev )
{
    DBG_TX( DbgInfo, "Tx on Port 5\n" );
    return wl_tx( skb, dev, HCF_PORT_5 );
} 

int wl_tx_port6( struct sk_buff *skb, struct net_device *dev )
{
    DBG_TX( DbgInfo, "Tx on Port 6\n" );
    return wl_tx( skb, dev, HCF_PORT_6 );
} 

void wl_wds_device_alloc( struct wl_private *lp )
{
    int count;
    

    DBG_FUNC( "wl_wds_device_alloc" );
    DBG_ENTER( DbgInfo );

    for( count = 0; count < NUM_WDS_PORTS; count++ ) {
        struct net_device *dev_wds = NULL;

        dev_wds = kmalloc( sizeof( struct net_device ), GFP_KERNEL );
        memset( dev_wds, 0, sizeof( struct net_device ));

        ether_setup( dev_wds );

        lp->wds_port[count].dev = dev_wds;

        lp->wds_port[count].dev->init           = &wl_init;
        lp->wds_port[count].dev->get_stats      = &wl_stats;
        lp->wds_port[count].dev->tx_timeout     = &wl_tx_timeout;
        lp->wds_port[count].dev->watchdog_timeo = TX_TIMEOUT;
        lp->wds_port[count].dev->priv           = lp;

        sprintf( lp->wds_port[count].dev->name, "wds%d", count );
    }

    
    lp->wds_port[0].dev->hard_start_xmit = &wl_tx_port1;
    lp->wds_port[1].dev->hard_start_xmit = &wl_tx_port2;
    lp->wds_port[2].dev->hard_start_xmit = &wl_tx_port3;
    lp->wds_port[3].dev->hard_start_xmit = &wl_tx_port4;
    lp->wds_port[4].dev->hard_start_xmit = &wl_tx_port5;
    lp->wds_port[5].dev->hard_start_xmit = &wl_tx_port6;

    WL_WDS_NETIF_STOP_QUEUE( lp );

    DBG_LEAVE( DbgInfo );
    return;
} 

void wl_wds_device_dealloc( struct wl_private *lp )
{
    int count;
    

    DBG_FUNC( "wl_wds_device_dealloc" );
    DBG_ENTER( DbgInfo );

    for( count = 0; count < NUM_WDS_PORTS; count++ ) {
        struct net_device *dev_wds = NULL;

        dev_wds = lp->wds_port[count].dev;

        if( dev_wds != NULL ) {
            if( dev_wds->flags & IFF_UP ) {
                dev_close( dev_wds );
                dev_wds->flags &= ~( IFF_UP | IFF_RUNNING );
            }

            free_netdev(dev_wds);
            lp->wds_port[count].dev = NULL;
        }
    }

    DBG_LEAVE( DbgInfo );
    return;
} 

void wl_wds_netif_start_queue( struct wl_private *lp )
{
    int count;
    

    if( lp != NULL ) {
        for( count = 0; count < NUM_WDS_PORTS; count++ ) {
            if( lp->wds_port[count].is_registered &&
                lp->wds_port[count].netif_queue_on == FALSE ) {
                netif_start_queue( lp->wds_port[count].dev );
                lp->wds_port[count].netif_queue_on = TRUE;
            }
        }
    }

    return;
} 

void wl_wds_netif_stop_queue( struct wl_private *lp )
{
    int count;
    

    if( lp != NULL ) {
        for( count = 0; count < NUM_WDS_PORTS; count++ ) {
            if( lp->wds_port[count].is_registered &&
                lp->wds_port[count].netif_queue_on == TRUE ) {
                netif_stop_queue( lp->wds_port[count].dev );
                lp->wds_port[count].netif_queue_on = FALSE;
            }
        }
    }

    return;
} 

void wl_wds_netif_wake_queue( struct wl_private *lp )
{
    int count;
    

    if( lp != NULL ) {
        for( count = 0; count < NUM_WDS_PORTS; count++ ) {
            if( lp->wds_port[count].is_registered &&
                lp->wds_port[count].netif_queue_on == FALSE ) {
                netif_wake_queue( lp->wds_port[count].dev );
                lp->wds_port[count].netif_queue_on = TRUE;
            }
        }
    }

    return;
} 

void wl_wds_netif_carrier_on( struct wl_private *lp )
{
    int count;
    

    if( lp != NULL ) {
        for( count = 0; count < NUM_WDS_PORTS; count++ ) {
            if( lp->wds_port[count].is_registered ) {
                netif_carrier_on( lp->wds_port[count].dev );
            }
        }
    }

    return;
} 

void wl_wds_netif_carrier_off( struct wl_private *lp )
{
    int count;
    

    if( lp != NULL ) {
        for( count = 0; count < NUM_WDS_PORTS; count++ ) {
            if( lp->wds_port[count].is_registered ) {
                netif_carrier_off( lp->wds_port[count].dev );
            }
        }
    }

    return;
} 

#endif  

#ifdef ENABLE_DMA
int wl_send_dma( struct wl_private *lp, struct sk_buff *skb, int port )
{
    int         len;
    DESC_STRCT *desc = NULL;
    DESC_STRCT *desc_next = NULL;
    

    DBG_FUNC( "wl_send_dma" );

    if( lp == NULL )
    {
        DBG_ERROR( DbgInfo, "Private adapter struct is NULL\n" );
        return FALSE;
    }

    if( lp->dev == NULL )
    {
        DBG_ERROR( DbgInfo, "net_device struct in wl_private is NULL\n" );
        return FALSE;
    }

    

    if( skb == NULL )
    {
        DBG_WARNING (DbgInfo, "Nothing to send.\n");
        return FALSE;
    }

    len = skb->len;

    
    desc = wl_pci_dma_get_tx_packet( lp );

    if( desc == NULL )
    {
        if( lp->netif_queue_on == TRUE ) {
            netif_stop_queue( lp->dev );
            WL_WDS_NETIF_STOP_QUEUE( lp );
            lp->netif_queue_on = FALSE;

            dev_kfree_skb( skb );
            return 0;
        }
    }

    SET_BUF_CNT( desc, HFS_ADDR_DEST );
    SET_BUF_SIZE( desc, HCF_DMA_TX_BUF1_SIZE );

    desc_next = desc->next_desc_addr;

    if( desc_next->buf_addr == NULL )
    {
        DBG_ERROR( DbgInfo, "DMA descriptor buf_addr is NULL\n" );
        return FALSE;
    }

    
    memcpy( desc_next->buf_addr, skb->data, len );

    SET_BUF_CNT( desc_next, len );
    SET_BUF_SIZE( desc_next, HCF_MAX_PACKET_SIZE );

    hcf_dma_tx_put( &( lp->hcfCtx ), desc, 0 );

    dev_kfree_skb( skb );

    return TRUE;
} 

int wl_rx_dma( struct net_device *dev )
{
    int                      port;
    hcf_16                   pktlen;
    hcf_16                   hfs_stat;
    struct sk_buff          *skb;
    struct wl_private       *lp = NULL;
    DESC_STRCT              *desc, *desc_next;
    
    

    DBG_FUNC("wl_rx")
    DBG_PARAM(DbgInfo, "dev", "%s (0x%p)", dev->name, dev);

    if((( lp = dev->priv ) != NULL ) &&
	!( lp->flags & WVLAN2_UIL_BUSY )) {

#ifdef USE_RTS
        if( lp->useRTS == 1 ) {
            DBG_PRINT( "RTS: We're getting an Rx...\n" );
            return -EIO;
        }
#endif  

        
        
            desc = hcf_dma_rx_get( &( lp->hcfCtx ));

            if( desc != NULL )
            {
                

                desc_next = desc->next_desc_addr;

                
                if( GET_BUF_CNT( desc ) == 0 ) {
                    DBG_WARNING( DbgInfo, "Buffer is empty!\n" );

                    
                    hcf_dma_rx_put( &( lp->hcfCtx ), desc );
                    return -EIO;
                }

                
                hfs_stat = (hcf_16)( desc->buf_addr[HFS_STAT/2] );

                
                if(( hfs_stat & HFS_STAT_ERR ) != HCF_SUCCESS )
                {
                    DBG_WARNING( DbgInfo, "HFS_STAT_ERROR (0x%x) in Rx Packet\n",
                                desc->buf_addr[HFS_STAT/2] );

                    
                    hcf_dma_rx_put( &( lp->hcfCtx ), desc );
                    return -EIO;
                }

                
                port = ( hfs_stat >> 8 ) & 0x0007;
                DBG_RX( DbgInfo, "Rx frame for port %d\n", port );

                pktlen = GET_BUF_CNT(desc_next);
                if (pktlen != 0) {
                    skb = ALLOC_SKB(pktlen);
                    if (skb != NULL) {
                        switch( port ) {
#ifdef USE_WDS
                        case 1:
                        case 2:
                        case 3:
                        case 4:
                        case 5:
                        case 6:
                            skb->dev = lp->wds_port[port-1].dev;
                            break;
#endif  

                        case 0:
                        default:
                            skb->dev = dev;
                            break;
                        }

                        GET_PACKET_DMA( skb->dev, skb, pktlen );

                        
                        hcf_dma_rx_put( &( lp->hcfCtx ), desc );

                        netif_rx( skb );

                        if( port == 0 ) {
                            lp->stats.rx_packets++;
                            lp->stats.rx_bytes += pktlen;
                        }
#ifdef USE_WDS
                        else
                        {
                            lp->wds_port[port-1].stats.rx_packets++;
                            lp->wds_port[port-1].stats.rx_bytes += pktlen;
                        }
#endif  

                        dev->last_rx = jiffies;

                    } else {
                        DBG_ERROR( DbgInfo, "Could not alloc skb\n" );

                        if( port == 0 )
	                    {
	                        lp->stats.rx_dropped++;
	                    }
#ifdef USE_WDS
                        else
                        {
                            lp->wds_port[port-1].stats.rx_dropped++;
                        }
#endif  
                    }
                }
            }
        
    }

    return 0;
} 
#endif  
