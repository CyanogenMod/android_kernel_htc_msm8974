
#include <linux/module.h>
#include <linux/init.h>
#include <linux/mm.h>
#include <linux/memory.h>
#include <linux/vmstat.h>
#include <linux/node.h>
#include <linux/hugetlb.h>
#include <linux/compaction.h>
#include <linux/cpumask.h>
#include <linux/topology.h>
#include <linux/nodemask.h>
#include <linux/cpu.h>
#include <linux/device.h>
#include <linux/swap.h>
#include <linux/slab.h>

static struct bus_type node_subsys = {
	.name = "node",
	.dev_name = "node",
};


static ssize_t node_read_cpumap(struct device *dev, int type, char *buf)
{
	struct node *node_dev = to_node(dev);
	const struct cpumask *mask = cpumask_of_node(node_dev->dev.id);
	int len;

	
	BUILD_BUG_ON((NR_CPUS/32 * 9) > (PAGE_SIZE-1));

	len = type?
		cpulist_scnprintf(buf, PAGE_SIZE-2, mask) :
		cpumask_scnprintf(buf, PAGE_SIZE-2, mask);
 	buf[len++] = '\n';
 	buf[len] = '\0';
	return len;
}

static inline ssize_t node_read_cpumask(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return node_read_cpumap(dev, 0, buf);
}
static inline ssize_t node_read_cpulist(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return node_read_cpumap(dev, 1, buf);
}

static DEVICE_ATTR(cpumap,  S_IRUGO, node_read_cpumask, NULL);
static DEVICE_ATTR(cpulist, S_IRUGO, node_read_cpulist, NULL);

