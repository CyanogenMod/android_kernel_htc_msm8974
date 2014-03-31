/*
 * omap_hwmod macros, structures
 *
 * Copyright (C) 2009-2011 Nokia Corporation
 * Copyright (C) 2011 Texas Instruments, Inc.
 * Paul Walmsley
 *
 * Created in collaboration with (alphabetical order): Benoît Cousson,
 * Kevin Hilman, Tony Lindgren, Rajendra Nayak, Vikram Pandita, Sakari
 * Poussa, Anand Sawant, Santosh Shilimkar, Richard Woodruff
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * These headers and macros are used to define OMAP on-chip module
 * data and their integration with other OMAP modules and Linux.
 * Copious documentation and references can also be found in the
 * omap_hwmod code, in arch/arm/mach-omap2/omap_hwmod.c (as of this
 * writing).
 *
 * To do:
 * - add interconnect error log structures
 * - add pinmuxing
 * - init_conn_id_bit (CONNID_BIT_VECTOR)
 * - implement default hwmod SMS/SDRC flags?
 * - move Linux-specific data ("non-ROM data") out
 *
 */
#ifndef __ARCH_ARM_PLAT_OMAP_INCLUDE_MACH_OMAP_HWMOD_H
#define __ARCH_ARM_PLAT_OMAP_INCLUDE_MACH_OMAP_HWMOD_H

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/ioport.h>
#include <linux/spinlock.h>
#include <plat/cpu.h>

struct omap_device;

extern struct omap_hwmod_sysc_fields omap_hwmod_sysc_type1;
extern struct omap_hwmod_sysc_fields omap_hwmod_sysc_type2;

#define SYSC_TYPE1_MIDLEMODE_SHIFT	12
#define SYSC_TYPE1_MIDLEMODE_MASK	(0x3 << SYSC_TYPE1_MIDLEMODE_SHIFT)
#define SYSC_TYPE1_CLOCKACTIVITY_SHIFT	8
#define SYSC_TYPE1_CLOCKACTIVITY_MASK	(0x3 << SYSC_TYPE1_CLOCKACTIVITY_SHIFT)
#define SYSC_TYPE1_SIDLEMODE_SHIFT	3
#define SYSC_TYPE1_SIDLEMODE_MASK	(0x3 << SYSC_TYPE1_SIDLEMODE_SHIFT)
#define SYSC_TYPE1_ENAWAKEUP_SHIFT	2
#define SYSC_TYPE1_ENAWAKEUP_MASK	(1 << SYSC_TYPE1_ENAWAKEUP_SHIFT)
#define SYSC_TYPE1_SOFTRESET_SHIFT	1
#define SYSC_TYPE1_SOFTRESET_MASK	(1 << SYSC_TYPE1_SOFTRESET_SHIFT)
#define SYSC_TYPE1_AUTOIDLE_SHIFT	0
#define SYSC_TYPE1_AUTOIDLE_MASK	(1 << SYSC_TYPE1_AUTOIDLE_SHIFT)

#define SYSC_TYPE2_SOFTRESET_SHIFT	0
#define SYSC_TYPE2_SOFTRESET_MASK	(1 << SYSC_TYPE2_SOFTRESET_SHIFT)
#define SYSC_TYPE2_SIDLEMODE_SHIFT	2
#define SYSC_TYPE2_SIDLEMODE_MASK	(0x3 << SYSC_TYPE2_SIDLEMODE_SHIFT)
#define SYSC_TYPE2_MIDLEMODE_SHIFT	4
#define SYSC_TYPE2_MIDLEMODE_MASK	(0x3 << SYSC_TYPE2_MIDLEMODE_SHIFT)

#define SYSS_RESETDONE_SHIFT		0
#define SYSS_RESETDONE_MASK		(1 << SYSS_RESETDONE_SHIFT)

