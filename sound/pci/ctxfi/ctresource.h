/**
 * Copyright (C) 2008, Creative Technology Ltd. All Rights Reserved.
 *
 * This source file is released under GPL v2 license (no other versions).
 * See the COPYING file included in the main directory of this source
 * distribution for the license terms and conditions.
 *
 * @File	ctresource.h
 *
 * @Brief
 * This file contains the definition of generic hardware resources for
 * resource management.
 *
 * @Author	Liu Chun
 * @Date 	May 13 2008
 *
 */

#ifndef CTRESOURCE_H
#define CTRESOURCE_H

#include <linux/types.h>

enum RSCTYP {
	SRC,
	SRCIMP,
	AMIXER,
	SUM,
	DAIO,
	NUM_RSCTYP	
};

struct rsc_ops;

struct rsc {
	u32 idx:12;	
	u32 type:4;	
	u32 conj:12;	
	u32 msr:4;	
	void *ctrl_blk;	
	void *hw;	
	struct rsc_ops *ops;	
};

struct rsc_ops {
	int (*master)(struct rsc *rsc);	
	int (*next_conj)(struct rsc *rsc); 
	int (*index)(const struct rsc *rsc); 
	
	int (*output_slot)(const struct rsc *rsc);
};

int rsc_init(struct rsc *rsc, u32 idx, enum RSCTYP type, u32 msr, void *hw);
int rsc_uninit(struct rsc *rsc);

struct rsc_mgr {
	enum RSCTYP type; 
	unsigned int amount; 
	unsigned int avail; 
	unsigned char *rscs; 
	void *ctrl_blk; 
	void *hw; 
};

int rsc_mgr_init(struct rsc_mgr *mgr, enum RSCTYP type,
		 unsigned int amount, void *hw);
int rsc_mgr_uninit(struct rsc_mgr *mgr);
int mgr_get_resource(struct rsc_mgr *mgr, unsigned int n, unsigned int *ridx);
int mgr_put_resource(struct rsc_mgr *mgr, unsigned int n, unsigned int idx);

#endif 
