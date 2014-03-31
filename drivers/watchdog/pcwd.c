/*
 * PC Watchdog Driver
 * by Ken Hollis (khollis@bitgate.com)
 *
 * Permission granted from Simon Machell (smachell@berkprod.com)
 * Written for the Linux Kernel, and GPLed by Ken Hollis
 *
 * 960107	Added request_region routines, modulized the whole thing.
 * 960108	Fixed end-of-file pointer (Thanks to Dan Hollis), added
 *		WD_TIMEOUT define.
 * 960216	Added eof marker on the file, and changed verbose messages.
 * 960716	Made functional and cosmetic changes to the source for
 *		inclusion in Linux 2.0.x kernels, thanks to Alan Cox.
 * 960717	Removed read/seek routines, replaced with ioctl.  Also, added
 *		check_region command due to Alan's suggestion.
 * 960821	Made changes to compile in newer 2.0.x kernels.  Added
 *		"cold reboot sense" entry.
 * 960825	Made a few changes to code, deleted some defines and made
 *		typedefs to replace them.  Made heartbeat reset only available
 *		via ioctl, and removed the write routine.
 * 960828	Added new items for PC Watchdog Rev.C card.
 * 960829	Changed around all of the IOCTLs, added new features,
 *		added watchdog disable/re-enable routines.  Added firmware
 *		version reporting.  Added read routine for temperature.
 *		Removed some extra defines, added an autodetect Revision
 *		routine.
 * 961006	Revised some documentation, fixed some cosmetic bugs.  Made
 *		drivers to panic the system if it's overheating at bootup.
 * 961118	Changed some verbiage on some of the output, tidied up
 *		code bits, and added compatibility to 2.1.x.
 * 970912	Enabled board on open and disable on close.
 * 971107	Took account of recent VFS changes (broke read).
 * 971210	Disable board on initialisation in case board already ticking.
 * 971222	Changed open/close for temperature handling
 *		Michael Meskes <meskes@debian.org>.
 * 980112	Used minor numbers from include/linux/miscdevice.h
 * 990403	Clear reset status after reading control status register in
 *		pcwd_showprevstate(). [Marc Boucher <marc@mbsi.ca>]
 * 990605	Made changes to code to support Firmware 1.22a, added
 *		fairly useless proc entry.
 * 990610	removed said useless proc code for the merge <alan>
 * 000403	Removed last traces of proc code. <davej>
 * 011214	Added nowayout module option to override
 *		CONFIG_WATCHDOG_NOWAYOUT <Matt_Domsch@dell.com>
 *		Added timeout module option to override default
 */


#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>	
#include <linux/moduleparam.h>	
#include <linux/types.h>	
#include <linux/errno.h>	
#include <linux/kernel.h>	
#include <linux/delay.h>	
#include <linux/timer.h>	
#include <linux/jiffies.h>	
#include <linux/miscdevice.h>	
#include <linux/watchdog.h>	
#include <linux/reboot.h>	
#include <linux/init.h>		
#include <linux/fs.h>		
#include <linux/isa.h>		
#include <linux/ioport.h>	
#include <linux/spinlock.h>	
#include <linux/uaccess.h>	
#include <linux/io.h>		

#define WATCHDOG_VERSION "1.20"
#define WATCHDOG_DATE "18 Feb 2007"
#define WATCHDOG_DRIVER_NAME "ISA-PC Watchdog"
#define WATCHDOG_NAME "pcwd"
#define DRIVER_VERSION WATCHDOG_DRIVER_NAME " driver, v" WATCHDOG_VERSION "\n"

#define	PCWD_REVISION_A		1
#define	PCWD_REVISION_C		2

#define PCWD_ISA_NR_CARDS	3
static int pcwd_ioports[] = { 0x270, 0x350, 0x370, 0x000 };

