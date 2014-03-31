/*
 * IEEE-1284 implementation for parport.
 *
 * Authors: Phil Blundell <philb@gnu.org>
 *          Carsten Gross <carsten@sol.wohnheim.uni-ulm.de>
 *	    Jose Renau <renau@acm.org>
 *          Tim Waugh <tim@cyberelk.demon.co.uk> (largely rewritten)
 *
 * This file is responsible for IEEE 1284 negotiation, and for handing
 * read/write requests to low-level drivers.
 *
 * Any part of this program may be used in documents licensed under
 * the GNU Free Documentation License, Version 1.1 or any later version
 * published by the Free Software Foundation.
 *
 * Various hacks, Fred Barnes <frmb2@ukc.ac.uk>, 04/2000
 */

#include <linux/module.h>
#include <linux/threads.h>
#include <linux/parport.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/timer.h>
#include <linux/sched.h>

#undef DEBUG 

#ifdef CONFIG_LP_CONSOLE
#undef DEBUG 
#endif

#ifdef DEBUG
#define DPRINTK(stuff...) printk (stuff)
#else
#define DPRINTK(stuff...)
#endif

static void parport_ieee1284_wakeup (struct parport *port)
{
	up (&port->physport->ieee1284.irq);
}

static struct parport *port_from_cookie[PARPORT_MAX];
static void timeout_waiting_on_port (unsigned long cookie)
{
	parport_ieee1284_wakeup (port_from_cookie[cookie % PARPORT_MAX]);
}


int parport_wait_event (struct parport *port, signed long timeout)
{
	int ret;
	struct timer_list timer;

	if (!port->physport->cad->timeout)
		return 1;

	init_timer_on_stack(&timer);
	timer.expires = jiffies + timeout;
	timer.function = timeout_waiting_on_port;
	port_from_cookie[port->number % PARPORT_MAX] = port;
	timer.data = port->number;

	add_timer (&timer);
	ret = down_interruptible (&port->physport->ieee1284.irq);
	if (!del_timer_sync(&timer) && !ret)
		
		ret = 1;

	destroy_timer_on_stack(&timer);

	return ret;
}


int parport_poll_peripheral(struct parport *port,
			    unsigned char mask,
			    unsigned char result,
			    int usec)
{
	
	int count = usec / 5 + 2;
	int i;
	unsigned char status;
	for (i = 0; i < count; i++) {
		status = parport_read_status (port);
		if ((status & mask) == result)
			return 0;
		if (signal_pending (current))
			return -EINTR;
		if (need_resched())
			break;
		if (i >= 2)
			udelay (5);
	}

	return 1;
}


int parport_wait_peripheral(struct parport *port,
			    unsigned char mask, 
			    unsigned char result)
{
	int ret;
	int usec;
	unsigned long deadline;
	unsigned char status;

	usec = port->physport->spintime; 
	if (!port->physport->cad->timeout)
		usec = 35000;

	ret = parport_poll_peripheral (port, mask, result, usec);
	if (ret != 1)
		return ret;

	if (!port->physport->cad->timeout)
		return 1;

	
	deadline = jiffies + msecs_to_jiffies(40);
	while (time_before (jiffies, deadline)) {
		if (signal_pending (current))
			return -EINTR;

		if ((ret = parport_wait_event (port, msecs_to_jiffies(10))) < 0)
			return ret;

		status = parport_read_status (port);
		if ((status & mask) == result)
			return 0;

		if (!ret) {
			schedule_timeout_interruptible(msecs_to_jiffies(10));
		}
	}

	return 1;
}

