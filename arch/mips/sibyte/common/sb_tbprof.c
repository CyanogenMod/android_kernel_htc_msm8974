/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * Copyright (C) 2001, 2002, 2003 Broadcom Corporation
 * Copyright (C) 2007 Ralf Baechle <ralf@linux-mips.org>
 * Copyright (C) 2007 MIPS Technologies, Inc.
 *    written by Ralf Baechle <ralf@linux-mips.org>
 */

#undef DEBUG

#include <linux/device.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/vmalloc.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/wait.h>
#include <asm/io.h>
#include <asm/sibyte/sb1250.h>

#if defined(CONFIG_SIBYTE_BCM1x55) || defined(CONFIG_SIBYTE_BCM1x80)
#include <asm/sibyte/bcm1480_regs.h>
#include <asm/sibyte/bcm1480_scd.h>
#include <asm/sibyte/bcm1480_int.h>
#elif defined(CONFIG_SIBYTE_SB1250) || defined(CONFIG_SIBYTE_BCM112X)
#include <asm/sibyte/sb1250_regs.h>
#include <asm/sibyte/sb1250_scd.h>
#include <asm/sibyte/sb1250_int.h>
#else
#error invalid SiByte UART configuration
#endif

#if defined(CONFIG_SIBYTE_BCM1x55) || defined(CONFIG_SIBYTE_BCM1x80)
#undef K_INT_TRACE_FREEZE
#define K_INT_TRACE_FREEZE K_BCM1480_INT_TRACE_FREEZE
#undef K_INT_PERF_CNT
#define K_INT_PERF_CNT K_BCM1480_INT_PERF_CNT
#endif

#include <asm/uaccess.h>

#define SBPROF_TB_MAJOR 240

typedef u64 tb_sample_t[6*256];

enum open_status {
	SB_CLOSED,
	SB_OPENING,
	SB_OPEN
};

struct sbprof_tb {
	wait_queue_head_t	tb_sync;
	wait_queue_head_t	tb_read;
	struct mutex		lock;
	enum open_status	open;
	tb_sample_t		*sbprof_tbbuf;
	int			next_tb_sample;

	volatile int		tb_enable;
	volatile int		tb_armed;

};

static struct sbprof_tb sbp;

#define MAX_SAMPLE_BYTES (24*1024*1024)
#define MAX_TBSAMPLE_BYTES (12*1024*1024)

#define MAX_SAMPLES (MAX_SAMPLE_BYTES/sizeof(u_int32_t))
#define TB_SAMPLE_SIZE (sizeof(tb_sample_t))
#define MAX_TB_SAMPLES (MAX_TBSAMPLE_BYTES/TB_SAMPLE_SIZE)

#define SBPROF_ZBSTART		_IOW('s', 0, int)
#define SBPROF_ZBSTOP		_IOW('s', 1, int)
#define SBPROF_ZBWAITFULL	_IOW('s', 2, int)



#define zclk_timer_init(val) \
  __asm__ __volatile__ (".set push;" \
			".set mips64;" \
			"la   $8, 0xb00204c0;"  \
			"sd   %0, 0x10($8);"    \
			"sd   %1, 0($8);"       \
			".set pop" \
			:  \
						      \
			:  "r"(val), "r" ((1ULL << 33) | 1ULL) \
			:  "$8" )


#define zclk_get(val) \
  __asm__ __volatile__ (".set push;" \
			".set mips64;" \
			"la   $8, 0xb00204c0;"  \
			"ld   %0, 0x10($8);"    \
			".set pop" \
			:  "=r"(val) \
			:  \
			:  "$8" )

#define DEVNAME "sb_tbprof"

#define TB_FULL (sbp.next_tb_sample == MAX_TB_SAMPLES)


static u64 tb_period;

