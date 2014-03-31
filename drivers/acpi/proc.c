#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/export.h>
#include <linux/suspend.h>
#include <linux/bcd.h>
#include <asm/uaccess.h>

#include <acpi/acpi_bus.h>
#include <acpi/acpi_drivers.h>

#ifdef CONFIG_X86
#include <linux/mc146818rtc.h>
#endif

#include "sleep.h"

#define _COMPONENT		ACPI_SYSTEM_COMPONENT


ACPI_MODULE_NAME("sleep")

#if defined(CONFIG_RTC_DRV_CMOS) || defined(CONFIG_RTC_DRV_CMOS_MODULE) || !defined(CONFIG_X86)
#else
#define	HAVE_ACPI_LEGACY_ALARM
#endif

#ifdef	HAVE_ACPI_LEGACY_ALARM

static u32 cmos_bcd_read(int offset, int rtc_control);

static int acpi_system_alarm_seq_show(struct seq_file *seq, void *offset)
{
	u32 sec, min, hr;
	u32 day, mo, yr, cent = 0;
	u32 today = 0;
	unsigned char rtc_control = 0;
	unsigned long flags;

	spin_lock_irqsave(&rtc_lock, flags);

	rtc_control = CMOS_READ(RTC_CONTROL);
	sec = cmos_bcd_read(RTC_SECONDS_ALARM, rtc_control);
	min = cmos_bcd_read(RTC_MINUTES_ALARM, rtc_control);
	hr = cmos_bcd_read(RTC_HOURS_ALARM, rtc_control);

	
	if (acpi_gbl_FADT.day_alarm) {
		
		day = CMOS_READ(acpi_gbl_FADT.day_alarm) & 0x3F;
		if (!(rtc_control & RTC_DM_BINARY) || RTC_ALWAYS_BCD)
			day = bcd2bin(day);
	} else
		day = cmos_bcd_read(RTC_DAY_OF_MONTH, rtc_control);
	if (acpi_gbl_FADT.month_alarm)
		mo = cmos_bcd_read(acpi_gbl_FADT.month_alarm, rtc_control);
	else {
		mo = cmos_bcd_read(RTC_MONTH, rtc_control);
		today = cmos_bcd_read(RTC_DAY_OF_MONTH, rtc_control);
	}
	if (acpi_gbl_FADT.century)
		cent = cmos_bcd_read(acpi_gbl_FADT.century, rtc_control);

	yr = cmos_bcd_read(RTC_YEAR, rtc_control);

	spin_unlock_irqrestore(&rtc_lock, flags);

	
	if (!acpi_gbl_FADT.century)
		yr += 2000;
	else
		yr += cent * 100;

	if (day < today) {
		if (mo < 12) {
			mo += 1;
		} else {
			mo = 1;
			yr += 1;
		}
	}

	seq_printf(seq, "%4.4u-", yr);
	(mo > 12) ? seq_puts(seq, "**-") : seq_printf(seq, "%2.2u-", mo);
	(day > 31) ? seq_puts(seq, "** ") : seq_printf(seq, "%2.2u ", day);
	(hr > 23) ? seq_puts(seq, "**:") : seq_printf(seq, "%2.2u:", hr);
	(min > 59) ? seq_puts(seq, "**:") : seq_printf(seq, "%2.2u:", min);
	(sec > 59) ? seq_puts(seq, "**\n") : seq_printf(seq, "%2.2u\n", sec);

	return 0;
}

static int acpi_system_alarm_open_fs(struct inode *inode, struct file *file)
{
	return single_open(file, acpi_system_alarm_seq_show, PDE(inode)->data);
}

static int get_date_field(char **p, u32 * value)
{
	char *next = NULL;
	char *string_end = NULL;
	int result = -EINVAL;

	if (*p == NULL)
		return result;

	next = strpbrk(*p, "- :");
	if (next)
		*next++ = '\0';

	*value = simple_strtoul(*p, &string_end, 10);

	
	if (string_end != *p)
		result = 0;

	if (next)
		*p = next;
	else
		*p = NULL;

	return result;
}

static u32 cmos_bcd_read(int offset, int rtc_control)
{
	u32 val = CMOS_READ(offset);
	if (!(rtc_control & RTC_DM_BINARY) || RTC_ALWAYS_BCD)
		val = bcd2bin(val);
	return val;
}

