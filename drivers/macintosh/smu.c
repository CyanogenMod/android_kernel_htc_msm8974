/*
 * PowerMac G5 SMU driver
 *
 * Copyright 2004 J. Mayer <l_indien@magic.fr>
 * Copyright 2005 Benjamin Herrenschmidt, IBM Corp.
 *
 * Released under the term of the GNU GPL v2.
 */


#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/dmapool.h>
#include <linux/bootmem.h>
#include <linux/vmalloc.h>
#include <linux/highmem.h>
#include <linux/jiffies.h>
#include <linux/interrupt.h>
#include <linux/rtc.h>
#include <linux/completion.h>
#include <linux/miscdevice.h>
#include <linux/delay.h>
#include <linux/poll.h>
#include <linux/mutex.h>
#include <linux/of_device.h>
#include <linux/of_platform.h>
#include <linux/slab.h>

#include <asm/byteorder.h>
#include <asm/io.h>
#include <asm/prom.h>
#include <asm/machdep.h>
#include <asm/pmac_feature.h>
#include <asm/smu.h>
#include <asm/sections.h>
#include <asm/abs_addr.h>
#include <asm/uaccess.h>

#define VERSION "0.7"
#define AUTHOR  "(c) 2005 Benjamin Herrenschmidt, IBM Corp."

#undef DEBUG_SMU

#ifdef DEBUG_SMU
#define DPRINTK(fmt, args...) do { printk(KERN_DEBUG fmt , ##args); } while (0)
#else
#define DPRINTK(fmt, args...) do { } while (0)
#endif

#define SMU_MAX_DATA	254

struct smu_cmd_buf {
	u8 cmd;
	u8 length;
	u8 data[SMU_MAX_DATA];
};

struct smu_device {
	spinlock_t		lock;
	struct device_node	*of_node;
	struct platform_device	*of_dev;
	int			doorbell;	
	u32 __iomem		*db_buf;	
	struct device_node	*db_node;
	unsigned int		db_irq;
	int			msg;
	struct device_node	*msg_node;
	unsigned int		msg_irq;
	struct smu_cmd_buf	*cmd_buf;	
	u32			cmd_buf_abs;	
	struct list_head	cmd_list;
	struct smu_cmd		*cmd_cur;	
	int			broken_nap;
	struct list_head	cmd_i2c_list;
	struct smu_i2c_cmd	*cmd_i2c_cur;	
	struct timer_list	i2c_timer;
};

static DEFINE_MUTEX(smu_mutex);
static struct smu_device	*smu;
static DEFINE_MUTEX(smu_part_access);
static int smu_irq_inited;

static void smu_i2c_retry(unsigned long data);


static void smu_start_cmd(void)
{
	unsigned long faddr, fend;
	struct smu_cmd *cmd;

	if (list_empty(&smu->cmd_list))
		return;

	
	cmd = list_entry(smu->cmd_list.next, struct smu_cmd, link);
	smu->cmd_cur = cmd;
	list_del(&cmd->link);

	DPRINTK("SMU: starting cmd %x, %d bytes data\n", cmd->cmd,
		cmd->data_len);
	DPRINTK("SMU: data buffer: %02x %02x %02x %02x %02x %02x %02x %02x\n",
		((u8 *)cmd->data_buf)[0], ((u8 *)cmd->data_buf)[1],
		((u8 *)cmd->data_buf)[2], ((u8 *)cmd->data_buf)[3],
		((u8 *)cmd->data_buf)[4], ((u8 *)cmd->data_buf)[5],
		((u8 *)cmd->data_buf)[6], ((u8 *)cmd->data_buf)[7]);

	
	smu->cmd_buf->cmd = cmd->cmd;
	smu->cmd_buf->length = cmd->data_len;
	memcpy(smu->cmd_buf->data, cmd->data_buf, cmd->data_len);

	
	faddr = (unsigned long)smu->cmd_buf;
	fend = faddr + smu->cmd_buf->length + 2;
	flush_inval_dcache_range(faddr, fend);


	/* We also disable NAP mode for the duration of the command
	 * on U3 based machines.
	 * This is slightly racy as it can be written back to 1 by a sysctl
	 * but that never happens in practice. There seem to be an issue with
	 * U3 based machines such as the iMac G5 where napping for the
	 * whole duration of the command prevents the SMU from fetching it
	 * from memory. This might be related to the strange i2c based
	 * mechanism the SMU uses to access memory.
	 */
	if (smu->broken_nap)
		powersave_nap = 0;

	writel(smu->cmd_buf_abs, smu->db_buf);

	
	pmac_do_feature_call(PMAC_FTR_WRITE_GPIO, NULL, smu->doorbell, 4);
}


