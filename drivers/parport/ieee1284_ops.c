

#include <linux/module.h>
#include <linux/parport.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <asm/uaccess.h>

#undef DEBUG 

#ifdef CONFIG_LP_CONSOLE
#undef DEBUG 
#endif

#ifdef DEBUG
#define DPRINTK(stuff...) printk (stuff)
#else
#define DPRINTK(stuff...)
#endif


size_t parport_ieee1284_write_compat (struct parport *port,
				      const void *buffer, size_t len,
				      int flags)
{
	int no_irq = 1;
	ssize_t count = 0;
	const unsigned char *addr = buffer;
	unsigned char byte;
	struct pardevice *dev = port->physport->cad;
	unsigned char ctl = (PARPORT_CONTROL_SELECT
			     | PARPORT_CONTROL_INIT);

	if (port->irq != PARPORT_IRQ_NONE) {
		parport_enable_irq (port);
		no_irq = 0;
	}

	port->physport->ieee1284.phase = IEEE1284_PH_FWD_DATA;
	parport_write_control (port, ctl);
	parport_data_forward (port);
	while (count < len) {
		unsigned long expire = jiffies + dev->timeout;
		long wait = msecs_to_jiffies(10);
		unsigned char mask = (PARPORT_STATUS_ERROR
				      | PARPORT_STATUS_BUSY);
		unsigned char val = (PARPORT_STATUS_ERROR
				     | PARPORT_STATUS_BUSY);

		
		do {
			
			if (!parport_wait_peripheral (port, mask, val))
				
				goto ready;

			
			if ((parport_read_status (port) &
			     (PARPORT_STATUS_PAPEROUT |
			      PARPORT_STATUS_SELECT |
			      PARPORT_STATUS_ERROR))
			    != (PARPORT_STATUS_SELECT |
				PARPORT_STATUS_ERROR))
				goto stop;

			
			if (!time_before (jiffies, expire))
				break;

			if (count && no_irq) {
				parport_release (dev);
				schedule_timeout_interruptible(wait);
				parport_claim_or_block (dev);
			}
			else
				
				parport_wait_event (port, wait);

			
			if (signal_pending (current))
				break;

			
			wait *= 2;
		} while (time_before (jiffies, expire));

		if (signal_pending (current))
			break;

		DPRINTK (KERN_DEBUG "%s: Timed out\n", port->name);
		break;

	ready:
		
		byte = *addr++;
		parport_write_data (port, byte);
		udelay (1);

		
		parport_write_control (port, ctl | PARPORT_CONTROL_STROBE);
		udelay (1); 

		parport_write_control (port, ctl);
		udelay (1); 

		
		count++;

                
		if (time_before (jiffies, expire))
			if (!parport_yield_blocking (dev)
			    && need_resched())
				schedule ();
	}
 stop:
	port->physport->ieee1284.phase = IEEE1284_PH_FWD_IDLE;

	return count;
}