#define WD_WDRST		0x01	
#define WD_T110			0x02	
#define WD_HRTBT		0x04	
#define WD_RLY2			0x08	
#define WD_SRLY2		0x80	
#define WD_REVC_WTRP		0x01	
#define WD_REVC_HRBT		0x02	
#define WD_REVC_TTRP		0x04	
#define WD_REVC_RL2A		0x08	
#define WD_REVC_RL1A		0x10	
#define WD_REVC_R2DS		0x40	
#define WD_REVC_RLY2		0x80	
#define WD_WDIS			0x10	
#define WD_ENTP			0x20	
#define WD_SSEL			0x40	
#define WD_WCMD			0x80	

#define ISA_COMMAND_TIMEOUT     1000

#define CMD_ISA_IDLE			0x00
#define CMD_ISA_VERSION_INTEGER		0x01
#define CMD_ISA_VERSION_TENTH		0x02
#define CMD_ISA_VERSION_HUNDRETH	0x03
#define CMD_ISA_VERSION_MINOR		0x04
#define CMD_ISA_SWITCH_SETTINGS		0x05
#define CMD_ISA_RESET_PC		0x06
#define CMD_ISA_ARM_0			0x07
#define CMD_ISA_ARM_30			0x08
#define CMD_ISA_ARM_60			0x09
#define CMD_ISA_DELAY_TIME_2SECS	0x0A
#define CMD_ISA_DELAY_TIME_4SECS	0x0B
#define CMD_ISA_DELAY_TIME_8SECS	0x0C
#define CMD_ISA_RESET_RELAYS		0x0D

static const int heartbeat_tbl[] = {
	20,	
	40,	
	60,	
	300,	
	600,	
	1800,	
	3600,	
	7200,	
};


#define WDT_INTERVAL (HZ/2+1)

static int cards_found;

static unsigned long open_allowed;
static char expect_close;
static int temp_panic;

static struct {
	char fw_ver_str[6];		
	int revision;			
	int supports_temp;		
	int command_mode;		
	int boot_status;		
	int io_addr;			
	spinlock_t io_lock;		
	struct timer_list timer;	
	unsigned long next_heartbeat;	
} pcwd_private;

#define QUIET	0	
#define VERBOSE	1	
#define DEBUG	2	
static int debug = QUIET;
module_param(debug, int, 0);
MODULE_PARM_DESC(debug,
		"Debug level: 0=Quiet, 1=Verbose, 2=Debug (default=0)");

#define WATCHDOG_HEARTBEAT 0
static int heartbeat = WATCHDOG_HEARTBEAT;
module_param(heartbeat, int, 0);
MODULE_PARM_DESC(heartbeat, "Watchdog heartbeat in seconds. "
	"(2 <= heartbeat <= 7200 or 0=delay-time from dip-switches, default="
				__MODULE_STRING(WATCHDOG_HEARTBEAT) ")");

static bool nowayout = WATCHDOG_NOWAYOUT;
module_param(nowayout, bool, 0);
MODULE_PARM_DESC(nowayout,
		"Watchdog cannot be stopped once started (default="
				__MODULE_STRING(WATCHDOG_NOWAYOUT) ")");


static int send_isa_command(int cmd)
{
	int i;
	int control_status;
	int port0, last_port0;	

	if (debug >= DEBUG)
		pr_debug("sending following data cmd=0x%02x\n", cmd);

	
	control_status = (cmd & 0x0F) | WD_WCMD;
	outb_p(control_status, pcwd_private.io_addr + 2);
	udelay(ISA_COMMAND_TIMEOUT);

	port0 = inb_p(pcwd_private.io_addr);
	for (i = 0; i < 25; ++i) {
		last_port0 = port0;
		port0 = inb_p(pcwd_private.io_addr);

		if (port0 == last_port0)
			break;	

		udelay(250);
	}

	if (debug >= DEBUG)
		pr_debug("received following data for cmd=0x%02x: port0=0x%02x last_port0=0x%02x\n",
			 cmd, port0, last_port0);

	return port0;
}