#define HWMOD_IDLEMODE_FORCE		(1 << 0)
#define HWMOD_IDLEMODE_NO		(1 << 1)
#define HWMOD_IDLEMODE_SMART		(1 << 2)
#define HWMOD_IDLEMODE_SMART_WKUP	(1 << 3)

#define MODULEMODE_HWCTRL		1
#define MODULEMODE_SWCTRL		2


struct omap_hwmod_mux_info {
	int				nr_pads;
	struct omap_device_pad		*pads;
	int				nr_pads_dynamic;
	struct omap_device_pad		**pads_dynamic;
	int				*irqs;
	bool				enabled;
};

struct omap_hwmod_irq_info {
	const char	*name;
	s16		irq;
};

struct omap_hwmod_dma_info {
	const char	*name;
	s16		dma_req;
};

struct omap_hwmod_rst_info {
	const char	*name;
	u8		rst_shift;
	u8		st_shift;
};

struct omap_hwmod_opt_clk {
	const char	*role;
	const char	*clk;
	struct clk	*_clk;
};


#define OMAP_FIREWALL_L3		(1 << 0)
#define OMAP_FIREWALL_L4		(1 << 1)

struct omap_hwmod_omap2_firewall {
	u8 l3_perm_bit;
	u8 l4_fw_region;
	u8 l4_prot_group;
	u8 flags;
};


#define ADDR_MAP_ON_INIT	(1 << 0)	
#define ADDR_TYPE_RT		(1 << 1)

struct omap_hwmod_addr_space {
	const char *name;
	u32 pa_start;
	u32 pa_end;
	u8 flags;
};


#define OCP_USER_MPU			(1 << 0)
#define OCP_USER_SDMA			(1 << 1)

#define OCPIF_SWSUP_IDLE		(1 << 0)
#define OCPIF_CAN_BURST			(1 << 1)

struct omap_hwmod_ocp_if {
	struct omap_hwmod		*master;
	struct omap_hwmod		*slave;
	struct omap_hwmod_addr_space	*addr;
	const char			*clk;
	struct clk			*_clk;
	union {
		struct omap_hwmod_omap2_firewall omap2;
	}				fw;
	u8				width;
	u8				user;
	u8				flags;
};



#define MASTER_STANDBY_SHIFT	4
#define SLAVE_IDLE_SHIFT	0
#define SIDLE_FORCE		(HWMOD_IDLEMODE_FORCE << SLAVE_IDLE_SHIFT)
#define SIDLE_NO		(HWMOD_IDLEMODE_NO << SLAVE_IDLE_SHIFT)
#define SIDLE_SMART		(HWMOD_IDLEMODE_SMART << SLAVE_IDLE_SHIFT)
#define SIDLE_SMART_WKUP	(HWMOD_IDLEMODE_SMART_WKUP << SLAVE_IDLE_SHIFT)
#define MSTANDBY_FORCE		(HWMOD_IDLEMODE_FORCE << MASTER_STANDBY_SHIFT)
#define MSTANDBY_NO		(HWMOD_IDLEMODE_NO << MASTER_STANDBY_SHIFT)
#define MSTANDBY_SMART		(HWMOD_IDLEMODE_SMART << MASTER_STANDBY_SHIFT)
#define MSTANDBY_SMART_WKUP	(HWMOD_IDLEMODE_SMART_WKUP << MASTER_STANDBY_SHIFT)

#define SYSC_HAS_AUTOIDLE	(1 << 0)
#define SYSC_HAS_SOFTRESET	(1 << 1)
#define SYSC_HAS_ENAWAKEUP	(1 << 2)
#define SYSC_HAS_EMUFREE	(1 << 3)
#define SYSC_HAS_CLOCKACTIVITY	(1 << 4)
#define SYSC_HAS_SIDLEMODE	(1 << 5)
#define SYSC_HAS_MIDLEMODE	(1 << 6)
#define SYSS_HAS_RESET_STATUS	(1 << 7)
#define SYSC_NO_CACHE		(1 << 8)  
#define SYSC_HAS_RESET_STATUS	(1 << 9)