size_t parport_ieee1284_read_nibble (struct parport *port, 
				     void *buffer, size_t len,
				     int flags)
{
#ifndef CONFIG_PARPORT_1284
	return 0;
#else
	unsigned char *buf = buffer;
	int i;
	unsigned char byte = 0;

	len *= 2; 
	for (i=0; i < len; i++) {
		unsigned char nibble;

		
		if (((i & 1) == 0) &&
		    (parport_read_status(port) & PARPORT_STATUS_ERROR)) {
			goto end_of_data;
		}

		
		parport_frob_control (port,
				      PARPORT_CONTROL_AUTOFD,
				      PARPORT_CONTROL_AUTOFD);

		
		port->ieee1284.phase = IEEE1284_PH_REV_DATA;
		if (parport_wait_peripheral (port,
					     PARPORT_STATUS_ACK, 0)) {
			
			DPRINTK (KERN_DEBUG
				 "%s: Nibble timeout at event 9 (%d bytes)\n",
				 port->name, i/2);
			parport_frob_control (port, PARPORT_CONTROL_AUTOFD, 0);
			break;
		}


		
		nibble = parport_read_status (port) >> 3;
		nibble &= ~8;
		if ((nibble & 0x10) == 0)
			nibble |= 8;
		nibble &= 0xf;

		
		parport_frob_control (port, PARPORT_CONTROL_AUTOFD, 0);

		
		if (parport_wait_peripheral (port,
					     PARPORT_STATUS_ACK,
					     PARPORT_STATUS_ACK)) {
			
			DPRINTK (KERN_DEBUG
				 "%s: Nibble timeout at event 11\n",
				 port->name);
			break;
		}

		if (i & 1) {
			
			byte |= nibble << 4;
			*buf++ = byte;
		} else 
			byte = nibble;
	}

	if (i == len) {
		
		if (parport_read_status (port) & PARPORT_STATUS_ERROR) {
		end_of_data:
			DPRINTK (KERN_DEBUG
				"%s: No more nibble data (%d bytes)\n",
				port->name, i/2);

			
			parport_frob_control (port,
					      PARPORT_CONTROL_AUTOFD,
					      PARPORT_CONTROL_AUTOFD);
			port->physport->ieee1284.phase = IEEE1284_PH_REV_IDLE;
		}
		else
			port->physport->ieee1284.phase = IEEE1284_PH_HBUSY_DAVAIL;
	}

	return i/2;
#endif 
}

size_t parport_ieee1284_read_byte (struct parport *port,
				   void *buffer, size_t len,
				   int flags)
{
#ifndef CONFIG_PARPORT_1284
	return 0;
#else
	unsigned char *buf = buffer;
	ssize_t count = 0;

	for (count = 0; count < len; count++) {
		unsigned char byte;

		
		if (parport_read_status (port) & PARPORT_STATUS_ERROR) {
			goto end_of_data;
		}

		
		parport_data_reverse (port);

		
		parport_frob_control (port,
				      PARPORT_CONTROL_AUTOFD,
				      PARPORT_CONTROL_AUTOFD);

		
		port->physport->ieee1284.phase = IEEE1284_PH_REV_DATA;
		if (parport_wait_peripheral (port,
					     PARPORT_STATUS_ACK,
					     0)) {
			
			parport_frob_control (port, PARPORT_CONTROL_AUTOFD,
						 0);
			DPRINTK (KERN_DEBUG "%s: Byte timeout at event 9\n",
				 port->name);
			break;
		}

		byte = parport_read_data (port);
		*buf++ = byte;

		
		parport_frob_control (port, PARPORT_CONTROL_AUTOFD, 0);

		
		if (parport_wait_peripheral (port,
					     PARPORT_STATUS_ACK,
					     PARPORT_STATUS_ACK)) {
			
			DPRINTK (KERN_DEBUG "%s: Byte timeout at event 11\n",
				 port->name);
			break;
		}

		
		parport_frob_control (port,
				      PARPORT_CONTROL_STROBE,
				      PARPORT_CONTROL_STROBE);
		udelay (5);

		
		parport_frob_control (port, PARPORT_CONTROL_STROBE, 0);
	}

	if (count == len) {
		
		if (parport_read_status (port) & PARPORT_STATUS_ERROR) {
		end_of_data:
			DPRINTK (KERN_DEBUG
				 "%s: No more byte data (%Zd bytes)\n",
				 port->name, count);

			
			parport_frob_control (port,
					      PARPORT_CONTROL_AUTOFD,
					      PARPORT_CONTROL_AUTOFD);
			port->physport->ieee1284.phase = IEEE1284_PH_REV_IDLE;
		}
		else
			port->physport->ieee1284.phase = IEEE1284_PH_HBUSY_DAVAIL;
	}

	return count;
#endif 
}


#ifdef CONFIG_PARPORT_1284

