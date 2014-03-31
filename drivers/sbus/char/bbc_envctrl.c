/* bbc_envctrl.c: UltraSPARC-III environment control driver.
 *
 * Copyright (C) 2001, 2008 David S. Miller (davem@davemloft.net)
 */

#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/kmod.h>
#include <linux/reboot.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/of_device.h>
#include <asm/oplib.h>

#include "bbc_i2c.h"
#include "max1617.h"

#undef ENVCTRL_TRACE



struct temp_limits {
	s8 high_pwroff, high_shutdown, high_warn;
	s8 low_warn, low_shutdown, low_pwroff;
};

static struct temp_limits cpu_temp_limits[2] = {
	{ 100, 85, 80, 5, -5, -10 },
	{ 100, 85, 80, 5, -5, -10 },
};

static struct temp_limits amb_temp_limits[2] = {
	{ 65, 55, 40, 5, -5, -10 },
	{ 65, 55, 40, 5, -5, -10 },
};

static LIST_HEAD(all_temps);
static LIST_HEAD(all_fans);

#define CPU_FAN_REG	0xf0
#define SYS_FAN_REG	0xf2
#define PSUPPLY_FAN_REG	0xf4

#define FAN_SPEED_MIN	0x0c
#define FAN_SPEED_MAX	0x3f

#define PSUPPLY_FAN_ON	0x1f
#define PSUPPLY_FAN_OFF	0x00

static void set_fan_speeds(struct bbc_fan_control *fp)
{
	if (fp->cpu_fan_speed < FAN_SPEED_MIN)
		fp->cpu_fan_speed = FAN_SPEED_MIN;
	if (fp->cpu_fan_speed > FAN_SPEED_MAX)
		fp->cpu_fan_speed = FAN_SPEED_MAX;
	if (fp->system_fan_speed < FAN_SPEED_MIN)
		fp->system_fan_speed = FAN_SPEED_MIN;
	if (fp->system_fan_speed > FAN_SPEED_MAX)
		fp->system_fan_speed = FAN_SPEED_MAX;
#ifdef ENVCTRL_TRACE
	printk("fan%d: Changed fan speed to cpu(%02x) sys(%02x)\n",
	       fp->index,
	       fp->cpu_fan_speed, fp->system_fan_speed);
#endif

	bbc_i2c_writeb(fp->client, fp->cpu_fan_speed, CPU_FAN_REG);
	bbc_i2c_writeb(fp->client, fp->system_fan_speed, SYS_FAN_REG);
	bbc_i2c_writeb(fp->client,
		       (fp->psupply_fan_on ?
			PSUPPLY_FAN_ON : PSUPPLY_FAN_OFF),
		       PSUPPLY_FAN_REG);
}

static void get_current_temps(struct bbc_cpu_temperature *tp)
{
	tp->prev_amb_temp = tp->curr_amb_temp;
	bbc_i2c_readb(tp->client,
		      (unsigned char *) &tp->curr_amb_temp,
		      MAX1617_AMB_TEMP);
	tp->prev_cpu_temp = tp->curr_cpu_temp;
	bbc_i2c_readb(tp->client,
		      (unsigned char *) &tp->curr_cpu_temp,
		      MAX1617_CPU_TEMP);
#ifdef ENVCTRL_TRACE
	printk("temp%d: cpu(%d C) amb(%d C)\n",
	       tp->index,
	       (int) tp->curr_cpu_temp, (int) tp->curr_amb_temp);
#endif
}


static void do_envctrl_shutdown(struct bbc_cpu_temperature *tp)
{
	static int shutting_down = 0;
	char *type = "???";
	s8 val = -1;

	if (shutting_down != 0)
		return;

	if (tp->curr_amb_temp >= amb_temp_limits[tp->index].high_shutdown ||
	    tp->curr_amb_temp < amb_temp_limits[tp->index].low_shutdown) {
		type = "ambient";
		val = tp->curr_amb_temp;
	} else if (tp->curr_cpu_temp >= cpu_temp_limits[tp->index].high_shutdown ||
		   tp->curr_cpu_temp < cpu_temp_limits[tp->index].low_shutdown) {
		type = "CPU";
		val = tp->curr_cpu_temp;
	}

	printk(KERN_CRIT "temp%d: Outside of safe %s "
	       "operating temperature, %d C.\n",
	       tp->index, type, val);

	printk(KERN_CRIT "kenvctrld: Shutting down the system now.\n");

	shutting_down = 1;
	if (orderly_poweroff(true) < 0)
		printk(KERN_CRIT "envctrl: shutdown execution failed\n");
}

#define WARN_INTERVAL	(30 * HZ)