static irqreturn_t smu_db_intr(int irq, void *arg)
{
	unsigned long flags;
	struct smu_cmd *cmd;
	void (*done)(struct smu_cmd *cmd, void *misc) = NULL;
	void *misc = NULL;
	u8 gpio;
	int rc = 0;

	spin_lock_irqsave(&smu->lock, flags);

	gpio = pmac_do_feature_call(PMAC_FTR_READ_GPIO, NULL, smu->doorbell);
	if ((gpio & 7) != 7) {
		spin_unlock_irqrestore(&smu->lock, flags);
		return IRQ_HANDLED;
	}

	cmd = smu->cmd_cur;
	smu->cmd_cur = NULL;
	if (cmd == NULL)
		goto bail;

	if (rc == 0) {
		unsigned long faddr;
		int reply_len;
		u8 ack;

		faddr = (unsigned long)smu->cmd_buf;
		flush_inval_dcache_range(faddr, faddr + 256);

		
		ack = (~cmd->cmd) & 0xff;
		if (ack != smu->cmd_buf->cmd) {
			DPRINTK("SMU: incorrect ack, want %x got %x\n",
				ack, smu->cmd_buf->cmd);
			rc = -EIO;
		}
		reply_len = rc == 0 ? smu->cmd_buf->length : 0;
		DPRINTK("SMU: reply len: %d\n", reply_len);
		if (reply_len > cmd->reply_len) {
			printk(KERN_WARNING "SMU: reply buffer too small,"
			       "got %d bytes for a %d bytes buffer\n",
			       reply_len, cmd->reply_len);
			reply_len = cmd->reply_len;
		}
		cmd->reply_len = reply_len;
		if (cmd->reply_buf && reply_len)
			memcpy(cmd->reply_buf, smu->cmd_buf->data, reply_len);
	}

	done = cmd->done;
	misc = cmd->misc;
	mb();
	cmd->status = rc;

	
	if (smu->broken_nap)
		powersave_nap = 1;
 bail:
	
	smu_start_cmd();
	spin_unlock_irqrestore(&smu->lock, flags);

	
	if (done)
		done(cmd, misc);

	
	return IRQ_HANDLED;
}


static irqreturn_t smu_msg_intr(int irq, void *arg)
{

	printk(KERN_INFO "SMU: message interrupt !\n");

	
	return IRQ_HANDLED;
}



int smu_queue_cmd(struct smu_cmd *cmd)
{
	unsigned long flags;

	if (smu == NULL)
		return -ENODEV;
	if (cmd->data_len > SMU_MAX_DATA ||
	    cmd->reply_len > SMU_MAX_DATA)
		return -EINVAL;

	cmd->status = 1;
	spin_lock_irqsave(&smu->lock, flags);
	list_add_tail(&cmd->link, &smu->cmd_list);
	if (smu->cmd_cur == NULL)
		smu_start_cmd();
	spin_unlock_irqrestore(&smu->lock, flags);

	
	if (!smu_irq_inited || smu->db_irq == NO_IRQ)
		smu_spinwait_cmd(cmd);

	return 0;
}
EXPORT_SYMBOL(smu_queue_cmd);


int smu_queue_simple(struct smu_simple_cmd *scmd, u8 command,
		     unsigned int data_len,
		     void (*done)(struct smu_cmd *cmd, void *misc),
		     void *misc, ...)
{
	struct smu_cmd *cmd = &scmd->cmd;
	va_list list;
	int i;

	if (data_len > sizeof(scmd->buffer))
		return -EINVAL;

	memset(scmd, 0, sizeof(*scmd));
	cmd->cmd = command;
	cmd->data_len = data_len;
	cmd->data_buf = scmd->buffer;
	cmd->reply_len = sizeof(scmd->buffer);
	cmd->reply_buf = scmd->buffer;
	cmd->done = done;
	cmd->misc = misc;

	va_start(list, misc);
	for (i = 0; i < data_len; ++i)
		scmd->buffer[i] = (u8)va_arg(list, int);
	va_end(list);

	return smu_queue_cmd(cmd);
}
EXPORT_SYMBOL(smu_queue_simple);


void smu_poll(void)
{
	u8 gpio;

	if (smu == NULL)
		return;

	gpio = pmac_do_feature_call(PMAC_FTR_READ_GPIO, NULL, smu->doorbell);
	if ((gpio & 7) == 7)
		smu_db_intr(smu->db_irq, smu);
}
EXPORT_SYMBOL(smu_poll);


void smu_done_complete(struct smu_cmd *cmd, void *misc)
{
	struct completion *comp = misc;

	complete(comp);
}
EXPORT_SYMBOL(smu_done_complete);


void smu_spinwait_cmd(struct smu_cmd *cmd)
{
	while(cmd->status == 1)
		smu_poll();
}
EXPORT_SYMBOL(smu_spinwait_cmd);


