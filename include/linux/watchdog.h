
#ifndef _LINUX_WATCHDOG_H
#define _LINUX_WATCHDOG_H

#include <linux/ioctl.h>
#include <linux/types.h>

#define	WATCHDOG_IOCTL_BASE	'W'

struct watchdog_info {
	__u32 options;		
	__u32 firmware_version;	
	__u8  identity[32];	
};

#define	WDIOC_GETSUPPORT	_IOR(WATCHDOG_IOCTL_BASE, 0, struct watchdog_info)
#define	WDIOC_GETSTATUS		_IOR(WATCHDOG_IOCTL_BASE, 1, int)
#define	WDIOC_GETBOOTSTATUS	_IOR(WATCHDOG_IOCTL_BASE, 2, int)
#define	WDIOC_GETTEMP		_IOR(WATCHDOG_IOCTL_BASE, 3, int)
#define	WDIOC_SETOPTIONS	_IOR(WATCHDOG_IOCTL_BASE, 4, int)
#define	WDIOC_KEEPALIVE		_IOR(WATCHDOG_IOCTL_BASE, 5, int)
#define	WDIOC_SETTIMEOUT        _IOWR(WATCHDOG_IOCTL_BASE, 6, int)
#define	WDIOC_GETTIMEOUT        _IOR(WATCHDOG_IOCTL_BASE, 7, int)
#define	WDIOC_SETPRETIMEOUT	_IOWR(WATCHDOG_IOCTL_BASE, 8, int)
#define	WDIOC_GETPRETIMEOUT	_IOR(WATCHDOG_IOCTL_BASE, 9, int)
#define	WDIOC_GETTIMELEFT	_IOR(WATCHDOG_IOCTL_BASE, 10, int)

#define	WDIOF_UNKNOWN		-1	
#define	WDIOS_UNKNOWN		-1	

#define	WDIOF_OVERHEAT		0x0001	
#define	WDIOF_FANFAULT		0x0002	
#define	WDIOF_EXTERN1		0x0004	
#define	WDIOF_EXTERN2		0x0008	
#define	WDIOF_POWERUNDER	0x0010	
#define	WDIOF_CARDRESET		0x0020	
#define	WDIOF_POWEROVER		0x0040	
#define	WDIOF_SETTIMEOUT	0x0080  
#define	WDIOF_MAGICCLOSE	0x0100	
#define	WDIOF_PRETIMEOUT	0x0200  
#define	WDIOF_KEEPALIVEPING	0x8000	

#define	WDIOS_DISABLECARD	0x0001	
#define	WDIOS_ENABLECARD	0x0002	
#define	WDIOS_TEMPPANIC		0x0004	

#ifdef __KERNEL__

#include <linux/bitops.h>

struct watchdog_ops;
struct watchdog_device;

struct watchdog_ops {
	struct module *owner;
	
	int (*start)(struct watchdog_device *);
	int (*stop)(struct watchdog_device *);
	
	int (*ping)(struct watchdog_device *);
	unsigned int (*status)(struct watchdog_device *);
	int (*set_timeout)(struct watchdog_device *, unsigned int);
	unsigned int (*get_timeleft)(struct watchdog_device *);
	long (*ioctl)(struct watchdog_device *, unsigned int, unsigned long);
};

struct watchdog_device {
	const struct watchdog_info *info;
	const struct watchdog_ops *ops;
	unsigned int bootstatus;
	unsigned int timeout;
	unsigned int min_timeout;
	unsigned int max_timeout;
	void *driver_data;
	unsigned long status;
#define WDOG_ACTIVE		0	
#define WDOG_DEV_OPEN		1	
#define WDOG_ALLOW_RELEASE	2	
#define WDOG_NO_WAY_OUT		3	
};

#ifdef CONFIG_WATCHDOG_NOWAYOUT
#define WATCHDOG_NOWAYOUT		1
#define WATCHDOG_NOWAYOUT_INIT_STATUS	(1 << WDOG_NO_WAY_OUT)
#else
#define WATCHDOG_NOWAYOUT		0
#define WATCHDOG_NOWAYOUT_INIT_STATUS	0
#endif

static inline void watchdog_set_nowayout(struct watchdog_device *wdd, bool nowayout)
{
	if (nowayout)
		set_bit(WDOG_NO_WAY_OUT, &wdd->status);
}

static inline void watchdog_set_drvdata(struct watchdog_device *wdd, void *data)
{
	wdd->driver_data = data;
}

static inline void *watchdog_get_drvdata(struct watchdog_device *wdd)
{
	return wdd->driver_data;
}

extern int watchdog_register_device(struct watchdog_device *);
extern void watchdog_unregister_device(struct watchdog_device *);

#endif	

#endif  