static inline
int ecp_forward_to_reverse (struct parport *port)
{
	int retval;

	
	parport_frob_control (port,
			      PARPORT_CONTROL_AUTOFD,
			      PARPORT_CONTROL_AUTOFD);
	parport_data_reverse (port);
	udelay (5);

	
	parport_frob_control (port,
			      PARPORT_CONTROL_INIT,
			      0);

	
	retval = parport_wait_peripheral (port,
					  PARPORT_STATUS_PAPEROUT, 0);

	if (!retval) {
		DPRINTK (KERN_DEBUG "%s: ECP direction: reverse\n",
			 port->name);
		port->ieee1284.phase = IEEE1284_PH_REV_IDLE;
	} else {
		DPRINTK (KERN_DEBUG "%s: ECP direction: failed to reverse\n",
			 port->name);
		port->ieee1284.phase = IEEE1284_PH_ECP_DIR_UNKNOWN;
	}

	return retval;
}

static inline
int ecp_reverse_to_forward (struct parport *port)
{
	int retval;

	
	parport_frob_control (port,
			      PARPORT_CONTROL_INIT
			      | PARPORT_CONTROL_AUTOFD,
			      PARPORT_CONTROL_INIT
			      | PARPORT_CONTROL_AUTOFD);

	
	retval = parport_wait_peripheral (port,
					  PARPORT_STATUS_PAPEROUT,
					  PARPORT_STATUS_PAPEROUT);

	if (!retval) {
		parport_data_forward (port);
		DPRINTK (KERN_DEBUG "%s: ECP direction: forward\n",
			 port->name);
		port->ieee1284.phase = IEEE1284_PH_FWD_IDLE;
	} else {
		DPRINTK (KERN_DEBUG
			 "%s: ECP direction: failed to switch forward\n",
			 port->name);
		port->ieee1284.phase = IEEE1284_PH_ECP_DIR_UNKNOWN;
	}


	return retval;
}

#endif 

size_t parport_ieee1284_ecp_write_data (struct parport *port,
					const void *buffer, size_t len,
					int flags)
{
#ifndef CONFIG_PARPORT_1284
	return 0;
#else
	const unsigned char *buf = buffer;
	size_t written;
	int retry;

	port = port->physport;

	if (port->ieee1284.phase != IEEE1284_PH_FWD_IDLE)
		if (ecp_reverse_to_forward (port))
			return 0;

	port->ieee1284.phase = IEEE1284_PH_FWD_DATA;

	
	parport_frob_control (port,
			      PARPORT_CONTROL_AUTOFD
			      | PARPORT_CONTROL_STROBE
			      | PARPORT_CONTROL_INIT,
			      PARPORT_CONTROL_INIT);
	for (written = 0; written < len; written++, buf++) {
		unsigned long expire = jiffies + port->cad->timeout;
		unsigned char byte;

		byte = *buf;
	try_again:
		parport_write_data (port, byte);
		parport_frob_control (port, PARPORT_CONTROL_STROBE,
				      PARPORT_CONTROL_STROBE);
		udelay (5);
		for (retry = 0; retry < 100; retry++) {
			if (!parport_wait_peripheral (port,
						      PARPORT_STATUS_BUSY, 0))
				goto success;

			if (signal_pending (current)) {
				parport_frob_control (port,
						      PARPORT_CONTROL_STROBE,
						      0);
				break;
			}
		}

		
		DPRINTK (KERN_DEBUG "%s: ECP transfer stalled!\n", port->name);

		parport_frob_control (port, PARPORT_CONTROL_INIT,
				      PARPORT_CONTROL_INIT);
		udelay (50);
		if (parport_read_status (port) & PARPORT_STATUS_PAPEROUT) {
			
			parport_frob_control (port, PARPORT_CONTROL_INIT, 0);
			break;
		}

		parport_frob_control (port, PARPORT_CONTROL_INIT, 0);
		udelay (50);
		if (!(parport_read_status (port) & PARPORT_STATUS_PAPEROUT))
			break;

		DPRINTK (KERN_DEBUG "%s: Host transfer recovered\n",
			 port->name);

		if (time_after_eq (jiffies, expire)) break;
		goto try_again;
	success:
		parport_frob_control (port, PARPORT_CONTROL_STROBE, 0);
		udelay (5);
		if (parport_wait_peripheral (port,
					     PARPORT_STATUS_BUSY,
					     PARPORT_STATUS_BUSY))
			
			break;
	}

	port->ieee1284.phase = IEEE1284_PH_FWD_IDLE;

	return written;
#endif 
}