static void arm_tb(void)
{
        u64 scdperfcnt;
	u64 next = (1ULL << 40) - tb_period;
	u64 tb_options = M_SCD_TRACE_CFG_FREEZE_FULL;

	__raw_writeq(0, IOADDR(A_SCD_PERF_CNT_1));
	scdperfcnt = __raw_readq(IOADDR(A_SCD_PERF_CNT_CFG));

#if defined(CONFIG_SIBYTE_BCM1x55) || defined(CONFIG_SIBYTE_BCM1x80)
	__raw_writeq((scdperfcnt & ~M_SPC_CFG_SRC1) |
						
		     V_SPC_CFG_SRC1(1),		
		     IOADDR(A_BCM1480_SCD_PERF_CNT_CFG0));
	__raw_writeq(
		     M_SPC_CFG_ENABLE |		
		     M_SPC_CFG_CLEAR |		
		     V_SPC_CFG_SRC1(1),		
		     IOADDR(A_BCM1480_SCD_PERF_CNT_CFG1));
#else
	__raw_writeq((scdperfcnt & ~M_SPC_CFG_SRC1) |
						
		     M_SPC_CFG_ENABLE |		
		     M_SPC_CFG_CLEAR |		
		     V_SPC_CFG_SRC1(1),		
		     IOADDR(A_SCD_PERF_CNT_CFG));
#endif
	__raw_writeq(next, IOADDR(A_SCD_PERF_CNT_1));
	
	__raw_writeq(M_SCD_TRACE_CFG_RESET, IOADDR(A_SCD_TRACE_CFG));
#if 0 && defined(M_SCD_TRACE_CFG_FORCECNT)
	
	tb_options |= M_SCD_TRACE_CFG_FORCECNT;
#endif
	__raw_writeq(tb_options, IOADDR(A_SCD_TRACE_CFG));
	sbp.tb_armed = 1;
}

static irqreturn_t sbprof_tb_intr(int irq, void *dev_id)
{
	int i;

	pr_debug(DEVNAME ": tb_intr\n");

	if (sbp.next_tb_sample < MAX_TB_SAMPLES) {
		
		u64 *p = sbp.sbprof_tbbuf[sbp.next_tb_sample++];
		
		__raw_writeq(M_SCD_TRACE_CFG_START_READ,
			     IOADDR(A_SCD_TRACE_CFG));
		__asm__ __volatile__ ("sync" : : : "memory");
		
		for (i = 256 * 6; i > 0; i -= 6) {
			
			
			p[i - 1] = __raw_readq(IOADDR(A_SCD_TRACE_READ));
			
			p[i - 2] = __raw_readq(IOADDR(A_SCD_TRACE_READ));
			
			p[i - 3] = __raw_readq(IOADDR(A_SCD_TRACE_READ));
			
			p[i - 4] = __raw_readq(IOADDR(A_SCD_TRACE_READ));
			
			p[i - 5] = __raw_readq(IOADDR(A_SCD_TRACE_READ));
			
			p[i - 6] = __raw_readq(IOADDR(A_SCD_TRACE_READ));
			
		}
		if (!sbp.tb_enable) {
			pr_debug(DEVNAME ": tb_intr shutdown\n");
			__raw_writeq(M_SCD_TRACE_CFG_RESET,
				     IOADDR(A_SCD_TRACE_CFG));
			sbp.tb_armed = 0;
			wake_up_interruptible(&sbp.tb_sync);
		} else {
			
			arm_tb();
		}
	} else {
		
		pr_debug(DEVNAME ": tb_intr full\n");
		__raw_writeq(M_SCD_TRACE_CFG_RESET, IOADDR(A_SCD_TRACE_CFG));
		sbp.tb_armed = 0;
		if (!sbp.tb_enable)
			wake_up_interruptible(&sbp.tb_sync);
		wake_up_interruptible(&sbp.tb_read);
	}
	return IRQ_HANDLED;
}

static irqreturn_t sbprof_pc_intr(int irq, void *dev_id)
{
	printk(DEVNAME ": unexpected pc_intr");
	return IRQ_NONE;
}


