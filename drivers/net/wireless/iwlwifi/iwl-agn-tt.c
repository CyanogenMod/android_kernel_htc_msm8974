/******************************************************************************
 *
 * Copyright(c) 2007 - 2012 Intel Corporation. All rights reserved.
 *
 * Portions of this file are derived from the ipw3945 project, as well
 * as portions of the ieee80211 subsystem header files.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110, USA
 *
 * The full GNU General Public License is included in this distribution in the
 * file called LICENSE.
 *
 * Contact Information:
 *  Intel Linux Wireless <ilw@linux.intel.com>
 * Intel Corporation, 5200 N.E. Elam Young Parkway, Hillsboro, OR 97124-6497
 *****************************************************************************/


#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/init.h>

#include <net/mac80211.h>

#include "iwl-agn.h"
#include "iwl-eeprom.h"
#include "iwl-dev.h"
#include "iwl-core.h"
#include "iwl-io.h"
#include "iwl-commands.h"
#include "iwl-debug.h"
#include "iwl-agn-tt.h"

static const struct iwl_tt_trans tt_range_0[IWL_TI_STATE_MAX - 1] = {
	{IWL_TI_0, IWL_ABSOLUTE_ZERO, 104},
	{IWL_TI_1, 105, CT_KILL_THRESHOLD - 1},
	{IWL_TI_CT_KILL, CT_KILL_THRESHOLD, IWL_ABSOLUTE_MAX}
};
static const struct iwl_tt_trans tt_range_1[IWL_TI_STATE_MAX - 1] = {
	{IWL_TI_0, IWL_ABSOLUTE_ZERO, 95},
	{IWL_TI_2, 110, CT_KILL_THRESHOLD - 1},
	{IWL_TI_CT_KILL, CT_KILL_THRESHOLD, IWL_ABSOLUTE_MAX}
};
static const struct iwl_tt_trans tt_range_2[IWL_TI_STATE_MAX - 1] = {
	{IWL_TI_1, IWL_ABSOLUTE_ZERO, 100},
	{IWL_TI_CT_KILL, CT_KILL_THRESHOLD, IWL_ABSOLUTE_MAX},
	{IWL_TI_CT_KILL, CT_KILL_THRESHOLD, IWL_ABSOLUTE_MAX}
};
static const struct iwl_tt_trans tt_range_3[IWL_TI_STATE_MAX - 1] = {
	{IWL_TI_0, IWL_ABSOLUTE_ZERO, CT_KILL_EXIT_THRESHOLD},
	{IWL_TI_CT_KILL, CT_KILL_EXIT_THRESHOLD + 1, IWL_ABSOLUTE_MAX},
	{IWL_TI_CT_KILL, CT_KILL_EXIT_THRESHOLD + 1, IWL_ABSOLUTE_MAX}
};

static const struct iwl_tt_restriction restriction_range[IWL_TI_STATE_MAX] = {
	{IWL_ANT_OK_MULTI, IWL_ANT_OK_MULTI, true },
	{IWL_ANT_OK_SINGLE, IWL_ANT_OK_MULTI, true },
	{IWL_ANT_OK_SINGLE, IWL_ANT_OK_SINGLE, false },
	{IWL_ANT_OK_NONE, IWL_ANT_OK_NONE, false }
};

bool iwl_tt_is_low_power_state(struct iwl_priv *priv)
{
	struct iwl_tt_mgmt *tt = &priv->thermal_throttle;

	if (tt->state >= IWL_TI_1)
		return true;
	return false;
}

u8 iwl_tt_current_power_mode(struct iwl_priv *priv)
{
	struct iwl_tt_mgmt *tt = &priv->thermal_throttle;

	return tt->tt_power_mode;
}

bool iwl_ht_enabled(struct iwl_priv *priv)
{
	struct iwl_tt_mgmt *tt = &priv->thermal_throttle;
	struct iwl_tt_restriction *restriction;

	if (!priv->thermal_throttle.advanced_tt)
		return true;
	restriction = tt->restriction + tt->state;
	return restriction->is_ht;
}