static void cmos_bcd_write(u32 val, int offset, int rtc_control)
{
	if (!(rtc_control & RTC_DM_BINARY) || RTC_ALWAYS_BCD)
		val = bin2bcd(val);
	CMOS_WRITE(val, offset);
}

static ssize_t
acpi_system_write_alarm(struct file *file,
			const char __user * buffer, size_t count, loff_t * ppos)
{
	int result = 0;
	char alarm_string[30] = { '\0' };
	char *p = alarm_string;
	u32 sec, min, hr, day, mo, yr;
	int adjust = 0;
	unsigned char rtc_control = 0;

	if (count > sizeof(alarm_string) - 1)
		return -EINVAL;

	if (copy_from_user(alarm_string, buffer, count))
		return -EFAULT;

	alarm_string[count] = '\0';

	
	if (alarm_string[0] == '+') {
		p++;
		adjust = 1;
	}

	if ((result = get_date_field(&p, &yr)))
		goto end;
	if ((result = get_date_field(&p, &mo)))
		goto end;
	if ((result = get_date_field(&p, &day)))
		goto end;
	if ((result = get_date_field(&p, &hr)))
		goto end;
	if ((result = get_date_field(&p, &min)))
		goto end;
	if ((result = get_date_field(&p, &sec)))
		goto end;

	spin_lock_irq(&rtc_lock);

	rtc_control = CMOS_READ(RTC_CONTROL);

	if (adjust) {
		yr += cmos_bcd_read(RTC_YEAR, rtc_control);
		mo += cmos_bcd_read(RTC_MONTH, rtc_control);
		day += cmos_bcd_read(RTC_DAY_OF_MONTH, rtc_control);
		hr += cmos_bcd_read(RTC_HOURS, rtc_control);
		min += cmos_bcd_read(RTC_MINUTES, rtc_control);
		sec += cmos_bcd_read(RTC_SECONDS, rtc_control);
	}

	spin_unlock_irq(&rtc_lock);

	if (sec > 59) {
		min += sec/60;
		sec = sec%60;
	}
	if (min > 59) {
		hr += min/60;
		min = min%60;
	}
	if (hr > 23) {
		day += hr/24;
		hr = hr%24;
	}
	if (day > 31) {
		mo += day/32;
		day = day%32;
	}
	if (mo > 12) {
		yr += mo/13;
		mo = mo%13;
	}

	spin_lock_irq(&rtc_lock);
	rtc_control &= ~RTC_AIE;
	CMOS_WRITE(rtc_control, RTC_CONTROL);
	CMOS_READ(RTC_INTR_FLAGS);

	
	cmos_bcd_write(hr, RTC_HOURS_ALARM, rtc_control);
	cmos_bcd_write(min, RTC_MINUTES_ALARM, rtc_control);
	cmos_bcd_write(sec, RTC_SECONDS_ALARM, rtc_control);

	if (acpi_gbl_FADT.day_alarm)
		cmos_bcd_write(day, acpi_gbl_FADT.day_alarm, rtc_control);
	if (acpi_gbl_FADT.month_alarm)
		cmos_bcd_write(mo, acpi_gbl_FADT.month_alarm, rtc_control);
	if (acpi_gbl_FADT.century) {
		if (adjust)
			yr += cmos_bcd_read(acpi_gbl_FADT.century, rtc_control) * 100;
		cmos_bcd_write(yr / 100, acpi_gbl_FADT.century, rtc_control);
	}
	
	rtc_control |= RTC_AIE;
	CMOS_WRITE(rtc_control, RTC_CONTROL);
	CMOS_READ(RTC_INTR_FLAGS);

	spin_unlock_irq(&rtc_lock);

	acpi_clear_event(ACPI_EVENT_RTC);
	acpi_enable_event(ACPI_EVENT_RTC, 0);

	*ppos += count;

	result = 0;
      end:
	return result ? result : count;
}
#endif				

