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
 *   This file contains processing and initialization specific to PCI/miniPCI
 *   devices.
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

#include <wireless/wl_version.h>

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/pci.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/ptrace.h>
#include <linux/ctype.h>
#include <linux/string.h>
#include <linux/interrupt.h>
#include <linux/in.h>
#include <linux/delay.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/bitops.h>
#include <asm/uaccess.h>

#include <linux/ethtool.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/if_arp.h>
#include <linux/ioport.h>

#include <hcf/debug.h>

#include <hcf.h>
#include <dhf.h>
#include <hcfdef.h>

#include <wireless/wl_if.h>
#include <wireless/wl_internal.h>
#include <wireless/wl_util.h>
#include <wireless/wl_main.h>
#include <wireless/wl_netdev.h>
#include <wireless/wl_pci.h>


#if DBG
extern dbg_info_t *DbgInfo;
#endif  

static struct pci_device_id wl_pci_tbl[] __devinitdata = {
	{ PCI_DEVICE(PCI_VENDOR_ID_WL_LKM, PCI_DEVICE_ID_WL_LKM_0), },
	{ PCI_DEVICE(PCI_VENDOR_ID_WL_LKM, PCI_DEVICE_ID_WL_LKM_1), },
	{ PCI_DEVICE(PCI_VENDOR_ID_WL_LKM, PCI_DEVICE_ID_WL_LKM_2), },

	{ }			
};

MODULE_DEVICE_TABLE(pci, wl_pci_tbl);

int __devinit wl_pci_probe( struct pci_dev *pdev,
                                const struct pci_device_id *ent );
void __devexit wl_pci_remove(struct pci_dev *pdev);
int wl_pci_setup( struct pci_dev *pdev );
void wl_pci_enable_cardbus_interrupts( struct pci_dev *pdev );

#ifdef ENABLE_DMA
int wl_pci_dma_alloc( struct pci_dev *pdev, struct wl_private *lp );
int wl_pci_dma_free( struct pci_dev *pdev, struct wl_private *lp );
int wl_pci_dma_alloc_tx_packet( struct pci_dev *pdev, struct wl_private *lp,
                                DESC_STRCT **desc );
int wl_pci_dma_free_tx_packet( struct pci_dev *pdev, struct wl_private *lp,
                                DESC_STRCT **desc );
int wl_pci_dma_alloc_rx_packet( struct pci_dev *pdev, struct wl_private *lp,
                                DESC_STRCT **desc );
int wl_pci_dma_free_rx_packet( struct pci_dev *pdev, struct wl_private *lp,
                                DESC_STRCT **desc );
int wl_pci_dma_alloc_desc_and_buf( struct pci_dev *pdev, struct wl_private *lp,
                                   DESC_STRCT **desc, int size );
int wl_pci_dma_free_desc_and_buf( struct pci_dev *pdev, struct wl_private *lp,
                                   DESC_STRCT **desc );
int wl_pci_dma_alloc_desc( struct pci_dev *pdev, struct wl_private *lp,
                           DESC_STRCT **desc );
int wl_pci_dma_free_desc( struct pci_dev *pdev, struct wl_private *lp,
                           DESC_STRCT **desc );
int wl_pci_dma_alloc_buf( struct pci_dev *pdev, struct wl_private *lp,
                          DESC_STRCT *desc, int size );
int wl_pci_dma_free_buf( struct pci_dev *pdev, struct wl_private *lp,
                          DESC_STRCT *desc );

void wl_pci_dma_hcf_reclaim_rx( struct wl_private *lp );
#endif  

static struct pci_driver wl_driver =
{
	name:		MODULE_NAME,
    id_table:	wl_pci_tbl,
	probe:		wl_pci_probe,
	remove:		__devexit_p(wl_pci_remove),
    suspend:    NULL,
    resume:     NULL,
};

int wl_adapter_init_module( void )
{
    int result;
    

    DBG_FUNC( "wl_adapter_init_module()" );
    DBG_ENTER( DbgInfo );
    DBG_TRACE( DbgInfo, "wl_adapter_init_module() -- PCI\n" );

    result = pci_register_driver( &wl_driver ); 
	

    DBG_LEAVE( DbgInfo );
    return 0;
} 