#ifdef CONFIG_PARPORT_1284
static void parport_ieee1284_terminate (struct parport *port)
{
	int r;
	port = port->physport;

	
	switch (port->ieee1284.mode) {
	case IEEE1284_MODE_EPP:
	case IEEE1284_MODE_EPPSL:
	case IEEE1284_MODE_EPPSWE:
		

		
		parport_frob_control (port, PARPORT_CONTROL_INIT, 0);
		udelay (50);

		
		parport_frob_control (port,
				      PARPORT_CONTROL_SELECT
				      | PARPORT_CONTROL_INIT,
				      PARPORT_CONTROL_SELECT
				      | PARPORT_CONTROL_INIT);
		break;

	case IEEE1284_MODE_ECP:
	case IEEE1284_MODE_ECPRLE:
	case IEEE1284_MODE_ECPSWE:
		
		if (port->ieee1284.phase != IEEE1284_PH_FWD_IDLE) {
			
			parport_frob_control (port,
					      PARPORT_CONTROL_INIT
					      | PARPORT_CONTROL_AUTOFD,
					      PARPORT_CONTROL_INIT
					      | PARPORT_CONTROL_AUTOFD);

			
			r = parport_wait_peripheral (port,
						     PARPORT_STATUS_PAPEROUT,
						     PARPORT_STATUS_PAPEROUT);
			if (r)
				DPRINTK (KERN_INFO "%s: Timeout at event 49\n",
					 port->name);

			parport_data_forward (port);
			DPRINTK (KERN_DEBUG "%s: ECP direction: forward\n",
				 port->name);
			port->ieee1284.phase = IEEE1284_PH_FWD_IDLE;
		}

		

	default:
		

		
		parport_frob_control (port,
				      PARPORT_CONTROL_SELECT
				      | PARPORT_CONTROL_AUTOFD,
				      PARPORT_CONTROL_SELECT);

		
		r = parport_wait_peripheral (port, PARPORT_STATUS_ACK, 0);
		if (r)
			DPRINTK (KERN_INFO "%s: Timeout at event 24\n",
				 port->name);

		
		parport_frob_control (port,
				      PARPORT_CONTROL_AUTOFD,
				      PARPORT_CONTROL_AUTOFD);

		
		r = parport_wait_peripheral (port,
					     PARPORT_STATUS_ACK, 
					     PARPORT_STATUS_ACK);
		if (r)
			DPRINTK (KERN_INFO "%s: Timeout at event 27\n",
				 port->name);

		
		parport_frob_control (port, PARPORT_CONTROL_AUTOFD, 0);
	}

	port->ieee1284.mode = IEEE1284_MODE_COMPAT;
	port->ieee1284.phase = IEEE1284_PH_FWD_IDLE;

	DPRINTK (KERN_DEBUG "%s: In compatibility (forward idle) mode\n",
		 port->name);
}		
#endif 


