/*
 * Filesystem based user-mode API to USB Gadget controller hardware
 *
 * Other than ep0 operations, most things are done by read() and write()
 * on endpoint files found in one directory.  They are configured by
 * writing descriptors, and then may be used for normal stream style
 * i/o requests.  When ep0 is configured, the device can enumerate;
 * when it's closed, the device disconnects from usb.  Operations on
 * ep0 require ioctl() operations.
 *
 * Configuration and device descriptors get written to /dev/gadget/$CHIP,
 * which may then be used to read usb_gadgetfs_event structs.  The driver
 * may activate endpoints as it handles SET_CONFIGURATION setup events,
 * or earlier; writing endpoint descriptors to /dev/gadget/$ENDPOINT
 * then performing data transfers by reading or writing.
 */

#ifndef __LINUX_USB_GADGETFS_H
#define __LINUX_USB_GADGETFS_H

#include <linux/types.h>
#include <linux/ioctl.h>

#include <linux/usb/ch9.h>


enum usb_gadgetfs_event_type {
	GADGETFS_NOP = 0,

	GADGETFS_CONNECT,
	GADGETFS_DISCONNECT,
	GADGETFS_SETUP,
	GADGETFS_SUSPEND,
	
};

struct usb_gadgetfs_event {
	union {

		
		enum usb_device_speed	speed;

		struct usb_ctrlrequest	setup;
	} u;
	enum usb_gadgetfs_event_type	type;
};




#define	GADGETFS_FIFO_STATUS	_IO('g', 1)

#define	GADGETFS_FIFO_FLUSH	_IO('g', 2)

#define	GADGETFS_CLEAR_HALT	_IO('g', 3)

#endif 