void wl_adapter_cleanup_module( void )
{
	
    DBG_FUNC( "wl_adapter_cleanup_module" );
    DBG_ENTER( DbgInfo );

	
    DBG_TRACE( DbgInfo, "wl_adapter_cleanup_module() -- PCI\n" );

    pci_unregister_driver( &wl_driver );

    DBG_LEAVE( DbgInfo );
    return;
} 

int wl_adapter_insert( struct net_device *dev )
{
    int result = FALSE;
    

    DBG_FUNC( "wl_adapter_insert" );
    DBG_ENTER( DbgInfo );

    DBG_TRACE( DbgInfo, "wl_adapter_insert() -- PCI\n" );

    if( dev == NULL ) {
        DBG_ERROR( DbgInfo, "net_device pointer is NULL!!!\n" );
    } else if( dev->priv == NULL ) {
        DBG_ERROR( DbgInfo, "wl_private pointer is NULL!!!\n" );
    } else if( wl_insert( dev ) ) { 
		result = TRUE;
	} else {
        DBG_TRACE( DbgInfo, "wl_insert() FAILED\n" );
    }
    DBG_LEAVE( DbgInfo );
    return result;
} 

int wl_adapter_open( struct net_device *dev )
{
    int         result = 0;
    int         hcf_status = HCF_SUCCESS;
    

    DBG_FUNC( "wl_adapter_open" );
    DBG_ENTER( DbgInfo );

    DBG_TRACE( DbgInfo, "wl_adapter_open() -- PCI\n" );

    hcf_status = wl_open( dev );

    if( hcf_status != HCF_SUCCESS ) {
        result = -ENODEV;
    }

    DBG_LEAVE( DbgInfo );
    return result;
} 

int wl_adapter_close( struct net_device *dev )
{
    DBG_FUNC( "wl_adapter_close" );
    DBG_ENTER( DbgInfo );

    DBG_TRACE( DbgInfo, "wl_adapter_close() -- PCI\n" );
    DBG_TRACE( DbgInfo, "%s: Shutting down adapter.\n", dev->name );

    wl_close( dev );

    DBG_LEAVE( DbgInfo );
    return 0;
} 

int wl_adapter_is_open( struct net_device *dev )
{

    return TRUE;
} 

int __devinit wl_pci_probe( struct pci_dev *pdev,
                                const struct pci_device_id *ent )
{
    int result;
    

    DBG_FUNC( "wl_pci_probe" );
    DBG_ENTER( DbgInfo );
	DBG_PRINT( "%s\n", VERSION_INFO );

    result = wl_pci_setup( pdev );

    DBG_LEAVE( DbgInfo );

    return result;
} 

void __devexit wl_pci_remove(struct pci_dev *pdev)
{
    struct net_device       *dev = NULL;
    

    DBG_FUNC( "wl_pci_remove" );
    DBG_ENTER( DbgInfo );

    
    if( pdev == NULL ) {
        DBG_ERROR( DbgInfo, "PCI subsys passed in an invalid pci_dev pointer\n" );
        return;
    }

    dev = pci_get_drvdata( pdev );
    if( dev == NULL ) {
        DBG_ERROR( DbgInfo, "Could not retrieve net_device structure\n" );
        return;
    }

    
    wl_remove( dev );
    free_irq( dev->irq, dev );

#ifdef ENABLE_DMA
    wl_pci_dma_free( pdev, dev->priv );
#endif

    wl_device_dealloc( dev );

    DBG_LEAVE( DbgInfo );
    return;
} 

