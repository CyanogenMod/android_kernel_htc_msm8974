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
 *   This file contains the main driver entry points and other adapter
 *   specific routines.
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


#define __KERNEL_SYSCALLS__

#include <wl_version.h>

#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/unistd.h>
#include <asm/uaccess.h>

#include <linux/netdevice.h>
#include <linux/etherdevice.h>

#define BIN_DL 0
#if BIN_DL
#include <linux/vmalloc.h>
#endif 


#include <debug.h>

#include <hcf.h>
#include <dhf.h>
#include <hcfdef.h>

#include <wl_if.h>
#include <wl_internal.h>
#include <wl_util.h>
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
#define VALID_PARAM(C) \
	{ \
		if (!(C)) \
		{ \
			printk(KERN_INFO "Wireless, parameter error: \"%s\"\n", #C); \
			goto failed; \
		} \
	}
void wl_isr_handler( unsigned long p );

#if 0 
int scull_read_procmem(char *buf, char **start, off_t offset, int len, int *eof, void *data );
static int write_int(struct file *file, const char *buffer, unsigned long count, void *data);
static void proc_write(const char *name, write_proc_t *w, void *data);

#endif 

static p_u16    irq_mask                = 0xdeb8; 
static p_s8     irq_list[4]             = { -1 };

#if 0
MODULE_PARM(irq_mask,               "h");
MODULE_PARM_DESC(irq_mask,               "IRQ mask [0xdeb8]");
MODULE_PARM(irq_list,               "1-4b");
MODULE_PARM_DESC(irq_list,               "IRQ list [<irq_mask>]");
#endif

static p_u8     PARM_AUTHENTICATION        	= PARM_DEFAULT_AUTHENTICATION;
static p_u16    PARM_AUTH_KEY_MGMT_SUITE   	= PARM_DEFAULT_AUTH_KEY_MGMT_SUITE;
static p_u16    PARM_BRSC_2GHZ             	= PARM_DEFAULT_BRSC_2GHZ;
static p_u16    PARM_BRSC_5GHZ             	= PARM_DEFAULT_BRSC_5GHZ;
static p_u16    PARM_COEXISTENCE           	= PARM_DEFAULT_COEXISTENCE;
static p_u16    PARM_CONNECTION_CONTROL    	= PARM_DEFAULT_CONNECTION_CONTROL;  
static p_char  *PARM_CREATE_IBSS           	= PARM_DEFAULT_CREATE_IBSS_STR;
static p_char  *PARM_DESIRED_SSID          	= PARM_DEFAULT_SSID;
static p_char  *PARM_DOWNLOAD_FIRMWARE      = "";
static p_u16    PARM_ENABLE_ENCRYPTION     	= PARM_DEFAULT_ENABLE_ENCRYPTION;
static p_char  *PARM_EXCLUDE_UNENCRYPTED   	= PARM_DEFAULT_EXCLUDE_UNENCRYPTED_STR;
static p_char  *PARM_INTRA_BSS_RELAY       	= PARM_DEFAULT_INTRA_BSS_RELAY_STR;
static p_char  *PARM_KEY1                  	= "";
static p_char  *PARM_KEY2                  	= "";
static p_char  *PARM_KEY3                  	= "";
static p_char  *PARM_KEY4                  	= "";
static p_char  *PARM_LOAD_BALANCING        	= PARM_DEFAULT_LOAD_BALANCING_STR;
static p_u16    PARM_MAX_SLEEP             	= PARM_DEFAULT_MAX_PM_SLEEP;
static p_char  *PARM_MEDIUM_DISTRIBUTION   	= PARM_DEFAULT_MEDIUM_DISTRIBUTION_STR;
static p_char  *PARM_MICROWAVE_ROBUSTNESS  	= PARM_DEFAULT_MICROWAVE_ROBUSTNESS_STR;
static p_char  *PARM_MULTICAST_PM_BUFFERING	= PARM_DEFAULT_MULTICAST_PM_BUFFERING_STR;
static p_u16    PARM_MULTICAST_RATE        	= PARM_DEFAULT_MULTICAST_RATE_2GHZ;
static p_char  *PARM_MULTICAST_RX          	= PARM_DEFAULT_MULTICAST_RX_STR;
static p_u8     PARM_NETWORK_ADDR[ETH_ALEN]	= PARM_DEFAULT_NETWORK_ADDR;
static p_u16    PARM_OWN_ATIM_WINDOW       	= PARM_DEFAULT_OWN_ATIM_WINDOW;
static p_u16    PARM_OWN_BEACON_INTERVAL   	= PARM_DEFAULT_OWN_BEACON_INTERVAL;
static p_u8     PARM_OWN_CHANNEL           	= PARM_DEFAULT_OWN_CHANNEL;
static p_u8     PARM_OWN_DTIM_PERIOD       	= PARM_DEFAULT_OWN_DTIM_PERIOD;
static p_char  *PARM_OWN_NAME              	= PARM_DEFAULT_OWN_NAME;
static p_char  *PARM_OWN_SSID              	= PARM_DEFAULT_SSID;
static p_u16	PARM_PM_ENABLED            	= WVLAN_PM_STATE_DISABLED;
static p_u16    PARM_PM_HOLDOVER_DURATION  	= PARM_DEFAULT_PM_HOLDOVER_DURATION;
static p_u8     PARM_PORT_TYPE             	= PARM_DEFAULT_PORT_TYPE;
static p_char  *PARM_PROMISCUOUS_MODE      	= PARM_DEFAULT_PROMISCUOUS_MODE_STR;
static p_char  *PARM_REJECT_ANY            	= PARM_DEFAULT_REJECT_ANY_STR;
#ifdef USE_WDS
static p_u16    PARM_RTS_THRESHOLD1        	= PARM_DEFAULT_RTS_THRESHOLD;
static p_u16    PARM_RTS_THRESHOLD2        	= PARM_DEFAULT_RTS_THRESHOLD;
static p_u16    PARM_RTS_THRESHOLD3        	= PARM_DEFAULT_RTS_THRESHOLD;
static p_u16    PARM_RTS_THRESHOLD4        	= PARM_DEFAULT_RTS_THRESHOLD;
static p_u16    PARM_RTS_THRESHOLD5        	= PARM_DEFAULT_RTS_THRESHOLD;
static p_u16    PARM_RTS_THRESHOLD6        	= PARM_DEFAULT_RTS_THRESHOLD;
#endif 
static p_u16    PARM_RTS_THRESHOLD         	= PARM_DEFAULT_RTS_THRESHOLD;
static p_u16    PARM_SRSC_2GHZ             	= PARM_DEFAULT_SRSC_2GHZ;
static p_u16    PARM_SRSC_5GHZ             	= PARM_DEFAULT_SRSC_5GHZ;
static p_u8     PARM_SYSTEM_SCALE          	= PARM_DEFAULT_SYSTEM_SCALE;
static p_u8     PARM_TX_KEY                	= PARM_DEFAULT_TX_KEY;
static p_u16    PARM_TX_POW_LEVEL          	= PARM_DEFAULT_TX_POW_LEVEL;
#ifdef USE_WDS
static p_u16    PARM_TX_RATE1              	= PARM_DEFAULT_TX_RATE_2GHZ;
static p_u16    PARM_TX_RATE2              	= PARM_DEFAULT_TX_RATE_2GHZ;
static p_u16    PARM_TX_RATE3              	= PARM_DEFAULT_TX_RATE_2GHZ;
static p_u16    PARM_TX_RATE4              	= PARM_DEFAULT_TX_RATE_2GHZ;
static p_u16    PARM_TX_RATE5              	= PARM_DEFAULT_TX_RATE_2GHZ;
static p_u16    PARM_TX_RATE6              	= PARM_DEFAULT_TX_RATE_2GHZ;
#endif 
static p_u16    PARM_TX_RATE               	= PARM_DEFAULT_TX_RATE_2GHZ;
#ifdef USE_WDS
static p_u8     PARM_WDS_ADDRESS1[ETH_ALEN]	= PARM_DEFAULT_NETWORK_ADDR;
static p_u8     PARM_WDS_ADDRESS2[ETH_ALEN]	= PARM_DEFAULT_NETWORK_ADDR;
static p_u8     PARM_WDS_ADDRESS3[ETH_ALEN]	= PARM_DEFAULT_NETWORK_ADDR;
static p_u8     PARM_WDS_ADDRESS4[ETH_ALEN]	= PARM_DEFAULT_NETWORK_ADDR;
static p_u8     PARM_WDS_ADDRESS5[ETH_ALEN]	= PARM_DEFAULT_NETWORK_ADDR;
static p_u8     PARM_WDS_ADDRESS6[ETH_ALEN]	= PARM_DEFAULT_NETWORK_ADDR;
#endif 


#if 0
MODULE_PARM(PARM_DESIRED_SSID,          "s");
MODULE_PARM_DESC(PARM_DESIRED_SSID,             "Network Name (<string>) [ANY]");
MODULE_PARM(PARM_OWN_SSID,              "s");
MODULE_PARM_DESC(PARM_OWN_SSID,                 "Network Name (<string>) [ANY]");
MODULE_PARM(PARM_OWN_CHANNEL,           "b");
MODULE_PARM_DESC(PARM_OWN_CHANNEL,              "Channel (0 - 14) [0]");
MODULE_PARM(PARM_SYSTEM_SCALE,          "b");
MODULE_PARM_DESC(PARM_SYSTEM_SCALE,             "Distance Between APs (1 - 3) [1]");
MODULE_PARM(PARM_TX_RATE,               "b");
MODULE_PARM_DESC(PARM_TX_RATE,                  "Transmit Rate Control");
MODULE_PARM(PARM_RTS_THRESHOLD,         "h");
MODULE_PARM_DESC(PARM_RTS_THRESHOLD,            "Medium Reservation (RTS/CTS Fragment Length) (256 - 2347) [2347]");
MODULE_PARM(PARM_MICROWAVE_ROBUSTNESS,  "s");
MODULE_PARM_DESC(PARM_MICROWAVE_ROBUSTNESS,     "Microwave Oven Robustness Enabled (<string> N or Y) [N]");
MODULE_PARM(PARM_OWN_NAME,              "s");
MODULE_PARM_DESC(PARM_OWN_NAME,                 "Station Name (<string>) [Linux]");

MODULE_PARM(PARM_ENABLE_ENCRYPTION,     "b");
MODULE_PARM_DESC(PARM_ENABLE_ENCRYPTION,        "Encryption Mode (0 - 7) [0]");

MODULE_PARM(PARM_KEY1,                  "s");
MODULE_PARM_DESC(PARM_KEY1,                     "Data Encryption Key 1 (<string>) []");
MODULE_PARM(PARM_KEY2,                  "s");
MODULE_PARM_DESC(PARM_KEY2,                     "Data Encryption Key 2 (<string>) []");
MODULE_PARM(PARM_KEY3,                  "s");
MODULE_PARM_DESC(PARM_KEY3,                     "Data Encryption Key 3 (<string>) []");
MODULE_PARM(PARM_KEY4,                  "s");
MODULE_PARM_DESC(PARM_KEY4,                     "Data Encryption Key 4 (<string>) []");
MODULE_PARM(PARM_TX_KEY,                "b");
MODULE_PARM_DESC(PARM_TX_KEY,                   "Transmit Key ID (1 - 4) [1]");
MODULE_PARM(PARM_MULTICAST_RATE,        "b");
MODULE_PARM_DESC(PARM_MULTICAST_RATE,           "Multicast Rate");
MODULE_PARM(PARM_DOWNLOAD_FIRMWARE,     "s");
MODULE_PARM_DESC(PARM_DOWNLOAD_FIRMWARE,        "filename of firmware image");

MODULE_PARM(PARM_AUTH_KEY_MGMT_SUITE,   "b");
MODULE_PARM_DESC(PARM_AUTH_KEY_MGMT_SUITE,      "Authentication Key Management suite (0-4) [0]");

MODULE_PARM(PARM_LOAD_BALANCING,        "s");
MODULE_PARM_DESC(PARM_LOAD_BALANCING,           "Load Balancing Enabled (<string> N or Y) [Y]");
MODULE_PARM(PARM_MEDIUM_DISTRIBUTION,   "s");
MODULE_PARM_DESC(PARM_MEDIUM_DISTRIBUTION,      "Medium Distribution Enabled (<string> N or Y) [Y]");
MODULE_PARM(PARM_TX_POW_LEVEL,          "b");
MODULE_PARM_DESC(PARM_TX_POW_LEVEL,             "Transmit Power (0 - 6) [3]");
MODULE_PARM(PARM_SRSC_2GHZ,             "b");
MODULE_PARM_DESC(PARM_SRSC_2GHZ,                "Supported Rate Set Control 2.4 GHz");
MODULE_PARM(PARM_SRSC_5GHZ,             "b");
MODULE_PARM_DESC(PARM_SRSC_5GHZ,                "Supported Rate Set Control 5.0 GHz");
MODULE_PARM(PARM_BRSC_2GHZ,             "b");
MODULE_PARM_DESC(PARM_BRSC_2GHZ,                "Basic Rate Set Control 2.4 GHz");
MODULE_PARM(PARM_BRSC_5GHZ,             "b");
MODULE_PARM_DESC(PARM_BRSC_5GHZ,                "Basic Rate Set Control 5.0 GHz");
#if 1 
MODULE_PARM(PARM_PM_ENABLED,            "h");
MODULE_PARM_DESC(PARM_PM_ENABLED,               "Power Management State (0 - 2, 8001 - 8002) [0]");
MODULE_PARM(PARM_PORT_TYPE,             "b");
MODULE_PARM_DESC(PARM_PORT_TYPE,                "Port Type (1 - 3) [1]");
MODULE_PARM(PARM_CONNECTION_CONTROL,    "b");
MODULE_PARM_DESC(PARM_CONNECTION_CONTROL,       "Connection Control (0 - 3) [2]");
#endif 
#if 1 
					
MODULE_PARM(PARM_OWN_DTIM_PERIOD,       "b");
MODULE_PARM_DESC(PARM_OWN_DTIM_PERIOD,          "DTIM Period (0 - 255) [1]");
MODULE_PARM(PARM_REJECT_ANY,            "s");
MODULE_PARM_DESC(PARM_REJECT_ANY,               "Closed System (<string> N or Y) [N]");
MODULE_PARM(PARM_EXCLUDE_UNENCRYPTED,   "s");
MODULE_PARM_DESC(PARM_EXCLUDE_UNENCRYPTED,      "Deny non-encrypted (<string> N or Y) [Y]");
MODULE_PARM(PARM_MULTICAST_PM_BUFFERING,"s");
MODULE_PARM_DESC(PARM_MULTICAST_PM_BUFFERING,   "Buffer MAC frames for Tx after DTIM (<string> Y or N) [Y]");
MODULE_PARM(PARM_INTRA_BSS_RELAY,       "s");
MODULE_PARM_DESC(PARM_INTRA_BSS_RELAY,          "IntraBSS Relay (<string> N or Y) [Y]");
MODULE_PARM(PARM_RTS_THRESHOLD1,        "h");
MODULE_PARM_DESC(PARM_RTS_THRESHOLD1,           "RTS Threshold, WDS Port 1 (256 - 2347) [2347]");
MODULE_PARM(PARM_RTS_THRESHOLD2,        "h");
MODULE_PARM_DESC(PARM_RTS_THRESHOLD2,           "RTS Threshold, WDS Port 2 (256 - 2347) [2347]");
MODULE_PARM(PARM_RTS_THRESHOLD3,        "h");
MODULE_PARM_DESC(PARM_RTS_THRESHOLD3,           "RTS Threshold, WDS Port 3 (256 - 2347) [2347]");
MODULE_PARM(PARM_RTS_THRESHOLD4,        "h");
MODULE_PARM_DESC(PARM_RTS_THRESHOLD4,           "RTS Threshold, WDS Port 4 (256 - 2347) [2347]");
MODULE_PARM(PARM_RTS_THRESHOLD5,        "h");
MODULE_PARM_DESC(PARM_RTS_THRESHOLD5,           "RTS Threshold, WDS Port 5 (256 - 2347) [2347]");
MODULE_PARM(PARM_RTS_THRESHOLD6,        "h");
MODULE_PARM_DESC(PARM_RTS_THRESHOLD6,           "RTS Threshold, WDS Port 6 (256 - 2347) [2347]");
MODULE_PARM(PARM_TX_RATE1,              "b");
MODULE_PARM_DESC(PARM_TX_RATE1,                 "Transmit Rate Control, WDS Port 1 (1 - 7) [3]");
MODULE_PARM(PARM_TX_RATE2,              "b");
MODULE_PARM_DESC(PARM_TX_RATE2,                 "Transmit Rate Control, WDS Port 2 (1 - 7) [3]");
MODULE_PARM(PARM_TX_RATE3,              "b");
MODULE_PARM_DESC(PARM_TX_RATE3,                 "Transmit Rate Control, WDS Port 3 (1 - 7) [3]");
MODULE_PARM(PARM_TX_RATE4,              "b");
MODULE_PARM_DESC(PARM_TX_RATE4,                 "Transmit Rate Control, WDS Port 4 (1 - 7) [3]");
MODULE_PARM(PARM_TX_RATE5,              "b");
MODULE_PARM_DESC(PARM_TX_RATE5,                 "Transmit Rate Control, WDS Port 5 (1 - 7) [3]");
MODULE_PARM(PARM_TX_RATE6,              "b");
MODULE_PARM_DESC(PARM_TX_RATE6,                 "Transmit Rate Control, WDS Port 6 (1 - 7) [3]");
MODULE_PARM(PARM_WDS_ADDRESS1,          "6b");
MODULE_PARM_DESC(PARM_WDS_ADDRESS1,             "MAC Address, WDS Port 1 ([0x00-0xff],[0x00-0xff],[0x00-0xff],[0x00-0xff],[0x00-0xff],[0x00-0xff]) [{0}]");
MODULE_PARM(PARM_WDS_ADDRESS2,          "6b");
MODULE_PARM_DESC(PARM_WDS_ADDRESS2,             "MAC Address, WDS Port 2 ([0x00-0xff],[0x00-0xff],[0x00-0xff],[0x00-0xff],[0x00-0xff],[0x00-0xff]) [{0}]");
MODULE_PARM(PARM_WDS_ADDRESS3,          "6b");
MODULE_PARM_DESC(PARM_WDS_ADDRESS3,             "MAC Address, WDS Port 3 ([0x00-0xff],[0x00-0xff],[0x00-0xff],[0x00-0xff],[0x00-0xff],[0x00-0xff]) [{0}]");
MODULE_PARM(PARM_WDS_ADDRESS4,          "6b");
MODULE_PARM_DESC(PARM_WDS_ADDRESS4,             "MAC Address, WDS Port 4 ([0x00-0xff],[0x00-0xff],[0x00-0xff],[0x00-0xff],[0x00-0xff],[0x00-0xff]) [{0}]");
MODULE_PARM(PARM_WDS_ADDRESS5,          "6b");
MODULE_PARM_DESC(PARM_WDS_ADDRESS5,             "MAC Address, WDS Port 5 ([0x00-0xff],[0x00-0xff],[0x00-0xff],[0x00-0xff],[0x00-0xff],[0x00-0xff]) [{0}]");
MODULE_PARM(PARM_WDS_ADDRESS6,          "6b");
MODULE_PARM_DESC(PARM_WDS_ADDRESS6,             "MAC Address, WDS Port 6 ([0x00-0xff],[0x00-0xff],[0x00-0xff],[0x00-0xff],[0x00-0xff],[0x00-0xff]) [{0}]");

MODULE_PARM(PARM_OWN_BEACON_INTERVAL,   "b");
MODULE_PARM_DESC(PARM_OWN_BEACON_INTERVAL,      "Own Beacon Interval (20 - 200) [100]");
MODULE_PARM(PARM_COEXISTENCE,   "b");
MODULE_PARM_DESC(PARM_COEXISTENCE,      "Coexistence (0-7) [0]");

#endif 
#endif

#if DBG

static p_u32    pc_debug = DBG_LVL;
 p_u32    DebugFlag = ~0; 

dbg_info_t   wl_info = { DBG_MOD_NAME, 0, 0 };
dbg_info_t  *DbgInfo = &wl_info;

#endif 
#ifdef USE_RTS

static p_char  *useRTS = "N";
MODULE_PARM( useRTS, "s" );
MODULE_PARM_DESC( useRTS, "Use RTS test interface (<string> N or Y) [N]" );

#endif  
extern struct CFG_RANGE2_STRCT BASED
	cfg_drv_act_ranges_pri; 	    

#if 0 
extern memimage ap;                 
#endif 

#if 1 
extern memimage fw_image;            
#endif 


int wl_insert( struct net_device *dev )
{
	int                     result = 0;
	int                     hcf_status = HCF_SUCCESS;
	int                     i;
	unsigned long           flags = 0;
	struct wl_private       *lp = wl_priv(dev);
	
	DBG_FUNC( "wl_insert" );
	DBG_ENTER( DbgInfo );

	
	memset( &( lp->hcfCtx ), 0, sizeof( IFB_STRCT ));

	
	spin_lock_init( &( lp->slock ));

	
	
        lp->is_handling_int = WL_NOT_HANDLING_INT;
	lp->firmware_present = WL_FRIMWARE_NOT_PRESENT;

	lp->dev = dev;

	DBG_PARAM( DbgInfo, "irq_mask", "0x%04x", irq_mask & 0x0FFFF );
	DBG_PARAM( DbgInfo, "irq_list", "0x%02x 0x%02x 0x%02x 0x%02x",
			   irq_list[0] & 0x0FF, irq_list[1] & 0x0FF,
			   irq_list[2] & 0x0FF, irq_list[3] & 0x0FF );
	DBG_PARAM( DbgInfo, PARM_NAME_DESIRED_SSID, "\"%s\"", PARM_DESIRED_SSID );
	DBG_PARAM( DbgInfo, PARM_NAME_OWN_SSID, "\"%s\"", PARM_OWN_SSID );
	DBG_PARAM( DbgInfo, PARM_NAME_OWN_CHANNEL, "%d", PARM_OWN_CHANNEL);
	DBG_PARAM( DbgInfo, PARM_NAME_SYSTEM_SCALE, "%d", PARM_SYSTEM_SCALE );
	DBG_PARAM( DbgInfo, PARM_NAME_TX_RATE, "%d", PARM_TX_RATE );
	DBG_PARAM( DbgInfo, PARM_NAME_RTS_THRESHOLD, "%d", PARM_RTS_THRESHOLD );
	DBG_PARAM( DbgInfo, PARM_NAME_MICROWAVE_ROBUSTNESS, "\"%s\"", PARM_MICROWAVE_ROBUSTNESS );
	DBG_PARAM( DbgInfo, PARM_NAME_OWN_NAME, "\"%s\"", PARM_OWN_NAME );
	DBG_PARAM( DbgInfo, PARM_NAME_KEY1, "\"%s\"", PARM_KEY1 );
	DBG_PARAM( DbgInfo, PARM_NAME_KEY2, "\"%s\"", PARM_KEY2 );
	DBG_PARAM( DbgInfo, PARM_NAME_KEY3, "\"%s\"", PARM_KEY3 );
	DBG_PARAM( DbgInfo, PARM_NAME_KEY4, "\"%s\"", PARM_KEY4 );
	DBG_PARAM( DbgInfo, PARM_NAME_TX_KEY, "%d", PARM_TX_KEY );
	DBG_PARAM( DbgInfo, PARM_NAME_MULTICAST_RATE, "%d", PARM_MULTICAST_RATE );
	DBG_PARAM( DbgInfo, PARM_NAME_DOWNLOAD_FIRMWARE, "\"%s\"", PARM_DOWNLOAD_FIRMWARE );
	DBG_PARAM( DbgInfo, PARM_NAME_AUTH_KEY_MGMT_SUITE, "%d", PARM_AUTH_KEY_MGMT_SUITE );
					
		DBG_PARAM( DbgInfo, PARM_NAME_PM_ENABLED, "%04x", PARM_PM_ENABLED );
#if 1 
		
		
	DBG_PARAM( DbgInfo, PARM_NAME_OWN_DTIM_PERIOD, "%d", PARM_OWN_DTIM_PERIOD );
	DBG_PARAM( DbgInfo, PARM_NAME_REJECT_ANY, "\"%s\"", PARM_REJECT_ANY );
	DBG_PARAM( DbgInfo, PARM_NAME_EXCLUDE_UNENCRYPTED, "\"%s\"", PARM_EXCLUDE_UNENCRYPTED );
	DBG_PARAM( DbgInfo, PARM_NAME_MULTICAST_PM_BUFFERING, "\"%s\"", PARM_MULTICAST_PM_BUFFERING );
	DBG_PARAM( DbgInfo, PARM_NAME_INTRA_BSS_RELAY, "\"%s\"", PARM_INTRA_BSS_RELAY );
#ifdef USE_WDS
	DBG_PARAM( DbgInfo, PARM_NAME_RTS_THRESHOLD1, "%d", PARM_RTS_THRESHOLD1 );
	DBG_PARAM( DbgInfo, PARM_NAME_RTS_THRESHOLD2, "%d", PARM_RTS_THRESHOLD2 );
	DBG_PARAM( DbgInfo, PARM_NAME_RTS_THRESHOLD3, "%d", PARM_RTS_THRESHOLD3 );
	DBG_PARAM( DbgInfo, PARM_NAME_RTS_THRESHOLD4, "%d", PARM_RTS_THRESHOLD4 );
	DBG_PARAM( DbgInfo, PARM_NAME_RTS_THRESHOLD5, "%d", PARM_RTS_THRESHOLD5 );
	DBG_PARAM( DbgInfo, PARM_NAME_RTS_THRESHOLD6, "%d", PARM_RTS_THRESHOLD6 );
	DBG_PARAM( DbgInfo, PARM_NAME_TX_RATE1, "%d", PARM_TX_RATE1 );
	DBG_PARAM( DbgInfo, PARM_NAME_TX_RATE2, "%d", PARM_TX_RATE2 );
	DBG_PARAM( DbgInfo, PARM_NAME_TX_RATE3, "%d", PARM_TX_RATE3 );
	DBG_PARAM( DbgInfo, PARM_NAME_TX_RATE4, "%d", PARM_TX_RATE4 );
	DBG_PARAM( DbgInfo, PARM_NAME_TX_RATE5, "%d", PARM_TX_RATE5 );
	DBG_PARAM( DbgInfo, PARM_NAME_TX_RATE6, "%d", PARM_TX_RATE6 );
	DBG_PARAM(DbgInfo, PARM_NAME_WDS_ADDRESS1, "\"%pM\"",
			PARM_WDS_ADDRESS1);
	DBG_PARAM(DbgInfo, PARM_NAME_WDS_ADDRESS2, "\"%pM\"",
			PARM_WDS_ADDRESS2);
	DBG_PARAM(DbgInfo, PARM_NAME_WDS_ADDRESS3, "\"%pM\"",
			PARM_WDS_ADDRESS3);
	DBG_PARAM(DbgInfo, PARM_NAME_WDS_ADDRESS4, "\"%pM\"",
			PARM_WDS_ADDRESS4);
	DBG_PARAM(DbgInfo, PARM_NAME_WDS_ADDRESS5, "\"%pM\"",
			PARM_WDS_ADDRESS5);
	DBG_PARAM(DbgInfo, PARM_NAME_WDS_ADDRESS6, "\"%pM\"",
			PARM_WDS_ADDRESS6);
#endif 
#endif 

	VALID_PARAM( !PARM_DESIRED_SSID || ( strlen( PARM_DESIRED_SSID ) <= PARM_MAX_NAME_LEN ));
	VALID_PARAM( !PARM_OWN_SSID || ( strlen( PARM_OWN_SSID ) <= PARM_MAX_NAME_LEN ));
	VALID_PARAM(( PARM_OWN_CHANNEL <= PARM_MAX_OWN_CHANNEL ));
	VALID_PARAM(( PARM_SYSTEM_SCALE >= PARM_MIN_SYSTEM_SCALE ) && ( PARM_SYSTEM_SCALE <= PARM_MAX_SYSTEM_SCALE ));
	VALID_PARAM(( PARM_TX_RATE >= PARM_MIN_TX_RATE ) && ( PARM_TX_RATE <= PARM_MAX_TX_RATE ));
	VALID_PARAM(( PARM_RTS_THRESHOLD <= PARM_MAX_RTS_THRESHOLD ));
	VALID_PARAM( !PARM_MICROWAVE_ROBUSTNESS || strchr( "NnYy", PARM_MICROWAVE_ROBUSTNESS[0] ) != NULL );
	VALID_PARAM( !PARM_OWN_NAME || ( strlen( PARM_NAME_OWN_NAME ) <= PARM_MAX_NAME_LEN ));
	VALID_PARAM(( PARM_ENABLE_ENCRYPTION <= PARM_MAX_ENABLE_ENCRYPTION ));
	VALID_PARAM( is_valid_key_string( PARM_KEY1 ));
	VALID_PARAM( is_valid_key_string( PARM_KEY2 ));
	VALID_PARAM( is_valid_key_string( PARM_KEY3 ));
	VALID_PARAM( is_valid_key_string( PARM_KEY4 ));
	VALID_PARAM(( PARM_TX_KEY >= PARM_MIN_TX_KEY ) && ( PARM_TX_KEY <= PARM_MAX_TX_KEY ));

	VALID_PARAM(( PARM_MULTICAST_RATE >= PARM_MIN_MULTICAST_RATE ) &&
					( PARM_MULTICAST_RATE <= PARM_MAX_MULTICAST_RATE ));

	VALID_PARAM( !PARM_DOWNLOAD_FIRMWARE || ( strlen( PARM_DOWNLOAD_FIRMWARE ) <= 255  ));
	VALID_PARAM(( PARM_AUTH_KEY_MGMT_SUITE < PARM_MAX_AUTH_KEY_MGMT_SUITE ));

	VALID_PARAM( !PARM_LOAD_BALANCING || strchr( "NnYy", PARM_LOAD_BALANCING[0] ) != NULL );
	VALID_PARAM( !PARM_MEDIUM_DISTRIBUTION || strchr( "NnYy", PARM_MEDIUM_DISTRIBUTION[0] ) != NULL );
	VALID_PARAM(( PARM_TX_POW_LEVEL <= PARM_MAX_TX_POW_LEVEL ));

 	VALID_PARAM(( PARM_PORT_TYPE >= PARM_MIN_PORT_TYPE ) && ( PARM_PORT_TYPE <= PARM_MAX_PORT_TYPE ));
	VALID_PARAM( PARM_PM_ENABLED <= WVLAN_PM_STATE_STANDARD ||
				 ( PARM_PM_ENABLED & 0x7FFF ) <= WVLAN_PM_STATE_STANDARD );
 	VALID_PARAM( !PARM_CREATE_IBSS || strchr( "NnYy", PARM_CREATE_IBSS[0] ) != NULL );
 	VALID_PARAM( !PARM_MULTICAST_RX || strchr( "NnYy", PARM_MULTICAST_RX[0] ) != NULL );
 	VALID_PARAM(( PARM_MAX_SLEEP <= PARM_MAX_MAX_PM_SLEEP ));
 	VALID_PARAM(( PARM_AUTHENTICATION <= PARM_MAX_AUTHENTICATION ));
 	VALID_PARAM(( PARM_OWN_ATIM_WINDOW <= PARM_MAX_OWN_ATIM_WINDOW ));
 	VALID_PARAM(( PARM_PM_HOLDOVER_DURATION <= PARM_MAX_PM_HOLDOVER_DURATION ));
 	VALID_PARAM( !PARM_PROMISCUOUS_MODE || strchr( "NnYy", PARM_PROMISCUOUS_MODE[0] ) != NULL );
	VALID_PARAM(( PARM_CONNECTION_CONTROL <= PARM_MAX_CONNECTION_CONTROL ));

	VALID_PARAM(( PARM_OWN_DTIM_PERIOD >= PARM_MIN_OWN_DTIM_PERIOD ));
	VALID_PARAM( !PARM_REJECT_ANY || strchr( "NnYy", PARM_REJECT_ANY[0] ) != NULL );
	VALID_PARAM( !PARM_EXCLUDE_UNENCRYPTED || strchr( "NnYy", PARM_EXCLUDE_UNENCRYPTED[0] ) != NULL );
	VALID_PARAM( !PARM_MULTICAST_PM_BUFFERING || strchr( "NnYy", PARM_MULTICAST_PM_BUFFERING[0] ) != NULL );
	VALID_PARAM( !PARM_INTRA_BSS_RELAY || strchr( "NnYy", PARM_INTRA_BSS_RELAY[0] ) != NULL );
#ifdef USE_WDS
	VALID_PARAM(( PARM_RTS_THRESHOLD1 <= PARM_MAX_RTS_THRESHOLD ));
	VALID_PARAM(( PARM_RTS_THRESHOLD2 <= PARM_MAX_RTS_THRESHOLD ));
	VALID_PARAM(( PARM_RTS_THRESHOLD3 <= PARM_MAX_RTS_THRESHOLD ));
	VALID_PARAM(( PARM_RTS_THRESHOLD4 <= PARM_MAX_RTS_THRESHOLD ));
	VALID_PARAM(( PARM_RTS_THRESHOLD5 <= PARM_MAX_RTS_THRESHOLD ));
	VALID_PARAM(( PARM_RTS_THRESHOLD6 <= PARM_MAX_RTS_THRESHOLD ));
	VALID_PARAM(( PARM_TX_RATE1 >= PARM_MIN_TX_RATE ) && (PARM_TX_RATE1 <= PARM_MAX_TX_RATE ));
	VALID_PARAM(( PARM_TX_RATE2 >= PARM_MIN_TX_RATE ) && (PARM_TX_RATE2 <= PARM_MAX_TX_RATE ));
	VALID_PARAM(( PARM_TX_RATE3 >= PARM_MIN_TX_RATE ) && (PARM_TX_RATE3 <= PARM_MAX_TX_RATE ));
	VALID_PARAM(( PARM_TX_RATE4 >= PARM_MIN_TX_RATE ) && (PARM_TX_RATE4 <= PARM_MAX_TX_RATE ));
	VALID_PARAM(( PARM_TX_RATE5 >= PARM_MIN_TX_RATE ) && (PARM_TX_RATE5 <= PARM_MAX_TX_RATE ));
	VALID_PARAM(( PARM_TX_RATE6 >= PARM_MIN_TX_RATE ) && (PARM_TX_RATE6 <= PARM_MAX_TX_RATE ));
#endif 

	VALID_PARAM(( PARM_OWN_BEACON_INTERVAL >= PARM_MIN_OWN_BEACON_INTERVAL ) && ( PARM_OWN_BEACON_INTERVAL <= PARM_MAX_OWN_BEACON_INTERVAL ));
	VALID_PARAM(( PARM_COEXISTENCE <= PARM_COEXISTENCE ));

	


	

	lp->Channel             = PARM_OWN_CHANNEL;
	lp->DistanceBetweenAPs  = PARM_SYSTEM_SCALE;

	
	lp->TxRateControl[0]    = PARM_DEFAULT_TX_RATE_2GHZ;
	lp->TxRateControl[1]    = PARM_DEFAULT_TX_RATE_5GHZ;

	lp->RTSThreshold        = PARM_RTS_THRESHOLD;

	
	lp->MulticastRate[0]    = PARM_DEFAULT_MULTICAST_RATE_2GHZ;
	lp->MulticastRate[1]    = PARM_DEFAULT_MULTICAST_RATE_5GHZ;

	if ( strchr( "Yy", PARM_MICROWAVE_ROBUSTNESS[0] ) != NULL ) {
		lp->MicrowaveRobustness = 1;
	} else {
		lp->MicrowaveRobustness = 0;
	}
	if ( PARM_DESIRED_SSID && ( strlen( PARM_DESIRED_SSID ) <= HCF_MAX_NAME_LEN )) {
		strcpy( lp->NetworkName, PARM_DESIRED_SSID );
	}
	if ( PARM_OWN_SSID && ( strlen( PARM_OWN_SSID ) <= HCF_MAX_NAME_LEN )) {
		strcpy( lp->NetworkName, PARM_OWN_SSID );
	}
	if ( PARM_OWN_NAME && ( strlen( PARM_OWN_NAME ) <= HCF_MAX_NAME_LEN )) {
		strcpy( lp->StationName, PARM_OWN_NAME );
	}
	lp->EnableEncryption = PARM_ENABLE_ENCRYPTION;
	if ( PARM_KEY1 && ( strlen( PARM_KEY1 ) <= MAX_KEY_LEN )) {
		strcpy( lp->Key1, PARM_KEY1 );
	}
	if ( PARM_KEY2 && ( strlen( PARM_KEY2 ) <= MAX_KEY_LEN )) {
		strcpy( lp->Key2, PARM_KEY2 );
	}
	if ( PARM_KEY3 && ( strlen( PARM_KEY3 ) <= MAX_KEY_LEN )) {
		strcpy( lp->Key3, PARM_KEY3 );
	}
	if ( PARM_KEY4 && ( strlen( PARM_KEY4 ) <= MAX_KEY_LEN )) {
		strcpy( lp->Key4, PARM_KEY4 );
	}

	lp->TransmitKeyID = PARM_TX_KEY;

	key_string2key( lp->Key1, &(lp->DefaultKeys.key[0] ));
	key_string2key( lp->Key2, &(lp->DefaultKeys.key[1] ));
	key_string2key( lp->Key3, &(lp->DefaultKeys.key[2] ));
	key_string2key( lp->Key4, &(lp->DefaultKeys.key[3] ));

	lp->DownloadFirmware = 1 ; 
	lp->AuthKeyMgmtSuite = PARM_AUTH_KEY_MGMT_SUITE;

	if ( strchr( "Yy", PARM_LOAD_BALANCING[0] ) != NULL ) {
		lp->loadBalancing = 1;
	} else {
		lp->loadBalancing = 0;
	}

	if ( strchr( "Yy", PARM_MEDIUM_DISTRIBUTION[0] ) != NULL ) {
		lp->mediumDistribution = 1;
	} else {
		lp->mediumDistribution = 0;
	}

	lp->txPowLevel = PARM_TX_POW_LEVEL;

	lp->srsc[0] = PARM_SRSC_2GHZ;
	lp->srsc[1] = PARM_SRSC_5GHZ;
	lp->brsc[0] = PARM_BRSC_2GHZ;
	lp->brsc[1] = PARM_BRSC_5GHZ;
#if 1 
	lp->PortType            = PARM_PORT_TYPE;
	lp->MaxSleepDuration    = PARM_MAX_SLEEP;
	lp->authentication      = PARM_AUTHENTICATION;
	lp->atimWindow          = PARM_OWN_ATIM_WINDOW;
	lp->holdoverDuration    = PARM_PM_HOLDOVER_DURATION;
	lp->PMEnabled           = PARM_PM_ENABLED;  
	if ( strchr( "Yy", PARM_CREATE_IBSS[0] ) != NULL ) {
		lp->CreateIBSS = 1;
	} else {
		lp->CreateIBSS = 0;
	}
	if ( strchr( "Nn", PARM_MULTICAST_RX[0] ) != NULL ) {
		lp->MulticastReceive = 0;
	} else {
		lp->MulticastReceive = 1;
	}
	if ( strchr( "Yy", PARM_PROMISCUOUS_MODE[0] ) != NULL ) {
		lp->promiscuousMode = 1;
	} else {
		lp->promiscuousMode = 0;
	}
	for( i = 0; i < ETH_ALEN; i++ ) {
	   lp->MACAddress[i] = PARM_NETWORK_ADDR[i];
	}

	lp->connectionControl = PARM_CONNECTION_CONTROL;

#endif 
#if 1 
	
	lp->DTIMPeriod = PARM_OWN_DTIM_PERIOD;

	if ( strchr( "Yy", PARM_REJECT_ANY[0] ) != NULL ) {
		lp->RejectAny = 1;
	} else {
		lp->RejectAny = 0;
	}
	if ( strchr( "Nn", PARM_EXCLUDE_UNENCRYPTED[0] ) != NULL ) {
		lp->ExcludeUnencrypted = 0;
	} else {
		lp->ExcludeUnencrypted = 1;
	}
	if ( strchr( "Yy", PARM_MULTICAST_PM_BUFFERING[0] ) != NULL ) {
		lp->multicastPMBuffering = 1;
	} else {
		lp->multicastPMBuffering = 0;
	}
	if ( strchr( "Yy", PARM_INTRA_BSS_RELAY[0] ) != NULL ) {
		lp->intraBSSRelay = 1;
	} else {
		lp->intraBSSRelay = 0;
	}

	lp->ownBeaconInterval = PARM_OWN_BEACON_INTERVAL;
	lp->coexistence       = PARM_COEXISTENCE;

#ifdef USE_WDS
	lp->wds_port[0].rtsThreshold    = PARM_RTS_THRESHOLD1;
	lp->wds_port[1].rtsThreshold    = PARM_RTS_THRESHOLD2;
	lp->wds_port[2].rtsThreshold    = PARM_RTS_THRESHOLD3;
	lp->wds_port[3].rtsThreshold    = PARM_RTS_THRESHOLD4;
	lp->wds_port[4].rtsThreshold    = PARM_RTS_THRESHOLD5;
	lp->wds_port[5].rtsThreshold    = PARM_RTS_THRESHOLD6;
	lp->wds_port[0].txRateCntl      = PARM_TX_RATE1;
	lp->wds_port[1].txRateCntl      = PARM_TX_RATE2;
	lp->wds_port[2].txRateCntl      = PARM_TX_RATE3;
	lp->wds_port[3].txRateCntl      = PARM_TX_RATE4;
	lp->wds_port[4].txRateCntl      = PARM_TX_RATE5;
	lp->wds_port[5].txRateCntl      = PARM_TX_RATE6;

	for( i = 0; i < ETH_ALEN; i++ ) {
		lp->wds_port[0].wdsAddress[i] = PARM_WDS_ADDRESS1[i];
	}
	for( i = 0; i < ETH_ALEN; i++ ) {
		lp->wds_port[1].wdsAddress[i] = PARM_WDS_ADDRESS2[i];
	}
	for( i = 0; i < ETH_ALEN; i++ ) {
		lp->wds_port[2].wdsAddress[i] = PARM_WDS_ADDRESS3[i];
	}
	for( i = 0; i < ETH_ALEN; i++ ) {
		lp->wds_port[3].wdsAddress[i] = PARM_WDS_ADDRESS4[i];
	}
	for( i = 0; i < ETH_ALEN; i++ ) {
		lp->wds_port[4].wdsAddress[i] = PARM_WDS_ADDRESS5[i];
	}
	for( i = 0; i < ETH_ALEN; i++ ) {
		lp->wds_port[5].wdsAddress[i] = PARM_WDS_ADDRESS6[i];
	}
#endif  
#endif  
#ifdef USE_RTS
	if ( strchr( "Yy", useRTS[0] ) != NULL ) {
		lp->useRTS = 1;
	} else {
		lp->useRTS = 0;
	}
#endif  


	


	wl_lock( lp, &flags );

	
	lp->portState = WVLAN_PORT_STATE_DISABLED;

	
	memset( &( lp->scan_results ), 0, sizeof( lp->scan_results ));
	lp->scan_results.scan_complete = FALSE;

	
	memset( &( lp->probe_results ), 0, sizeof( lp->probe_results ));
	lp->probe_results.scan_complete = FALSE;
	lp->probe_num_aps = 0;


	
	memset( lp->txList, 0, sizeof( lp->txList ));

	INIT_LIST_HEAD( &( lp->txFree ));

	lp->txF.skb  = NULL;
	lp->txF.port = 0;


	for( i = 0; i < DEFAULT_NUM_TX_FRAMES; i++ ) {
		list_add_tail( &( lp->txList[i].node ), &( lp->txFree ));
	}


	for( i = 0; i < WVLAN_MAX_TX_QUEUES; i++ ) {
		INIT_LIST_HEAD( &( lp->txQ[i] ));
	}

	lp->netif_queue_on = TRUE;
	lp->txQ_count = 0;
#ifdef WARP
#ifdef ENABLE_DMA
	DBG_TRACE( DbgInfo, "HERMES 2.5 BUSMASTER DMA MODE\n" );
	lp->use_dma = 1;
#else
	DBG_TRACE( DbgInfo, "HERMES 2.5 PORT I/O MODE\n" );
	lp->use_dma = 0;
#endif 
#endif 

        tasklet_init(&lp->task, wl_isr_handler, (unsigned long)lp);

        
        DBG_TRACE( DbgInfo, "Calling hcf_connect()...\n" );
        hcf_status = hcf_connect( &lp->hcfCtx, dev->base_addr );
	
	
	if ( hcf_status != HCF_SUCCESS && hcf_status != HCF_ERR_INCOMP_FW ) {
		DBG_ERROR( DbgInfo, "hcf_connect() failed, status: 0x%x\n", hcf_status );
		wl_unlock( lp, &flags );
		goto hcf_failed;
	}

	
	lp->driverInfo.IO_address       = dev->base_addr;
	lp->driverInfo.IO_range         = HCF_NUM_IO_PORTS;	
	lp->driverInfo.IRQ_number       = dev->irq;
	lp->driverInfo.card_stat        = lp->hcfCtx.IFB_CardStat;
	

	
	lp->driverIdentity.len              = ( sizeof( lp->driverIdentity ) / sizeof( hcf_16 )) - 1;
	lp->driverIdentity.typ              = CFG_DRV_IDENTITY;
	lp->driverIdentity.comp_id          = DRV_IDENTITY;
	lp->driverIdentity.variant          = DRV_VARIANT;
	lp->driverIdentity.version_major    = DRV_MAJOR_VERSION;
	lp->driverIdentity.version_minor    = DRV_MINOR_VERSION;


	DBG_TRACE( DbgInfo, "Calling wvlan_go() to perform a card reset...\n" );
	hcf_status = wl_go( lp );

	if ( hcf_status != HCF_SUCCESS ) {
		DBG_ERROR( DbgInfo, "wl_go() failed\n" );
		wl_unlock( lp, &flags );
		goto hcf_failed;
	}

	
	wl_put_ltv_init( lp );

#if 0 
	
	if ( wl_adapter_is_open( lp->dev )) {
		
		DBG_TRACE( DbgInfo, "Enabling Port 0\n" );
		hcf_status = wl_enable( lp );

		if ( hcf_status != HCF_SUCCESS ) {
			DBG_TRACE( DbgInfo, "Enable port 0 failed: 0x%x\n", hcf_status );
		}

#if (HCF_TYPE) & HCF_TYPE_AP
		DBG_TRACE( DbgInfo, "Enabling WDS Ports\n" );
		
#endif  

	}
#endif

	
	memcpy( lp->dev->dev_addr, lp->MACAddress, ETH_ALEN );
	dev->addr_len = ETH_ALEN;

	lp->is_registered = TRUE;

#ifdef USE_PROFILE
	parse_config( dev );
#endif  

	WL_WDS_NETDEV_REGISTER( lp );

	lp->DownloadFirmware = WVLAN_DRV_MODE_STA; 

#ifdef USE_RTS
	if ( lp->useRTS == 1 ) {
		DBG_TRACE( DbgInfo, "ENTERING RTS MODE...\n" );
                wl_act_int_off( lp );
                lp->is_handling_int = WL_NOT_HANDLING_INT; 

		wl_disable( lp );

        	hcf_connect( &lp->hcfCtx, HCF_DISCONNECT);
	}
#endif  

	wl_unlock( lp, &flags );

	DBG_TRACE( DbgInfo, "%s: Wireless, io_addr %#03lx, irq %d, ""mac_address ",
			   dev->name, dev->base_addr, dev->irq );

	for( i = 0; i < ETH_ALEN; i++ ) {
		printk( "%02X%c", dev->dev_addr[i], (( i < ( ETH_ALEN-1 )) ? ':' : '\n' ));
	}

#if 0 
	create_proc_read_entry( "wlags", 0, NULL, scull_read_procmem, dev );
	proc_mkdir("driver/wlags49", 0);
	proc_write("driver/wlags49/wlags49_type", write_int, &lp->wlags49_type);
#endif 

	DBG_LEAVE( DbgInfo );
	return result;

hcf_failed:
	wl_hcf_error( dev, hcf_status );

failed:

	DBG_ERROR( DbgInfo, "wl_insert() FAILED\n" );

	if ( lp->is_registered == TRUE ) {
		lp->is_registered = FALSE;
	}

	WL_WDS_NETDEV_DEREGISTER( lp );

	result = -EFAULT;


	DBG_LEAVE( DbgInfo );
	return result;
} 


int wl_reset(struct net_device *dev)
{
	struct wl_private  *lp = wl_priv(dev);
	int                 hcf_status = HCF_SUCCESS;
	
	DBG_FUNC( "wl_reset" );
	DBG_ENTER( DbgInfo );
	DBG_PARAM( DbgInfo, "dev", "%s (0x%p)", dev->name, dev );
	DBG_PARAM( DbgInfo, "dev->base_addr", "(%#03lx)", dev->base_addr );


	DBG_TRACE( DbgInfo, "Device Base Address: %#03lx\n", dev->base_addr );
	if ( dev->base_addr ) {
		
		hcf_connect( &lp->hcfCtx, HCF_DISCONNECT );

		
		lp->txBytes = 0;

		
        	hcf_status = hcf_connect( &lp->hcfCtx, dev->base_addr );
		if ( hcf_status != HCF_SUCCESS && hcf_status != HCF_ERR_INCOMP_FW ) {
			DBG_ERROR( DbgInfo, "hcf_connect() failed, status: 0x%x\n", hcf_status );
			goto out;
		}

		
		if ( hcf_status == HCF_ERR_INCOMP_FW ) {
			lp->firmware_present = WL_FRIMWARE_NOT_PRESENT;
		}

		
		lp->portState = WVLAN_PORT_STATE_DISABLED;

		
		hcf_status = wl_go( lp );
		if ( hcf_status != HCF_SUCCESS ) {
			DBG_ERROR( DbgInfo, "wl_go() failed, status: 0x%x\n", hcf_status );
			goto out;
		}

		
		wl_put_ltv_init( lp );
	} else {
		DBG_ERROR( DbgInfo, "Device Base Address INVALID!!!\n" );
	}

out:
	DBG_LEAVE( DbgInfo );
	return hcf_status;
} 


int wl_go( struct wl_private *lp )
{
	int  	hcf_status = HCF_SUCCESS;
	char	*cp = NULL;			
	int	retries = 0;
	
	DBG_FUNC( "wl_go" );
	DBG_ENTER( DbgInfo );

	hcf_status = wl_disable( lp );
	if ( hcf_status != HCF_SUCCESS ) {
		DBG_TRACE( DbgInfo, "Disable port 0 failed: 0x%x\n", hcf_status );

		while (( hcf_status != HCF_SUCCESS ) && (retries < 10)) {
			retries++;
			hcf_status = wl_disable( lp );
		}
		if ( hcf_status == HCF_SUCCESS ) {
			DBG_TRACE( DbgInfo, "Disable port 0 succes : %d retries\n", retries );
		} else {
			DBG_TRACE( DbgInfo, "Disable port 0 failed after: %d retries\n", retries );
		}
	}

#if 1 
	
	
#endif  


#if BIN_DL
	if ( strlen( lp->fw_image_filename ) ) {
mm_segment_t	fs;
int		    	file_desc;
int 			rc;

		DBG_TRACE( DbgInfo, "F/W image:%s:\n", lp->fw_image_filename );
		
		fs = get_fs( );
		set_fs( get_ds( ));
		file_desc = open( lp->fw_image_filename, O_RDONLY, 0 );
		if ( file_desc == -1 ) {
			DBG_ERROR( DbgInfo, "No image file found\n" );
		} else {
			DBG_TRACE( DbgInfo, "F/W image file found\n" );
#define DHF_ALLOC_SIZE 96000			
			cp = (char*)vmalloc( DHF_ALLOC_SIZE );
			if ( cp == NULL ) {
				DBG_ERROR( DbgInfo, "error in vmalloc\n" );
			} else {
				rc = read( file_desc, cp, DHF_ALLOC_SIZE );
				if ( rc == DHF_ALLOC_SIZE ) {
					DBG_ERROR( DbgInfo, "buffer too small, %d\n", DHF_ALLOC_SIZE );
				} else if ( rc > 0 ) {
					DBG_TRACE( DbgInfo, "read O.K.: %d bytes  %.12s\n", rc, cp );
					rc = read( file_desc, &cp[rc], 1 );
					if ( rc == 0 ) { 
						DBG_TRACE( DbgInfo, "no more to read\n" );
					}
				}
				if ( rc != 0 ) {
					DBG_ERROR( DbgInfo, "file not read in one swoop or other error"\
										", give up, too complicated, rc = %0X\n", rc );
					DBG_ERROR( DbgInfo, "still have to change code to get a real download now !!!!!!!!\n" );
				} else {
					DBG_TRACE( DbgInfo, "before dhf_download_binary\n" );
					hcf_status = dhf_download_binary( (memimage *)cp );
					DBG_TRACE( DbgInfo, "after dhf_download_binary, before dhf_download_fw\n" );
					
					hcf_status = dhf_download_fw( &lp->hcfCtx, (memimage *)cp );
					DBG_TRACE( DbgInfo, "after dhf_download_fw\n" );
				}
				vfree( cp );
			}
			close( file_desc );
		}
		set_fs( fs );			
	}
#endif 

	
	if ( (lp->firmware_present == WL_FRIMWARE_PRESENT)
	     &&
	     ( CNV_INT_TO_LITTLE( lp->hcfCtx.IFB_FWIdentity.comp_id ) != COMP_ID_FW_STA )
	     &&
	     ( CNV_INT_TO_LITTLE( lp->hcfCtx.IFB_FWIdentity.comp_id ) != COMP_ID_FW_AP ) ) {
		
		lp->firmware_present = WL_FRIMWARE_NOT_PRESENT;
	}

	if(lp->firmware_present == WL_FRIMWARE_NOT_PRESENT)
	{
		if ( cp == NULL ) {
			DBG_TRACE( DbgInfo, "Downloading STA firmware...\n" );
			hcf_status = dhf_download_fw( &lp->hcfCtx, &fw_image );
		}
		if ( hcf_status != HCF_SUCCESS ) {
			DBG_ERROR( DbgInfo, "Firmware Download failed\n" );
			DBG_LEAVE( DbgInfo );
			return hcf_status;
		}
	}
	
	
	if ( CNV_INT_TO_LITTLE( lp->hcfCtx.IFB_FWIdentity.comp_id ) == COMP_ID_FW_STA  ) {
		DBG_TRACE( DbgInfo, "downloaded station F/W\n" );
	} else if ( CNV_INT_TO_LITTLE( lp->hcfCtx.IFB_FWIdentity.comp_id ) == COMP_ID_FW_AP  ) {
		DBG_TRACE( DbgInfo, "downloaded AP F/W\n" );
	} else {
		DBG_ERROR( DbgInfo, "unknown F/W type\n" );
	}

	lp->firmware_present = WL_FRIMWARE_PRESENT;

	DBG_TRACE( DbgInfo, "ComponentID:%04x variant:%04x major:%04x minor:%04x\n",
				CNV_INT_TO_LITTLE( lp->hcfCtx.IFB_FWIdentity.comp_id ),
				CNV_INT_TO_LITTLE( lp->hcfCtx.IFB_FWIdentity.variant ),
				CNV_INT_TO_LITTLE( lp->hcfCtx.IFB_FWIdentity.version_major ),
				CNV_INT_TO_LITTLE( lp->hcfCtx.IFB_FWIdentity.version_minor ));

	
	lp->ltvRecord.len = 4;
	if ( CNV_INT_TO_LITTLE( lp->hcfCtx.IFB_FWIdentity.comp_id ) == COMP_ID_FW_AP  ) {
		lp->ltvRecord.typ = CFG_NIC_MAC_ADDR;
	} else
	{
		lp->ltvRecord.typ = CFG_CNF_OWN_MAC_ADDR;
	}
	hcf_status = hcf_get_info( &lp->hcfCtx, (LTVP)&( lp->ltvRecord ));
	if ( hcf_status != HCF_SUCCESS ) {
		DBG_ERROR( DbgInfo, "Could not retrieve MAC address\n" );
		DBG_LEAVE( DbgInfo );
		return hcf_status;
	}
	memcpy( lp->MACAddress, &lp->ltvRecord.u.u8[0], ETH_ALEN );
	DBG_TRACE(DbgInfo, "Card MAC Address: %pM\n", lp->MACAddress);

	wl_put_ltv( lp );
	
	hcf_status = wl_enable( lp );

	if ( lp->DownloadFirmware == WVLAN_DRV_MODE_AP ) {
#ifdef USE_WDS
		wl_enable_wds_ports( lp );
#endif 
		hcf_status = wl_connect( lp );
	}
	DBG_LEAVE( DbgInfo );
	return hcf_status;
} 


void wl_set_wep_keys( struct wl_private *lp )
{
	int count = 0;
	
	DBG_FUNC( "wl_set_wep_keys" );
	DBG_ENTER( DbgInfo );
	DBG_PARAM( DbgInfo, "lp", "%s (0x%p)", lp->dev->name, lp );
	if ( lp->EnableEncryption ) {

		
		lp->ltvRecord.len = 2;
		lp->ltvRecord.typ       = CFG_TX_KEY_ID;
		lp->ltvRecord.u.u16[0]  = CNV_INT_TO_LITTLE(lp->TransmitKeyID - 1);

		hcf_put_info( &lp->hcfCtx, (LTVP)&( lp->ltvRecord ));

		DBG_TRACE( DbgInfo, "Key 1 len: %d\n", lp->DefaultKeys.key[0].len );
		DBG_TRACE( DbgInfo, "Key 2 len: %d\n", lp->DefaultKeys.key[1].len );
		DBG_TRACE( DbgInfo, "Key 3 len: %d\n", lp->DefaultKeys.key[2].len );
		DBG_TRACE( DbgInfo, "Key 4 len: %d\n", lp->DefaultKeys.key[3].len );

		
		lp->DefaultKeys.len = sizeof( lp->DefaultKeys ) / sizeof( hcf_16 ) - 1;
		lp->DefaultKeys.typ = CFG_DEFAULT_KEYS;

		
		for( count = 0; count < MAX_KEYS; count++ ) {
			lp->DefaultKeys.key[count].len = CNV_INT_TO_LITTLE( lp->DefaultKeys.key[count].len );
		}

		hcf_put_info( &lp->hcfCtx, (LTVP)&( lp->DefaultKeys ));

		for( count = 0; count < MAX_KEYS; count++ ) {
			lp->DefaultKeys.key[count].len = CNV_INT_TO_LITTLE( lp->DefaultKeys.key[count].len );
		}

		DBG_NOTICE( DbgInfo, "encrypt: %d, ID: %d\n", lp->EnableEncryption, lp->TransmitKeyID );
		DBG_NOTICE( DbgInfo, "set key: %s(%d) [%d]\n", lp->DefaultKeys.key[lp->TransmitKeyID-1].key, lp->DefaultKeys.key[lp->TransmitKeyID-1].len, lp->TransmitKeyID-1 );
	}

	DBG_LEAVE( DbgInfo );
} 


int wl_apply(struct wl_private *lp)
{
	int hcf_status = HCF_SUCCESS;
	
	DBG_FUNC( "wl_apply" );
	DBG_ENTER( DbgInfo );
	DBG_ASSERT( lp != NULL);
	DBG_PARAM( DbgInfo, "lp", "%s (0x%p)", lp->dev->name, lp );

	if ( !( lp->flags & WVLAN2_UIL_BUSY )) {

		if ( wl_adapter_is_open( lp->dev )) {
			
			hcf_status = wl_disconnect( lp );
			if ( hcf_status != HCF_SUCCESS ) {
				DBG_ERROR( DbgInfo, "Disconnect failed\n" );
				DBG_LEAVE( DbgInfo );
				return -1;
			}
			hcf_status = wl_disable( lp );
			if ( hcf_status != HCF_SUCCESS ) {
				DBG_ERROR( DbgInfo, "Disable failed\n" );
				DBG_LEAVE( DbgInfo );
				return -1;
			} else {
				hcf_status = wl_put_ltv( lp );

				if ( hcf_status == HCF_SUCCESS ) {
					hcf_status = wl_enable( lp );

					if ( CNV_INT_TO_LITTLE( lp->hcfCtx.IFB_FWIdentity.comp_id ) == COMP_ID_FW_AP  ) {
						hcf_status = wl_connect( lp );
					}
				} else {
					DBG_WARNING( DbgInfo, "wl_put_ltv() failed\n" );
				}
			}
		}
	}

	DBG_LEAVE( DbgInfo );
	return hcf_status;
} 


int wl_put_ltv_init( struct wl_private *lp )
{
	int i;
	int hcf_status;
	CFG_RID_LOG_STRCT *RidLog;
	
	DBG_FUNC( "wl_put_ltv_init" );
	DBG_ENTER( DbgInfo );
	if ( lp == NULL ) {
		DBG_ERROR( DbgInfo, "lp pointer is NULL\n" );
		DBG_LEAVE( DbgInfo );
		return -1;
	}
	
	lp->ltvRecord.len = 2;
	lp->ltvRecord.typ = CFG_CNTL_OPT;

#ifdef BUS_PCMCIA
	lp->ltvRecord.u.u16[0] = CNV_INT_TO_LITTLE( USE_16BIT );
#else
	if ( lp->use_dma ) {
		lp->ltvRecord.u.u16[0] = CNV_INT_TO_LITTLE( USE_DMA );
	} else {
		lp->ltvRecord.u.u16[0] = CNV_INT_TO_LITTLE( 0 );
	}

#endif
	hcf_status = hcf_put_info( &lp->hcfCtx, (LTVP)&( lp->ltvRecord ));
	DBG_TRACE( DbgInfo, "CFG_CNTL_OPT                      : 0x%04x\n",
			   lp->ltvRecord.u.u16[0] );
	DBG_TRACE( DbgInfo, "CFG_CNTL_OPT result               : 0x%04x\n",
			   hcf_status );

	i=0;

	lp->RidList[i].len     = sizeof( lp->ProbeResp );
	lp->RidList[i].typ     = CFG_ACS_SCAN;
	lp->RidList[i].bufp    = (wci_recordp)&lp->ProbeResp;
	
	i++;

	lp->RidList[i].len     = sizeof( lp->assoc_stat );
	lp->RidList[i].typ     = CFG_ASSOC_STAT;
	lp->RidList[i].bufp    = (wci_recordp)&lp->assoc_stat;
	lp->assoc_stat.len     = 0xFFFF;
	i++;

	lp->RidList[i].len     = 4;
	lp->RidList[i].typ     = CFG_UPDATED_INFO_RECORD;
	lp->RidList[i].bufp    = (wci_recordp)&lp->updatedRecord;
	lp->updatedRecord.len  = 0xFFFF;
	i++;

	lp->RidList[i].len     = sizeof( lp->sec_stat );
	lp->RidList[i].typ     = CFG_SECURITY_STAT;
	lp->RidList[i].bufp    = (wci_recordp)&lp->sec_stat;
	lp->sec_stat.len       = 0xFFFF;
	i++;

	lp->RidList[i].typ     = 0;    

	RidLog = (CFG_RID_LOG_STRCT *)&lp->ltvRecord;
	RidLog->len     = 3;
	RidLog->typ     = CFG_REG_INFO_LOG;
	RidLog->recordp = (RID_LOGP)&lp->RidList[0];

	hcf_status = hcf_put_info( &lp->hcfCtx, (LTVP)&( lp->ltvRecord ));
	DBG_TRACE( DbgInfo, "CFG_REG_INFO_LOG\n" );
	DBG_TRACE( DbgInfo, "CFG_REG_INFO_LOG result           : 0x%04x\n",
			   hcf_status );
	DBG_LEAVE( DbgInfo );
	return hcf_status;
} 


int wl_put_ltv( struct wl_private *lp )
{
	int len;
	int hcf_status;
	
	DBG_FUNC( "wl_put_ltv" );
	DBG_ENTER( DbgInfo );

	if ( lp == NULL ) {
		DBG_ERROR( DbgInfo, "lp pointer is NULL\n" );
		return -1;
	}
	if ( CNV_INT_TO_LITTLE( lp->hcfCtx.IFB_FWIdentity.comp_id ) == COMP_ID_FW_AP  ) {
		lp->maxPort = 6;			
	} else {
		lp->maxPort = 0;
	}

	
	lp->ltvRecord.len       = 4;
	lp->ltvRecord.typ       = CFG_REG_MB;
	lp->ltvRecord.u.u32[0]  = (u_long)&( lp->mailbox );
	lp->ltvRecord.u.u16[2]  = ( MB_SIZE / sizeof( hcf_16 ));
	hcf_status = hcf_put_info( &lp->hcfCtx, (LTVP)&( lp->ltvRecord ));

	
	lp->ltvRecord.len       = 2;
	lp->ltvRecord.typ       = CFG_CNF_MAX_DATA_LEN;
	lp->ltvRecord.u.u16[0]  = CNV_INT_TO_LITTLE( HCF_MAX_PACKET_SIZE );
	hcf_status = hcf_put_info( &lp->hcfCtx, (LTVP)&( lp->ltvRecord ));

	
	lp->ltvRecord.len       = 2;
	lp->ltvRecord.typ       = CFG_CNF_SYSTEM_SCALE;
	lp->ltvRecord.u.u16[0]  = CNV_INT_TO_LITTLE( lp->DistanceBetweenAPs );
	hcf_status = hcf_put_info( &lp->hcfCtx, (LTVP)&( lp->ltvRecord ));

	
	if ( lp->CreateIBSS && ( lp->Channel == 0 )) {
		DBG_TRACE( DbgInfo, "Create IBSS" );
		lp->Channel = 10;
	}
	lp->ltvRecord.len       = 2;
	lp->ltvRecord.typ       = CFG_CNF_OWN_CHANNEL;
	lp->ltvRecord.u.u16[0]  = CNV_INT_TO_LITTLE( lp->Channel );
	hcf_status = hcf_put_info( &lp->hcfCtx, (LTVP)&( lp->ltvRecord ));

	
	lp->ltvRecord.len       = 2;
	lp->ltvRecord.typ       = CFG_CNF_MICRO_WAVE;
	lp->ltvRecord.u.u16[0]  = CNV_INT_TO_LITTLE( lp->MicrowaveRobustness );
	hcf_status = hcf_put_info( &lp->hcfCtx, (LTVP)&( lp->ltvRecord ));

	
	lp->ltvRecord.len       = 2;
	lp->ltvRecord.typ       = CFG_CNF_LOAD_BALANCING;
	lp->ltvRecord.u.u16[0]  = CNV_INT_TO_LITTLE( lp->loadBalancing );
	hcf_status = hcf_put_info( &lp->hcfCtx, (LTVP)&( lp->ltvRecord ));

	
	lp->ltvRecord.len       = 2;
	lp->ltvRecord.typ       = CFG_CNF_MEDIUM_DISTRIBUTION;
	lp->ltvRecord.u.u16[0]  = CNV_INT_TO_LITTLE( lp->mediumDistribution );
	hcf_status = hcf_put_info( &lp->hcfCtx, (LTVP)&( lp->ltvRecord ));
	

#ifdef WARP
	
	lp->ltvRecord.len       = 2;
	lp->ltvRecord.typ       = CFG_CNF_TX_POW_LVL;
	lp->ltvRecord.u.u16[0]  = CNV_INT_TO_LITTLE( lp->txPowLevel );
	hcf_status = hcf_put_info( &lp->hcfCtx, (LTVP)&( lp->ltvRecord ));

	

	

	
	lp->ltvRecord.len       = 3;
	lp->ltvRecord.typ       = CFG_SUPPORTED_RATE_SET_CNTL; 
	lp->ltvRecord.u.u16[0]  = CNV_INT_TO_LITTLE( lp->srsc[0] );
	lp->ltvRecord.u.u16[1]  = CNV_INT_TO_LITTLE( lp->srsc[1] );
	hcf_status = hcf_put_info( &lp->hcfCtx, (LTVP)&( lp->ltvRecord ));

	
	lp->ltvRecord.len       = 3;
	lp->ltvRecord.typ       = CFG_BASIC_RATE_SET_CNTL; 
	lp->ltvRecord.u.u16[0]  = CNV_INT_TO_LITTLE( lp->brsc[0] );
	lp->ltvRecord.u.u16[1]  = CNV_INT_TO_LITTLE( lp->brsc[1] );
	hcf_status = hcf_put_info( &lp->hcfCtx, (LTVP)&( lp->ltvRecord ));

	
	

#endif 

#ifdef WARP
	
	lp->ltvRecord.len       = 3;
	lp->ltvRecord.typ       = CFG_CNF_MCAST_RATE;
	lp->ltvRecord.u.u16[0]  = CNV_INT_TO_LITTLE( lp->MulticastRate[0] );
	lp->ltvRecord.u.u16[1]  = CNV_INT_TO_LITTLE( lp->MulticastRate[1] );
#else
	lp->ltvRecord.len       = 2;
	lp->ltvRecord.typ       = CFG_CNF_MCAST_RATE;
	lp->ltvRecord.u.u16[0]  = CNV_INT_TO_LITTLE( lp->MulticastRate[0] );
#endif 
	hcf_status = hcf_put_info( &lp->hcfCtx, (LTVP)&( lp->ltvRecord ));

	
	if (( len = ( strlen( lp->StationName ) + 1 ) & ~0x01 ) != 0 ) {
		
		

		lp->ltvRecord.len       = 2 + ( len / sizeof( hcf_16 ));
		lp->ltvRecord.typ       = CFG_CNF_OWN_NAME;
		lp->ltvRecord.u.u16[0]  = CNV_INT_TO_LITTLE( strlen( lp->StationName ));

		memcpy( &( lp->ltvRecord.u.u8[2] ), lp->StationName, len );
	} else {
		

		lp->ltvRecord.len       = 2;
		lp->ltvRecord.typ       = CFG_CNF_OWN_NAME;
		lp->ltvRecord.u.u16[0]  = CNV_INT_TO_LITTLE( 0 );
	}

	hcf_status = hcf_put_info( &lp->hcfCtx, (LTVP)&( lp->ltvRecord ));

	
	

	
	if ( CNV_INT_TO_LITTLE( lp->hcfCtx.IFB_FWIdentity.comp_id ) == COMP_ID_FW_STA  ) {

		
		lp->ltvRecord.len       = 2;
		lp->ltvRecord.typ       = CFG_RTS_THRH;
		lp->ltvRecord.u.u16[0]  = CNV_INT_TO_LITTLE( lp->RTSThreshold );
		hcf_status = hcf_put_info( &lp->hcfCtx, (LTVP)&( lp->ltvRecord ));

		
		lp->ltvRecord.len       = 2;
		lp->ltvRecord.typ       = CFG_CNF_PORT_TYPE;
		lp->ltvRecord.u.u16[0]  = CNV_INT_TO_LITTLE( lp->PortType );
		hcf_status = hcf_put_info( &lp->hcfCtx, (LTVP)&( lp->ltvRecord ));

		
#ifdef WARP
		lp->ltvRecord.len       = 3;
		lp->ltvRecord.typ       = CFG_TX_RATE_CNTL;
		lp->ltvRecord.u.u16[0]  = CNV_INT_TO_LITTLE( lp->TxRateControl[0] );
		lp->ltvRecord.u.u16[1]  = CNV_INT_TO_LITTLE( lp->TxRateControl[1] );
#else
		lp->ltvRecord.len       = 2;
		lp->ltvRecord.typ       = CFG_TX_RATE_CNTL;
		lp->ltvRecord.u.u16[0]  = CNV_INT_TO_LITTLE( lp->TxRateControl[0] );
#endif  


		DBG_TRACE( DbgInfo, "CFG_TX_RATE_CNTL 2.4GHz           : 0x%04x\n",
				   lp->TxRateControl[0] );
		DBG_TRACE( DbgInfo, "CFG_TX_RATE_CNTL 5.0GHz           : 0x%04x\n",
				   lp->TxRateControl[1] );
		DBG_TRACE( DbgInfo, "CFG_TX_RATE_CNTL result           : 0x%04x\n",
				   hcf_status );
		
		lp->ltvRecord.len       = 2;
		lp->ltvRecord.typ       = CFG_CNF_PM_ENABLED;
		lp->ltvRecord.u.u16[0]  = CNV_INT_TO_LITTLE( lp->PMEnabled );
		DBG_TRACE( DbgInfo, "CFG_CNF_PM_ENABLED                : 0x%04x\n", lp->PMEnabled );
		hcf_status = hcf_put_info( &lp->hcfCtx, (LTVP)&( lp->ltvRecord ));
		
		lp->ltvRecord.len       = 2;
		lp->ltvRecord.typ       = CFG_CNF_MCAST_RX;
		lp->ltvRecord.u.u16[0]  = CNV_INT_TO_LITTLE( lp->MulticastReceive );
		hcf_status = hcf_put_info( &lp->hcfCtx, (LTVP)&( lp->ltvRecord ));

		
		lp->ltvRecord.len       = 2;
		lp->ltvRecord.typ       = CFG_CNF_MAX_SLEEP_DURATION;
		lp->ltvRecord.u.u16[0]  = CNV_INT_TO_LITTLE( lp->MaxSleepDuration );
		hcf_status = hcf_put_info( &lp->hcfCtx, (LTVP)&( lp->ltvRecord ));

		
		lp->ltvRecord.len       = 2;
		lp->ltvRecord.typ       = CFG_CREATE_IBSS;
		lp->ltvRecord.u.u16[0]  = CNV_INT_TO_LITTLE( lp->CreateIBSS );
		hcf_status = hcf_put_info( &lp->hcfCtx, (LTVP)&( lp->ltvRecord ));

		
		if ((( len = ( strlen( lp->NetworkName ) + 1 ) & ~0x01 ) != 0 ) &&
			 ( strcmp( lp->NetworkName, "ANY" ) != 0 ) &&
			 ( strcmp( lp->NetworkName, "any" ) != 0 )) {
			
			

			lp->ltvRecord.len       = 2 + (len / sizeof(hcf_16));
			lp->ltvRecord.typ       = CFG_DESIRED_SSID;
			lp->ltvRecord.u.u16[0]  = CNV_INT_TO_LITTLE( strlen( lp->NetworkName ));

			memcpy( &( lp->ltvRecord.u.u8[2] ), lp->NetworkName, len );
		} else {
			

			lp->ltvRecord.len       = 2;
			lp->ltvRecord.typ       = CFG_DESIRED_SSID;
			lp->ltvRecord.u.u16[0]  = CNV_INT_TO_LITTLE( 0 );
		}

		hcf_status = hcf_put_info( &lp->hcfCtx, (LTVP)&( lp->ltvRecord ));

		
		
		
		lp->ltvRecord.len       = 2;
		lp->ltvRecord.typ       = CFG_CNF_OWN_ATIM_WINDOW;
		lp->ltvRecord.u.u16[0]  = CNV_INT_TO_LITTLE( lp->atimWindow );
		hcf_status = hcf_put_info( &lp->hcfCtx, (LTVP)&( lp->ltvRecord ));


		
		lp->ltvRecord.len       = 2;
		lp->ltvRecord.typ       = CFG_CNF_HOLDOVER_DURATION;
		lp->ltvRecord.u.u16[0]  = CNV_INT_TO_LITTLE( lp->holdoverDuration );
		hcf_status = hcf_put_info( &lp->hcfCtx, (LTVP)&( lp->ltvRecord ));

		
		lp->ltvRecord.len       = 2;
		lp->ltvRecord.typ       = CFG_PROMISCUOUS_MODE;
		lp->ltvRecord.u.u16[0]  = CNV_INT_TO_LITTLE( lp->promiscuousMode );
		hcf_status = hcf_put_info( &lp->hcfCtx, (LTVP)&( lp->ltvRecord ));

		
		lp->ltvRecord.len       = 2;
		lp->ltvRecord.typ       = CFG_CNF_AUTHENTICATION;
		lp->ltvRecord.u.u16[0]  = CNV_INT_TO_LITTLE( lp->authentication );
		hcf_status = hcf_put_info( &lp->hcfCtx, (LTVP)&( lp->ltvRecord ));
#ifdef WARP
		
		lp->ltvRecord.len       = 2;
		lp->ltvRecord.typ       = CFG_CNF_CONNECTION_CNTL;
		lp->ltvRecord.u.u16[0]  = CNV_INT_TO_LITTLE( lp->connectionControl );
		hcf_status = hcf_put_info( &lp->hcfCtx, (LTVP)&( lp->ltvRecord ));



		
#endif 
	} else {
		
#if 0 
		

		
		lp->ltvRecord.len       = 2;
		lp->ltvRecord.typ       = CFG_CNF_OWN_DTIM_PERIOD;
		lp->ltvRecord.u.u16[0]  = CNV_INT_TO_LITTLE( lp->DTIMPeriod );
		hcf_status = hcf_put_info( &lp->hcfCtx, (LTVP)&( lp->ltvRecord ));

		
		lp->ltvRecord.len       = 2;
		lp->ltvRecord.typ       = CFG_CNF_MCAST_PM_BUF;
		lp->ltvRecord.u.u16[0]  = CNV_INT_TO_LITTLE( lp->multicastPMBuffering );
		hcf_status = hcf_put_info( &lp->hcfCtx, (LTVP)&( lp->ltvRecord ));

		
		lp->ltvRecord.len       = 2;
		lp->ltvRecord.typ       = CFG_CNF_REJECT_ANY;
		lp->ltvRecord.u.u16[0]  = CNV_INT_TO_LITTLE( lp->RejectAny );

		hcf_status = hcf_put_info( &lp->hcfCtx, (LTVP)&( lp->ltvRecord ));

		
		lp->ltvRecord.len       = 2;
		lp->ltvRecord.typ       = CFG_CNF_EXCL_UNENCRYPTED;
		lp->ltvRecord.u.u16[0]  = CNV_INT_TO_LITTLE( lp->ExcludeUnencrypted );

		hcf_status = hcf_put_info( &lp->hcfCtx, (LTVP)&( lp->ltvRecord ));

		
		lp->ltvRecord.len       = 2;
		lp->ltvRecord.typ       = CFG_CNF_INTRA_BSS_RELAY;
		lp->ltvRecord.u.u16[0]  = CNV_INT_TO_LITTLE( lp->intraBSSRelay );
		hcf_status = hcf_put_info( &lp->hcfCtx, (LTVP)&( lp->ltvRecord ));

		
		lp->ltvRecord.len       = 2;
		lp->ltvRecord.typ       = CFG_RTS_THRH0;
		lp->ltvRecord.u.u16[0]  = CNV_INT_TO_LITTLE( lp->RTSThreshold );

		hcf_status = hcf_put_info( &lp->hcfCtx, (LTVP)&( lp->ltvRecord ));

		
#ifdef WARP
		lp->ltvRecord.len       = 3;
		lp->ltvRecord.typ       = CFG_TX_RATE_CNTL0;
		lp->ltvRecord.u.u16[0]  = CNV_INT_TO_LITTLE( lp->TxRateControl[0] );
		lp->ltvRecord.u.u16[1]  = CNV_INT_TO_LITTLE( lp->TxRateControl[1] );
#else
		lp->ltvRecord.len       = 2;
		lp->ltvRecord.typ       = CFG_TX_RATE_CNTL0;
		lp->ltvRecord.u.u16[0]  = CNV_INT_TO_LITTLE( lp->TxRateControl[0] );
#endif  

		hcf_status = hcf_put_info( &lp->hcfCtx, (LTVP)&( lp->ltvRecord ));

		
		lp->ltvRecord.len       = 2;
		lp->ltvRecord.typ       = 0xFC31;
		lp->ltvRecord.u.u16[0]  = CNV_INT_TO_LITTLE( lp->ownBeaconInterval );
		hcf_status = hcf_put_info( &lp->hcfCtx, (LTVP)&( lp->ltvRecord ));

		
		lp->ltvRecord.len       = 2;
		lp->ltvRecord.typ       = 0xFCC7;
		lp->ltvRecord.u.u16[0]  = CNV_INT_TO_LITTLE( lp->coexistence );
		hcf_status = hcf_put_info( &lp->hcfCtx, (LTVP)&( lp->ltvRecord ));

#ifdef USE_WDS

		
		lp->ltvRecord.len       = 2;
		lp->ltvRecord.typ       = CFG_RTS_THRH1;
		lp->ltvRecord.u.u16[0]  = CNV_INT_TO_LITTLE( lp->wds_port[0].rtsThreshold );
		hcf_status = hcf_put_info( &lp->hcfCtx, (LTVP)&( lp->ltvRecord ));

		
		lp->ltvRecord.len       = 2;
		lp->ltvRecord.typ       = CFG_RTS_THRH2;
		lp->ltvRecord.u.u16[0]  = CNV_INT_TO_LITTLE( lp->wds_port[1].rtsThreshold );
		hcf_status = hcf_put_info( &lp->hcfCtx, (LTVP)&( lp->ltvRecord ));


		
		lp->ltvRecord.len       = 2;
		lp->ltvRecord.typ       = CFG_RTS_THRH3;
		lp->ltvRecord.u.u16[0]  = CNV_INT_TO_LITTLE( lp->wds_port[2].rtsThreshold );
		hcf_status = hcf_put_info( &lp->hcfCtx, (LTVP)&( lp->ltvRecord ));


		
		lp->ltvRecord.len       = 2;
		lp->ltvRecord.typ       = CFG_RTS_THRH4;
		lp->ltvRecord.u.u16[0]  = CNV_INT_TO_LITTLE( lp->wds_port[3].rtsThreshold );
		hcf_status = hcf_put_info( &lp->hcfCtx, (LTVP)&( lp->ltvRecord ));


		
		lp->ltvRecord.len       = 2;
		lp->ltvRecord.typ       = CFG_RTS_THRH5;
		lp->ltvRecord.u.u16[0]  = CNV_INT_TO_LITTLE( lp->wds_port[4].rtsThreshold );
		hcf_status = hcf_put_info( &lp->hcfCtx, (LTVP)&( lp->ltvRecord ));

		
		lp->ltvRecord.len       = 2;
		lp->ltvRecord.typ       = CFG_RTS_THRH6;
		lp->ltvRecord.u.u16[0]  = CNV_INT_TO_LITTLE( lp->wds_port[5].rtsThreshold );
		hcf_status = hcf_put_info( &lp->hcfCtx, (LTVP)&( lp->ltvRecord ));
#if 0
		
		lp->ltvRecord.len       = 2;
		lp->ltvRecord.typ       = CFG_TX_RATE_CNTL1;
		lp->ltvRecord.u.u16[0]  = CNV_INT_TO_LITTLE( lp->wds_port[0].txRateCntl );
		hcf_status = hcf_put_info( &lp->hcfCtx, (LTVP)&( lp->ltvRecord ));

		
		lp->ltvRecord.len       = 2;
		lp->ltvRecord.typ       = CFG_TX_RATE_CNTL2;
		lp->ltvRecord.u.u16[0]  = CNV_INT_TO_LITTLE( lp->wds_port[1].txRateCntl );
		hcf_status = hcf_put_info( &lp->hcfCtx, (LTVP)&( lp->ltvRecord ));

		
		lp->ltvRecord.len       = 2;
		lp->ltvRecord.typ       = CFG_TX_RATE_CNTL3;
		lp->ltvRecord.u.u16[0]  = CNV_INT_TO_LITTLE( lp->wds_port[2].txRateCntl );
		hcf_status = hcf_put_info( &lp->hcfCtx, (LTVP)&( lp->ltvRecord ));

		
		lp->ltvRecord.len       = 2;
		lp->ltvRecord.typ       = CFG_TX_RATE_CNTL4;
		lp->ltvRecord.u.u16[0]  = CNV_INT_TO_LITTLE( lp->wds_port[3].txRateCntl );
		hcf_status = hcf_put_info( &lp->hcfCtx, (LTVP)&( lp->ltvRecord ));

		
		lp->ltvRecord.len       = 2;
		lp->ltvRecord.typ       = CFG_TX_RATE_CNTL5;
		lp->ltvRecord.u.u16[0]  = CNV_INT_TO_LITTLE( lp->wds_port[4].txRateCntl );
		hcf_status = hcf_put_info( &lp->hcfCtx, (LTVP)&( lp->ltvRecord ));

		
		lp->ltvRecord.len       = 2;
		lp->ltvRecord.typ       = CFG_TX_RATE_CNTL6;
		lp->ltvRecord.u.u16[0]  = CNV_INT_TO_LITTLE( lp->wds_port[5].txRateCntl );
		hcf_status = hcf_put_info( &lp->hcfCtx, (LTVP)&( lp->ltvRecord ));

#endif


		
		lp->ltvRecord.len      = 4;
		lp->ltvRecord.typ      = CFG_CNF_WDS_ADDR1;

		memcpy( &lp->ltvRecord.u.u8[0], lp->wds_port[0].wdsAddress, ETH_ALEN );
		hcf_status = hcf_put_info( &lp->hcfCtx, (LTVP)&( lp->ltvRecord ));

		
		lp->ltvRecord.len      = 4;
		lp->ltvRecord.typ      = CFG_CNF_WDS_ADDR2;

		memcpy( &lp->ltvRecord.u.u8[0], lp->wds_port[1].wdsAddress, ETH_ALEN );
		hcf_status = hcf_put_info( &lp->hcfCtx, (LTVP)&( lp->ltvRecord ));

		
		lp->ltvRecord.len      = 4;
		lp->ltvRecord.typ      = CFG_CNF_WDS_ADDR3;

		memcpy( &lp->ltvRecord.u.u8[0], lp->wds_port[2].wdsAddress, ETH_ALEN );
		hcf_status = hcf_put_info( &lp->hcfCtx, (LTVP)&( lp->ltvRecord ));

		
		lp->ltvRecord.len      = 4;
		lp->ltvRecord.typ      = CFG_CNF_WDS_ADDR4;

		memcpy( &lp->ltvRecord.u.u8[0], lp->wds_port[3].wdsAddress, ETH_ALEN );
		hcf_status = hcf_put_info( &lp->hcfCtx, (LTVP)&( lp->ltvRecord ));

		
		lp->ltvRecord.len      = 4;
		lp->ltvRecord.typ      = CFG_CNF_WDS_ADDR5;

		memcpy( &lp->ltvRecord.u.u8[0], lp->wds_port[4].wdsAddress, ETH_ALEN );
		hcf_status = hcf_put_info( &lp->hcfCtx, (LTVP)&( lp->ltvRecord ));

		
		lp->ltvRecord.len      = 4;
		lp->ltvRecord.typ      = CFG_CNF_WDS_ADDR6;

		memcpy( &lp->ltvRecord.u.u8[0], lp->wds_port[5].wdsAddress, ETH_ALEN );
		hcf_status = hcf_put_info( &lp->hcfCtx, (LTVP)&( lp->ltvRecord ));
#endif  
#endif  
	}

	

	if ( WVLAN_VALID_MAC_ADDRESS( lp->MACAddress )) {
		
		

		lp->ltvRecord.len = 1 + ( ETH_ALEN / sizeof( hcf_16 ));
		if ( CNV_INT_TO_LITTLE( lp->hcfCtx.IFB_FWIdentity.comp_id ) == COMP_ID_FW_AP  ) {
			
			lp->ltvRecord.typ = CFG_NIC_MAC_ADDR;
		} else {
			
			lp->ltvRecord.typ = CFG_CNF_OWN_MAC_ADDR;
		}
		
		memcpy( &( lp->ltvRecord.u.u8[0] ), lp->MACAddress, ETH_ALEN );
		hcf_status = hcf_put_info( &lp->hcfCtx, (LTVP)&( lp->ltvRecord ));
		
		

		
		memcpy( lp->dev->dev_addr, lp->MACAddress, ETH_ALEN ); 
	}
	
	if ((( len = ( strlen( lp->NetworkName ) + 1 ) & ~0x01 ) != 0 ) &&
				 ( strcmp( lp->NetworkName, "ANY" ) != 0 ) &&
				 ( strcmp( lp->NetworkName, "any" ) != 0 )) {
		
		
		lp->ltvRecord.len       = 2 + (len / sizeof(hcf_16));
		lp->ltvRecord.typ       = CFG_CNF_OWN_SSID;
		lp->ltvRecord.u.u16[0]  = CNV_INT_TO_LITTLE( strlen( lp->NetworkName ));

		memcpy( &( lp->ltvRecord.u.u8[2] ), lp->NetworkName, len );
	} else {
		
		lp->ltvRecord.len       = 2;
		lp->ltvRecord.typ       = CFG_CNF_OWN_SSID;
		lp->ltvRecord.u.u16[0]  = CNV_INT_TO_LITTLE( 0 );
	}

	hcf_status = hcf_put_info( &lp->hcfCtx, (LTVP)&( lp->ltvRecord ));

	
	
	
	lp->ltvRecord.len       = 2;
	lp->ltvRecord.typ       = CFG_CNF_ENCRYPTION;
	lp->ltvRecord.u.u16[0]  = CNV_INT_TO_LITTLE( lp->EnableEncryption );
	hcf_status = hcf_put_info( &lp->hcfCtx, (LTVP)&( lp->ltvRecord ));

	
	lp->ltvRecord.len       = 2;
	lp->ltvRecord.typ       = CFG_SET_WPA_AUTH_KEY_MGMT_SUITE;
	lp->ltvRecord.u.u16[0]  = CNV_INT_TO_LITTLE( lp->AuthKeyMgmtSuite );
	hcf_status = hcf_put_info( &lp->hcfCtx, (LTVP)&( lp->ltvRecord ));

	
	if (lp->wext_enc != IW_ENCODE_ALG_TKIP)
		wl_set_wep_keys(lp);

	
	

	DBG_LEAVE( DbgInfo );
	return hcf_status;
} 


static int __init wl_module_init( void )
{
	int result;
	

	DBG_FUNC( "wl_module_init" );

#if DBG
	DbgInfo->DebugFlag = 0;
	DbgInfo->DebugFlag = DBG_TRACE_ON;		
	if ( pc_debug ) switch( pc_debug ) {
	  case 8:
		DbgInfo->DebugFlag |= DBG_DS_ON;
	  case 7:
		DbgInfo->DebugFlag |= DBG_RX_ON | DBG_TX_ON;
	  case 6:
		DbgInfo->DebugFlag |= DBG_PARAM_ON;
	  case 5:
		DbgInfo->DebugFlag |= DBG_TRACE_ON;
	  case 4:
		DbgInfo->DebugFlag |= DBG_VERBOSE_ON;
	  case 1:
		DbgInfo->DebugFlag |= DBG_DEFAULTS;
	  default:
		break;
	}
#endif 

	DBG_ENTER( DbgInfo );
	printk(KERN_INFO "%s\n", VERSION_INFO);
    	printk(KERN_INFO "*** Modified for kernel 2.6 by Henk de Groot <pe1dnn@amsat.org>\n");
        printk(KERN_INFO "*** Based on 7.18 version by Andrey Borzenkov <arvidjaar@mail.ru> $Revision: 39 $\n");



	result = wl_adapter_init_module( );
	DBG_LEAVE( DbgInfo );
	return result;
} 


static void __exit wl_module_exit( void )
{
	DBG_FUNC( "wl_module_exit" );
	DBG_ENTER(DbgInfo);

	wl_adapter_cleanup_module( );
#if 0 
	remove_proc_entry( "wlags", NULL );		
#endif

	DBG_LEAVE( DbgInfo );
	return;
} 

module_init(wl_module_init);
module_exit(wl_module_exit);

irqreturn_t wl_isr( int irq, void *dev_id, struct pt_regs *regs )
{
	int                 events;
	struct net_device   *dev = (struct net_device *) dev_id;
	struct wl_private   *lp = NULL;
	
	if (( dev == NULL ) || ( !netif_device_present( dev ))) {
		return IRQ_NONE;
	}

	
	lp = wl_priv(dev);

#ifdef USE_RTS
	if ( lp->useRTS == 1 ) {
		DBG_PRINT( "EXITING ISR, IN RTS MODE...\n" );
		return;
		}
#endif  

	events = hcf_action( &lp->hcfCtx, HCF_ACT_INT_OFF );

	if ( events == HCF_INT_PENDING ) {
		tasklet_schedule(&lp->task);
	} else {
		
		hcf_action( &lp->hcfCtx, HCF_ACT_INT_ON );
	}

	return IRQ_RETVAL(events == HCF_INT_PENDING);
} 


#define WVLAN_MAX_INT_SERVICES  50

void wl_isr_handler( unsigned long p )
{
	struct net_device       *dev;
	unsigned long           flags;
	bool_t                  stop = TRUE;
	int                     count;
	int                     result;
        struct wl_private       *lp = (struct wl_private *)p;
	

	if ( lp == NULL ) {
		DBG_PRINT( "wl_isr_handler  lp adapter pointer is NULL!!!\n" );
	} else {
		wl_lock( lp, &flags );

		dev = (struct net_device *)lp->dev;
		if ( dev != NULL && netif_device_present( dev ) ) stop = FALSE;
		for( count = 0; stop == FALSE && count < WVLAN_MAX_INT_SERVICES; count++ ) {
			stop = TRUE;
			result = hcf_service_nic( &lp->hcfCtx,
									  (wci_bufp)lp->lookAheadBuf,
									  sizeof( lp->lookAheadBuf ));
			if ( result == HCF_ERR_MIC ) {
				wl_wext_event_mic_failed( dev ); 	
				
				
			}

#ifndef USE_MBOX_SYNC
			if ( lp->hcfCtx.IFB_MBInfoLen != 0 ) {	
				wl_mbx( lp );
				stop = FALSE;
			}
#endif
			
			if ( ( lp->hcfCtx.IFB_LinkStat & CFG_LINK_STAT_FW ) != 0 ) {
				wl_process_link_status( lp );
				stop = FALSE;
			}
			
			if ( lp->ProbeResp.infoType != 0 &&
				lp->ProbeResp.infoType != 0xFFFF ) {
				wl_process_probe_response( lp );
				memset( &lp->ProbeResp, 0, sizeof( lp->ProbeResp ));
				lp->ProbeResp.infoType = 0xFFFF;
				stop = FALSE;
			}
			
			if ( lp->updatedRecord.len != 0xFFFF ) {
				wl_process_updated_record( lp );
				lp->updatedRecord.len = 0xFFFF;
				stop = FALSE;
			}
			
			if ( lp->assoc_stat.len != 0xFFFF ) {
				wl_process_assoc_status( lp );
				lp->assoc_stat.len = 0xFFFF;
				stop = FALSE;
			}
			
			if ( lp->sec_stat.len != 0xFFFF ) {
				wl_process_security_status( lp );
				lp->sec_stat.len = 0xFFFF;
				stop = FALSE;
			}

#ifdef ENABLE_DMA
			if ( lp->use_dma ) {
				
				if ( lp->hcfCtx.IFB_DmaPackets & HREG_EV_RDMAD ) {
					wl_rx_dma( dev );
					stop = FALSE;
				}
				
				if ( lp->hcfCtx.IFB_DmaPackets & HREG_EV_TDMAD ) {
					wl_pci_dma_hcf_reclaim_tx( lp );
					stop = FALSE;
				}
			}
			else
#endif 
			{
				
				if ( lp->hcfCtx.IFB_RxLen != 0 ) {
					wl_rx( dev );
					stop = FALSE;
				}
				
				if ( wl_send( lp )) {
					stop = FALSE;
				}
			}
		}
		
		hcf_action( &lp->hcfCtx, HCF_ACT_INT_ON );
		wl_unlock( lp, &flags );
	}
	return;
} 


void wl_remove( struct net_device *dev )
{
	struct wl_private   *lp = wl_priv(dev);
	unsigned long   flags;
	
	DBG_FUNC( "wl_remove" );
	DBG_ENTER( DbgInfo );

	DBG_PARAM( DbgInfo, "dev", "%s (0x%p)", dev->name, dev );

	wl_lock( lp, &flags );

	
	wl_act_int_off( lp );
        lp->is_handling_int = WL_NOT_HANDLING_INT;

	
	lp->portState = WVLAN_PORT_STATE_DISABLED;

#if 0 
#ifdef USE_WDS
	
#endif 
#endif  

	
	lp->is_registered = FALSE;

	
	WL_WDS_NETDEV_DEREGISTER( lp );
#ifdef USE_RTS
	if ( lp->useRTS == 1 ) {
		wl_unlock( lp, &flags );

		DBG_LEAVE( DbgInfo );
		return;
	}
#endif  

	
	hcf_connect( &lp->hcfCtx, HCF_DISCONNECT );

	wl_unlock( lp, &flags );

	DBG_LEAVE( DbgInfo );
	return;
} 


void wl_suspend( struct net_device *dev )
{
	struct wl_private  *lp = wl_priv(dev);
	unsigned long   flags;
	
	DBG_FUNC( "wl_suspend" );
	DBG_ENTER( DbgInfo );

	DBG_PARAM( DbgInfo, "dev", "%s (0x%p)", dev->name, dev );

	wl_lock( lp, &flags );

	
	wl_act_int_off( lp );

	
	wl_disconnect( lp );

	
	wl_disable( lp );

        
	hcf_connect( &lp->hcfCtx, HCF_DISCONNECT );

	
	lp->portState = WVLAN_PORT_STATE_DISABLED;

	wl_unlock( lp, &flags );

	DBG_LEAVE( DbgInfo );
	return;
} 


void wl_resume(struct net_device *dev)
{
	struct wl_private  *lp = wl_priv(dev);
	unsigned long   flags;
	
	DBG_FUNC( "wl_resume" );
	DBG_ENTER( DbgInfo );

	DBG_PARAM( DbgInfo, "dev", "%s (0x%p)", dev->name, dev );

	wl_lock( lp, &flags );

        
	hcf_connect( &lp->hcfCtx, dev->base_addr );

	
	lp->portState = WVLAN_PORT_STATE_DISABLED;

	
	lp->firmware_present = WL_FRIMWARE_NOT_PRESENT;

	
	wl_reset( dev );

	
	wl_act_int_on( lp );

	wl_unlock( lp, &flags );

	DBG_LEAVE( DbgInfo );
	return;
} 


void wl_release( struct net_device *dev )
{
	struct wl_private  *lp = wl_priv(dev);
	
	DBG_FUNC( "wl_release" );
	DBG_ENTER( DbgInfo );

	DBG_PARAM( DbgInfo, "dev", "%s (0x%p)", dev->name, dev );
	if ( lp->is_registered == TRUE ) {
		DBG_TRACE( DbgInfo, "Calling unregister_netdev(), as it wasn't called yet\n" );
		wl_remove( dev );

		lp->is_registered = FALSE;
	}

	DBG_LEAVE( DbgInfo );
	return;
} 


p_u16 wl_get_irq_mask( void )
{
	return irq_mask;
} 


p_s8 * wl_get_irq_list( void )
{
	return irq_list;
} 



int wl_enable( struct wl_private *lp )
{
	int hcf_status = HCF_SUCCESS;
	
	DBG_FUNC( "wl_enable" );
	DBG_ENTER( DbgInfo );

	if ( lp->portState == WVLAN_PORT_STATE_ENABLED ) {
		DBG_TRACE( DbgInfo, "No action: Card already enabled\n" );
	} else if ( lp->portState == WVLAN_PORT_STATE_CONNECTED ) {
		
		DBG_TRACE( DbgInfo, "No action: Card already connected\n" );
	} else {
		hcf_status = hcf_cntl( &lp->hcfCtx, HCF_CNTL_ENABLE );
		if ( hcf_status == HCF_SUCCESS ) {
			
			lp->portState = WVLAN_PORT_STATE_ENABLED;   
#ifdef ENABLE_DMA
			if ( lp->use_dma ) {
				wl_pci_dma_hcf_supply( lp );  
			}
#endif
		}
	}
	if ( hcf_status != HCF_SUCCESS ) {  
		DBG_TRACE( DbgInfo, "failed: 0x%x\n", hcf_status );
	}
	DBG_LEAVE( DbgInfo );
	return hcf_status;
} 


#ifdef USE_WDS
void wl_enable_wds_ports( struct wl_private * lp )
{

	DBG_FUNC( "wl_enable_wds_ports" );
	DBG_ENTER( DbgInfo );
	if ( CNV_INT_TO_LITTLE( lp->hcfCtx.IFB_FWIdentity.comp_id ) == COMP_ID_FW_AP  ){
		DBG_ERROR( DbgInfo, "!!!!;? someone misunderstood something !!!!!\n" );
	}
	DBG_LEAVE( DbgInfo );
	return;
} 
#endif  


int wl_connect( struct wl_private *lp )
{
	int hcf_status;
	

	DBG_FUNC( "wl_connect" );
	DBG_ENTER( DbgInfo );

	if ( lp->portState != WVLAN_PORT_STATE_ENABLED ) {
		DBG_TRACE( DbgInfo, "No action: Not in enabled state\n" );
		DBG_LEAVE( DbgInfo );
		return HCF_SUCCESS;
	}
	hcf_status = hcf_cntl( &lp->hcfCtx, HCF_CNTL_CONNECT );
	if ( hcf_status == HCF_SUCCESS ) {
		lp->portState = WVLAN_PORT_STATE_CONNECTED;
	}
	DBG_LEAVE( DbgInfo );
	return hcf_status;
} 


int wl_disconnect( struct wl_private *lp )
{
	int hcf_status;
	

	DBG_FUNC( "wl_disconnect" );
	DBG_ENTER( DbgInfo );

	if ( lp->portState != WVLAN_PORT_STATE_CONNECTED ) {
		DBG_TRACE( DbgInfo, "No action: Not in connected state\n" );
		DBG_LEAVE( DbgInfo );
		return HCF_SUCCESS;
	}
	hcf_status = hcf_cntl( &lp->hcfCtx, HCF_CNTL_DISCONNECT );
	if ( hcf_status == HCF_SUCCESS ) {
		lp->portState = WVLAN_PORT_STATE_ENABLED;
	}
	DBG_LEAVE( DbgInfo );
	return hcf_status;
} 


int wl_disable( struct wl_private *lp )
{
	int hcf_status = HCF_SUCCESS;
	
	DBG_FUNC( "wl_disable" );
	DBG_ENTER( DbgInfo );

	if ( lp->portState == WVLAN_PORT_STATE_DISABLED ) {
		DBG_TRACE( DbgInfo, "No action: Port state is disabled\n" );
	} else {
		hcf_status = hcf_cntl( &lp->hcfCtx, HCF_CNTL_DISABLE );
		if ( hcf_status == HCF_SUCCESS ) {
			 
			lp->portState = WVLAN_PORT_STATE_DISABLED;

#ifdef ENABLE_DMA
			if ( lp->use_dma ) {
				wl_pci_dma_hcf_reclaim( lp );
			}
#endif
		}
	}
	if ( hcf_status != HCF_SUCCESS ) {
		DBG_TRACE( DbgInfo, "failed: 0x%x\n", hcf_status );
	}
	DBG_LEAVE( DbgInfo );
	return hcf_status;
} 


#ifdef USE_WDS
void wl_disable_wds_ports( struct wl_private * lp )
{

	DBG_FUNC( "wl_disable_wds_ports" );
	DBG_ENTER( DbgInfo );

	if ( CNV_INT_TO_LITTLE( lp->hcfCtx.IFB_FWIdentity.comp_id ) == COMP_ID_FW_AP  ){
		DBG_ERROR( DbgInfo, "!!!!;? someone misunderstood something !!!!!\n" );
	}
	DBG_LEAVE( DbgInfo );
	return;
} 
#endif 


#ifndef USE_MBOX_SYNC
int wl_mbx( struct wl_private *lp )
{
	int hcf_status = HCF_SUCCESS;
	
	DBG_FUNC( "wl_mbx" );
	DBG_ENTER( DbgInfo );
	DBG_TRACE( DbgInfo, "Mailbox Info: IFB_MBInfoLen: %d\n",
			   lp->hcfCtx.IFB_MBInfoLen );

	memset( &( lp->ltvRecord ), 0, sizeof( ltv_t ));

	lp->ltvRecord.len = MB_SIZE;
	lp->ltvRecord.typ = CFG_MB_INFO;
	hcf_status = hcf_get_info( &lp->hcfCtx, (LTVP)&( lp->ltvRecord ));

	if ( hcf_status != HCF_SUCCESS ) {
		DBG_ERROR( DbgInfo, "hcf_get_info returned 0x%x\n", hcf_status );

		DBG_LEAVE( DbgInfo );
		return hcf_status;
	}

	if ( lp->ltvRecord.typ == CFG_MB_INFO ) {
		DBG_LEAVE( DbgInfo );
		return hcf_status;
	}
	
	wl_endian_translate_mailbox( &( lp->ltvRecord ));
	wl_process_mailbox( lp );
	DBG_LEAVE( DbgInfo );
	return hcf_status;
} 


void wl_endian_translate_mailbox( ltv_t *ltv )
{

	DBG_FUNC( "wl_endian_translate_mailbox" );
	DBG_ENTER( DbgInfo );
	switch( ltv->typ ) {
	  case CFG_TALLIES:
		break;

	  case CFG_SCAN:
		{
			int num_aps;
			SCAN_RS_STRCT *aps = (SCAN_RS_STRCT *)&ltv->u.u8[0];

			num_aps = (hcf_16)(( (size_t)(ltv->len - 1 ) * 2 ) /
								 ( sizeof( SCAN_RS_STRCT )));

			while( num_aps >= 1 ) {
				num_aps--;

				aps[num_aps].channel_id =
					CNV_LITTLE_TO_INT( aps[num_aps].channel_id );

				aps[num_aps].noise_level =
					CNV_LITTLE_TO_INT( aps[num_aps].noise_level );

				aps[num_aps].signal_level =
					CNV_LITTLE_TO_INT( aps[num_aps].signal_level );

				aps[num_aps].beacon_interval_time =
					CNV_LITTLE_TO_INT( aps[num_aps].beacon_interval_time );

				aps[num_aps].capability =
					CNV_LITTLE_TO_INT( aps[num_aps].capability );

				aps[num_aps].ssid_len =
					CNV_LITTLE_TO_INT( aps[num_aps].ssid_len );

				aps[num_aps].ssid_val[aps[num_aps].ssid_len] = 0;
			}
		}
		break;

	  case CFG_ACS_SCAN:
		{
			PROBE_RESP *probe_resp = (PROBE_RESP *)ltv;

			probe_resp->frameControl   = CNV_LITTLE_TO_INT( probe_resp->frameControl );
			probe_resp->durID          = CNV_LITTLE_TO_INT( probe_resp->durID );
			probe_resp->sequence       = CNV_LITTLE_TO_INT( probe_resp->sequence );
			probe_resp->dataLength     = CNV_LITTLE_TO_INT( probe_resp->dataLength );
#ifndef WARP
			probe_resp->lenType        = CNV_LITTLE_TO_INT( probe_resp->lenType );
#endif 
			probe_resp->beaconInterval = CNV_LITTLE_TO_INT( probe_resp->beaconInterval );
			probe_resp->capability     = CNV_LITTLE_TO_INT( probe_resp->capability );
			probe_resp->flags          = CNV_LITTLE_TO_INT( probe_resp->flags );
		}
		break;

	  case CFG_LINK_STAT:
#define ls ((LINK_STATUS_STRCT *)ltv)
			ls->linkStatus = CNV_LITTLE_TO_INT( ls->linkStatus );
		break;
#undef ls

	  case CFG_ASSOC_STAT:
		{
			ASSOC_STATUS_STRCT *as = (ASSOC_STATUS_STRCT *)ltv;

			as->assocStatus = CNV_LITTLE_TO_INT( as->assocStatus );
		}
		break;

	  case CFG_SECURITY_STAT:
		{
			SECURITY_STATUS_STRCT *ss = (SECURITY_STATUS_STRCT *)ltv;

			ss->securityStatus  = CNV_LITTLE_TO_INT( ss->securityStatus );
			ss->reason          = CNV_LITTLE_TO_INT( ss->reason );
		}
		break;

	  case CFG_WMP:
		break;

	  case CFG_NULL:
		break;

	default:
		break;
	}

	DBG_LEAVE( DbgInfo );
	return;
} 

void wl_process_mailbox( struct wl_private *lp )
{
	ltv_t   *ltv;
	hcf_16  ltv_val = 0xFFFF;
	
	DBG_FUNC( "wl_process_mailbox" );
	DBG_ENTER( DbgInfo );
	ltv = &( lp->ltvRecord );

	switch( ltv->typ ) {

	  case CFG_TALLIES:
		DBG_TRACE( DbgInfo, "CFG_TALLIES\n" );
		break;
	  case CFG_SCAN:
		DBG_TRACE( DbgInfo, "CFG_SCAN\n" );

		{
			int num_aps;
			SCAN_RS_STRCT *aps = (SCAN_RS_STRCT *)&ltv->u.u8[0];

			num_aps = (hcf_16)(( (size_t)(ltv->len - 1 ) * 2 ) /
								 ( sizeof( SCAN_RS_STRCT )));

			lp->scan_results.num_aps = num_aps;

			DBG_TRACE( DbgInfo, "Number of APs: %d\n", num_aps );

			while( num_aps >= 1 ) {
				num_aps--;

				DBG_TRACE( DbgInfo, "AP              : %d\n", num_aps );
				DBG_TRACE( DbgInfo, "=========================\n" );
				DBG_TRACE( DbgInfo, "Channel ID      : 0x%04x\n",
						   aps[num_aps].channel_id );
				DBG_TRACE( DbgInfo, "Noise Level     : 0x%04x\n",
						   aps[num_aps].noise_level );
				DBG_TRACE( DbgInfo, "Signal Level    : 0x%04x\n",
						   aps[num_aps].signal_level );
				DBG_TRACE( DbgInfo, "Beacon Interval : 0x%04x\n",
						   aps[num_aps].beacon_interval_time );
				DBG_TRACE( DbgInfo, "Capability      : 0x%04x\n",
						   aps[num_aps].capability );
				DBG_TRACE( DbgInfo, "SSID Length     : 0x%04x\n",
						   aps[num_aps].ssid_len );
				DBG_TRACE(DbgInfo, "BSSID           : %pM\n",
						   aps[num_aps].bssid);

				if ( aps[num_aps].ssid_len != 0 ) {
					DBG_TRACE( DbgInfo, "SSID            : %s.\n",
							   aps[num_aps].ssid_val );
				} else {
					DBG_TRACE( DbgInfo, "SSID            : %s.\n", "ANY" );
				}

				DBG_TRACE( DbgInfo, "\n" );

				memcpy( &( lp->scan_results.APTable[num_aps]), &( aps[num_aps] ),
						sizeof( SCAN_RS_STRCT ));
			}

			lp->scan_results.scan_complete = TRUE;
		}

		break;
	  case CFG_ACS_SCAN:
		DBG_TRACE( DbgInfo, "CFG_ACS_SCAN\n" );

		{
			PROBE_RESP  *probe_rsp = (PROBE_RESP *)ltv;
			hcf_8       *wpa_ie = NULL;
			hcf_16      wpa_ie_len = 0;

			DBG_TRACE( DbgInfo, "(%s) =========================\n",
					   lp->dev->name );

			DBG_TRACE( DbgInfo, "(%s) length      : 0x%04x.\n",
					   lp->dev->name, probe_rsp->length );

			if ( probe_rsp->length > 1 ) {
				DBG_TRACE( DbgInfo, "(%s) infoType    : 0x%04x.\n",
						   lp->dev->name, probe_rsp->infoType );

				DBG_TRACE( DbgInfo, "(%s) signal      : 0x%02x.\n",
						   lp->dev->name, probe_rsp->signal );

				DBG_TRACE( DbgInfo, "(%s) silence     : 0x%02x.\n",
						   lp->dev->name, probe_rsp->silence );

				DBG_TRACE( DbgInfo, "(%s) rxFlow      : 0x%02x.\n",
						   lp->dev->name, probe_rsp->rxFlow );

				DBG_TRACE( DbgInfo, "(%s) rate        : 0x%02x.\n",
						   lp->dev->name, probe_rsp->rate );

				DBG_TRACE( DbgInfo, "(%s) frame cntl  : 0x%04x.\n",
						   lp->dev->name, probe_rsp->frameControl );

				DBG_TRACE( DbgInfo, "(%s) durID       : 0x%04x.\n",
						   lp->dev->name, probe_rsp->durID );

				DBG_TRACE(DbgInfo, "(%s) address1    : %pM\n",
					lp->dev->name, probe_rsp->address1);

				DBG_TRACE(DbgInfo, "(%s) address2    : %pM\n",
					lp->dev->name, probe_rsp->address2);

				DBG_TRACE(DbgInfo, "(%s) BSSID       : %pM\n",
					lp->dev->name, probe_rsp->BSSID);

				DBG_TRACE( DbgInfo, "(%s) sequence    : 0x%04x.\n",
						   lp->dev->name, probe_rsp->sequence );

				DBG_TRACE(DbgInfo, "(%s) address4    : %pM\n",
					lp->dev->name, probe_rsp->address4);

				DBG_TRACE( DbgInfo, "(%s) datalength  : 0x%04x.\n",
						   lp->dev->name, probe_rsp->dataLength );

				DBG_TRACE(DbgInfo, "(%s) DA          : %pM\n",
					lp->dev->name, probe_rsp->DA);

				DBG_TRACE(DbgInfo, "(%s) SA          : %pM\n",
					lp->dev->name, probe_rsp->SA);

				
				

				DBG_TRACE(DbgInfo, "(%s) timeStamp   : "
						"%d.%d.%d.%d.%d.%d.%d.%d\n",
						lp->dev->name,
						probe_rsp->timeStamp[0],
						probe_rsp->timeStamp[1],
						probe_rsp->timeStamp[2],
						probe_rsp->timeStamp[3],
						probe_rsp->timeStamp[4],
						probe_rsp->timeStamp[5],
						probe_rsp->timeStamp[6],
						probe_rsp->timeStamp[7]);

				DBG_TRACE( DbgInfo, "(%s) beaconInt   : 0x%04x.\n",
						   lp->dev->name, probe_rsp->beaconInterval );

				DBG_TRACE( DbgInfo, "(%s) capability  : 0x%04x.\n",
						   lp->dev->name, probe_rsp->capability );

				DBG_TRACE( DbgInfo, "(%s) SSID len    : 0x%04x.\n",
						   lp->dev->name, probe_rsp->rawData[1] );

				if ( probe_rsp->rawData[1] > 0 ) {
					char ssid[HCF_MAX_NAME_LEN];

					memset( ssid, 0, sizeof( ssid ));
					strncpy( ssid, &probe_rsp->rawData[2],
							 probe_rsp->rawData[1] );

					DBG_TRACE( DbgInfo, "(%s) SSID        : %s\n",
							   lp->dev->name, ssid );
				}

				
				wpa_ie = wl_parse_wpa_ie( probe_rsp, &wpa_ie_len );
				if ( wpa_ie != NULL ) {
					DBG_TRACE( DbgInfo, "(%s) WPA-IE      : %s\n",
					lp->dev->name, wl_print_wpa_ie( wpa_ie, wpa_ie_len ));
				}

				DBG_TRACE( DbgInfo, "(%s) flags       : 0x%04x.\n",
						   lp->dev->name, probe_rsp->flags );
			}

			DBG_TRACE( DbgInfo, "\n\n" );
			
			if ( probe_rsp->length == 1 ) {
				DBG_TRACE( DbgInfo, "SCAN COMPLETE\n" );
				lp->probe_results.num_aps = lp->probe_num_aps;
				lp->probe_results.scan_complete = TRUE;

				
				lp->probe_num_aps = 0;

				
				wl_wext_event_scan_complete( lp->dev );
			} else {
				if ( lp->probe_num_aps == 0 ) {
					memcpy( &( lp->probe_results.ProbeTable[lp->probe_num_aps] ),
							probe_rsp, sizeof( PROBE_RESP ));

					
					lp->probe_num_aps++;
				} else {
					int count;
					int unique = 1;

					for( count = 0; count < lp->probe_num_aps; count++ ) {
						if ( memcmp( &( probe_rsp->BSSID ),
							lp->probe_results.ProbeTable[count].BSSID,
							ETH_ALEN ) == 0 ) {
							unique = 0;
						}
					}

					if ( unique ) {
						if ( lp->probe_num_aps < MAX_NAPS )
						{
							memcpy( &( lp->probe_results.ProbeTable[lp->probe_num_aps] ),
									probe_rsp, sizeof( PROBE_RESP ));
						}
						else
						{
							DBG_WARNING( DbgInfo, "Num of scan results exceeds storage, truncating\n" );
						}

						lp->probe_num_aps++;
					}
				}
			}
		}

		break;

	  case CFG_LINK_STAT:
#define ls ((LINK_STATUS_STRCT *)ltv)
		DBG_TRACE( DbgInfo, "CFG_LINK_STAT\n" );

		switch( ls->linkStatus ) {
		  case 1:
			DBG_TRACE( DbgInfo, "Link Status : Connected\n" );
			wl_wext_event_ap( lp->dev );
			break;

		  case 2:
			DBG_TRACE( DbgInfo, "Link Status : Disconnected\n"  );
			break;

		  case 3:
			DBG_TRACE( DbgInfo, "Link Status : Access Point Change\n" );
			break;

		  case 4:
			DBG_TRACE( DbgInfo, "Link Status : Access Point Out of Range\n" );
			break;

		  case 5:
			DBG_TRACE( DbgInfo, "Link Status : Access Point In Range\n" );
			break;

		default:
			DBG_TRACE( DbgInfo, "Link Status : UNKNOWN (0x%04x)\n",
					   ls->linkStatus );
			break;
		}

		break;
#undef ls

	  case CFG_ASSOC_STAT:
		DBG_TRACE( DbgInfo, "CFG_ASSOC_STAT\n" );

		{
			ASSOC_STATUS_STRCT *as = (ASSOC_STATUS_STRCT *)ltv;

			switch( as->assocStatus ) {
			  case 1:
				DBG_TRACE( DbgInfo, "Association Status : STA Associated\n" );
				break;

			  case 2:
				DBG_TRACE( DbgInfo, "Association Status : STA Reassociated\n" );
				break;

			  case 3:
				DBG_TRACE( DbgInfo, "Association Status : STA Disassociated\n" );
				break;

			default:
				DBG_TRACE( DbgInfo, "Association Status : UNKNOWN (0x%04x)\n",
						   as->assocStatus );
				break;
			}

			DBG_TRACE(DbgInfo, "STA Address        : %pM\n",
					   as->staAddr);

			if (( as->assocStatus == 2 )  && ( as->len == 8 )) {
				DBG_TRACE(DbgInfo, "Old AP Address     : %pM\n",
						   as->oldApAddr);
			}
		}

		break;

	  case CFG_SECURITY_STAT:
		DBG_TRACE( DbgInfo, "CFG_SECURITY_STAT\n" );

		{
			SECURITY_STATUS_STRCT *ss = (SECURITY_STATUS_STRCT *)ltv;

			switch( ss->securityStatus ) {
			  case 1:
				DBG_TRACE( DbgInfo, "Security Status : Dissassociate [AP]\n" );
				break;

			  case 2:
				DBG_TRACE( DbgInfo, "Security Status : Deauthenticate [AP]\n" );
				break;

			  case 3:
				DBG_TRACE( DbgInfo, "Security Status : Authenticate Fail [STA] or [AP]\n" );
				break;

			  case 4:
				DBG_TRACE( DbgInfo, "Security Status : MIC Fail\n" );
				break;

			  case 5:
				DBG_TRACE( DbgInfo, "Security Status : Associate Fail\n" );
				break;

			default:
				DBG_TRACE( DbgInfo, "Security Status : UNKNOWN %d\n",
						   ss->securityStatus );
				break;
			}

			DBG_TRACE(DbgInfo, "STA Address     : %pM\n",
					ss->staAddr);

			DBG_TRACE(DbgInfo, "Reason          : 0x%04x\n",
					ss->reason);
		}

		break;

	  case CFG_WMP:
		DBG_TRACE( DbgInfo, "CFG_WMP, size is %d bytes\n", ltv->len );
		{
			WMP_RSP_STRCT *wmp_rsp = (WMP_RSP_STRCT *)ltv;

			DBG_TRACE( DbgInfo, "CFG_WMP, pdu type is 0x%x\n",
					   wmp_rsp->wmpRsp.wmpHdr.type );

			switch( wmp_rsp->wmpRsp.wmpHdr.type ) {
			  case WVLAN_WMP_PDU_TYPE_LT_RSP:
				{
#if DBG
					LINKTEST_RSP_STRCT  *lt_rsp = (LINKTEST_RSP_STRCT *)ltv;
#endif 
					DBG_TRACE( DbgInfo, "LINK TEST RESULT\n" );
					DBG_TRACE( DbgInfo, "================\n" );
					DBG_TRACE( DbgInfo, "Length        : %d.\n",     lt_rsp->len );

					DBG_TRACE( DbgInfo, "Name          : %s.\n",     lt_rsp->ltRsp.ltRsp.name );
					DBG_TRACE( DbgInfo, "Signal Level  : 0x%02x.\n", lt_rsp->ltRsp.ltRsp.signal );
					DBG_TRACE( DbgInfo, "Noise  Level  : 0x%02x.\n", lt_rsp->ltRsp.ltRsp.noise );
					DBG_TRACE( DbgInfo, "Receive Flow  : 0x%02x.\n", lt_rsp->ltRsp.ltRsp.rxFlow );
					DBG_TRACE( DbgInfo, "Data Rate     : 0x%02x.\n", lt_rsp->ltRsp.ltRsp.dataRate );
					DBG_TRACE( DbgInfo, "Protocol      : 0x%04x.\n", lt_rsp->ltRsp.ltRsp.protocol );
					DBG_TRACE( DbgInfo, "Station       : 0x%02x.\n", lt_rsp->ltRsp.ltRsp.station );
					DBG_TRACE( DbgInfo, "Data Rate Cap : 0x%02x.\n", lt_rsp->ltRsp.ltRsp.dataRateCap );

					DBG_TRACE( DbgInfo, "Power Mgmt    : 0x%02x 0x%02x 0x%02x 0x%02x.\n",
								lt_rsp->ltRsp.ltRsp.powerMgmt[0],
								lt_rsp->ltRsp.ltRsp.powerMgmt[1],
								lt_rsp->ltRsp.ltRsp.powerMgmt[2],
								lt_rsp->ltRsp.ltRsp.powerMgmt[3] );

					DBG_TRACE( DbgInfo, "Robustness    : 0x%02x 0x%02x 0x%02x 0x%02x.\n",
								lt_rsp->ltRsp.ltRsp.robustness[0],
								lt_rsp->ltRsp.ltRsp.robustness[1],
								lt_rsp->ltRsp.ltRsp.robustness[2],
								lt_rsp->ltRsp.ltRsp.robustness[3] );

					DBG_TRACE( DbgInfo, "Scaling       : 0x%02x.\n", lt_rsp->ltRsp.ltRsp.scaling );
				}

				break;

			default:
				break;
			}
		}

		break;

	  case CFG_NULL:
		DBG_TRACE( DbgInfo, "CFG_NULL\n" );
		break;

	  case CFG_UPDATED_INFO_RECORD:        
		DBG_TRACE( DbgInfo, "UPDATED INFORMATION RECORD\n" );

		ltv_val = CNV_INT_TO_LITTLE( ltv->u.u16[0] );

		
		switch( ltv_val ) {
		  case CFG_CUR_COUNTRY_INFO:  
			DBG_TRACE( DbgInfo, "Updated country info\n" );

			wl_connect( lp );
			break;

		  case CFG_PORT_STAT:    
			

			break;

		default:
			DBG_WARNING( DbgInfo, "Unknown RID: 0x%04x\n", ltv_val );
		}

		break;

	default:
		DBG_TRACE( DbgInfo, "UNKNOWN MESSAGE: 0x%04x\n", ltv->typ );
		break;
	}
	DBG_LEAVE( DbgInfo );
	return;
} 
#endif  

#ifdef USE_WDS
void wl_wds_netdev_register( struct wl_private *lp )
{
	int count;
	
	DBG_FUNC( "wl_wds_netdev_register" );
	DBG_ENTER( DbgInfo );
	
	if ( CNV_INT_TO_LITTLE( lp->hcfCtx.IFB_FWIdentity.comp_id ) == COMP_ID_FW_AP  ) {
		for( count = 0; count < NUM_WDS_PORTS; count++ ) {
			if ( WVLAN_VALID_MAC_ADDRESS( lp->wds_port[count].wdsAddress )) {
				if ( register_netdev( lp->wds_port[count].dev ) != 0 ) {
					DBG_WARNING( DbgInfo, "net device for WDS port %d could not be registered\n",
								( count + 1 ));
				}
				lp->wds_port[count].is_registered = TRUE;

				
				memcpy( lp->wds_port[count].dev->dev_addr, lp->MACAddress, ETH_ALEN );
				lp->wds_port[count].dev->addr_len = ETH_ALEN;
			}
		}
	}
	DBG_LEAVE( DbgInfo );
	return;
} 


void wl_wds_netdev_deregister( struct wl_private *lp )
{
	int count;
	
	DBG_FUNC( "wl_wds_netdev_deregister" );
	DBG_ENTER( DbgInfo );
	if ( CNV_INT_TO_LITTLE( lp->hcfCtx.IFB_FWIdentity.comp_id ) == COMP_ID_FW_AP  ) {
		for( count = 0; count < NUM_WDS_PORTS; count++ ) {
			if ( WVLAN_VALID_MAC_ADDRESS( lp->wds_port[count].wdsAddress )) {
				unregister_netdev( lp->wds_port[count].dev );
			}
			lp->wds_port[count].is_registered = FALSE;
		}
	}
	DBG_LEAVE( DbgInfo );
	return;
} 
#endif  


#if 0 
int printf_hcf_16( char *s, char *buf, hcf_16* p, int n );
int printf_hcf_16( char *s, char *buf, hcf_16* p, int n ) {

int i, len;

	len = sprintf(buf, "%s", s );
	while ( len < 20 ) len += sprintf(buf+len, " " );
	len += sprintf(buf+len,": " );
	for ( i = 0; i < n; i++ ) {
		if ( len % 80 > 75 ) {
			len += sprintf(buf+len,"\n" );
		}
		len += sprintf(buf+len,"%04X ", p[i] );
	}
	len += sprintf(buf+len,"\n" );
	return len;
} 

int printf_hcf_8( char *s, char *buf, hcf_8* p, int n );
int printf_hcf_8( char *s, char *buf, hcf_8* p, int n ) {

int i, len;

	len = sprintf(buf, "%s", s );
	while ( len < 20 ) len += sprintf(buf+len, " " );
	len += sprintf(buf+len,": " );
	for ( i = 0; i <= n; i++ ) {
		if ( len % 80 > 77 ) {
			len += sprintf(buf+len,"\n" );
		}
		len += sprintf(buf+len,"%02X ", p[i] );
	}
	len += sprintf(buf+len,"\n" );
	return len;
} 

int printf_strct( char *s, char *buf, hcf_16* p );
int printf_strct( char *s, char *buf, hcf_16* p ) {

int i, len;

	len = sprintf(buf, "%s", s );
	while ( len < 20 ) len += sprintf(buf+len, " " );
	len += sprintf(buf+len,": " );
	for ( i = 0; i <= *p; i++ ) {
		if ( len % 80 > 75 ) {
			len += sprintf(buf+len,"\n" );
		}
		len += sprintf(buf+len,"%04X ", p[i] );
	}
	len += sprintf(buf+len,"\n" );
	return len;
} 

int scull_read_procmem(char *buf, char **start, off_t offset, int len, int *eof, void *data )
{
	struct wl_private	*lp = NULL;
	IFBP				ifbp;
   	CFG_HERMES_TALLIES_STRCT *p;

    #define LIMIT (PAGE_SIZE-80) 

    len=0;

	lp = ((struct net_device *)data)->priv;
	if (lp == NULL) {
        len += sprintf(buf+len,"No wl_private in scull_read_procmem\n" );
	} else if ( lp->wlags49_type == 0 ){
   	    ifbp = &lp->hcfCtx;
   	    len += sprintf(buf+len,"Magic:               0x%04X\n", ifbp->IFB_Magic );
   	    len += sprintf(buf+len,"IOBase:              0x%04X\n", ifbp->IFB_IOBase );
   	    len += sprintf(buf+len,"LinkStat:            0x%04X\n", ifbp->IFB_LinkStat );
   	    len += sprintf(buf+len,"DSLinkStat:          0x%04X\n", ifbp->IFB_DSLinkStat );
   	    len += sprintf(buf+len,"TickIni:         0x%08lX\n", ifbp->IFB_TickIni );
   	    len += sprintf(buf+len,"TickCnt:             0x%04X\n", ifbp->IFB_TickCnt );
   	    len += sprintf(buf+len,"IntOffCnt:           0x%04X\n", ifbp->IFB_IntOffCnt );
		len += printf_hcf_16( "IFB_FWIdentity", &buf[len],
							  &ifbp->IFB_FWIdentity.len, ifbp->IFB_FWIdentity.len + 1 );
	} else if ( lp->wlags49_type == 1 ) {
   	    len += sprintf(buf+len,"Channel:              0x%04X\n", lp->Channel );
#ifdef WIRELESS_EXT
#endif 
   	    len += sprintf(buf+len,"IFB:                  0x%p\n", &lp->hcfCtx );
   	    len += sprintf(buf+len,"flags:                %#.8lX\n", lp->flags );  
   	    len += sprintf(buf+len,"DebugFlag(wl_private) 0x%04X\n", lp->DebugFlag );
#if DBG
   	    len += sprintf(buf+len,"DebugFlag (DbgInfo):   0x%08lX\n", DbgInfo->DebugFlag );
#endif 
   	    len += sprintf(buf+len,"is_registered:        0x%04X\n", lp->is_registered );
		len += printf_strct( "driverInfo", &buf[len], (hcf_16*)&lp->driverInfo );
		len += printf_strct( "driverIdentity", &buf[len], (hcf_16*)&lp->driverIdentity );
		len += printf_strct( "StationIdentity", &buf[len], (hcf_16*)&lp->StationIdentity );
		len += printf_strct( "PrimaryIdentity", &buf[len], (hcf_16*)&lp->hcfCtx.IFB_PRIIdentity );
		len += printf_strct( "PrimarySupplier", &buf[len], (hcf_16*)&lp->hcfCtx.IFB_PRISup );
		len += printf_strct( "NICIdentity", &buf[len], (hcf_16*)&lp->NICIdentity );
   	    len += sprintf(buf+len,"txBytes:              0x%08lX\n", lp->txBytes );
   	    len += sprintf(buf+len,"maxPort:              0x%04X\n", lp->maxPort );        
	
   	    len += sprintf(buf+len,"PortType:             0x%04X\n", lp->PortType );           
   	    len += sprintf(buf+len,"Channel:              0x%04X\n", lp->Channel );            
   	    len += sprintf(buf+len,"TxRateControl[2]:     0x%04X 0x%04X\n",
						lp->TxRateControl[0], lp->TxRateControl[1] );
   	    len += sprintf(buf+len,"DistanceBetweenAPs:   0x%04X\n", lp->DistanceBetweenAPs ); 
   	    len += sprintf(buf+len,"RTSThreshold:         0x%04X\n", lp->RTSThreshold );       
   	    len += sprintf(buf+len,"PMEnabled:            0x%04X\n", lp->PMEnabled );          
   	    len += sprintf(buf+len,"MicrowaveRobustness:  0x%04X\n", lp->MicrowaveRobustness );
   	    len += sprintf(buf+len,"CreateIBSS:           0x%04X\n", lp->CreateIBSS );         
   	    len += sprintf(buf+len,"MulticastReceive:     0x%04X\n", lp->MulticastReceive );   
   	    len += sprintf(buf+len,"MaxSleepDuration:     0x%04X\n", lp->MaxSleepDuration );   
		len += printf_hcf_8( "MACAddress", &buf[len], lp->MACAddress, ETH_ALEN );
   	    len += sprintf(buf+len,"NetworkName:          %.32s\n", lp->NetworkName );
   	    len += sprintf(buf+len,"EnableEncryption:     0x%04X\n", lp->EnableEncryption );   
		len += printf_hcf_8( "Key1", &buf[len], lp->Key1, MAX_KEY_LEN );
   	    len += sprintf(buf+len,"TransmitKeyID:        0x%04X\n", lp->TransmitKeyID );      
   	    len += sprintf(buf+len,"driverEnable:         0x%04X\n", lp->driverEnable );
   	    len += sprintf(buf+len,"wolasEnable:          0x%04X\n", lp->wolasEnable );
   	    len += sprintf(buf+len,"atimWindow:           0x%04X\n", lp->atimWindow );
   	    len += sprintf(buf+len,"holdoverDuration:     0x%04X\n", lp->holdoverDuration );
   	    len += sprintf(buf+len,"authentication:       0x%04X\n", lp->authentication ); 
   	    len += sprintf(buf+len,"promiscuousMode:      0x%04X\n", lp->promiscuousMode );
   	    len += sprintf(buf+len,"DownloadFirmware:     0x%04X\n", lp->DownloadFirmware );   
   	    len += sprintf(buf+len,"AuthKeyMgmtSuite:     0x%04X\n", lp->AuthKeyMgmtSuite );
   	    len += sprintf(buf+len,"loadBalancing:        0x%04X\n", lp->loadBalancing );
   	    len += sprintf(buf+len,"mediumDistribution:   0x%04X\n", lp->mediumDistribution );
   	    len += sprintf(buf+len,"txPowLevel:           0x%04X\n", lp->txPowLevel );
   	    len += sprintf(buf+len,"connectionControl:    0x%04X\n", lp->connectionControl );
   	    len += sprintf(buf+len,"ownBeaconInterval:    0x%04X\n", lp->ownBeaconInterval );
   	    len += sprintf(buf+len,"coexistence:          0x%04X\n", lp->coexistence );
   	    len += sprintf(buf+len,"netif_queue_on:       0x%04X\n", lp->netif_queue_on );
   	    len += sprintf(buf+len,"txQ_count:            0x%04X\n", lp->txQ_count );
   	    len += sprintf(buf+len,"probe_num_aps:        0x%04X\n", lp->probe_num_aps );
   	    len += sprintf(buf+len,"use_dma:              0x%04X\n", lp->use_dma );
#ifdef USE_RTS
   	    len += sprintf(buf+len,"useRTS:               0x%04X\n", lp->useRTS );
#endif  
#if 1 
		
		
   	    len += sprintf(buf+len,"DTIMPeriod:           0x%04X\n", lp->DTIMPeriod );         
   	    len += sprintf(buf+len,"multicastPMBuffering: 0x%04X\n", lp->multicastPMBuffering );
   	    len += sprintf(buf+len,"RejectAny:            0x%04X\n", lp->RejectAny );          
   	    len += sprintf(buf+len,"ExcludeUnencrypted:   0x%04X\n", lp->ExcludeUnencrypted ); 
   	    len += sprintf(buf+len,"intraBSSRelay:        0x%04X\n", lp->intraBSSRelay );
   	    len += sprintf(buf+len,"wlags49_type:             0x%08lX\n", lp->wlags49_type );
#ifdef USE_WDS
#endif 
#endif 
	} else if ( lp->wlags49_type == 2 ){
        len += sprintf(buf+len,"tallies to be added\n" );
   	    p = &lp->hcfCtx.IFB_NIC_Tallies;
        len += sprintf(buf+len,"TxUnicastFrames:          %08lX\n", p->TxUnicastFrames );
        len += sprintf(buf+len,"TxMulticastFrames:        %08lX\n", p->TxMulticastFrames );
        len += sprintf(buf+len,"TxFragments:              %08lX\n", p->TxFragments );
        len += sprintf(buf+len,"TxUnicastOctets:          %08lX\n", p->TxUnicastOctets );
        len += sprintf(buf+len,"TxMulticastOctets:        %08lX\n", p->TxMulticastOctets );
        len += sprintf(buf+len,"TxDeferredTransmissions:  %08lX\n", p->TxDeferredTransmissions );
        len += sprintf(buf+len,"TxSingleRetryFrames:      %08lX\n", p->TxSingleRetryFrames );
        len += sprintf(buf+len,"TxMultipleRetryFrames:    %08lX\n", p->TxMultipleRetryFrames );
        len += sprintf(buf+len,"TxRetryLimitExceeded:     %08lX\n", p->TxRetryLimitExceeded );
        len += sprintf(buf+len,"TxDiscards:               %08lX\n", p->TxDiscards );
        len += sprintf(buf+len,"RxUnicastFrames:          %08lX\n", p->RxUnicastFrames );
        len += sprintf(buf+len,"RxMulticastFrames:        %08lX\n", p->RxMulticastFrames );
        len += sprintf(buf+len,"RxFragments:              %08lX\n", p->RxFragments );
        len += sprintf(buf+len,"RxUnicastOctets:          %08lX\n", p->RxUnicastOctets );
        len += sprintf(buf+len,"RxMulticastOctets:        %08lX\n", p->RxMulticastOctets );
        len += sprintf(buf+len,"RxFCSErrors:              %08lX\n", p->RxFCSErrors );
        len += sprintf(buf+len,"RxDiscardsNoBuffer:       %08lX\n", p->RxDiscardsNoBuffer );
        len += sprintf(buf+len,"TxDiscardsWrongSA:        %08lX\n", p->TxDiscardsWrongSA );
        len += sprintf(buf+len,"RxWEPUndecryptable:       %08lX\n", p->RxWEPUndecryptable );
        len += sprintf(buf+len,"RxMsgInMsgFragments:      %08lX\n", p->RxMsgInMsgFragments );
        len += sprintf(buf+len,"RxMsgInBadMsgFragments:   %08lX\n", p->RxMsgInBadMsgFragments );
        len += sprintf(buf+len,"RxDiscardsWEPICVError:    %08lX\n", p->RxDiscardsWEPICVError );
        len += sprintf(buf+len,"RxDiscardsWEPExcluded:    %08lX\n", p->RxDiscardsWEPExcluded );
#if (HCF_EXT) & HCF_EXT_TALLIES_FW
        
#endif 
	} else if ( lp->wlags49_type & 0x8000 ) {	
#if DBG
		DbgInfo->DebugFlag = lp->wlags49_type & 0x7FFF;
#endif 
		lp->wlags49_type = 0;				
	} else {
        len += sprintf(buf+len,"unknown value for wlags49_type: 0x%08lX\n", lp->wlags49_type );
        len += sprintf(buf+len,"0x0000 - IFB\n" );
        len += sprintf(buf+len,"0x0001 - wl_private\n" );
        len += sprintf(buf+len,"0x0002 - Tallies\n" );
        len += sprintf(buf+len,"0x8xxx - Change debufflag\n" );
        len += sprintf(buf+len,"ERROR    0001\nWARNING  0002\nNOTICE   0004\nTRACE    0008\n" );
        len += sprintf(buf+len,"VERBOSE  0010\nPARAM    0020\nBREAK    0040\nRX       0100\n" );
        len += sprintf(buf+len,"TX       0200\nDS       0400\n" );
	}
    return len;
} 

static void proc_write(const char *name, write_proc_t *w, void *data)
{
	struct proc_dir_entry * entry = create_proc_entry(name, S_IFREG | S_IWUSR, NULL);
	if (entry) {
		entry->write_proc = w;
		entry->data = data;
	}
} 

static int write_int(struct file *file, const char *buffer, unsigned long count, void *data)
{
	static char		proc_number[11];
	unsigned int	nr = 0;

	DBG_FUNC( "write_int" );
	DBG_ENTER( DbgInfo );

	if (count > 9) {
		count = -EINVAL;
	} else if ( copy_from_user(proc_number, buffer, count) ) {
		count = -EFAULT;
	}
	if  (count > 0 ) {
		proc_number[count] = 0;
		nr = simple_strtoul(proc_number , NULL, 0);
 		*(unsigned int *)data = nr;
		if ( nr & 0x8000 ) {	
#if DBG
			DbgInfo->DebugFlag = nr & 0x7FFF;
#endif 
		}
	}
	DBG_PRINT( "value: %08X\n", nr );
	DBG_LEAVE( DbgInfo );
	return count;
} 

#endif 

#ifdef DN554
#define RUN_AT(x)		(jiffies+(x))		
#define DS_OOR	0x8000		

		lp->timer_oor_cnt = DS_OOR;
		init_timer( &lp->timer_oor );
		lp->timer_oor.function = timer_oor;
		lp->timer_oor.data = (unsigned long)lp;
		lp->timer_oor.expires = RUN_AT( 3 * HZ );
		add_timer( &lp->timer_oor );
		printk( "<5>wl_enable: %ld\n", jiffies );		
#endif 
#ifdef DN554
void timer_oor( u_long arg )
{
	struct wl_private       *lp = (struct wl_private *)arg;

    

    DBG_FUNC( "timer_oor" );
    DBG_ENTER( DbgInfo );
    DBG_PARAM( DbgInfo, "arg", "0x%08lx", arg );

	printk( "<5>timer_oor: %ld 0x%04X\n", jiffies, lp->timer_oor_cnt );		
	lp->timer_oor_cnt += 10;
    if ( (lp->timer_oor_cnt & ~DS_OOR) > 300 ) {
		lp->timer_oor_cnt = 300;
	}
	lp->timer_oor_cnt |= DS_OOR;
	init_timer( &lp->timer_oor );
	lp->timer_oor.function = timer_oor;
	lp->timer_oor.data = (unsigned long)lp;
	lp->timer_oor.expires = RUN_AT( (lp->timer_oor_cnt & ~DS_OOR) * HZ );
	add_timer( &lp->timer_oor );

    DBG_LEAVE( DbgInfo );
} 
#endif 

MODULE_LICENSE("Dual BSD/GPL");