static bool iwl_within_ct_kill_margin(struct iwl_priv *priv)
{
	s32 temp = priv->temperature; 
	bool within_margin = false;

	if (!priv->thermal_throttle.advanced_tt)
		within_margin = ((temp + IWL_TT_CT_KILL_MARGIN) >=
				CT_KILL_THRESHOLD_LEGACY) ? true : false;
	else
		within_margin = ((temp + IWL_TT_CT_KILL_MARGIN) >=
				CT_KILL_THRESHOLD) ? true : false;
	return within_margin;
}

bool iwl_check_for_ct_kill(struct iwl_priv *priv)
{
	bool is_ct_kill = false;

	if (iwl_within_ct_kill_margin(priv)) {
		iwl_tt_enter_ct_kill(priv);
		is_ct_kill = true;
	}
	return is_ct_kill;
}

enum iwl_antenna_ok iwl_tx_ant_restriction(struct iwl_priv *priv)
{
	struct iwl_tt_mgmt *tt = &priv->thermal_throttle;
	struct iwl_tt_restriction *restriction;

	if (!priv->thermal_throttle.advanced_tt)
		return IWL_ANT_OK_MULTI;
	restriction = tt->restriction + tt->state;
	return restriction->tx_stream;
}

enum iwl_antenna_ok iwl_rx_ant_restriction(struct iwl_priv *priv)
{
	struct iwl_tt_mgmt *tt = &priv->thermal_throttle;
	struct iwl_tt_restriction *restriction;

	if (!priv->thermal_throttle.advanced_tt)
		return IWL_ANT_OK_MULTI;
	restriction = tt->restriction + tt->state;
	return restriction->rx_stream;
}

#define CT_KILL_EXIT_DURATION (5)	
#define CT_KILL_WAITING_DURATION (300)	

static void iwl_tt_check_exit_ct_kill(unsigned long data)
{
	struct iwl_priv *priv = (struct iwl_priv *)data;
	struct iwl_tt_mgmt *tt = &priv->thermal_throttle;
	unsigned long flags;

	if (test_bit(STATUS_EXIT_PENDING, &priv->status))
		return;

	if (tt->state == IWL_TI_CT_KILL) {
		if (priv->thermal_throttle.ct_kill_toggle) {
			iwl_write32(trans(priv), CSR_UCODE_DRV_GP1_CLR,
				    CSR_UCODE_DRV_GP1_REG_BIT_CT_KILL_EXIT);
			priv->thermal_throttle.ct_kill_toggle = false;
		} else {
			iwl_write32(trans(priv), CSR_UCODE_DRV_GP1_SET,
				    CSR_UCODE_DRV_GP1_REG_BIT_CT_KILL_EXIT);
			priv->thermal_throttle.ct_kill_toggle = true;
		}
		iwl_read32(trans(priv), CSR_UCODE_DRV_GP1);
		spin_lock_irqsave(&trans(priv)->reg_lock, flags);
		if (likely(iwl_grab_nic_access(trans(priv))))
			iwl_release_nic_access(trans(priv));
		spin_unlock_irqrestore(&trans(priv)->reg_lock, flags);

		IWL_DEBUG_TEMP(priv, "schedule ct_kill exit timer\n");
		mod_timer(&priv->thermal_throttle.ct_kill_exit_tm,
			  jiffies + CT_KILL_EXIT_DURATION * HZ);
	}
}

static void iwl_perform_ct_kill_task(struct iwl_priv *priv,
			   bool stop)
{
	if (stop) {
		IWL_DEBUG_TEMP(priv, "Stop all queues\n");
		if (priv->mac80211_registered)
			ieee80211_stop_queues(priv->hw);
		IWL_DEBUG_TEMP(priv,
				"Schedule 5 seconds CT_KILL Timer\n");
		mod_timer(&priv->thermal_throttle.ct_kill_exit_tm,
			  jiffies + CT_KILL_EXIT_DURATION * HZ);
	} else {
		IWL_DEBUG_TEMP(priv, "Wake all queues\n");
		if (priv->mac80211_registered)
			ieee80211_wake_queues(priv->hw);
	}
}