#define K(x) ((x) << (PAGE_SHIFT - 10))
static ssize_t node_read_meminfo(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	int n;
	int nid = dev->id;
	struct sysinfo i;

	si_meminfo_node(&i, nid);
	n = sprintf(buf,
		       "Node %d MemTotal:       %8lu kB\n"
		       "Node %d MemFree:        %8lu kB\n"
		       "Node %d MemUsed:        %8lu kB\n"
		       "Node %d Active:         %8lu kB\n"
		       "Node %d Inactive:       %8lu kB\n"
		       "Node %d Active(anon):   %8lu kB\n"
		       "Node %d Inactive(anon): %8lu kB\n"
		       "Node %d Active(file):   %8lu kB\n"
		       "Node %d Inactive(file): %8lu kB\n"
		       "Node %d Unevictable:    %8lu kB\n"
		       "Node %d Mlocked:        %8lu kB\n",
		       nid, K(i.totalram),
		       nid, K(i.freeram),
		       nid, K(i.totalram - i.freeram),
		       nid, K(node_page_state(nid, NR_ACTIVE_ANON) +
				node_page_state(nid, NR_ACTIVE_FILE)),
		       nid, K(node_page_state(nid, NR_INACTIVE_ANON) +
				node_page_state(nid, NR_INACTIVE_FILE)),
		       nid, K(node_page_state(nid, NR_ACTIVE_ANON)),
		       nid, K(node_page_state(nid, NR_INACTIVE_ANON)),
		       nid, K(node_page_state(nid, NR_ACTIVE_FILE)),
		       nid, K(node_page_state(nid, NR_INACTIVE_FILE)),
		       nid, K(node_page_state(nid, NR_UNEVICTABLE)),
		       nid, K(node_page_state(nid, NR_MLOCK)));

#ifdef CONFIG_HIGHMEM
	n += sprintf(buf + n,
		       "Node %d HighTotal:      %8lu kB\n"
		       "Node %d HighFree:       %8lu kB\n"
		       "Node %d LowTotal:       %8lu kB\n"
		       "Node %d LowFree:        %8lu kB\n",
		       nid, K(i.totalhigh),
		       nid, K(i.freehigh),
		       nid, K(i.totalram - i.totalhigh),
		       nid, K(i.freeram - i.freehigh));
#endif
	n += sprintf(buf + n,
		       "Node %d Dirty:          %8lu kB\n"
		       "Node %d Writeback:      %8lu kB\n"
		       "Node %d FilePages:      %8lu kB\n"
		       "Node %d Mapped:         %8lu kB\n"
		       "Node %d AnonPages:      %8lu kB\n"
		       "Node %d Shmem:          %8lu kB\n"
		       "Node %d KernelStack:    %8lu kB\n"
		       "Node %d PageTables:     %8lu kB\n"
		       "Node %d NFS_Unstable:   %8lu kB\n"
		       "Node %d Bounce:         %8lu kB\n"
		       "Node %d WritebackTmp:   %8lu kB\n"
		       "Node %d Slab:           %8lu kB\n"
		       "Node %d SReclaimable:   %8lu kB\n"
		       "Node %d SUnreclaim:     %8lu kB\n"
#ifdef CONFIG_TRANSPARENT_HUGEPAGE
		       "Node %d AnonHugePages:  %8lu kB\n"
#endif
			,
		       nid, K(node_page_state(nid, NR_FILE_DIRTY)),
		       nid, K(node_page_state(nid, NR_WRITEBACK)),
		       nid, K(node_page_state(nid, NR_FILE_PAGES)),
		       nid, K(node_page_state(nid, NR_FILE_MAPPED)),
#ifdef CONFIG_TRANSPARENT_HUGEPAGE
		       nid, K(node_page_state(nid, NR_ANON_PAGES)
			+ node_page_state(nid, NR_ANON_TRANSPARENT_HUGEPAGES) *
			HPAGE_PMD_NR),
#else
		       nid, K(node_page_state(nid, NR_ANON_PAGES)),
#endif
		       nid, K(node_page_state(nid, NR_SHMEM)),
		       nid, node_page_state(nid, NR_KERNEL_STACK) *
				THREAD_SIZE / 1024,
		       nid, K(node_page_state(nid, NR_PAGETABLE)),
		       nid, K(node_page_state(nid, NR_UNSTABLE_NFS)),
		       nid, K(node_page_state(nid, NR_BOUNCE)),
		       nid, K(node_page_state(nid, NR_WRITEBACK_TEMP)),
		       nid, K(node_page_state(nid, NR_SLAB_RECLAIMABLE) +
				node_page_state(nid, NR_SLAB_UNRECLAIMABLE)),
		       nid, K(node_page_state(nid, NR_SLAB_RECLAIMABLE)),
#ifdef CONFIG_TRANSPARENT_HUGEPAGE
		       nid, K(node_page_state(nid, NR_SLAB_UNRECLAIMABLE))
			, nid,
			K(node_page_state(nid, NR_ANON_TRANSPARENT_HUGEPAGES) *
			HPAGE_PMD_NR));
#else
		       nid, K(node_page_state(nid, NR_SLAB_UNRECLAIMABLE)));
#endif
	n += hugetlb_report_node_meminfo(nid, buf + n);
	return n;
}

#undef K
static DEVICE_ATTR(meminfo, S_IRUGO, node_read_meminfo, NULL);

static ssize_t node_read_numastat(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return sprintf(buf,
		       "numa_hit %lu\n"
		       "numa_miss %lu\n"
		       "numa_foreign %lu\n"
		       "interleave_hit %lu\n"
		       "local_node %lu\n"
		       "other_node %lu\n",
		       node_page_state(dev->id, NUMA_HIT),
		       node_page_state(dev->id, NUMA_MISS),
		       node_page_state(dev->id, NUMA_FOREIGN),
		       node_page_state(dev->id, NUMA_INTERLEAVE_HIT),
		       node_page_state(dev->id, NUMA_LOCAL),
		       node_page_state(dev->id, NUMA_OTHER));
}
static DEVICE_ATTR(numastat, S_IRUGO, node_read_numastat, NULL);

static ssize_t node_read_vmstat(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int nid = dev->id;
	int i;
	int n = 0;

	for (i = 0; i < NR_VM_ZONE_STAT_ITEMS; i++)
		n += sprintf(buf+n, "%s %lu\n", vmstat_text[i],
			     node_page_state(nid, i));

	return n;
}
static DEVICE_ATTR(vmstat, S_IRUGO, node_read_vmstat, NULL);

