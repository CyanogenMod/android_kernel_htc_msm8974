/*
 * Ultra Wide Band
 * UWB API
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
 * FIXME: doc: overview of the API, different parts and pointers
 */

#ifndef __LINUX__UWB_H__
#define __LINUX__UWB_H__

#include <linux/limits.h>
#include <linux/device.h>
#include <linux/mutex.h>
#include <linux/timer.h>
#include <linux/wait.h>
#include <linux/workqueue.h>
#include <linux/uwb/spec.h>
#include <asm/page.h>

struct uwb_dev;
struct uwb_beca_e;
struct uwb_rc;
struct uwb_rsv;
struct uwb_dbg;

struct uwb_dev {
	struct mutex mutex;
	struct list_head list_node;
	struct device dev;
	struct uwb_rc *rc;		
	struct uwb_beca_e *bce;		

	struct uwb_mac_addr mac_addr;
	struct uwb_dev_addr dev_addr;
	int beacon_slot;
	DECLARE_BITMAP(streams, UWB_NUM_STREAMS);
	DECLARE_BITMAP(last_availability_bm, UWB_NUM_MAS);
};
#define to_uwb_dev(d) container_of(d, struct uwb_dev, dev)

enum { UWB_RC_CTX_MAX = 256 };


struct uwb_notifs_chain {
	struct list_head list;
	struct mutex mutex;
};

struct uwb_beca {
	struct list_head list;
	size_t entries;
	struct mutex mutex;
};

struct uwbd {
	int pid;
	struct task_struct *task;
	wait_queue_head_t wq;
	struct list_head event_list;
	spinlock_t event_list_lock;
};

struct uwb_mas_bm {
	DECLARE_BITMAP(bm, UWB_NUM_MAS);
	DECLARE_BITMAP(unsafe_bm, UWB_NUM_MAS);
	int safe;
	int unsafe;
};

enum uwb_rsv_state {
	UWB_RSV_STATE_NONE = 0,
	UWB_RSV_STATE_O_INITIATED,
	UWB_RSV_STATE_O_PENDING,
	UWB_RSV_STATE_O_MODIFIED,
	UWB_RSV_STATE_O_ESTABLISHED,
	UWB_RSV_STATE_O_TO_BE_MOVED,
	UWB_RSV_STATE_O_MOVE_EXPANDING,
	UWB_RSV_STATE_O_MOVE_COMBINING,
	UWB_RSV_STATE_O_MOVE_REDUCING,
	UWB_RSV_STATE_T_ACCEPTED,
	UWB_RSV_STATE_T_DENIED,
	UWB_RSV_STATE_T_CONFLICT,
	UWB_RSV_STATE_T_PENDING,
	UWB_RSV_STATE_T_EXPANDING_ACCEPTED,
	UWB_RSV_STATE_T_EXPANDING_CONFLICT,
	UWB_RSV_STATE_T_EXPANDING_PENDING,
	UWB_RSV_STATE_T_EXPANDING_DENIED,
	UWB_RSV_STATE_T_RESIZED,

	UWB_RSV_STATE_LAST,
};

enum uwb_rsv_target_type {
	UWB_RSV_TARGET_DEV,
	UWB_RSV_TARGET_DEVADDR,
};

struct uwb_rsv_target {
	enum uwb_rsv_target_type type;
	union {
		struct uwb_dev *dev;
		struct uwb_dev_addr devaddr;
	};
};

struct uwb_rsv_move {
	struct uwb_mas_bm final_mas;
	struct uwb_ie_drp *companion_drp_ie;
	struct uwb_mas_bm companion_mas;
};

#define UWB_NUM_GLOBAL_STREAMS 1

typedef void (*uwb_rsv_cb_f)(struct uwb_rsv *rsv);

struct uwb_rsv {
	struct uwb_rc *rc;
	struct list_head rc_node;
	struct list_head pal_node;
	struct kref kref;

	struct uwb_dev *owner;
	struct uwb_rsv_target target;
	enum uwb_drp_type type;
	int max_mas;
	int min_mas;
	int max_interval;
	bool is_multicast;

	uwb_rsv_cb_f callback;
	void *pal_priv;