static void analyze_ambient_temp(struct bbc_cpu_temperature *tp, unsigned long *last_warn, int tick)
{
	int ret = 0;

	if (time_after(jiffies, (*last_warn + WARN_INTERVAL))) {
		if (tp->curr_amb_temp >=
		    amb_temp_limits[tp->index].high_warn) {
			printk(KERN_WARNING "temp%d: "
			       "Above safe ambient operating temperature, %d C.\n",
			       tp->index, (int) tp->curr_amb_temp);
			ret = 1;
		} else if (tp->curr_amb_temp <
			   amb_temp_limits[tp->index].low_warn) {
			printk(KERN_WARNING "temp%d: "
			       "Below safe ambient operating temperature, %d C.\n",
			       tp->index, (int) tp->curr_amb_temp);
			ret = 1;
		}
		if (ret)
			*last_warn = jiffies;
	} else if (tp->curr_amb_temp >= amb_temp_limits[tp->index].high_warn ||
		   tp->curr_amb_temp < amb_temp_limits[tp->index].low_warn)
		ret = 1;

	
	if (tp->curr_amb_temp >= amb_temp_limits[tp->index].high_shutdown ||
	    tp->curr_amb_temp < amb_temp_limits[tp->index].low_shutdown) {
		do_envctrl_shutdown(tp);
		ret = 1;
	}

	if (ret) {
		tp->fan_todo[FAN_AMBIENT] = FAN_FULLBLAST;
	} else if ((tick & (8 - 1)) == 0) {
		s8 amb_goal_hi = amb_temp_limits[tp->index].high_warn - 10;
		s8 amb_goal_lo;

		amb_goal_lo = amb_goal_hi - 3;

		if (tp->avg_amb_temp < amb_goal_hi) {
			if (tp->avg_amb_temp >= amb_goal_lo)
				tp->fan_todo[FAN_AMBIENT] = FAN_SAME;
			else
				tp->fan_todo[FAN_AMBIENT] = FAN_SLOWER;
		} else {
			tp->fan_todo[FAN_AMBIENT] = FAN_FASTER;
		}
	} else {
		tp->fan_todo[FAN_AMBIENT] = FAN_SAME;
	}
}

static void analyze_cpu_temp(struct bbc_cpu_temperature *tp, unsigned long *last_warn, int tick)
{
	int ret = 0;

	if (time_after(jiffies, (*last_warn + WARN_INTERVAL))) {
		if (tp->curr_cpu_temp >=
		    cpu_temp_limits[tp->index].high_warn) {
			printk(KERN_WARNING "temp%d: "
			       "Above safe CPU operating temperature, %d C.\n",
			       tp->index, (int) tp->curr_cpu_temp);
			ret = 1;
		} else if (tp->curr_cpu_temp <
			   cpu_temp_limits[tp->index].low_warn) {
			printk(KERN_WARNING "temp%d: "
			       "Below safe CPU operating temperature, %d C.\n",
			       tp->index, (int) tp->curr_cpu_temp);
			ret = 1;
		}
		if (ret)
			*last_warn = jiffies;
	} else if (tp->curr_cpu_temp >= cpu_temp_limits[tp->index].high_warn ||
		   tp->curr_cpu_temp < cpu_temp_limits[tp->index].low_warn)
		ret = 1;

	
	if (tp->curr_cpu_temp >= cpu_temp_limits[tp->index].high_shutdown ||
	    tp->curr_cpu_temp < cpu_temp_limits[tp->index].low_shutdown) {
		do_envctrl_shutdown(tp);
		ret = 1;
	}

	if (ret) {
		tp->fan_todo[FAN_CPU] = FAN_FULLBLAST;
	} else if ((tick & (8 - 1)) == 0) {
		s8 cpu_goal_hi = cpu_temp_limits[tp->index].high_warn - 10;
		s8 cpu_goal_lo;

		cpu_goal_lo = cpu_goal_hi - 3;

		if (tp->avg_cpu_temp < cpu_goal_hi) {
			if (tp->avg_cpu_temp >= cpu_goal_lo)
				tp->fan_todo[FAN_CPU] = FAN_SAME;
			else
				tp->fan_todo[FAN_CPU] = FAN_SLOWER;
		} else {
			tp->fan_todo[FAN_CPU] = FAN_FASTER;
		}
	} else {
		tp->fan_todo[FAN_CPU] = FAN_SAME;
	}
}

static void analyze_temps(struct bbc_cpu_temperature *tp, unsigned long *last_warn)
{
	tp->avg_amb_temp = (s8)((int)((int)tp->avg_amb_temp + (int)tp->curr_amb_temp) / 2);
	tp->avg_cpu_temp = (s8)((int)((int)tp->avg_cpu_temp + (int)tp->curr_cpu_temp) / 2);

	analyze_ambient_temp(tp, last_warn, tp->sample_tick);
	analyze_cpu_temp(tp, last_warn, tp->sample_tick);

	tp->sample_tick++;
}