static void iwl_tt_ready_for_ct_kill(unsigned long data)
{
	struct iwl_priv *priv = (struct iwl_priv *)data;
	struct iwl_tt_mgmt *tt = &priv->thermal_throttle;

	if (test_bit(STATUS_EXIT_PENDING, &priv->status))
		return;

	
	if (tt->state != IWL_TI_CT_KILL) {
		IWL_DEBUG_TEMP(priv, "entering CT_KILL state when "
				"temperature timer expired\n");
		tt->state = IWL_TI_CT_KILL;
		set_bit(STATUS_CT_KILL, &priv->status);
		iwl_perform_ct_kill_task(priv, true);
	}
}

static void iwl_prepare_ct_kill_task(struct iwl_priv *priv)
{
	IWL_DEBUG_TEMP(priv, "Prepare to enter IWL_TI_CT_KILL\n");
	
	iwl_send_statistics_request(priv, CMD_SYNC, false);
	
	mod_timer(&priv->thermal_throttle.ct_kill_waiting_tm,
		 jiffies + msecs_to_jiffies(CT_KILL_WAITING_DURATION));
}

#define IWL_MINIMAL_POWER_THRESHOLD		(CT_KILL_THRESHOLD_LEGACY)
#define IWL_REDUCED_PERFORMANCE_THRESHOLD_2	(100)
#define IWL_REDUCED_PERFORMANCE_THRESHOLD_1	(90)

static void iwl_legacy_tt_handler(struct iwl_priv *priv, s32 temp, bool force)
{
	struct iwl_tt_mgmt *tt = &priv->thermal_throttle;
	enum iwl_tt_state old_state;

#ifdef CONFIG_IWLWIFI_DEBUG
	if ((tt->tt_previous_temp) &&
	    (temp > tt->tt_previous_temp) &&
	    ((temp - tt->tt_previous_temp) >
	    IWL_TT_INCREASE_MARGIN)) {
		IWL_DEBUG_TEMP(priv,
			"Temperature increase %d degree Celsius\n",
			(temp - tt->tt_previous_temp));
	}
#endif
	old_state = tt->state;
	
	if (temp >= IWL_MINIMAL_POWER_THRESHOLD)
		tt->state = IWL_TI_CT_KILL;
	else if (temp >= IWL_REDUCED_PERFORMANCE_THRESHOLD_2)
		tt->state = IWL_TI_2;
	else if (temp >= IWL_REDUCED_PERFORMANCE_THRESHOLD_1)
		tt->state = IWL_TI_1;
	else
		tt->state = IWL_TI_0;

#ifdef CONFIG_IWLWIFI_DEBUG
	tt->tt_previous_temp = temp;
#endif
	
	del_timer_sync(&priv->thermal_throttle.ct_kill_waiting_tm);
	if (tt->state != old_state) {
		switch (tt->state) {
		case IWL_TI_0:
			break;
		case IWL_TI_1:
			tt->tt_power_mode = IWL_POWER_INDEX_3;
			break;
		case IWL_TI_2:
			tt->tt_power_mode = IWL_POWER_INDEX_4;
			break;
		default:
			tt->tt_power_mode = IWL_POWER_INDEX_5;
			break;
		}
		mutex_lock(&priv->mutex);
		if (old_state == IWL_TI_CT_KILL)
			clear_bit(STATUS_CT_KILL, &priv->status);
		if (tt->state != IWL_TI_CT_KILL &&
		    iwl_power_update_mode(priv, true)) {
			if (old_state == IWL_TI_CT_KILL)
				set_bit(STATUS_CT_KILL, &priv->status);
			tt->state = old_state;
			IWL_ERR(priv, "Cannot update power mode, "
					"TT state not updated\n");
		} else {
			if (tt->state == IWL_TI_CT_KILL) {
				if (force) {
					set_bit(STATUS_CT_KILL, &priv->status);
					iwl_perform_ct_kill_task(priv, true);
				} else {
					iwl_prepare_ct_kill_task(priv);
					tt->state = old_state;
				}
			} else if (old_state == IWL_TI_CT_KILL &&
				 tt->state != IWL_TI_CT_KILL)
				iwl_perform_ct_kill_task(priv, false);
			IWL_DEBUG_TEMP(priv, "Temperature state changed %u\n",
					tt->state);
			IWL_DEBUG_TEMP(priv, "Power Index change to %u\n",
					tt->tt_power_mode);
		}
		mutex_unlock(&priv->mutex);
	}
}