static inline int bcd2hex (int n)
{
	return (((n & 0xf0) >> 4) * 10) + (n & 0xf);
}


static inline int hex2bcd (int n)
{
	return ((n / 10) << 4) + (n % 10);
}


static inline void smu_fill_set_rtc_cmd(struct smu_cmd_buf *cmd_buf,
					struct rtc_time *time)
{
	cmd_buf->cmd = 0x8e;
	cmd_buf->length = 8;
	cmd_buf->data[0] = 0x80;
	cmd_buf->data[1] = hex2bcd(time->tm_sec);
	cmd_buf->data[2] = hex2bcd(time->tm_min);
	cmd_buf->data[3] = hex2bcd(time->tm_hour);
	cmd_buf->data[4] = time->tm_wday;
	cmd_buf->data[5] = hex2bcd(time->tm_mday);
	cmd_buf->data[6] = hex2bcd(time->tm_mon) + 1;
	cmd_buf->data[7] = hex2bcd(time->tm_year - 100);
}


int smu_get_rtc_time(struct rtc_time *time, int spinwait)
{
	struct smu_simple_cmd cmd;
	int rc;

	if (smu == NULL)
		return -ENODEV;

	memset(time, 0, sizeof(struct rtc_time));
	rc = smu_queue_simple(&cmd, SMU_CMD_RTC_COMMAND, 1, NULL, NULL,
			      SMU_CMD_RTC_GET_DATETIME);
	if (rc)
		return rc;
	smu_spinwait_simple(&cmd);

	time->tm_sec = bcd2hex(cmd.buffer[0]);
	time->tm_min = bcd2hex(cmd.buffer[1]);
	time->tm_hour = bcd2hex(cmd.buffer[2]);
	time->tm_wday = bcd2hex(cmd.buffer[3]);
	time->tm_mday = bcd2hex(cmd.buffer[4]);
	time->tm_mon = bcd2hex(cmd.buffer[5]) - 1;
	time->tm_year = bcd2hex(cmd.buffer[6]) + 100;

	return 0;
}


int smu_set_rtc_time(struct rtc_time *time, int spinwait)
{
	struct smu_simple_cmd cmd;
	int rc;

	if (smu == NULL)
		return -ENODEV;

	rc = smu_queue_simple(&cmd, SMU_CMD_RTC_COMMAND, 8, NULL, NULL,
			      SMU_CMD_RTC_SET_DATETIME,
			      hex2bcd(time->tm_sec),
			      hex2bcd(time->tm_min),
			      hex2bcd(time->tm_hour),
			      time->tm_wday,
			      hex2bcd(time->tm_mday),
			      hex2bcd(time->tm_mon) + 1,
			      hex2bcd(time->tm_year - 100));
	if (rc)
		return rc;
	smu_spinwait_simple(&cmd);

	return 0;
}


void smu_shutdown(void)
{
	struct smu_simple_cmd cmd;

	if (smu == NULL)
		return;

	if (smu_queue_simple(&cmd, SMU_CMD_POWER_COMMAND, 9, NULL, NULL,
			     'S', 'H', 'U', 'T', 'D', 'O', 'W', 'N', 0))
		return;
	smu_spinwait_simple(&cmd);
	for (;;)
		;
}


void smu_restart(void)
{
	struct smu_simple_cmd cmd;

	if (smu == NULL)
		return;

	if (smu_queue_simple(&cmd, SMU_CMD_POWER_COMMAND, 8, NULL, NULL,
			     'R', 'E', 'S', 'T', 'A', 'R', 'T', 0))
		return;
	smu_spinwait_simple(&cmd);
	for (;;)
		;
}


int smu_present(void)
{
	return smu != NULL;
}
EXPORT_SYMBOL(smu_present);