int parport_negotiate (struct parport *port, int mode)
{
#ifndef CONFIG_PARPORT_1284
	if (mode == IEEE1284_MODE_COMPAT)
		return 0;
	printk (KERN_ERR "parport: IEEE1284 not supported in this kernel\n");
	return -1;
#else
	int m = mode & ~IEEE1284_ADDR;
	int r;
	unsigned char xflag;

	port = port->physport;

	
	if (port->ieee1284.mode == mode)
		return 0;

	
	if ((port->ieee1284.mode & ~IEEE1284_ADDR) == (mode & ~IEEE1284_ADDR)){
		port->ieee1284.mode = mode;
		return 0;
	}

	
	if (port->ieee1284.mode != IEEE1284_MODE_COMPAT)
		parport_ieee1284_terminate (port);

	if (mode == IEEE1284_MODE_COMPAT)
		
		return 0; 

	switch (mode) {
	case IEEE1284_MODE_ECPSWE:
		m = IEEE1284_MODE_ECP;
		break;
	case IEEE1284_MODE_EPPSL:
	case IEEE1284_MODE_EPPSWE:
		m = IEEE1284_MODE_EPP;
		break;
	case IEEE1284_MODE_BECP:
		return -ENOSYS; 
	}

	if (mode & IEEE1284_EXT_LINK)
		m = 1<<7; 

	port->ieee1284.phase = IEEE1284_PH_NEGOTIATION;

	
	parport_frob_control (port,
			      PARPORT_CONTROL_STROBE
			      | PARPORT_CONTROL_AUTOFD
			      | PARPORT_CONTROL_SELECT,
			      PARPORT_CONTROL_SELECT);
	udelay(1);

	
	parport_data_forward (port);
	parport_write_data (port, m);
	udelay (400); 

	
	parport_frob_control (port,
			      PARPORT_CONTROL_SELECT
			      | PARPORT_CONTROL_AUTOFD,
			      PARPORT_CONTROL_AUTOFD);

	
	if (parport_wait_peripheral (port,
				     PARPORT_STATUS_ERROR
				     | PARPORT_STATUS_SELECT
				     | PARPORT_STATUS_PAPEROUT
				     | PARPORT_STATUS_ACK,
				     PARPORT_STATUS_ERROR
				     | PARPORT_STATUS_SELECT
				     | PARPORT_STATUS_PAPEROUT)) {
		
		parport_frob_control (port,
				      PARPORT_CONTROL_SELECT
				      | PARPORT_CONTROL_AUTOFD,
				      PARPORT_CONTROL_SELECT);
		DPRINTK (KERN_DEBUG
			 "%s: Peripheral not IEEE1284 compliant (0x%02X)\n",
			 port->name, parport_read_status (port));
		port->ieee1284.phase = IEEE1284_PH_FWD_IDLE;
		return -1; 
	}

	
	parport_frob_control (port,
			      PARPORT_CONTROL_STROBE,
			      PARPORT_CONTROL_STROBE);

	
	udelay (5);
	parport_frob_control (port,
			      PARPORT_CONTROL_STROBE
			      | PARPORT_CONTROL_AUTOFD,
			      0);

	
	if (parport_wait_peripheral (port,
				     PARPORT_STATUS_ACK,
				     PARPORT_STATUS_ACK)) {
		
		DPRINTK (KERN_DEBUG
			 "%s: Mode 0x%02x not supported? (0x%02x)\n",
			 port->name, mode, port->ops->read_status (port));
		parport_ieee1284_terminate (port);
		return 1;
	}

	xflag = parport_read_status (port) & PARPORT_STATUS_SELECT;

	
	if (mode && !xflag) {
		
		DPRINTK (KERN_DEBUG "%s: Mode 0x%02x rejected by peripheral\n",
			 port->name, mode);
		parport_ieee1284_terminate (port);
		return 1;
	}

	
	if (mode & IEEE1284_EXT_LINK) {
		m = mode & 0x7f;
		udelay (1);
		parport_write_data (port, m);
		udelay (1);

		
		parport_frob_control (port,
				      PARPORT_CONTROL_STROBE,
				      PARPORT_CONTROL_STROBE);

		
		if (parport_wait_peripheral (port, PARPORT_STATUS_ACK, 0)) {
			
			DPRINTK (KERN_DEBUG
				 "%s: Event 52 didn't happen\n",
				 port->name);
			parport_ieee1284_terminate (port);
			return 1;
		}

		
		parport_frob_control (port,
				      PARPORT_CONTROL_STROBE,
				      0);

		
		if (parport_wait_peripheral (port,
					     PARPORT_STATUS_ACK,
					     PARPORT_STATUS_ACK)) {
			DPRINTK (KERN_DEBUG
				 "%s: Mode 0x%02x not supported? (0x%02x)\n",
				 port->name, mode,
				 port->ops->read_status (port));
			parport_ieee1284_terminate (port);
			return 1;
		}

		
		xflag = parport_read_status (port) & PARPORT_STATUS_SELECT;

		
		if (!xflag) {
			
			DPRINTK (KERN_DEBUG "%s: Extended mode 0x%02x not "
				 "supported\n", port->name, mode);
			parport_ieee1284_terminate (port);
			return 1;
		}

		
	}

	
	DPRINTK (KERN_DEBUG "%s: In mode 0x%02x\n", port->name, mode);
	port->ieee1284.mode = mode;

	
	if (!(mode & IEEE1284_EXT_LINK) && (m & IEEE1284_MODE_ECP)) {
		port->ieee1284.phase = IEEE1284_PH_ECP_SETUP;

		
		parport_frob_control (port,
				      PARPORT_CONTROL_AUTOFD,
				      PARPORT_CONTROL_AUTOFD);

		
		r = parport_wait_peripheral (port,
					     PARPORT_STATUS_PAPEROUT,
					     PARPORT_STATUS_PAPEROUT);
		if (r) {
			DPRINTK (KERN_INFO "%s: Timeout at event 31\n",
				port->name);
		}

		port->ieee1284.phase = IEEE1284_PH_FWD_IDLE;
		DPRINTK (KERN_DEBUG "%s: ECP direction: forward\n",
			 port->name);
	} else switch (mode) {
	case IEEE1284_MODE_NIBBLE:
	case IEEE1284_MODE_BYTE:
		port->ieee1284.phase = IEEE1284_PH_REV_IDLE;
		break;
	default:
		port->ieee1284.phase = IEEE1284_PH_FWD_IDLE;
	}


	return 0;
#endif 
}