static int sbprof_zbprof_start(struct file *filp)
{
	u64 scdperfcnt;
	int err;

	if (xchg(&sbp.tb_enable, 1))
		return -EBUSY;

	pr_debug(DEVNAME ": starting\n");

	sbp.next_tb_sample = 0;
	filp->f_pos = 0;

	err = request_irq(K_INT_TRACE_FREEZE, sbprof_tb_intr, 0,
			  DEVNAME " trace freeze", &sbp);
	if (err)
		return -EBUSY;

	
	scdperfcnt = __raw_readq(IOADDR(A_SCD_PERF_CNT_CFG));
	
	__raw_writeq((scdperfcnt & ~(M_SPC_CFG_SRC1 | M_SPC_CFG_ENABLE)) |
		     M_SPC_CFG_ENABLE | M_SPC_CFG_CLEAR | V_SPC_CFG_SRC1(1),
		     IOADDR(A_SCD_PERF_CNT_CFG));

	if (request_irq(K_INT_PERF_CNT, sbprof_pc_intr, 0, DEVNAME " scd perfcnt", &sbp)) {
		free_irq(K_INT_TRACE_FREEZE, &sbp);
		return -EBUSY;
	}

#if defined(CONFIG_SIBYTE_BCM1x55) || defined(CONFIG_SIBYTE_BCM1x80)
	__raw_writeq(K_BCM1480_INT_MAP_I3,
		     IOADDR(A_BCM1480_IMR_REGISTER(0, R_BCM1480_IMR_INTERRUPT_MAP_BASE_L) +
			    ((K_BCM1480_INT_PERF_CNT & 0x3f) << 3)));
#else
	__raw_writeq(K_INT_MAP_I3,
		     IOADDR(A_IMR_REGISTER(0, R_IMR_INTERRUPT_MAP_BASE) +
			    (K_INT_PERF_CNT << 3)));
#endif

	
	__raw_writeq(0, IOADDR(A_ADDR_TRAP_UP_0));
	__raw_writeq(0, IOADDR(A_ADDR_TRAP_UP_1));
	__raw_writeq(0, IOADDR(A_ADDR_TRAP_UP_2));
	__raw_writeq(0, IOADDR(A_ADDR_TRAP_UP_3));

	__raw_writeq(0, IOADDR(A_ADDR_TRAP_DOWN_0));
	__raw_writeq(0, IOADDR(A_ADDR_TRAP_DOWN_1));
	__raw_writeq(0, IOADDR(A_ADDR_TRAP_DOWN_2));
	__raw_writeq(0, IOADDR(A_ADDR_TRAP_DOWN_3));

	__raw_writeq(0, IOADDR(A_ADDR_TRAP_CFG_0));
	__raw_writeq(0, IOADDR(A_ADDR_TRAP_CFG_1));
	__raw_writeq(0, IOADDR(A_ADDR_TRAP_CFG_2));
	__raw_writeq(0, IOADDR(A_ADDR_TRAP_CFG_3));

	
	
	__raw_writeq(M_SCD_TREVT_INTERRUPT, IOADDR(A_SCD_TRACE_EVENT_0));
	__raw_writeq(0, IOADDR(A_SCD_TRACE_EVENT_1));
	__raw_writeq(0, IOADDR(A_SCD_TRACE_EVENT_2));
	__raw_writeq(0, IOADDR(A_SCD_TRACE_EVENT_3));
	__raw_writeq(0, IOADDR(A_SCD_TRACE_EVENT_4));
	__raw_writeq(0, IOADDR(A_SCD_TRACE_EVENT_5));
	__raw_writeq(0, IOADDR(A_SCD_TRACE_EVENT_6));
	__raw_writeq(0, IOADDR(A_SCD_TRACE_EVENT_7));

	
	
	__raw_writeq(V_SCD_TRSEQ_FUNC_START | 0x0fff,
		     IOADDR(A_SCD_TRACE_SEQUENCE_0));
	
	__raw_writeq(M_SCD_TRSEQ_ASAMPLE | M_SCD_TRSEQ_DSAMPLE |
		     K_SCD_TRSEQ_TRIGGER_ALL,
		     IOADDR(A_SCD_TRACE_SEQUENCE_1));
	__raw_writeq(0, IOADDR(A_SCD_TRACE_SEQUENCE_2));
	__raw_writeq(0, IOADDR(A_SCD_TRACE_SEQUENCE_3));
	__raw_writeq(0, IOADDR(A_SCD_TRACE_SEQUENCE_4));
	__raw_writeq(0, IOADDR(A_SCD_TRACE_SEQUENCE_5));
	__raw_writeq(0, IOADDR(A_SCD_TRACE_SEQUENCE_6));
	__raw_writeq(0, IOADDR(A_SCD_TRACE_SEQUENCE_7));

	
#if defined(CONFIG_SIBYTE_BCM1x55) || defined(CONFIG_SIBYTE_BCM1x80)
	__raw_writeq(1ULL << (K_BCM1480_INT_PERF_CNT & 0x3f),
		     IOADDR(A_BCM1480_IMR_REGISTER(0, R_BCM1480_IMR_INTERRUPT_TRACE_L)));
#else
	__raw_writeq(1ULL << K_INT_PERF_CNT,
		     IOADDR(A_IMR_REGISTER(0, R_IMR_INTERRUPT_TRACE)));
#endif
	arm_tb();

	pr_debug(DEVNAME ": done starting\n");

	return 0;
}