size_t parport_ieee1284_ecp_read_data (struct parport *port,
				       void *buffer, size_t len, int flags)
{
#ifndef CONFIG_PARPORT_1284
	return 0;
#else
	struct pardevice *dev = port->cad;
	unsigned char *buf = buffer;
	int rle_count = 0; 
	unsigned char ctl;
	int rle = 0;
	ssize_t count = 0;

	port = port->physport;

	if (port->ieee1284.phase != IEEE1284_PH_REV_IDLE)
		if (ecp_forward_to_reverse (port))
			return 0;

	port->ieee1284.phase = IEEE1284_PH_REV_DATA;

	
	ctl = parport_read_control (port);
	ctl &= ~(PARPORT_CONTROL_STROBE | PARPORT_CONTROL_INIT |
		 PARPORT_CONTROL_AUTOFD);
	parport_write_control (port,
			       ctl | PARPORT_CONTROL_AUTOFD);
	while (count < len) {
		unsigned long expire = jiffies + dev->timeout;
		unsigned char byte;
		int command;

		while (parport_wait_peripheral (port, PARPORT_STATUS_ACK, 0)) {
			if (count)
				goto out;

			if (!time_before (jiffies, expire))
				goto out;

			
			if (count && dev->port->irq != PARPORT_IRQ_NONE) {
				parport_release (dev);
				schedule_timeout_interruptible(msecs_to_jiffies(40));
				parport_claim_or_block (dev);
			}
			else
				
				parport_wait_event (port, msecs_to_jiffies(40));

			
			if (signal_pending (current))
				goto out;
		}

		
		if (rle)
			command = 0;
		else
			command = (parport_read_status (port) &
				   PARPORT_STATUS_BUSY) ? 1 : 0;

		
		byte = parport_read_data (port);

		if (command) {
			if (byte & 0x80) {
				DPRINTK (KERN_DEBUG "%s: stopping short at "
					 "channel command (%02x)\n",
					 port->name, byte);
				goto out;
			}
			else if (port->ieee1284.mode != IEEE1284_MODE_ECPRLE)
				DPRINTK (KERN_DEBUG "%s: device illegally "
					 "using RLE; accepting anyway\n",
					 port->name);

			rle_count = byte + 1;

			
			if (rle_count > (len - count)) {
				DPRINTK (KERN_DEBUG "%s: leaving %d RLE bytes "
					 "for next time\n", port->name,
					 rle_count);
				break;
			}

			rle = 1;
		}

		
		parport_write_control (port, ctl);

		
		if (parport_wait_peripheral (port, PARPORT_STATUS_ACK,
					     PARPORT_STATUS_ACK)) {
			DPRINTK (KERN_DEBUG "ECP read timed out at 45\n");

			if (command)
				printk (KERN_WARNING
					"%s: command ignored (%02x)\n",
					port->name, byte);

			break;
		}

		
		parport_write_control (port,
				       ctl | PARPORT_CONTROL_AUTOFD);

		
		if (command)
			continue;

		
		if (rle) {
			rle = 0;
			memset (buf, byte, rle_count);
			buf += rle_count;
			count += rle_count;
			DPRINTK (KERN_DEBUG "%s: decompressed to %d bytes\n",
				 port->name, rle_count);
		} else {
			
			*buf = byte;
			buf++, count++;
		}
	}

 out:
	port->ieee1284.phase = IEEE1284_PH_REV_IDLE;
	return count;
#endif 
}