int wl_pci_setup( struct pci_dev *pdev )
{
    int                 result = 0;
    struct net_device   *dev = NULL;
    struct wl_private   *lp = NULL;
    

    DBG_FUNC( "wl_pci_setup" );
    DBG_ENTER( DbgInfo );

    
    if( pdev == NULL ) {
        DBG_ERROR( DbgInfo, "PCI subsys passed in an invalid pci_dev pointer\n" );
        return -ENODEV;
    }

    result = pci_enable_device( pdev );
    if( result != 0 ) {
        DBG_ERROR( DbgInfo, "pci_enable_device() failed\n" );
        DBG_LEAVE( DbgInfo );
        return result;
    }

    
    DBG_TRACE( DbgInfo, "Found our device, now registering\n" );
    dev = wl_device_alloc( );
    if( dev == NULL ) {
        DBG_ERROR( DbgInfo, "Could not register device!!!\n" );
        DBG_LEAVE( DbgInfo );
        return -ENOMEM;
    }

    
    if( dev->priv == NULL ) {
        DBG_ERROR( DbgInfo, "Private adapter struct was not allocated!!!\n" );
        DBG_LEAVE( DbgInfo );
        return -ENOMEM;
    }

#ifdef ENABLE_DMA
    
    if( wl_pci_dma_alloc( pdev, dev->priv ) < 0 ) {
        DBG_ERROR( DbgInfo, "Could not allocate DMA descriptor memory!!!\n" );
        DBG_LEAVE( DbgInfo );
        return -ENOMEM;
    }
#endif

    
    pci_set_drvdata( pdev, dev );

    
    dev->irq = pdev->irq;
    SET_MODULE_OWNER( dev );

    DBG_TRACE( DbgInfo, "Device Base Address: %#03lx\n", pdev->resource[0].start );
	dev->base_addr = pdev->resource[0].start;

    
    if( !wl_adapter_insert( dev )) {
        DBG_ERROR( DbgInfo, "wl_adapter_insert() FAILED!!!\n" );
        wl_device_dealloc( dev );
        DBG_LEAVE( DbgInfo );
        return -EINVAL;
    }

    
    DBG_TRACE( DbgInfo, "Registering ISR...\n" );

    result = request_irq(dev->irq, wl_isr, SA_SHIRQ, dev->name, dev);
    if( result ) {
        DBG_WARNING( DbgInfo, "Could not register ISR!!!\n" );
        DBG_LEAVE( DbgInfo );
        return result;
	}

    
    lp = dev->priv;

    if( lp->hcfCtx.IFB_BusType == CFG_NIC_BUS_TYPE_CARDBUS ||
	    lp->hcfCtx.IFB_BusType == CFG_NIC_BUS_TYPE_PCI 		) {
        DBG_TRACE( DbgInfo, "This is a PCI/CardBus card, enable interrupts\n" );
        wl_pci_enable_cardbus_interrupts( pdev );
    }

    
    pci_set_master( pdev );

    DBG_LEAVE( DbgInfo );
    return 0;
} 

void wl_pci_enable_cardbus_interrupts( struct pci_dev *pdev )
{
    u32                 bar2_reg;
    u32                 mem_addr_bus;
    u32                 func_evt_mask_reg;
    void                *mem_addr_kern = NULL;
    

    DBG_FUNC( "wl_pci_enable_cardbus_interrupts" );
    DBG_ENTER( DbgInfo );

    
    bar2_reg = 0xdeadbeef;
    mem_addr_bus = 0xdeadbeef;

    pci_read_config_dword( pdev, PCI_BASE_ADDRESS_2, &bar2_reg );
    mem_addr_bus = bar2_reg & PCI_BASE_ADDRESS_MEM_MASK;

    mem_addr_kern = ioremap( mem_addr_bus, 0x200 );

#ifdef HERMES25
#define REG_OFFSET  0x07F4
#else
#define REG_OFFSET  0x01F4
#endif 

#define BIT15       0x8000

    func_evt_mask_reg = *(u32 *)( mem_addr_kern + REG_OFFSET );
    func_evt_mask_reg |= BIT15;
    *(u32 *)( mem_addr_kern + REG_OFFSET ) = func_evt_mask_reg;

    
    iounmap( mem_addr_kern );

    DBG_LEAVE( DbgInfo );
    return;
} 

