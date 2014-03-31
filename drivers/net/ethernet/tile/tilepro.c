/*
 * Copyright 2011 Tilera Corporation. All Rights Reserved.
 *
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation, version 2.
 *
 *   This program is distributed in the hope that it will be useful, but
 *   WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE, GOOD TITLE or
 *   NON INFRINGEMENT.  See the GNU General Public License for
 *   more details.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/moduleparam.h>
#include <linux/sched.h>
#include <linux/kernel.h>      
#include <linux/slab.h>        
#include <linux/errno.h>       
#include <linux/types.h>       
#include <linux/interrupt.h>
#include <linux/in.h>
#include <linux/netdevice.h>   
#include <linux/etherdevice.h> 
#include <linux/skbuff.h>
#include <linux/ioctl.h>
#include <linux/cdev.h>
#include <linux/hugetlb.h>
#include <linux/in6.h>
#include <linux/timer.h>
#include <linux/io.h>
#include <asm/checksum.h>
#include <asm/homecache.h>

#include <hv/drv_xgbe_intf.h>
#include <hv/drv_xgbe_impl.h>
#include <hv/hypervisor.h>
#include <hv/netio_intf.h>

#include <linux/ip.h>
#include <linux/tcp.h>




#define TILE_NET_MTU 1500




#define TILE_NET_TX_QUEUE_LEN 0





#define TILE_NET_TIMEOUT (5 * HZ)

#define TILE_NET_RETRY_INTERVAL (5 * HZ)

#define TILE_NET_DEVS 4



#if NET_IP_ALIGN != LIPP_PACKET_PADDING
#error "NET_IP_ALIGN must match LIPP_PACKET_PADDING."
#endif


#ifdef TILE_NET_DEBUG
#define PDEBUG(fmt, args...) net_printk(fmt, ## args)
#else
#define PDEBUG(fmt, args...)
#endif


MODULE_AUTHOR("Tilera");
MODULE_LICENSE("GPL");


struct tile_netio_queue {
	netio_queue_impl_t *__system_part;
	netio_queue_user_impl_t __user_part;

};


struct tile_net_stats_t {
	u32 rx_packets;
	u32 rx_bytes;
	u32 tx_packets;
	u32 tx_bytes;
};


struct tile_net_cpu {
	
	struct napi_struct napi;
	
	struct tile_netio_queue queue;
	
	struct tile_net_stats_t stats;
	
	bool napi_enabled;
	
	bool registered;
	
	bool link_down;
	
	bool egress_timer_scheduled;
	
	unsigned int num_needed_small_buffers;
	
	unsigned int num_needed_large_buffers;
	
	struct timer_list egress_timer;
};


struct tile_net_priv {
	
	struct net_device *dev;
	
	struct page *eq_pages;
	
	lepp_queue_t *eq;
	
	spinlock_t eq_lock;
	
	int hv_devhdl;
	
	u32 intr_id;
	
	bool partly_opened;
	
	bool active;
	
	struct cpumask network_cpus_map;
	
	int network_cpus_count;
	
	int network_cpus_credits;
	
	struct net_device_stats stats;
	
	struct delayed_work retry_work;
	
	struct tile_net_cpu *cpu[NR_CPUS];
};

#define EQ_ORDER  get_order(sizeof(lepp_queue_t))
#define EQ_SIZE   (1 << (PAGE_SHIFT + EQ_ORDER))

static struct net_device *tile_net_devs[TILE_NET_DEVS];

static DEFINE_PER_CPU(struct tile_net_cpu, hv_xgbe0);
static DEFINE_PER_CPU(struct tile_net_cpu, hv_xgbe1);
static DEFINE_PER_CPU(struct tile_net_cpu, hv_gbe0);
static DEFINE_PER_CPU(struct tile_net_cpu, hv_gbe1);


static bool network_cpus_used;

static struct cpumask network_cpus_map;



#ifdef TILE_NET_DEBUG
static void net_printk(char *fmt, ...)
{
	int i;
	int len;
	va_list args;
	static char buf[256];

	len = sprintf(buf, "tile_net[%2.2d]: ", smp_processor_id());
	va_start(args, fmt);
	i = vscnprintf(buf + len, sizeof(buf) - len - 1, fmt, args);
	va_end(args);
	buf[255] = '\0';
	pr_notice(buf);
}
#endif


#ifdef TILE_NET_DUMP_PACKETS
static void dump_packet(unsigned char *data, unsigned long length, char *s)
{
	int my_cpu = smp_processor_id();

	unsigned long i;
	char buf[128];

	static unsigned int count;

	pr_info("dump_packet(data %p, length 0x%lx s %s count 0x%x)\n",
	       data, length, s, count++);

	pr_info("\n");

	for (i = 0; i < length; i++) {
		if ((i & 0xf) == 0)
			sprintf(buf, "[%02d] %8.8lx:", my_cpu, i);
		sprintf(buf + strlen(buf), " %2.2x", data[i]);
		if ((i & 0xf) == 0xf || i == length - 1) {
			strcat(buf, "\n");
			pr_info("%s", buf);
		}
	}
}
#endif


inline int __netio_fastio1(u32 fastio_index, u32 arg0)
{
	long result, clobber_r1, clobber_r10;
	asm volatile("swint2"
		     : "=R00" (result),
		       "=R01" (clobber_r1), "=R10" (clobber_r10)
		     : "R10" (fastio_index), "R01" (arg0)
		     : "memory", "r2", "r3", "r4",
		       "r5", "r6", "r7", "r8", "r9",
		       "r11", "r12", "r13", "r14",
		       "r15", "r16", "r17", "r18", "r19",
		       "r20", "r21", "r22", "r23", "r24",
		       "r25", "r26", "r27", "r28", "r29");
	return result;
}


static void tile_net_return_credit(struct tile_net_cpu *info)
{
	struct tile_netio_queue *queue = &info->queue;
	netio_queue_user_impl_t *qup = &queue->__user_part;

	
	if (--qup->__receive_credit_remaining == 0) {
		u32 interval = qup->__receive_credit_interval;
		qup->__receive_credit_remaining = interval;
		__netio_fastio_return_credits(qup->__fastio_index, interval);
	}
}



static void tile_net_provide_linux_buffer(struct tile_net_cpu *info,
					  void *va, bool small)
{
	struct tile_netio_queue *queue = &info->queue;

	
	unsigned int buffer = ((unsigned int)(__pa(va) >> 7) << 1) + small;

	__netio_fastio_free_buffer(queue->__user_part.__fastio_index, buffer);
}


static bool tile_net_provide_needed_buffer(struct tile_net_cpu *info,
					   bool small)
{
#if TILE_NET_MTU <= 1536
	
	unsigned int large_size = NET_IP_ALIGN + 1536;
#else
	
	unsigned int large_size = NET_IP_ALIGN + TILE_NET_MTU + 100;
#endif

	
	
	unsigned int len =
		 (((small ? LIPP_SMALL_PACKET_SIZE : large_size) +
		   CHIP_L2_LINE_SIZE() - 1) & -CHIP_L2_LINE_SIZE());

	unsigned int padding = 128 - NET_SKB_PAD;
	unsigned int align;

	struct sk_buff *skb;
	void *va;

	struct sk_buff **skb_ptr;

	
	skb = netdev_alloc_skb(info->napi.dev, len + padding);
	if (skb == NULL)
		return false;

	
	align = -(long)skb->data & (128 - 1);
	BUG_ON(align > padding);
	skb_reserve(skb, align);

	
	va = skb->data;

	
	BUG_ON(((((long)va & ~HPAGE_MASK) + len) & HPAGE_MASK) != 0);

#ifdef TILE_NET_PARANOIA
#if CHIP_HAS_CBOX_HOME_MAP()
	if (hash_default) {
		HV_PTE pte = *virt_to_pte(current->mm, (unsigned long)va);
		if (hv_pte_get_mode(pte) != HV_PTE_MODE_CACHE_HASH_L3)
			panic("Non-HFH ingress buffer! VA=%p Mode=%d PTE=%llx",
			      va, hv_pte_get_mode(pte), hv_pte_val(pte));
	}
#endif
#endif

	
	if (!hash_default)
		__inv_buffer(va, len);

	
	
	
	skb_reserve(skb, NET_IP_ALIGN);

	
	skb_ptr = va - sizeof(*skb_ptr);
	*skb_ptr = skb;

	
	__insn_mf();

	
	tile_net_provide_linux_buffer(info, va, small);

	return true;
}


static void tile_net_provide_needed_buffers(struct tile_net_cpu *info)
{
	while (info->num_needed_small_buffers != 0) {
		if (!tile_net_provide_needed_buffer(info, true))
			goto oops;
		info->num_needed_small_buffers--;
	}

	while (info->num_needed_large_buffers != 0) {
		if (!tile_net_provide_needed_buffer(info, false))
			goto oops;
		info->num_needed_large_buffers--;
	}

	return;

oops:

	
	pr_notice("Could not provide a linux buffer to LIPP.\n");
}


static unsigned int tile_net_lepp_grab_comps(lepp_queue_t *eq,
					     struct sk_buff *comps[],
					     unsigned int comps_size,
					     unsigned int min_size)
{
	unsigned int n = 0;

	unsigned int comp_head = eq->comp_head;
	unsigned int comp_busy = eq->comp_busy;

	while (comp_head != comp_busy && n < comps_size) {
		comps[n++] = eq->comps[comp_head];
		LEPP_QINC(comp_head);
	}

	if (n < min_size)
		return 0;

	eq->comp_head = comp_head;

	return n;
}


static bool tile_net_lepp_free_comps(struct net_device *dev, bool all)
{
	struct tile_net_priv *priv = netdev_priv(dev);

	lepp_queue_t *eq = priv->eq;

	struct sk_buff *olds[64];
	unsigned int wanted = 64;
	unsigned int i, n;
	bool pending;

	spin_lock(&priv->eq_lock);

	if (all)
		eq->comp_busy = eq->comp_tail;

	n = tile_net_lepp_grab_comps(eq, olds, wanted, 0);

	pending = (eq->comp_head != eq->comp_tail);

	spin_unlock(&priv->eq_lock);

	for (i = 0; i < n; i++)
		kfree_skb(olds[i]);

	return pending;
}


static void tile_net_schedule_egress_timer(struct tile_net_cpu *info)
{
	if (!info->egress_timer_scheduled) {
		mod_timer_pinned(&info->egress_timer, jiffies + 1);
		info->egress_timer_scheduled = true;
	}
}


static void tile_net_handle_egress_timer(unsigned long arg)
{
	struct tile_net_cpu *info = (struct tile_net_cpu *)arg;
	struct net_device *dev = info->napi.dev;

	
	info->egress_timer_scheduled = false;

	
	if (tile_net_lepp_free_comps(dev, false))
		tile_net_schedule_egress_timer(info);
}


#ifdef IGNORE_DUP_ACKS

static bool is_dup_ack(char *s1, char *s2, unsigned int len)
{
	int i;

	unsigned long long ignorable = 0;

	
	ignorable |= (1ULL << 0x12);
	ignorable |= (1ULL << 0x13);

	
	ignorable |= (1ULL << 0x18);
	ignorable |= (1ULL << 0x19);

	
	ignorable |= (1ULL << 0x2a);
	ignorable |= (1ULL << 0x2b);
	ignorable |= (1ULL << 0x2c);
	ignorable |= (1ULL << 0x2d);

	
	ignorable |= (1ULL << 0x30);
	ignorable |= (1ULL << 0x31);

	
	ignorable |= (1ULL << 0x32);
	ignorable |= (1ULL << 0x33);

	for (i = 0; i < len; i++, ignorable >>= 1) {

		if ((ignorable & 1) || (s1[i] == s2[i]))
			continue;

#ifdef TILE_NET_DEBUG
		
		if (i < 0x38 && i != 0x2f &&
		    net_ratelimit())
			pr_info("Diff at 0x%x\n", i);
#endif

		return false;
	}

#ifdef TILE_NET_NO_SUPPRESS_DUP_ACKS
	
	
	if (s1[0x2a] == s2[0x2a] &&
	    s1[0x2b] == s2[0x2b] &&
	    s1[0x2c] == s2[0x2c] &&
	    s1[0x2d] == s2[0x2d]) {
		return false;
	}
#endif

	return true;
}

#endif



static void tile_net_discard_aux(struct tile_net_cpu *info, int index)
{
	struct tile_netio_queue *queue = &info->queue;
	netio_queue_impl_t *qsp = queue->__system_part;
	netio_queue_user_impl_t *qup = &queue->__user_part;

	int index2_aux = index + sizeof(netio_pkt_t);
	int index2 =
		((index2_aux ==
		  qsp->__packet_receive_queue.__last_packet_plus_one) ?
		 0 : index2_aux);

	netio_pkt_t *pkt = (netio_pkt_t *)((unsigned long) &qsp[1] + index);

	
	unsigned int buffer = pkt->__packet.word;

	
	void *va = __va((phys_addr_t)(buffer >> 1) << 7);

	
	struct sk_buff **skb_ptr = va - sizeof(*skb_ptr);
	struct sk_buff *skb = *skb_ptr;

	kfree_skb(skb);

	
	qup->__packet_receive_read = index2;
}


static void tile_net_discard_packets(struct net_device *dev)
{
	struct tile_net_priv *priv = netdev_priv(dev);
	int my_cpu = smp_processor_id();
	struct tile_net_cpu *info = priv->cpu[my_cpu];
	struct tile_netio_queue *queue = &info->queue;
	netio_queue_impl_t *qsp = queue->__system_part;
	netio_queue_user_impl_t *qup = &queue->__user_part;

	while (qup->__packet_receive_read !=
	       qsp->__packet_receive_queue.__packet_write) {
		int index = qup->__packet_receive_read;
		tile_net_discard_aux(info, index);
	}
}


static bool tile_net_poll_aux(struct tile_net_cpu *info, int index)
{
	struct net_device *dev = info->napi.dev;

	struct tile_netio_queue *queue = &info->queue;
	netio_queue_impl_t *qsp = queue->__system_part;
	netio_queue_user_impl_t *qup = &queue->__user_part;
	struct tile_net_stats_t *stats = &info->stats;

	int filter;

	int index2_aux = index + sizeof(netio_pkt_t);
	int index2 =
		((index2_aux ==
		  qsp->__packet_receive_queue.__last_packet_plus_one) ?
		 0 : index2_aux);

	netio_pkt_t *pkt = (netio_pkt_t *)((unsigned long) &qsp[1] + index);

	netio_pkt_metadata_t *metadata = NETIO_PKT_METADATA(pkt);

	
	
	unsigned long len =
		(NETIO_PKT_CUSTOM_LENGTH(pkt) +
		 NET_IP_ALIGN - NETIO_PACKET_PADDING);

	
	unsigned int buffer = pkt->__packet.word;

	
	bool small = ((buffer & 1) != 0);

	
	void *va = __va((phys_addr_t)(buffer >> 1) << 7);

	
	
	unsigned char *buf = va + NET_IP_ALIGN;

	
	if (!hash_default)
		__inv_buffer(buf, len);

	
	dev->last_rx = jiffies;

#ifdef TILE_NET_DUMP_PACKETS
	dump_packet(buf, len, "rx");
#endif 

#ifdef TILE_NET_VERIFY_INGRESS
	if (!NETIO_PKT_L4_CSUM_CORRECT_M(metadata, pkt) &&
	    NETIO_PKT_L4_CSUM_CALCULATED_M(metadata, pkt)) {
		
		pr_warning("Bad L4 checksum on %d byte packet.\n", len);
	}
	if (!NETIO_PKT_L3_CSUM_CORRECT_M(metadata, pkt) &&
	    NETIO_PKT_L3_CSUM_CALCULATED_M(metadata, pkt)) {
		dump_packet(buf, len, "rx");
		panic("Bad L3 checksum.");
	}
	switch (NETIO_PKT_STATUS_M(metadata, pkt)) {
	case NETIO_PKT_STATUS_OVERSIZE:
		if (len >= 64) {
			dump_packet(buf, len, "rx");
			panic("Unexpected OVERSIZE.");
		}
		break;
	case NETIO_PKT_STATUS_BAD:
		pr_warning("Unexpected BAD %ld byte packet.\n", len);
	}
#endif

	filter = 0;

	

	if (!(dev->flags & IFF_UP)) {
		
		filter = 1;
	} else if (NETIO_PKT_STATUS_M(metadata, pkt) == NETIO_PKT_STATUS_BAD) {
		
		filter = 1;
	} else if (!(dev->flags & IFF_PROMISC)) {
		
		if (!is_multicast_ether_addr(buf)) {
			
			const u8 *mine = dev->dev_addr;
			filter = compare_ether_addr(mine, buf);
		}
	}

	if (filter) {

		

		tile_net_provide_linux_buffer(info, va, small);

	} else {

		
		struct sk_buff **skb_ptr = va - sizeof(*skb_ptr);
		struct sk_buff *skb = *skb_ptr;

		
		if (skb->data != buf)
			panic("Corrupt linux buffer from LIPP! "
			      "VA=%p, skb=%p, skb->data=%p\n",
			      va, skb, skb->data);

		
		skb_put(skb, len);

		
		skb->protocol = eth_type_trans(skb, dev);

		
		if (NETIO_PKT_L4_CSUM_CORRECT_M(metadata, pkt))
			skb->ip_summed = CHECKSUM_UNNECESSARY;

		netif_receive_skb(skb);

		stats->rx_packets++;
		stats->rx_bytes += len;
	}

	
	
	tile_net_return_credit(info);

	
	qup->__packet_receive_read = index2;

	return !filter;
}


static int tile_net_poll(struct napi_struct *napi, int budget)
{
	struct net_device *dev = napi->dev;
	struct tile_net_priv *priv = netdev_priv(dev);
	int my_cpu = smp_processor_id();
	struct tile_net_cpu *info = priv->cpu[my_cpu];
	struct tile_netio_queue *queue = &info->queue;
	netio_queue_impl_t *qsp = queue->__system_part;
	netio_queue_user_impl_t *qup = &queue->__user_part;

	unsigned int work = 0;

	while (priv->active) {
		int index = qup->__packet_receive_read;
		if (index == qsp->__packet_receive_queue.__packet_write)
			break;

		if (tile_net_poll_aux(info, index)) {
			if (++work >= budget)
				goto done;
		}
	}

	napi_complete(&info->napi);

	if (!priv->active)
		goto done;

	
	enable_percpu_irq(priv->intr_id, 0);

	
	if (qup->__packet_receive_read !=
	    qsp->__packet_receive_queue.__packet_write) {
		
		
		(void)napi_reschedule(&info->napi);
	}

done:

	if (priv->active)
		tile_net_provide_needed_buffers(info);

	return work;
}


static irqreturn_t tile_net_handle_ingress_interrupt(int irq, void *dev_ptr)
{
	struct net_device *dev = (struct net_device *)dev_ptr;
	struct tile_net_priv *priv = netdev_priv(dev);
	int my_cpu = smp_processor_id();
	struct tile_net_cpu *info = priv->cpu[my_cpu];

	
	disable_percpu_irq(priv->intr_id);

	
	if (!priv->active)
		return IRQ_HANDLED;

	

	napi_schedule(&info->napi);

	return IRQ_HANDLED;
}


static int tile_net_open_aux(struct net_device *dev)
{
	struct tile_net_priv *priv = netdev_priv(dev);

	int ret;
	int dummy;
	unsigned int epp_lotar;

	ret = hv_dev_pread(priv->hv_devhdl, 0,
			   (HV_VirtAddr)&epp_lotar, sizeof(epp_lotar),
			   NETIO_EPP_SHM_OFF);
	if (ret < 0) {
		pr_err("could not read epp_shm_queue lotar.\n");
		return -EIO;
	}

	{
		int epp_home = hv_lotar_to_cpu(epp_lotar);
		homecache_change_page_home(priv->eq_pages, EQ_ORDER, epp_home);
	}

	{
		netio_ipp_address_t ea = {
			.va = 0,
			.pa = __pa(priv->eq),
			.pte = hv_pte(0),
			.size = EQ_SIZE,
		};
		ea.pte = hv_pte_set_lotar(ea.pte, epp_lotar);
		ea.pte = hv_pte_set_mode(ea.pte, HV_PTE_MODE_CACHE_TILE_L3);
		ret = hv_dev_pwrite(priv->hv_devhdl, 0,
				    (HV_VirtAddr)&ea,
				    sizeof(ea),
				    NETIO_EPP_SHM_OFF);
		if (ret < 0)
			return -EIO;
	}

	if (hv_dev_pwrite(priv->hv_devhdl, 0, (HV_VirtAddr)&dummy,
			  sizeof(dummy), NETIO_IPP_START_SHIM_OFF) < 0) {
		pr_warning("Failed to start LIPP/LEPP.\n");
		return -EIO;
	}

	return 0;
}


static void tile_net_register(void *dev_ptr)
{
	struct net_device *dev = (struct net_device *)dev_ptr;
	struct tile_net_priv *priv = netdev_priv(dev);
	int my_cpu = smp_processor_id();
	struct tile_net_cpu *info;

	struct tile_netio_queue *queue;

	
	int queue_id =
		cpumask_test_cpu(my_cpu, &priv->network_cpus_map) ? 0 : 255;

	netio_input_config_t config = {
		.flags = 0,
		.num_receive_packets = priv->network_cpus_credits,
		.queue_id = queue_id
	};

	int ret = 0;
	netio_queue_impl_t *queuep;

	PDEBUG("tile_net_register(queue_id %d)\n", queue_id);

	if (!strcmp(dev->name, "xgbe0"))
		info = &__get_cpu_var(hv_xgbe0);
	else if (!strcmp(dev->name, "xgbe1"))
		info = &__get_cpu_var(hv_xgbe1);
	else if (!strcmp(dev->name, "gbe0"))
		info = &__get_cpu_var(hv_gbe0);
	else if (!strcmp(dev->name, "gbe1"))
		info = &__get_cpu_var(hv_gbe1);
	else
		BUG();

	
	init_timer(&info->egress_timer);
	info->egress_timer.data = (long)info;
	info->egress_timer.function = tile_net_handle_egress_timer;

	priv->cpu[my_cpu] = info;

	ret = hv_dev_pwrite(priv->hv_devhdl, 0,
			    (HV_VirtAddr)&config,
			    sizeof(netio_input_config_t),
			    NETIO_IPP_INPUT_REGISTER_OFF);
	PDEBUG("hv_dev_pwrite(NETIO_IPP_INPUT_REGISTER_OFF) returned %d\n",
	       ret);
	if (ret < 0) {
		if (ret != NETIO_LINK_DOWN) {
			printk(KERN_DEBUG "hv_dev_pwrite "
			       "NETIO_IPP_INPUT_REGISTER_OFF failure %d\n",
			       ret);
		}
		info->link_down = (ret == NETIO_LINK_DOWN);
		return;
	}


	ret = hv_dev_pread(priv->hv_devhdl, 0,
			   (HV_VirtAddr)&queuep,
			   sizeof(netio_queue_impl_t *),
			   NETIO_IPP_INPUT_REGISTER_OFF);
	PDEBUG("hv_dev_pread(NETIO_IPP_INPUT_REGISTER_OFF) returned %d\n",
	       ret);
	PDEBUG("queuep %p\n", queuep);
	if (ret <= 0) {
		
		pr_err("hv_dev_pread NETIO_IPP_INPUT_REGISTER_OFF failure\n");
		return;
	}

	queue = &info->queue;

	queue->__system_part = queuep;

	memset(&queue->__user_part, 0, sizeof(netio_queue_user_impl_t));

	
	queue->__user_part.__receive_credit_interval = 4;
	queue->__user_part.__receive_credit_remaining =
		queue->__user_part.__receive_credit_interval;

	ret = hv_dev_pread(priv->hv_devhdl, 0,
			   (HV_VirtAddr)&queue->__user_part.__fastio_index,
			   sizeof(queue->__user_part.__fastio_index),
			   NETIO_IPP_GET_FASTIO_OFF);
	PDEBUG("hv_dev_pread(NETIO_IPP_GET_FASTIO_OFF) returned %d\n", ret);

	
	info->registered = true;
}


static void tile_net_deregister(void *dev_ptr)
{
	struct net_device *dev = (struct net_device *)dev_ptr;
	struct tile_net_priv *priv = netdev_priv(dev);
	int my_cpu = smp_processor_id();
	struct tile_net_cpu *info = priv->cpu[my_cpu];

	
	disable_percpu_irq(priv->intr_id);

	
	if (info == NULL || !info->registered)
		return;

	{
		struct tile_netio_queue *queue = &info->queue;
		netio_queue_user_impl_t *qup = &queue->__user_part;

		
		__netio_fastio_return_credits(qup->__fastio_index, -1);
	}
}


static void tile_net_unregister(void *dev_ptr)
{
	struct net_device *dev = (struct net_device *)dev_ptr;
	struct tile_net_priv *priv = netdev_priv(dev);
	int my_cpu = smp_processor_id();
	struct tile_net_cpu *info = priv->cpu[my_cpu];

	int ret;
	int dummy = 0;

	
	disable_percpu_irq(priv->intr_id);

	
	if (info == NULL || !info->registered)
		return;

	
	ret = hv_dev_pwrite(priv->hv_devhdl, 0, (HV_VirtAddr)&dummy,
			    sizeof(dummy), NETIO_IPP_INPUT_UNREGISTER_OFF);
	if (ret < 0)
		panic("Failed to unregister with LIPP/LEPP!\n");

	
	tile_net_discard_packets(dev);

	
	info->num_needed_small_buffers = 0;
	info->num_needed_large_buffers = 0;

	
	del_timer(&info->egress_timer);
	info->egress_timer_scheduled = false;
}


static void tile_net_stop_aux(struct net_device *dev)
{
	struct tile_net_priv *priv = netdev_priv(dev);
	int i;

	int dummy = 0;

	on_each_cpu(tile_net_unregister, (void *)dev, 1);
	for_each_online_cpu(i) {
		struct tile_net_cpu *info = priv->cpu[i];
		if (info != NULL && info->registered) {
			netif_napi_del(&info->napi);
			info->registered = false;
		}
	}

	
	if (hv_dev_pwrite(priv->hv_devhdl, 0, (HV_VirtAddr)&dummy,
			  sizeof(dummy), NETIO_IPP_STOP_SHIM_OFF) < 0)
		panic("Failed to stop LIPP/LEPP!\n");

	priv->partly_opened = false;
}


static void tile_net_stop_disable(void *dev_ptr)
{
	struct net_device *dev = (struct net_device *)dev_ptr;
	struct tile_net_priv *priv = netdev_priv(dev);
	int my_cpu = smp_processor_id();
	struct tile_net_cpu *info = priv->cpu[my_cpu];

	
	if (info != NULL && info->napi_enabled) {
		napi_disable(&info->napi);
		info->napi_enabled = false;
	}
}


static void tile_net_open_enable(void *dev_ptr)
{
	struct net_device *dev = (struct net_device *)dev_ptr;
	struct tile_net_priv *priv = netdev_priv(dev);
	int my_cpu = smp_processor_id();
	struct tile_net_cpu *info = priv->cpu[my_cpu];

	
	napi_enable(&info->napi);
	info->napi_enabled = true;

	
	enable_percpu_irq(priv->intr_id, 0);
}


static int tile_net_open_inner(struct net_device *dev)
{
	struct tile_net_priv *priv = netdev_priv(dev);
	int my_cpu = smp_processor_id();
	struct tile_net_cpu *info;
	struct tile_netio_queue *queue;
	int result = 0;
	int i;
	int dummy = 0;

	tile_net_register(dev);
	info = priv->cpu[my_cpu];
	if (!info->registered) {
		if (info->link_down)
			return 1;
		return -EAGAIN;
	}

	smp_call_function(tile_net_register, (void *)dev, 1);
	for_each_online_cpu(i) {
		struct tile_net_cpu *info = priv->cpu[i];
		if (info->registered)
			netif_napi_add(dev, &info->napi, tile_net_poll, 64);
		else
			result = -EAGAIN;
	}
	if (result != 0) {
		tile_net_stop_aux(dev);
		return result;
	}

	queue = &info->queue;

	if (priv->intr_id == 0) {
		unsigned int irq;

		priv->intr_id = queue->__system_part->__intr_id;
		BUG_ON(priv->intr_id == 0);
		irq = __ffs(priv->intr_id);

		tile_irq_activate(irq, TILE_IRQ_PERCPU);
		BUG_ON(request_irq(irq, tile_net_handle_ingress_interrupt,
				   0, dev->name, (void *)dev) != 0);
	}

	{
		

		int max_buffers =
			priv->network_cpus_count * priv->network_cpus_credits;

		info->num_needed_small_buffers =
			min(LIPP_SMALL_BUFFERS, max_buffers);

		info->num_needed_large_buffers =
			min(LIPP_LARGE_BUFFERS, max_buffers);

		tile_net_provide_needed_buffers(info);

		if (info->num_needed_small_buffers != 0 ||
		    info->num_needed_large_buffers != 0)
			panic("Insufficient memory for buffer stack!");
	}

	
	priv->active = true;

	
	mb();

	
	on_each_cpu(tile_net_open_enable, (void *)dev, 1);

	
	if (hv_dev_pwrite(priv->hv_devhdl, 0, (HV_VirtAddr)&dummy,
			  sizeof(dummy), NETIO_IPP_INPUT_INIT_OFF) < 0)
		panic("Failed to activate the LIPP Shim!\n");

	
	netif_start_queue(dev);

	return 0;
}


static void tile_net_open_retry(struct work_struct *w)
{
	struct delayed_work *dw =
		container_of(w, struct delayed_work, work);

	struct tile_net_priv *priv =
		container_of(dw, struct tile_net_priv, retry_work);

	if (tile_net_open_inner(priv->dev) != 0)
		schedule_delayed_work(&priv->retry_work,
				      TILE_NET_RETRY_INTERVAL);
	else
		netif_carrier_on(priv->dev);
}


static int tile_net_open(struct net_device *dev)
{
	int ret = 0;
	struct tile_net_priv *priv = netdev_priv(dev);

	if (!priv->partly_opened) {

		int count;
		int credits;

		
		ret = tile_net_open_aux(dev);
		if (ret < 0) {
			pr_err("tile_net_open_aux failed: %d\n", ret);
			return ret;
		}

		

		if (network_cpus_used)
			cpumask_copy(&priv->network_cpus_map,
				     &network_cpus_map);
		else
			cpumask_copy(&priv->network_cpus_map, cpu_online_mask);


		count = cpumask_weight(&priv->network_cpus_map);

		
		credits = max(16, (LIPP_LARGE_BUFFERS / count) & ~1);

		
		
		credits = min(NETIO_MAX_RECEIVE_PKTS, credits);

		priv->network_cpus_count = count;
		priv->network_cpus_credits = credits;

#ifdef TILE_NET_DEBUG
		pr_info("Using %d network cpus, with %d credits each\n",
		       priv->network_cpus_count, priv->network_cpus_credits);
#endif

		priv->partly_opened = true;

	} else {
		
		
	}

	ret = tile_net_open_inner(dev);
	if (ret <= 0) {
		if (ret == 0)
			netif_carrier_on(dev);
		return ret;
	}

	netif_carrier_off(dev);
	schedule_delayed_work(&priv->retry_work, TILE_NET_RETRY_INTERVAL);

	return 0;
}


static int tile_net_drain_lipp_buffers(struct tile_net_priv *priv)
{
	int n = 0;

	
	while (true) {
		unsigned int buffer;

		
		if (hv_dev_pread(priv->hv_devhdl, 0, (HV_VirtAddr)&buffer,
				 sizeof(buffer), NETIO_IPP_DRAIN_OFF) < 0)
			break;

		
		if (buffer == 0)
			break;

		{
			
			void *va = __va((phys_addr_t)(buffer >> 1) << 7);

			
			struct sk_buff **skb_ptr = va - sizeof(*skb_ptr);
			struct sk_buff *skb = *skb_ptr;

			kfree_skb(skb);
		}

		n++;
	}

	return n;
}


static int tile_net_stop(struct net_device *dev)
{
	struct tile_net_priv *priv = netdev_priv(dev);

	PDEBUG("tile_net_stop()\n");

	
	priv->active = false;

	
	mb();

	on_each_cpu(tile_net_deregister, (void *)dev, 1);

	
	(void)tile_net_drain_lipp_buffers(priv);

	
	cancel_delayed_work_sync(&priv->retry_work);

	
	netif_stop_queue(dev);

	
	on_each_cpu(tile_net_stop_disable, (void *)dev, 1);

	if (tile_net_drain_lipp_buffers(priv) != 0)
		printk("Had to drain some extra LIPP buffers!\n");

	
	tile_net_stop_aux(dev);

	while (tile_net_lepp_free_comps(dev, true))
		;

	
	memset(priv->eq, 0, sizeof(lepp_queue_t));
	mb();

	return 0;
}


static unsigned int tile_net_tx_frags(lepp_frag_t *frags,
				      struct sk_buff *skb,
				      void *b_data, unsigned int b_len)
{
	unsigned int i, n = 0;

	struct skb_shared_info *sh = skb_shinfo(skb);

	phys_addr_t cpa;

	if (b_len != 0) {

		if (!hash_default)
			finv_buffer_remote(b_data, b_len, 0);

		cpa = __pa(b_data);
		frags[n].cpa_lo = cpa;
		frags[n].cpa_hi = cpa >> 32;
		frags[n].length = b_len;
		frags[n].hash_for_home = hash_default;
		n++;
	}

	for (i = 0; i < sh->nr_frags; i++) {

		skb_frag_t *f = &sh->frags[i];
		unsigned long pfn = page_to_pfn(skb_frag_page(f));

		
		
		int hash_for_home = hash_default;

		
		if (!hash_default) {
			void *va = pfn_to_kaddr(pfn) + f->page_offset;
			BUG_ON(PageHighMem(skb_frag_page(f)));
			finv_buffer_remote(va, skb_frag_size(f), 0);
		}

		cpa = ((phys_addr_t)pfn << PAGE_SHIFT) + f->page_offset;
		frags[n].cpa_lo = cpa;
		frags[n].cpa_hi = cpa >> 32;
		frags[n].length = skb_frag_size(f);
		frags[n].hash_for_home = hash_for_home;
		n++;
	}

	return n;
}


static int tile_net_tx_tso(struct sk_buff *skb, struct net_device *dev)
{
	struct tile_net_priv *priv = netdev_priv(dev);
	int my_cpu = smp_processor_id();
	struct tile_net_cpu *info = priv->cpu[my_cpu];
	struct tile_net_stats_t *stats = &info->stats;

	struct skb_shared_info *sh = skb_shinfo(skb);

	unsigned char *data = skb->data;

	
	struct iphdr *ih = ip_hdr(skb);
	unsigned int ih_len = ih->ihl * 4;

	
	unsigned char *nh = skb_network_header(skb);
	unsigned int eh_len = nh - data;

	
	struct tcphdr *th = (struct tcphdr *)(nh + ih_len);
	unsigned int th_len = th->doff * 4;

	
	
	unsigned int sh_len = eh_len + ih_len + th_len;

	
	
	unsigned int b_len = skb_headlen(skb) - sh_len;

	
	unsigned int d_len = b_len + skb->data_len;

	
	unsigned int p_len = sh->gso_size;

	
	unsigned int num_segs = sh->gso_segs;

	
	u32 cmd_body[(LEPP_MAX_CMD_SIZE + 3) / 4];
	lepp_tso_cmd_t *cmd = (lepp_tso_cmd_t *)cmd_body;

	
	unsigned int num_frags =
		tile_net_tx_frags(cmd->frags, skb, data + sh_len, b_len);

	
	size_t cmd_size = LEPP_TSO_CMD_SIZE(num_frags, sh_len);

	
	lepp_tso_cmd_t cmd_init = {
		.tso = true,
		.header_size = sh_len,
		.ip_offset = eh_len,
		.tcp_offset = eh_len + ih_len,
		.payload_size = p_len,
		.num_frags = num_frags,
	};

	unsigned long irqflags;

	lepp_queue_t *eq = priv->eq;

	struct sk_buff *olds[8];
	unsigned int wanted = 8;
	unsigned int i, nolds = 0;

	unsigned int cmd_head, cmd_tail, cmd_next;
	unsigned int comp_tail;


	
	BUG_ON(skb->protocol != htons(ETH_P_IP));
	BUG_ON(ih->protocol != IPPROTO_TCP);
	BUG_ON(skb->ip_summed != CHECKSUM_PARTIAL);
	BUG_ON(num_frags > LEPP_MAX_FRAGS);
	
	BUG_ON(num_segs <= 1);


	

	
	*cmd = cmd_init;

	
	memcpy(&cmd->frags[num_frags], data, sh_len);


	
	prefetch_L1(&eq->comp_tail);
	prefetch_L1(&eq->cmd_tail);
	mb();


	

	spin_lock_irqsave(&priv->eq_lock, irqflags);

	
	
	if (lepp_num_free_comp_slots(eq) == 0) {
		nolds = tile_net_lepp_grab_comps(eq, olds, wanted, 0);
		if (nolds == 0) {
busy:
			spin_unlock_irqrestore(&priv->eq_lock, irqflags);
			return NETDEV_TX_BUSY;
		}
	}

	cmd_head = eq->cmd_head;
	cmd_tail = eq->cmd_tail;

	
	
	cmd_next = cmd_tail + cmd_size;
	if (cmd_tail < cmd_head && cmd_next >= cmd_head)
		goto busy;
	if (cmd_next > LEPP_CMD_LIMIT) {
		cmd_next = 0;
		if (cmd_next == cmd_head)
			goto busy;
	}

	
	memcpy(&eq->cmds[cmd_tail], cmd, cmd_size);

	
	cmd_tail = cmd_next;

	
	comp_tail = eq->comp_tail;
	eq->comps[comp_tail] = skb;
	LEPP_QINC(comp_tail);
	eq->comp_tail = comp_tail;

	
	
	__insn_mf();

	eq->cmd_tail = cmd_tail;

	
	
	
	if (nolds == 0)
		nolds = tile_net_lepp_grab_comps(eq, olds, wanted, 4);

	spin_unlock_irqrestore(&priv->eq_lock, irqflags);

	
	for (i = 0; i < nolds; i++)
		kfree_skb(olds[i]);

	
	stats->tx_packets += num_segs;
	stats->tx_bytes += (num_segs * sh_len) + d_len;

	
	tile_net_schedule_egress_timer(info);

	return NETDEV_TX_OK;
}


static int tile_net_tx(struct sk_buff *skb, struct net_device *dev)
{
	struct tile_net_priv *priv = netdev_priv(dev);
	int my_cpu = smp_processor_id();
	struct tile_net_cpu *info = priv->cpu[my_cpu];
	struct tile_net_stats_t *stats = &info->stats;

	unsigned long irqflags;

	struct skb_shared_info *sh = skb_shinfo(skb);

	unsigned int len = skb->len;
	unsigned char *data = skb->data;

	unsigned int csum_start = skb_checksum_start_offset(skb);

	lepp_frag_t frags[LEPP_MAX_FRAGS];

	unsigned int num_frags;

	lepp_queue_t *eq = priv->eq;

	struct sk_buff *olds[8];
	unsigned int wanted = 8;
	unsigned int i, nolds = 0;

	unsigned int cmd_size = sizeof(lepp_cmd_t);

	unsigned int cmd_head, cmd_tail, cmd_next;
	unsigned int comp_tail;

	lepp_cmd_t cmds[LEPP_MAX_FRAGS];


	if (!info->registered)
		return NETDEV_TX_BUSY;


	
	dev->trans_start = jiffies;


#ifdef TILE_NET_PARANOIA
#if CHIP_HAS_CBOX_HOME_MAP()
	if (hash_default) {
		HV_PTE pte = *virt_to_pte(current->mm, (unsigned long)data);
		if (hv_pte_get_mode(pte) != HV_PTE_MODE_CACHE_HASH_L3)
			panic("Non-HFH egress buffer! VA=%p Mode=%d PTE=%llx",
			      data, hv_pte_get_mode(pte), hv_pte_val(pte));
	}
#endif
#endif


#ifdef TILE_NET_DUMP_PACKETS
	
	dump_packet(data, skb_headlen(skb), "tx");
#endif 


	if (sh->gso_size != 0)
		return tile_net_tx_tso(skb, dev);


	

	num_frags = tile_net_tx_frags(frags, skb, data, skb_headlen(skb));

	for (i = 0; i < num_frags; i++) {

		bool final = (i == num_frags - 1);

		lepp_cmd_t cmd = {
			.cpa_lo = frags[i].cpa_lo,
			.cpa_hi = frags[i].cpa_hi,
			.length = frags[i].length,
			.hash_for_home = frags[i].hash_for_home,
			.send_completion = final,
			.end_of_packet = final
		};

		if (i == 0 && skb->ip_summed == CHECKSUM_PARTIAL) {
			cmd.compute_checksum = 1;
			cmd.checksum_data.bits.start_byte = csum_start;
			cmd.checksum_data.bits.count = len - csum_start;
			cmd.checksum_data.bits.destination_byte =
				csum_start + skb->csum_offset;
		}

		cmds[i] = cmd;
	}


	
	prefetch_L1(&eq->comp_tail);
	prefetch_L1(&eq->cmd_tail);
	mb();


	

	spin_lock_irqsave(&priv->eq_lock, irqflags);

	
	
	if (lepp_num_free_comp_slots(eq) == 0) {
		nolds = tile_net_lepp_grab_comps(eq, olds, wanted, 0);
		if (nolds == 0) {
busy:
			spin_unlock_irqrestore(&priv->eq_lock, irqflags);
			return NETDEV_TX_BUSY;
		}
	}

	cmd_head = eq->cmd_head;
	cmd_tail = eq->cmd_tail;

	
	
	for (i = 0; i < num_frags; i++) {

		
		cmd_next = cmd_tail + cmd_size;
		if (cmd_tail < cmd_head && cmd_next >= cmd_head)
			goto busy;
		if (cmd_next > LEPP_CMD_LIMIT) {
			cmd_next = 0;
			if (cmd_next == cmd_head)
				goto busy;
		}

		
		*(lepp_cmd_t *)&eq->cmds[cmd_tail] = cmds[i];

		
		cmd_tail = cmd_next;
	}

	
	comp_tail = eq->comp_tail;
	eq->comps[comp_tail] = skb;
	LEPP_QINC(comp_tail);
	eq->comp_tail = comp_tail;

	
	
	__insn_mf();

	eq->cmd_tail = cmd_tail;

	
	
	
	if (nolds == 0)
		nolds = tile_net_lepp_grab_comps(eq, olds, wanted, 4);

	spin_unlock_irqrestore(&priv->eq_lock, irqflags);

	
	for (i = 0; i < nolds; i++)
		kfree_skb(olds[i]);

	
	stats->tx_packets++;
	stats->tx_bytes += ((len >= ETH_ZLEN) ? len : ETH_ZLEN);

	
	tile_net_schedule_egress_timer(info);

	return NETDEV_TX_OK;
}


static void tile_net_tx_timeout(struct net_device *dev)
{
	PDEBUG("tile_net_tx_timeout()\n");
	PDEBUG("Transmit timeout at %ld, latency %ld\n", jiffies,
	       jiffies - dev->trans_start);

	
	netif_wake_queue(dev);
}


static int tile_net_ioctl(struct net_device *dev, struct ifreq *rq, int cmd)
{
	return -EOPNOTSUPP;
}


static struct net_device_stats *tile_net_get_stats(struct net_device *dev)
{
	struct tile_net_priv *priv = netdev_priv(dev);
	u32 rx_packets = 0;
	u32 tx_packets = 0;
	u32 rx_bytes = 0;
	u32 tx_bytes = 0;
	int i;

	for_each_online_cpu(i) {
		if (priv->cpu[i]) {
			rx_packets += priv->cpu[i]->stats.rx_packets;
			rx_bytes += priv->cpu[i]->stats.rx_bytes;
			tx_packets += priv->cpu[i]->stats.tx_packets;
			tx_bytes += priv->cpu[i]->stats.tx_bytes;
		}
	}

	priv->stats.rx_packets = rx_packets;
	priv->stats.rx_bytes = rx_bytes;
	priv->stats.tx_packets = tx_packets;
	priv->stats.tx_bytes = tx_bytes;

	return &priv->stats;
}


static int tile_net_change_mtu(struct net_device *dev, int new_mtu)
{
	PDEBUG("tile_net_change_mtu()\n");

	
	if ((new_mtu < 68) || (new_mtu > 1500))
		return -EINVAL;

	
	dev->mtu = new_mtu;

	return 0;
}


static int tile_net_set_mac_address(struct net_device *dev, void *p)
{
	struct sockaddr *addr = p;

	if (!is_valid_ether_addr(addr->sa_data))
		return -EADDRNOTAVAIL;

	
	memcpy(dev->dev_addr, addr->sa_data, dev->addr_len);
	dev->addr_assign_type &= ~NET_ADDR_RANDOM;

	return 0;
}


static int tile_net_get_mac(struct net_device *dev)
{
	struct tile_net_priv *priv = netdev_priv(dev);

	char hv_dev_name[32];
	int len;

	__netio_getset_offset_t offset = { .word = NETIO_IPP_PARAM_OFF };

	int ret;

	
	strcpy(hv_dev_name, dev->name);
	len = strlen(hv_dev_name);

	
	hv_dev_name[len] = hv_dev_name[len - 1];
	hv_dev_name[len - 1] = '/';
	len++;

	
	strcpy(hv_dev_name + len, hash_default ? "/native_hash" : "/native");

	
	priv->hv_devhdl = hv_dev_open((HV_VirtAddr)hv_dev_name, 0);
	PDEBUG("hv_dev_open(%s) returned %d %p\n",
	       hv_dev_name, priv->hv_devhdl, &priv->hv_devhdl);
	if (priv->hv_devhdl < 0) {
		if (priv->hv_devhdl == HV_ENODEV)
			printk(KERN_DEBUG "Ignoring unconfigured device %s\n",
				 hv_dev_name);
		else
			printk(KERN_DEBUG "hv_dev_open(%s) returned %d\n",
				 hv_dev_name, priv->hv_devhdl);
		return -1;
	}

	offset.bits.class = NETIO_PARAM;
	offset.bits.addr = NETIO_PARAM_MAC;
	ret = hv_dev_pread(priv->hv_devhdl, 0,
			   (HV_VirtAddr)dev->dev_addr, dev->addr_len,
			   offset.word);
	PDEBUG("hv_dev_pread(NETIO_PARAM_MAC) returned %d\n", ret);
	if (ret <= 0) {
		printk(KERN_DEBUG "hv_dev_pread(NETIO_PARAM_MAC) %s failed\n",
		       dev->name);
		eth_hw_addr_random(dev);
	}

	return 0;
}


#ifdef CONFIG_NET_POLL_CONTROLLER
static void tile_net_netpoll(struct net_device *dev)
{
	struct tile_net_priv *priv = netdev_priv(dev);
	disable_percpu_irq(priv->intr_id);
	tile_net_handle_ingress_interrupt(priv->intr_id, dev);
	enable_percpu_irq(priv->intr_id, 0);
}
#endif


static const struct net_device_ops tile_net_ops = {
	.ndo_open = tile_net_open,
	.ndo_stop = tile_net_stop,
	.ndo_start_xmit = tile_net_tx,
	.ndo_do_ioctl = tile_net_ioctl,
	.ndo_get_stats = tile_net_get_stats,
	.ndo_change_mtu = tile_net_change_mtu,
	.ndo_tx_timeout = tile_net_tx_timeout,
	.ndo_set_mac_address = tile_net_set_mac_address,
#ifdef CONFIG_NET_POLL_CONTROLLER
	.ndo_poll_controller = tile_net_netpoll,
#endif
};


static void tile_net_setup(struct net_device *dev)
{
	PDEBUG("tile_net_setup()\n");

	ether_setup(dev);

	dev->netdev_ops = &tile_net_ops;

	dev->watchdog_timeo = TILE_NET_TIMEOUT;

	
	dev->features |= NETIF_F_LLTX;

	
	dev->features |= NETIF_F_HW_CSUM;

	
	dev->features |= NETIF_F_SG;

	
	dev->features |= NETIF_F_TSO;

#ifdef TILE_NET_GSO
	
	dev->features |= NETIF_F_GSO;
#endif

	if (hash_default)
		dev->features |= NETIF_F_HIGHDMA;

	

	dev->tx_queue_len = TILE_NET_TX_QUEUE_LEN;

	dev->mtu = TILE_NET_MTU;
}


static struct net_device *tile_net_dev_init(const char *name)
{
	int ret;
	struct net_device *dev;
	struct tile_net_priv *priv;

	dev = alloc_netdev(sizeof(*priv), name, tile_net_setup);
	if (!dev) {
		pr_err("alloc_netdev(%s) failed\n", name);
		return NULL;
	}

	priv = netdev_priv(dev);

	

	memset(priv, 0, sizeof(*priv));

	
	priv->dev = dev;

	INIT_DELAYED_WORK(&priv->retry_work, tile_net_open_retry);

	spin_lock_init(&priv->eq_lock);

	
	priv->eq_pages = alloc_pages(GFP_KERNEL | __GFP_ZERO, EQ_ORDER);
	if (!priv->eq_pages) {
		free_netdev(dev);
		return NULL;
	}
	priv->eq = page_address(priv->eq_pages);

	
	ret = register_netdev(dev);
	if (ret) {
		pr_err("register_netdev %s failed %d\n", dev->name, ret);
		__free_pages(priv->eq_pages, EQ_ORDER);
		free_netdev(dev);
		return NULL;
	}

	
	ret = tile_net_get_mac(dev);
	if (ret < 0) {
		unregister_netdev(dev);
		__free_pages(priv->eq_pages, EQ_ORDER);
		free_netdev(dev);
		return NULL;
	}

	return dev;
}


static void tile_net_cleanup(void)
{
	int i;

	for (i = 0; i < TILE_NET_DEVS; i++) {
		if (tile_net_devs[i]) {
			struct net_device *dev = tile_net_devs[i];
			struct tile_net_priv *priv = netdev_priv(dev);
			unregister_netdev(dev);
			finv_buffer_remote(priv->eq, EQ_SIZE, 0);
			__free_pages(priv->eq_pages, EQ_ORDER);
			free_netdev(dev);
		}
	}
}


static int tile_net_init_module(void)
{
	pr_info("Tilera Network Driver\n");

	tile_net_devs[0] = tile_net_dev_init("xgbe0");
	tile_net_devs[1] = tile_net_dev_init("xgbe1");
	tile_net_devs[2] = tile_net_dev_init("gbe0");
	tile_net_devs[3] = tile_net_dev_init("gbe1");

	return 0;
}


module_init(tile_net_init_module);
module_exit(tile_net_cleanup);


#ifndef MODULE

static int __init network_cpus_setup(char *str)
{
	int rc = cpulist_parse_crop(str, &network_cpus_map);
	if (rc != 0) {
		pr_warning("network_cpus=%s: malformed cpu list\n",
		       str);
	} else {

		
		cpumask_and(&network_cpus_map, &network_cpus_map,
			    cpu_possible_mask);


		if (cpumask_empty(&network_cpus_map)) {
			pr_warning("Ignoring network_cpus='%s'.\n",
			       str);
		} else {
			char buf[1024];
			cpulist_scnprintf(buf, sizeof(buf), &network_cpus_map);
			pr_info("Linux network CPUs: %s\n", buf);
			network_cpus_used = true;
		}
	}

	return 0;
}
__setup("network_cpus=", network_cpus_setup);

#endif
