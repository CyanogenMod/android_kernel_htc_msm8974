/* Siemens ID Mouse driver v0.6

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License as
  published by the Free Software Foundation; either version 2 of
  the License, or (at your option) any later version.

  Copyright (C) 2004-5 by Florian 'Floe' Echtler  <echtler@fs.tum.de>
                      and Andreas  'ad'  Deresch <aderesch@fs.tum.de>

  Derived from the USB Skeleton driver 1.1,
  Copyright (C) 2003 Greg Kroah-Hartman (greg@kroah.com)

  Additional information provided by Martin Reising
  <Martin.Reising@natural-computing.de>

*/

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/completion.h>
#include <linux/mutex.h>
#include <asm/uaccess.h>
#include <linux/usb.h>

#define WIDTH 225
#define HEIGHT 289
#define HEADER "P5 225 289 255 "
#define IMGSIZE ((WIDTH * HEIGHT) + sizeof(HEADER)-1)

#define DRIVER_VERSION "0.6"
#define DRIVER_SHORT   "idmouse"
#define DRIVER_AUTHOR  "Florian 'Floe' Echtler <echtler@fs.tum.de>"
#define DRIVER_DESC    "Siemens ID Mouse FingerTIP Sensor Driver"

#define USB_IDMOUSE_MINOR_BASE 132

#define ID_SIEMENS 0x0681
#define ID_IDMOUSE 0x0005
#define ID_CHERRY  0x0010

static const struct usb_device_id idmouse_table[] = {
	{USB_DEVICE(ID_SIEMENS, ID_IDMOUSE)}, 
	{USB_DEVICE(ID_SIEMENS, ID_CHERRY )}, 
	{}                                    
};

#define FTIP_RESET   0x20
#define FTIP_ACQUIRE 0x21
#define FTIP_RELEASE 0x22
#define FTIP_BLINK   0x23  
#define FTIP_SCROLL  0x24

#define ftip_command(dev, command, value, index) \
	usb_control_msg (dev->udev, usb_sndctrlpipe (dev->udev, 0), command, \
	USB_TYPE_VENDOR | USB_RECIP_ENDPOINT | USB_DIR_OUT, value, index, NULL, 0, 1000)

MODULE_DEVICE_TABLE(usb, idmouse_table);
static DEFINE_MUTEX(open_disc_mutex);

struct usb_idmouse {

	struct usb_device *udev; 
	struct usb_interface *interface; 

	unsigned char *bulk_in_buffer; 
	size_t bulk_in_size; 
	size_t orig_bi_size; 
	__u8 bulk_in_endpointAddr; 

	int open; 
	int present; 
	struct mutex lock; 

};

static ssize_t idmouse_read(struct file *file, char __user *buffer,
				size_t count, loff_t * ppos);

static int idmouse_open(struct inode *inode, struct file *file);
static int idmouse_release(struct inode *inode, struct file *file);

static int idmouse_probe(struct usb_interface *interface,
				const struct usb_device_id *id);

static void idmouse_disconnect(struct usb_interface *interface);
static int idmouse_suspend(struct usb_interface *intf, pm_message_t message);
static int idmouse_resume(struct usb_interface *intf);

static const struct file_operations idmouse_fops = {
	.owner = THIS_MODULE,
	.read = idmouse_read,
	.open = idmouse_open,
	.release = idmouse_release,
	.llseek = default_llseek,
};

static struct usb_class_driver idmouse_class = {
	.name = "idmouse%d",
	.fops = &idmouse_fops,
	.minor_base = USB_IDMOUSE_MINOR_BASE,
};

static struct usb_driver idmouse_driver = {
	.name = DRIVER_SHORT,
	.probe = idmouse_probe,
	.disconnect = idmouse_disconnect,
	.suspend = idmouse_suspend,
	.resume = idmouse_resume,
	.reset_resume = idmouse_resume,
	.id_table = idmouse_table,
	.supports_autosuspend = 1,
};

static int idmouse_create_image(struct usb_idmouse *dev)
{
	int bytes_read;
	int bulk_read;
	int result;

	memcpy(dev->bulk_in_buffer, HEADER, sizeof(HEADER)-1);
	bytes_read = sizeof(HEADER)-1;

	
	result = ftip_command(dev, FTIP_RELEASE, 0, 0);
	if (result < 0)
		goto reset;
	result = ftip_command(dev, FTIP_BLINK,   1, 0);
	if (result < 0)
		goto reset;

	
	
	result = ftip_command(dev, FTIP_ACQUIRE, 0, 0);
	if (result < 0)
		goto reset;
	result = ftip_command(dev, FTIP_ACQUIRE, 0, 0);
	if (result < 0)
		goto reset;

	
	
	result = ftip_command(dev, FTIP_RESET,   0, 0);
	if (result < 0)
		goto reset;
	result = ftip_command(dev, FTIP_RESET,   0, 0);
	if (result < 0)
		goto reset;

	
	while (bytes_read < IMGSIZE) {
		result = usb_bulk_msg (dev->udev,
				usb_rcvbulkpipe (dev->udev, dev->bulk_in_endpointAddr),
				dev->bulk_in_buffer + bytes_read,
				dev->bulk_in_size, &bulk_read, 5000);
		if (result < 0) {
			
			
			if (dev->bulk_in_size != dev->orig_bi_size) {
				dev->bulk_in_size = dev->orig_bi_size;
				result = -EAGAIN;
			}
			break;
		}
		if (signal_pending(current)) {
			result = -EINTR;
			break;
		}
		bytes_read += bulk_read;
	}

	
reset:
	ftip_command(dev, FTIP_RELEASE, 0, 0);

	
	
	for (bytes_read = sizeof(HEADER)-1 + WIDTH-1; bytes_read < IMGSIZE; bytes_read += WIDTH)
		if (dev->bulk_in_buffer[bytes_read] != 0x00)
			return -EAGAIN;

	
	for (bytes_read = IMGSIZE-WIDTH; bytes_read < IMGSIZE-1; bytes_read++)
		if (dev->bulk_in_buffer[bytes_read] != 0xFF)
			return -EAGAIN;

	
	dbg("read %d bytes fingerprint data", bytes_read);
	return result;
}