static ssize_t node_read_distance(struct device *dev,
			struct device_attribute *attr, char * buf)
{
	int nid = dev->id;
	int len = 0;
	int i;

	BUILD_BUG_ON(MAX_NUMNODES * 4 > PAGE_SIZE);

	for_each_online_node(i)
		len += sprintf(buf + len, "%s%d", i ? " " : "", node_distance(nid, i));

	len += sprintf(buf + len, "\n");
	return len;
}
static DEVICE_ATTR(distance, S_IRUGO, node_read_distance, NULL);

#ifdef CONFIG_HUGETLBFS
static node_registration_func_t __hugetlb_register_node;
static node_registration_func_t __hugetlb_unregister_node;

static inline bool hugetlb_register_node(struct node *node)
{
	if (__hugetlb_register_node &&
			node_state(node->dev.id, N_HIGH_MEMORY)) {
		__hugetlb_register_node(node);
		return true;
	}
	return false;
}

static inline void hugetlb_unregister_node(struct node *node)
{
	if (__hugetlb_unregister_node)
		__hugetlb_unregister_node(node);
}

void register_hugetlbfs_with_node(node_registration_func_t doregister,
				  node_registration_func_t unregister)
{
	__hugetlb_register_node   = doregister;
	__hugetlb_unregister_node = unregister;
}
#else
static inline void hugetlb_register_node(struct node *node) {}

static inline void hugetlb_unregister_node(struct node *node) {}
#endif


int register_node(struct node *node, int num, struct node *parent)
{
	int error;

	node->dev.id = num;
	node->dev.bus = &node_subsys;
	error = device_register(&node->dev);

	if (!error){
		device_create_file(&node->dev, &dev_attr_cpumap);
		device_create_file(&node->dev, &dev_attr_cpulist);
		device_create_file(&node->dev, &dev_attr_meminfo);
		device_create_file(&node->dev, &dev_attr_numastat);
		device_create_file(&node->dev, &dev_attr_distance);
		device_create_file(&node->dev, &dev_attr_vmstat);

		scan_unevictable_register_node(node);

		hugetlb_register_node(node);

		compaction_register_node(node);
	}
	return error;
}

void unregister_node(struct node *node)
{
	device_remove_file(&node->dev, &dev_attr_cpumap);
	device_remove_file(&node->dev, &dev_attr_cpulist);
	device_remove_file(&node->dev, &dev_attr_meminfo);
	device_remove_file(&node->dev, &dev_attr_numastat);
	device_remove_file(&node->dev, &dev_attr_distance);
	device_remove_file(&node->dev, &dev_attr_vmstat);

	scan_unevictable_unregister_node(node);
	hugetlb_unregister_node(node);		

	device_unregister(&node->dev);
}

struct node node_devices[MAX_NUMNODES];

int register_cpu_under_node(unsigned int cpu, unsigned int nid)
{
	int ret;
	struct device *obj;

	if (!node_online(nid))
		return 0;

	obj = get_cpu_device(cpu);
	if (!obj)
		return 0;

	ret = sysfs_create_link(&node_devices[nid].dev.kobj,
				&obj->kobj,
				kobject_name(&obj->kobj));
	if (ret)
		return ret;

	return sysfs_create_link(&obj->kobj,
				 &node_devices[nid].dev.kobj,
				 kobject_name(&node_devices[nid].dev.kobj));
}

int unregister_cpu_under_node(unsigned int cpu, unsigned int nid)
{
	struct device *obj;

	if (!node_online(nid))
		return 0;

	obj = get_cpu_device(cpu);
	if (!obj)
		return 0;

	sysfs_remove_link(&node_devices[nid].dev.kobj,
			  kobject_name(&obj->kobj));
	sysfs_remove_link(&obj->kobj,
			  kobject_name(&node_devices[nid].dev.kobj));

	return 0;
}

#ifdef CONFIG_MEMORY_HOTPLUG_SPARSE
#define page_initialized(page)  (page->lru.next)

static int get_nid_for_pfn(unsigned long pfn)
{
	struct page *page;

	if (!pfn_valid_within(pfn))
		return -1;
	page = pfn_to_page(pfn);
	if (!page_initialized(page))
		return -1;
	return pfn_to_nid(pfn);
}