static int
acpi_system_wakeup_device_seq_show(struct seq_file *seq, void *offset)
{
	struct list_head *node, *next;

	seq_printf(seq, "Device\tS-state\t  Status   Sysfs node\n");

	mutex_lock(&acpi_device_lock);
	list_for_each_safe(node, next, &acpi_wakeup_device_list) {
		struct acpi_device *dev =
		    container_of(node, struct acpi_device, wakeup_list);
		struct device *ldev;

		if (!dev->wakeup.flags.valid)
			continue;

		ldev = acpi_get_physical_device(dev->handle);
		seq_printf(seq, "%s\t  S%d\t%c%-8s  ",
			   dev->pnp.bus_id,
			   (u32) dev->wakeup.sleep_state,
			   dev->wakeup.flags.run_wake ? '*' : ' ',
			   (device_may_wakeup(&dev->dev)
			     || (ldev && device_may_wakeup(ldev))) ?
			       "enabled" : "disabled");
		if (ldev)
			seq_printf(seq, "%s:%s",
				   ldev->bus ? ldev->bus->name : "no-bus",
				   dev_name(ldev));
		seq_printf(seq, "\n");
		put_device(ldev);

	}
	mutex_unlock(&acpi_device_lock);
	return 0;
}

static void physical_device_enable_wakeup(struct acpi_device *adev)
{
	struct device *dev = acpi_get_physical_device(adev->handle);

	if (dev && device_can_wakeup(dev)) {
		bool enable = !device_may_wakeup(dev);
		device_set_wakeup_enable(dev, enable);
	}
}

static ssize_t
acpi_system_write_wakeup_device(struct file *file,
				const char __user * buffer,
				size_t count, loff_t * ppos)
{
	struct list_head *node, *next;
	char strbuf[5];
	char str[5] = "";
	unsigned int len = count;

	if (len > 4)
		len = 4;
	if (len < 0)
		return -EFAULT;

	if (copy_from_user(strbuf, buffer, len))
		return -EFAULT;
	strbuf[len] = '\0';
	sscanf(strbuf, "%s", str);

	mutex_lock(&acpi_device_lock);
	list_for_each_safe(node, next, &acpi_wakeup_device_list) {
		struct acpi_device *dev =
		    container_of(node, struct acpi_device, wakeup_list);
		if (!dev->wakeup.flags.valid)
			continue;

		if (!strncmp(dev->pnp.bus_id, str, 4)) {
			if (device_can_wakeup(&dev->dev)) {
				bool enable = !device_may_wakeup(&dev->dev);
				device_set_wakeup_enable(&dev->dev, enable);
			} else {
				physical_device_enable_wakeup(dev);
			}
			break;
		}
	}
	mutex_unlock(&acpi_device_lock);
	return count;
}

static int
acpi_system_wakeup_device_open_fs(struct inode *inode, struct file *file)
{
	return single_open(file, acpi_system_wakeup_device_seq_show,
			   PDE(inode)->data);
}

static const struct file_operations acpi_system_wakeup_device_fops = {
	.owner = THIS_MODULE,
	.open = acpi_system_wakeup_device_open_fs,
	.read = seq_read,
	.write = acpi_system_write_wakeup_device,
	.llseek = seq_lseek,
	.release = single_release,
};

#ifdef	HAVE_ACPI_LEGACY_ALARM
static const struct file_operations acpi_system_alarm_fops = {
	.owner = THIS_MODULE,
	.open = acpi_system_alarm_open_fs,
	.read = seq_read,
	.write = acpi_system_write_alarm,
	.llseek = seq_lseek,
	.release = single_release,
};

static u32 rtc_handler(void *context)
{
	acpi_clear_event(ACPI_EVENT_RTC);
	acpi_disable_event(ACPI_EVENT_RTC, 0);

	return ACPI_INTERRUPT_HANDLED;
}
#endif				

int __init acpi_sleep_proc_init(void)
{
#ifdef	HAVE_ACPI_LEGACY_ALARM
	
	proc_create("alarm", S_IFREG | S_IRUGO | S_IWUSR,
		    acpi_root_dir, &acpi_system_alarm_fops);

	acpi_install_fixed_event_handler(ACPI_EVENT_RTC, rtc_handler, NULL);
	acpi_clear_event(ACPI_EVENT_RTC);
	acpi_disable_event(ACPI_EVENT_RTC, 0);
#endif				

	
	proc_create("wakeup", S_IFREG | S_IRUGO | S_IWUSR,
		    acpi_root_dir, &acpi_system_wakeup_device_fops);

	return 0;
}