static int sbprof_zbprof_stop(void)
{
	int err = 0;

	pr_debug(DEVNAME ": stopping\n");

	if (sbp.tb_enable) {
		pr_debug(DEVNAME ": wait for disarm\n");
		err = wait_event_interruptible(sbp.tb_sync, !sbp.tb_armed);
		pr_debug(DEVNAME ": disarm complete, stat %d\n", err);

		if (err)
			return err;

		sbp.tb_enable = 0;
		free_irq(K_INT_TRACE_FREEZE, &sbp);
		free_irq(K_INT_PERF_CNT, &sbp);
	}

	pr_debug(DEVNAME ": done stopping\n");

	return err;
}

static int sbprof_tb_open(struct inode *inode, struct file *filp)
{
	int minor;

	minor = iminor(inode);
	if (minor != 0)
		return -ENODEV;

	if (xchg(&sbp.open, SB_OPENING) != SB_CLOSED)
		return -EBUSY;

	memset(&sbp, 0, sizeof(struct sbprof_tb));
	sbp.sbprof_tbbuf = vzalloc(MAX_TBSAMPLE_BYTES);
	if (!sbp.sbprof_tbbuf) {
		sbp.open = SB_CLOSED;
		wmb();
		return -ENOMEM;
	}

	init_waitqueue_head(&sbp.tb_sync);
	init_waitqueue_head(&sbp.tb_read);
	mutex_init(&sbp.lock);

	sbp.open = SB_OPEN;
	wmb();

	return 0;
}

static int sbprof_tb_release(struct inode *inode, struct file *filp)
{
	int minor;

	minor = iminor(inode);
	if (minor != 0 || sbp.open != SB_CLOSED)
		return -ENODEV;

	mutex_lock(&sbp.lock);

	if (sbp.tb_armed || sbp.tb_enable)
		sbprof_zbprof_stop();

	vfree(sbp.sbprof_tbbuf);
	sbp.open = SB_CLOSED;
	wmb();

	mutex_unlock(&sbp.lock);

	return 0;
}

