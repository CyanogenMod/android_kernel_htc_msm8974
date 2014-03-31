/* cpumap.c: used for optimizing CPU assignment
 *
 * Copyright (C) 2009 Hong H. Pham <hong.pham@windriver.com>
 */

#include <linux/export.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/cpumask.h>
#include <linux/spinlock.h>
#include <asm/cpudata.h>
#include "cpumap.h"


enum {
	CPUINFO_LVL_ROOT = 0,
	CPUINFO_LVL_NODE,
	CPUINFO_LVL_CORE,
	CPUINFO_LVL_PROC,
	CPUINFO_LVL_MAX,
};

enum {
	ROVER_NO_OP              = 0,
	
	ROVER_INC_ON_VISIT       = 1 << 0,
	
	ROVER_INC_PARENT_ON_LOOP = 1 << 1,
};

struct cpuinfo_node {
	int id;
	int level;
	int num_cpus;    
	int parent_index;
	int child_start; 
	int child_end;   
	int rover;       
};

struct cpuinfo_level {
	int start_index; 
	int end_index;   
	int num_nodes;   
};

struct cpuinfo_tree {
	int total_nodes;

	
	struct cpuinfo_level level[CPUINFO_LVL_MAX];
	struct cpuinfo_node  nodes[0];
};


static struct cpuinfo_tree *cpuinfo_tree;

static u16 cpu_distribution_map[NR_CPUS];
static DEFINE_SPINLOCK(cpu_map_lock);


static const int niagara_iterate_method[] = {
	[CPUINFO_LVL_ROOT] = ROVER_NO_OP,

	[CPUINFO_LVL_NODE] = ROVER_INC_ON_VISIT|ROVER_INC_PARENT_ON_LOOP,

	[CPUINFO_LVL_CORE] = ROVER_INC_ON_VISIT,

	
	[CPUINFO_LVL_PROC] = ROVER_INC_ON_VISIT,
};

static const int generic_iterate_method[] = {
	[CPUINFO_LVL_ROOT] = ROVER_INC_ON_VISIT,
	[CPUINFO_LVL_NODE] = ROVER_NO_OP,
	[CPUINFO_LVL_CORE] = ROVER_INC_PARENT_ON_LOOP,
	[CPUINFO_LVL_PROC] = ROVER_INC_ON_VISIT|ROVER_INC_PARENT_ON_LOOP,
};


static int cpuinfo_id(int cpu, int level)
{
	int id;

	switch (level) {
	case CPUINFO_LVL_ROOT:
		id = 0;
		break;
	case CPUINFO_LVL_NODE:
		id = cpu_to_node(cpu);
		break;
	case CPUINFO_LVL_CORE:
		id = cpu_data(cpu).core_id;
		break;
	case CPUINFO_LVL_PROC:
		id = cpu_data(cpu).proc_id;
		break;
	default:
		id = -EINVAL;
	}
	return id;
}

static int enumerate_cpuinfo_nodes(struct cpuinfo_level *tree_level)
{
	int prev_id[CPUINFO_LVL_MAX];
	int i, n, num_nodes;

	for (i = CPUINFO_LVL_ROOT; i < CPUINFO_LVL_MAX; i++) {
		struct cpuinfo_level *lv = &tree_level[i];

		prev_id[i] = -1;
		lv->start_index = lv->end_index = lv->num_nodes = 0;
	}

	num_nodes = 1; 

	for (i = 0; i < num_possible_cpus(); i++) {
		if (!cpu_online(i))
			continue;

		n = cpuinfo_id(i, CPUINFO_LVL_NODE);
		if (n > prev_id[CPUINFO_LVL_NODE]) {
			tree_level[CPUINFO_LVL_NODE].num_nodes++;
			prev_id[CPUINFO_LVL_NODE] = n;
			num_nodes++;
		}
		n = cpuinfo_id(i, CPUINFO_LVL_CORE);
		if (n > prev_id[CPUINFO_LVL_CORE]) {
			tree_level[CPUINFO_LVL_CORE].num_nodes++;
			prev_id[CPUINFO_LVL_CORE] = n;
			num_nodes++;
		}
		n = cpuinfo_id(i, CPUINFO_LVL_PROC);
		if (n > prev_id[CPUINFO_LVL_PROC]) {
			tree_level[CPUINFO_LVL_PROC].num_nodes++;
			prev_id[CPUINFO_LVL_PROC] = n;
			num_nodes++;
		}
	}

	tree_level[CPUINFO_LVL_ROOT].num_nodes = 1;

	n = tree_level[CPUINFO_LVL_NODE].num_nodes;
	tree_level[CPUINFO_LVL_NODE].start_index = 1;
	tree_level[CPUINFO_LVL_NODE].end_index   = n;

	n++;
	tree_level[CPUINFO_LVL_CORE].start_index = n;
	n += tree_level[CPUINFO_LVL_CORE].num_nodes;
	tree_level[CPUINFO_LVL_CORE].end_index   = n - 1;

	tree_level[CPUINFO_LVL_PROC].start_index = n;
	n += tree_level[CPUINFO_LVL_PROC].num_nodes;
	tree_level[CPUINFO_LVL_PROC].end_index   = n - 1;

	return num_nodes;
}