int register_mem_sect_under_node(struct memory_block *mem_blk, int nid)
{
	int ret;
	unsigned long pfn, sect_start_pfn, sect_end_pfn;

	if (!mem_blk)
		return -EFAULT;
	if (!node_online(nid))
		return 0;

	sect_start_pfn = section_nr_to_pfn(mem_blk->start_section_nr);
	sect_end_pfn = section_nr_to_pfn(mem_blk->end_section_nr);
	sect_end_pfn += PAGES_PER_SECTION - 1;
	for (pfn = sect_start_pfn; pfn <= sect_end_pfn; pfn++) {
		int page_nid;

		page_nid = get_nid_for_pfn(pfn);
		if (page_nid < 0)
			continue;
		if (page_nid != nid)
			continue;
		ret = sysfs_create_link_nowarn(&node_devices[nid].dev.kobj,
					&mem_blk->dev.kobj,
					kobject_name(&mem_blk->dev.kobj));
		if (ret)
			return ret;

		return sysfs_create_link_nowarn(&mem_blk->dev.kobj,
				&node_devices[nid].dev.kobj,
				kobject_name(&node_devices[nid].dev.kobj));
	}
	
	return 0;
}

int unregister_mem_sect_under_nodes(struct memory_block *mem_blk,
				    unsigned long phys_index)
{
	NODEMASK_ALLOC(nodemask_t, unlinked_nodes, GFP_KERNEL);
	unsigned long pfn, sect_start_pfn, sect_end_pfn;

	if (!mem_blk) {
		NODEMASK_FREE(unlinked_nodes);
		return -EFAULT;
	}
	if (!unlinked_nodes)
		return -ENOMEM;
	nodes_clear(*unlinked_nodes);

	sect_start_pfn = section_nr_to_pfn(phys_index);
	sect_end_pfn = sect_start_pfn + PAGES_PER_SECTION - 1;
	for (pfn = sect_start_pfn; pfn <= sect_end_pfn; pfn++) {
		int nid;

		nid = get_nid_for_pfn(pfn);
		if (nid < 0)
			continue;
		if (!node_online(nid))
			continue;
		if (node_test_and_set(nid, *unlinked_nodes))
			continue;
		sysfs_remove_link(&node_devices[nid].dev.kobj,
			 kobject_name(&mem_blk->dev.kobj));
		sysfs_remove_link(&mem_blk->dev.kobj,
			 kobject_name(&node_devices[nid].dev.kobj));
	}
	NODEMASK_FREE(unlinked_nodes);
	return 0;
}

static int link_mem_sections(int nid)
{
	unsigned long start_pfn = NODE_DATA(nid)->node_start_pfn;
	unsigned long end_pfn = start_pfn + NODE_DATA(nid)->node_spanned_pages;
	unsigned long pfn;
	struct memory_block *mem_blk = NULL;
	int err = 0;

	for (pfn = start_pfn; pfn < end_pfn; pfn += PAGES_PER_SECTION) {
		unsigned long section_nr = pfn_to_section_nr(pfn);
		struct mem_section *mem_sect;
		int ret;

		if (!present_section_nr(section_nr))
			continue;
		mem_sect = __nr_to_section(section_nr);

		
		if (mem_blk)
			if ((section_nr >= mem_blk->start_section_nr) &&
			    (section_nr <= mem_blk->end_section_nr))
				continue;

		mem_blk = find_memory_block_hinted(mem_sect, mem_blk);

		ret = register_mem_sect_under_node(mem_blk, nid);
		if (!err)
			err = ret;

		
	}

	if (mem_blk)
		kobject_put(&mem_blk->dev.kobj);
	return err;
}

#ifdef CONFIG_HUGETLBFS
static void node_hugetlb_work(struct work_struct *work)
{
	struct node *node = container_of(work, struct node, node_work);

	if (!hugetlb_register_node(node))
		hugetlb_unregister_node(node);
}