#ifdef CONFIG_PARPORT_1284
static int parport_ieee1284_ack_data_avail (struct parport *port)
{
	if (parport_read_status (port) & PARPORT_STATUS_ERROR)
		
		return -1;

	
	port->ops->frob_control (port, PARPORT_CONTROL_AUTOFD, 0);
	port->ieee1284.phase = IEEE1284_PH_HBUSY_DAVAIL;
	return 0;
}
#endif 

void parport_ieee1284_interrupt (void *handle)
{
	struct parport *port = handle;
	parport_ieee1284_wakeup (port);

#ifdef CONFIG_PARPORT_1284
	if (port->ieee1284.phase == IEEE1284_PH_REV_IDLE) {
		DPRINTK (KERN_DEBUG "%s: Data available\n", port->name);
		parport_ieee1284_ack_data_avail (port);
	}
#endif 
}


ssize_t parport_write (struct parport *port, const void *buffer, size_t len)
{
#ifndef CONFIG_PARPORT_1284
	return port->ops->compat_write_data (port, buffer, len, 0);
#else
	ssize_t retval;
	int mode = port->ieee1284.mode;
	int addr = mode & IEEE1284_ADDR;
	size_t (*fn) (struct parport *, const void *, size_t, int);

	
	mode &= ~(IEEE1284_DEVICEID | IEEE1284_ADDR);

	
	switch (mode) {
	case IEEE1284_MODE_NIBBLE:
	case IEEE1284_MODE_BYTE:
		parport_negotiate (port, IEEE1284_MODE_COMPAT);
	case IEEE1284_MODE_COMPAT:
		DPRINTK (KERN_DEBUG "%s: Using compatibility mode\n",
			 port->name);
		fn = port->ops->compat_write_data;
		break;

	case IEEE1284_MODE_EPP:
		DPRINTK (KERN_DEBUG "%s: Using EPP mode\n", port->name);
		if (addr) {
			fn = port->ops->epp_write_addr;
		} else {
			fn = port->ops->epp_write_data;
		}
		break;
	case IEEE1284_MODE_EPPSWE:
		DPRINTK (KERN_DEBUG "%s: Using software-emulated EPP mode\n",
			port->name);
		if (addr) {
			fn = parport_ieee1284_epp_write_addr;
		} else {
			fn = parport_ieee1284_epp_write_data;
		}
		break;
	case IEEE1284_MODE_ECP:
	case IEEE1284_MODE_ECPRLE:
		DPRINTK (KERN_DEBUG "%s: Using ECP mode\n", port->name);
		if (addr) {
			fn = port->ops->ecp_write_addr;
		} else {
			fn = port->ops->ecp_write_data;
		}
		break;

	case IEEE1284_MODE_ECPSWE:
		DPRINTK (KERN_DEBUG "%s: Using software-emulated ECP mode\n",
			 port->name);
		if (addr) {
			fn = parport_ieee1284_ecp_write_addr;
		} else {
			fn = parport_ieee1284_ecp_write_data;
		}
		break;

	default:
		DPRINTK (KERN_DEBUG "%s: Unknown mode 0x%02x\n", port->name,
			port->ieee1284.mode);
		return -ENOSYS;
	}

	retval = (*fn) (port, buffer, len, 0);
	DPRINTK (KERN_DEBUG "%s: wrote %d/%d bytes\n", port->name, retval, len);
	return retval;
#endif 
}


