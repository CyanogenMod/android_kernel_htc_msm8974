/*
 *  Copyright (C) 2010 ST-Ericsson
 *  Copyright (C) 2009 STMicroelectronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

struct clkops {
	void (*enable) (struct clk *);
	void (*disable) (struct clk *);
	unsigned long (*get_rate) (struct clk *);
	int (*set_parent)(struct clk *, struct clk *);
};

struct clk {
	const struct clkops	*ops;
	const char 		*name;
	unsigned int		enabled;
	unsigned long		(*get_rate)(struct clk *);
	void			*data;

	unsigned long		rate;
	struct list_head	list;

	

	unsigned int		prcmu_cg_off;
	unsigned int		prcmu_cg_bit;
	unsigned int		prcmu_cg_mgt;

	

	int			cluster;
	unsigned int		prcc_bus;
	unsigned int		prcc_kernel;

	struct clk		*parent_cluster;
	struct clk		*parent_periph;
#if defined(CONFIG_DEBUG_FS)
	struct dentry		*dent;		
	struct dentry		*dent_bus;	
#endif
};

#define DEFINE_PRCMU_CLK(_name, _cg_off, _cg_bit, _reg)		\
struct clk clk_##_name = {					\
		.name		= #_name,			\
		.ops    	= &clk_prcmu_ops, 		\
		.prcmu_cg_off	= _cg_off, 			\
		.prcmu_cg_bit	= _cg_bit,			\
		.prcmu_cg_mgt	= PRCM_##_reg##_MGT		\
	}

#define DEFINE_PRCMU_CLK_RATE(_name, _cg_off, _cg_bit, _reg, _rate)	\
struct clk clk_##_name = {						\
		.name		= #_name,				\
		.ops    	= &clk_prcmu_ops, 			\
		.prcmu_cg_off	= _cg_off, 				\
		.prcmu_cg_bit	= _cg_bit,				\
		.rate		= _rate,				\
		.prcmu_cg_mgt	= PRCM_##_reg##_MGT			\
	}

#define DEFINE_PRCC_CLK(_pclust, _name, _bus_en, _kernel_en, _kernclk)	\
struct clk clk_##_name = {						\
		.name		= #_name,				\
		.ops    	= &clk_prcc_ops, 			\
		.cluster 	= _pclust,				\
		.prcc_bus 	= _bus_en, 				\
		.prcc_kernel 	= _kernel_en, 				\
		.parent_cluster = &clk_per##_pclust##clk,		\
		.parent_periph 	= _kernclk				\
	}

#define DEFINE_PRCC_CLK_CUSTOM(_pclust, _name, _bus_en, _kernel_en, _kernclk, _callback, _data) \
struct clk clk_##_name = {						\
		.name		= #_name,				\
		.ops		= &clk_prcc_ops,			\
		.cluster	= _pclust,				\
		.prcc_bus	= _bus_en,				\
		.prcc_kernel	= _kernel_en,				\
		.parent_cluster = &clk_per##_pclust##clk,		\
		.parent_periph	= _kernclk,				\
		.get_rate	= _callback,				\
		.data		= (void *) _data			\
	}


#define CLK(_clk, _devname, _conname)			\
	{						\
		.clk	= &clk_##_clk,			\
		.dev_id	= _devname,			\
		.con_id = _conname,			\
	}

int __init clk_db8500_ed_fixup(void);
int __init clk_init(void);