static void init_node_hugetlb_work(int nid)
{
	INIT_WORK(&node_devices[nid].node_work, node_hugetlb_work);
}

static int node_memory_callback(struct notifier_block *self,
				unsigned long action, void *arg)
{
	struct memory_notify *mnb = arg;
	int nid = mnb->status_change_nid;

	switch (action) {
	case MEM_ONLINE:
	case MEM_OFFLINE:
		if (nid != NUMA_NO_NODE)
			schedule_work(&node_devices[nid].node_work);
		break;

	case MEM_GOING_ONLINE:
	case MEM_GOING_OFFLINE:
	case MEM_CANCEL_ONLINE:
	case MEM_CANCEL_OFFLINE:
	default:
		break;
	}

	return NOTIFY_OK;
}
#endif	
#else	

static int link_mem_sections(int nid) { return 0; }
#endif	

#if !defined(CONFIG_MEMORY_HOTPLUG_SPARSE) || \
    !defined(CONFIG_HUGETLBFS)
static inline int node_memory_callback(struct notifier_block *self,
				unsigned long action, void *arg)
{
	return NOTIFY_OK;
}

static void init_node_hugetlb_work(int nid) { }

#endif

int register_one_node(int nid)
{
	int error = 0;
	int cpu;

	if (node_online(nid)) {
		int p_node = parent_node(nid);
		struct node *parent = NULL;

		if (p_node != nid)
			parent = &node_devices[p_node];

		error = register_node(&node_devices[nid], nid, parent);

		
		for_each_present_cpu(cpu) {
			if (cpu_to_node(cpu) == nid)
				register_cpu_under_node(cpu, nid);
		}

		
		error = link_mem_sections(nid);

		
		init_node_hugetlb_work(nid);
	}

	return error;

}

void unregister_one_node(int nid)
{
	unregister_node(&node_devices[nid]);
}


static ssize_t print_nodes_state(enum node_states state, char *buf)
{
	int n;

	n = nodelist_scnprintf(buf, PAGE_SIZE, node_states[state]);
	if (n > 0 && PAGE_SIZE > n + 1) {
		*(buf + n++) = '\n';
		*(buf + n++) = '\0';
	}
	return n;
}

struct node_attr {
	struct device_attribute attr;
	enum node_states state;
};

static ssize_t show_node_state(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	struct node_attr *na = container_of(attr, struct node_attr, attr);
	return print_nodes_state(na->state, buf);
}

#define _NODE_ATTR(name, state) \
	{ __ATTR(name, 0444, show_node_state, NULL), state }

static struct node_attr node_state_attr[] = {
	_NODE_ATTR(possible, N_POSSIBLE),
	_NODE_ATTR(online, N_ONLINE),
	_NODE_ATTR(has_normal_memory, N_NORMAL_MEMORY),
	_NODE_ATTR(has_cpu, N_CPU),
#ifdef CONFIG_HIGHMEM
	_NODE_ATTR(has_high_memory, N_HIGH_MEMORY),
#endif
};

static struct attribute *node_state_attrs[] = {
	&node_state_attr[0].attr.attr,
	&node_state_attr[1].attr.attr,
	&node_state_attr[2].attr.attr,
	&node_state_attr[3].attr.attr,
#ifdef CONFIG_HIGHMEM
	&node_state_attr[4].attr.attr,
#endif
	NULL
};

static struct attribute_group memory_root_attr_group = {
	.attrs = node_state_attrs,
};

static const struct attribute_group *cpu_root_attr_groups[] = {
	&memory_root_attr_group,
	NULL,
};

#define NODE_CALLBACK_PRI	2	
static int __init register_node_type(void)
{
	int ret;

 	BUILD_BUG_ON(ARRAY_SIZE(node_state_attr) != NR_NODE_STATES);
 	BUILD_BUG_ON(ARRAY_SIZE(node_state_attrs)-1 != NR_NODE_STATES);

	ret = subsys_system_register(&node_subsys, cpu_root_attr_groups);
	if (!ret) {
		hotplug_memory_notifier(node_memory_callback,
					NODE_CALLBACK_PRI);
	}

	return ret;
}
postcore_initcall(register_node_type);