	enum uwb_rsv_state state;
	bool needs_release_companion_mas;
	u8 stream;
	u8 tiebreaker;
	struct uwb_mas_bm mas;
	struct uwb_ie_drp *drp_ie;
	struct uwb_rsv_move mv;
	bool ie_valid;
	struct timer_list timer;
	struct work_struct handle_timeout_work;
};

static const
struct uwb_mas_bm uwb_mas_bm_zero = { .bm = { 0 } };

static inline void uwb_mas_bm_copy_le(void *dst, const struct uwb_mas_bm *mas)
{
	bitmap_copy_le(dst, mas->bm, UWB_NUM_MAS);
}

struct uwb_drp_avail {
	DECLARE_BITMAP(global, UWB_NUM_MAS);
	DECLARE_BITMAP(local, UWB_NUM_MAS);
	DECLARE_BITMAP(pending, UWB_NUM_MAS);
	struct uwb_ie_drp_avail ie;
	bool ie_valid;
};

struct uwb_drp_backoff_win {
	u8 window;
	u8 n;
	int total_expired;
	struct timer_list timer;
	bool can_reserve_extra_mases;
};

const char *uwb_rsv_state_str(enum uwb_rsv_state state);
const char *uwb_rsv_type_str(enum uwb_drp_type type);

struct uwb_rsv *uwb_rsv_create(struct uwb_rc *rc, uwb_rsv_cb_f cb,
			       void *pal_priv);
void uwb_rsv_destroy(struct uwb_rsv *rsv);

int uwb_rsv_establish(struct uwb_rsv *rsv);
int uwb_rsv_modify(struct uwb_rsv *rsv,
		   int max_mas, int min_mas, int sparsity);
void uwb_rsv_terminate(struct uwb_rsv *rsv);

void uwb_rsv_accept(struct uwb_rsv *rsv, uwb_rsv_cb_f cb, void *pal_priv);

void uwb_rsv_get_usable_mas(struct uwb_rsv *orig_rsv, struct uwb_mas_bm *mas);

struct uwb_rc {
	struct uwb_dev uwb_dev;
	int index;
	u16 version;

	struct module *owner;
	void *priv;
	int (*start)(struct uwb_rc *rc);
	void (*stop)(struct uwb_rc *rc);
	int (*cmd)(struct uwb_rc *, const struct uwb_rccb *, size_t);
	int (*reset)(struct uwb_rc *rc);
	int (*filter_cmd)(struct uwb_rc *, struct uwb_rccb **, size_t *);
	int (*filter_event)(struct uwb_rc *, struct uwb_rceb **, const size_t,
			    size_t *, size_t *);

	spinlock_t neh_lock;		
	struct list_head neh_list;	
	unsigned long ctx_bm[UWB_RC_CTX_MAX / 8 / sizeof(unsigned long)];
	u8 ctx_roll;

	int beaconing;			
	int beaconing_forced;
	int scanning;
	enum uwb_scan_type scan_type:3;
	unsigned ready:1;
	struct uwb_notifs_chain notifs_chain;
	struct uwb_beca uwb_beca;

	struct uwbd uwbd;

	struct uwb_drp_backoff_win bow;
	struct uwb_drp_avail drp_avail;
	struct list_head reservations;
	struct list_head cnflt_alien_list;
	struct uwb_mas_bm cnflt_alien_bitmap;
	struct mutex rsvs_mutex;
	spinlock_t rsvs_lock;
	struct workqueue_struct *rsv_workq;

	struct delayed_work rsv_update_work;
	struct delayed_work rsv_alien_bp_work;
	int set_drp_ie_pending;
	struct mutex ies_mutex;
	struct uwb_rc_cmd_set_ie *ies;
	size_t ies_capacity;

	struct list_head pals;
	int active_pals;

	struct uwb_dbg *dbg;
};


struct uwb_pal {
	struct list_head node;
	const char *name;
	struct device *device;
	struct uwb_rc *rc;

	void (*channel_changed)(struct uwb_pal *pal, int channel);
	void (*new_rsv)(struct uwb_pal *pal, struct uwb_rsv *rsv);

	int channel;
	struct dentry *debugfs_dir;
};

void uwb_pal_init(struct uwb_pal *pal);
int uwb_pal_register(struct uwb_pal *pal);
void uwb_pal_unregister(struct uwb_pal *pal);

int uwb_radio_start(struct uwb_pal *pal);
void uwb_radio_stop(struct uwb_pal *pal);