#define CLOCKACT_TEST_BOTH	0x0
#define CLOCKACT_TEST_MAIN	0x1
#define CLOCKACT_TEST_ICLK	0x2
#define CLOCKACT_TEST_NONE	0x3

struct omap_hwmod_sysc_fields {
	u8 midle_shift;
	u8 clkact_shift;
	u8 sidle_shift;
	u8 enwkup_shift;
	u8 srst_shift;
	u8 autoidle_shift;
};

struct omap_hwmod_class_sysconfig {
	u16 rev_offs;
	u16 sysc_offs;
	u16 syss_offs;
	u16 sysc_flags;
	struct omap_hwmod_sysc_fields *sysc_fields;
	u8 srst_udelay;
	u8 idlemodes;
	u8 clockact;
};

struct omap_hwmod_omap2_prcm {
	s16 module_offs;
	u8 prcm_reg_id;
	u8 module_bit;
	u8 idlest_reg_id;
	u8 idlest_idle_bit;
	u8 idlest_stdby_bit;
};


struct omap_hwmod_omap4_prcm {
	u16		clkctrl_offs;
	u16		rstctrl_offs;
	u16		context_offs;
	u8		submodule_wkdep_bit;
	u8		modulemode;
};


#define HWMOD_SWSUP_SIDLE			(1 << 0)
#define HWMOD_SWSUP_MSTANDBY			(1 << 1)
#define HWMOD_INIT_NO_RESET			(1 << 2)
#define HWMOD_INIT_NO_IDLE			(1 << 3)
#define HWMOD_NO_OCP_AUTOIDLE			(1 << 4)
#define HWMOD_SET_DEFAULT_CLOCKACT		(1 << 5)
#define HWMOD_NO_IDLEST				(1 << 6)
#define HWMOD_CONTROL_OPT_CLKS_IN_RESET		(1 << 7)
#define HWMOD_16BIT_REG				(1 << 8)

#define _HWMOD_NO_MPU_PORT			(1 << 0)
#define _HWMOD_WAKEUP_ENABLED			(1 << 1)
#define _HWMOD_SYSCONFIG_LOADED			(1 << 2)
#define _HWMOD_SKIP_ENABLE			(1 << 3)

#define _HWMOD_STATE_UNKNOWN			0
#define _HWMOD_STATE_REGISTERED			1
#define _HWMOD_STATE_CLKS_INITED		2
#define _HWMOD_STATE_INITIALIZED		3
#define _HWMOD_STATE_ENABLED			4
#define _HWMOD_STATE_IDLE			5
#define _HWMOD_STATE_DISABLED			6

struct omap_hwmod_class {
	const char				*name;
	struct omap_hwmod_class_sysconfig	*sysc;
	u32					rev;
	int					(*pre_shutdown)(struct omap_hwmod *oh);
	int					(*reset)(struct omap_hwmod *oh);
};

struct omap_hwmod {
	const char			*name;
	struct omap_hwmod_class		*class;
	struct omap_device		*od;
	struct omap_hwmod_mux_info	*mux;
	struct omap_hwmod_irq_info	*mpu_irqs;
	struct omap_hwmod_dma_info	*sdma_reqs;
	struct omap_hwmod_rst_info	*rst_lines;
	union {
		struct omap_hwmod_omap2_prcm omap2;
		struct omap_hwmod_omap4_prcm omap4;
	}				prcm;
	const char			*main_clk;
	struct clk			*_clk;
	struct omap_hwmod_opt_clk	*opt_clks;
	char				*clkdm_name;
	struct clockdomain		*clkdm;
	struct omap_hwmod_ocp_if	**masters; 
	struct omap_hwmod_ocp_if	**slaves;  
	void				*dev_attr;
	u32				_sysc_cache;
	void __iomem			*_mpu_rt_va;
	spinlock_t			_lock;
	struct list_head		node;
	u16				flags;
	u8				_mpu_port_index;
	u8				response_lat;
	u8				rst_lines_cnt;
	u8				opt_clks_cnt;
	u8				masters_cnt;
	u8				slaves_cnt;
	u8				hwmods_cnt;
	u8				_int_flags;
	u8				_state;
	u8				_postsetup_state;
};

