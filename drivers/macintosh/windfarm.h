/*
 * Windfarm PowerMac thermal control.
 *
 * (c) Copyright 2005 Benjamin Herrenschmidt, IBM Corp.
 *                    <benh@kernel.crashing.org>
 *
 * Released under the term of the GNU GPL v2.
 */

#ifndef __WINDFARM_H__
#define __WINDFARM_H__

#include <linux/kref.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/notifier.h>
#include <linux/device.h>

#define FIX32TOPRINT(f)	((f) >> 16),((((f) & 0xffff) * 1000) >> 16)


struct wf_control;

struct wf_control_ops {
	int			(*set_value)(struct wf_control *ct, s32 val);
	int			(*get_value)(struct wf_control *ct, s32 *val);
	s32			(*get_min)(struct wf_control *ct);
	s32			(*get_max)(struct wf_control *ct);
	void			(*release)(struct wf_control *ct);
	struct module		*owner;
};

struct wf_control {
	struct list_head	link;
	struct wf_control_ops	*ops;
	char			*name;
	int			type;
	struct kref		ref;
	struct device_attribute	attr;
};

#define WF_CONTROL_TYPE_GENERIC		0
#define WF_CONTROL_RPM_FAN		1
#define WF_CONTROL_PWM_FAN		2


extern int wf_register_control(struct wf_control *ct);
extern void wf_unregister_control(struct wf_control *ct);
extern struct wf_control * wf_find_control(const char *name);
extern int wf_get_control(struct wf_control *ct);
extern void wf_put_control(struct wf_control *ct);

static inline int wf_control_set_max(struct wf_control *ct)
{
	s32 vmax = ct->ops->get_max(ct);
	return ct->ops->set_value(ct, vmax);
}

static inline int wf_control_set_min(struct wf_control *ct)
{
	s32 vmin = ct->ops->get_min(ct);
	return ct->ops->set_value(ct, vmin);
}


struct wf_sensor;

struct wf_sensor_ops {
	int			(*get_value)(struct wf_sensor *sr, s32 *val);
	void			(*release)(struct wf_sensor *sr);
	struct module		*owner;
};

struct wf_sensor {
	struct list_head	link;
	struct wf_sensor_ops	*ops;
	char			*name;
	struct kref		ref;
	struct device_attribute	attr;
};

extern int wf_register_sensor(struct wf_sensor *sr);
extern void wf_unregister_sensor(struct wf_sensor *sr);
extern struct wf_sensor * wf_find_sensor(const char *name);
extern int wf_get_sensor(struct wf_sensor *sr);
extern void wf_put_sensor(struct wf_sensor *sr);

extern int wf_register_client(struct notifier_block *nb);
extern int wf_unregister_client(struct notifier_block *nb);

extern void wf_set_overtemp(void);
extern void wf_clear_overtemp(void);
extern int wf_is_overtemp(void);

#define WF_EVENT_NEW_CONTROL	0 
#define WF_EVENT_NEW_SENSOR	1 
#define WF_EVENT_OVERTEMP	2 
#define WF_EVENT_NORMALTEMP	3 
#define WF_EVENT_TICK		4 


#endif 
