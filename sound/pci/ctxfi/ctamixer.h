/**
 * Copyright (C) 2008, Creative Technology Ltd. All Rights Reserved.
 *
 * This source file is released under GPL v2 license (no other versions).
 * See the COPYING file included in the main directory of this source
 * distribution for the license terms and conditions.
 *
 * @File	ctamixer.h
 *
 * @Brief
 * This file contains the definition of the Audio Mixer
 * resource management object.
 *
 * @Author	Liu Chun
 * @Date 	May 21 2008
 *
 */

#ifndef CTAMIXER_H
#define CTAMIXER_H

#include "ctresource.h"
#include <linux/spinlock.h>

struct sum {
	struct rsc rsc;		
	unsigned char idx[8];
};

struct sum_desc {
	unsigned int msr;
};

struct sum_mgr {
	struct rsc_mgr mgr;	
	spinlock_t mgr_lock;

	 
	int (*get_sum)(struct sum_mgr *mgr,
			const struct sum_desc *desc, struct sum **rsum);
	
	int (*put_sum)(struct sum_mgr *mgr, struct sum *sum);
};

int sum_mgr_create(void *hw, struct sum_mgr **rsum_mgr);
int sum_mgr_destroy(struct sum_mgr *sum_mgr);

struct amixer_rsc_ops;

struct amixer {
	struct rsc rsc;		
	unsigned char idx[8];
	struct rsc *input;	
	struct sum *sum;	
	struct amixer_rsc_ops *ops;	
};

struct amixer_rsc_ops {
	int (*set_input)(struct amixer *amixer, struct rsc *rsc);
	int (*set_scale)(struct amixer *amixer, unsigned int scale);
	int (*set_invalid_squash)(struct amixer *amixer, unsigned int iv);
	int (*set_sum)(struct amixer *amixer, struct sum *sum);
	int (*commit_write)(struct amixer *amixer);
	
	int (*commit_raw_write)(struct amixer *amixer);
	int (*setup)(struct amixer *amixer, struct rsc *input,
			unsigned int scale, struct sum *sum);
	int (*get_scale)(struct amixer *amixer);
};

struct amixer_desc {
	unsigned int msr;
};

struct amixer_mgr {
	struct rsc_mgr mgr;	
	spinlock_t mgr_lock;

	 
	int (*get_amixer)(struct amixer_mgr *mgr,
			  const struct amixer_desc *desc,
			  struct amixer **ramixer);
	
	int (*put_amixer)(struct amixer_mgr *mgr, struct amixer *amixer);
};

int amixer_mgr_create(void *hw, struct amixer_mgr **ramixer_mgr);
int amixer_mgr_destroy(struct amixer_mgr *amixer_mgr);

#endif 