size_t parport_ieee1284_ecp_write_addr (struct parport *port,
					const void *buffer, size_t len,
					int flags)
{
#ifndef CONFIG_PARPORT_1284
	return 0;
#else
	const unsigned char *buf = buffer;
	size_t written;
	int retry;

	port = port->physport;

	if (port->ieee1284.phase != IEEE1284_PH_FWD_IDLE)
		if (ecp_reverse_to_forward (port))
			return 0;

	port->ieee1284.phase = IEEE1284_PH_FWD_DATA;

	
	parport_frob_control (port,
			      PARPORT_CONTROL_AUTOFD
			      | PARPORT_CONTROL_STROBE
			      | PARPORT_CONTROL_INIT,
			      PARPORT_CONTROL_AUTOFD
			      | PARPORT_CONTROL_INIT);
	for (written = 0; written < len; written++, buf++) {
		unsigned long expire = jiffies + port->cad->timeout;
		unsigned char byte;

		byte = *buf;
	try_again:
		parport_write_data (port, byte);
		parport_frob_control (port, PARPORT_CONTROL_STROBE,
				      PARPORT_CONTROL_STROBE);
		udelay (5);
		for (retry = 0; retry < 100; retry++) {
			if (!parport_wait_peripheral (port,
						      PARPORT_STATUS_BUSY, 0))
				goto success;

			if (signal_pending (current)) {
				parport_frob_control (port,
						      PARPORT_CONTROL_STROBE,
						      0);
				break;
			}
		}

		
		DPRINTK (KERN_DEBUG "%s: ECP transfer stalled!\n", port->name);

		parport_frob_control (port, PARPORT_CONTROL_INIT,
				      PARPORT_CONTROL_INIT);
		udelay (50);
		if (parport_read_status (port) & PARPORT_STATUS_PAPEROUT) {
			
			parport_frob_control (port, PARPORT_CONTROL_INIT, 0);
			break;
		}

		parport_frob_control (port, PARPORT_CONTROL_INIT, 0);
		udelay (50);
		if (!(parport_read_status (port) & PARPORT_STATUS_PAPEROUT))
			break;

		DPRINTK (KERN_DEBUG "%s: Host transfer recovered\n",
			 port->name);

		if (time_after_eq (jiffies, expire)) break;
		goto try_again;
	success:
		parport_frob_control (port, PARPORT_CONTROL_STROBE, 0);
		udelay (5);
		if (parport_wait_peripheral (port,
					     PARPORT_STATUS_BUSY,
					     PARPORT_STATUS_BUSY))
			
			break;
	}

	port->ieee1284.phase = IEEE1284_PH_FWD_IDLE;

	return written;
#endif 
}


size_t parport_ieee1284_epp_write_data (struct parport *port,
					const void *buffer, size_t len,
					int flags)
{
	unsigned char *bp = (unsigned char *) buffer;
	size_t ret = 0;

	
	parport_frob_control (port,
			      PARPORT_CONTROL_STROBE |
			      PARPORT_CONTROL_AUTOFD |
			      PARPORT_CONTROL_SELECT |
			      PARPORT_CONTROL_INIT,
			      PARPORT_CONTROL_STROBE |
			      PARPORT_CONTROL_INIT);
	port->ops->data_forward (port);
	for (; len > 0; len--, bp++) {
		
		parport_write_data (port, *bp);
		parport_frob_control (port, PARPORT_CONTROL_AUTOFD,
				      PARPORT_CONTROL_AUTOFD);

		
		if (parport_poll_peripheral (port, PARPORT_STATUS_BUSY, 0, 10))
			break;

		
		parport_frob_control (port, PARPORT_CONTROL_AUTOFD, 0);

		
		if (parport_poll_peripheral (port, PARPORT_STATUS_BUSY,
					     PARPORT_STATUS_BUSY, 5))
			break;

		ret++;
	}

	
	parport_frob_control (port, PARPORT_CONTROL_STROBE, 0);

	return ret;
}