static ssize_t sbprof_tb_read(struct file *filp, char *buf,
			      size_t size, loff_t *offp)
{
	int cur_sample, sample_off, cur_count, sample_left;
	char *src;
	int   count   =	 0;
	char *dest    =	 buf;
	long  cur_off = *offp;

	if (!access_ok(VERIFY_WRITE, buf, size))
		return -EFAULT;

	mutex_lock(&sbp.lock);

	count = 0;
	cur_sample = cur_off / TB_SAMPLE_SIZE;
	sample_off = cur_off % TB_SAMPLE_SIZE;
	sample_left = TB_SAMPLE_SIZE - sample_off;

	while (size && (cur_sample < sbp.next_tb_sample)) {
		int err;

		cur_count = size < sample_left ? size : sample_left;
		src = (char *)(((long)sbp.sbprof_tbbuf[cur_sample])+sample_off);
		err = __copy_to_user(dest, src, cur_count);
		if (err) {
			*offp = cur_off + cur_count - err;
			mutex_unlock(&sbp.lock);
			return err;
		}
		pr_debug(DEVNAME ": read from sample %d, %d bytes\n",
		         cur_sample, cur_count);
		size -= cur_count;
		sample_left -= cur_count;
		if (!sample_left) {
			cur_sample++;
			sample_off = 0;
			sample_left = TB_SAMPLE_SIZE;
		} else {
			sample_off += cur_count;
		}
		cur_off += cur_count;
		dest += cur_count;
		count += cur_count;
	}
	*offp = cur_off;
	mutex_unlock(&sbp.lock);

	return count;
}

static long sbprof_tb_ioctl(struct file *filp,
			    unsigned int command,
			    unsigned long arg)
{
	int err = 0;

	switch (command) {
	case SBPROF_ZBSTART:
		mutex_lock(&sbp.lock);
		err = sbprof_zbprof_start(filp);
		mutex_unlock(&sbp.lock);
		break;

	case SBPROF_ZBSTOP:
		mutex_lock(&sbp.lock);
		err = sbprof_zbprof_stop();
		mutex_unlock(&sbp.lock);
		break;

	case SBPROF_ZBWAITFULL: {
		err = wait_event_interruptible(sbp.tb_read, TB_FULL);
		if (err)
			break;

		err = put_user(TB_FULL, (int *) arg);
		break;
	}

	default:
		err = -EINVAL;
		break;
	}

	return err;
}

static const struct file_operations sbprof_tb_fops = {
	.owner		= THIS_MODULE,
	.open		= sbprof_tb_open,
	.release	= sbprof_tb_release,
	.read		= sbprof_tb_read,
	.unlocked_ioctl	= sbprof_tb_ioctl,
	.compat_ioctl	= sbprof_tb_ioctl,
	.mmap		= NULL,
	.llseek		= default_llseek,
};

static struct class *tb_class;
static struct device *tb_dev;

static int __init sbprof_tb_init(void)
{
	struct device *dev;
	struct class *tbc;
	int err;

	if (register_chrdev(SBPROF_TB_MAJOR, DEVNAME, &sbprof_tb_fops)) {
		printk(KERN_WARNING DEVNAME ": initialization failed (dev %d)\n",
		       SBPROF_TB_MAJOR);
		return -EIO;
	}

	tbc = class_create(THIS_MODULE, "sb_tracebuffer");
	if (IS_ERR(tbc)) {
		err = PTR_ERR(tbc);
		goto out_chrdev;
	}

	tb_class = tbc;

	dev = device_create(tbc, NULL, MKDEV(SBPROF_TB_MAJOR, 0), NULL, "tb");
	if (IS_ERR(dev)) {
		err = PTR_ERR(dev);
		goto out_class;
	}
	tb_dev = dev;

	sbp.open = SB_CLOSED;
	wmb();
	tb_period = zbbus_mhz * 10000LL;
	pr_info(DEVNAME ": initialized - tb_period = %lld\n",
		(long long) tb_period);
	return 0;

out_class:
	class_destroy(tb_class);
out_chrdev:
	unregister_chrdev(SBPROF_TB_MAJOR, DEVNAME);

	return err;
}

static void __exit sbprof_tb_cleanup(void)
{
	device_destroy(tb_class, MKDEV(SBPROF_TB_MAJOR, 0));
	unregister_chrdev(SBPROF_TB_MAJOR, DEVNAME);
	class_destroy(tb_class);
}

module_init(sbprof_tb_init);
module_exit(sbprof_tb_cleanup);

MODULE_ALIAS_CHARDEV_MAJOR(SBPROF_TB_MAJOR);
MODULE_AUTHOR("Ralf Baechle <ralf@linux-mips.org>");
MODULE_LICENSE("GPL");