int __init smu_init (void)
{
	struct device_node *np;
	const u32 *data;
	int ret = 0;

        np = of_find_node_by_type(NULL, "smu");
        if (np == NULL)
		return -ENODEV;

	printk(KERN_INFO "SMU: Driver %s %s\n", VERSION, AUTHOR);

	if (smu_cmdbuf_abs == 0) {
		printk(KERN_ERR "SMU: Command buffer not allocated !\n");
		ret = -EINVAL;
		goto fail_np;
	}

	smu = alloc_bootmem(sizeof(struct smu_device));

	spin_lock_init(&smu->lock);
	INIT_LIST_HEAD(&smu->cmd_list);
	INIT_LIST_HEAD(&smu->cmd_i2c_list);
	smu->of_node = np;
	smu->db_irq = NO_IRQ;
	smu->msg_irq = NO_IRQ;

	smu->cmd_buf_abs = (u32)smu_cmdbuf_abs;
	smu->cmd_buf = (struct smu_cmd_buf *)abs_to_virt(smu_cmdbuf_abs);

	smu->db_node = of_find_node_by_name(NULL, "smu-doorbell");
	if (smu->db_node == NULL) {
		printk(KERN_ERR "SMU: Can't find doorbell GPIO !\n");
		ret = -ENXIO;
		goto fail_bootmem;
	}
	data = of_get_property(smu->db_node, "reg", NULL);
	if (data == NULL) {
		printk(KERN_ERR "SMU: Can't find doorbell GPIO address !\n");
		ret = -ENXIO;
		goto fail_db_node;
	}

	smu->doorbell = *data;
	if (smu->doorbell < 0x50)
		smu->doorbell += 0x50;

	
	do {
		smu->msg_node = of_find_node_by_name(NULL, "smu-interrupt");
		if (smu->msg_node == NULL)
			break;
		data = of_get_property(smu->msg_node, "reg", NULL);
		if (data == NULL) {
			of_node_put(smu->msg_node);
			smu->msg_node = NULL;
			break;
		}
		smu->msg = *data;
		if (smu->msg < 0x50)
			smu->msg += 0x50;
	} while(0);

	smu->db_buf = ioremap(0x8000860c, 0x1000);
	if (smu->db_buf == NULL) {
		printk(KERN_ERR "SMU: Can't map doorbell buffer pointer !\n");
		ret = -ENXIO;
		goto fail_msg_node;
	}

	
	smu->broken_nap = pmac_get_uninorth_variant() < 4;
	if (smu->broken_nap)
		printk(KERN_INFO "SMU: using NAP mode workaround\n");

	sys_ctrler = SYS_CTRLER_SMU;
	return 0;

fail_msg_node:
	if (smu->msg_node)
		of_node_put(smu->msg_node);
fail_db_node:
	of_node_put(smu->db_node);
fail_bootmem:
	free_bootmem((unsigned long)smu, sizeof(struct smu_device));
	smu = NULL;
fail_np:
	of_node_put(np);
	return ret;
}


static int smu_late_init(void)
{
	if (!smu)
		return 0;

	init_timer(&smu->i2c_timer);
	smu->i2c_timer.function = smu_i2c_retry;
	smu->i2c_timer.data = (unsigned long)smu;

	if (smu->db_node) {
		smu->db_irq = irq_of_parse_and_map(smu->db_node, 0);
		if (smu->db_irq == NO_IRQ)
			printk(KERN_ERR "smu: failed to map irq for node %s\n",
			       smu->db_node->full_name);
	}
	if (smu->msg_node) {
		smu->msg_irq = irq_of_parse_and_map(smu->msg_node, 0);
		if (smu->msg_irq == NO_IRQ)
			printk(KERN_ERR "smu: failed to map irq for node %s\n",
			       smu->msg_node->full_name);
	}


	if (smu->db_irq != NO_IRQ) {
		if (request_irq(smu->db_irq, smu_db_intr,
				IRQF_SHARED, "SMU doorbell", smu) < 0) {
			printk(KERN_WARNING "SMU: can't "
			       "request interrupt %d\n",
			       smu->db_irq);
			smu->db_irq = NO_IRQ;
		}
	}

	if (smu->msg_irq != NO_IRQ) {
		if (request_irq(smu->msg_irq, smu_msg_intr,
				IRQF_SHARED, "SMU message", smu) < 0) {
			printk(KERN_WARNING "SMU: can't "
			       "request interrupt %d\n",
			       smu->msg_irq);
			smu->msg_irq = NO_IRQ;
		}
	}

	smu_irq_inited = 1;
	return 0;
}
core_initcall(smu_late_init);


static void smu_expose_childs(struct work_struct *unused)
{
	struct device_node *np;

	for (np = NULL; (np = of_get_next_child(smu->of_node, np)) != NULL;)
		if (of_device_is_compatible(np, "smu-sensors"))
			of_platform_device_create(np, "smu-sensors",
						  &smu->of_dev->dev);
}

static DECLARE_WORK(smu_expose_childs_work, smu_expose_childs);

static int smu_platform_probe(struct platform_device* dev)
{
	if (!smu)
		return -ENODEV;
	smu->of_dev = dev;

	schedule_work(&smu_expose_childs_work);

	return 0;
}

static const struct of_device_id smu_platform_match[] =
{
	{
		.type		= "smu",
	},
	{},
};

static struct platform_driver smu_of_platform_driver =
{
	.driver = {
		.name = "smu",
		.owner = THIS_MODULE,
		.of_match_table = smu_platform_match,
	},
	.probe		= smu_platform_probe,
};

static int __init smu_init_sysfs(void)
{
	platform_driver_register(&smu_of_platform_driver);
	return 0;
}

