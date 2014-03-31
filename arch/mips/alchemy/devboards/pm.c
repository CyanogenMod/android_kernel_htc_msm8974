
#include <linux/init.h>
#include <linux/kobject.h>
#include <linux/suspend.h>
#include <linux/sysfs.h>
#include <asm/mach-au1x00/au1000.h>
#include <asm/mach-au1x00/gpio.h>
#include <asm/mach-db1x00/bcsr.h>



static unsigned long db1x_pm_sleep_secs;
static unsigned long db1x_pm_wakemsk;
static unsigned long db1x_pm_last_wakesrc;

static int db1x_pm_enter(suspend_state_t state)
{
	unsigned short bcsrs[16];
	int i, j, hasint;

	
	hasint = bcsr_read(BCSR_WHOAMI);
	hasint = BCSR_WHOAMI_BOARD(hasint) >= BCSR_WHOAMI_DB1200;
	j = (hasint) ? BCSR_MASKSET : BCSR_SYSTEM;

	for (i = BCSR_STATUS; i <= j; i++)
		bcsrs[i] = bcsr_read(i);

	
	bcsr_write(BCSR_HEXCLEAR, 3);

	
	alchemy_gpio1_input_enable();

	
	au_writel(0, SYS_WAKEMSK);
	au_sync();
	au_writel(0, SYS_WAKESRC);
	au_sync();

	au_writel(db1x_pm_wakemsk, SYS_WAKEMSK);
	au_sync();

	
	while (au_readl(SYS_COUNTER_CNTRL) & SYS_CNTRL_M20)
		asm volatile ("nop");

	au_writel(au_readl(SYS_TOYREAD) + db1x_pm_sleep_secs, SYS_TOYMATCH2);
	au_sync();

	
	while (au_readl(SYS_COUNTER_CNTRL) & SYS_CNTRL_M20)
		asm volatile ("nop");

	
	au_sleep();


	
	for (i = BCSR_STATUS; i <= BCSR_SYSTEM; i++)
		bcsr_write(i, bcsrs[i]);

	
	if (hasint) {
		bcsr_write(BCSR_INTCLR, 0xffff);
		bcsr_write(BCSR_MASKCLR, 0xffff);
		bcsr_write(BCSR_INTSTAT, 0xffff);
		bcsr_write(BCSR_INTSET, bcsrs[BCSR_INTSET]);
		bcsr_write(BCSR_MASKSET, bcsrs[BCSR_MASKSET]);
	}

	
	bcsr_write(BCSR_HEXCLEAR, 0);

	return 0;
}

static int db1x_pm_begin(suspend_state_t state)
{
	if (!db1x_pm_wakemsk) {
		printk(KERN_ERR "db1x: no wakeup source activated!\n");
		return -EINVAL;
	}

	return 0;
}

static void db1x_pm_end(void)
{
	db1x_pm_last_wakesrc = au_readl(SYS_WAKESRC);

	au_writel(0, SYS_WAKEMSK);
	au_writel(0, SYS_WAKESRC);
	au_sync();

}

static const struct platform_suspend_ops db1x_pm_ops = {
	.valid		= suspend_valid_only_mem,
	.begin		= db1x_pm_begin,
	.enter		= db1x_pm_enter,
	.end		= db1x_pm_end,
};

#define ATTRCMP(x) (0 == strcmp(attr->attr.name, #x))

static ssize_t db1x_pmattr_show(struct kobject *kobj,
				struct kobj_attribute *attr,
				char *buf)
{
	int idx;

	if (ATTRCMP(timer_timeout))
		return sprintf(buf, "%lu\n", db1x_pm_sleep_secs);

	else if (ATTRCMP(timer))
		return sprintf(buf, "%u\n",
				!!(db1x_pm_wakemsk & SYS_WAKEMSK_M2));

	else if (ATTRCMP(wakesrc))
		return sprintf(buf, "%lu\n", db1x_pm_last_wakesrc);

	else if (ATTRCMP(gpio0) || ATTRCMP(gpio1) || ATTRCMP(gpio2) ||
		 ATTRCMP(gpio3) || ATTRCMP(gpio4) || ATTRCMP(gpio5) ||
		 ATTRCMP(gpio6) || ATTRCMP(gpio7)) {
		idx = (attr->attr.name)[4] - '0';
		return sprintf(buf, "%d\n",
			!!(db1x_pm_wakemsk & SYS_WAKEMSK_GPIO(idx)));

	} else if (ATTRCMP(wakemsk)) {
		return sprintf(buf, "%08lx\n", db1x_pm_wakemsk);
	}

	return -ENOENT;
}