struct uwb_dev *uwb_dev_get_by_devaddr(struct uwb_rc *rc,
				       const struct uwb_dev_addr *devaddr);
struct uwb_dev *uwb_dev_get_by_rc(struct uwb_dev *, struct uwb_rc *);
static inline void uwb_dev_get(struct uwb_dev *uwb_dev)
{
	get_device(&uwb_dev->dev);
}
static inline void uwb_dev_put(struct uwb_dev *uwb_dev)
{
	put_device(&uwb_dev->dev);
}
struct uwb_dev *uwb_dev_try_get(struct uwb_rc *rc, struct uwb_dev *uwb_dev);

typedef int (*uwb_dev_for_each_f)(struct device *dev, void *priv);
int uwb_dev_for_each(struct uwb_rc *rc, uwb_dev_for_each_f func, void *priv);

struct uwb_rc *uwb_rc_alloc(void);
struct uwb_rc *uwb_rc_get_by_dev(const struct uwb_dev_addr *);
struct uwb_rc *uwb_rc_get_by_grandpa(const struct device *);
void uwb_rc_put(struct uwb_rc *rc);

typedef void (*uwb_rc_cmd_cb_f)(struct uwb_rc *rc, void *arg,
                                struct uwb_rceb *reply, ssize_t reply_size);

int uwb_rc_cmd_async(struct uwb_rc *rc, const char *cmd_name,
		     struct uwb_rccb *cmd, size_t cmd_size,
		     u8 expected_type, u16 expected_event,
		     uwb_rc_cmd_cb_f cb, void *arg);
ssize_t uwb_rc_cmd(struct uwb_rc *rc, const char *cmd_name,
		   struct uwb_rccb *cmd, size_t cmd_size,
		   struct uwb_rceb *reply, size_t reply_size);
ssize_t uwb_rc_vcmd(struct uwb_rc *rc, const char *cmd_name,
		    struct uwb_rccb *cmd, size_t cmd_size,
		    u8 expected_type, u16 expected_event,
		    struct uwb_rceb **preply);

size_t __uwb_addr_print(char *, size_t, const unsigned char *, int);

int uwb_rc_dev_addr_set(struct uwb_rc *, const struct uwb_dev_addr *);
int uwb_rc_dev_addr_get(struct uwb_rc *, struct uwb_dev_addr *);
int uwb_rc_mac_addr_set(struct uwb_rc *, const struct uwb_mac_addr *);
int uwb_rc_mac_addr_get(struct uwb_rc *, struct uwb_mac_addr *);
int __uwb_mac_addr_assigned_check(struct device *, void *);
int __uwb_dev_addr_assigned_check(struct device *, void *);

static inline size_t uwb_dev_addr_print(char *buf, size_t buf_size,
					const struct uwb_dev_addr *addr)
{
	return __uwb_addr_print(buf, buf_size, addr->data, 0);
}

static inline size_t uwb_mac_addr_print(char *buf, size_t buf_size,
					const struct uwb_mac_addr *addr)
{
	return __uwb_addr_print(buf, buf_size, addr->data, 1);
}

static inline int uwb_dev_addr_cmp(const struct uwb_dev_addr *addr1,
				   const struct uwb_dev_addr *addr2)
{
	return memcmp(addr1, addr2, sizeof(*addr1));
}

static inline int uwb_mac_addr_cmp(const struct uwb_mac_addr *addr1,
				   const struct uwb_mac_addr *addr2)
{
	return memcmp(addr1, addr2, sizeof(*addr1));
}

static inline int uwb_mac_addr_bcast(const struct uwb_mac_addr *addr)
{
	struct uwb_mac_addr bcast = {
		.data = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff }
	};
	return !uwb_mac_addr_cmp(addr, &bcast);
}

static inline int uwb_mac_addr_unset(const struct uwb_mac_addr *addr)
{
	struct uwb_mac_addr unset = {
		.data = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }
	};
	return !uwb_mac_addr_cmp(addr, &unset);
}

static inline unsigned __uwb_dev_addr_assigned(struct uwb_rc *rc,
					       struct uwb_dev_addr *addr)
{
	return uwb_dev_for_each(rc, __uwb_dev_addr_assigned_check, addr);
}