device_initcall(smu_init_sysfs);

struct platform_device *smu_get_ofdev(void)
{
	if (!smu)
		return NULL;
	return smu->of_dev;
}

EXPORT_SYMBOL_GPL(smu_get_ofdev);


static void smu_i2c_complete_command(struct smu_i2c_cmd *cmd, int fail)
{
	void (*done)(struct smu_i2c_cmd *cmd, void *misc) = cmd->done;
	void *misc = cmd->misc;
	unsigned long flags;

	
	if (!fail && cmd->read) {
		if (cmd->pdata[0] < 1)
			fail = 1;
		else
			memcpy(cmd->info.data, &cmd->pdata[1],
			       cmd->info.datalen);
	}

	DPRINTK("SMU: completing, success: %d\n", !fail);

	spin_lock_irqsave(&smu->lock, flags);
	smu->cmd_i2c_cur = NULL;
	wmb();
	cmd->status = fail ? -EIO : 0;

	
	if (!list_empty(&smu->cmd_i2c_list)) {
		struct smu_i2c_cmd *newcmd;

		
		newcmd = list_entry(smu->cmd_i2c_list.next,
				    struct smu_i2c_cmd, link);
		smu->cmd_i2c_cur = newcmd;
		list_del(&cmd->link);

		
		list_add_tail(&cmd->scmd.link, &smu->cmd_list);
		if (smu->cmd_cur == NULL)
			smu_start_cmd();
	}
	spin_unlock_irqrestore(&smu->lock, flags);

	
	if (done)
		done(cmd, misc);

}


static void smu_i2c_retry(unsigned long data)
{
	struct smu_i2c_cmd	*cmd = smu->cmd_i2c_cur;

	DPRINTK("SMU: i2c failure, requeuing...\n");

	
	cmd->pdata[0] = 0xff;
	cmd->scmd.reply_len = sizeof(cmd->pdata);
	smu_queue_cmd(&cmd->scmd);
}


static void smu_i2c_low_completion(struct smu_cmd *scmd, void *misc)
{
	struct smu_i2c_cmd	*cmd = misc;
	int			fail = 0;

	DPRINTK("SMU: i2c compl. stage=%d status=%x pdata[0]=%x rlen: %x\n",
		cmd->stage, scmd->status, cmd->pdata[0], scmd->reply_len);

	
	if (scmd->status < 0)
		fail = 1;
	else if (cmd->read) {
		if (cmd->stage == 0)
			fail = cmd->pdata[0] != 0;
		else
			fail = cmd->pdata[0] >= 0x80;
	} else {
		fail = cmd->pdata[0] != 0;
	}

	if (fail && --cmd->retries > 0) {
		DPRINTK("SMU: i2c failure, starting timer...\n");
		BUG_ON(cmd != smu->cmd_i2c_cur);
		if (!smu_irq_inited) {
			mdelay(5);
			smu_i2c_retry(0);
			return;
		}
		mod_timer(&smu->i2c_timer, jiffies + msecs_to_jiffies(5));
		return;
	}

	
	if (fail || cmd->stage != 0) {
		smu_i2c_complete_command(cmd, fail);
		return;
	}

	DPRINTK("SMU: going to stage 1\n");

	
	scmd->reply_buf = cmd->pdata;
	scmd->reply_len = sizeof(cmd->pdata);
	scmd->data_buf = cmd->pdata;
	scmd->data_len = 1;
	cmd->pdata[0] = 0;
	cmd->stage = 1;
	cmd->retries = 20;
	smu_queue_cmd(scmd);
}