static struct cpuinfo_tree *build_cpuinfo_tree(void)
{
	struct cpuinfo_tree *new_tree;
	struct cpuinfo_node *node;
	struct cpuinfo_level tmp_level[CPUINFO_LVL_MAX];
	int num_cpus[CPUINFO_LVL_MAX];
	int level_rover[CPUINFO_LVL_MAX];
	int prev_id[CPUINFO_LVL_MAX];
	int n, id, cpu, prev_cpu, last_cpu, level;

	n = enumerate_cpuinfo_nodes(tmp_level);

	new_tree = kzalloc(sizeof(struct cpuinfo_tree) +
	                   (sizeof(struct cpuinfo_node) * n), GFP_ATOMIC);
	if (!new_tree)
		return NULL;

	new_tree->total_nodes = n;
	memcpy(&new_tree->level, tmp_level, sizeof(tmp_level));

	prev_cpu = cpu = cpumask_first(cpu_online_mask);

	
	for (level = CPUINFO_LVL_PROC; level >= CPUINFO_LVL_ROOT; level--) {
		n = new_tree->level[level].start_index;

		level_rover[level] = n;
		node = &new_tree->nodes[n];

		id = cpuinfo_id(cpu, level);
		if (unlikely(id < 0)) {
			kfree(new_tree);
			return NULL;
		}
		node->id = id;
		node->level = level;
		node->num_cpus = 1;

		node->parent_index = (level > CPUINFO_LVL_ROOT)
		    ? new_tree->level[level - 1].start_index : -1;

		node->child_start = node->child_end = node->rover =
		    (level == CPUINFO_LVL_PROC)
		    ? cpu : new_tree->level[level + 1].start_index;

		prev_id[level] = node->id;
		num_cpus[level] = 1;
	}

	for (last_cpu = (num_possible_cpus() - 1); last_cpu >= 0; last_cpu--) {
		if (cpu_online(last_cpu))
			break;
	}

	while (++cpu <= last_cpu) {
		if (!cpu_online(cpu))
			continue;

		for (level = CPUINFO_LVL_PROC; level >= CPUINFO_LVL_ROOT;
		     level--) {
			id = cpuinfo_id(cpu, level);
			if (unlikely(id < 0)) {
				kfree(new_tree);
				return NULL;
			}

			if ((id != prev_id[level]) || (cpu == last_cpu)) {
				prev_id[level] = id;
				node = &new_tree->nodes[level_rover[level]];
				node->num_cpus = num_cpus[level];
				num_cpus[level] = 1;

				if (cpu == last_cpu)
					node->num_cpus++;

				
				if (level == CPUINFO_LVL_ROOT)
					node->parent_index = -1;
				else
					node->parent_index =
					    level_rover[level - 1];

				if (level == CPUINFO_LVL_PROC) {
					node->child_end =
					    (cpu == last_cpu) ? cpu : prev_cpu;
				} else {
					node->child_end =
					    level_rover[level + 1] - 1;
				}

				
				n = ++level_rover[level];
				if (n <= new_tree->level[level].end_index) {
					node = &new_tree->nodes[n];
					node->id = id;
					node->level = level;

					
					node->child_start = node->child_end =
					node->rover =
					    (level == CPUINFO_LVL_PROC)
					    ? cpu : level_rover[level + 1];
				}
			} else
				num_cpus[level]++;
		}
		prev_cpu = cpu;
	}

