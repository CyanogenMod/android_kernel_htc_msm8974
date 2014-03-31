#ifndef __ASM_ARM_CPUIDLE_H
#define __ASM_ARM_CPUIDLE_H

#ifdef CONFIG_CPU_IDLE
extern int arm_cpuidle_simple_enter(struct cpuidle_device *dev,
		struct cpuidle_driver *drv, int index);
#else
static inline int arm_cpuidle_simple_enter(struct cpuidle_device *dev,
		struct cpuidle_driver *drv, int index) { return -ENODEV; }
#endif

#define ARM_CPUIDLE_WFI_STATE_PWR(p) {\
	.enter                  = arm_cpuidle_simple_enter,\
	.exit_latency           = 1,\
	.target_residency       = 1,\
	.power_usage		= p,\
	.flags                  = CPUIDLE_FLAG_TIME_VALID,\
	.name                   = "WFI",\
	.desc                   = "ARM WFI",\
}

#define ARM_CPUIDLE_WFI_STATE ARM_CPUIDLE_WFI_STATE_PWR(UINT_MAX)

#endif