static int idmouse_suspend(struct usb_interface *intf, pm_message_t message)
{
	return 0;
}

static int idmouse_resume(struct usb_interface *intf)
{
	return 0;
}

static inline void idmouse_delete(struct usb_idmouse *dev)
{
	kfree(dev->bulk_in_buffer);
	kfree(dev);
}

static int idmouse_open(struct inode *inode, struct file *file)
{
	struct usb_idmouse *dev;
	struct usb_interface *interface;
	int result;

	
	interface = usb_find_interface (&idmouse_driver, iminor (inode));
	if (!interface)
		return -ENODEV;

	mutex_lock(&open_disc_mutex);
	
	dev = usb_get_intfdata(interface);
	if (!dev) {
		mutex_unlock(&open_disc_mutex);
		return -ENODEV;
	}

	
	mutex_lock(&dev->lock);
	mutex_unlock(&open_disc_mutex);

	
	if (dev->open) {

		
		result = -EBUSY;

	} else {

		
		result = usb_autopm_get_interface(interface);
		if (result)
			goto error;
		result = idmouse_create_image (dev);
		if (result)
			goto error;
		usb_autopm_put_interface(interface);

		
		++dev->open;

		
		file->private_data = dev;

	} 

error:

	
	mutex_unlock(&dev->lock);
	return result;
}

static int idmouse_release(struct inode *inode, struct file *file)
{
	struct usb_idmouse *dev;

	dev = file->private_data;

	if (dev == NULL)
		return -ENODEV;

	mutex_lock(&open_disc_mutex);
	
	mutex_lock(&dev->lock);

	
	if (dev->open <= 0) {
		mutex_unlock(&dev->lock);
		mutex_unlock(&open_disc_mutex);
		return -ENODEV;
	}

	--dev->open;

	if (!dev->present) {
		
		mutex_unlock(&dev->lock);
		mutex_unlock(&open_disc_mutex);
		idmouse_delete(dev);
	} else {
		mutex_unlock(&dev->lock);
		mutex_unlock(&open_disc_mutex);
	}
	return 0;
}

static ssize_t idmouse_read(struct file *file, char __user *buffer, size_t count,
				loff_t * ppos)
{
	struct usb_idmouse *dev = file->private_data;
	int result;

	
	mutex_lock(&dev->lock);

	
	if (!dev->present) {
		mutex_unlock(&dev->lock);
		return -ENODEV;
	}

	result = simple_read_from_buffer(buffer, count, ppos,
					dev->bulk_in_buffer, IMGSIZE);
	
	mutex_unlock(&dev->lock);
	return result;
}

static int idmouse_probe(struct usb_interface *interface,
				const struct usb_device_id *id)
{
	struct usb_device *udev = interface_to_usbdev(interface);
	struct usb_idmouse *dev;
	struct usb_host_interface *iface_desc;
	struct usb_endpoint_descriptor *endpoint;
	int result;

	
	iface_desc = &interface->altsetting[0];
	if (iface_desc->desc.bInterfaceClass != 0x0A)
		return -ENODEV;

	
	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (dev == NULL)
		return -ENOMEM;

	mutex_init(&dev->lock);
	dev->udev = udev;
	dev->interface = interface;

	
	endpoint = &iface_desc->endpoint[0].desc;
	if (!dev->bulk_in_endpointAddr && usb_endpoint_is_bulk_in(endpoint)) {
		
		dev->orig_bi_size = usb_endpoint_maxp(endpoint);
		dev->bulk_in_size = 0x200; 
		dev->bulk_in_endpointAddr = endpoint->bEndpointAddress;
		dev->bulk_in_buffer =
			kmalloc(IMGSIZE + dev->bulk_in_size, GFP_KERNEL);

		if (!dev->bulk_in_buffer) {
			err("Unable to allocate input buffer.");
			idmouse_delete(dev);
			return -ENOMEM;
		}
	}

	if (!(dev->bulk_in_endpointAddr)) {
		err("Unable to find bulk-in endpoint.");
		idmouse_delete(dev);
		return -ENODEV;
	}
	
	dev->present = 1;

	
	usb_set_intfdata(interface, dev);
	result = usb_register_dev(interface, &idmouse_class);
	if (result) {
		
		err("Unble to allocate minor number.");
		usb_set_intfdata(interface, NULL);
		idmouse_delete(dev);
		return result;
	}

	
	dev_info(&interface->dev,"%s now attached\n",DRIVER_DESC);

	return 0;
}

static void idmouse_disconnect(struct usb_interface *interface)
{
	struct usb_idmouse *dev;

	
	dev = usb_get_intfdata(interface);

	
	usb_deregister_dev(interface, &idmouse_class);

	mutex_lock(&open_disc_mutex);
	usb_set_intfdata(interface, NULL);
	
	mutex_lock(&dev->lock);
	mutex_unlock(&open_disc_mutex);

	
	dev->present = 0;

	
	if (!dev->open) {
		mutex_unlock(&dev->lock);
		idmouse_delete(dev);
	} else {
		
		mutex_unlock(&dev->lock);
	}

	dev_info(&interface->dev, "disconnected\n");
}

module_usb_driver(idmouse_driver);

MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL");

