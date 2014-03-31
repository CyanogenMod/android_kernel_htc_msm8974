/*
 * The USB Monitor, inspired by Dave Harding's USBMon.
 *
 * Copyright (C) 2005 Pete Zaitcev (zaitcev@redhat.com)
 */

#ifndef __USB_MON_H
#define __USB_MON_H

#include <linux/list.h>
#include <linux/slab.h>
#include <linux/kref.h>
	

#define TAG "usbmon"

struct mon_bus {
	struct list_head bus_link;
	spinlock_t lock;
	struct usb_bus *u_bus;

	int text_inited;
	int bin_inited;
	struct dentry *dent_s;		
	struct dentry *dent_t;		
	struct dentry *dent_u;		
	struct device *classdev;	

	
	int nreaders;			
	struct list_head r_list;	
	struct kref ref;		

	
	unsigned int cnt_events;
	unsigned int cnt_text_lost;
};

struct mon_reader {
	struct list_head r_link;
	struct mon_bus *m_bus;
	void *r_data;		

	void (*rnf_submit)(void *data, struct urb *urb);
	void (*rnf_error)(void *data, struct urb *urb, int error);
	void (*rnf_complete)(void *data, struct urb *urb, int status);
};

void mon_reader_add(struct mon_bus *mbus, struct mon_reader *r);
void mon_reader_del(struct mon_bus *mbus, struct mon_reader *r);

struct mon_bus *mon_bus_lookup(unsigned int num);

int  mon_text_add(struct mon_bus *mbus, const struct usb_bus *ubus);
void mon_text_del(struct mon_bus *mbus);
int  mon_bin_add(struct mon_bus *mbus, const struct usb_bus *ubus);
void mon_bin_del(struct mon_bus *mbus);

int __init mon_text_init(void);
void mon_text_exit(void);
int __init mon_bin_init(void);
void mon_bin_exit(void);

extern struct mutex mon_lock;

extern const struct file_operations mon_fops_stat;

extern struct mon_bus mon_bus0;		

#endif 