static enum fan_action prioritize_fan_action(int which_fan)
{
	struct bbc_cpu_temperature *tp;
	enum fan_action decision = FAN_STATE_MAX;

	list_for_each_entry(tp, &all_temps, glob_list) {
		if (tp->fan_todo[which_fan] == FAN_FULLBLAST) {
			decision = FAN_FULLBLAST;
			break;
		}
		if (tp->fan_todo[which_fan] == FAN_SAME &&
		    decision != FAN_FASTER)
			decision = FAN_SAME;
		else if (tp->fan_todo[which_fan] == FAN_FASTER)
			decision = FAN_FASTER;
		else if (decision != FAN_FASTER &&
			 decision != FAN_SAME &&
			 tp->fan_todo[which_fan] == FAN_SLOWER)
			decision = FAN_SLOWER;
	}
	if (decision == FAN_STATE_MAX)
		decision = FAN_SAME;

	return decision;
}

static int maybe_new_ambient_fan_speed(struct bbc_fan_control *fp)
{
	enum fan_action decision = prioritize_fan_action(FAN_AMBIENT);
	int ret;

	if (decision == FAN_SAME)
		return 0;

	ret = 1;
	if (decision == FAN_FULLBLAST) {
		if (fp->system_fan_speed >= FAN_SPEED_MAX)
			ret = 0;
		else
			fp->system_fan_speed = FAN_SPEED_MAX;
	} else {
		if (decision == FAN_FASTER) {
			if (fp->system_fan_speed >= FAN_SPEED_MAX)
				ret = 0;
			else
				fp->system_fan_speed += 2;
		} else {
			int orig_speed = fp->system_fan_speed;

			if (orig_speed <= FAN_SPEED_MIN ||
			    orig_speed <= (fp->cpu_fan_speed - 3))
				ret = 0;
			else
				fp->system_fan_speed -= 1;
		}
	}

	return ret;
}

static int maybe_new_cpu_fan_speed(struct bbc_fan_control *fp)
{
	enum fan_action decision = prioritize_fan_action(FAN_CPU);
	int ret;

	if (decision == FAN_SAME)
		return 0;

	ret = 1;
	if (decision == FAN_FULLBLAST) {
		if (fp->cpu_fan_speed >= FAN_SPEED_MAX)
			ret = 0;
		else
			fp->cpu_fan_speed = FAN_SPEED_MAX;
	} else {
		if (decision == FAN_FASTER) {
			if (fp->cpu_fan_speed >= FAN_SPEED_MAX)
				ret = 0;
			else {
				fp->cpu_fan_speed += 2;
				if (fp->system_fan_speed <
				    (fp->cpu_fan_speed - 3))
					fp->system_fan_speed =
						fp->cpu_fan_speed - 3;
			}
		} else {
			if (fp->cpu_fan_speed <= FAN_SPEED_MIN)
				ret = 0;
			else
				fp->cpu_fan_speed -= 1;
		}
	}

	return ret;
}

static void maybe_new_fan_speeds(struct bbc_fan_control *fp)
{
	int new;

	new  = maybe_new_ambient_fan_speed(fp);
	new |= maybe_new_cpu_fan_speed(fp);

	if (new)
		set_fan_speeds(fp);
}

static void fans_full_blast(void)
{
	struct bbc_fan_control *fp;

	list_for_each_entry(fp, &all_fans, glob_list) {
		fp->cpu_fan_speed = FAN_SPEED_MAX;
		fp->system_fan_speed = FAN_SPEED_MAX;
		fp->psupply_fan_on = 1;
		set_fan_speeds(fp);
	}
}

#define POLL_INTERVAL	(5 * 1000)
static unsigned long last_warning_jiffies;
static struct task_struct *kenvctrld_task;

static int kenvctrld(void *__unused)
{
	printk(KERN_INFO "bbc_envctrl: kenvctrld starting...\n");
	last_warning_jiffies = jiffies - WARN_INTERVAL;
	for (;;) {
		struct bbc_cpu_temperature *tp;
		struct bbc_fan_control *fp;

		msleep_interruptible(POLL_INTERVAL);
		if (kthread_should_stop())
			break;

		list_for_each_entry(tp, &all_temps, glob_list) {
			get_current_temps(tp);
			analyze_temps(tp, &last_warning_jiffies);
		}
		list_for_each_entry(fp, &all_fans, glob_list)
			maybe_new_fan_speeds(fp);
	}
	printk(KERN_INFO "bbc_envctrl: kenvctrld exiting...\n");

	fans_full_blast();

	return 0;
}