static int set_command_mode(void)
{
	int i, found = 0, count = 0;

	
	spin_lock(&pcwd_private.io_lock);
	while ((!found) && (count < 3)) {
		i = send_isa_command(CMD_ISA_IDLE);

		if (i == 0x00)
			found = 1;
		else if (i == 0xF3) {
			
			outb_p(0x00, pcwd_private.io_addr + 2);
			udelay(1200);	
			outb_p(0x00, pcwd_private.io_addr + 2);
			udelay(ISA_COMMAND_TIMEOUT);
		}
		count++;
	}
	spin_unlock(&pcwd_private.io_lock);
	pcwd_private.command_mode = found;

	if (debug >= DEBUG)
		pr_debug("command_mode=%d\n", pcwd_private.command_mode);

	return found;
}

static void unset_command_mode(void)
{
	
	spin_lock(&pcwd_private.io_lock);
	outb_p(0x00, pcwd_private.io_addr + 2);
	udelay(ISA_COMMAND_TIMEOUT);
	spin_unlock(&pcwd_private.io_lock);

	pcwd_private.command_mode = 0;

	if (debug >= DEBUG)
		pr_debug("command_mode=%d\n", pcwd_private.command_mode);
}

static inline void pcwd_check_temperature_support(void)
{
	if (inb(pcwd_private.io_addr) != 0xF0)
		pcwd_private.supports_temp = 1;
}

static inline void pcwd_get_firmware(void)
{
	int one, ten, hund, minor;

	strcpy(pcwd_private.fw_ver_str, "ERROR");

	if (set_command_mode()) {
		one = send_isa_command(CMD_ISA_VERSION_INTEGER);
		ten = send_isa_command(CMD_ISA_VERSION_TENTH);
		hund = send_isa_command(CMD_ISA_VERSION_HUNDRETH);
		minor = send_isa_command(CMD_ISA_VERSION_MINOR);
		sprintf(pcwd_private.fw_ver_str, "%c.%c%c%c",
					one, ten, hund, minor);
	}
	unset_command_mode();

	return;
}

static inline int pcwd_get_option_switches(void)
{
	int option_switches = 0;

	if (set_command_mode()) {
		
		option_switches = send_isa_command(CMD_ISA_SWITCH_SETTINGS);
	}

	unset_command_mode();
	return option_switches;
}

static void pcwd_show_card_info(void)
{
	int option_switches;

	
	if (pcwd_private.revision == PCWD_REVISION_A)
		pr_info("ISA-PC Watchdog (REV.A) detected at port 0x%04x\n",
			pcwd_private.io_addr);
	else if (pcwd_private.revision == PCWD_REVISION_C) {
		pcwd_get_firmware();
		pr_info("ISA-PC Watchdog (REV.C) detected at port 0x%04x (Firmware version: %s)\n",
			pcwd_private.io_addr, pcwd_private.fw_ver_str);
		option_switches = pcwd_get_option_switches();
		pr_info("Option switches (0x%02x): Temperature Reset Enable=%s, Power On Delay=%s\n",
			option_switches,
			((option_switches & 0x10) ? "ON" : "OFF"),
			((option_switches & 0x08) ? "ON" : "OFF"));

		
		if (set_command_mode()) {
			send_isa_command(CMD_ISA_DELAY_TIME_2SECS);
			unset_command_mode();
		}
	}

	if (pcwd_private.supports_temp)
		pr_info("Temperature Option Detected\n");

	if (pcwd_private.boot_status & WDIOF_CARDRESET)
		pr_info("Previous reboot was caused by the card\n");

	if (pcwd_private.boot_status & WDIOF_OVERHEAT) {
		pr_emerg("Card senses a CPU Overheat. Panicking!\n");
		pr_emerg("CPU Overheat\n");
	}

	if (pcwd_private.boot_status == 0)
		pr_info("No previous trip detected - Cold boot or reset\n");
}

static void pcwd_timer_ping(unsigned long data)
{
	int wdrst_stat;

	if (time_before(jiffies, pcwd_private.next_heartbeat)) {
		
		spin_lock(&pcwd_private.io_lock);
		if (pcwd_private.revision == PCWD_REVISION_A) {
			wdrst_stat = inb_p(pcwd_private.io_addr);
			wdrst_stat &= 0x0F;
			wdrst_stat |= WD_WDRST;

			outb_p(wdrst_stat, pcwd_private.io_addr + 1);
		} else {
			
			outb_p(0x00, pcwd_private.io_addr);
		}

		
		mod_timer(&pcwd_private.timer, jiffies + WDT_INTERVAL);

		spin_unlock(&pcwd_private.io_lock);
	} else {
		pr_warn("Heartbeat lost! Will not ping the watchdog\n");
	}
}