size_t parport_ieee1284_epp_read_data (struct parport *port,
				       void *buffer, size_t len,
				       int flags)
{
	unsigned char *bp = (unsigned char *) buffer;
	unsigned ret = 0;

	
	parport_frob_control (port,
			      PARPORT_CONTROL_STROBE |
			      PARPORT_CONTROL_AUTOFD |
			      PARPORT_CONTROL_SELECT |
			      PARPORT_CONTROL_INIT,
			      PARPORT_CONTROL_INIT);
	port->ops->data_reverse (port);
	for (; len > 0; len--, bp++) {
		
		parport_frob_control (port,
				      PARPORT_CONTROL_AUTOFD,
				      PARPORT_CONTROL_AUTOFD);
		
		if (parport_wait_peripheral (port, PARPORT_STATUS_BUSY, 0)) {
			break;
		}

		*bp = parport_read_data (port);

		
		parport_frob_control (port, PARPORT_CONTROL_AUTOFD, 0);

		
		if (parport_poll_peripheral (port, PARPORT_STATUS_BUSY,
					     PARPORT_STATUS_BUSY, 5)) {
			break;
		}

		ret++;
	}
	port->ops->data_forward (port);

	return ret;
}

size_t parport_ieee1284_epp_write_addr (struct parport *port,
					const void *buffer, size_t len,
					int flags)
{
	unsigned char *bp = (unsigned char *) buffer;
	size_t ret = 0;

	
	parport_frob_control (port,
			      PARPORT_CONTROL_STROBE |
			      PARPORT_CONTROL_AUTOFD |
			      PARPORT_CONTROL_SELECT |
			      PARPORT_CONTROL_INIT,
			      PARPORT_CONTROL_STROBE |
			      PARPORT_CONTROL_INIT);
	port->ops->data_forward (port);
	for (; len > 0; len--, bp++) {
		
		parport_write_data (port, *bp);
		parport_frob_control (port, PARPORT_CONTROL_SELECT,
				      PARPORT_CONTROL_SELECT);

		
		if (parport_poll_peripheral (port, PARPORT_STATUS_BUSY, 0, 10))
			break;

		
		parport_frob_control (port, PARPORT_CONTROL_SELECT, 0);

		
		if (parport_poll_peripheral (port, PARPORT_STATUS_BUSY,
					     PARPORT_STATUS_BUSY, 5))
			break;

		ret++;
	}

	
	parport_frob_control (port, PARPORT_CONTROL_STROBE, 0);

	return ret;
}

size_t parport_ieee1284_epp_read_addr (struct parport *port,
				       void *buffer, size_t len,
				       int flags)
{
	unsigned char *bp = (unsigned char *) buffer;
	unsigned ret = 0;

	
	parport_frob_control (port,
			      PARPORT_CONTROL_STROBE |
			      PARPORT_CONTROL_AUTOFD |
			      PARPORT_CONTROL_SELECT |
			      PARPORT_CONTROL_INIT,
			      PARPORT_CONTROL_INIT);
	port->ops->data_reverse (port);
	for (; len > 0; len--, bp++) {
		
		parport_frob_control (port, PARPORT_CONTROL_SELECT,
				      PARPORT_CONTROL_SELECT);

		
		if (parport_wait_peripheral (port, PARPORT_STATUS_BUSY, 0)) {
			break;
		}

		*bp = parport_read_data (port);

		
		parport_frob_control (port, PARPORT_CONTROL_SELECT,
				      0);

		
		if (parport_poll_peripheral (port, PARPORT_STATUS_BUSY, 
					     PARPORT_STATUS_BUSY, 5))
			break;

		ret++;
	}
	port->ops->data_forward (port);

	return ret;
}

EXPORT_SYMBOL(parport_ieee1284_ecp_write_data);
EXPORT_SYMBOL(parport_ieee1284_ecp_read_data);
EXPORT_SYMBOL(parport_ieee1284_ecp_write_addr);
EXPORT_SYMBOL(parport_ieee1284_write_compat);
EXPORT_SYMBOL(parport_ieee1284_read_nibble);
EXPORT_SYMBOL(parport_ieee1284_read_byte);
EXPORT_SYMBOL(parport_ieee1284_epp_write_data);
EXPORT_SYMBOL(parport_ieee1284_epp_read_data);
EXPORT_SYMBOL(parport_ieee1284_epp_write_addr);
EXPORT_SYMBOL(parport_ieee1284_epp_read_addr);
