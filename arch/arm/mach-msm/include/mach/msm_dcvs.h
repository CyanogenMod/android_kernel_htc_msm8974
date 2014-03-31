/* Copyright (c) 2012, The Linux Foundation. All rights reserved.
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
#ifndef _ARCH_ARM_MACH_MSM_MSM_DCVS_H
#define _ARCH_ARM_MACH_MSM_MSM_DCVS_H

#include <mach/msm_dcvs_scm.h>

#define CORE_NAME_MAX (32)
#define CORES_MAX (10)

#define CPU_OFFSET	1  
#define GPU_OFFSET (CORES_MAX * 2/3)  

enum msm_core_idle_state {
	MSM_DCVS_IDLE_ENTER,
	MSM_DCVS_IDLE_EXIT,
};

enum msm_core_control_event {
	MSM_DCVS_ENABLE_IDLE_PULSE,
	MSM_DCVS_DISABLE_IDLE_PULSE,
	MSM_DCVS_ENABLE_HIGH_LATENCY_MODES,
	MSM_DCVS_DISABLE_HIGH_LATENCY_MODES,
};

struct msm_dcvs_sync_rule {
	unsigned long cpu_khz;
	unsigned long gpu_floor_khz;
};

struct msm_dcvs_platform_data {
	struct msm_dcvs_sync_rule *sync_rules;
	unsigned num_sync_rules;
	unsigned long gpu_max_nom_khz;
};

struct msm_gov_platform_data {
	struct msm_dcvs_core_info *info;
	int latency;
};

#ifdef CONFIG_MSM_DCVS
void msm_dcvs_register_cpu_freq(uint32_t freq, uint32_t voltage);
#else
static inline void msm_dcvs_register_cpu_freq(uint32_t freq, uint32_t voltage)
{}
#endif

int msm_dcvs_idle(int dcvs_core_id, enum msm_core_idle_state state,
		uint32_t iowaited);

struct msm_dcvs_core_info {
	int					num_cores;
	int					*sensors;
	int					thermal_poll_ms;
	struct msm_dcvs_freq_entry		*freq_tbl;
	struct msm_dcvs_core_param		core_param;
	struct msm_dcvs_algo_param		algo_param;
	struct msm_dcvs_energy_curve_coeffs	energy_coeffs;
	struct msm_dcvs_power_params		power_param;
};

extern int msm_dcvs_register_core(
	enum msm_dcvs_core_type type,
	int type_core_num,
	struct msm_dcvs_core_info *info,
	int (*set_frequency)(int type_core_num, unsigned int freq),
	unsigned int (*get_frequency)(int type_core_num),
	int (*idle_enable)(int type_core_num,
				enum msm_core_control_event event),
	int (*set_floor_frequency)(int type_core_num, unsigned int freq),
	int sensor);

extern int msm_dcvs_freq_sink_start(int dcvs_core_id);

extern int msm_dcvs_freq_sink_stop(int dcvs_core_id);

extern void msm_dcvs_update_limits(int dcvs_core_id);

extern void msm_dcvs_apply_gpu_floor(unsigned long cpu_freq);

extern int msm_dcvs_update_algo_params(void);
#endif
