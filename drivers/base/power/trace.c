/*
 * drivers/base/power/trace.c
 *
 * Copyright (C) 2006 Linus Torvalds
 *
 * Trace facility for suspend/resume problems, when none of the
 * devices may be working.
 */

#include <linux/resume-trace.h>
#include <linux/export.h>
#include <linux/rtc.h>

#include <asm/rtc.h>

#include "power.h"

#define USERHASH (16)
#define FILEHASH (997)
#define DEVHASH (1009)

#define DEVSEED (7919)

static unsigned int dev_hash_value;

static int set_magic_time(unsigned int user, unsigned int file, unsigned int device)
{
	unsigned int n = user + USERHASH*(file + FILEHASH*device);

	
	static struct rtc_time time = {
		.tm_sec = 0,
		.tm_min = 0,
		.tm_hour = 0,
		.tm_mday = 7,
		.tm_mon = 5,	
		.tm_year = 106,
		.tm_wday = 3,
		.tm_yday = 160,
		.tm_isdst = 1
	};

	time.tm_year = (n % 100);
	n /= 100;
	time.tm_mon = (n % 12);
	n /= 12;
	time.tm_mday = (n % 28) + 1;
	n /= 28;
	time.tm_hour = (n % 24);
	n /= 24;
	time.tm_min = (n % 20) * 3;
	n /= 20;
	set_rtc_time(&time);
	return n ? -1 : 0;
}

static unsigned int read_magic_time(void)
{
	struct rtc_time time;
	unsigned int val;

	get_rtc_time(&time);
	pr_info("RTC time: %2d:%02d:%02d, date: %02d/%02d/%02d\n",
		time.tm_hour, time.tm_min, time.tm_sec,
		time.tm_mon + 1, time.tm_mday, time.tm_year % 100);
	val = time.tm_year;				
	if (val > 100)
		val -= 100;
	val += time.tm_mon * 100;			
	val += (time.tm_mday-1) * 100 * 12;		
	val += time.tm_hour * 100 * 12 * 28;		
	val += (time.tm_min / 3) * 100 * 12 * 28 * 24;	
	return val;
}

static unsigned int hash_string(unsigned int seed, const char *data, unsigned int mod)
{
	unsigned char c;
	while ((c = *data++) != 0) {
		seed = (seed << 16) + (seed << 6) - seed + c;
	}
	return seed % mod;
}

void set_trace_device(struct device *dev)
{
	dev_hash_value = hash_string(DEVSEED, dev_name(dev), DEVHASH);
}
EXPORT_SYMBOL(set_trace_device);

void generate_resume_trace(const void *tracedata, unsigned int user)
{
	unsigned short lineno = *(unsigned short *)tracedata;
	const char *file = *(const char **)(tracedata + 2);
	unsigned int user_hash_value, file_hash_value;

	user_hash_value = user % USERHASH;
	file_hash_value = hash_string(lineno, file, FILEHASH);
	set_magic_time(user_hash_value, file_hash_value, dev_hash_value);
}
EXPORT_SYMBOL(generate_resume_trace);

extern char __tracedata_start, __tracedata_end;
static int show_file_hash(unsigned int value)
{
	int match;
	char *tracedata;

	match = 0;
	for (tracedata = &__tracedata_start ; tracedata < &__tracedata_end ;
			tracedata += 2 + sizeof(unsigned long)) {
		unsigned short lineno = *(unsigned short *)tracedata;
		const char *file = *(const char **)(tracedata + 2);
		unsigned int hash = hash_string(lineno, file, FILEHASH);
		if (hash != value)
			continue;
		pr_info("  hash matches %s:%u\n", file, lineno);
		match++;
	}
	return match;
}

static int show_dev_hash(unsigned int value)
{
	int match = 0;
	struct list_head *entry;

	device_pm_lock();
	entry = dpm_list.prev;
	while (entry != &dpm_list) {
		struct device * dev = to_device(entry);
		unsigned int hash = hash_string(DEVSEED, dev_name(dev), DEVHASH);
		if (hash == value) {
			dev_info(dev, "hash matches\n");
			match++;
		}
		entry = entry->prev;
	}
	device_pm_unlock();
	return match;
}

static unsigned int hash_value_early_read;

int show_trace_dev_match(char *buf, size_t size)
{
	unsigned int value = hash_value_early_read / (USERHASH * FILEHASH);
	int ret = 0;
	struct list_head *entry;

	device_pm_lock();
	entry = dpm_list.prev;
	while (size && entry != &dpm_list) {
		struct device *dev = to_device(entry);
		unsigned int hash = hash_string(DEVSEED, dev_name(dev),
						DEVHASH);
		if (hash == value) {
			int len = snprintf(buf, size, "%s\n",
					    dev_driver_string(dev));
			if (len > size)
				len = size;
			buf += len;
			ret += len;
			size -= len;
		}
		entry = entry->prev;
	}
	device_pm_unlock();
	return ret;
}

static int early_resume_init(void)
{
	hash_value_early_read = read_magic_time();
	return 0;
}

static int late_resume_init(void)
{
	unsigned int val = hash_value_early_read;
	unsigned int user, file, dev;

	user = val % USERHASH;
	val = val / USERHASH;
	file = val % FILEHASH;
	val = val / FILEHASH;
	dev = val ;

	pr_info("  Magic number: %d:%d:%d\n", user, file, dev);
	show_file_hash(file);
	show_dev_hash(dev);
	return 0;
}

core_initcall(early_resume_init);
late_initcall(late_resume_init);
