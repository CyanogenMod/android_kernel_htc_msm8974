
/*
 * Copyright (c) 2002 by David Brownell
 * Copyright (c) 2010 by Samsung Electronics
 * Author: Michal Nazarewicz <mina86@mina86.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */


#include <stdio.h>
#include <string.h>
#include <ftw.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <sys/ioctl.h>
#include <linux/usbdevice_fs.h>


#define	TEST_CASES	30


struct usbtest_param {
	
	unsigned		test_num;	
	unsigned		iterations;
	unsigned		length;
	unsigned		vary;
	unsigned		sglen;

	
	struct timeval		duration;
};
#define USBTEST_REQUEST	_IOWR('U', 100, struct usbtest_param)



#define USB_DT_DEVICE			0x01
#define USB_DT_INTERFACE		0x04

#define USB_CLASS_PER_INTERFACE		0	
#define USB_CLASS_VENDOR_SPEC		0xff


struct usb_device_descriptor {
	__u8  bLength;
	__u8  bDescriptorType;
	__u16 bcdUSB;
	__u8  bDeviceClass;
	__u8  bDeviceSubClass;
	__u8  bDeviceProtocol;
	__u8  bMaxPacketSize0;
	__u16 idVendor;
	__u16 idProduct;
	__u16 bcdDevice;
	__u8  iManufacturer;
	__u8  iProduct;
	__u8  iSerialNumber;
	__u8  bNumConfigurations;
} __attribute__ ((packed));

struct usb_interface_descriptor {
	__u8  bLength;
	__u8  bDescriptorType;

	__u8  bInterfaceNumber;
	__u8  bAlternateSetting;
	__u8  bNumEndpoints;
	__u8  bInterfaceClass;
	__u8  bInterfaceSubClass;
	__u8  bInterfaceProtocol;
	__u8  iInterface;
} __attribute__ ((packed));

enum usb_device_speed {
	USB_SPEED_UNKNOWN = 0,			
	USB_SPEED_LOW, USB_SPEED_FULL,		
	USB_SPEED_HIGH				
};


static char *speed (enum usb_device_speed s)
{
	switch (s) {
	case USB_SPEED_UNKNOWN:	return "unknown";
	case USB_SPEED_LOW:	return "low";
	case USB_SPEED_FULL:	return "full";
	case USB_SPEED_HIGH:	return "high";
	default:		return "??";
	}
}

struct testdev {
	struct testdev		*next;
	char			*name;
	pthread_t		thread;
	enum usb_device_speed	speed;
	unsigned		ifnum : 8;
	unsigned		forever : 1;
	int			test;

	struct usbtest_param	param;
};
static struct testdev		*testdevs;

static int testdev_ffs_ifnum(FILE *fd)
{
	union {
		char buf[255];
		struct usb_interface_descriptor intf;
	} u;

	for (;;) {
		if (fread(u.buf, 1, 1, fd) != 1)
			return -1;
		if (fread(u.buf + 1, (unsigned char)u.buf[0] - 1, 1, fd) != 1)
			return -1;

		if (u.intf.bLength == sizeof u.intf
		 && u.intf.bDescriptorType == USB_DT_INTERFACE
		 && u.intf.bNumEndpoints == 2
		 && u.intf.bInterfaceClass == USB_CLASS_VENDOR_SPEC
		 && u.intf.bInterfaceSubClass == 0
		 && u.intf.bInterfaceProtocol == 0)
			return (unsigned char)u.intf.bInterfaceNumber;
	}
}