static int pcwd_start(void)
{
	int stat_reg;

	pcwd_private.next_heartbeat = jiffies + (heartbeat * HZ);

	
	mod_timer(&pcwd_private.timer, jiffies + WDT_INTERVAL);

	
	if (pcwd_private.revision == PCWD_REVISION_C) {
		spin_lock(&pcwd_private.io_lock);
		outb_p(0x00, pcwd_private.io_addr + 3);
		udelay(ISA_COMMAND_TIMEOUT);
		stat_reg = inb_p(pcwd_private.io_addr + 2);
		spin_unlock(&pcwd_private.io_lock);
		if (stat_reg & WD_WDIS) {
			pr_info("Could not start watchdog\n");
			return -EIO;
		}
	}

	if (debug >= VERBOSE)
		pr_debug("Watchdog started\n");

	return 0;
}

static int pcwd_stop(void)
{
	int stat_reg;

	
	del_timer(&pcwd_private.timer);

	
	if (pcwd_private.revision == PCWD_REVISION_C) {
		spin_lock(&pcwd_private.io_lock);
		outb_p(0xA5, pcwd_private.io_addr + 3);
		udelay(ISA_COMMAND_TIMEOUT);
		outb_p(0xA5, pcwd_private.io_addr + 3);
		udelay(ISA_COMMAND_TIMEOUT);
		stat_reg = inb_p(pcwd_private.io_addr + 2);
		spin_unlock(&pcwd_private.io_lock);
		if ((stat_reg & WD_WDIS) == 0) {
			pr_info("Could not stop watchdog\n");
			return -EIO;
		}
	}

	if (debug >= VERBOSE)
		pr_debug("Watchdog stopped\n");

	return 0;
}

static int pcwd_keepalive(void)
{
	
	pcwd_private.next_heartbeat = jiffies + (heartbeat * HZ);

	if (debug >= DEBUG)
		pr_debug("Watchdog keepalive signal send\n");

	return 0;
}

static int pcwd_set_heartbeat(int t)
{
	if (t < 2 || t > 7200) 
		return -EINVAL;

	heartbeat = t;

	if (debug >= VERBOSE)
		pr_debug("New heartbeat: %d\n", heartbeat);

	return 0;
}

static int pcwd_get_status(int *status)
{
	int control_status;

	*status = 0;
	spin_lock(&pcwd_private.io_lock);
	if (pcwd_private.revision == PCWD_REVISION_A)
		control_status = inb(pcwd_private.io_addr);
	else {
		control_status = inb(pcwd_private.io_addr + 1);
	}
	spin_unlock(&pcwd_private.io_lock);

	if (pcwd_private.revision == PCWD_REVISION_A) {
		if (control_status & WD_WDRST)
			*status |= WDIOF_CARDRESET;

		if (control_status & WD_T110) {
			*status |= WDIOF_OVERHEAT;
			if (temp_panic) {
				pr_info("Temperature overheat trip!\n");
				kernel_power_off();
			}
		}
	} else {
		if (control_status & WD_REVC_WTRP)
			*status |= WDIOF_CARDRESET;

		if (control_status & WD_REVC_TTRP) {
			*status |= WDIOF_OVERHEAT;
			if (temp_panic) {
				pr_info("Temperature overheat trip!\n");
				kernel_power_off();
			}
		}
	}

	return 0;
}

static int pcwd_clear_status(void)
{
	int control_status;

	if (pcwd_private.revision == PCWD_REVISION_C) {
		spin_lock(&pcwd_private.io_lock);

		if (debug >= VERBOSE)
			pr_info("clearing watchdog trip status\n");

		control_status = inb_p(pcwd_private.io_addr + 1);

		if (debug >= DEBUG) {
			pr_debug("status was: 0x%02x\n", control_status);
			pr_debug("sending: 0x%02x\n",
				 (control_status & WD_REVC_R2DS));
		}

		
		outb_p((control_status & WD_REVC_R2DS),
						pcwd_private.io_addr + 1);

		spin_unlock(&pcwd_private.io_lock);
	}
	return 0;
}