int smu_queue_i2c(struct smu_i2c_cmd *cmd)
{
	unsigned long flags;

	if (smu == NULL)
		return -ENODEV;

	
	cmd->scmd.cmd = SMU_CMD_I2C_COMMAND;
	cmd->scmd.done = smu_i2c_low_completion;
	cmd->scmd.misc = cmd;
	cmd->scmd.reply_buf = cmd->pdata;
	cmd->scmd.reply_len = sizeof(cmd->pdata);
	cmd->scmd.data_buf = (u8 *)(char *)&cmd->info;
	cmd->scmd.status = 1;
	cmd->stage = 0;
	cmd->pdata[0] = 0xff;
	cmd->retries = 20;
	cmd->status = 1;

	cmd->info.caddr = cmd->info.devaddr;
	cmd->read = cmd->info.devaddr & 0x01;
	switch(cmd->info.type) {
	case SMU_I2C_TRANSFER_SIMPLE:
		memset(&cmd->info.sublen, 0, 4);
		break;
	case SMU_I2C_TRANSFER_COMBINED:
		cmd->info.devaddr &= 0xfe;
	case SMU_I2C_TRANSFER_STDSUB:
		if (cmd->info.sublen > 3)
			return -EINVAL;
		break;
	default:
		return -EINVAL;
	}

	if (cmd->read) {
		if (cmd->info.datalen > SMU_I2C_READ_MAX)
			return -EINVAL;
		memset(cmd->info.data, 0xff, cmd->info.datalen);
		cmd->scmd.data_len = 9;
	} else {
		if (cmd->info.datalen > SMU_I2C_WRITE_MAX)
			return -EINVAL;
		cmd->scmd.data_len = 9 + cmd->info.datalen;
	}

	DPRINTK("SMU: i2c enqueuing command\n");
	DPRINTK("SMU:   %s, len=%d bus=%x addr=%x sub0=%x type=%x\n",
		cmd->read ? "read" : "write", cmd->info.datalen,
		cmd->info.bus, cmd->info.caddr,
		cmd->info.subaddr[0], cmd->info.type);


	spin_lock_irqsave(&smu->lock, flags);
	if (smu->cmd_i2c_cur == NULL) {
		smu->cmd_i2c_cur = cmd;
		list_add_tail(&cmd->scmd.link, &smu->cmd_list);
		if (smu->cmd_cur == NULL)
			smu_start_cmd();
	} else
		list_add_tail(&cmd->link, &smu->cmd_i2c_list);
	spin_unlock_irqrestore(&smu->lock, flags);

	return 0;
}


static int smu_read_datablock(u8 *dest, unsigned int addr, unsigned int len)
{
	DECLARE_COMPLETION_ONSTACK(comp);
	unsigned int chunk;
	struct smu_cmd cmd;
	int rc;
	u8 params[8];

	chunk = 0xe;

	while (len) {
		unsigned int clen = min(len, chunk);

		cmd.cmd = SMU_CMD_MISC_ee_COMMAND;
		cmd.data_len = 7;
		cmd.data_buf = params;
		cmd.reply_len = chunk;
		cmd.reply_buf = dest;
		cmd.done = smu_done_complete;
		cmd.misc = &comp;
		params[0] = SMU_CMD_MISC_ee_GET_DATABLOCK_REC;
		params[1] = 0x4;
		*((u32 *)&params[2]) = addr;
		params[6] = clen;

		rc = smu_queue_cmd(&cmd);
		if (rc)
			return rc;
		wait_for_completion(&comp);
		if (cmd.status != 0)
			return rc;
		if (cmd.reply_len != clen) {
			printk(KERN_DEBUG "SMU: short read in "
			       "smu_read_datablock, got: %d, want: %d\n",
			       cmd.reply_len, clen);
			return -EIO;
		}
		len -= clen;
		addr += clen;
		dest += clen;
	}
	return 0;
}

static struct smu_sdbp_header *smu_create_sdb_partition(int id)
{
	DECLARE_COMPLETION_ONSTACK(comp);
	struct smu_simple_cmd cmd;
	unsigned int addr, len, tlen;
	struct smu_sdbp_header *hdr;
	struct property *prop;

	
	DPRINTK("SMU: Query partition infos ... (irq=%d)\n", smu->db_irq);
	smu_queue_simple(&cmd, SMU_CMD_PARTITION_COMMAND, 2,
			 smu_done_complete, &comp,
			 SMU_CMD_PARTITION_LATEST, id);
	wait_for_completion(&comp);
	DPRINTK("SMU: done, status: %d, reply_len: %d\n",
		cmd.cmd.status, cmd.cmd.reply_len);

	
	if (cmd.cmd.status != 0 || cmd.cmd.reply_len != 6)
		return NULL;

	
	addr = *((u16 *)cmd.buffer);
	len = cmd.buffer[3] << 2;
	tlen = sizeof(struct property) + len + 18;

	prop = kzalloc(tlen, GFP_KERNEL);
	if (prop == NULL)
		return NULL;
	hdr = (struct smu_sdbp_header *)(prop + 1);
	prop->name = ((char *)prop) + tlen - 18;
	sprintf(prop->name, "sdb-partition-%02x", id);
	prop->length = len;
	prop->value = hdr;
	prop->next = NULL;

	
	if (smu_read_datablock((u8 *)hdr, addr, len)) {
		printk(KERN_DEBUG "SMU: datablock read failed while reading "
		       "partition %02x !\n", id);
		goto failure;
	}

	
	if (hdr->id != id) {
		printk(KERN_DEBUG "SMU: Reading partition %02x and got "
		       "%02x !\n", id, hdr->id);
		goto failure;
	}
	if (prom_add_property(smu->of_node, prop)) {
		printk(KERN_DEBUG "SMU: Failed creating sdb-partition-%02x "
		       "property !\n", id);
		goto failure;
	}