static void iwl_advance_tt_handler(struct iwl_priv *priv, s32 temp, bool force)
{
	struct iwl_tt_mgmt *tt = &priv->thermal_throttle;
	int i;
	bool changed = false;
	enum iwl_tt_state old_state;
	struct iwl_tt_trans *transaction;

	old_state = tt->state;
	for (i = 0; i < IWL_TI_STATE_MAX - 1; i++) {
		transaction = tt->transaction +
			((old_state * (IWL_TI_STATE_MAX - 1)) + i);
		if (temp >= transaction->tt_low &&
		    temp <= transaction->tt_high) {
#ifdef CONFIG_IWLWIFI_DEBUG
			if ((tt->tt_previous_temp) &&
			    (temp > tt->tt_previous_temp) &&
			    ((temp - tt->tt_previous_temp) >
			    IWL_TT_INCREASE_MARGIN)) {
				IWL_DEBUG_TEMP(priv,
					"Temperature increase %d "
					"degree Celsius\n",
					(temp - tt->tt_previous_temp));
			}
			tt->tt_previous_temp = temp;
#endif
			if (old_state !=
			    transaction->next_state) {
				changed = true;
				tt->state =
					transaction->next_state;
			}
			break;
		}
	}
	
	del_timer_sync(&priv->thermal_throttle.ct_kill_waiting_tm);
	if (changed) {
		if (tt->state >= IWL_TI_1) {
			
			tt->tt_power_mode = IWL_POWER_INDEX_5;

			if (!iwl_ht_enabled(priv)) {
				struct iwl_rxon_context *ctx;

				for_each_context(priv, ctx) {
					struct iwl_rxon_cmd *rxon;

					rxon = &ctx->staging;

					
					rxon->flags &= ~(
						RXON_FLG_CHANNEL_MODE_MSK |
						RXON_FLG_CTRL_CHANNEL_LOC_HI_MSK |
						RXON_FLG_HT40_PROT_MSK |
						RXON_FLG_HT_PROT_MSK);
				}
			} else {
				iwl_set_rxon_ht(priv, &priv->current_ht_config);
			}

		} else {

			iwl_set_rxon_ht(priv, &priv->current_ht_config);
		}
		mutex_lock(&priv->mutex);
		if (old_state == IWL_TI_CT_KILL)
			clear_bit(STATUS_CT_KILL, &priv->status);
		if (tt->state != IWL_TI_CT_KILL &&
		    iwl_power_update_mode(priv, true)) {
			IWL_ERR(priv, "Cannot update power mode, "
					"TT state not updated\n");
			if (old_state == IWL_TI_CT_KILL)
				set_bit(STATUS_CT_KILL, &priv->status);
			tt->state = old_state;
		} else {
			IWL_DEBUG_TEMP(priv,
					"Thermal Throttling to new state: %u\n",
					tt->state);
			if (old_state != IWL_TI_CT_KILL &&
			    tt->state == IWL_TI_CT_KILL) {
				if (force) {
					IWL_DEBUG_TEMP(priv,
						"Enter IWL_TI_CT_KILL\n");
					set_bit(STATUS_CT_KILL, &priv->status);
					iwl_perform_ct_kill_task(priv, true);
				} else {
					iwl_prepare_ct_kill_task(priv);
					tt->state = old_state;
				}
			} else if (old_state == IWL_TI_CT_KILL &&
				  tt->state != IWL_TI_CT_KILL) {
				IWL_DEBUG_TEMP(priv, "Exit IWL_TI_CT_KILL\n");
				iwl_perform_ct_kill_task(priv, false);
			}
		}
		mutex_unlock(&priv->mutex);
	}
}

