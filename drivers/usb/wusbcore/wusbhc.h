/*
 * Wireless USB Host Controller
 * Common infrastructure for WHCI and HWA WUSB-HC drivers
 *
 *
 * Copyright (C) 2005-2006 Intel Corporation
 * Inaky Perez-Gonzalez <inaky.perez-gonzalez@intel.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 *
 * This driver implements parts common to all Wireless USB Host
 * Controllers (struct wusbhc, embedding a struct usb_hcd) and is used
 * by:
 *
 *   - hwahc: HWA, USB-dongle that implements a Wireless USB host
 *     controller, (Wireless USB 1.0 Host-Wire-Adapter specification).
 *
 *   - whci: WHCI, a PCI card with a wireless host controller
 *     (Wireless Host Controller Interface 1.0 specification).
 *
 * Check out the Design-overview.txt file in the source documentation
 * for other details on the implementation.
 *
 * Main blocks:
 *
 *  rh         Root Hub emulation (part of the HCD glue)
 *
 *  devconnect Handle all the issues related to device connection,
 *             authentication, disconnection, timeout, reseting,
 *             keepalives, etc.
 *
 *  mmc        MMC IE broadcasting handling
 *
 * A host controller driver just initializes its stuff and as part of
 * that, creates a 'struct wusbhc' instance that handles all the
 * common WUSB mechanisms. Links in the function ops that are specific
 * to it and then registers the host controller. Ready to run.
 */

#ifndef __WUSBHC_H__
#define __WUSBHC_H__

#include <linux/usb.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/kref.h>
#include <linux/workqueue.h>
#include <linux/usb/hcd.h>
#include <linux/uwb.h>
#include <linux/usb/wusb.h>

#define WUSB_CHANNEL_STOP_DELAY_MS 8

struct wusb_dev {
	struct kref refcnt;
	struct wusbhc *wusbhc;
	struct list_head cack_node;	
	u8 port_idx;
	u8 addr;
	u8 beacon_type:4;
	struct usb_encryption_descriptor ccm1_etd;
	struct wusb_ckhdid cdid;
	unsigned long entry_ts;
	struct usb_bos_descriptor *bos;
	struct usb_wireless_cap_descriptor *wusb_cap_descr;
	struct uwb_mas_bm availability;
	struct work_struct devconnect_acked_work;
	struct urb *set_gtk_urb;
	struct usb_ctrlrequest *set_gtk_req;
	struct usb_device *usb_dev;
};

#define WUSB_DEV_ADDR_UNAUTH 0x80

static inline void wusb_dev_init(struct wusb_dev *wusb_dev)
{
	kref_init(&wusb_dev->refcnt);
	
}

extern void wusb_dev_destroy(struct kref *_wusb_dev);

static inline struct wusb_dev *wusb_dev_get(struct wusb_dev *wusb_dev)
{
	kref_get(&wusb_dev->refcnt);
	return wusb_dev;
}

static inline void wusb_dev_put(struct wusb_dev *wusb_dev)
{
	kref_put(&wusb_dev->refcnt, wusb_dev_destroy);
}

struct wusb_port {
	u16 status;
	u16 change;
	struct wusb_dev *wusb_dev;	
	u32 ptk_tkid;
};

struct wusbhc {
	struct usb_hcd usb_hcd;		
	struct device *dev;
	struct uwb_rc *uwb_rc;
	struct uwb_pal pal;

	unsigned trust_timeout;			
	struct wusb_ckhdid chid;
	uint8_t phy_rate;
	struct wuie_host_info *wuie_host_info;

	struct mutex mutex;			
	u16 cluster_id;				
	struct wusb_port *port;			
	struct wusb_dev_info *dev_info;		
	u8 ports_max;
	unsigned active:1;			
	struct wuie_keep_alive keep_alive_ie;	
	struct delayed_work keep_alive_timer;
	struct list_head cack_list;		
	size_t cack_count;			
	struct wuie_connect_ack cack_ie;
	struct uwb_rsv *rsv;		