	return hdr;
 failure:
	kfree(prop);
	return NULL;
}

const struct smu_sdbp_header *__smu_get_sdb_partition(int id,
		unsigned int *size, int interruptible)
{
	char pname[32];
	const struct smu_sdbp_header *part;

	if (!smu)
		return NULL;

	sprintf(pname, "sdb-partition-%02x", id);

	DPRINTK("smu_get_sdb_partition(%02x)\n", id);

	if (interruptible) {
		int rc;
		rc = mutex_lock_interruptible(&smu_part_access);
		if (rc)
			return ERR_PTR(rc);
	} else
		mutex_lock(&smu_part_access);

	part = of_get_property(smu->of_node, pname, size);
	if (part == NULL) {
		DPRINTK("trying to extract from SMU ...\n");
		part = smu_create_sdb_partition(id);
		if (part != NULL && size)
			*size = part->len << 2;
	}
	mutex_unlock(&smu_part_access);
	return part;
}

const struct smu_sdbp_header *smu_get_sdb_partition(int id, unsigned int *size)
{
	return __smu_get_sdb_partition(id, size, 0);
}
EXPORT_SYMBOL(smu_get_sdb_partition);




static LIST_HEAD(smu_clist);
static DEFINE_SPINLOCK(smu_clist_lock);

enum smu_file_mode {
	smu_file_commands,
	smu_file_events,
	smu_file_closing
};

struct smu_private
{
	struct list_head	list;
	enum smu_file_mode	mode;
	int			busy;
	struct smu_cmd		cmd;
	spinlock_t		lock;
	wait_queue_head_t	wait;
	u8			buffer[SMU_MAX_DATA];
};


static int smu_open(struct inode *inode, struct file *file)
{
	struct smu_private *pp;
	unsigned long flags;

	pp = kzalloc(sizeof(struct smu_private), GFP_KERNEL);
	if (pp == 0)
		return -ENOMEM;
	spin_lock_init(&pp->lock);
	pp->mode = smu_file_commands;
	init_waitqueue_head(&pp->wait);

	mutex_lock(&smu_mutex);
	spin_lock_irqsave(&smu_clist_lock, flags);
	list_add(&pp->list, &smu_clist);
	spin_unlock_irqrestore(&smu_clist_lock, flags);
	file->private_data = pp;
	mutex_unlock(&smu_mutex);

	return 0;
}


static void smu_user_cmd_done(struct smu_cmd *cmd, void *misc)
{
	struct smu_private *pp = misc;

	wake_up_all(&pp->wait);
}


static ssize_t smu_write(struct file *file, const char __user *buf,
			 size_t count, loff_t *ppos)
{
	struct smu_private *pp = file->private_data;
	unsigned long flags;
	struct smu_user_cmd_hdr hdr;
	int rc = 0;

	if (pp->busy)
		return -EBUSY;
	else if (copy_from_user(&hdr, buf, sizeof(hdr)))
		return -EFAULT;
	else if (hdr.cmdtype == SMU_CMDTYPE_WANTS_EVENTS) {
		pp->mode = smu_file_events;
		return 0;
	} else if (hdr.cmdtype == SMU_CMDTYPE_GET_PARTITION) {
		const struct smu_sdbp_header *part;
		part = __smu_get_sdb_partition(hdr.cmd, NULL, 1);
		if (part == NULL)
			return -EINVAL;
		else if (IS_ERR(part))
			return PTR_ERR(part);
		return 0;
	} else if (hdr.cmdtype != SMU_CMDTYPE_SMU)
		return -EINVAL;
	else if (pp->mode != smu_file_commands)
		return -EBADFD;
	else if (hdr.data_len > SMU_MAX_DATA)
		return -EINVAL;

	spin_lock_irqsave(&pp->lock, flags);
	if (pp->busy) {
		spin_unlock_irqrestore(&pp->lock, flags);
		return -EBUSY;
	}
	pp->busy = 1;
	pp->cmd.status = 1;
	spin_unlock_irqrestore(&pp->lock, flags);

	if (copy_from_user(pp->buffer, buf + sizeof(hdr), hdr.data_len)) {
		pp->busy = 0;
		return -EFAULT;
	}

	pp->cmd.cmd = hdr.cmd;
	pp->cmd.data_len = hdr.data_len;
	pp->cmd.reply_len = SMU_MAX_DATA;
	pp->cmd.data_buf = pp->buffer;
	pp->cmd.reply_buf = pp->buffer;
	pp->cmd.done = smu_user_cmd_done;
	pp->cmd.misc = pp;
	rc = smu_queue_cmd(&pp->cmd);
	if (rc < 0)
		return rc;
	return count;
}