int omap_hwmod_register(struct omap_hwmod **ohs);
struct omap_hwmod *omap_hwmod_lookup(const char *name);
int omap_hwmod_for_each(int (*fn)(struct omap_hwmod *oh, void *data),
			void *data);

int __init omap_hwmod_setup_one(const char *name);

int omap_hwmod_enable(struct omap_hwmod *oh);
int _omap_hwmod_enable(struct omap_hwmod *oh);
int omap_hwmod_idle(struct omap_hwmod *oh);
int _omap_hwmod_idle(struct omap_hwmod *oh);
int omap_hwmod_shutdown(struct omap_hwmod *oh);

int omap_hwmod_assert_hardreset(struct omap_hwmod *oh, const char *name);
int omap_hwmod_deassert_hardreset(struct omap_hwmod *oh, const char *name);
int omap_hwmod_read_hardreset(struct omap_hwmod *oh, const char *name);

int omap_hwmod_enable_clocks(struct omap_hwmod *oh);
int omap_hwmod_disable_clocks(struct omap_hwmod *oh);

int omap_hwmod_set_slave_idlemode(struct omap_hwmod *oh, u8 idlemode);
int omap_hwmod_set_ocp_autoidle(struct omap_hwmod *oh, u8 autoidle);

int omap_hwmod_reset(struct omap_hwmod *oh);
void omap_hwmod_ocp_barrier(struct omap_hwmod *oh);

void omap_hwmod_write(u32 v, struct omap_hwmod *oh, u16 reg_offs);
u32 omap_hwmod_read(struct omap_hwmod *oh, u16 reg_offs);
int omap_hwmod_softreset(struct omap_hwmod *oh);

int omap_hwmod_count_resources(struct omap_hwmod *oh);
int omap_hwmod_fill_resources(struct omap_hwmod *oh, struct resource *res);

struct powerdomain *omap_hwmod_get_pwrdm(struct omap_hwmod *oh);
void __iomem *omap_hwmod_get_mpu_rt_va(struct omap_hwmod *oh);

int omap_hwmod_add_initiator_dep(struct omap_hwmod *oh,
				 struct omap_hwmod *init_oh);
int omap_hwmod_del_initiator_dep(struct omap_hwmod *oh,
				 struct omap_hwmod *init_oh);

int omap_hwmod_set_clockact_both(struct omap_hwmod *oh);
int omap_hwmod_set_clockact_main(struct omap_hwmod *oh);
int omap_hwmod_set_clockact_iclk(struct omap_hwmod *oh);
int omap_hwmod_set_clockact_none(struct omap_hwmod *oh);

int omap_hwmod_enable_wakeup(struct omap_hwmod *oh);
int omap_hwmod_disable_wakeup(struct omap_hwmod *oh);

int omap_hwmod_for_each_by_class(const char *classname,
				 int (*fn)(struct omap_hwmod *oh,
					   void *user),
				 void *user);

int omap_hwmod_set_postsetup_state(struct omap_hwmod *oh, u8 state);
int omap_hwmod_get_context_loss_count(struct omap_hwmod *oh);

int omap_hwmod_no_setup_reset(struct omap_hwmod *oh);

int omap_hwmod_pad_route_irq(struct omap_hwmod *oh, int pad_idx, int irq_idx);

extern int omap2420_hwmod_init(void);
extern int omap2430_hwmod_init(void);
extern int omap3xxx_hwmod_init(void);
extern int omap44xx_hwmod_init(void);

#endif