ssize_t parport_read (struct parport *port, void *buffer, size_t len)
{
#ifndef CONFIG_PARPORT_1284
	printk (KERN_ERR "parport: IEEE1284 not supported in this kernel\n");
	return -ENODEV;
#else
	int mode = port->physport->ieee1284.mode;
	int addr = mode & IEEE1284_ADDR;
	size_t (*fn) (struct parport *, void *, size_t, int);

	
	mode &= ~(IEEE1284_DEVICEID | IEEE1284_ADDR);

	
	switch (mode) {
	case IEEE1284_MODE_COMPAT:
		if ((port->physport->modes & PARPORT_MODE_TRISTATE) &&
		    !parport_negotiate (port, IEEE1284_MODE_BYTE)) {
			
			DPRINTK (KERN_DEBUG "%s: Using byte mode\n", port->name);
			fn = port->ops->byte_read_data;
			break;
		}
		if (parport_negotiate (port, IEEE1284_MODE_NIBBLE)) {
			return -EIO;
		}
		
	case IEEE1284_MODE_NIBBLE:
		DPRINTK (KERN_DEBUG "%s: Using nibble mode\n", port->name);
		fn = port->ops->nibble_read_data;
		break;

	case IEEE1284_MODE_BYTE:
		DPRINTK (KERN_DEBUG "%s: Using byte mode\n", port->name);
		fn = port->ops->byte_read_data;
		break;

	case IEEE1284_MODE_EPP:
		DPRINTK (KERN_DEBUG "%s: Using EPP mode\n", port->name);
		if (addr) {
			fn = port->ops->epp_read_addr;
		} else {
			fn = port->ops->epp_read_data;
		}
		break;
	case IEEE1284_MODE_EPPSWE:
		DPRINTK (KERN_DEBUG "%s: Using software-emulated EPP mode\n",
			port->name);
		if (addr) {
			fn = parport_ieee1284_epp_read_addr;
		} else {
			fn = parport_ieee1284_epp_read_data;
		}
		break;
	case IEEE1284_MODE_ECP:
	case IEEE1284_MODE_ECPRLE:
		DPRINTK (KERN_DEBUG "%s: Using ECP mode\n", port->name);
		fn = port->ops->ecp_read_data;
		break;

	case IEEE1284_MODE_ECPSWE:
		DPRINTK (KERN_DEBUG "%s: Using software-emulated ECP mode\n",
			 port->name);
		fn = parport_ieee1284_ecp_read_data;
		break;

	default:
		DPRINTK (KERN_DEBUG "%s: Unknown mode 0x%02x\n", port->name,
			 port->physport->ieee1284.mode);
		return -ENOSYS;
	}

	return (*fn) (port, buffer, len, 0);
#endif 
}


long parport_set_timeout (struct pardevice *dev, long inactivity)
{
	long int old = dev->timeout;

	dev->timeout = inactivity;

	if (dev->port->physport->cad == dev)
		parport_ieee1284_wakeup (dev->port);

	return old;
}


EXPORT_SYMBOL(parport_negotiate);
EXPORT_SYMBOL(parport_write);
EXPORT_SYMBOL(parport_read);
EXPORT_SYMBOL(parport_wait_peripheral);
EXPORT_SYMBOL(parport_wait_event);
EXPORT_SYMBOL(parport_set_timeout);
EXPORT_SYMBOL(parport_ieee1284_interrupt);