static ssize_t db1x_pmattr_store(struct kobject *kobj,
				 struct kobj_attribute *attr,
				 const char *instr,
				 size_t bytes)
{
	unsigned long l;
	int tmp;

	if (ATTRCMP(timer_timeout)) {
		tmp = strict_strtoul(instr, 0, &l);
		if (tmp)
			return tmp;

		db1x_pm_sleep_secs = l;

	} else if (ATTRCMP(timer)) {
		if (instr[0] != '0')
			db1x_pm_wakemsk |= SYS_WAKEMSK_M2;
		else
			db1x_pm_wakemsk &= ~SYS_WAKEMSK_M2;

	} else if (ATTRCMP(gpio0) || ATTRCMP(gpio1) || ATTRCMP(gpio2) ||
		   ATTRCMP(gpio3) || ATTRCMP(gpio4) || ATTRCMP(gpio5) ||
		   ATTRCMP(gpio6) || ATTRCMP(gpio7)) {
		tmp = (attr->attr.name)[4] - '0';
		if (instr[0] != '0') {
			db1x_pm_wakemsk |= SYS_WAKEMSK_GPIO(tmp);
		} else {
			db1x_pm_wakemsk &= ~SYS_WAKEMSK_GPIO(tmp);
		}

	} else if (ATTRCMP(wakemsk)) {
		tmp = strict_strtoul(instr, 0, &l);
		if (tmp)
			return tmp;

		db1x_pm_wakemsk = l & 0x0000003f;

	} else
		bytes = -ENOENT;

	return bytes;
}

#define ATTR(x)							\
	static struct kobj_attribute x##_attribute = 		\
		__ATTR(x, 0664, db1x_pmattr_show,		\
				db1x_pmattr_store);

ATTR(gpio0)		
ATTR(gpio1)
ATTR(gpio2)
ATTR(gpio3)
ATTR(gpio4)
ATTR(gpio5)
ATTR(gpio6)
ATTR(gpio7)
ATTR(timer)		
ATTR(timer_timeout)	
ATTR(wakesrc)		
ATTR(wakemsk)		

#define ATTR_LIST(x)	& x ## _attribute.attr
static struct attribute *db1x_pmattrs[] = {
	ATTR_LIST(gpio0),
	ATTR_LIST(gpio1),
	ATTR_LIST(gpio2),
	ATTR_LIST(gpio3),
	ATTR_LIST(gpio4),
	ATTR_LIST(gpio5),
	ATTR_LIST(gpio6),
	ATTR_LIST(gpio7),
	ATTR_LIST(timer),
	ATTR_LIST(timer_timeout),
	ATTR_LIST(wakesrc),
	ATTR_LIST(wakemsk),
	NULL,		
};

static struct attribute_group db1x_pmattr_group = {
	.name	= "db1x",
	.attrs	= db1x_pmattrs,
};

static int __init pm_init(void)
{
	if (au_readl(SYS_TOYTRIM) != 32767) {
		au_writel(32767, SYS_TOYTRIM);
		au_sync();
	}

	db1x_pm_last_wakesrc = au_readl(SYS_WAKESRC);

	au_writel(0, SYS_WAKESRC);
	au_sync();
	au_writel(0, SYS_WAKEMSK);
	au_sync();

	suspend_set_ops(&db1x_pm_ops);

	return sysfs_create_group(power_kobj, &db1x_pmattr_group);
}

late_initcall(pm_init);
