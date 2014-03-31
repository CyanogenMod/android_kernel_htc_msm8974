/* 
 *  Parallel SCSI (SPI) transport specific attributes exported to sysfs.
 *
 *  Copyright (c) 2003 Silicon Graphics, Inc.  All rights reserved.
 *  Copyright (c) 2004, 2005 James Bottomley <James.Bottomley@SteelEye.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include <linux/ctype.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/workqueue.h>
#include <linux/blkdev.h>
#include <linux/mutex.h>
#include <linux/sysfs.h>
#include <linux/slab.h>
#include <scsi/scsi.h>
#include "scsi_priv.h"
#include <scsi/scsi_device.h>
#include <scsi/scsi_host.h>
#include <scsi/scsi_cmnd.h>
#include <scsi/scsi_eh.h>
#include <scsi/scsi_transport.h>
#include <scsi/scsi_transport_spi.h>

#define SPI_NUM_ATTRS 14	
#define SPI_OTHER_ATTRS 1	
#define SPI_HOST_ATTRS	1

#define SPI_MAX_ECHO_BUFFER_SIZE	4096

#define DV_LOOPS	3
#define DV_TIMEOUT	(10*HZ)
#define DV_RETRIES	3	

enum {
	SPI_BLIST_NOIUS = 0x1,
};

static struct {
	char *vendor;
	char *model;
	unsigned flags;
} spi_static_device_list[] __initdata = {
	{"HP", "Ultrium 3-SCSI", SPI_BLIST_NOIUS },
	{"IBM", "ULTRIUM-TD3", SPI_BLIST_NOIUS },
	{NULL, NULL, 0}
};

#define spi_dv_in_progress(x) (((struct spi_transport_attrs *)&(x)->starget_data)->dv_in_progress)
#define spi_dv_mutex(x) (((struct spi_transport_attrs *)&(x)->starget_data)->dv_mutex)

struct spi_internal {
	struct scsi_transport_template t;
	struct spi_function_template *f;
};

#define to_spi_internal(tmpl)	container_of(tmpl, struct spi_internal, t)

static const int ppr_to_ps[] = {
	-1,			
	-1,			
	-1,			
	-1,			
	-1,			
	-1,			
	-1,			
	 3125,			
	 6250,			
	12500,			
	25000,			
	30300,			
	50000,			
};
#define SPI_STATIC_PPR	0x0c

static int sprint_frac(char *dest, int value, int denom)
{
	int frac = value % denom;
	int result = sprintf(dest, "%d", value / denom);

	if (frac == 0)
		return result;
	dest[result++] = '.';

	do {
		denom /= 10;
		sprintf(dest + result, "%d", frac / denom);
		result++;
		frac %= denom;
	} while (frac);

	dest[result++] = '\0';
	return result;
}

static int spi_execute(struct scsi_device *sdev, const void *cmd,
		       enum dma_data_direction dir,
		       void *buffer, unsigned bufflen,
		       struct scsi_sense_hdr *sshdr)
{
	int i, result;
	unsigned char sense[SCSI_SENSE_BUFFERSIZE];

	for(i = 0; i < DV_RETRIES; i++) {
		result = scsi_execute(sdev, cmd, dir, buffer, bufflen,
				      sense, DV_TIMEOUT,  1,
				      REQ_FAILFAST_DEV |
				      REQ_FAILFAST_TRANSPORT |
				      REQ_FAILFAST_DRIVER,
				      NULL);
		if (driver_byte(result) & DRIVER_SENSE) {
			struct scsi_sense_hdr sshdr_tmp;
			if (!sshdr)
				sshdr = &sshdr_tmp;

			if (scsi_normalize_sense(sense, SCSI_SENSE_BUFFERSIZE,
						 sshdr)
			    && sshdr->sense_key == UNIT_ATTENTION)
				continue;
		}
		break;
	}
	return result;
}

static struct {
	enum spi_signal_type	value;
	char			*name;
} signal_types[] = {
	{ SPI_SIGNAL_UNKNOWN, "unknown" },
	{ SPI_SIGNAL_SE, "SE" },
	{ SPI_SIGNAL_LVD, "LVD" },
	{ SPI_SIGNAL_HVD, "HVD" },
};

static inline const char *spi_signal_to_string(enum spi_signal_type type)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(signal_types); i++) {
		if (type == signal_types[i].value)
			return signal_types[i].name;
	}
	return NULL;
}
static inline enum spi_signal_type spi_signal_to_value(const char *name)
{
	int i, len;

	for (i = 0; i < ARRAY_SIZE(signal_types); i++) {
		len =  strlen(signal_types[i].name);
		if (strncmp(name, signal_types[i].name, len) == 0 &&
		    (name[len] == '\n' || name[len] == '\0'))
			return signal_types[i].value;
	}
	return SPI_SIGNAL_UNKNOWN;
}

static int spi_host_setup(struct transport_container *tc, struct device *dev,
			  struct device *cdev)
{
	struct Scsi_Host *shost = dev_to_shost(dev);

	spi_signalling(shost) = SPI_SIGNAL_UNKNOWN;

	return 0;
}

static int spi_host_configure(struct transport_container *tc,
			      struct device *dev,
			      struct device *cdev);

static DECLARE_TRANSPORT_CLASS(spi_host_class,
			       "spi_host",
			       spi_host_setup,
			       NULL,
			       spi_host_configure);

static int spi_host_match(struct attribute_container *cont,
			  struct device *dev)
{
	struct Scsi_Host *shost;

	if (!scsi_is_host_device(dev))
		return 0;

	shost = dev_to_shost(dev);
	if (!shost->transportt  || shost->transportt->host_attrs.ac.class
	    != &spi_host_class.class)
		return 0;

	return &shost->transportt->host_attrs.ac == cont;
}

static int spi_target_configure(struct transport_container *tc,
				struct device *dev,
				struct device *cdev);

static int spi_device_configure(struct transport_container *tc,
				struct device *dev,
				struct device *cdev)
{
	struct scsi_device *sdev = to_scsi_device(dev);
	struct scsi_target *starget = sdev->sdev_target;
	unsigned bflags = scsi_get_device_flags_keyed(sdev, &sdev->inquiry[8],
						      &sdev->inquiry[16],
						      SCSI_DEVINFO_SPI);


	spi_support_sync(starget) = scsi_device_sync(sdev);
	spi_support_wide(starget) = scsi_device_wide(sdev);
	spi_support_dt(starget) = scsi_device_dt(sdev);
	spi_support_dt_only(starget) = scsi_device_dt_only(sdev);
	spi_support_ius(starget) = scsi_device_ius(sdev);
	if (bflags & SPI_BLIST_NOIUS) {
		dev_info(dev, "Information Units disabled by blacklist\n");
		spi_support_ius(starget) = 0;
	}
	spi_support_qas(starget) = scsi_device_qas(sdev);

	return 0;
}

static int spi_setup_transport_attrs(struct transport_container *tc,
				     struct device *dev,
				     struct device *cdev)
{
	struct scsi_target *starget = to_scsi_target(dev);

	spi_period(starget) = -1;	
	spi_min_period(starget) = 0;
	spi_offset(starget) = 0;	
	spi_max_offset(starget) = 255;
	spi_width(starget) = 0;	
	spi_max_width(starget) = 1;
	spi_iu(starget) = 0;	
	spi_max_iu(starget) = 1;
	spi_dt(starget) = 0;	
	spi_qas(starget) = 0;
	spi_max_qas(starget) = 1;
	spi_wr_flow(starget) = 0;
	spi_rd_strm(starget) = 0;
	spi_rti(starget) = 0;
	spi_pcomp_en(starget) = 0;
	spi_hold_mcs(starget) = 0;
	spi_dv_pending(starget) = 0;
	spi_dv_in_progress(starget) = 0;
	spi_initial_dv(starget) = 0;
	mutex_init(&spi_dv_mutex(starget));

	return 0;
}

#define spi_transport_show_simple(field, format_string)			\
									\
static ssize_t								\
show_spi_transport_##field(struct device *dev, 			\
			   struct device_attribute *attr, char *buf)	\
{									\
	struct scsi_target *starget = transport_class_to_starget(dev);	\
	struct spi_transport_attrs *tp;					\
									\
	tp = (struct spi_transport_attrs *)&starget->starget_data;	\
	return snprintf(buf, 20, format_string, tp->field);		\
}

#define spi_transport_store_simple(field, format_string)		\
									\
static ssize_t								\
store_spi_transport_##field(struct device *dev, 			\
			    struct device_attribute *attr, 		\
			    const char *buf, size_t count)		\
{									\
	int val;							\
	struct scsi_target *starget = transport_class_to_starget(dev);	\
	struct spi_transport_attrs *tp;					\
									\
	tp = (struct spi_transport_attrs *)&starget->starget_data;	\
	val = simple_strtoul(buf, NULL, 0);				\
	tp->field = val;						\
	return count;							\
}

#define spi_transport_show_function(field, format_string)		\
									\
static ssize_t								\
show_spi_transport_##field(struct device *dev, 			\
			   struct device_attribute *attr, char *buf)	\
{									\
	struct scsi_target *starget = transport_class_to_starget(dev);	\
	struct Scsi_Host *shost = dev_to_shost(starget->dev.parent);	\
	struct spi_transport_attrs *tp;					\
	struct spi_internal *i = to_spi_internal(shost->transportt);	\
	tp = (struct spi_transport_attrs *)&starget->starget_data;	\
	if (i->f->get_##field)						\
		i->f->get_##field(starget);				\
	return snprintf(buf, 20, format_string, tp->field);		\
}

#define spi_transport_store_function(field, format_string)		\
static ssize_t								\
store_spi_transport_##field(struct device *dev, 			\
			    struct device_attribute *attr,		\
			    const char *buf, size_t count)		\
{									\
	int val;							\
	struct scsi_target *starget = transport_class_to_starget(dev);	\
	struct Scsi_Host *shost = dev_to_shost(starget->dev.parent);	\
	struct spi_internal *i = to_spi_internal(shost->transportt);	\
									\
	if (!i->f->set_##field)						\
		return -EINVAL;						\
	val = simple_strtoul(buf, NULL, 0);				\
	i->f->set_##field(starget, val);				\
	return count;							\
}

#define spi_transport_store_max(field, format_string)			\
static ssize_t								\
store_spi_transport_##field(struct device *dev, 			\
			    struct device_attribute *attr,		\
			    const char *buf, size_t count)		\
{									\
	int val;							\
	struct scsi_target *starget = transport_class_to_starget(dev);	\
	struct Scsi_Host *shost = dev_to_shost(starget->dev.parent);	\
	struct spi_internal *i = to_spi_internal(shost->transportt);	\
	struct spi_transport_attrs *tp					\
		= (struct spi_transport_attrs *)&starget->starget_data;	\
									\
	if (i->f->set_##field)						\
		return -EINVAL;						\
	val = simple_strtoul(buf, NULL, 0);				\
	if (val > tp->max_##field)					\
		val = tp->max_##field;					\
	i->f->set_##field(starget, val);				\
	return count;							\
}

#define spi_transport_rd_attr(field, format_string)			\
	spi_transport_show_function(field, format_string)		\
	spi_transport_store_function(field, format_string)		\
static DEVICE_ATTR(field, S_IRUGO,				\
		   show_spi_transport_##field,			\
		   store_spi_transport_##field);

#define spi_transport_simple_attr(field, format_string)			\
	spi_transport_show_simple(field, format_string)			\
	spi_transport_store_simple(field, format_string)		\
static DEVICE_ATTR(field, S_IRUGO,				\
		   show_spi_transport_##field,			\
		   store_spi_transport_##field);

#define spi_transport_max_attr(field, format_string)			\
	spi_transport_show_function(field, format_string)		\
	spi_transport_store_max(field, format_string)			\
	spi_transport_simple_attr(max_##field, format_string)		\
static DEVICE_ATTR(field, S_IRUGO,				\
		   show_spi_transport_##field,			\
		   store_spi_transport_##field);

spi_transport_max_attr(offset, "%d\n");
spi_transport_max_attr(width, "%d\n");
spi_transport_max_attr(iu, "%d\n");
spi_transport_rd_attr(dt, "%d\n");
spi_transport_max_attr(qas, "%d\n");
spi_transport_rd_attr(wr_flow, "%d\n");
spi_transport_rd_attr(rd_strm, "%d\n");
spi_transport_rd_attr(rti, "%d\n");
spi_transport_rd_attr(pcomp_en, "%d\n");
spi_transport_rd_attr(hold_mcs, "%d\n");

static int child_iter(struct device *dev, void *data)
{
	if (!scsi_is_sdev_device(dev))
		return 0;

	spi_dv_device(to_scsi_device(dev));
	return 1;
}

static ssize_t
store_spi_revalidate(struct device *dev, struct device_attribute *attr,
		     const char *buf, size_t count)
{
	struct scsi_target *starget = transport_class_to_starget(dev);

	device_for_each_child(&starget->dev, NULL, child_iter);
	return count;
}
static DEVICE_ATTR(revalidate, S_IWUSR, NULL, store_spi_revalidate);

static int period_to_str(char *buf, int period)
{
	int len, picosec;

	if (period < 0 || period > 0xff) {
		picosec = -1;
	} else if (period <= SPI_STATIC_PPR) {
		picosec = ppr_to_ps[period];
	} else {
		picosec = period * 4000;
	}

	if (picosec == -1) {
		len = sprintf(buf, "reserved");
	} else {
		len = sprint_frac(buf, picosec, 1000);
	}

	return len;
}

static ssize_t
show_spi_transport_period_helper(char *buf, int period)
{
	int len = period_to_str(buf, period);
	buf[len++] = '\n';
	buf[len] = '\0';
	return len;
}

static ssize_t
store_spi_transport_period_helper(struct device *dev, const char *buf,
				  size_t count, int *periodp)
{
	int j, picosec, period = -1;
	char *endp;

	picosec = simple_strtoul(buf, &endp, 10) * 1000;
	if (*endp == '.') {
		int mult = 100;
		do {
			endp++;
			if (!isdigit(*endp))
				break;
			picosec += (*endp - '0') * mult;
			mult /= 10;
		} while (mult > 0);
	}

	for (j = 0; j <= SPI_STATIC_PPR; j++) {
		if (ppr_to_ps[j] < picosec)
			continue;
		period = j;
		break;
	}

	if (period == -1)
		period = picosec / 4000;

	if (period > 0xff)
		period = 0xff;

	*periodp = period;

	return count;
}

static ssize_t
show_spi_transport_period(struct device *dev,
			  struct device_attribute *attr, char *buf)
{
	struct scsi_target *starget = transport_class_to_starget(dev);
	struct Scsi_Host *shost = dev_to_shost(starget->dev.parent);
	struct spi_internal *i = to_spi_internal(shost->transportt);
	struct spi_transport_attrs *tp =
		(struct spi_transport_attrs *)&starget->starget_data;

	if (i->f->get_period)
		i->f->get_period(starget);

	return show_spi_transport_period_helper(buf, tp->period);
}

static ssize_t
store_spi_transport_period(struct device *cdev, struct device_attribute *attr,
			   const char *buf, size_t count)
{
	struct scsi_target *starget = transport_class_to_starget(cdev);
	struct Scsi_Host *shost = dev_to_shost(starget->dev.parent);
	struct spi_internal *i = to_spi_internal(shost->transportt);
	struct spi_transport_attrs *tp =
		(struct spi_transport_attrs *)&starget->starget_data;
	int period, retval;

	if (!i->f->set_period)
		return -EINVAL;

	retval = store_spi_transport_period_helper(cdev, buf, count, &period);

	if (period < tp->min_period)
		period = tp->min_period;

	i->f->set_period(starget, period);

	return retval;
}

static DEVICE_ATTR(period, S_IRUGO,
		   show_spi_transport_period,
		   store_spi_transport_period);

static ssize_t
show_spi_transport_min_period(struct device *cdev,
			      struct device_attribute *attr, char *buf)
{
	struct scsi_target *starget = transport_class_to_starget(cdev);
	struct Scsi_Host *shost = dev_to_shost(starget->dev.parent);
	struct spi_internal *i = to_spi_internal(shost->transportt);
	struct spi_transport_attrs *tp =
		(struct spi_transport_attrs *)&starget->starget_data;

	if (!i->f->set_period)
		return -EINVAL;

	return show_spi_transport_period_helper(buf, tp->min_period);
}

static ssize_t
store_spi_transport_min_period(struct device *cdev,
			       struct device_attribute *attr,
			       const char *buf, size_t count)
{
	struct scsi_target *starget = transport_class_to_starget(cdev);
	struct spi_transport_attrs *tp =
		(struct spi_transport_attrs *)&starget->starget_data;

	return store_spi_transport_period_helper(cdev, buf, count,
						 &tp->min_period);
}


static DEVICE_ATTR(min_period, S_IRUGO,
		   show_spi_transport_min_period,
		   store_spi_transport_min_period);


static ssize_t show_spi_host_signalling(struct device *cdev,
					struct device_attribute *attr,
					char *buf)
{
	struct Scsi_Host *shost = transport_class_to_shost(cdev);
	struct spi_internal *i = to_spi_internal(shost->transportt);

	if (i->f->get_signalling)
		i->f->get_signalling(shost);

	return sprintf(buf, "%s\n", spi_signal_to_string(spi_signalling(shost)));
}
static ssize_t store_spi_host_signalling(struct device *dev,
					 struct device_attribute *attr,
					 const char *buf, size_t count)
{
	struct Scsi_Host *shost = transport_class_to_shost(dev);
	struct spi_internal *i = to_spi_internal(shost->transportt);
	enum spi_signal_type type = spi_signal_to_value(buf);

	if (!i->f->set_signalling)
		return -EINVAL;

	if (type != SPI_SIGNAL_UNKNOWN)
		i->f->set_signalling(shost, type);

	return count;
}
static DEVICE_ATTR(signalling, S_IRUGO,
		   show_spi_host_signalling,
		   store_spi_host_signalling);

static ssize_t show_spi_host_width(struct device *cdev,
				      struct device_attribute *attr,
				      char *buf)
{
	struct Scsi_Host *shost = transport_class_to_shost(cdev);

	return sprintf(buf, "%s\n", shost->max_id == 16 ? "wide" : "narrow");
}
static DEVICE_ATTR(host_width, S_IRUGO,
		   show_spi_host_width, NULL);

static ssize_t show_spi_host_hba_id(struct device *cdev,
				    struct device_attribute *attr,
				    char *buf)
{
	struct Scsi_Host *shost = transport_class_to_shost(cdev);

	return sprintf(buf, "%d\n", shost->this_id);
}
static DEVICE_ATTR(hba_id, S_IRUGO,
		   show_spi_host_hba_id, NULL);

#define DV_SET(x, y)			\
	if(i->f->set_##x)		\
		i->f->set_##x(sdev->sdev_target, y)

enum spi_compare_returns {
	SPI_COMPARE_SUCCESS,
	SPI_COMPARE_FAILURE,
	SPI_COMPARE_SKIP_TEST,
};


static enum spi_compare_returns
spi_dv_device_echo_buffer(struct scsi_device *sdev, u8 *buffer,
			  u8 *ptr, const int retries)
{
	int len = ptr - buffer;
	int j, k, r, result;
	unsigned int pattern = 0x0000ffff;
	struct scsi_sense_hdr sshdr;

	const char spi_write_buffer[] = {
		WRITE_BUFFER, 0x0a, 0, 0, 0, 0, 0, len >> 8, len & 0xff, 0
	};
	const char spi_read_buffer[] = {
		READ_BUFFER, 0x0a, 0, 0, 0, 0, 0, len >> 8, len & 0xff, 0
	};

	for (j = 0; j < len; ) {

		
		for ( ; j < min(len, 32); j++)
			buffer[j] = j;
		k = j;
		for ( ; j < min(len, k + 32); j += 2) {
			u16 *word = (u16 *)&buffer[j];
			
			*word = (j & 0x02) ? 0x0000 : 0xffff;
		}
		k = j;
		for ( ; j < min(len, k + 32); j += 2) {
			u16 *word = (u16 *)&buffer[j];

			*word = (j & 0x02) ? 0x5555 : 0xaaaa;
		}
		k = j;
		
		for ( ; j < min(len, k + 32); j += 4) {
			u32 *word = (unsigned int *)&buffer[j];
			u32 roll = (pattern & 0x80000000) ? 1 : 0;
			
			*word = pattern;
			pattern = (pattern << 1) | roll;
		}
		
	}

	for (r = 0; r < retries; r++) {
		result = spi_execute(sdev, spi_write_buffer, DMA_TO_DEVICE,
				     buffer, len, &sshdr);
		if(result || !scsi_device_online(sdev)) {

			scsi_device_set_state(sdev, SDEV_QUIESCE);
			if (scsi_sense_valid(&sshdr)
			    && sshdr.sense_key == ILLEGAL_REQUEST
			    
			    && sshdr.asc == 0x24 && sshdr.ascq == 0x00)
				return SPI_COMPARE_SKIP_TEST;


			sdev_printk(KERN_ERR, sdev, "Write Buffer failure %x\n", result);
			return SPI_COMPARE_FAILURE;
		}

		memset(ptr, 0, len);
		spi_execute(sdev, spi_read_buffer, DMA_FROM_DEVICE,
			    ptr, len, NULL);
		scsi_device_set_state(sdev, SDEV_QUIESCE);

		if (memcmp(buffer, ptr, len) != 0)
			return SPI_COMPARE_FAILURE;
	}
	return SPI_COMPARE_SUCCESS;
}

static enum spi_compare_returns
spi_dv_device_compare_inquiry(struct scsi_device *sdev, u8 *buffer,
			      u8 *ptr, const int retries)
{
	int r, result;
	const int len = sdev->inquiry_len;
	const char spi_inquiry[] = {
		INQUIRY, 0, 0, 0, len, 0
	};

	for (r = 0; r < retries; r++) {
		memset(ptr, 0, len);

		result = spi_execute(sdev, spi_inquiry, DMA_FROM_DEVICE,
				     ptr, len, NULL);
		
		if(result || !scsi_device_online(sdev)) {
			scsi_device_set_state(sdev, SDEV_QUIESCE);
			return SPI_COMPARE_FAILURE;
		}

		if (ptr == buffer) {
			ptr += len;
			--r;
			continue;
		}

		if (memcmp(buffer, ptr, len) != 0)
			
			return SPI_COMPARE_FAILURE;
	}
	return SPI_COMPARE_SUCCESS;
}

static enum spi_compare_returns
spi_dv_retrain(struct scsi_device *sdev, u8 *buffer, u8 *ptr,
	       enum spi_compare_returns 
	       (*compare_fn)(struct scsi_device *, u8 *, u8 *, int))
{
	struct spi_internal *i = to_spi_internal(sdev->host->transportt);
	struct scsi_target *starget = sdev->sdev_target;
	int period = 0, prevperiod = 0; 
	enum spi_compare_returns retval;


	for (;;) {
		int newperiod;
		retval = compare_fn(sdev, buffer, ptr, DV_LOOPS);

		if (retval == SPI_COMPARE_SUCCESS
		    || retval == SPI_COMPARE_SKIP_TEST)
			break;

		
		if (i->f->get_iu)
			i->f->get_iu(starget);
		if (i->f->get_qas)
			i->f->get_qas(starget);
		if (i->f->get_period)
			i->f->get_period(sdev->sdev_target);

		if (i->f->set_iu && spi_iu(starget)) {
			starget_printk(KERN_ERR, starget, "Domain Validation Disabing Information Units\n");
			DV_SET(iu, 0);
		} else if (i->f->set_qas && spi_qas(starget)) {
			starget_printk(KERN_ERR, starget, "Domain Validation Disabing Quick Arbitration and Selection\n");
			DV_SET(qas, 0);
		} else {
			newperiod = spi_period(starget);
			period = newperiod > period ? newperiod : period;
			if (period < 0x0d)
				period++;
			else
				period += period >> 1;

			if (unlikely(period > 0xff || period == prevperiod)) {
				
				starget_printk(KERN_ERR, starget, "Domain Validation Failure, dropping back to Asynchronous\n");
				DV_SET(offset, 0);
				return SPI_COMPARE_FAILURE;
			}
			starget_printk(KERN_ERR, starget, "Domain Validation detected failure, dropping back\n");
			DV_SET(period, period);
			prevperiod = period;
		}
	}
	return retval;
}

static int
spi_dv_device_get_echo_buffer(struct scsi_device *sdev, u8 *buffer)
{
	int l, result;

	
	const char spi_test_unit_ready[] = {
		TEST_UNIT_READY, 0, 0, 0, 0, 0
	};

	const char spi_read_buffer_descriptor[] = {
		READ_BUFFER, 0x0b, 0, 0, 0, 0, 0, 0, 4, 0
	};

	
	for (l = 0; ; l++) {
		result = spi_execute(sdev, spi_test_unit_ready, DMA_NONE, 
				     NULL, 0, NULL);

		if(result) {
			if(l >= 3)
				return 0;
		} else {
			
			break;
		}
	}

	result = spi_execute(sdev, spi_read_buffer_descriptor, 
			     DMA_FROM_DEVICE, buffer, 4, NULL);

	if (result)
		
		return 0;

	return buffer[3] + ((buffer[2] & 0x1f) << 8);
}

static void
spi_dv_device_internal(struct scsi_device *sdev, u8 *buffer)
{
	struct spi_internal *i = to_spi_internal(sdev->host->transportt);
	struct scsi_target *starget = sdev->sdev_target;
	struct Scsi_Host *shost = sdev->host;
	int len = sdev->inquiry_len;
	int min_period = spi_min_period(starget);
	int max_width = spi_max_width(starget);
	
	DV_SET(offset, 0);
	DV_SET(width, 0);

	if (spi_dv_device_compare_inquiry(sdev, buffer, buffer, DV_LOOPS)
	    != SPI_COMPARE_SUCCESS) {
		starget_printk(KERN_ERR, starget, "Domain Validation Initial Inquiry Failed\n");
		
		return;
	}

	if (!spi_support_wide(starget)) {
		spi_max_width(starget) = 0;
		max_width = 0;
	}

	
	if (i->f->set_width && max_width) {
		i->f->set_width(starget, 1);

		if (spi_dv_device_compare_inquiry(sdev, buffer,
						   buffer + len,
						   DV_LOOPS)
		    != SPI_COMPARE_SUCCESS) {
			starget_printk(KERN_ERR, starget, "Wide Transfers Fail\n");
			i->f->set_width(starget, 0);
			max_width = 0;
			if (min_period < 10)
				min_period = 10;
		}
	}

	if (!i->f->set_period)
		return;

	
	if (!spi_support_sync(starget) && !spi_support_dt(starget))
		return;

	len = -1;

 retry:

	
	DV_SET(offset, spi_max_offset(starget));
	DV_SET(period, min_period);

	if (spi_support_qas(starget) && spi_max_qas(starget)) {
		DV_SET(qas, 1);
	} else {
		DV_SET(qas, 0);
	}

	if (spi_support_ius(starget) && spi_max_iu(starget) &&
	    min_period < 9) {
		
		DV_SET(iu, 1);
		
		DV_SET(rd_strm, 1);
		DV_SET(wr_flow, 1);
		DV_SET(rti, 1);
		if (min_period == 8)
			DV_SET(pcomp_en, 1);
	} else {
		DV_SET(iu, 0);
	}

	if (i->f->get_signalling)
		i->f->get_signalling(shost);
	if (spi_signalling(shost) == SPI_SIGNAL_SE ||
	    spi_signalling(shost) == SPI_SIGNAL_HVD ||
	    !spi_support_dt(starget)) {
		DV_SET(dt, 0);
	} else {
		DV_SET(dt, 1);
	}
	DV_SET(width, max_width);

	
	spi_dv_retrain(sdev, buffer, buffer + sdev->inquiry_len,
		       spi_dv_device_compare_inquiry);
	
	if (i->f->get_dt)
		i->f->get_dt(starget);


	if (len == -1 && spi_dt(starget))
		len = spi_dv_device_get_echo_buffer(sdev, buffer);

	if (len <= 0) {
		starget_printk(KERN_INFO, starget, "Domain Validation skipping write tests\n");
		return;
	}

	if (len > SPI_MAX_ECHO_BUFFER_SIZE) {
		starget_printk(KERN_WARNING, starget, "Echo buffer size %d is too big, trimming to %d\n", len, SPI_MAX_ECHO_BUFFER_SIZE);
		len = SPI_MAX_ECHO_BUFFER_SIZE;
	}

	if (spi_dv_retrain(sdev, buffer, buffer + len,
			   spi_dv_device_echo_buffer)
	    == SPI_COMPARE_SKIP_TEST) {
		len = 0;
		goto retry;
	}
}


void
spi_dv_device(struct scsi_device *sdev)
{
	struct scsi_target *starget = sdev->sdev_target;
	u8 *buffer;
	const int len = SPI_MAX_ECHO_BUFFER_SIZE*2;

	if (unlikely(scsi_device_get(sdev)))
		return;

	if (unlikely(spi_dv_in_progress(starget)))
		return;
	spi_dv_in_progress(starget) = 1;

	buffer = kzalloc(len, GFP_KERNEL);

	if (unlikely(!buffer))
		goto out_put;

	if (unlikely(scsi_device_quiesce(sdev)))
		goto out_free;

	scsi_target_quiesce(starget);

	spi_dv_pending(starget) = 1;
	mutex_lock(&spi_dv_mutex(starget));

	starget_printk(KERN_INFO, starget, "Beginning Domain Validation\n");

	spi_dv_device_internal(sdev, buffer);

	starget_printk(KERN_INFO, starget, "Ending Domain Validation\n");

	mutex_unlock(&spi_dv_mutex(starget));
	spi_dv_pending(starget) = 0;

	scsi_target_resume(starget);

	spi_initial_dv(starget) = 1;

 out_free:
	kfree(buffer);
 out_put:
	spi_dv_in_progress(starget) = 0;
	scsi_device_put(sdev);
}
EXPORT_SYMBOL(spi_dv_device);

struct work_queue_wrapper {
	struct work_struct	work;
	struct scsi_device	*sdev;
};

static void
spi_dv_device_work_wrapper(struct work_struct *work)
{
	struct work_queue_wrapper *wqw =
		container_of(work, struct work_queue_wrapper, work);
	struct scsi_device *sdev = wqw->sdev;

	kfree(wqw);
	spi_dv_device(sdev);
	spi_dv_pending(sdev->sdev_target) = 0;
	scsi_device_put(sdev);
}


void
spi_schedule_dv_device(struct scsi_device *sdev)
{
	struct work_queue_wrapper *wqw =
		kmalloc(sizeof(struct work_queue_wrapper), GFP_ATOMIC);

	if (unlikely(!wqw))
		return;

	if (unlikely(spi_dv_pending(sdev->sdev_target))) {
		kfree(wqw);
		return;
	}
	
	spi_dv_pending(sdev->sdev_target) = 1;
	if (unlikely(scsi_device_get(sdev))) {
		kfree(wqw);
		spi_dv_pending(sdev->sdev_target) = 0;
		return;
	}

	INIT_WORK(&wqw->work, spi_dv_device_work_wrapper);
	wqw->sdev = sdev;

	schedule_work(&wqw->work);
}
EXPORT_SYMBOL(spi_schedule_dv_device);

void spi_display_xfer_agreement(struct scsi_target *starget)
{
	struct spi_transport_attrs *tp;
	tp = (struct spi_transport_attrs *)&starget->starget_data;

	if (tp->offset > 0 && tp->period > 0) {
		unsigned int picosec, kb100;
		char *scsi = "FAST-?";
		char tmp[8];

		if (tp->period <= SPI_STATIC_PPR) {
			picosec = ppr_to_ps[tp->period];
			switch (tp->period) {
				case  7: scsi = "FAST-320"; break;
				case  8: scsi = "FAST-160"; break;
				case  9: scsi = "FAST-80"; break;
				case 10:
				case 11: scsi = "FAST-40"; break;
				case 12: scsi = "FAST-20"; break;
			}
		} else {
			picosec = tp->period * 4000;
			if (tp->period < 25)
				scsi = "FAST-20";
			else if (tp->period < 50)
				scsi = "FAST-10";
			else
				scsi = "FAST-5";
		}

		kb100 = (10000000 + picosec / 2) / picosec;
		if (tp->width)
			kb100 *= 2;
		sprint_frac(tmp, picosec, 1000);

		dev_info(&starget->dev,
			 "%s %sSCSI %d.%d MB/s %s%s%s%s%s%s%s%s (%s ns, offset %d)\n",
			 scsi, tp->width ? "WIDE " : "", kb100/10, kb100 % 10,
			 tp->dt ? "DT" : "ST",
			 tp->iu ? " IU" : "",
			 tp->qas  ? " QAS" : "",
			 tp->rd_strm ? " RDSTRM" : "",
			 tp->rti ? " RTI" : "",
			 tp->wr_flow ? " WRFLOW" : "",
			 tp->pcomp_en ? " PCOMP" : "",
			 tp->hold_mcs ? " HMCS" : "",
			 tmp, tp->offset);
	} else {
		dev_info(&starget->dev, "%sasynchronous\n",
				tp->width ? "wide " : "");
	}
}
EXPORT_SYMBOL(spi_display_xfer_agreement);

int spi_populate_width_msg(unsigned char *msg, int width)
{
	msg[0] = EXTENDED_MESSAGE;
	msg[1] = 2;
	msg[2] = EXTENDED_WDTR;
	msg[3] = width;
	return 4;
}
EXPORT_SYMBOL_GPL(spi_populate_width_msg);

int spi_populate_sync_msg(unsigned char *msg, int period, int offset)
{
	msg[0] = EXTENDED_MESSAGE;
	msg[1] = 3;
	msg[2] = EXTENDED_SDTR;
	msg[3] = period;
	msg[4] = offset;
	return 5;
}
EXPORT_SYMBOL_GPL(spi_populate_sync_msg);

int spi_populate_ppr_msg(unsigned char *msg, int period, int offset,
		int width, int options)
{
	msg[0] = EXTENDED_MESSAGE;
	msg[1] = 6;
	msg[2] = EXTENDED_PPR;
	msg[3] = period;
	msg[4] = 0;
	msg[5] = offset;
	msg[6] = width;
	msg[7] = options;
	return 8;
}
EXPORT_SYMBOL_GPL(spi_populate_ppr_msg);

#ifdef CONFIG_SCSI_CONSTANTS
static const char * const one_byte_msgs[] = {
 "Task Complete", NULL , "Save Pointers",
 "Restore Pointers", "Disconnect", "Initiator Error", 
 "Abort Task Set", "Message Reject", "Nop", "Message Parity Error",
 "Linked Command Complete", "Linked Command Complete w/flag",
 "Target Reset", "Abort Task", "Clear Task Set", 
 "Initiate Recovery", "Release Recovery",
 "Terminate Process", "Continue Task", "Target Transfer Disable",
 NULL, NULL, "Clear ACA", "LUN Reset"
};

static const char * const two_byte_msgs[] = {
 "Simple Queue Tag", "Head of Queue Tag", "Ordered Queue Tag",
 "Ignore Wide Residue", "ACA"
};

static const char * const extended_msgs[] = {
 "Modify Data Pointer", "Synchronous Data Transfer Request",
 "SCSI-I Extended Identify", "Wide Data Transfer Request",
 "Parallel Protocol Request", "Modify Bidirectional Data Pointer"
};

static void print_nego(const unsigned char *msg, int per, int off, int width)
{
	if (per) {
		char buf[20];
		period_to_str(buf, msg[per]);
		printk("period = %s ns ", buf);
	}

	if (off)
		printk("offset = %d ", msg[off]);
	if (width)
		printk("width = %d ", 8 << msg[width]);
}

static void print_ptr(const unsigned char *msg, int msb, const char *desc)
{
	int ptr = (msg[msb] << 24) | (msg[msb+1] << 16) | (msg[msb+2] << 8) |
			msg[msb+3];
	printk("%s = %d ", desc, ptr);
}

int spi_print_msg(const unsigned char *msg)
{
	int len = 1, i;
	if (msg[0] == EXTENDED_MESSAGE) {
		len = 2 + msg[1];
		if (len == 2)
			len += 256;
		if (msg[2] < ARRAY_SIZE(extended_msgs))
			printk ("%s ", extended_msgs[msg[2]]); 
		else 
			printk ("Extended Message, reserved code (0x%02x) ",
				(int) msg[2]);
		switch (msg[2]) {
		case EXTENDED_MODIFY_DATA_POINTER:
			print_ptr(msg, 3, "pointer");
			break;
		case EXTENDED_SDTR:
			print_nego(msg, 3, 4, 0);
			break;
		case EXTENDED_WDTR:
			print_nego(msg, 0, 0, 3);
			break;
		case EXTENDED_PPR:
			print_nego(msg, 3, 5, 6);
			break;
		case EXTENDED_MODIFY_BIDI_DATA_PTR:
			print_ptr(msg, 3, "out");
			print_ptr(msg, 7, "in");
			break;
		default:
		for (i = 2; i < len; ++i) 
			printk("%02x ", msg[i]);
		}
	
	} else if (msg[0] & 0x80) {
		printk("Identify disconnect %sallowed %s %d ",
			(msg[0] & 0x40) ? "" : "not ",
			(msg[0] & 0x20) ? "target routine" : "lun",
			msg[0] & 0x7);
	
	} else if (msg[0] < 0x1f) {
		if (msg[0] < ARRAY_SIZE(one_byte_msgs) && one_byte_msgs[msg[0]])
			printk("%s ", one_byte_msgs[msg[0]]);
		else
			printk("reserved (%02x) ", msg[0]);
	} else if (msg[0] == 0x55) {
		printk("QAS Request ");
	
	} else if (msg[0] <= 0x2f) {
		if ((msg[0] - 0x20) < ARRAY_SIZE(two_byte_msgs))
			printk("%s %02x ", two_byte_msgs[msg[0] - 0x20], 
				msg[1]);
		else 
			printk("reserved two byte (%02x %02x) ", 
				msg[0], msg[1]);
		len = 2;
	} else 
		printk("reserved ");
	return len;
}
EXPORT_SYMBOL(spi_print_msg);

#else  

int spi_print_msg(const unsigned char *msg)
{
	int len = 1, i;

	if (msg[0] == EXTENDED_MESSAGE) {
		len = 2 + msg[1];
		if (len == 2)
			len += 256;
		for (i = 0; i < len; ++i)
			printk("%02x ", msg[i]);
	
	} else if (msg[0] & 0x80) {
		printk("%02x ", msg[0]);
	
	} else if ((msg[0] < 0x1f) || (msg[0] == 0x55)) {
		printk("%02x ", msg[0]);
	
	} else if (msg[0] <= 0x2f) {
		printk("%02x %02x", msg[0], msg[1]);
		len = 2;
	} else 
		printk("%02x ", msg[0]);
	return len;
}
EXPORT_SYMBOL(spi_print_msg);
#endif 

static int spi_device_match(struct attribute_container *cont,
			    struct device *dev)
{
	struct scsi_device *sdev;
	struct Scsi_Host *shost;
	struct spi_internal *i;

	if (!scsi_is_sdev_device(dev))
		return 0;

	sdev = to_scsi_device(dev);
	shost = sdev->host;
	if (!shost->transportt  || shost->transportt->host_attrs.ac.class
	    != &spi_host_class.class)
		return 0;
	i = to_spi_internal(shost->transportt);
	if (i->f->deny_binding && i->f->deny_binding(sdev->sdev_target))
		return 0;
	return 1;
}

static int spi_target_match(struct attribute_container *cont,
			    struct device *dev)
{
	struct Scsi_Host *shost;
	struct scsi_target *starget;
	struct spi_internal *i;

	if (!scsi_is_target_device(dev))
		return 0;

	shost = dev_to_shost(dev->parent);
	if (!shost->transportt  || shost->transportt->host_attrs.ac.class
	    != &spi_host_class.class)
		return 0;

	i = to_spi_internal(shost->transportt);
	starget = to_scsi_target(dev);

	if (i->f->deny_binding && i->f->deny_binding(starget))
		return 0;

	return &i->t.target_attrs.ac == cont;
}

static DECLARE_TRANSPORT_CLASS(spi_transport_class,
			       "spi_transport",
			       spi_setup_transport_attrs,
			       NULL,
			       spi_target_configure);

static DECLARE_ANON_TRANSPORT_CLASS(spi_device_class,
				    spi_device_match,
				    spi_device_configure);

static struct attribute *host_attributes[] = {
	&dev_attr_signalling.attr,
	&dev_attr_host_width.attr,
	&dev_attr_hba_id.attr,
	NULL
};

static struct attribute_group host_attribute_group = {
	.attrs = host_attributes,
};

static int spi_host_configure(struct transport_container *tc,
			      struct device *dev,
			      struct device *cdev)
{
	struct kobject *kobj = &cdev->kobj;
	struct Scsi_Host *shost = transport_class_to_shost(cdev);
	struct spi_internal *si = to_spi_internal(shost->transportt);
	struct attribute *attr = &dev_attr_signalling.attr;
	int rc = 0;

	if (si->f->set_signalling)
		rc = sysfs_chmod_file(kobj, attr, attr->mode | S_IWUSR);

	return rc;
}

#define TARGET_ATTRIBUTE_HELPER(name) \
	(si->f->show_##name ? S_IRUGO : 0) | \
	(si->f->set_##name ? S_IWUSR : 0)

static umode_t target_attribute_is_visible(struct kobject *kobj,
					  struct attribute *attr, int i)
{
	struct device *cdev = container_of(kobj, struct device, kobj);
	struct scsi_target *starget = transport_class_to_starget(cdev);
	struct Scsi_Host *shost = transport_class_to_shost(cdev);
	struct spi_internal *si = to_spi_internal(shost->transportt);

	if (attr == &dev_attr_period.attr &&
	    spi_support_sync(starget))
		return TARGET_ATTRIBUTE_HELPER(period);
	else if (attr == &dev_attr_min_period.attr &&
		 spi_support_sync(starget))
		return TARGET_ATTRIBUTE_HELPER(period);
	else if (attr == &dev_attr_offset.attr &&
		 spi_support_sync(starget))
		return TARGET_ATTRIBUTE_HELPER(offset);
	else if (attr == &dev_attr_max_offset.attr &&
		 spi_support_sync(starget))
		return TARGET_ATTRIBUTE_HELPER(offset);
	else if (attr == &dev_attr_width.attr &&
		 spi_support_wide(starget))
		return TARGET_ATTRIBUTE_HELPER(width);
	else if (attr == &dev_attr_max_width.attr &&
		 spi_support_wide(starget))
		return TARGET_ATTRIBUTE_HELPER(width);
	else if (attr == &dev_attr_iu.attr &&
		 spi_support_ius(starget))
		return TARGET_ATTRIBUTE_HELPER(iu);
	else if (attr == &dev_attr_max_iu.attr &&
		 spi_support_ius(starget))
		return TARGET_ATTRIBUTE_HELPER(iu);
	else if (attr == &dev_attr_dt.attr &&
		 spi_support_dt(starget))
		return TARGET_ATTRIBUTE_HELPER(dt);
	else if (attr == &dev_attr_qas.attr &&
		 spi_support_qas(starget))
		return TARGET_ATTRIBUTE_HELPER(qas);
	else if (attr == &dev_attr_max_qas.attr &&
		 spi_support_qas(starget))
		return TARGET_ATTRIBUTE_HELPER(qas);
	else if (attr == &dev_attr_wr_flow.attr &&
		 spi_support_ius(starget))
		return TARGET_ATTRIBUTE_HELPER(wr_flow);
	else if (attr == &dev_attr_rd_strm.attr &&
		 spi_support_ius(starget))
		return TARGET_ATTRIBUTE_HELPER(rd_strm);
	else if (attr == &dev_attr_rti.attr &&
		 spi_support_ius(starget))
		return TARGET_ATTRIBUTE_HELPER(rti);
	else if (attr == &dev_attr_pcomp_en.attr &&
		 spi_support_ius(starget))
		return TARGET_ATTRIBUTE_HELPER(pcomp_en);
	else if (attr == &dev_attr_hold_mcs.attr &&
		 spi_support_ius(starget))
		return TARGET_ATTRIBUTE_HELPER(hold_mcs);
	else if (attr == &dev_attr_revalidate.attr)
		return S_IWUSR;

	return 0;
}

static struct attribute *target_attributes[] = {
	&dev_attr_period.attr,
	&dev_attr_min_period.attr,
	&dev_attr_offset.attr,
	&dev_attr_max_offset.attr,
	&dev_attr_width.attr,
	&dev_attr_max_width.attr,
	&dev_attr_iu.attr,
	&dev_attr_max_iu.attr,
	&dev_attr_dt.attr,
	&dev_attr_qas.attr,
	&dev_attr_max_qas.attr,
	&dev_attr_wr_flow.attr,
	&dev_attr_rd_strm.attr,
	&dev_attr_rti.attr,
	&dev_attr_pcomp_en.attr,
	&dev_attr_hold_mcs.attr,
	&dev_attr_revalidate.attr,
	NULL
};

static struct attribute_group target_attribute_group = {
	.attrs = target_attributes,
	.is_visible = target_attribute_is_visible,
};

static int spi_target_configure(struct transport_container *tc,
				struct device *dev,
				struct device *cdev)
{
	struct kobject *kobj = &cdev->kobj;

	
	sysfs_update_group(kobj, &target_attribute_group);

	return 0;
}

struct scsi_transport_template *
spi_attach_transport(struct spi_function_template *ft)
{
	struct spi_internal *i = kzalloc(sizeof(struct spi_internal),
					 GFP_KERNEL);

	if (unlikely(!i))
		return NULL;

	i->t.target_attrs.ac.class = &spi_transport_class.class;
	i->t.target_attrs.ac.grp = &target_attribute_group;
	i->t.target_attrs.ac.match = spi_target_match;
	transport_container_register(&i->t.target_attrs);
	i->t.target_size = sizeof(struct spi_transport_attrs);
	i->t.host_attrs.ac.class = &spi_host_class.class;
	i->t.host_attrs.ac.grp = &host_attribute_group;
	i->t.host_attrs.ac.match = spi_host_match;
	transport_container_register(&i->t.host_attrs);
	i->t.host_size = sizeof(struct spi_host_attrs);
	i->f = ft;

	return &i->t;
}
EXPORT_SYMBOL(spi_attach_transport);

void spi_release_transport(struct scsi_transport_template *t)
{
	struct spi_internal *i = to_spi_internal(t);

	transport_container_unregister(&i->t.target_attrs);
	transport_container_unregister(&i->t.host_attrs);

	kfree(i);
}
EXPORT_SYMBOL(spi_release_transport);

static __init int spi_transport_init(void)
{
	int error = scsi_dev_info_add_list(SCSI_DEVINFO_SPI,
					   "SCSI Parallel Transport Class");
	if (!error) {
		int i;

		for (i = 0; spi_static_device_list[i].vendor; i++)
			scsi_dev_info_list_add_keyed(1,	
						     spi_static_device_list[i].vendor,
						     spi_static_device_list[i].model,
						     NULL,
						     spi_static_device_list[i].flags,
						     SCSI_DEVINFO_SPI);
	}

	error = transport_class_register(&spi_transport_class);
	if (error)
		return error;
	error = anon_transport_class_register(&spi_device_class);
	return transport_class_register(&spi_host_class);
}

static void __exit spi_transport_exit(void)
{
	transport_class_unregister(&spi_transport_class);
	anon_transport_class_unregister(&spi_device_class);
	transport_class_unregister(&spi_host_class);
	scsi_dev_info_remove_list(SCSI_DEVINFO_SPI);
}

MODULE_AUTHOR("Martin Hicks");
MODULE_DESCRIPTION("SPI Transport Attributes");
MODULE_LICENSE("GPL");

module_init(spi_transport_init);
module_exit(spi_transport_exit);
