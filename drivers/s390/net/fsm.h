#ifndef _FSM_H_
#define _FSM_H_

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/timer.h>
#include <linux/time.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/string.h>
#include <linux/atomic.h>

#define FSM_DEBUG         0

#define FSM_TIMER_DEBUG   0

#define FSM_DEBUG_HISTORY 0
#define FSM_HISTORY_SIZE  40

struct fsm_instance_t;

typedef void (*fsm_function_t)(struct fsm_instance_t *, int, void *);

typedef struct {
	fsm_function_t *jumpmatrix;
	int nr_events;
	int nr_states;
	const char **event_names;
	const char **state_names;
} fsm;

#if FSM_DEBUG_HISTORY
typedef struct {
	int state;
	int event;
} fsm_history;
#endif

typedef struct fsm_instance_t {
	fsm *f;
	atomic_t state;
	char name[16];
	void *userdata;
	int userint;
	wait_queue_head_t wait_q;
#if FSM_DEBUG_HISTORY
	int         history_index;
	int         history_size;
	fsm_history history[FSM_HISTORY_SIZE];
#endif
} fsm_instance;

typedef struct {
	int cond_state;
	int cond_event;
	fsm_function_t function;
} fsm_node;

typedef struct {
	fsm_instance *fi;
	struct timer_list tl;
	int expire_event;
	void *event_arg;
} fsm_timer;

extern fsm_instance *
init_fsm(char *name, const char **state_names,
	 const char **event_names,
	 int nr_states, int nr_events, const fsm_node *tmpl,
	 int tmpl_len, gfp_t order);

extern void kfree_fsm(fsm_instance *fi);

#if FSM_DEBUG_HISTORY
extern void
fsm_print_history(fsm_instance *fi);

extern void
fsm_record_history(fsm_instance *fi, int state, int event);
#endif

static inline int
fsm_event(fsm_instance *fi, int event, void *arg)
{
	fsm_function_t r;
	int state = atomic_read(&fi->state);

	if ((state >= fi->f->nr_states) ||
	    (event >= fi->f->nr_events)       ) {
		printk(KERN_ERR "fsm(%s): Invalid state st(%ld/%ld) ev(%d/%ld)\n",
			fi->name, (long)state,(long)fi->f->nr_states, event,
			(long)fi->f->nr_events);
#if FSM_DEBUG_HISTORY
		fsm_print_history(fi);
#endif
		return 1;
	}
	r = fi->f->jumpmatrix[fi->f->nr_states * event + state];
	if (r) {
#if FSM_DEBUG
		printk(KERN_DEBUG "fsm(%s): state %s event %s\n",
		       fi->name, fi->f->state_names[state],
		       fi->f->event_names[event]);
#endif
#if FSM_DEBUG_HISTORY
		fsm_record_history(fi, state, event);
#endif
		r(fi, event, arg);
		return 0;
	} else {
#if FSM_DEBUG || FSM_DEBUG_HISTORY
		printk(KERN_DEBUG "fsm(%s): no function for event %s in state %s\n",
		       fi->name, fi->f->event_names[event],
		       fi->f->state_names[state]);
#endif
#if FSM_DEBUG_HISTORY
		fsm_print_history(fi);
#endif
		return !0;
	}
}

static inline void
fsm_newstate(fsm_instance *fi, int newstate)
{
	atomic_set(&fi->state,newstate);
#if FSM_DEBUG_HISTORY
	fsm_record_history(fi, newstate, -1);
#endif
#if FSM_DEBUG
	printk(KERN_DEBUG "fsm(%s): New state %s\n", fi->name,
		fi->f->state_names[newstate]);
#endif
	wake_up(&fi->wait_q);
}

static inline int
fsm_getstate(fsm_instance *fi)
{
	return atomic_read(&fi->state);
}

extern const char *fsm_getstate_str(fsm_instance *fi);

extern void fsm_settimer(fsm_instance *fi, fsm_timer *);

extern void fsm_deltimer(fsm_timer *timer);

extern int fsm_addtimer(fsm_timer *timer, int millisec, int event, void *arg);

extern void fsm_modtimer(fsm_timer *timer, int millisec, int event, void *arg);

#endif 