static void iwl_bg_ct_enter(struct work_struct *work)
{
	struct iwl_priv *priv = container_of(work, struct iwl_priv, ct_enter);
	struct iwl_tt_mgmt *tt = &priv->thermal_throttle;

	if (test_bit(STATUS_EXIT_PENDING, &priv->status))
		return;

	if (!iwl_is_ready(priv))
		return;

	if (tt->state != IWL_TI_CT_KILL) {
		IWL_ERR(priv, "Device reached critical temperature "
			      "- ucode going to sleep!\n");
		if (!priv->thermal_throttle.advanced_tt)
			iwl_legacy_tt_handler(priv,
					      IWL_MINIMAL_POWER_THRESHOLD,
					      true);
		else
			iwl_advance_tt_handler(priv,
					       CT_KILL_THRESHOLD + 1, true);
	}
}

static void iwl_bg_ct_exit(struct work_struct *work)
{
	struct iwl_priv *priv = container_of(work, struct iwl_priv, ct_exit);
	struct iwl_tt_mgmt *tt = &priv->thermal_throttle;

	if (test_bit(STATUS_EXIT_PENDING, &priv->status))
		return;

	if (!iwl_is_ready(priv))
		return;

	
	del_timer_sync(&priv->thermal_throttle.ct_kill_exit_tm);

	if (tt->state == IWL_TI_CT_KILL) {
		IWL_ERR(priv,
			"Device temperature below critical"
			"- ucode awake!\n");
		priv->temperature = 0;
		if (!priv->thermal_throttle.advanced_tt)
			iwl_legacy_tt_handler(priv,
				      IWL_REDUCED_PERFORMANCE_THRESHOLD_2,
				      true);
		else
			iwl_advance_tt_handler(priv, CT_KILL_EXIT_THRESHOLD,
					       true);
	}
}

void iwl_tt_enter_ct_kill(struct iwl_priv *priv)
{
	if (test_bit(STATUS_EXIT_PENDING, &priv->status))
		return;

	IWL_DEBUG_TEMP(priv, "Queueing critical temperature enter.\n");
	queue_work(priv->workqueue, &priv->ct_enter);
}

void iwl_tt_exit_ct_kill(struct iwl_priv *priv)
{
	if (test_bit(STATUS_EXIT_PENDING, &priv->status))
		return;

	IWL_DEBUG_TEMP(priv, "Queueing critical temperature exit.\n");
	queue_work(priv->workqueue, &priv->ct_exit);
}

static void iwl_bg_tt_work(struct work_struct *work)
{
	struct iwl_priv *priv = container_of(work, struct iwl_priv, tt_work);
	s32 temp = priv->temperature; 

	if (test_bit(STATUS_EXIT_PENDING, &priv->status))
		return;

	if (!priv->thermal_throttle.advanced_tt)
		iwl_legacy_tt_handler(priv, temp, false);
	else
		iwl_advance_tt_handler(priv, temp, false);
}

void iwl_tt_handler(struct iwl_priv *priv)
{
	if (test_bit(STATUS_EXIT_PENDING, &priv->status))
		return;

	IWL_DEBUG_TEMP(priv, "Queueing thermal throttling work.\n");
	queue_work(priv->workqueue, &priv->tt_work);
}