void uwb_rc_init(struct uwb_rc *);
int uwb_rc_add(struct uwb_rc *, struct device *dev, void *rc_priv);
void uwb_rc_rm(struct uwb_rc *);
void uwb_rc_neh_grok(struct uwb_rc *, void *, size_t);
void uwb_rc_neh_error(struct uwb_rc *, int);
void uwb_rc_reset_all(struct uwb_rc *rc);
void uwb_rc_pre_reset(struct uwb_rc *rc);
int uwb_rc_post_reset(struct uwb_rc *rc);

static inline bool uwb_rsv_is_owner(struct uwb_rsv *rsv)
{
	return rsv->owner == &rsv->rc->uwb_dev;
}

enum uwb_notifs {
	UWB_NOTIF_ONAIR,
	UWB_NOTIF_OFFAIR,
};

struct uwb_notifs_handler {
	struct list_head list_node;
	void (*cb)(void *, struct uwb_dev *, enum uwb_notifs);
	void *data;
};

int uwb_notifs_register(struct uwb_rc *, struct uwb_notifs_handler *);
int uwb_notifs_deregister(struct uwb_rc *, struct uwb_notifs_handler *);


struct uwb_est_entry {
	size_t size;
	unsigned offset;
	enum { UWB_EST_16 = 0, UWB_EST_8 = 1 } type;
};

int uwb_est_register(u8 type, u8 code_high, u16 vendor, u16 product,
		     const struct uwb_est_entry *, size_t entries);
int uwb_est_unregister(u8 type, u8 code_high, u16 vendor, u16 product,
		       const struct uwb_est_entry *, size_t entries);
ssize_t uwb_est_find_size(struct uwb_rc *rc, const struct uwb_rceb *rceb,
			  size_t len);


enum {
	EDC_MAX_ERRORS = 10,
	EDC_ERROR_TIMEFRAME = HZ,
};

struct edc {
	unsigned long timestart;
	u16 errorcount;
};

static inline
void edc_init(struct edc *edc)
{
	edc->timestart = jiffies;
}

static inline int edc_inc(struct edc *err_hist, u16 max_err, u16 timeframe)
{
	unsigned long now;

	now = jiffies;
	if (now - err_hist->timestart > timeframe) {
		err_hist->errorcount = 1;
		err_hist->timestart = now;
	} else if (++err_hist->errorcount > max_err) {
			err_hist->errorcount = 0;
			err_hist->timestart = now;
			return 1;
	}
	return 0;
}



struct uwb_ie_hdr *uwb_ie_next(void **ptr, size_t *len);
int uwb_rc_ie_add(struct uwb_rc *uwb_rc, const struct uwb_ie_hdr *ies, size_t size);
int uwb_rc_ie_rm(struct uwb_rc *uwb_rc, enum uwb_ie element_id);

struct stats {
	s8 min, max;
	s16 sigma;
	atomic_t samples;
};

static inline
void stats_init(struct stats *stats)
{
	atomic_set(&stats->samples, 0);
	wmb();
}

static inline
void stats_add_sample(struct stats *stats, s8 sample)
{
	s8 min, max;
	s16 sigma;
	unsigned samples = atomic_read(&stats->samples);
	if (samples == 0) {	
		min = 127;
		max = -128;
		sigma = 0;
	} else {
		min = stats->min;
		max = stats->max;
		sigma = stats->sigma;
	}

	if (sample < min)	
		min = sample;
	else if (sample > max)
		max = sample;
	sigma += sample;

	stats->min = min;	
	stats->max = max;
	stats->sigma = sigma;
	if (atomic_add_return(1, &stats->samples) > 255) {
		
		stats->sigma = sigma / 256;
		atomic_set(&stats->samples, 1);
	}
}

static inline ssize_t stats_show(struct stats *stats, char *buf)
{
	int min, max, avg;
	int samples = atomic_read(&stats->samples);
	if (samples == 0)
		min = max = avg = 0;
	else {
		min = stats->min;
		max = stats->max;
		avg = stats->sigma / samples;
	}
	return scnprintf(buf, PAGE_SIZE, "%d %d %d\n", min, max, avg);
}

static inline ssize_t stats_store(struct stats *stats, const char *buf,
				  size_t size)
{
	stats_init(stats);
	return size;
}

#endif 
