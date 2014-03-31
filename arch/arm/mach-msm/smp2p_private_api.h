/* arch/arm/mach-msm/smp2p_private_api.h
 *
 * Copyright (c) 2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#ifndef _ARCH_ARM_MACH_MSM_SMP2P_PRIVATE_API_H_
#define _ARCH_ARM_MACH_MSM_SMP2P_PRIVATE_API_H_

#include <linux/notifier.h>

struct msm_smp2p_out;

#define SMP2P_MAX_ENTRY_NAME 16

#define SMP2P_BITS_PER_ENTRY 32

enum {
	SMP2P_APPS_PROC       = 0,
	SMP2P_MODEM_PROC      = 1,
	SMP2P_AUDIO_PROC      = 2,
	SMP2P_RESERVED_PROC_1 = 3,
	SMP2P_WIRELESS_PROC   = 4,
	SMP2P_RESERVED_PROC_2 = 5,
	SMP2P_POWER_PROC      = 6,
	

	SMP2P_REMOTE_MOCK_PROC,
	SMP2P_NUM_PROCS,
};

enum msm_smp2p_events {
	SMP2P_OPEN,         
	SMP2P_ENTRY_UPDATE, 
};

struct msm_smp2p_update_notif {
	uint32_t previous_value;
	uint32_t current_value;
};

int msm_smp2p_out_open(int remote_pid, const char *entry,
	struct notifier_block *open_notifier,
	struct msm_smp2p_out **handle);
int msm_smp2p_out_close(struct msm_smp2p_out **handle);
int msm_smp2p_out_read(struct msm_smp2p_out *handle, uint32_t *data);
int msm_smp2p_out_write(struct msm_smp2p_out *handle, uint32_t data);
int msm_smp2p_out_modify(struct msm_smp2p_out *handle, uint32_t set_mask,
	uint32_t clear_mask);
int msm_smp2p_in_read(int remote_pid, const char *entry, uint32_t *data);
int msm_smp2p_in_register(int remote_pid, const char *entry,
	struct notifier_block *in_notifier);
int msm_smp2p_in_unregister(int remote_pid, const char *entry,
	struct notifier_block *in_notifier);

#endif 