#ifdef ENABLE_DMA
int wl_pci_dma_alloc( struct pci_dev *pdev, struct wl_private *lp )
{
    int i;
    int status = 0;
    

    DBG_FUNC( "wl_pci_dma_alloc" );
    DBG_ENTER( DbgInfo );

    return status;
} 

int wl_pci_dma_free( struct pci_dev *pdev, struct wl_private *lp )
{
    int i;
    int status = 0;
    

    DBG_FUNC( "wl_pci_dma_free" );
    DBG_ENTER( DbgInfo );

    
    
    
    
    

    
    for( i = 0; i < NUM_RX_DESC; i++ ) {
        if( lp->dma.rx_packet[i] ) {
            status = wl_pci_dma_free_rx_packet( pdev, lp, &lp->dma.rx_packet[i] );
            if( status != 0 ) {
                DBG_WARNING( DbgInfo, "Problem freeing Rx packet\n" );
            }
        }
    }
    lp->dma.rx_rsc_ind = 0;

    if( lp->dma.rx_reclaim_desc ) {
        status = wl_pci_dma_free_desc( pdev, lp, &lp->dma.rx_reclaim_desc );
        if( status != 0 ) {
            DBG_WARNING( DbgInfo, "Problem freeing Rx reclaim descriptor\n" );
        }
    }

    
    for( i = 0; i < NUM_TX_DESC; i++ ) {
        if( lp->dma.tx_packet[i] ) {
            status = wl_pci_dma_free_tx_packet( pdev, lp, &lp->dma.tx_packet[i] );
            if( status != 0 ) {
                DBG_WARNING( DbgInfo, "Problem freeing Tx packet\n" );
            }
        }
    }
    lp->dma.tx_rsc_ind = 0;

    if( lp->dma.tx_reclaim_desc ) {
        status = wl_pci_dma_free_desc( pdev, lp, &lp->dma.tx_reclaim_desc );
        if( status != 0 ) {
            DBG_WARNING( DbgInfo, "Problem freeing Tx reclaim descriptor\n" );
        }
    }

    DBG_LEAVE( DbgInfo );
    return status;
} 


int wl_pci_dma_alloc_tx_packet( struct pci_dev *pdev, struct wl_private *lp,
                                DESC_STRCT **desc )
{
} 

int wl_pci_dma_free_tx_packet( struct pci_dev *pdev, struct wl_private *lp,
                                DESC_STRCT **desc )
{
    int status = 0;
    

    if( *desc == NULL ) {
        DBG_PRINT( "Null descriptor\n" );
        status = -EFAULT;
    }
	
	
    if( status == 0 && (*desc)->next_desc_addr ) {
        status = wl_pci_dma_free_desc_and_buf( pdev, lp, &(*desc)->next_desc_addr );
    }
    if( status == 0 ) {
        status = wl_pci_dma_free_desc_and_buf( pdev, lp, desc );
    }
    return status;
} 

int wl_pci_dma_alloc_rx_packet( struct pci_dev *pdev, struct wl_private *lp,
                                DESC_STRCT **desc )
{
    int         status = 0;
    DESC_STRCT  *p;
    


    return status;
} 

int wl_pci_dma_free_rx_packet( struct pci_dev *pdev, struct wl_private *lp,
                                DESC_STRCT **desc )
{
    int status = 0;
    DESC_STRCT *p;
    

    if( *desc == NULL ) {
        status = -EFAULT;
    }
    if( status == 0 ) {
        p = (*desc)->next_desc_addr;

        
        if( p != NULL ) {
            p->buf_addr      = NULL;
            p->buf_phys_addr = 0;

            status = wl_pci_dma_free_desc( pdev, lp, &p );
        }
    }

    
    if( status == 0 ) {
        SET_BUF_SIZE( *desc, HCF_MAX_PACKET_SIZE );
        status = wl_pci_dma_free_desc_and_buf( pdev, lp, desc );
    }
    return status;
} 

int wl_pci_dma_alloc_desc_and_buf( struct pci_dev *pdev, struct wl_private *lp,
                                   DESC_STRCT **desc, int size )
{
    int status = 0;
    

    return status;
} 