static ssize_t smu_read_command(struct file *file, struct smu_private *pp,
				char __user *buf, size_t count)
{
	DECLARE_WAITQUEUE(wait, current);
	struct smu_user_reply_hdr hdr;
	unsigned long flags;
	int size, rc = 0;

	if (!pp->busy)
		return 0;
	if (count < sizeof(struct smu_user_reply_hdr))
		return -EOVERFLOW;
	spin_lock_irqsave(&pp->lock, flags);
	if (pp->cmd.status == 1) {
		if (file->f_flags & O_NONBLOCK) {
			spin_unlock_irqrestore(&pp->lock, flags);
			return -EAGAIN;
		}
		add_wait_queue(&pp->wait, &wait);
		for (;;) {
			set_current_state(TASK_INTERRUPTIBLE);
			rc = 0;
			if (pp->cmd.status != 1)
				break;
			rc = -ERESTARTSYS;
			if (signal_pending(current))
				break;
			spin_unlock_irqrestore(&pp->lock, flags);
			schedule();
			spin_lock_irqsave(&pp->lock, flags);
		}
		set_current_state(TASK_RUNNING);
		remove_wait_queue(&pp->wait, &wait);
	}
	spin_unlock_irqrestore(&pp->lock, flags);
	if (rc)
		return rc;
	if (pp->cmd.status != 0)
		pp->cmd.reply_len = 0;
	size = sizeof(hdr) + pp->cmd.reply_len;
	if (count < size)
		size = count;
	rc = size;
	hdr.status = pp->cmd.status;
	hdr.reply_len = pp->cmd.reply_len;
	if (copy_to_user(buf, &hdr, sizeof(hdr)))
		return -EFAULT;
	size -= sizeof(hdr);
	if (size && copy_to_user(buf + sizeof(hdr), pp->buffer, size))
		return -EFAULT;
	pp->busy = 0;

	return rc;
}


static ssize_t smu_read_events(struct file *file, struct smu_private *pp,
			       char __user *buf, size_t count)
{
	
	msleep_interruptible(1000);
	return 0;
}


static ssize_t smu_read(struct file *file, char __user *buf,
			size_t count, loff_t *ppos)
{
	struct smu_private *pp = file->private_data;

	if (pp->mode == smu_file_commands)
		return smu_read_command(file, pp, buf, count);
	if (pp->mode == smu_file_events)
		return smu_read_events(file, pp, buf, count);

	return -EBADFD;
}

static unsigned int smu_fpoll(struct file *file, poll_table *wait)
{
	struct smu_private *pp = file->private_data;
	unsigned int mask = 0;
	unsigned long flags;

	if (pp == 0)
		return 0;

	if (pp->mode == smu_file_commands) {
		poll_wait(file, &pp->wait, wait);

		spin_lock_irqsave(&pp->lock, flags);
		if (pp->busy && pp->cmd.status != 1)
			mask |= POLLIN;
		spin_unlock_irqrestore(&pp->lock, flags);
	} if (pp->mode == smu_file_events) {
		
	}
	return mask;
}

static int smu_release(struct inode *inode, struct file *file)
{
	struct smu_private *pp = file->private_data;
	unsigned long flags;
	unsigned int busy;

	if (pp == 0)
		return 0;

	file->private_data = NULL;

	
	spin_lock_irqsave(&pp->lock, flags);
	pp->mode = smu_file_closing;
	busy = pp->busy;

	
	if (busy && pp->cmd.status == 1) {
		DECLARE_WAITQUEUE(wait, current);

		add_wait_queue(&pp->wait, &wait);
		for (;;) {
			set_current_state(TASK_UNINTERRUPTIBLE);
			if (pp->cmd.status != 1)
				break;
			spin_unlock_irqrestore(&pp->lock, flags);
			schedule();
			spin_lock_irqsave(&pp->lock, flags);
		}
		set_current_state(TASK_RUNNING);
		remove_wait_queue(&pp->wait, &wait);
	}
	spin_unlock_irqrestore(&pp->lock, flags);

	spin_lock_irqsave(&smu_clist_lock, flags);
	list_del(&pp->list);
	spin_unlock_irqrestore(&smu_clist_lock, flags);
	kfree(pp);

	return 0;
}


static const struct file_operations smu_device_fops = {
	.llseek		= no_llseek,
	.read		= smu_read,
	.write		= smu_write,
	.poll		= smu_fpoll,
	.open		= smu_open,
	.release	= smu_release,
};

static struct miscdevice pmu_device = {
	MISC_DYNAMIC_MINOR, "smu", &smu_device_fops
};

static int smu_device_init(void)
{
	if (!smu)
		return -ENODEV;
	if (misc_register(&pmu_device) < 0)
		printk(KERN_ERR "via-pmu: cannot register misc device.\n");
	return 0;
}
device_initcall(smu_device_init);