static int pcwd_get_temperature(int *temperature)
{
	
	if (pcwd_private.command_mode)
		return -1;

	*temperature = 0;
	if (!pcwd_private.supports_temp)
		return -ENODEV;

	spin_lock(&pcwd_private.io_lock);
	*temperature = ((inb(pcwd_private.io_addr)) * 9 / 5) + 32;
	spin_unlock(&pcwd_private.io_lock);

	if (debug >= DEBUG) {
		pr_debug("temperature is: %d F\n", *temperature);
	}

	return 0;
}


static long pcwd_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int rv;
	int status;
	int temperature;
	int new_heartbeat;
	int __user *argp = (int __user *)arg;
	static const struct watchdog_info ident = {
		.options =		WDIOF_OVERHEAT |
					WDIOF_CARDRESET |
					WDIOF_KEEPALIVEPING |
					WDIOF_SETTIMEOUT |
					WDIOF_MAGICCLOSE,
		.firmware_version =	1,
		.identity =		"PCWD",
	};

	switch (cmd) {
	case WDIOC_GETSUPPORT:
		if (copy_to_user(argp, &ident, sizeof(ident)))
			return -EFAULT;
		return 0;

	case WDIOC_GETSTATUS:
		pcwd_get_status(&status);
		return put_user(status, argp);

	case WDIOC_GETBOOTSTATUS:
		return put_user(pcwd_private.boot_status, argp);

	case WDIOC_GETTEMP:
		if (pcwd_get_temperature(&temperature))
			return -EFAULT;

		return put_user(temperature, argp);

	case WDIOC_SETOPTIONS:
		if (pcwd_private.revision == PCWD_REVISION_C) {
			if (get_user(rv, argp))
				return -EFAULT;

			if (rv & WDIOS_DISABLECARD) {
				status = pcwd_stop();
				if (status < 0)
					return status;
			}
			if (rv & WDIOS_ENABLECARD) {
				status = pcwd_start();
				if (status < 0)
					return status;
			}
			if (rv & WDIOS_TEMPPANIC)
				temp_panic = 1;
		}
		return -EINVAL;

	case WDIOC_KEEPALIVE:
		pcwd_keepalive();
		return 0;

	case WDIOC_SETTIMEOUT:
		if (get_user(new_heartbeat, argp))
			return -EFAULT;

		if (pcwd_set_heartbeat(new_heartbeat))
			return -EINVAL;

		pcwd_keepalive();
		

	case WDIOC_GETTIMEOUT:
		return put_user(heartbeat, argp);

	default:
		return -ENOTTY;
	}

	return 0;
}

static ssize_t pcwd_write(struct file *file, const char __user *buf, size_t len,
			  loff_t *ppos)
{
	if (len) {
		if (!nowayout) {
			size_t i;

			
			expect_close = 0;

			for (i = 0; i != len; i++) {
				char c;

				if (get_user(c, buf + i))
					return -EFAULT;
				if (c == 'V')
					expect_close = 42;
			}
		}
		pcwd_keepalive();
	}
	return len;
}

static int pcwd_open(struct inode *inode, struct file *file)
{
	if (test_and_set_bit(0, &open_allowed))
		return -EBUSY;
	if (nowayout)
		__module_get(THIS_MODULE);
	
	pcwd_start();
	pcwd_keepalive();
	return nonseekable_open(inode, file);
}

static int pcwd_close(struct inode *inode, struct file *file)
{
	if (expect_close == 42)
		pcwd_stop();
	else {
		pr_crit("Unexpected close, not stopping watchdog!\n");
		pcwd_keepalive();
	}
	expect_close = 0;
	clear_bit(0, &open_allowed);
	return 0;
}