int wl_pci_dma_free_desc_and_buf( struct pci_dev *pdev, struct wl_private *lp,
                                   DESC_STRCT **desc )
{
    int status = 0;
    

    if( desc == NULL ) {
        status = -EFAULT;
    }
    if( status == 0 && *desc == NULL ) {
        status = -EFAULT;
    }
    if( status == 0 ) {
        status = wl_pci_dma_free_buf( pdev, lp, *desc );

        if( status == 0 ) {
            status = wl_pci_dma_free_desc( pdev, lp, desc );
        }
    }
    return status;
} 

int wl_pci_dma_alloc_desc( struct pci_dev *pdev, struct wl_private *lp,
                           DESC_STRCT **desc )
{
} 

int wl_pci_dma_free_desc( struct pci_dev *pdev, struct wl_private *lp,
                           DESC_STRCT **desc )
{
    int         status = 0;
    

    if( *desc == NULL ) {
        status = -EFAULT;
    }
    if( status == 0 ) {
        pci_free_consistent( pdev, sizeof( DESC_STRCT ), *desc,
                             (*desc)->desc_phys_addr );
    }
    *desc = NULL;
    return status;
} 

int wl_pci_dma_alloc_buf( struct pci_dev *pdev, struct wl_private *lp,
                          DESC_STRCT *desc, int size )
{
    int         status = 0;
    dma_addr_t  pa;
    

    return status;
} 

int wl_pci_dma_free_buf( struct pci_dev *pdev, struct wl_private *lp,
                         DESC_STRCT *desc )
{
    int         status = 0;
    

    if( desc == NULL ) {
        status = -EFAULT;
    }
    if( status == 0 && desc->buf_addr == NULL ) {
        status = -EFAULT;
    }
    if( status == 0 ) {
        pci_free_consistent( pdev, GET_BUF_SIZE( desc ), desc->buf_addr,
                             desc->buf_phys_addr );

        desc->buf_addr = 0;
        desc->buf_phys_addr = 0;
        SET_BUF_SIZE( desc, 0 );
    }
    return status;
} 

void wl_pci_dma_hcf_supply( struct wl_private *lp )
{
    int i;
    

    DBG_FUNC( "wl_pci_dma_hcf_supply" );
    DBG_ENTER( DbgInfo );

    
    
        
        if( lp->dma.tx_reclaim_desc ) {
            DBG_PRINT( "lp->dma.tx_reclaim_desc: 0x%p\n", lp->dma.tx_reclaim_desc );
            hcf_dma_tx_put( &lp->hcfCtx, lp->dma.tx_reclaim_desc, 0 );
            lp->dma.tx_reclaim_desc = NULL;
            DBG_PRINT( "lp->dma.tx_reclaim_desc: 0x%p\n", lp->dma.tx_reclaim_desc );
        }
        if( lp->dma.rx_reclaim_desc ) {
            DBG_PRINT( "lp->dma.rx_reclaim_desc: 0x%p\n", lp->dma.rx_reclaim_desc );
            hcf_dma_rx_put( &lp->hcfCtx, lp->dma.rx_reclaim_desc );
            lp->dma.rx_reclaim_desc = NULL;
            DBG_PRINT( "lp->dma.rx_reclaim_desc: 0x%p\n", lp->dma.rx_reclaim_desc );
        }
        
        for( i = 0; i < NUM_RX_DESC; i++ ) {
            DBG_PRINT( "lp->dma.rx_packet[%d]:    0x%p\n", i, lp->dma.rx_packet[i] );
            hcf_dma_rx_put( &lp->hcfCtx, lp->dma.rx_packet[i] );
            lp->dma.rx_packet[i] = NULL;
            DBG_PRINT( "lp->dma.rx_packet[%d]:    0x%p\n", i, lp->dma.rx_packet[i] );
        }
    

    DBG_LEAVE( DbgInfo );
    return;
} 