	struct mutex mmcie_mutex;		
	struct wuie_hdr **mmcie;		
	u8 mmcies_max;
	
	int (*start)(struct wusbhc *wusbhc);
	void (*stop)(struct wusbhc *wusbhc, int delay);
	int (*mmcie_add)(struct wusbhc *wusbhc, u8 interval, u8 repeat_cnt,
			 u8 handle, struct wuie_hdr *wuie);
	int (*mmcie_rm)(struct wusbhc *wusbhc, u8 handle);
	int (*dev_info_set)(struct wusbhc *, struct wusb_dev *wusb_dev);
	int (*bwa_set)(struct wusbhc *wusbhc, s8 stream_index,
		       const struct uwb_mas_bm *);
	int (*set_ptk)(struct wusbhc *wusbhc, u8 port_idx,
		       u32 tkid, const void *key, size_t key_size);
	int (*set_gtk)(struct wusbhc *wusbhc,
		       u32 tkid, const void *key, size_t key_size);
	int (*set_num_dnts)(struct wusbhc *wusbhc, u8 interval, u8 slots);

	struct {
		struct usb_key_descriptor descr;
		u8 data[16];				
	} __attribute__((packed)) gtk;
	u8 gtk_index;
	u32 gtk_tkid;
	struct work_struct gtk_rekey_done_work;
	int pending_set_gtks;

	struct usb_encryption_descriptor *ccm1_etd;
};

#define usb_hcd_to_wusbhc(u) container_of((u), struct wusbhc, usb_hcd)


extern int wusbhc_create(struct wusbhc *);
extern int wusbhc_b_create(struct wusbhc *);
extern void wusbhc_b_destroy(struct wusbhc *);
extern void wusbhc_destroy(struct wusbhc *);
extern int wusb_dev_sysfs_add(struct wusbhc *, struct usb_device *,
			      struct wusb_dev *);
extern void wusb_dev_sysfs_rm(struct wusb_dev *);
extern int wusbhc_sec_create(struct wusbhc *);
extern int wusbhc_sec_start(struct wusbhc *);
extern void wusbhc_sec_stop(struct wusbhc *);
extern void wusbhc_sec_destroy(struct wusbhc *);
extern void wusbhc_giveback_urb(struct wusbhc *wusbhc, struct urb *urb,
				int status);
void wusbhc_reset_all(struct wusbhc *wusbhc);

int wusbhc_pal_register(struct wusbhc *wusbhc);
void wusbhc_pal_unregister(struct wusbhc *wusbhc);

static inline struct usb_hcd *usb_hcd_get_by_usb_dev(struct usb_device *usb_dev)
{
	struct usb_hcd *usb_hcd;
	usb_hcd = container_of(usb_dev->bus, struct usb_hcd, self);
	return usb_get_hcd(usb_hcd);
}

static inline struct wusbhc *wusbhc_get(struct wusbhc *wusbhc)
{
	return usb_get_hcd(&wusbhc->usb_hcd) ? wusbhc : NULL;
}

static inline struct wusbhc *wusbhc_get_by_usb_dev(struct usb_device *usb_dev)
{
	struct wusbhc *wusbhc = NULL;
	struct usb_hcd *usb_hcd;
	if (usb_dev->devnum > 1 && !usb_dev->wusb) {
		
		dev_err(&usb_dev->dev, "devnum %d wusb %d\n", usb_dev->devnum,
			usb_dev->wusb);
		BUG_ON(usb_dev->devnum > 1 && !usb_dev->wusb);
	}
	usb_hcd = usb_hcd_get_by_usb_dev(usb_dev);
	if (usb_hcd == NULL)
		return NULL;
	BUG_ON(usb_hcd->wireless == 0);
	return wusbhc = usb_hcd_to_wusbhc(usb_hcd);
}