	return new_tree;
}

static void increment_rover(struct cpuinfo_tree *t, int node_index,
                            int root_index, const int *rover_inc_table)
{
	struct cpuinfo_node *node = &t->nodes[node_index];
	int top_level, level;

	top_level = t->nodes[root_index].level;
	for (level = node->level; level >= top_level; level--) {
		node->rover++;
		if (node->rover <= node->child_end)
			return;

		node->rover = node->child_start;
		
		if ((level == top_level) ||
		    !(rover_inc_table[level] & ROVER_INC_PARENT_ON_LOOP))
			return;

		node = &t->nodes[node->parent_index];
	}
}

static int iterate_cpu(struct cpuinfo_tree *t, unsigned int root_index)
{
	const int *rover_inc_table;
	int level, new_index, index = root_index;

	switch (sun4v_chip_type) {
	case SUN4V_CHIP_NIAGARA1:
	case SUN4V_CHIP_NIAGARA2:
	case SUN4V_CHIP_NIAGARA3:
	case SUN4V_CHIP_NIAGARA4:
	case SUN4V_CHIP_NIAGARA5:
		rover_inc_table = niagara_iterate_method;
		break;
	default:
		rover_inc_table = generic_iterate_method;
	}

	for (level = t->nodes[root_index].level; level < CPUINFO_LVL_MAX;
	     level++) {
		new_index = t->nodes[index].rover;
		if (rover_inc_table[level] & ROVER_INC_ON_VISIT)
			increment_rover(t, index, root_index, rover_inc_table);

		index = new_index;
	}
	return index;
}

static void _cpu_map_rebuild(void)
{
	int i;

	if (cpuinfo_tree) {
		kfree(cpuinfo_tree);
		cpuinfo_tree = NULL;
	}

	cpuinfo_tree = build_cpuinfo_tree();
	if (!cpuinfo_tree)
		return;

	for (i = 0; i < cpuinfo_tree->nodes[0].num_cpus; i++)
		cpu_distribution_map[i] = iterate_cpu(cpuinfo_tree, 0);
}

static int simple_map_to_cpu(unsigned int index)
{
	int i, end, cpu_rover;

	cpu_rover = 0;
	end = index % num_online_cpus();
	for (i = 0; i < num_possible_cpus(); i++) {
		if (cpu_online(cpu_rover)) {
			if (cpu_rover >= end)
				return cpu_rover;

			cpu_rover++;
		}
	}

	
	return cpumask_first(cpu_online_mask);
}

static int _map_to_cpu(unsigned int index)
{
	struct cpuinfo_node *root_node;

	if (unlikely(!cpuinfo_tree)) {
		_cpu_map_rebuild();
		if (!cpuinfo_tree)
			return simple_map_to_cpu(index);
	}

	root_node = &cpuinfo_tree->nodes[0];
#ifdef CONFIG_HOTPLUG_CPU
	if (unlikely(root_node->num_cpus != num_online_cpus())) {
		_cpu_map_rebuild();
		if (!cpuinfo_tree)
			return simple_map_to_cpu(index);
	}
#endif
	return cpu_distribution_map[index % root_node->num_cpus];
}

int map_to_cpu(unsigned int index)
{
	int mapped_cpu;
	unsigned long flag;

	spin_lock_irqsave(&cpu_map_lock, flag);
	mapped_cpu = _map_to_cpu(index);

#ifdef CONFIG_HOTPLUG_CPU
	while (unlikely(!cpu_online(mapped_cpu)))
		mapped_cpu = _map_to_cpu(index);
#endif
	spin_unlock_irqrestore(&cpu_map_lock, flag);
	return mapped_cpu;
}
EXPORT_SYMBOL(map_to_cpu);

void cpu_map_rebuild(void)
{
	unsigned long flag;

	spin_lock_irqsave(&cpu_map_lock, flag);
	_cpu_map_rebuild();
	spin_unlock_irqrestore(&cpu_map_lock, flag);
}