static int testdev_ifnum(FILE *fd)
{
	struct usb_device_descriptor dev;

	if (fread(&dev, sizeof dev, 1, fd) != 1)
		return -1;

	if (dev.bLength != sizeof dev || dev.bDescriptorType != USB_DT_DEVICE)
		return -1;

	
	if (dev.idVendor == 0x0547 && dev.idProduct == 0x1002)
		return 0;

	


	
	if (dev.idVendor == 0x0547 && dev.idProduct == 0x2235)
		return 0;

	
	if (dev.idVendor == 0x04b4 && dev.idProduct == 0x8613)
		return 0;

	
	if (dev.idVendor == 0x0547 && dev.idProduct == 0x0080)
		return 0;

	
	if (dev.idVendor == 0x06cd && dev.idProduct == 0x010b)
		return 0;

	

	
	if (dev.idVendor == 0x0525 && dev.idProduct == 0xa4a0)
		return 0;

	
	if (dev.idVendor == 0x0525 && dev.idProduct == 0xa4a4)
		return testdev_ffs_ifnum(fd);
		

	
	if (dev.idVendor == 0x0525 && dev.idProduct == 0xa4a3)
		return 0;

	/* some GPL'd test firmware uses these IDs */

	if (dev.idVendor == 0xfff0 && dev.idProduct == 0xfff0)
		return 0;

	

	
	if (dev.idVendor == 0x0b62 && dev.idProduct == 0x0059)
		return 0;

	


	if (dev.idVendor == 0x0525 && dev.idProduct == 0xa4ac
	 && (dev.bDeviceClass == USB_CLASS_PER_INTERFACE
	  || dev.bDeviceClass == USB_CLASS_VENDOR_SPEC))
		return testdev_ffs_ifnum(fd);

	return -1;
}

static int find_testdev(const char *name, const struct stat *sb, int flag)
{
	FILE				*fd;
	int				ifnum;
	struct testdev			*entry;

	(void)sb; 

	if (flag != FTW_F)
		return 0;
	
	if (strrchr(name, '/')[1] == 'd')
		return 0;

	fd = fopen(name, "rb");
	if (!fd) {
		perror(name);
		return 0;
	}

	ifnum = testdev_ifnum(fd);
	fclose(fd);
	if (ifnum < 0)
		return 0;

	entry = calloc(1, sizeof *entry);
	if (!entry)
		goto nomem;

	entry->name = strdup(name);
	if (!entry->name) {
		free(entry);
nomem:
		perror("malloc");
		return 0;
	}

	entry->ifnum = ifnum;


	fprintf(stderr, "%s speed\t%s\t%u\n",
		speed(entry->speed), entry->name, entry->ifnum);

	entry->next = testdevs;
	testdevs = entry;
	return 0;
}

static int
usbdev_ioctl (int fd, int ifno, unsigned request, void *param)
{
	struct usbdevfs_ioctl	wrapper;

	wrapper.ifno = ifno;
	wrapper.ioctl_code = request;
	wrapper.data = param;

	return ioctl (fd, USBDEVFS_IOCTL, &wrapper);
}

static void *handle_testdev (void *arg)
{
	struct testdev		*dev = arg;
	int			fd, i;
	int			status;

	if ((fd = open (dev->name, O_RDWR)) < 0) {
		perror ("can't open dev file r/w");
		return 0;
	}

restart:
	for (i = 0; i < TEST_CASES; i++) {
		if (dev->test != -1 && dev->test != i)
			continue;
		dev->param.test_num = i;

		status = usbdev_ioctl (fd, dev->ifnum,
				USBTEST_REQUEST, &dev->param);
		if (status < 0 && errno == EOPNOTSUPP)
			continue;

		

		
		if (status < 0) {
			char	buf [80];
			int	err = errno;

			if (strerror_r (errno, buf, sizeof buf)) {
				snprintf (buf, sizeof buf, "error %d", err);
				errno = err;
			}
			printf ("%s test %d --> %d (%s)\n",
				dev->name, i, errno, buf);
		} else
			printf ("%s test %d, %4d.%.06d secs\n", dev->name, i,
				(int) dev->param.duration.tv_sec,
				(int) dev->param.duration.tv_usec);

		fflush (stdout);
	}
	if (dev->forever)
		goto restart;

	close (fd);
	return arg;
}

