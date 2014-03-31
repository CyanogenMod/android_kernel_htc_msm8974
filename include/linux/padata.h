/*
 * padata.h - header for the padata parallelization interface
 *
 * Copyright (C) 2008, 2009 secunet Security Networks AG
 * Copyright (C) 2008, 2009 Steffen Klassert <steffen.klassert@secunet.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef PADATA_H
#define PADATA_H

#include <linux/workqueue.h>
#include <linux/spinlock.h>
#include <linux/list.h>
#include <linux/timer.h>
#include <linux/notifier.h>
#include <linux/kobject.h>

#define PADATA_CPU_SERIAL   0x01
#define PADATA_CPU_PARALLEL 0x02

struct padata_priv {
	struct list_head	list;
	struct parallel_data	*pd;
	int			cb_cpu;
	int			info;
	void                    (*parallel)(struct padata_priv *padata);
	void                    (*serial)(struct padata_priv *padata);
};

struct padata_list {
	struct list_head        list;
	spinlock_t              lock;
};

struct padata_serial_queue {
       struct padata_list    serial;
       struct work_struct    work;
       struct parallel_data *pd;
};

struct padata_parallel_queue {
       struct padata_list    parallel;
       struct padata_list    reorder;
       struct parallel_data *pd;
       struct work_struct    work;
       atomic_t              num_obj;
       int                   cpu_index;
};

struct padata_cpumask {
	cpumask_var_t	pcpu;
	cpumask_var_t	cbcpu;
};

struct parallel_data {
	struct padata_instance		*pinst;
	struct padata_parallel_queue	__percpu *pqueue;
	struct padata_serial_queue	__percpu *squeue;
	atomic_t			reorder_objects;
	atomic_t			refcnt;
	struct padata_cpumask		cpumask;
	spinlock_t                      lock ____cacheline_aligned;
	spinlock_t                      seq_lock;
	unsigned int			seq_nr;
	unsigned int			processed;
	struct timer_list		timer;
};

struct padata_instance {
	struct notifier_block		 cpu_notifier;
	struct workqueue_struct		*wq;
	struct parallel_data		*pd;
	struct padata_cpumask		cpumask;
	struct blocking_notifier_head	 cpumask_change_notifier;
	struct kobject                   kobj;
	struct mutex			 lock;
	u8				 flags;
#define	PADATA_INIT	1
#define	PADATA_RESET	2
#define	PADATA_INVALID	4
};

extern struct padata_instance *padata_alloc_possible(
					struct workqueue_struct *wq);
extern struct padata_instance *padata_alloc(struct workqueue_struct *wq,
					    const struct cpumask *pcpumask,
					    const struct cpumask *cbcpumask);
extern void padata_free(struct padata_instance *pinst);
extern int padata_do_parallel(struct padata_instance *pinst,
			      struct padata_priv *padata, int cb_cpu);
extern void padata_do_serial(struct padata_priv *padata);
extern int padata_set_cpumask(struct padata_instance *pinst, int cpumask_type,
			      cpumask_var_t cpumask);
extern int padata_set_cpumasks(struct padata_instance *pinst,
			       cpumask_var_t pcpumask,
			       cpumask_var_t cbcpumask);
extern int padata_add_cpu(struct padata_instance *pinst, int cpu, int mask);
extern int padata_remove_cpu(struct padata_instance *pinst, int cpu, int mask);
extern int padata_start(struct padata_instance *pinst);
extern void padata_stop(struct padata_instance *pinst);
extern int padata_register_cpumask_notifier(struct padata_instance *pinst,
					    struct notifier_block *nblock);
extern int padata_unregister_cpumask_notifier(struct padata_instance *pinst,
					      struct notifier_block *nblock);
#endif