void iwl_tt_initialize(struct iwl_priv *priv)
{
	struct iwl_tt_mgmt *tt = &priv->thermal_throttle;
	int size = sizeof(struct iwl_tt_trans) * (IWL_TI_STATE_MAX - 1);
	struct iwl_tt_trans *transaction;

	IWL_DEBUG_TEMP(priv, "Initialize Thermal Throttling\n");

	memset(tt, 0, sizeof(struct iwl_tt_mgmt));

	tt->state = IWL_TI_0;
	init_timer(&priv->thermal_throttle.ct_kill_exit_tm);
	priv->thermal_throttle.ct_kill_exit_tm.data = (unsigned long)priv;
	priv->thermal_throttle.ct_kill_exit_tm.function =
		iwl_tt_check_exit_ct_kill;
	init_timer(&priv->thermal_throttle.ct_kill_waiting_tm);
	priv->thermal_throttle.ct_kill_waiting_tm.data =
		(unsigned long)priv;
	priv->thermal_throttle.ct_kill_waiting_tm.function =
		iwl_tt_ready_for_ct_kill;
	
	INIT_WORK(&priv->tt_work, iwl_bg_tt_work);
	INIT_WORK(&priv->ct_enter, iwl_bg_ct_enter);
	INIT_WORK(&priv->ct_exit, iwl_bg_ct_exit);

	if (cfg(priv)->base_params->adv_thermal_throttle) {
		IWL_DEBUG_TEMP(priv, "Advanced Thermal Throttling\n");
		tt->restriction = kcalloc(IWL_TI_STATE_MAX,
					  sizeof(struct iwl_tt_restriction),
					  GFP_KERNEL);
		tt->transaction = kcalloc(IWL_TI_STATE_MAX *
					  (IWL_TI_STATE_MAX - 1),
					  sizeof(struct iwl_tt_trans),
					  GFP_KERNEL);
		if (!tt->restriction || !tt->transaction) {
			IWL_ERR(priv, "Fallback to Legacy Throttling\n");
			priv->thermal_throttle.advanced_tt = false;
			kfree(tt->restriction);
			tt->restriction = NULL;
			kfree(tt->transaction);
			tt->transaction = NULL;
		} else {
			transaction = tt->transaction +
				(IWL_TI_0 * (IWL_TI_STATE_MAX - 1));
			memcpy(transaction, &tt_range_0[0], size);
			transaction = tt->transaction +
				(IWL_TI_1 * (IWL_TI_STATE_MAX - 1));
			memcpy(transaction, &tt_range_1[0], size);
			transaction = tt->transaction +
				(IWL_TI_2 * (IWL_TI_STATE_MAX - 1));
			memcpy(transaction, &tt_range_2[0], size);
			transaction = tt->transaction +
				(IWL_TI_CT_KILL * (IWL_TI_STATE_MAX - 1));
			memcpy(transaction, &tt_range_3[0], size);
			size = sizeof(struct iwl_tt_restriction) *
				IWL_TI_STATE_MAX;
			memcpy(tt->restriction,
				&restriction_range[0], size);
			priv->thermal_throttle.advanced_tt = true;
		}
	} else {
		IWL_DEBUG_TEMP(priv, "Legacy Thermal Throttling\n");
		priv->thermal_throttle.advanced_tt = false;
	}
}

void iwl_tt_exit(struct iwl_priv *priv)
{
	struct iwl_tt_mgmt *tt = &priv->thermal_throttle;

	
	del_timer_sync(&priv->thermal_throttle.ct_kill_exit_tm);
	
	del_timer_sync(&priv->thermal_throttle.ct_kill_waiting_tm);
	cancel_work_sync(&priv->tt_work);
	cancel_work_sync(&priv->ct_enter);
	cancel_work_sync(&priv->ct_exit);

	if (priv->thermal_throttle.advanced_tt) {
		
		kfree(tt->restriction);
		tt->restriction = NULL;
		kfree(tt->transaction);
		tt->transaction = NULL;
	}
}