static const char *usbfs_dir_find(void)
{
	static char usbfs_path_0[] = "/dev/usb/devices";
	static char usbfs_path_1[] = "/proc/bus/usb/devices";

	static char *const usbfs_paths[] = {
		usbfs_path_0, usbfs_path_1
	};

	static char *const *
		end = usbfs_paths + sizeof usbfs_paths / sizeof *usbfs_paths;

	char *const *it = usbfs_paths;
	do {
		int fd = open(*it, O_RDONLY);
		close(fd);
		if (fd >= 0) {
			strrchr(*it, '/')[0] = '\0';
			return *it;
		}
	} while (++it != end);

	return NULL;
}

static int parse_num(unsigned *num, const char *str)
{
	unsigned long val;
	char *end;

	errno = 0;
	val = strtoul(str, &end, 0);
	if (errno || *end || val > UINT_MAX)
		return -1;
	*num = val;
	return 0;
}

int main (int argc, char **argv)
{

	int			c;
	struct testdev		*entry;
	char			*device;
	const char		*usbfs_dir = NULL;
	int			all = 0, forever = 0, not = 0;
	int			test = -1 ;
	struct usbtest_param	param;

	param.iterations = 1000;
	param.length = 512;
	param.vary = 512;
	param.sglen = 32;

	
	device = getenv ("DEVICE");

	while ((c = getopt (argc, argv, "D:aA:c:g:hns:t:v:")) != EOF)
	switch (c) {
	case 'D':	
		device = optarg;
		continue;
	case 'A':	
		usbfs_dir = optarg;
		
	case 'a':	
		device = NULL;
		all = 1;
		continue;
	case 'c':	
		if (parse_num(&param.iterations, optarg))
			goto usage;
		continue;
	case 'g':	
		if (parse_num(&param.sglen, optarg))
			goto usage;
		continue;
	case 'l':	
		forever = 1;
		continue;
	case 'n':	
		not = 1;
		continue;
	case 's':	
		if (parse_num(&param.length, optarg))
			goto usage;
		continue;
	case 't':	
		test = atoi (optarg);
		if (test < 0)
			goto usage;
		continue;
	case 'v':	
		if (parse_num(&param.vary, optarg))
			goto usage;
		continue;
	case '?':
	case 'h':
	default:
usage:
		fprintf (stderr, "usage: %s [-n] [-D dev | -a | -A usbfs-dir]\n"
			"\t[-c iterations]  [-t testnum]\n"
			"\t[-s packetsize] [-g sglen] [-v vary]\n",
			argv [0]);
		return 1;
	}
	if (optind != argc)
		goto usage;
	if (!all && !device) {
		fprintf (stderr, "must specify '-a' or '-D dev', "
			"or DEVICE=/proc/bus/usb/BBB/DDD in env\n");
		goto usage;
	}

	
	if (!usbfs_dir) {
		usbfs_dir = usbfs_dir_find();
		if (!usbfs_dir) {
			fputs ("usbfs files are missing\n", stderr);
			return -1;
		}
	}

	
	if (ftw (usbfs_dir, find_testdev, 3) != 0) {
		fputs ("ftw failed; is usbfs missing?\n", stderr);
		return -1;
	}

	
	if (!testdevs && !device) {
		fputs ("no test devices recognized\n", stderr);
		return -1;
	}
	if (not)
		return 0;
	if (testdevs && testdevs->next == 0 && !device)
		device = testdevs->name;
	for (entry = testdevs; entry; entry = entry->next) {
		int	status;

		entry->param = param;
		entry->forever = forever;
		entry->test = test;

		if (device) {
			if (strcmp (entry->name, device))
				continue;
			return handle_testdev (entry) != entry;
		}
		status = pthread_create (&entry->thread, 0, handle_testdev, entry);
		if (status) {
			perror ("pthread_create");
			continue;
		}
	}
	if (device) {
		struct testdev		dev;

		
		fprintf (stderr, "%s: %s may see only control tests\n",
				argv [0], device);

		memset (&dev, 0, sizeof dev);
		dev.name = device;
		dev.param = param;
		dev.forever = forever;
		dev.test = test;
		return handle_testdev (&dev) != &dev;
	}

	
	for (entry = testdevs; entry; entry = entry->next) {
		void	*retval;

		if (pthread_join (entry->thread, &retval))
			perror ("pthread_join");
		
	}

	return 0;
}