static void attach_one_temp(struct bbc_i2c_bus *bp, struct platform_device *op,
			    int temp_idx)
{
	struct bbc_cpu_temperature *tp;

	tp = kzalloc(sizeof(*tp), GFP_KERNEL);
	if (!tp)
		return;

	tp->client = bbc_i2c_attach(bp, op);
	if (!tp->client) {
		kfree(tp);
		return;
	}


	tp->index = temp_idx;

	list_add(&tp->glob_list, &all_temps);
	list_add(&tp->bp_list, &bp->temps);

	bbc_i2c_writeb(tp->client, 0x00, MAX1617_WR_CFG_BYTE);
	bbc_i2c_writeb(tp->client, 0x02, MAX1617_WR_CVRATE_BYTE);

	
	bbc_i2c_writeb(tp->client, amb_temp_limits[tp->index].high_pwroff,
		       MAX1617_WR_AMB_HIGHLIM);
	bbc_i2c_writeb(tp->client, amb_temp_limits[tp->index].low_pwroff,
		       MAX1617_WR_AMB_LOWLIM);
	bbc_i2c_writeb(tp->client, cpu_temp_limits[tp->index].high_pwroff,
		       MAX1617_WR_CPU_HIGHLIM);
	bbc_i2c_writeb(tp->client, cpu_temp_limits[tp->index].low_pwroff,
		       MAX1617_WR_CPU_LOWLIM);

	get_current_temps(tp);
	tp->prev_cpu_temp = tp->avg_cpu_temp = tp->curr_cpu_temp;
	tp->prev_amb_temp = tp->avg_amb_temp = tp->curr_amb_temp;

	tp->fan_todo[FAN_AMBIENT] = FAN_SAME;
	tp->fan_todo[FAN_CPU] = FAN_SAME;
}

static void attach_one_fan(struct bbc_i2c_bus *bp, struct platform_device *op,
			   int fan_idx)
{
	struct bbc_fan_control *fp;

	fp = kzalloc(sizeof(*fp), GFP_KERNEL);
	if (!fp)
		return;

	fp->client = bbc_i2c_attach(bp, op);
	if (!fp->client) {
		kfree(fp);
		return;
	}

	fp->index = fan_idx;

	list_add(&fp->glob_list, &all_fans);
	list_add(&fp->bp_list, &bp->fans);

	fp->psupply_fan_on = 1;
	fp->cpu_fan_speed = (FAN_SPEED_MAX - FAN_SPEED_MIN) / 2;
	fp->cpu_fan_speed += FAN_SPEED_MIN;
	fp->system_fan_speed = (FAN_SPEED_MAX - FAN_SPEED_MIN) / 2;
	fp->system_fan_speed += FAN_SPEED_MIN;

	set_fan_speeds(fp);
}

static void destroy_one_temp(struct bbc_cpu_temperature *tp)
{
	bbc_i2c_detach(tp->client);
	kfree(tp);
}

static void destroy_all_temps(struct bbc_i2c_bus *bp)
{
	struct bbc_cpu_temperature *tp, *tpos;

	list_for_each_entry_safe(tp, tpos, &bp->temps, bp_list) {
		list_del(&tp->bp_list);
		list_del(&tp->glob_list);
		destroy_one_temp(tp);
	}
}

static void destroy_one_fan(struct bbc_fan_control *fp)
{
	bbc_i2c_detach(fp->client);
	kfree(fp);
}

static void destroy_all_fans(struct bbc_i2c_bus *bp)
{
	struct bbc_fan_control *fp, *fpos;

	list_for_each_entry_safe(fp, fpos, &bp->fans, bp_list) {
		list_del(&fp->bp_list);
		list_del(&fp->glob_list);
		destroy_one_fan(fp);
	}
}

int bbc_envctrl_init(struct bbc_i2c_bus *bp)
{
	struct platform_device *op;
	int temp_index = 0;
	int fan_index = 0;
	int devidx = 0;

	while ((op = bbc_i2c_getdev(bp, devidx++)) != NULL) {
		if (!strcmp(op->dev.of_node->name, "temperature"))
			attach_one_temp(bp, op, temp_index++);
		if (!strcmp(op->dev.of_node->name, "fan-control"))
			attach_one_fan(bp, op, fan_index++);
	}
	if (temp_index != 0 && fan_index != 0) {
		kenvctrld_task = kthread_run(kenvctrld, NULL, "kenvctrld");
		if (IS_ERR(kenvctrld_task)) {
			int err = PTR_ERR(kenvctrld_task);

			kenvctrld_task = NULL;
			destroy_all_temps(bp);
			destroy_all_fans(bp);
			return err;
		}
	}

	return 0;
}

void bbc_envctrl_cleanup(struct bbc_i2c_bus *bp)
{
	if (kenvctrld_task)
		kthread_stop(kenvctrld_task);

	destroy_all_temps(bp);
	destroy_all_fans(bp);
}