static ssize_t pcwd_temp_read(struct file *file, char __user *buf, size_t count,
			 loff_t *ppos)
{
	int temperature;

	if (pcwd_get_temperature(&temperature))
		return -EFAULT;

	if (copy_to_user(buf, &temperature, 1))
		return -EFAULT;

	return 1;
}

static int pcwd_temp_open(struct inode *inode, struct file *file)
{
	if (!pcwd_private.supports_temp)
		return -ENODEV;

	return nonseekable_open(inode, file);
}

static int pcwd_temp_close(struct inode *inode, struct file *file)
{
	return 0;
}


static const struct file_operations pcwd_fops = {
	.owner		= THIS_MODULE,
	.llseek		= no_llseek,
	.write		= pcwd_write,
	.unlocked_ioctl	= pcwd_ioctl,
	.open		= pcwd_open,
	.release	= pcwd_close,
};

static struct miscdevice pcwd_miscdev = {
	.minor =	WATCHDOG_MINOR,
	.name =		"watchdog",
	.fops =		&pcwd_fops,
};

static const struct file_operations pcwd_temp_fops = {
	.owner		= THIS_MODULE,
	.llseek		= no_llseek,
	.read		= pcwd_temp_read,
	.open		= pcwd_temp_open,
	.release	= pcwd_temp_close,
};

static struct miscdevice temp_miscdev = {
	.minor =	TEMP_MINOR,
	.name =		"temperature",
	.fops =		&pcwd_temp_fops,
};


static inline int get_revision(void)
{
	int r = PCWD_REVISION_C;

	spin_lock(&pcwd_private.io_lock);
	if ((inb(pcwd_private.io_addr + 2) == 0xFF) ||
	    (inb(pcwd_private.io_addr + 3) == 0xFF))
		r = PCWD_REVISION_A;
	spin_unlock(&pcwd_private.io_lock);

	return r;
}

static int __devinit pcwd_isa_match(struct device *dev, unsigned int id)
{
	int base_addr = pcwd_ioports[id];
	int port0, last_port0;	
	int port1, last_port1;	
	int i;
	int retval;

	if (debug >= DEBUG)
		pr_debug("pcwd_isa_match id=%d\n", id);

	if (!request_region(base_addr, 4, "PCWD")) {
		pr_info("Port 0x%04x unavailable\n", base_addr);
		return 0;
	}

	retval = 0;

	port0 = inb_p(base_addr);	
	port1 = inb_p(base_addr + 1);	
	if (port0 != 0xff || port1 != 0xff) {
		
		for (i = 0; i < 4; ++i) {

			msleep(500);

			last_port0 = port0;
			last_port1 = port1;

			port0 = inb_p(base_addr);
			port1 = inb_p(base_addr + 1);

			
			if ((port0 ^ last_port0) & WD_HRTBT ||
			    (port1 ^ last_port1) & WD_REVC_HRBT) {
				retval = 1;
				break;
			}
		}
	}
	release_region(base_addr, 4);

	return retval;
}