static inline void wusbhc_put(struct wusbhc *wusbhc)
{
	usb_put_hcd(&wusbhc->usb_hcd);
}

int wusbhc_start(struct wusbhc *wusbhc);
void wusbhc_stop(struct wusbhc *wusbhc);
extern int wusbhc_chid_set(struct wusbhc *, const struct wusb_ckhdid *);

extern int wusbhc_devconnect_create(struct wusbhc *);
extern void wusbhc_devconnect_destroy(struct wusbhc *);
extern int wusbhc_devconnect_start(struct wusbhc *wusbhc);
extern void wusbhc_devconnect_stop(struct wusbhc *wusbhc);
extern void wusbhc_handle_dn(struct wusbhc *, u8 srcaddr,
			     struct wusb_dn_hdr *dn_hdr, size_t size);
extern void __wusbhc_dev_disable(struct wusbhc *wusbhc, u8 port);
extern int wusb_usb_ncb(struct notifier_block *nb, unsigned long val,
			void *priv);
extern int wusb_set_dev_addr(struct wusbhc *wusbhc, struct wusb_dev *wusb_dev,
			     u8 addr);

extern int wusbhc_rh_create(struct wusbhc *);
extern void wusbhc_rh_destroy(struct wusbhc *);

extern int wusbhc_rh_status_data(struct usb_hcd *, char *);
extern int wusbhc_rh_control(struct usb_hcd *, u16, u16, u16, char *, u16);
extern int wusbhc_rh_suspend(struct usb_hcd *);
extern int wusbhc_rh_resume(struct usb_hcd *);
extern int wusbhc_rh_start_port_reset(struct usb_hcd *, unsigned);

extern int wusbhc_mmcie_create(struct wusbhc *);
extern void wusbhc_mmcie_destroy(struct wusbhc *);
extern int wusbhc_mmcie_set(struct wusbhc *, u8 interval, u8 repeat_cnt,
			    struct wuie_hdr *);
extern void wusbhc_mmcie_rm(struct wusbhc *, struct wuie_hdr *);

int wusbhc_rsv_establish(struct wusbhc *wusbhc);
void wusbhc_rsv_terminate(struct wusbhc *wusbhc);

extern int wusb_dev_sec_add(struct wusbhc *, struct usb_device *,
				struct wusb_dev *);
extern void wusb_dev_sec_rm(struct wusb_dev *) ;
extern int wusb_dev_4way_handshake(struct wusbhc *, struct wusb_dev *,
				   struct wusb_ckhdid *ck);
void wusbhc_gtk_rekey(struct wusbhc *wusbhc);
int wusb_dev_update_address(struct wusbhc *wusbhc, struct wusb_dev *wusb_dev);


extern u8 wusb_cluster_id_get(void);
extern void wusb_cluster_id_put(u8);

static inline struct wusb_port *wusb_port_by_idx(struct wusbhc *wusbhc,
						 u8 port_idx)
{
	return &wusbhc->port[port_idx];
}

static inline u8 wusb_port_no_to_idx(u8 port_no)
{
	return port_no - 1;
}

extern struct wusb_dev *__wusb_dev_get_by_usb_dev(struct wusbhc *,
						  struct usb_device *);

static inline
struct wusb_dev *wusb_dev_get_by_usb_dev(struct usb_device *usb_dev)
{
	struct wusbhc *wusbhc;
	struct wusb_dev *wusb_dev;
	wusbhc = wusbhc_get_by_usb_dev(usb_dev);
	if (wusbhc == NULL)
		return NULL;
	mutex_lock(&wusbhc->mutex);
	wusb_dev = __wusb_dev_get_by_usb_dev(wusbhc, usb_dev);
	mutex_unlock(&wusbhc->mutex);
	wusbhc_put(wusbhc);
	return wusb_dev;
}


extern struct workqueue_struct *wusbd;
#endif 