void wl_pci_dma_hcf_reclaim( struct wl_private *lp )
{
    int i;
    

    DBG_FUNC( "wl_pci_dma_hcf_reclaim" );
    DBG_ENTER( DbgInfo );

    wl_pci_dma_hcf_reclaim_rx( lp );
    for( i = 0; i < NUM_RX_DESC; i++ ) {
        DBG_PRINT( "rx_packet[%d] 0x%p\n", i, lp->dma.rx_packet[i] );
    }

    wl_pci_dma_hcf_reclaim_tx( lp );
    for( i = 0; i < NUM_TX_DESC; i++ ) {
        DBG_PRINT( "tx_packet[%d] 0x%p\n", i, lp->dma.tx_packet[i] );
     }

    DBG_LEAVE( DbgInfo );
    return;
} 

void wl_pci_dma_hcf_reclaim_rx( struct wl_private *lp )
{
    int         i;
    DESC_STRCT *p;
    

    DBG_FUNC( "wl_pci_dma_hcf_reclaim_rx" );
    DBG_ENTER( DbgInfo );

    
    
        while ( ( p = hcf_dma_rx_get( &lp->hcfCtx ) ) != NULL ) {
            if( p && p->buf_addr == NULL ) {
                lp->dma.rx_reclaim_desc = p;
            	DBG_PRINT( "reclaim_descriptor: 0x%p\n", p );
                continue;
            }
            for( i = 0; i < NUM_RX_DESC; i++ ) {
                if( lp->dma.rx_packet[i] == NULL ) {
                    break;
                }
            }
            
            lp->dma.rx_packet[i] = p;
            lp->dma.rx_rsc_ind++;
        	DBG_PRINT( "rx_packet[%d] 0x%p\n", i, lp->dma.rx_packet[i] );
        }
    
    DBG_LEAVE( DbgInfo );
} 

DESC_STRCT * wl_pci_dma_get_tx_packet( struct wl_private *lp )
{
    int i;
    DESC_STRCT *desc = NULL;
    

    for( i = 0; i < NUM_TX_DESC; i++ ) {
        if( lp->dma.tx_packet[i] ) {
            break;
        }
    }

    if( i != NUM_TX_DESC ) {
        desc = lp->dma.tx_packet[i];

        lp->dma.tx_packet[i] = NULL;
        lp->dma.tx_rsc_ind--;

        memset( desc->buf_addr, 0, HCF_DMA_TX_BUF1_SIZE );
    }

    return desc;
} 

void wl_pci_dma_put_tx_packet( struct wl_private *lp, DESC_STRCT *desc )
{
    int i;
    

    for( i = 0; i < NUM_TX_DESC; i++ ) {
        if( lp->dma.tx_packet[i] == NULL ) {
            break;
        }
    }

    if( i != NUM_TX_DESC ) {
        lp->dma.tx_packet[i] = desc;
        lp->dma.tx_rsc_ind++;
    }
} 

void wl_pci_dma_hcf_reclaim_tx( struct wl_private *lp )
{
    int         i;
    DESC_STRCT *p;
    

    DBG_FUNC( "wl_pci_dma_hcf_reclaim_tx" );
    DBG_ENTER( DbgInfo );

    
    
        while ( ( p = hcf_dma_tx_get( &lp->hcfCtx ) ) != NULL ) {

            if( p != NULL && p->buf_addr == NULL ) {
                lp->dma.tx_reclaim_desc = p;
            	DBG_PRINT( "reclaim_descriptor: 0x%p\n", p );
                continue;
            }
            for( i = 0; i < NUM_TX_DESC; i++ ) {
                if( lp->dma.tx_packet[i] == NULL ) {
                    break;
                }
            }
            
            lp->dma.tx_packet[i] = p;
            lp->dma.tx_rsc_ind++;
        	DBG_PRINT( "tx_packet[%d] 0x%p\n", i, lp->dma.tx_packet[i] );
        }
    

    if( lp->netif_queue_on == FALSE ) {
        netif_wake_queue( lp->dev );
        WL_WDS_NETIF_WAKE_QUEUE( lp );
        lp->netif_queue_on = TRUE;
    }
    DBG_LEAVE( DbgInfo );
    return;
} 
#endif  