static int __devinit pcwd_isa_probe(struct device *dev, unsigned int id)
{
	int ret;

	if (debug >= DEBUG)
		pr_debug("pcwd_isa_probe id=%d\n", id);

	cards_found++;
	if (cards_found == 1)
		pr_info("v%s Ken Hollis (kenji@bitgate.com)\n",
							WATCHDOG_VERSION);

	if (cards_found > 1) {
		pr_err("This driver only supports 1 device\n");
		return -ENODEV;
	}

	if (pcwd_ioports[id] == 0x0000) {
		pr_err("No I/O-Address for card detected\n");
		return -ENODEV;
	}
	pcwd_private.io_addr = pcwd_ioports[id];

	spin_lock_init(&pcwd_private.io_lock);

	
	pcwd_private.revision = get_revision();

	if (!request_region(pcwd_private.io_addr,
		(pcwd_private.revision == PCWD_REVISION_A) ? 2 : 4, "PCWD")) {
		pr_err("I/O address 0x%04x already in use\n",
		       pcwd_private.io_addr);
		ret = -EIO;
		goto error_request_region;
	}

	
	pcwd_private.supports_temp = 0;
	temp_panic = 0;
	pcwd_private.boot_status = 0x0000;

	
	pcwd_get_status(&pcwd_private.boot_status);

	
	pcwd_clear_status();

	setup_timer(&pcwd_private.timer, pcwd_timer_ping, 0);

	
	pcwd_stop();

	
	pcwd_check_temperature_support();

	
	pcwd_show_card_info();

	
	if (heartbeat == 0)
		heartbeat = heartbeat_tbl[(pcwd_get_option_switches() & 0x07)];

	if (pcwd_set_heartbeat(heartbeat)) {
		pcwd_set_heartbeat(WATCHDOG_HEARTBEAT);
		pr_info("heartbeat value must be 2 <= heartbeat <= 7200, using %d\n",
			WATCHDOG_HEARTBEAT);
	}

	if (pcwd_private.supports_temp) {
		ret = misc_register(&temp_miscdev);
		if (ret) {
			pr_err("cannot register miscdev on minor=%d (err=%d)\n",
			       TEMP_MINOR, ret);
			goto error_misc_register_temp;
		}
	}

	ret = misc_register(&pcwd_miscdev);
	if (ret) {
		pr_err("cannot register miscdev on minor=%d (err=%d)\n",
		       WATCHDOG_MINOR, ret);
		goto error_misc_register_watchdog;
	}

	pr_info("initialized. heartbeat=%d sec (nowayout=%d)\n",
		heartbeat, nowayout);

	return 0;

error_misc_register_watchdog:
	if (pcwd_private.supports_temp)
		misc_deregister(&temp_miscdev);
error_misc_register_temp:
	release_region(pcwd_private.io_addr,
			(pcwd_private.revision == PCWD_REVISION_A) ? 2 : 4);
error_request_region:
	pcwd_private.io_addr = 0x0000;
	cards_found--;
	return ret;
}

static int __devexit pcwd_isa_remove(struct device *dev, unsigned int id)
{
	if (debug >= DEBUG)
		pr_debug("pcwd_isa_remove id=%d\n", id);

	if (!pcwd_private.io_addr)
		return 1;

	
	if (!nowayout)
		pcwd_stop();

	
	misc_deregister(&pcwd_miscdev);
	if (pcwd_private.supports_temp)
		misc_deregister(&temp_miscdev);
	release_region(pcwd_private.io_addr,
			(pcwd_private.revision == PCWD_REVISION_A) ? 2 : 4);
	pcwd_private.io_addr = 0x0000;
	cards_found--;

	return 0;
}

static void pcwd_isa_shutdown(struct device *dev, unsigned int id)
{
	if (debug >= DEBUG)
		pr_debug("pcwd_isa_shutdown id=%d\n", id);

	pcwd_stop();
}

static struct isa_driver pcwd_isa_driver = {
	.match		= pcwd_isa_match,
	.probe		= pcwd_isa_probe,
	.remove		= __devexit_p(pcwd_isa_remove),
	.shutdown	= pcwd_isa_shutdown,
	.driver		= {
		.owner	= THIS_MODULE,
		.name	= WATCHDOG_NAME,
	},
};

static int __init pcwd_init_module(void)
{
	return isa_register_driver(&pcwd_isa_driver, PCWD_ISA_NR_CARDS);
}

static void __exit pcwd_cleanup_module(void)
{
	isa_unregister_driver(&pcwd_isa_driver);
	pr_info("Watchdog Module Unloaded\n");
}

module_init(pcwd_init_module);
module_exit(pcwd_cleanup_module);

MODULE_AUTHOR("Ken Hollis <kenji@bitgate.com>, "
		"Wim Van Sebroeck <wim@iguana.be>");
MODULE_DESCRIPTION("Berkshire ISA-PC Watchdog driver");
MODULE_VERSION(WATCHDOG_VERSION);
MODULE_LICENSE("GPL");
MODULE_ALIAS_MISCDEV(WATCHDOG_MINOR);
MODULE_ALIAS_MISCDEV(TEMP_MINOR);
