/* Copyright (c) 2008-2010, 2012 The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/mm.h>
#include <linux/fb.h>
#include <linux/init.h>
#include <linux/ioport.h>
#include <linux/device.h>
#include <linux/dma-mapping.h>

#include "msm_fb_panel.h"
#include "mddihost.h"
#include "mddihosti.h"

#define FEATURE_MDDI_UNDERRUN_RECOVERY
#ifndef FEATURE_MDDI_DISABLE_REVERSE
static void mddi_read_rev_packet(byte *data_ptr);
#endif

struct timer_list mddi_host_timer;

#define MDDI_DEFAULT_TIMER_LENGTH 5000	
uint32 mddi_rtd_frequency = 60000;	
uint32 mddi_client_status_frequency = 60000;	

boolean mddi_vsync_detect_enabled = FALSE;
mddi_gpio_info_type mddi_gpio;

uint32 mddi_host_core_version;
boolean mddi_debug_log_statistics = FALSE;
static boolean mddi_host_mdp_active_flag = TRUE;
static uint32 mddi_log_stats_counter;
uint32 mddi_log_stats_frequency = 4000;
int32 mddi_client_type;

#define MDDI_DEFAULT_REV_PKT_SIZE            0x20

#ifndef FEATURE_MDDI_DISABLE_REVERSE
static boolean mddi_rev_ptr_workaround = TRUE;
static uint32 mddi_reg_read_retry;
static uint32 mddi_reg_read_retry_max = 20;
static boolean mddi_enable_reg_read_retry = TRUE;
static boolean mddi_enable_reg_read_retry_once = FALSE;

#define MDDI_MAX_REV_PKT_SIZE                0x60

#define MDDI_CLIENT_CAPABILITY_REV_PKT_SIZE  0x60

#define MDDI_VIDEO_REV_PKT_SIZE              0x40
#define MDDI_REV_BUFFER_SIZE  MDDI_MAX_REV_PKT_SIZE
static byte rev_packet_data[MDDI_MAX_REV_PKT_SIZE];
#endif 

#define MDDI_MAX_REV_DATA_SIZE  128
boolean mddi_debug_clear_rev_data = TRUE;

uint32 *mddi_reg_read_value_ptr;

mddi_client_capability_type mddi_client_capability_pkt;
static boolean mddi_client_capability_request = FALSE;

#ifndef FEATURE_MDDI_DISABLE_REVERSE

#define MAX_MDDI_REV_HANDLERS 2
#define INVALID_PKT_TYPE 0xFFFF

typedef struct {
	mddi_rev_handler_type handler;	
	uint16 pkt_type;
} mddi_rev_pkt_handler_type;
static mddi_rev_pkt_handler_type mddi_rev_pkt_handler[MAX_MDDI_REV_HANDLERS] =
    { {NULL, INVALID_PKT_TYPE}, {NULL, INVALID_PKT_TYPE} };

static boolean mddi_rev_encap_user_request = FALSE;
static mddi_linked_list_notify_type mddi_rev_user;

spinlock_t mddi_host_spin_lock;
extern uint32 mdp_in_processing;
#endif

typedef enum {
	MDDI_REV_IDLE
#ifndef FEATURE_MDDI_DISABLE_REVERSE
	    , MDDI_REV_REG_READ_ISSUED,
	MDDI_REV_REG_READ_SENT,
	MDDI_REV_ENCAP_ISSUED,
	MDDI_REV_STATUS_REQ_ISSUED,
	MDDI_REV_CLIENT_CAP_ISSUED
#endif
} mddi_rev_link_state_type;

typedef enum {
	MDDI_LINK_DISABLED,
	MDDI_LINK_HIBERNATING,
	MDDI_LINK_ACTIVATING,
	MDDI_LINK_ACTIVE
} mddi_host_link_state_type;

typedef struct {
	uint32 count;
	uint32 in_count;
	uint32 disp_req_count;
	uint32 state_change_count;
	uint32 ll_done_count;
	uint32 rev_avail_count;
	uint32 error_count;
	uint32 rev_encap_count;
	uint32 llist_ptr_write_1;
	uint32 llist_ptr_write_2;
} mddi_host_int_type;

typedef struct {
	uint32 fwd_crc_count;
	uint32 rev_crc_count;
	uint32 pri_underflow;
	uint32 sec_underflow;
	uint32 rev_overflow;
	uint32 pri_overwrite;
	uint32 sec_overwrite;
	uint32 rev_overwrite;
	uint32 dma_failure;
	uint32 rtd_failure;
	uint32 reg_read_failure;
#ifdef FEATURE_MDDI_UNDERRUN_RECOVERY
	uint32 pri_underrun_detected;
#endif
} mddi_host_stat_type;

typedef struct {
	uint32 rtd_cnt;
	uint32 rev_enc_cnt;
	uint32 vid_cnt;
	uint32 reg_acc_cnt;
	uint32 cli_stat_cnt;
	uint32 cli_cap_cnt;
	uint32 reg_read_cnt;
	uint32 link_active_cnt;
	uint32 link_hibernate_cnt;
	uint32 vsync_response_cnt;
	uint32 fwd_crc_cnt;
	uint32 rev_crc_cnt;
} mddi_log_params_struct_type;

typedef struct {
	uint32 rtd_value;
	uint32 rtd_counter;
	uint32 client_status_cnt;
	boolean rev_ptr_written;
	uint8 *rev_ptr_start;
	uint8 *rev_ptr_curr;
	uint32 mddi_rev_ptr_write_val;
	dma_addr_t rev_data_dma_addr;
	uint16 rev_pkt_size;
	mddi_rev_link_state_type rev_state;
	mddi_host_link_state_type link_state;
	mddi_host_driver_state_type driver_state;
	boolean disable_hibernation;
	uint32 saved_int_reg;
	uint32 saved_int_en;
	mddi_linked_list_type *llist_ptr;
	dma_addr_t llist_dma_addr;
	mddi_linked_list_type *llist_dma_ptr;
	uint32 *rev_data_buf;
	struct completion mddi_llist_avail_comp;
	boolean mddi_waiting_for_llist_avail;
	mddi_host_int_type int_type;
	mddi_host_stat_type stats;
	mddi_log_params_struct_type log_parms;
	mddi_llist_info_type llist_info;
	mddi_linked_list_notify_type llist_notify[MDDI_MAX_NUM_LLIST_ITEMS];
} mddi_host_cntl_type;

static mddi_host_type mddi_curr_host = MDDI_HOST_PRIM;
static mddi_host_cntl_type mhctl[MDDI_NUM_HOST_CORES];
mddi_linked_list_type *llist_extern[MDDI_NUM_HOST_CORES];
mddi_linked_list_type *llist_dma_extern[MDDI_NUM_HOST_CORES];
mddi_linked_list_notify_type *llist_extern_notify[MDDI_NUM_HOST_CORES];
static mddi_log_params_struct_type prev_parms[MDDI_NUM_HOST_CORES];

extern uint32 mdp_total_vdopkts;

static boolean mddi_host_io_clock_on = FALSE;
static boolean mddi_host_hclk_on = FALSE;

int int_mddi_pri_flag = FALSE;
int int_mddi_ext_flag = FALSE;

static void mddi_report_errors(uint32 int_reg)
{
	mddi_host_type host_idx = mddi_curr_host;
	mddi_host_cntl_type *pmhctl = &(mhctl[host_idx]);

	if (int_reg & MDDI_INT_PRI_UNDERFLOW) {
		pmhctl->stats.pri_underflow++;
		MDDI_MSG_ERR("!!! MDDI Primary Underflow !!!\n");
	}
	if (int_reg & MDDI_INT_SEC_UNDERFLOW) {
		pmhctl->stats.sec_underflow++;
		MDDI_MSG_ERR("!!! MDDI Secondary Underflow !!!\n");
	}
#ifndef FEATURE_MDDI_DISABLE_REVERSE
	if (int_reg & MDDI_INT_REV_OVERFLOW) {
		pmhctl->stats.rev_overflow++;
		MDDI_MSG_ERR("!!! MDDI Reverse Overflow !!!\n");
		pmhctl->rev_ptr_curr = pmhctl->rev_ptr_start;
		mddi_host_reg_out(REV_PTR, pmhctl->mddi_rev_ptr_write_val);

	}
	if (int_reg & MDDI_INT_CRC_ERROR)
		MDDI_MSG_ERR("!!! MDDI Reverse CRC Error !!!\n");
#endif
	if (int_reg & MDDI_INT_PRI_OVERWRITE) {
		pmhctl->stats.pri_overwrite++;
		MDDI_MSG_ERR("!!! MDDI Primary Overwrite !!!\n");
	}
	if (int_reg & MDDI_INT_SEC_OVERWRITE) {
		pmhctl->stats.sec_overwrite++;
		MDDI_MSG_ERR("!!! MDDI Secondary Overwrite !!!\n");
	}
#ifndef FEATURE_MDDI_DISABLE_REVERSE
	if (int_reg & MDDI_INT_REV_OVERWRITE) {
		pmhctl->stats.rev_overwrite++;
		
		MDDI_MSG_DEBUG("MDDI Reverse Overwrite!\n");
	}
	if (int_reg & MDDI_INT_RTD_FAILURE) {
		mddi_host_reg_outm(INTEN, MDDI_INT_RTD_FAILURE, 0);
		pmhctl->stats.rtd_failure++;
		MDDI_MSG_ERR("!!! MDDI RTD Failure !!!\n");
	}
#endif
	if (int_reg & MDDI_INT_DMA_FAILURE) {
		pmhctl->stats.dma_failure++;
		MDDI_MSG_ERR("!!! MDDI DMA Abort !!!\n");
	}
}

static void mddi_host_enable_io_clock(void)
{
	if (!MDDI_HOST_IS_IO_CLOCK_ON)
		MDDI_HOST_ENABLE_IO_CLOCK;
}

static void mddi_host_enable_hclk(void)
{

	if (!MDDI_HOST_IS_HCLK_ON)
		MDDI_HOST_ENABLE_HCLK;
}

static void mddi_host_disable_io_clock(void)
{
#ifndef FEATURE_MDDI_HOST_IO_CLOCK_CONTROL_DISABLE
	if (MDDI_HOST_IS_IO_CLOCK_ON)
		MDDI_HOST_DISABLE_IO_CLOCK;
#endif
}

static void mddi_host_disable_hclk(void)
{
#ifndef FEATURE_MDDI_HOST_HCLK_CONTROL_DISABLE
	if (MDDI_HOST_IS_HCLK_ON)
		MDDI_HOST_DISABLE_HCLK;
#endif
}

static void mddi_vote_to_sleep(mddi_host_type host_idx, boolean sleep)
{
	uint16 vote_mask;

	if (host_idx == MDDI_HOST_PRIM)
		vote_mask = 0x01;
	else
		vote_mask = 0x02;
}

static void mddi_report_state_change(uint32 int_reg)
{
	mddi_host_type host_idx = mddi_curr_host;
	mddi_host_cntl_type *pmhctl = &(mhctl[host_idx]);

	if ((pmhctl->saved_int_reg & MDDI_INT_IN_HIBERNATION) &&
	    (pmhctl->saved_int_reg & MDDI_INT_LINK_ACTIVE)) {
		mddi_host_reg_out(CMD, MDDI_CMD_POWERUP);
	}

	if (int_reg & MDDI_INT_LINK_ACTIVE) {
		pmhctl->link_state = MDDI_LINK_ACTIVE;
		pmhctl->log_parms.link_active_cnt++;
		pmhctl->rtd_value = mddi_host_reg_in(RTD_VAL);
		MDDI_MSG_DEBUG("!!! MDDI Active RTD:0x%x!!!\n",
			       pmhctl->rtd_value);
		
		mddi_host_reg_outm(INTEN,
				   (MDDI_INT_IN_HIBERNATION |
				    MDDI_INT_LINK_ACTIVE),
				   MDDI_INT_IN_HIBERNATION);

#ifdef DEBUG_MDDIHOSTI
		if (mddi_gpio.polling_enabled) {
			timer_reg(&mddi_gpio_poll_timer,
		mddi_gpio_poll_timer_cb, 0, mddi_gpio.polling_interval, 0);
		}
#endif
#ifndef FEATURE_MDDI_DISABLE_REVERSE
		if (mddi_rev_ptr_workaround) {
			
			pmhctl->rev_ptr_written = FALSE;
			pmhctl->rev_ptr_curr = pmhctl->rev_ptr_start;
		}
#endif
		
		mddi_vote_to_sleep(host_idx, FALSE);

		if (host_idx == MDDI_HOST_PRIM) {
			if (mddi_vsync_detect_enabled) {
				mddi_client_lcd_vsync_detected(FALSE);
				pmhctl->log_parms.vsync_response_cnt++;
			}
		}
	}
	if (int_reg & MDDI_INT_IN_HIBERNATION) {
		pmhctl->link_state = MDDI_LINK_HIBERNATING;
		pmhctl->log_parms.link_hibernate_cnt++;
		MDDI_MSG_DEBUG("!!! MDDI Hibernating !!!\n");

		if (mddi_client_type == 2) {
			mddi_host_reg_out(PAD_CTL, 0x402a850f);
			mddi_host_reg_out(PAD_CAL, 0x10220020);
			mddi_host_reg_out(TA1_LEN, 0x0010);
			mddi_host_reg_out(TA2_LEN, 0x0040);
		}
		
#ifdef FEATURE_MDDI_DISABLE_REVERSE
		mddi_host_reg_outm(INTEN,
				   (MDDI_INT_MDDI_IN |
				    MDDI_INT_IN_HIBERNATION |
				    MDDI_INT_LINK_ACTIVE),
				   MDDI_INT_LINK_ACTIVE);
#else
		mddi_host_reg_outm(INTEN,
				   (MDDI_INT_MDDI_IN |
				    MDDI_INT_IN_HIBERNATION |
				    MDDI_INT_LINK_ACTIVE),
				   (MDDI_INT_MDDI_IN | MDDI_INT_LINK_ACTIVE));

		pmhctl->rtd_counter = mddi_rtd_frequency;

		if (pmhctl->rev_state != MDDI_REV_IDLE) {
			
			pmhctl->link_state = MDDI_LINK_ACTIVATING;
			mddi_host_reg_out(CMD, MDDI_CMD_LINK_ACTIVE);
		}
#endif

		if (pmhctl->disable_hibernation) {
			mddi_host_reg_out(CMD, MDDI_CMD_HIBERNATE);
			mddi_host_reg_out(CMD, MDDI_CMD_LINK_ACTIVE);
			pmhctl->link_state = MDDI_LINK_ACTIVATING;
		}
#ifdef FEATURE_MDDI_UNDERRUN_RECOVERY
		if ((pmhctl->llist_info.transmitting_start_idx !=
		     UNASSIGNED_INDEX)
		    &&
		    ((pmhctl->
		      saved_int_reg & (MDDI_INT_PRI_LINK_LIST_DONE |
				       MDDI_INT_PRI_PTR_READ)) ==
		     MDDI_INT_PRI_PTR_READ)) {
			mddi_linked_list_type *llist_dma;
			llist_dma = pmhctl->llist_dma_ptr;
			dma_coherent_pre_ops();
			
			mddi_host_reg_out(PRI_PTR,
					  &llist_dma[pmhctl->llist_info.
						     transmitting_start_idx]);
			pmhctl->stats.pri_underrun_detected++;
		}
#endif

		
		if (pmhctl->link_state == MDDI_LINK_HIBERNATING) {
			mddi_vote_to_sleep(host_idx, TRUE);
		}

#ifdef DEBUG_MDDIHOSTI
		
		if (mddi_gpio.polling_enabled) {
			(void) timer_clr(&mddi_gpio_poll_timer, T_NONE);
		}
#endif
	}
}

void mddi_host_timer_service(unsigned long data)
{
#ifndef FEATURE_MDDI_DISABLE_REVERSE
	unsigned long flags;
#endif
	mddi_host_type host_idx;
	mddi_host_cntl_type *pmhctl;

	unsigned long time_ms = MDDI_DEFAULT_TIMER_LENGTH;
	init_timer(&mddi_host_timer);
	for (host_idx = MDDI_HOST_PRIM; host_idx < MDDI_NUM_HOST_CORES;
	     host_idx++) {
		pmhctl = &(mhctl[host_idx]);
		mddi_log_stats_counter += (uint32) time_ms;
#ifndef FEATURE_MDDI_DISABLE_REVERSE
		pmhctl->rtd_counter += (uint32) time_ms;
		pmhctl->client_status_cnt += (uint32) time_ms;

		if (host_idx == MDDI_HOST_PRIM) {
			if (pmhctl->client_status_cnt >=
			    mddi_client_status_frequency) {
				if ((pmhctl->link_state ==
				     MDDI_LINK_HIBERNATING)
				    && (pmhctl->client_status_cnt >
					mddi_client_status_frequency)) {

					MDDI_MSG_INFO("wake up link!\n");
					spin_lock_irqsave(&mddi_host_spin_lock,
							  flags);
					mddi_host_enable_hclk();
					mddi_host_enable_io_clock();
					pmhctl->link_state =
					    MDDI_LINK_ACTIVATING;
					mddi_host_reg_out(CMD,
							  MDDI_CMD_LINK_ACTIVE);
					spin_unlock_irqrestore
					    (&mddi_host_spin_lock, flags);
				} else
				    if ((pmhctl->link_state == MDDI_LINK_ACTIVE)
					&& pmhctl->disable_hibernation) {
					MDDI_MSG_INFO("kick isr!\n");
					spin_lock_irqsave(&mddi_host_spin_lock,
							  flags);
					mddi_host_enable_hclk();
					mddi_host_reg_outm(INTEN,
							   MDDI_INT_NO_CMD_PKTS_PEND,
							   MDDI_INT_NO_CMD_PKTS_PEND);
					spin_unlock_irqrestore
					    (&mddi_host_spin_lock, flags);
				}
			}
		}
#endif 
	}

	
	for (host_idx = MDDI_HOST_PRIM; host_idx < MDDI_NUM_HOST_CORES;
	     host_idx++) {
		mddi_log_params_struct_type *prev_ptr = &(prev_parms[host_idx]);
		pmhctl = &(mhctl[host_idx]);

		if (mddi_debug_log_statistics) {

			
			pmhctl->log_parms.vid_cnt = mdp_total_vdopkts;

			if (mddi_log_stats_counter >= mddi_log_stats_frequency) {
				
				if (mddi_debug_log_statistics) {
					MDDI_MSG_NOTICE
					    ("MDDI Statistics since last report:\n");
					MDDI_MSG_NOTICE("  Packets sent:\n");
					MDDI_MSG_NOTICE
					    ("    %d RTD packet(s)\n",
					     pmhctl->log_parms.rtd_cnt -
					     prev_ptr->rtd_cnt);
					if (prev_ptr->rtd_cnt !=
					    pmhctl->log_parms.rtd_cnt) {
						unsigned long flags;
						spin_lock_irqsave
						    (&mddi_host_spin_lock,
						     flags);
						mddi_host_enable_hclk();
						pmhctl->rtd_value =
						    mddi_host_reg_in(RTD_VAL);
						spin_unlock_irqrestore
						    (&mddi_host_spin_lock,
						     flags);
						MDDI_MSG_NOTICE
						    ("      RTD value=%d\n",
						     pmhctl->rtd_value);
					}
					MDDI_MSG_NOTICE
					    ("    %d VIDEO packets\n",
					     pmhctl->log_parms.vid_cnt -
					     prev_ptr->vid_cnt);
					MDDI_MSG_NOTICE
					    ("    %d Register Access packets\n",
					     pmhctl->log_parms.reg_acc_cnt -
					     prev_ptr->reg_acc_cnt);
					MDDI_MSG_NOTICE
					    ("    %d Reverse Encapsulation packet(s)\n",
					     pmhctl->log_parms.rev_enc_cnt -
					     prev_ptr->rev_enc_cnt);
					if (prev_ptr->rev_enc_cnt !=
					    pmhctl->log_parms.rev_enc_cnt) {
						
						MDDI_MSG_NOTICE
						    ("      %d reverse CRC errors detected\n",
						     pmhctl->log_parms.
						     rev_crc_cnt -
						     prev_ptr->rev_crc_cnt);
					}
					MDDI_MSG_NOTICE
					    ("  Packets received:\n");
					MDDI_MSG_NOTICE
					    ("    %d Client Status packets",
					     pmhctl->log_parms.cli_stat_cnt -
					     prev_ptr->cli_stat_cnt);
					if (prev_ptr->cli_stat_cnt !=
					    pmhctl->log_parms.cli_stat_cnt) {
						MDDI_MSG_NOTICE
						    ("      %d forward CRC errors reported\n",
						     pmhctl->log_parms.
						     fwd_crc_cnt -
						     prev_ptr->fwd_crc_cnt);
					}
					MDDI_MSG_NOTICE
					    ("    %d Register Access Read packets\n",
					     pmhctl->log_parms.reg_read_cnt -
					     prev_ptr->reg_read_cnt);

					if (pmhctl->link_state ==
					    MDDI_LINK_ACTIVE) {
						MDDI_MSG_NOTICE
						    ("  Current Link Status: Active\n");
					} else
					    if ((pmhctl->link_state ==
						 MDDI_LINK_HIBERNATING)
						|| (pmhctl->link_state ==
						    MDDI_LINK_ACTIVATING)) {
						MDDI_MSG_NOTICE
						    ("  Current Link Status: Hibernation\n");
					} else {
						MDDI_MSG_NOTICE
						    ("  Current Link Status: Inactive\n");
					}
					MDDI_MSG_NOTICE
					    ("    Active state entered %d times\n",
					     pmhctl->log_parms.link_active_cnt -
					     prev_ptr->link_active_cnt);
					MDDI_MSG_NOTICE
					    ("    Hibernation state entered %d times\n",
					     pmhctl->log_parms.
					     link_hibernate_cnt -
					     prev_ptr->link_hibernate_cnt);
				}
			}
			prev_parms[host_idx] = pmhctl->log_parms;
		}
	}
	if (mddi_log_stats_counter >= mddi_log_stats_frequency)
		mddi_log_stats_counter = 0;

	mutex_lock(&mddi_timer_lock);
	if (!mddi_timer_shutdown_flag) {
		mddi_host_timer.function = mddi_host_timer_service;
		mddi_host_timer.data = 0;
		mddi_host_timer.expires = jiffies + ((time_ms * HZ) / 1000);
		add_timer(&mddi_host_timer);
	}
	mutex_unlock(&mddi_timer_lock);

	return;
}				

static void mddi_process_link_list_done(void)
{
	mddi_host_type host_idx = mddi_curr_host;
	mddi_host_cntl_type *pmhctl = &(mhctl[host_idx]);

	
	if (pmhctl->llist_info.transmitting_start_idx == UNASSIGNED_INDEX) {
		MDDI_MSG_ERR("**** getting LL done, but no list ****\n");
	} else {
		uint16 idx;

#ifndef FEATURE_MDDI_DISABLE_REVERSE
		if (pmhctl->rev_state == MDDI_REV_REG_READ_ISSUED) {
			
			pmhctl->rev_state = MDDI_REV_REG_READ_SENT;
			if (pmhctl->llist_info.reg_read_idx == UNASSIGNED_INDEX) {
				MDDI_MSG_ERR
				    ("**** getting LL done, but no list ****\n");
			}
		}
#endif
		for (idx = pmhctl->llist_info.transmitting_start_idx;;) {
			uint16 next_idx = pmhctl->llist_notify[idx].next_idx;
			if (idx != pmhctl->llist_info.reg_read_idx) {
				
				if (pmhctl->llist_notify[idx].waiting) {
					complete(&
						 (pmhctl->llist_notify[idx].
						  done_comp));
				}
				if (pmhctl->llist_notify[idx].done_cb != NULL) {
					(*(pmhctl->llist_notify[idx].done_cb))
					    ();
				}

				pmhctl->llist_notify[idx].in_use = FALSE;
				pmhctl->llist_notify[idx].waiting = FALSE;
				pmhctl->llist_notify[idx].done_cb = NULL;
				if (idx < MDDI_NUM_DYNAMIC_LLIST_ITEMS) {
					
					pmhctl->llist_notify[idx].next_idx =
					    UNASSIGNED_INDEX;
				}
				pmhctl->log_parms.reg_acc_cnt++;
			}
			if (idx == pmhctl->llist_info.transmitting_end_idx)
				break;
			idx = next_idx;
			if (idx == UNASSIGNED_INDEX)
				MDDI_MSG_CRIT("MDDI linked list corruption!\n");
		}

		pmhctl->llist_info.transmitting_start_idx = UNASSIGNED_INDEX;
		pmhctl->llist_info.transmitting_end_idx = UNASSIGNED_INDEX;

		if (pmhctl->mddi_waiting_for_llist_avail) {
			if (!
			    (pmhctl->
			     llist_notify[pmhctl->llist_info.next_free_idx].
			     in_use)) {
				pmhctl->mddi_waiting_for_llist_avail = FALSE;
				complete(&(pmhctl->mddi_llist_avail_comp));
			}
		}
	}

	
	mddi_host_reg_outm(INTEN, MDDI_INT_PRI_LINK_LIST_DONE, 0);

}

static void mddi_queue_forward_linked_list(void)
{
	uint16 first_pkt_index;
	mddi_linked_list_type *llist_dma;
	mddi_host_type host_idx = mddi_curr_host;
	mddi_host_cntl_type *pmhctl = &(mhctl[host_idx]);
	llist_dma = pmhctl->llist_dma_ptr;

	first_pkt_index = UNASSIGNED_INDEX;

	if (pmhctl->llist_info.transmitting_start_idx == UNASSIGNED_INDEX) {
#ifndef FEATURE_MDDI_DISABLE_REVERSE
		if (pmhctl->llist_info.reg_read_waiting) {
			if (pmhctl->rev_state == MDDI_REV_IDLE) {
				pmhctl->rev_state = MDDI_REV_REG_READ_ISSUED;
				mddi_reg_read_retry = 0;
				first_pkt_index =
				    pmhctl->llist_info.waiting_start_idx;
				pmhctl->llist_info.reg_read_waiting = FALSE;
			}
		} else
#endif
		{
			first_pkt_index = pmhctl->llist_info.waiting_start_idx;
		}
	}

	if (first_pkt_index != UNASSIGNED_INDEX) {
		pmhctl->llist_info.transmitting_start_idx =
		    pmhctl->llist_info.waiting_start_idx;
		pmhctl->llist_info.transmitting_end_idx =
		    pmhctl->llist_info.waiting_end_idx;
		pmhctl->llist_info.waiting_start_idx = UNASSIGNED_INDEX;
		pmhctl->llist_info.waiting_end_idx = UNASSIGNED_INDEX;

		
		MDDI_MSG_DEBUG("MDDI writing primary ptr with idx=%d\n",
			       first_pkt_index);

		pmhctl->int_type.llist_ptr_write_2++;

		dma_coherent_pre_ops();
		mddi_host_reg_out(PRI_PTR, &llist_dma[first_pkt_index]);

		
		mddi_host_reg_outm(INTEN, MDDI_INT_PRI_LINK_LIST_DONE,
				   MDDI_INT_PRI_LINK_LIST_DONE);

	}

}

#ifndef FEATURE_MDDI_DISABLE_REVERSE
static void mddi_read_rev_packet(byte *data_ptr)
{
	uint16 i, length;
	mddi_host_type host_idx = mddi_curr_host;
	mddi_host_cntl_type *pmhctl = &(mhctl[host_idx]);

	uint8 *rev_ptr_overflow =
	    (pmhctl->rev_ptr_start + MDDI_REV_BUFFER_SIZE);

	
	length = *pmhctl->rev_ptr_curr++;
	if (pmhctl->rev_ptr_curr >= rev_ptr_overflow)
		pmhctl->rev_ptr_curr = pmhctl->rev_ptr_start;
	length |= ((*pmhctl->rev_ptr_curr++) << 8);
	if (pmhctl->rev_ptr_curr >= rev_ptr_overflow)
		pmhctl->rev_ptr_curr = pmhctl->rev_ptr_start;
	if (length > (pmhctl->rev_pkt_size - 2)) {
		MDDI_MSG_ERR("Invalid rev pkt length %d\n", length);
		
		length = pmhctl->rev_pkt_size - 2;
	}

	if (data_ptr == NULL) {
		pmhctl->rev_ptr_curr += length;
		if (pmhctl->rev_ptr_curr >= rev_ptr_overflow)
			pmhctl->rev_ptr_curr -= MDDI_REV_BUFFER_SIZE;
		return;
	}

	data_ptr[0] = length & 0x0ff;
	data_ptr[1] = length >> 8;
	data_ptr += 2;
	
	for (i = 0; (i < length) && (pmhctl->rev_ptr_curr < rev_ptr_overflow);
	     i++)
		*data_ptr++ = *pmhctl->rev_ptr_curr++;
	if (pmhctl->rev_ptr_curr >= rev_ptr_overflow)
		pmhctl->rev_ptr_curr = pmhctl->rev_ptr_start;
	for (; (i < length) && (pmhctl->rev_ptr_curr < rev_ptr_overflow); i++)
		*data_ptr++ = *pmhctl->rev_ptr_curr++;
}

static void mddi_process_rev_packets(void)
{
	uint32 rev_packet_count;
	word i;
	uint32 crc_errors;
	boolean mddi_reg_read_successful = FALSE;
	mddi_host_type host_idx = mddi_curr_host;
	mddi_host_cntl_type *pmhctl = &(mhctl[host_idx]);

	pmhctl->log_parms.rev_enc_cnt++;
	if ((pmhctl->rev_state != MDDI_REV_ENCAP_ISSUED) &&
	    (pmhctl->rev_state != MDDI_REV_STATUS_REQ_ISSUED) &&
	    (pmhctl->rev_state != MDDI_REV_CLIENT_CAP_ISSUED)) {
		MDDI_MSG_ERR("Wrong state %d for reverse int\n",
			     pmhctl->rev_state);
	}
	
	mddi_host_reg_outm(INTEN, MDDI_INT_REV_DATA_AVAIL, 0);

	
	mddi_host_reg_out(INT, MDDI_INT_REV_DATA_AVAIL);

	
	rev_packet_count = mddi_host_reg_in(REV_PKT_CNT);

#ifndef T_MSM7500
	
	mddi_host_reg_out(REV_PKT_CNT, 0x0000);
#endif

#if defined(CONFIG_FB_MSM_MDP31) || defined(CONFIG_FB_MSM_MDP40)
	if ((pmhctl->rev_state == MDDI_REV_CLIENT_CAP_ISSUED) &&
	    (rev_packet_count > 0) &&
	    (mddi_host_core_version == 0x28 ||
	     mddi_host_core_version == 0x30)) {

		uint32 int_reg;
		uint32 max_count = 0;

		mddi_host_reg_out(REV_PTR, pmhctl->mddi_rev_ptr_write_val);
		int_reg = mddi_host_reg_in(INT);
		while ((int_reg & 0x100000) == 0) {
			udelay(3);
			int_reg = mddi_host_reg_in(INT);
			if (++max_count > 100)
				break;
		}
	}
#endif

	
	crc_errors = mddi_host_reg_in(REV_CRC_ERR);
	if (crc_errors != 0) {
		pmhctl->log_parms.rev_crc_cnt += crc_errors;
		pmhctl->stats.rev_crc_count += crc_errors;
		MDDI_MSG_ERR("!!! MDDI %d Reverse CRC Error(s) !!!\n",
			     crc_errors);
#ifndef T_MSM7500
		
		mddi_host_reg_out(REV_CRC_ERR, 0x0000);
#endif
		
		pmhctl->rtd_counter = mddi_rtd_frequency;
	}

	pmhctl->rtd_value = mddi_host_reg_in(RTD_VAL);

	MDDI_MSG_DEBUG("MDDI rev pkt cnt=%d, ptr=0x%x, RTD:0x%x\n",
		       rev_packet_count,
		       pmhctl->rev_ptr_curr - pmhctl->rev_ptr_start,
		       pmhctl->rtd_value);

	if (rev_packet_count >= 1) {
		mddi_invalidate_cache_lines((uint32 *) pmhctl->rev_ptr_start,
					    MDDI_REV_BUFFER_SIZE);
	} else {
		MDDI_MSG_ERR("Reverse pkt sent, no data rxd\n");
		if (mddi_reg_read_value_ptr)
			*mddi_reg_read_value_ptr = -EBUSY;
	}
	
	dma_coherent_post_ops();
	for (i = 0; i < rev_packet_count; i++) {
		mddi_rev_packet_type *rev_pkt_ptr;

		mddi_read_rev_packet(rev_packet_data);

		rev_pkt_ptr = (mddi_rev_packet_type *) rev_packet_data;

		if (rev_pkt_ptr->packet_length > pmhctl->rev_pkt_size) {
			MDDI_MSG_ERR("!!!invalid packet size: %d\n",
				     rev_pkt_ptr->packet_length);
		}

		MDDI_MSG_DEBUG("MDDI rev pkt 0x%x size 0x%x\n",
			       rev_pkt_ptr->packet_type,
			       rev_pkt_ptr->packet_length);

		
		switch (rev_pkt_ptr->packet_type) {
		case 66:	
			{
				mddi_client_capability_type
				    *client_capability_pkt_ptr;

				client_capability_pkt_ptr =
				    (mddi_client_capability_type *)
				    rev_packet_data;
				MDDI_MSG_NOTICE
				    ("Client Capability: Week=%d, Year=%d\n",
				     client_capability_pkt_ptr->
				     Week_of_Manufacture,
				     client_capability_pkt_ptr->
				     Year_of_Manufacture);
				memcpy((void *)&mddi_client_capability_pkt,
				       (void *)rev_packet_data,
				       sizeof(mddi_client_capability_type));
				pmhctl->log_parms.cli_cap_cnt++;
			}
			break;

		case 70:	
			{
				mddi_client_status_type *client_status_pkt_ptr;

				client_status_pkt_ptr =
				    (mddi_client_status_type *) rev_packet_data;
				if ((client_status_pkt_ptr->crc_error_count !=
				     0)
				    || (client_status_pkt_ptr->
					reverse_link_request != 0)) {
					MDDI_MSG_ERR
					    ("Client Status: RevReq=%d, CrcErr=%d\n",
					     client_status_pkt_ptr->
					     reverse_link_request,
					     client_status_pkt_ptr->
					     crc_error_count);
				} else {
					MDDI_MSG_DEBUG
					    ("Client Status: RevReq=%d, CrcErr=%d\n",
					     client_status_pkt_ptr->
					     reverse_link_request,
					     client_status_pkt_ptr->
					     crc_error_count);
				}
				pmhctl->log_parms.fwd_crc_cnt +=
				    client_status_pkt_ptr->crc_error_count;
				pmhctl->stats.fwd_crc_count +=
				    client_status_pkt_ptr->crc_error_count;
				pmhctl->log_parms.cli_stat_cnt++;
			}
			break;

		case 146:	
			{
				mddi_register_access_packet_type
				    * regacc_pkt_ptr;
				uint32 data_count;

				regacc_pkt_ptr =
				    (mddi_register_access_packet_type *)
				    rev_packet_data;

				
				data_count = regacc_pkt_ptr->read_write_info
					& 0x3FFF;
				MDDI_MSG_DEBUG("\n MDDI rev read: 0x%x",
					regacc_pkt_ptr->read_write_info);
				MDDI_MSG_DEBUG("Reg Acc parse reg=0x%x,"
					"value=0x%x\n", regacc_pkt_ptr->
					register_address, regacc_pkt_ptr->
					register_data_list[0]);

				
				if (mddi_reg_read_value_ptr) {
#if defined(T_MSM6280) && !defined(T_MSM7200)
					
					*mddi_reg_read_value_ptr =
					    regacc_pkt_ptr->
					    register_data_list[0] & 0x0000ffff;
					mddi_reg_read_successful = TRUE;
					mddi_reg_read_value_ptr = NULL;
#else
				if (data_count && data_count <=
					MDDI_HOST_MAX_CLIENT_REG_IN_SAME_ADDR) {
					memcpy(mddi_reg_read_value_ptr,
						(void *)&regacc_pkt_ptr->
						register_data_list[0],
						data_count * 4);
					mddi_reg_read_successful = TRUE;
					mddi_reg_read_value_ptr = NULL;
				}
#endif
				}

#ifdef DEBUG_MDDIHOSTI
				if ((mddi_gpio.polling_enabled) &&
				    (regacc_pkt_ptr->register_address ==
				     mddi_gpio.polling_reg)) {
					 mddi_client_lcd_gpio_poll(
					 regacc_pkt_ptr->register_data_list[0]);
				}
#endif
				pmhctl->log_parms.reg_read_cnt++;
			}
			break;

		case INVALID_PKT_TYPE:	
			MDDI_MSG_ERR("!!!INVALID_PKT_TYPE rcvd\n");
			break;

		default:	
			{
				uint16 hdlr;

				for (hdlr = 0; hdlr < MAX_MDDI_REV_HANDLERS;
				     hdlr++) {
					if (mddi_rev_pkt_handler[hdlr].
							handler == NULL)
						continue;
					if (mddi_rev_pkt_handler[hdlr].
					    pkt_type ==
					    rev_pkt_ptr->packet_type) {
						(*(mddi_rev_pkt_handler[hdlr].
						  handler)) (rev_pkt_ptr);
					
						break;
					}
				}
				if (hdlr >= MAX_MDDI_REV_HANDLERS)
					MDDI_MSG_ERR("MDDI unknown rev pkt\n");
			}
			break;
		}
	}
	if ((pmhctl->rev_ptr_curr + pmhctl->rev_pkt_size) >=
	    (pmhctl->rev_ptr_start + MDDI_REV_BUFFER_SIZE)) {
		pmhctl->rev_ptr_written = FALSE;
	}

	if (pmhctl->rev_state == MDDI_REV_ENCAP_ISSUED) {
		pmhctl->rev_state = MDDI_REV_IDLE;
		if (mddi_rev_user.waiting) {
			mddi_rev_user.waiting = FALSE;
			complete(&(mddi_rev_user.done_comp));
		} else if (pmhctl->llist_info.reg_read_idx == UNASSIGNED_INDEX) {
			MDDI_MSG_ERR
			    ("Reverse Encap state, but no reg read in progress\n");
		} else {
			if ((!mddi_reg_read_successful) &&
			    (mddi_reg_read_retry < mddi_reg_read_retry_max) &&
			    (mddi_enable_reg_read_retry)) {
				if (mddi_enable_reg_read_retry_once)
					mddi_reg_read_retry =
					    mddi_reg_read_retry_max;
				else
					mddi_reg_read_retry++;
				pmhctl->rev_state = MDDI_REV_REG_READ_SENT;
				pmhctl->stats.reg_read_failure++;
			} else {
				uint16 reg_read_idx =
				    pmhctl->llist_info.reg_read_idx;

				mddi_reg_read_retry = 0;
				if (pmhctl->llist_notify[reg_read_idx].waiting) {
					complete(&
						 (pmhctl->
						  llist_notify[reg_read_idx].
						  done_comp));
				}
				pmhctl->llist_info.reg_read_idx =
				    UNASSIGNED_INDEX;
				if (pmhctl->llist_notify[reg_read_idx].
				    done_cb != NULL) {
					(*
					 (pmhctl->llist_notify[reg_read_idx].
					  done_cb)) ();
				}
				pmhctl->llist_notify[reg_read_idx].next_idx =
				    UNASSIGNED_INDEX;
				pmhctl->llist_notify[reg_read_idx].in_use =
				    FALSE;
				pmhctl->llist_notify[reg_read_idx].waiting =
				    FALSE;
				pmhctl->llist_notify[reg_read_idx].done_cb =
				    NULL;
				if (!mddi_reg_read_successful)
					pmhctl->stats.reg_read_failure++;
			}
		}
	} else if (pmhctl->rev_state == MDDI_REV_CLIENT_CAP_ISSUED) {
#if defined(CONFIG_FB_MSM_MDP31) || defined(CONFIG_FB_MSM_MDP40)
		if (mddi_host_core_version == 0x28 ||
		    mddi_host_core_version == 0x30) {
			mddi_host_reg_out(FIFO_ALLOC, 0x00);
			pmhctl->rev_ptr_written = TRUE;
			mddi_host_reg_out(REV_PTR,
				pmhctl->mddi_rev_ptr_write_val);
			pmhctl->rev_ptr_curr = pmhctl->rev_ptr_start;
			mddi_host_reg_out(CMD, 0xC00);
		}
#endif

		if (mddi_rev_user.waiting) {
			mddi_rev_user.waiting = FALSE;
			complete(&(mddi_rev_user.done_comp));
		}
		pmhctl->rev_state = MDDI_REV_IDLE;
	} else {
		pmhctl->rev_state = MDDI_REV_IDLE;
	}

	

	
	mddi_host_reg_outm(INTEN, MDDI_INT_REV_DATA_AVAIL,
			   MDDI_INT_REV_DATA_AVAIL);

}

static void mddi_issue_reverse_encapsulation(void)
{
	mddi_host_type host_idx = mddi_curr_host;
	mddi_host_cntl_type *pmhctl = &(mhctl[host_idx]);
	if (((pmhctl->rev_state == MDDI_REV_IDLE) ||
	     (pmhctl->rev_state == MDDI_REV_REG_READ_SENT)) &&
	    (pmhctl->llist_info.transmitting_start_idx == UNASSIGNED_INDEX) &&
	    (!mdp_in_processing)) {
		uint32 mddi_command = MDDI_CMD_SEND_REV_ENCAP;

		if ((pmhctl->rev_state == MDDI_REV_REG_READ_SENT) ||
		    (mddi_rev_encap_user_request == TRUE)) {
			mddi_host_enable_io_clock();
			if (pmhctl->link_state == MDDI_LINK_HIBERNATING) {
				
				MDDI_MSG_DEBUG("wake up link!\n");
				pmhctl->link_state = MDDI_LINK_ACTIVATING;
				mddi_host_reg_out(CMD, MDDI_CMD_LINK_ACTIVE);
			} else {
				if (pmhctl->rtd_counter >= mddi_rtd_frequency) {
					MDDI_MSG_DEBUG
					    ("mddi sending RTD command!\n");
					mddi_host_reg_out(CMD,
							  MDDI_CMD_SEND_RTD);
					pmhctl->rtd_counter = 0;
					pmhctl->log_parms.rtd_cnt++;
				}
				if (pmhctl->rev_state != MDDI_REV_REG_READ_SENT) {
					mddi_rev_encap_user_request = FALSE;
				}
				
				pmhctl->rev_state = MDDI_REV_ENCAP_ISSUED;
				mddi_command = MDDI_CMD_SEND_REV_ENCAP;
				MDDI_MSG_DEBUG("sending rev encap!\n");
			}
		} else
		    if ((pmhctl->client_status_cnt >=
			 mddi_client_status_frequency)
			|| mddi_client_capability_request) {
			mddi_host_enable_io_clock();
			if (pmhctl->link_state == MDDI_LINK_HIBERNATING) {
				
				if ((pmhctl->client_status_cnt >=
				     (mddi_client_status_frequency * 2))
				    || mddi_client_capability_request) {
					
					MDDI_MSG_DEBUG("wake up link!\n");
					pmhctl->link_state =
					    MDDI_LINK_ACTIVATING;
					mddi_host_reg_out(CMD,
							  MDDI_CMD_LINK_ACTIVE);
				}
			} else {
				if (pmhctl->rtd_counter >= mddi_rtd_frequency) {
					MDDI_MSG_DEBUG
					    ("mddi sending RTD command!\n");
					mddi_host_reg_out(CMD,
							  MDDI_CMD_SEND_RTD);
					pmhctl->rtd_counter = 0;
					pmhctl->log_parms.rtd_cnt++;
				}
				
				MDDI_MSG_DEBUG
				    ("mddi sending rev enc! (get status)\n");
				if (mddi_client_capability_request) {
					pmhctl->rev_state =
					    MDDI_REV_CLIENT_CAP_ISSUED;
					mddi_command = MDDI_CMD_GET_CLIENT_CAP;
					mddi_client_capability_request = FALSE;
				} else {
					pmhctl->rev_state =
					    MDDI_REV_STATUS_REQ_ISSUED;
					pmhctl->client_status_cnt = 0;
					mddi_command =
					    MDDI_CMD_GET_CLIENT_STATUS;
				}
			}
		}
		if ((pmhctl->rev_state == MDDI_REV_ENCAP_ISSUED) ||
		    (pmhctl->rev_state == MDDI_REV_STATUS_REQ_ISSUED) ||
		    (pmhctl->rev_state == MDDI_REV_CLIENT_CAP_ISSUED)) {
			pmhctl->int_type.rev_encap_count++;
#if defined(T_MSM6280) && !defined(T_MSM7200)
			mddi_rev_pointer_written = TRUE;
			mddi_host_reg_out(REV_PTR, mddi_rev_ptr_write_val);
			mddi_rev_ptr_curr = mddi_rev_ptr_start;
			
			mddi_host_reg_out(CMD, 0xC00);
#else
			if (!pmhctl->rev_ptr_written) {
				MDDI_MSG_DEBUG("writing reverse pointer!\n");
				pmhctl->rev_ptr_written = TRUE;
#if defined(CONFIG_FB_MSM_MDP31) || defined(CONFIG_FB_MSM_MDP40)
				if ((pmhctl->rev_state ==
				     MDDI_REV_CLIENT_CAP_ISSUED) &&
				    (mddi_host_core_version == 0x28 ||
				     mddi_host_core_version == 0x30)) {
					pmhctl->rev_ptr_written = FALSE;
					mddi_host_reg_out(FIFO_ALLOC, 0x02);
				} else
					mddi_host_reg_out(REV_PTR,
						  pmhctl->
						  mddi_rev_ptr_write_val);
#else
				mddi_host_reg_out(REV_PTR,
						  pmhctl->
						  mddi_rev_ptr_write_val);
#endif
			}
#endif
			if (mddi_debug_clear_rev_data) {
				uint16 i;
				for (i = 0; i < MDDI_MAX_REV_DATA_SIZE / 4; i++)
					pmhctl->rev_data_buf[i] = 0xdddddddd;
				
				mddi_flush_cache_lines(pmhctl->rev_data_buf,
						       MDDI_MAX_REV_DATA_SIZE);
			}

			
			mddi_host_reg_out(CMD, mddi_command);
		}
	}

}

static void mddi_process_client_initiated_wakeup(void)
{
	mddi_host_type host_idx = mddi_curr_host;
	mddi_host_cntl_type *pmhctl = &(mhctl[host_idx]);

	mddi_host_reg_outm(INTEN, MDDI_INT_MDDI_IN, 0);

	if (host_idx == MDDI_HOST_PRIM) {
		if (mddi_vsync_detect_enabled) {
			mddi_host_enable_io_clock();
#ifndef MDDI_HOST_DISP_LISTEN
			
			
			if (pmhctl->link_state == MDDI_LINK_HIBERNATING) {
				pmhctl->link_state = MDDI_LINK_ACTIVATING;
				mddi_host_reg_out(CMD, MDDI_CMD_LINK_ACTIVE);
			}
#endif
			mddi_client_lcd_vsync_detected(TRUE);
			pmhctl->log_parms.vsync_response_cnt++;
			MDDI_MSG_NOTICE("MDDI_INT_IN condition\n");

		}
	}

	if (mddi_gpio.polling_enabled) {
		mddi_host_enable_io_clock();
		
		(void)mddi_queue_register_read_int(mddi_gpio.polling_reg,
						   &mddi_gpio.polling_val);
	}
}
#endif 

static void mddi_host_isr(void)
{
	uint32 int_reg, int_en;
#ifndef FEATURE_MDDI_DISABLE_REVERSE
	uint32 status_reg;
#endif
	mddi_host_type host_idx = mddi_curr_host;
	mddi_host_cntl_type *pmhctl = &(mhctl[host_idx]);

	if (!MDDI_HOST_IS_HCLK_ON) {
		MDDI_HOST_ENABLE_HCLK;
	}
	int_reg = mddi_host_reg_in(INT);
	int_en = mddi_host_reg_in(INTEN);
	pmhctl->saved_int_reg = int_reg;
	pmhctl->saved_int_en = int_en;
	int_reg = int_reg & int_en;
	pmhctl->int_type.count++;


#ifndef FEATURE_MDDI_DISABLE_REVERSE
	status_reg = mddi_host_reg_in(STAT);

	if ((int_reg & MDDI_INT_MDDI_IN) ||
	    ((int_en & MDDI_INT_MDDI_IN) &&
	     ((int_reg == 0) || (status_reg & MDDI_STAT_CLIENT_WAKEUP_REQ)))) {
		if (int_reg & MDDI_INT_MDDI_IN)
			pmhctl->int_type.in_count++;
		mddi_process_client_initiated_wakeup();
	}
#endif

	if (int_reg & MDDI_INT_LINK_STATE_CHANGES) {
		pmhctl->int_type.state_change_count++;
		mddi_report_state_change(int_reg);
	}

	if (int_reg & MDDI_INT_PRI_LINK_LIST_DONE) {
		pmhctl->int_type.ll_done_count++;
		mddi_process_link_list_done();
	}
#ifndef FEATURE_MDDI_DISABLE_REVERSE
	if (int_reg & MDDI_INT_REV_DATA_AVAIL) {
		pmhctl->int_type.rev_avail_count++;
		mddi_process_rev_packets();
	}
#endif

	if (int_reg & MDDI_INT_ERROR_CONDITIONS) {
		pmhctl->int_type.error_count++;
		mddi_report_errors(int_reg);

		mddi_host_reg_out(INT, int_reg & MDDI_INT_ERROR_CONDITIONS);
	}
#ifndef FEATURE_MDDI_DISABLE_REVERSE
	mddi_issue_reverse_encapsulation();

	if ((pmhctl->rev_state != MDDI_REV_ENCAP_ISSUED) &&
	    (pmhctl->rev_state != MDDI_REV_STATUS_REQ_ISSUED))
#endif
		
		mddi_queue_forward_linked_list();

	if (int_reg & MDDI_INT_NO_CMD_PKTS_PEND) {
		
		mddi_host_reg_outm(INTEN, MDDI_INT_NO_CMD_PKTS_PEND, 0);
	}

	if ((!mddi_host_mdp_active_flag) &&
	    (!mddi_vsync_detect_enabled) &&
	    (pmhctl->llist_info.transmitting_start_idx == UNASSIGNED_INDEX) &&
	    (pmhctl->llist_info.waiting_start_idx == UNASSIGNED_INDEX) &&
	    (pmhctl->rev_state == MDDI_REV_IDLE)) {
		if (pmhctl->link_state == MDDI_LINK_HIBERNATING) {
			mddi_host_disable_io_clock();
			mddi_host_disable_hclk();
		}
#ifdef FEATURE_MDDI_HOST_ENABLE_EARLY_HIBERNATION
		else if ((pmhctl->link_state == MDDI_LINK_ACTIVE) &&
			 (!pmhctl->disable_hibernation)) {
			mddi_host_reg_out(CMD, MDDI_CMD_POWERDOWN);
		}
#endif
	}
}

static void mddi_host_isr_primary(void)
{
	mddi_curr_host = MDDI_HOST_PRIM;
	mddi_host_isr();
}

irqreturn_t mddi_pmdh_isr_proxy(int irq, void *ptr)
{
	mddi_host_isr_primary();
	return IRQ_HANDLED;
}

static void mddi_host_isr_external(void)
{
	mddi_curr_host = MDDI_HOST_EXT;
	mddi_host_isr();
	mddi_curr_host = MDDI_HOST_PRIM;
}

irqreturn_t mddi_emdh_isr_proxy(int irq, void *ptr)
{
	mddi_host_isr_external();
	return IRQ_HANDLED;
}

static void mddi_host_initialize_registers(mddi_host_type host_idx)
{
	uint32 pad_reg_val;
	mddi_host_cntl_type *pmhctl = &(mhctl[host_idx]);

	if (pmhctl->driver_state == MDDI_DRIVER_ENABLED)
		return;

	
	mddi_host_enable_hclk();

	
	mddi_host_reg_out(CMD, MDDI_CMD_RESET);

	
	mddi_host_reg_out(VERSION, 0x0001);

	
	mddi_host_reg_out(BPS, MDDI_HOST_BYTES_PER_SUBFRAME);

	
	mddi_host_reg_out(SPM, 0x0003);

	
	mddi_host_reg_out(TA1_LEN, 0x0005);

	
	mddi_host_reg_out(TA2_LEN, MDDI_HOST_TA2_LEN);

	
	mddi_host_reg_out(DRIVE_HI, 0x0096);

	
	mddi_host_reg_out(DRIVE_LO, 0x0032);

	
	mddi_host_reg_out(DISP_WAKE, 0x003c);

	
	mddi_host_reg_out(REV_RATE_DIV, MDDI_HOST_REV_RATE_DIV);

#ifndef FEATURE_MDDI_DISABLE_REVERSE
	
	mddi_host_reg_out(REV_SIZE, MDDI_REV_BUFFER_SIZE);

	
	mddi_host_reg_out(REV_ENCAP_SZ, pmhctl->rev_pkt_size);
#endif

	
	
	mddi_host_reg_out(CMD, MDDI_CMD_PERIODIC_REV_ENCAP);

	pad_reg_val = mddi_host_reg_in(PAD_CTL);
	if (pad_reg_val == 0) {
		mddi_host_reg_out(PAD_CTL, 0x08000);
		udelay(5);
	}
#ifdef T_MSM7200
	
	mddi_host_reg_out(PAD_CTL, 0xa850a);
#else
	
	mddi_host_reg_out(PAD_CTL, 0xa850f);
#endif

	pad_reg_val = 0x00220020;

#if defined(CONFIG_FB_MSM_MDP31) || defined(CONFIG_FB_MSM_MDP40)
	mddi_host_reg_out(PAD_IO_CTL, 0x00320000);
	mddi_host_reg_out(PAD_CAL, pad_reg_val);
#endif

	mddi_host_core_version = mddi_host_reg_inm(CORE_VER, 0xffff);

#ifndef FEATURE_MDDI_DISABLE_REVERSE
	if (mddi_host_core_version >= 8)
		mddi_rev_ptr_workaround = FALSE;
	pmhctl->rev_ptr_curr = pmhctl->rev_ptr_start;
#endif

	if ((mddi_host_core_version > 8) && (mddi_host_core_version < 0x19))
		mddi_host_reg_out(TEST, 0x2);

	
	mddi_host_reg_out(DRIVER_START_CNT, 0x60006);

#ifndef T_MSM7500
	
	mddi_host_reg_out(MDP_VID_FMT_DES, 0x5666);
	mddi_host_reg_out(MDP_VID_PIX_ATTR, 0x00C3);
	mddi_host_reg_out(MDP_VID_CLIENTID, 0);
#endif

	
	if (pmhctl->disable_hibernation)
		mddi_host_reg_out(CMD, MDDI_CMD_HIBERNATE);
	else
		mddi_host_reg_out(CMD, MDDI_CMD_HIBERNATE | 1);

	
#ifdef MDDI_HOST_DISP_LISTEN
	mddi_host_reg_out(CMD, MDDI_CMD_DISP_LISTEN);
#else
	mddi_host_reg_out(CMD, MDDI_CMD_DISP_IGNORE);
#endif

}

void mddi_host_configure_interrupts(mddi_host_type host_idx, boolean enable)
{
	unsigned long flags;
	mddi_host_cntl_type *pmhctl = &(mhctl[host_idx]);

	spin_lock_irqsave(&mddi_host_spin_lock, flags);

	
	mddi_host_enable_hclk();
	
	mddi_host_reg_out(INTEN, 0);

	spin_unlock_irqrestore(&mddi_host_spin_lock, flags);

	if (enable) {
		pmhctl->driver_state = MDDI_DRIVER_ENABLED;

		if (host_idx == MDDI_HOST_PRIM) {
			if (request_irq
			    (INT_MDDI_PRI, mddi_pmdh_isr_proxy, IRQF_DISABLED,
			     "PMDH", 0) != 0)
				printk(KERN_ERR
				       "a mddi: unable to request_irq\n");
			else {
				int_mddi_pri_flag = TRUE;
				irq_enabled = 1;
			}
		} else {
			if (request_irq
			    (INT_MDDI_EXT, mddi_emdh_isr_proxy, IRQF_DISABLED,
			     "EMDH", 0) != 0)
				printk(KERN_ERR
				       "b mddi: unable to request_irq\n");
			else
				int_mddi_ext_flag = TRUE;
		}

		
#ifdef FEATURE_MDDI_DISABLE_REVERSE
		mddi_host_reg_out(INTEN,
				  MDDI_INT_ERROR_CONDITIONS |
				  MDDI_INT_LINK_STATE_CHANGES);
#else
		
		pmhctl->rev_ptr_written = FALSE;

		mddi_host_reg_out(INTEN,
				  MDDI_INT_REV_DATA_AVAIL |
				  MDDI_INT_ERROR_CONDITIONS |
				  MDDI_INT_LINK_STATE_CHANGES);
		pmhctl->rtd_counter = mddi_rtd_frequency;
		pmhctl->client_status_cnt = 0;
#endif
	} else {
		if (pmhctl->driver_state == MDDI_DRIVER_ENABLED)
			pmhctl->driver_state = MDDI_DRIVER_DISABLED;
	}

}

void mddi_host_client_cnt_reset(void)
{
	unsigned long flags;
	mddi_host_cntl_type *pmhctl;

	pmhctl = &(mhctl[MDDI_HOST_PRIM]);
	spin_lock_irqsave(&mddi_host_spin_lock, flags);
	pmhctl->client_status_cnt = 0;
	spin_unlock_irqrestore(&mddi_host_spin_lock, flags);
}

static void mddi_host_powerup(mddi_host_type host_idx)
{
	mddi_host_cntl_type *pmhctl = &(mhctl[host_idx]);

	if (pmhctl->link_state != MDDI_LINK_DISABLED)
		return;

	
	mddi_host_enable_io_clock();

	mddi_host_initialize_registers(host_idx);
	mddi_host_configure_interrupts(host_idx, TRUE);

	pmhctl->link_state = MDDI_LINK_ACTIVATING;

	
	mddi_host_reg_out(CMD, MDDI_CMD_LINK_ACTIVE);

#ifdef CLKRGM_MDDI_IO_CLOCK_IN_MHZ
	MDDI_MSG_NOTICE("MDDI Host: Activating Link %d Mbps\n",
			CLKRGM_MDDI_IO_CLOCK_IN_MHZ * 2);
#else
	MDDI_MSG_NOTICE("MDDI Host: Activating Link\n");
#endif

	
	if (host_idx == MDDI_HOST_PRIM)
		mddi_host_timer_service(0);
}

void mddi_send_fw_link_skew_cal(mddi_host_type host_idx)
{
	mddi_host_reg_out(CMD, MDDI_CMD_FW_LINK_SKEW_CAL);
	MDDI_MSG_DEBUG("%s: Skew Calibration done!!\n", __func__);
}


void mddi_host_init(mddi_host_type host_idx)
{
	static boolean initialized = FALSE;
	mddi_host_cntl_type *pmhctl;

	if (host_idx >= MDDI_NUM_HOST_CORES) {
		MDDI_MSG_ERR("Invalid host core index\n");
		return;
	}

	if (!initialized) {
		uint16 idx;
		mddi_host_type host;

		for (host = MDDI_HOST_PRIM; host < MDDI_NUM_HOST_CORES; host++) {
			pmhctl = &(mhctl[host]);
			initialized = TRUE;

			pmhctl->llist_ptr =
			    dma_alloc_coherent(NULL, MDDI_LLIST_POOL_SIZE,
					       &(pmhctl->llist_dma_addr),
					       GFP_KERNEL);
			pmhctl->llist_dma_ptr =
			    (mddi_linked_list_type *) (void *)pmhctl->
			    llist_dma_addr;
#ifdef FEATURE_MDDI_DISABLE_REVERSE
			pmhctl->rev_data_buf = NULL;
			if (pmhctl->llist_ptr == NULL)
#else
			mddi_rev_user.waiting = FALSE;
			init_completion(&(mddi_rev_user.done_comp));
			pmhctl->rev_data_buf =
			    dma_alloc_coherent(NULL, MDDI_MAX_REV_DATA_SIZE,
					       &(pmhctl->rev_data_dma_addr),
					       GFP_KERNEL);
			if ((pmhctl->llist_ptr == NULL)
			    || (pmhctl->rev_data_buf == NULL))
#endif
			{
				MDDI_MSG_CRIT
				    ("unable to alloc non-cached memory\n");
			}
			llist_extern[host] = pmhctl->llist_ptr;
			llist_dma_extern[host] = pmhctl->llist_dma_ptr;
			llist_extern_notify[host] = pmhctl->llist_notify;

			for (idx = 0; idx < UNASSIGNED_INDEX; idx++) {
				init_completion(&
						(pmhctl->llist_notify[idx].
						 done_comp));
			}
			init_completion(&(pmhctl->mddi_llist_avail_comp));
			spin_lock_init(&mddi_host_spin_lock);
			pmhctl->mddi_waiting_for_llist_avail = FALSE;
			pmhctl->mddi_rev_ptr_write_val =
			    (uint32) (void *)(pmhctl->rev_data_dma_addr);
			pmhctl->rev_ptr_start = (void *)pmhctl->rev_data_buf;

			pmhctl->rev_pkt_size = MDDI_DEFAULT_REV_PKT_SIZE;
			pmhctl->rev_state = MDDI_REV_IDLE;
#ifdef IMAGE_MODEM_PROC
			pmhctl->link_state = MDDI_LINK_HIBERNATING;
#else
			pmhctl->link_state = MDDI_LINK_DISABLED;
#endif
			pmhctl->driver_state = MDDI_DRIVER_DISABLED;
			pmhctl->disable_hibernation = FALSE;

			
			pmhctl->llist_info.transmitting_start_idx =
			    UNASSIGNED_INDEX;
			pmhctl->llist_info.transmitting_end_idx =
			    UNASSIGNED_INDEX;
			pmhctl->llist_info.waiting_start_idx = UNASSIGNED_INDEX;
			pmhctl->llist_info.waiting_end_idx = UNASSIGNED_INDEX;
			pmhctl->llist_info.reg_read_idx = UNASSIGNED_INDEX;
			pmhctl->llist_info.next_free_idx =
			    MDDI_FIRST_DYNAMIC_LLIST_IDX;
			pmhctl->llist_info.reg_read_waiting = FALSE;

			mddi_vsync_detect_enabled = FALSE;
			mddi_gpio.polling_enabled = FALSE;

			pmhctl->int_type.count = 0;
			pmhctl->int_type.in_count = 0;
			pmhctl->int_type.disp_req_count = 0;
			pmhctl->int_type.state_change_count = 0;
			pmhctl->int_type.ll_done_count = 0;
			pmhctl->int_type.rev_avail_count = 0;
			pmhctl->int_type.error_count = 0;
			pmhctl->int_type.rev_encap_count = 0;
			pmhctl->int_type.llist_ptr_write_1 = 0;
			pmhctl->int_type.llist_ptr_write_2 = 0;

			pmhctl->stats.fwd_crc_count = 0;
			pmhctl->stats.rev_crc_count = 0;
			pmhctl->stats.pri_underflow = 0;
			pmhctl->stats.sec_underflow = 0;
			pmhctl->stats.rev_overflow = 0;
			pmhctl->stats.pri_overwrite = 0;
			pmhctl->stats.sec_overwrite = 0;
			pmhctl->stats.rev_overwrite = 0;
			pmhctl->stats.dma_failure = 0;
			pmhctl->stats.rtd_failure = 0;
			pmhctl->stats.reg_read_failure = 0;
#ifdef FEATURE_MDDI_UNDERRUN_RECOVERY
			pmhctl->stats.pri_underrun_detected = 0;
#endif

			pmhctl->log_parms.rtd_cnt = 0;
			pmhctl->log_parms.rev_enc_cnt = 0;
			pmhctl->log_parms.vid_cnt = 0;
			pmhctl->log_parms.reg_acc_cnt = 0;
			pmhctl->log_parms.cli_stat_cnt = 0;
			pmhctl->log_parms.cli_cap_cnt = 0;
			pmhctl->log_parms.reg_read_cnt = 0;
			pmhctl->log_parms.link_active_cnt = 0;
			pmhctl->log_parms.link_hibernate_cnt = 0;
			pmhctl->log_parms.fwd_crc_cnt = 0;
			pmhctl->log_parms.rev_crc_cnt = 0;
			pmhctl->log_parms.vsync_response_cnt = 0;

			prev_parms[host_idx] = pmhctl->log_parms;
			mddi_client_capability_pkt.packet_length = 0;
		}

#ifndef T_MSM7500
		
		MDDI_HOST_ENABLE_IO_CLOCK;
#endif
	}

	mddi_host_powerup(host_idx);
	pmhctl = &(mhctl[host_idx]);
}

#ifdef CONFIG_FB_MSM_MDDI_AUTO_DETECT
static uint32 mddi_client_id;

uint32 mddi_get_client_id(void)
{

#ifndef FEATURE_MDDI_DISABLE_REVERSE
	mddi_host_type host_idx = MDDI_HOST_PRIM;
	static boolean client_detection_try = FALSE;
	mddi_host_cntl_type *pmhctl;
	unsigned long flags;
	uint16 saved_rev_pkt_size;
	int ret;

	if (!client_detection_try) {
		
		mddi_host_reg_out(DRIVE_LO, 0x0050);

		pmhctl = &(mhctl[MDDI_HOST_PRIM]);

		saved_rev_pkt_size = pmhctl->rev_pkt_size;

		
		pmhctl->rev_pkt_size = MDDI_CLIENT_CAPABILITY_REV_PKT_SIZE;
		mddi_host_reg_out(REV_ENCAP_SZ, pmhctl->rev_pkt_size);

		
		if (!pmhctl->disable_hibernation)
			mddi_host_reg_out(CMD, MDDI_CMD_HIBERNATE);

		mddi_rev_user.waiting = TRUE;
		INIT_COMPLETION(mddi_rev_user.done_comp);

		spin_lock_irqsave(&mddi_host_spin_lock, flags);

		
		mddi_host_enable_hclk();
		mddi_host_enable_io_clock();

		mddi_client_capability_request = TRUE;

		if (pmhctl->rev_state == MDDI_REV_IDLE) {
			
			mddi_issue_reverse_encapsulation();
		}
		spin_unlock_irqrestore(&mddi_host_spin_lock, flags);

		wait_for_completion_killable(&(mddi_rev_user.done_comp));

		
		pmhctl->rev_pkt_size = saved_rev_pkt_size;
		mddi_host_reg_out(REV_ENCAP_SZ, pmhctl->rev_pkt_size);

		
		if (!pmhctl->disable_hibernation)
			mddi_host_reg_out(CMD, MDDI_CMD_HIBERNATE | 1);

		mddi_host_reg_out(DRIVE_LO, 0x0032);
		client_detection_try = TRUE;

		mddi_client_id = (mddi_client_capability_pkt.Mfr_Name<<16) |
				mddi_client_capability_pkt.Product_Code;

		if (!mddi_client_id)
			mddi_disable(1);

		ret = mddi_client_power(mddi_client_id);
		if (ret < 0)
			MDDI_MSG_ERR("mddi_client_power return %d", ret);
	}

#endif

	return mddi_client_id;
}
#endif

void mddi_host_powerdown(mddi_host_type host_idx)
{
	mddi_host_cntl_type *pmhctl = &(mhctl[host_idx]);

	if (host_idx >= MDDI_NUM_HOST_CORES) {
		MDDI_MSG_ERR("Invalid host core index\n");
		return;
	}

	if (pmhctl->driver_state == MDDI_DRIVER_RESET) {
		return;
	}

	if (host_idx == MDDI_HOST_PRIM) {
		
		del_timer(&mddi_host_timer);
	}

	mddi_host_configure_interrupts(host_idx, FALSE);

	
	mddi_host_enable_hclk();

	
	mddi_host_reg_out(CMD, MDDI_CMD_RESET);

	
	mddi_host_reg_out(PAD_CTL, 0x0);

	
	mddi_host_disable_io_clock();
	mddi_host_disable_hclk();

	pmhctl->link_state = MDDI_LINK_DISABLED;
	pmhctl->driver_state = MDDI_DRIVER_RESET;

	MDDI_MSG_NOTICE("MDDI Host: Disabling Link\n");

}

uint16 mddi_get_next_free_llist_item(mddi_host_type host_idx, boolean wait)
{
	unsigned long flags;
	uint16 ret_idx;
	boolean forced_wait = FALSE;
	mddi_host_cntl_type *pmhctl = &(mhctl[host_idx]);

	ret_idx = pmhctl->llist_info.next_free_idx;

	pmhctl->llist_info.next_free_idx++;
	if (pmhctl->llist_info.next_free_idx >= MDDI_NUM_DYNAMIC_LLIST_ITEMS)
		pmhctl->llist_info.next_free_idx = MDDI_FIRST_DYNAMIC_LLIST_IDX;
	spin_lock_irqsave(&mddi_host_spin_lock, flags);
	if (pmhctl->llist_notify[ret_idx].in_use) {
		if (!wait) {
			pmhctl->llist_info.next_free_idx = ret_idx;
			ret_idx = UNASSIGNED_INDEX;
		} else {
			forced_wait = TRUE;
			INIT_COMPLETION(pmhctl->mddi_llist_avail_comp);
		}
	}
	spin_unlock_irqrestore(&mddi_host_spin_lock, flags);

	if (forced_wait) {
		wait_for_completion_killable(&
						  (pmhctl->
						   mddi_llist_avail_comp));
		MDDI_MSG_ERR("task waiting on mddi llist item\n");
	}

	if (ret_idx != UNASSIGNED_INDEX) {
		pmhctl->llist_notify[ret_idx].waiting = FALSE;
		pmhctl->llist_notify[ret_idx].done_cb = NULL;
		pmhctl->llist_notify[ret_idx].in_use = TRUE;
		pmhctl->llist_notify[ret_idx].next_idx = UNASSIGNED_INDEX;
	}

	return ret_idx;
}

uint16 mddi_get_reg_read_llist_item(mddi_host_type host_idx, boolean wait)
{
#ifdef FEATURE_MDDI_DISABLE_REVERSE
	MDDI_MSG_CRIT("No reverse link available\n");
	(void)wait;
	return FALSE;
#else
	unsigned long flags;
	uint16 ret_idx;
	boolean error = FALSE;
	mddi_host_cntl_type *pmhctl = &(mhctl[host_idx]);

	spin_lock_irqsave(&mddi_host_spin_lock, flags);
	if (pmhctl->llist_info.reg_read_idx != UNASSIGNED_INDEX) {
		
		error = TRUE;
		ret_idx = UNASSIGNED_INDEX;
	}
	spin_unlock_irqrestore(&mddi_host_spin_lock, flags);

	if (!error) {
		ret_idx = pmhctl->llist_info.reg_read_idx =
		    mddi_get_next_free_llist_item(host_idx, wait);
		
		pmhctl->llist_info.reg_read_waiting = FALSE;
	}

	if (error)
		MDDI_MSG_ERR("***** Reg read still in progress! ****\n");
	return ret_idx;
#endif

}

void mddi_queue_forward_packets(uint16 first_llist_idx,
				uint16 last_llist_idx,
				boolean wait,
				mddi_llist_done_cb_type llist_done_cb,
				mddi_host_type host_idx)
{
	unsigned long flags;
	mddi_linked_list_type *llist;
	mddi_linked_list_type *llist_dma;
	mddi_host_cntl_type *pmhctl = &(mhctl[host_idx]);

	if ((first_llist_idx >= UNASSIGNED_INDEX) ||
	    (last_llist_idx >= UNASSIGNED_INDEX)) {
		MDDI_MSG_ERR("MDDI queueing invalid linked list\n");
		return;
	}

	if (pmhctl->link_state == MDDI_LINK_DISABLED)
		MDDI_MSG_CRIT("MDDI host powered down!\n");

	llist = pmhctl->llist_ptr;
	llist_dma = pmhctl->llist_dma_ptr;

	
	memory_barrier();

	pmhctl->llist_notify[last_llist_idx].waiting = wait;
	if (wait)
		INIT_COMPLETION(pmhctl->llist_notify[last_llist_idx].done_comp);
	pmhctl->llist_notify[last_llist_idx].done_cb = llist_done_cb;

	spin_lock_irqsave(&mddi_host_spin_lock, flags);

	if ((pmhctl->llist_info.transmitting_start_idx == UNASSIGNED_INDEX) &&
	    (pmhctl->llist_info.waiting_start_idx == UNASSIGNED_INDEX) &&
	    (pmhctl->rev_state == MDDI_REV_IDLE)) {
		
#ifndef FEATURE_MDDI_DISABLE_REVERSE
		if (first_llist_idx == pmhctl->llist_info.reg_read_idx) {
			
			pmhctl->rev_state = MDDI_REV_REG_READ_ISSUED;
			mddi_reg_read_retry = 0;
			
		}
#endif
		
		pmhctl->llist_info.transmitting_start_idx = first_llist_idx;
		pmhctl->llist_info.transmitting_end_idx = last_llist_idx;

		
		mddi_host_enable_hclk();
		mddi_host_enable_io_clock();
		pmhctl->int_type.llist_ptr_write_1++;
		
		dma_coherent_pre_ops();
		mddi_host_reg_out(PRI_PTR, &llist_dma[first_llist_idx]);

		
		mddi_host_reg_outm(INTEN, MDDI_INT_PRI_LINK_LIST_DONE,
				   MDDI_INT_PRI_LINK_LIST_DONE);

	} else if (pmhctl->llist_info.waiting_start_idx == UNASSIGNED_INDEX) {
#ifndef FEATURE_MDDI_DISABLE_REVERSE
		if (first_llist_idx == pmhctl->llist_info.reg_read_idx) {
			
			pmhctl->llist_info.reg_read_waiting = TRUE;
		}
#endif

		
		pmhctl->llist_info.waiting_start_idx = first_llist_idx;
		pmhctl->llist_info.waiting_end_idx = last_llist_idx;
	} else {
		uint16 prev_end_idx = pmhctl->llist_info.waiting_end_idx;
#ifndef FEATURE_MDDI_DISABLE_REVERSE
		if (first_llist_idx == pmhctl->llist_info.reg_read_idx) {
			
			pmhctl->llist_info.reg_read_waiting = TRUE;
		}
#endif

		llist = pmhctl->llist_ptr;

		
		llist[prev_end_idx].link_controller_flags = 0;
		pmhctl->llist_notify[prev_end_idx].next_idx = first_llist_idx;

		
		llist[prev_end_idx].next_packet_pointer =
		    (void *)(&llist_dma[first_llist_idx]);

		
		memory_barrier();

		
		pmhctl->llist_info.waiting_end_idx = last_llist_idx;
	}

	spin_unlock_irqrestore(&mddi_host_spin_lock, flags);

}

void mddi_host_write_pix_attr_reg(uint32 value)
{
	(void)value;
}

void mddi_queue_reverse_encapsulation(boolean wait)
{
#ifdef FEATURE_MDDI_DISABLE_REVERSE
	MDDI_MSG_CRIT("No reverse link available\n");
	(void)wait;
#else
	unsigned long flags;
	boolean error = FALSE;
	mddi_host_type host_idx = MDDI_HOST_PRIM;
	mddi_host_cntl_type *pmhctl = &(mhctl[MDDI_HOST_PRIM]);

	spin_lock_irqsave(&mddi_host_spin_lock, flags);

	
	mddi_host_enable_hclk();
	mddi_host_enable_io_clock();

	if (wait) {
		if (!mddi_rev_user.waiting) {
			mddi_rev_user.waiting = TRUE;
			INIT_COMPLETION(mddi_rev_user.done_comp);
		} else
			error = TRUE;
	}
	mddi_rev_encap_user_request = TRUE;

	if (pmhctl->rev_state == MDDI_REV_IDLE) {
		
		mddi_host_type orig_host_idx = mddi_curr_host;
		mddi_curr_host = host_idx;
		mddi_issue_reverse_encapsulation();
		mddi_curr_host = orig_host_idx;
	}
	spin_unlock_irqrestore(&mddi_host_spin_lock, flags);

	if (error) {
		MDDI_MSG_ERR("Reverse Encap request already in progress\n");
	} else if (wait)
		wait_for_completion_killable(&(mddi_rev_user.done_comp));
#endif
}

boolean mddi_set_rev_handler(mddi_rev_handler_type handler, uint16 pkt_type)
{
#ifdef FEATURE_MDDI_DISABLE_REVERSE
	MDDI_MSG_CRIT("No reverse link available\n");
	(void)handler;
	(void)pkt_type;
	return (FALSE);
#else
	unsigned long flags;
	uint16 hdlr;
	boolean handler_set = FALSE;
	boolean overwrite = FALSE;
	mddi_host_type host_idx = MDDI_HOST_PRIM;
	mddi_host_cntl_type *pmhctl = &(mhctl[MDDI_HOST_PRIM]);

	
	spin_lock_irqsave(&mddi_host_spin_lock, flags);

	for (hdlr = 0; hdlr < MAX_MDDI_REV_HANDLERS; hdlr++) {
		if (mddi_rev_pkt_handler[hdlr].pkt_type == pkt_type) {
			mddi_rev_pkt_handler[hdlr].handler = handler;
			if (handler == NULL) {
				
				mddi_rev_pkt_handler[hdlr].pkt_type =
				    INVALID_PKT_TYPE;
				handler_set = TRUE;
				if (pkt_type == 0x10) {	
					
					mddi_host_enable_hclk();
					
					pmhctl->rev_pkt_size =
					    MDDI_DEFAULT_REV_PKT_SIZE;
					mddi_host_reg_out(REV_ENCAP_SZ,
							  pmhctl->rev_pkt_size);
				}
			} else {
				
				overwrite = TRUE;
			}
			break;
		}
	}
	if ((hdlr >= MAX_MDDI_REV_HANDLERS) && (handler != NULL)) {
		
		for (hdlr = 0; hdlr < MAX_MDDI_REV_HANDLERS; hdlr++) {
			if (mddi_rev_pkt_handler[hdlr].pkt_type ==
			    INVALID_PKT_TYPE) {
				if ((pkt_type == 0x10) &&	
				    (pmhctl->rev_pkt_size <
				     MDDI_VIDEO_REV_PKT_SIZE)) {
					
					mddi_host_enable_hclk();
					
					pmhctl->rev_pkt_size =
					    MDDI_VIDEO_REV_PKT_SIZE;
					mddi_host_reg_out(REV_ENCAP_SZ,
							  pmhctl->rev_pkt_size);
				}
				mddi_rev_pkt_handler[hdlr].handler = handler;
				mddi_rev_pkt_handler[hdlr].pkt_type = pkt_type;
				handler_set = TRUE;
				break;
			}
		}
	}

	
	spin_unlock_irqrestore(&mddi_host_spin_lock, flags);

	if (overwrite)
		MDDI_MSG_ERR("Overwriting previous rev packet handler\n");

	return handler_set;

#endif
}				

void mddi_host_disable_hibernation(boolean disable)
{
	mddi_host_type host_idx = MDDI_HOST_PRIM;
	mddi_host_cntl_type *pmhctl = &(mhctl[MDDI_HOST_PRIM]);

	if (disable) {
		pmhctl->disable_hibernation = TRUE;
		
	} else {
		if (pmhctl->disable_hibernation) {
			unsigned long flags;
			spin_lock_irqsave(&mddi_host_spin_lock, flags);
			if (!MDDI_HOST_IS_HCLK_ON)
				MDDI_HOST_ENABLE_HCLK;
			mddi_host_reg_out(CMD, MDDI_CMD_HIBERNATE | 1);
			spin_unlock_irqrestore(&mddi_host_spin_lock, flags);
			pmhctl->disable_hibernation = FALSE;
		}
	}
}

void mddi_mhctl_remove(mddi_host_type host_idx)
{
	mddi_host_cntl_type *pmhctl;

	pmhctl = &(mhctl[host_idx]);

	dma_free_coherent(NULL, MDDI_LLIST_POOL_SIZE, (void *)pmhctl->llist_ptr,
			  pmhctl->llist_dma_addr);

	dma_free_coherent(NULL, MDDI_MAX_REV_DATA_SIZE,
			  (void *)pmhctl->rev_data_buf,
			  pmhctl->rev_data_dma_addr);
}
