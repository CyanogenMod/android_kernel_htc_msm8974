/*
 * Copyright (C) ST-Ericsson SA 2012
 *
 * Battery temperature driver for AB8500
 *
 * License Terms: GNU General Public License v2
 * Author:
 *	Johan Palsson <johan.palsson@stericsson.com>
 *	Karl Komierowski <karl.komierowski@stericsson.com>
 *	Arun R Murthy <arun.murthy@stericsson.com>
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/completion.h>
#include <linux/workqueue.h>
#include <linux/mfd/abx500/ab8500.h>
#include <linux/mfd/abx500.h>
#include <linux/mfd/abx500/ab8500-bm.h>
#include <linux/mfd/abx500/ab8500-gpadc.h>
#include <linux/jiffies.h>

#define VTVOUT_V			1800

#define BTEMP_THERMAL_LOW_LIMIT		-10
#define BTEMP_THERMAL_MED_LIMIT		0
#define BTEMP_THERMAL_HIGH_LIMIT_52	52
#define BTEMP_THERMAL_HIGH_LIMIT_57	57
#define BTEMP_THERMAL_HIGH_LIMIT_62	62

#define BTEMP_BATCTRL_CURR_SRC_7UA	7
#define BTEMP_BATCTRL_CURR_SRC_20UA	20

#define to_ab8500_btemp_device_info(x) container_of((x), \
	struct ab8500_btemp, btemp_psy);

struct ab8500_btemp_interrupts {
	char *name;
	irqreturn_t (*isr)(int irq, void *data);
};

struct ab8500_btemp_events {
	bool batt_rem;
	bool btemp_high;
	bool btemp_medhigh;
	bool btemp_lowmed;
	bool btemp_low;
	bool ac_conn;
	bool usb_conn;
};

struct ab8500_btemp_ranges {
	int btemp_high_limit;
	int btemp_med_limit;
	int btemp_low_limit;
};

struct ab8500_btemp {
	struct device *dev;
	struct list_head node;
	int curr_source;
	int bat_temp;
	int prev_bat_temp;
	struct ab8500 *parent;
	struct ab8500_gpadc *gpadc;
	struct ab8500_fg *fg;
	struct abx500_btemp_platform_data *pdata;
	struct abx500_bm_data *bat;
	struct power_supply btemp_psy;
	struct ab8500_btemp_events events;
	struct ab8500_btemp_ranges btemp_ranges;
	struct workqueue_struct *btemp_wq;
	struct delayed_work btemp_periodic_work;
};

static enum power_supply_property ab8500_btemp_props[] = {
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_TEMP,
};

static LIST_HEAD(ab8500_btemp_list);

struct ab8500_btemp *ab8500_btemp_get(void)
{
	struct ab8500_btemp *btemp;
	btemp = list_first_entry(&ab8500_btemp_list, struct ab8500_btemp, node);

	return btemp;
}

static int ab8500_btemp_batctrl_volt_to_res(struct ab8500_btemp *di,
	int v_batctrl, int inst_curr)
{
	int rbs;

	if (is_ab8500_1p1_or_earlier(di->parent)) {
		return (450000 * (v_batctrl)) / (1800 - v_batctrl);
	}

	if (di->bat->adc_therm == ABx500_ADC_THERM_BATCTRL) {
		rbs = (v_batctrl * 1000
		       - di->bat->gnd_lift_resistance * inst_curr)
		      / di->curr_source;
	} else {
		rbs = (80000 * (v_batctrl)) / (1800 - v_batctrl);
	}

	return rbs;
}

static int ab8500_btemp_read_batctrl_voltage(struct ab8500_btemp *di)
{
	int vbtemp;
	static int prev;

	vbtemp = ab8500_gpadc_convert(di->gpadc, BAT_CTRL);
	if (vbtemp < 0) {
		dev_err(di->dev,
			"%s gpadc conversion failed, using previous value",
			__func__);
		return prev;
	}
	prev = vbtemp;
	return vbtemp;
}

static int ab8500_btemp_curr_source_enable(struct ab8500_btemp *di,
	bool enable)
{
	int curr;
	int ret = 0;

	if (is_ab8500_1p1_or_earlier(di->parent))
		return 0;

	
	if (di->bat->adc_therm == ABx500_ADC_THERM_BATCTRL && enable) {
		if (di->curr_source == BTEMP_BATCTRL_CURR_SRC_7UA)
			curr = BAT_CTRL_7U_ENA;
		else
			curr = BAT_CTRL_20U_ENA;

		dev_dbg(di->dev, "Set BATCTRL %duA\n", di->curr_source);

		ret = abx500_mask_and_set_register_interruptible(di->dev,
			AB8500_CHARGER, AB8500_BAT_CTRL_CURRENT_SOURCE,
			FORCE_BAT_CTRL_CMP_HIGH, FORCE_BAT_CTRL_CMP_HIGH);
		if (ret) {
			dev_err(di->dev, "%s failed setting cmp_force\n",
				__func__);
			return ret;
		}

		/*
		 * We have to wait one 32kHz cycle before enabling
		 * the current source, since ForceBatCtrlCmpHigh needs
		 * to be written in a separate cycle
		 */
		udelay(32);

		ret = abx500_set_register_interruptible(di->dev,
			AB8500_CHARGER, AB8500_BAT_CTRL_CURRENT_SOURCE,
			FORCE_BAT_CTRL_CMP_HIGH | curr);
		if (ret) {
			dev_err(di->dev, "%s failed enabling current source\n",
				__func__);
			goto disable_curr_source;
		}
	} else if (di->bat->adc_therm == ABx500_ADC_THERM_BATCTRL && !enable) {
		dev_dbg(di->dev, "Disable BATCTRL curr source\n");

		
		ret = abx500_mask_and_set_register_interruptible(di->dev,
			AB8500_CHARGER, AB8500_BAT_CTRL_CURRENT_SOURCE,
			BAT_CTRL_7U_ENA | BAT_CTRL_20U_ENA,
			~(BAT_CTRL_7U_ENA | BAT_CTRL_20U_ENA));
		if (ret) {
			dev_err(di->dev, "%s failed disabling current source\n",
				__func__);
			goto disable_curr_source;
		}

		
		ret = abx500_mask_and_set_register_interruptible(di->dev,
			AB8500_CHARGER,	AB8500_BAT_CTRL_CURRENT_SOURCE,
			BAT_CTRL_PULL_UP_ENA | BAT_CTRL_CMP_ENA,
			BAT_CTRL_PULL_UP_ENA | BAT_CTRL_CMP_ENA);
		if (ret) {
			dev_err(di->dev, "%s failed enabling PU and comp\n",
				__func__);
			goto enable_pu_comp;
		}

		/*
		 * We have to wait one 32kHz cycle before disabling
		 * ForceBatCtrlCmpHigh since this needs to be written
		 * in a separate cycle
		 */
		udelay(32);

		
		ret = abx500_mask_and_set_register_interruptible(di->dev,
			AB8500_CHARGER, AB8500_BAT_CTRL_CURRENT_SOURCE,
			FORCE_BAT_CTRL_CMP_HIGH, ~FORCE_BAT_CTRL_CMP_HIGH);
		if (ret) {
			dev_err(di->dev, "%s failed disabling force comp\n",
				__func__);
			goto disable_force_comp;
		}
	}
	return ret;

disable_curr_source:
	
	ret = abx500_mask_and_set_register_interruptible(di->dev,
			AB8500_CHARGER, AB8500_BAT_CTRL_CURRENT_SOURCE,
			BAT_CTRL_7U_ENA | BAT_CTRL_20U_ENA,
			~(BAT_CTRL_7U_ENA | BAT_CTRL_20U_ENA));
	if (ret) {
		dev_err(di->dev, "%s failed disabling current source\n",
			__func__);
		return ret;
	}
enable_pu_comp:
	
	ret = abx500_mask_and_set_register_interruptible(di->dev,
		AB8500_CHARGER,	AB8500_BAT_CTRL_CURRENT_SOURCE,
		BAT_CTRL_PULL_UP_ENA | BAT_CTRL_CMP_ENA,
		BAT_CTRL_PULL_UP_ENA | BAT_CTRL_CMP_ENA);
	if (ret) {
		dev_err(di->dev, "%s failed enabling PU and comp\n",
			__func__);
		return ret;
	}

disable_force_comp:
	/*
	 * We have to wait one 32kHz cycle before disabling
	 * ForceBatCtrlCmpHigh since this needs to be written
	 * in a separate cycle
	 */
	udelay(32);

	
	ret = abx500_mask_and_set_register_interruptible(di->dev,
		AB8500_CHARGER, AB8500_BAT_CTRL_CURRENT_SOURCE,
		FORCE_BAT_CTRL_CMP_HIGH, ~FORCE_BAT_CTRL_CMP_HIGH);
	if (ret) {
		dev_err(di->dev, "%s failed disabling force comp\n",
			__func__);
		return ret;
	}

	return ret;
}

static int ab8500_btemp_get_batctrl_res(struct ab8500_btemp *di)
{
	int ret;
	int batctrl = 0;
	int res;
	int inst_curr;
	int i;

	ret = ab8500_btemp_curr_source_enable(di, true);
	if (ret) {
		dev_err(di->dev, "%s curr source enabled failed\n", __func__);
		return ret;
	}

	if (!di->fg)
		di->fg = ab8500_fg_get();
	if (!di->fg) {
		dev_err(di->dev, "No fg found\n");
		return -EINVAL;
	}

	ret = ab8500_fg_inst_curr_start(di->fg);

	if (ret) {
		dev_err(di->dev, "Failed to start current measurement\n");
		return ret;
	}

	i = 0;

	do {
		batctrl += ab8500_btemp_read_batctrl_voltage(di);
		i++;
		msleep(20);
	} while (!ab8500_fg_inst_curr_done(di->fg));
	batctrl /= i;

	ret = ab8500_fg_inst_curr_finalize(di->fg, &inst_curr);
	if (ret) {
		dev_err(di->dev, "Failed to finalize current measurement\n");
		return ret;
	}

	res = ab8500_btemp_batctrl_volt_to_res(di, batctrl, inst_curr);

	ret = ab8500_btemp_curr_source_enable(di, false);
	if (ret) {
		dev_err(di->dev, "%s curr source disable failed\n", __func__);
		return ret;
	}

	dev_dbg(di->dev, "%s batctrl: %d res: %d inst_curr: %d samples: %d\n",
		__func__, batctrl, res, inst_curr, i);

	return res;
}

static int ab8500_btemp_res_to_temp(struct ab8500_btemp *di,
	const struct abx500_res_to_temp *tbl, int tbl_size, int res)
{
	int i, temp;
	if (res > tbl[0].resist)
		i = 0;
	else if (res <= tbl[tbl_size - 1].resist)
		i = tbl_size - 2;
	else {
		i = 0;
		while (!(res <= tbl[i].resist &&
			res > tbl[i + 1].resist))
			i++;
	}

	temp = tbl[i].temp + ((tbl[i + 1].temp - tbl[i].temp) *
		(res - tbl[i].resist)) / (tbl[i + 1].resist - tbl[i].resist);
	return temp;
}

static int ab8500_btemp_measure_temp(struct ab8500_btemp *di)
{
	int temp;
	static int prev;
	int rbat, rntc, vntc;
	u8 id;

	id = di->bat->batt_id;

	if (di->bat->adc_therm == ABx500_ADC_THERM_BATCTRL &&
			id != BATTERY_UNKNOWN) {

		rbat = ab8500_btemp_get_batctrl_res(di);
		if (rbat < 0) {
			dev_err(di->dev, "%s get batctrl res failed\n",
				__func__);
			return BTEMP_THERMAL_LOW_LIMIT;
		}

		temp = ab8500_btemp_res_to_temp(di,
			di->bat->bat_type[id].r_to_t_tbl,
			di->bat->bat_type[id].n_temp_tbl_elements, rbat);
	} else {
		vntc = ab8500_gpadc_convert(di->gpadc, BTEMP_BALL);
		if (vntc < 0) {
			dev_err(di->dev,
				"%s gpadc conversion failed,"
				" using previous value\n", __func__);
			return prev;
		}
		rntc = 230000 * vntc / (VTVOUT_V - vntc);

		temp = ab8500_btemp_res_to_temp(di,
			di->bat->bat_type[id].r_to_t_tbl,
			di->bat->bat_type[id].n_temp_tbl_elements, rntc);
		prev = temp;
	}
	dev_dbg(di->dev, "Battery temperature is %d\n", temp);
	return temp;
}

static int ab8500_btemp_id(struct ab8500_btemp *di)
{
	int res;
	u8 i;

	di->curr_source = BTEMP_BATCTRL_CURR_SRC_7UA;
	di->bat->batt_id = BATTERY_UNKNOWN;

	res =  ab8500_btemp_get_batctrl_res(di);
	if (res < 0) {
		dev_err(di->dev, "%s get batctrl res failed\n", __func__);
		return -ENXIO;
	}

	
	for (i = BATTERY_UNKNOWN + 1; i < di->bat->n_btypes; i++) {
		if ((res <= di->bat->bat_type[i].resis_high) &&
			(res >= di->bat->bat_type[i].resis_low)) {
			dev_dbg(di->dev, "Battery detected on %s"
				" low %d < res %d < high: %d"
				" index: %d\n",
				di->bat->adc_therm == ABx500_ADC_THERM_BATCTRL ?
				"BATCTRL" : "BATTEMP",
				di->bat->bat_type[i].resis_low, res,
				di->bat->bat_type[i].resis_high, i);

			di->bat->batt_id = i;
			break;
		}
	}

	if (di->bat->batt_id == BATTERY_UNKNOWN) {
		dev_warn(di->dev, "Battery identified as unknown"
			", resistance %d Ohm\n", res);
		return -ENXIO;
	}

	if (di->bat->adc_therm == ABx500_ADC_THERM_BATCTRL &&
			di->bat->batt_id == 1) {
		dev_dbg(di->dev, "Set BATCTRL current source to 20uA\n");
		di->curr_source = BTEMP_BATCTRL_CURR_SRC_20UA;
	}

	return di->bat->batt_id;
}

static void ab8500_btemp_periodic_work(struct work_struct *work)
{
	int interval;
	struct ab8500_btemp *di = container_of(work,
		struct ab8500_btemp, btemp_periodic_work.work);

	di->bat_temp = ab8500_btemp_measure_temp(di);

	if (di->bat_temp != di->prev_bat_temp) {
		di->prev_bat_temp = di->bat_temp;
		power_supply_changed(&di->btemp_psy);
	}

	if (di->events.ac_conn || di->events.usb_conn)
		interval = di->bat->temp_interval_chg;
	else
		interval = di->bat->temp_interval_nochg;

	
	queue_delayed_work(di->btemp_wq,
		&di->btemp_periodic_work,
		round_jiffies(interval * HZ));
}

static irqreturn_t ab8500_btemp_batctrlindb_handler(int irq, void *_di)
{
	struct ab8500_btemp *di = _di;
	dev_err(di->dev, "Battery removal detected!\n");

	di->events.batt_rem = true;
	power_supply_changed(&di->btemp_psy);

	return IRQ_HANDLED;
}

static irqreturn_t ab8500_btemp_templow_handler(int irq, void *_di)
{
	struct ab8500_btemp *di = _di;

	if (is_ab8500_2p0_or_earlier(di->parent)) {
		dev_dbg(di->dev, "Ignore false btemp low irq"
			" for ABB cut 1.0, 1.1 and 2.0\n");
	} else {
		dev_crit(di->dev, "Battery temperature lower than -10deg c\n");

		di->events.btemp_low = true;
		di->events.btemp_high = false;
		di->events.btemp_medhigh = false;
		di->events.btemp_lowmed = false;
		power_supply_changed(&di->btemp_psy);
	}

	return IRQ_HANDLED;
}

static irqreturn_t ab8500_btemp_temphigh_handler(int irq, void *_di)
{
	struct ab8500_btemp *di = _di;

	dev_crit(di->dev, "Battery temperature is higher than MAX temp\n");

	di->events.btemp_high = true;
	di->events.btemp_medhigh = false;
	di->events.btemp_lowmed = false;
	di->events.btemp_low = false;
	power_supply_changed(&di->btemp_psy);

	return IRQ_HANDLED;
}

static irqreturn_t ab8500_btemp_lowmed_handler(int irq, void *_di)
{
	struct ab8500_btemp *di = _di;

	dev_dbg(di->dev, "Battery temperature is between low and medium\n");

	di->events.btemp_lowmed = true;
	di->events.btemp_medhigh = false;
	di->events.btemp_high = false;
	di->events.btemp_low = false;
	power_supply_changed(&di->btemp_psy);

	return IRQ_HANDLED;
}

static irqreturn_t ab8500_btemp_medhigh_handler(int irq, void *_di)
{
	struct ab8500_btemp *di = _di;

	dev_dbg(di->dev, "Battery temperature is between medium and high\n");

	di->events.btemp_medhigh = true;
	di->events.btemp_lowmed = false;
	di->events.btemp_high = false;
	di->events.btemp_low = false;
	power_supply_changed(&di->btemp_psy);

	return IRQ_HANDLED;
}

static void ab8500_btemp_periodic(struct ab8500_btemp *di,
	bool enable)
{
	dev_dbg(di->dev, "Enable periodic temperature measurements: %d\n",
		enable);
	cancel_delayed_work_sync(&di->btemp_periodic_work);

	if (enable)
		queue_delayed_work(di->btemp_wq, &di->btemp_periodic_work, 0);
}

static int ab8500_btemp_get_temp(struct ab8500_btemp *di)
{
	int temp = 0;

	if (is_ab8500_2p0_or_earlier(di->parent)) {
		temp = di->bat_temp * 10;
	} else {
		if (di->events.btemp_low) {
			if (temp > di->btemp_ranges.btemp_low_limit)
				temp = di->btemp_ranges.btemp_low_limit;
			else
				temp = di->bat_temp * 10;
		} else if (di->events.btemp_high) {
			if (temp < di->btemp_ranges.btemp_high_limit)
				temp = di->btemp_ranges.btemp_high_limit;
			else
				temp = di->bat_temp * 10;
		} else if (di->events.btemp_lowmed) {
			if (temp > di->btemp_ranges.btemp_med_limit)
				temp = di->btemp_ranges.btemp_med_limit;
			else
				temp = di->bat_temp * 10;
		} else if (di->events.btemp_medhigh) {
			if (temp < di->btemp_ranges.btemp_med_limit)
				temp = di->btemp_ranges.btemp_med_limit;
			else
				temp = di->bat_temp * 10;
		} else
			temp = di->bat_temp * 10;
	}
	return temp;
}

int ab8500_btemp_get_batctrl_temp(struct ab8500_btemp *btemp)
{
	return btemp->bat_temp * 1000;
}

static int ab8500_btemp_get_property(struct power_supply *psy,
	enum power_supply_property psp,
	union power_supply_propval *val)
{
	struct ab8500_btemp *di;

	di = to_ab8500_btemp_device_info(psy);

	switch (psp) {
	case POWER_SUPPLY_PROP_PRESENT:
	case POWER_SUPPLY_PROP_ONLINE:
		if (di->events.batt_rem)
			val->intval = 0;
		else
			val->intval = 1;
		break;
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = di->bat->bat_type[di->bat->batt_id].name;
		break;
	case POWER_SUPPLY_PROP_TEMP:
		val->intval = ab8500_btemp_get_temp(di);
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int ab8500_btemp_get_ext_psy_data(struct device *dev, void *data)
{
	struct power_supply *psy;
	struct power_supply *ext;
	struct ab8500_btemp *di;
	union power_supply_propval ret;
	int i, j;
	bool psy_found = false;

	psy = (struct power_supply *)data;
	ext = dev_get_drvdata(dev);
	di = to_ab8500_btemp_device_info(psy);

	for (i = 0; i < ext->num_supplicants; i++) {
		if (!strcmp(ext->supplied_to[i], psy->name))
			psy_found = true;
	}

	if (!psy_found)
		return 0;

	
	for (j = 0; j < ext->num_properties; j++) {
		enum power_supply_property prop;
		prop = ext->properties[j];

		if (ext->get_property(ext, prop, &ret))
			continue;

		switch (prop) {
		case POWER_SUPPLY_PROP_PRESENT:
			switch (ext->type) {
			case POWER_SUPPLY_TYPE_MAINS:
				
				if (!ret.intval && di->events.ac_conn) {
					di->events.ac_conn = false;
				}
				
				else if (ret.intval && !di->events.ac_conn) {
					di->events.ac_conn = true;
					if (!di->events.usb_conn)
						ab8500_btemp_periodic(di, true);
				}
				break;
			case POWER_SUPPLY_TYPE_USB:
				
				if (!ret.intval && di->events.usb_conn) {
					di->events.usb_conn = false;
				}
				
				else if (ret.intval && !di->events.usb_conn) {
					di->events.usb_conn = true;
					if (!di->events.ac_conn)
						ab8500_btemp_periodic(di, true);
				}
				break;
			default:
				break;
			}
			break;
		default:
			break;
		}
	}
	return 0;
}

static void ab8500_btemp_external_power_changed(struct power_supply *psy)
{
	struct ab8500_btemp *di = to_ab8500_btemp_device_info(psy);

	class_for_each_device(power_supply_class, NULL,
		&di->btemp_psy, ab8500_btemp_get_ext_psy_data);
}

static struct ab8500_btemp_interrupts ab8500_btemp_irq[] = {
	{"BAT_CTRL_INDB", ab8500_btemp_batctrlindb_handler},
	{"BTEMP_LOW", ab8500_btemp_templow_handler},
	{"BTEMP_HIGH", ab8500_btemp_temphigh_handler},
	{"BTEMP_LOW_MEDIUM", ab8500_btemp_lowmed_handler},
	{"BTEMP_MEDIUM_HIGH", ab8500_btemp_medhigh_handler},
};

#if defined(CONFIG_PM)
static int ab8500_btemp_resume(struct platform_device *pdev)
{
	struct ab8500_btemp *di = platform_get_drvdata(pdev);

	ab8500_btemp_periodic(di, true);

	return 0;
}

static int ab8500_btemp_suspend(struct platform_device *pdev,
	pm_message_t state)
{
	struct ab8500_btemp *di = platform_get_drvdata(pdev);

	ab8500_btemp_periodic(di, false);

	return 0;
}
#else
#define ab8500_btemp_suspend      NULL
#define ab8500_btemp_resume       NULL
#endif

static int __devexit ab8500_btemp_remove(struct platform_device *pdev)
{
	struct ab8500_btemp *di = platform_get_drvdata(pdev);
	int i, irq;

	
	for (i = 0; i < ARRAY_SIZE(ab8500_btemp_irq); i++) {
		irq = platform_get_irq_byname(pdev, ab8500_btemp_irq[i].name);
		free_irq(irq, di);
	}

	
	destroy_workqueue(di->btemp_wq);

	flush_scheduled_work();
	power_supply_unregister(&di->btemp_psy);
	platform_set_drvdata(pdev, NULL);
	kfree(di);

	return 0;
}

static int __devinit ab8500_btemp_probe(struct platform_device *pdev)
{
	int irq, i, ret = 0;
	u8 val;
	struct abx500_bm_plat_data *plat_data;

	struct ab8500_btemp *di =
		kzalloc(sizeof(struct ab8500_btemp), GFP_KERNEL);
	if (!di)
		return -ENOMEM;

	
	di->dev = &pdev->dev;
	di->parent = dev_get_drvdata(pdev->dev.parent);
	di->gpadc = ab8500_gpadc_get("ab8500-gpadc.0");

	
	plat_data = pdev->dev.platform_data;
	di->pdata = plat_data->btemp;
	if (!di->pdata) {
		dev_err(di->dev, "no btemp platform data supplied\n");
		ret = -EINVAL;
		goto free_device_info;
	}

	
	di->bat = plat_data->battery;
	if (!di->bat) {
		dev_err(di->dev, "no battery platform data supplied\n");
		ret = -EINVAL;
		goto free_device_info;
	}

	
	di->btemp_psy.name = "ab8500_btemp";
	di->btemp_psy.type = POWER_SUPPLY_TYPE_BATTERY;
	di->btemp_psy.properties = ab8500_btemp_props;
	di->btemp_psy.num_properties = ARRAY_SIZE(ab8500_btemp_props);
	di->btemp_psy.get_property = ab8500_btemp_get_property;
	di->btemp_psy.supplied_to = di->pdata->supplied_to;
	di->btemp_psy.num_supplicants = di->pdata->num_supplicants;
	di->btemp_psy.external_power_changed =
		ab8500_btemp_external_power_changed;


	
	di->btemp_wq =
		create_singlethread_workqueue("ab8500_btemp_wq");
	if (di->btemp_wq == NULL) {
		dev_err(di->dev, "failed to create work queue\n");
		goto free_device_info;
	}

	
	INIT_DELAYED_WORK_DEFERRABLE(&di->btemp_periodic_work,
		ab8500_btemp_periodic_work);

	
	if (ab8500_btemp_id(di) < 0)
		dev_warn(di->dev, "failed to identify the battery\n");

	
	di->btemp_ranges.btemp_low_limit = BTEMP_THERMAL_LOW_LIMIT;
	di->btemp_ranges.btemp_med_limit = BTEMP_THERMAL_MED_LIMIT;

	ret = abx500_get_register_interruptible(di->dev, AB8500_CHARGER,
		AB8500_BTEMP_HIGH_TH, &val);
	if (ret < 0) {
		dev_err(di->dev, "%s ab8500 read failed\n", __func__);
		goto free_btemp_wq;
	}
	switch (val) {
	case BTEMP_HIGH_TH_57_0:
	case BTEMP_HIGH_TH_57_1:
		di->btemp_ranges.btemp_high_limit =
			BTEMP_THERMAL_HIGH_LIMIT_57;
		break;
	case BTEMP_HIGH_TH_52:
		di->btemp_ranges.btemp_high_limit =
			BTEMP_THERMAL_HIGH_LIMIT_52;
		break;
	case BTEMP_HIGH_TH_62:
		di->btemp_ranges.btemp_high_limit =
			BTEMP_THERMAL_HIGH_LIMIT_62;
		break;
	}

	
	ret = power_supply_register(di->dev, &di->btemp_psy);
	if (ret) {
		dev_err(di->dev, "failed to register BTEMP psy\n");
		goto free_btemp_wq;
	}

	
	for (i = 0; i < ARRAY_SIZE(ab8500_btemp_irq); i++) {
		irq = platform_get_irq_byname(pdev, ab8500_btemp_irq[i].name);
		ret = request_threaded_irq(irq, NULL, ab8500_btemp_irq[i].isr,
			IRQF_SHARED | IRQF_NO_SUSPEND,
			ab8500_btemp_irq[i].name, di);

		if (ret) {
			dev_err(di->dev, "failed to request %s IRQ %d: %d\n"
				, ab8500_btemp_irq[i].name, irq, ret);
			goto free_irq;
		}
		dev_dbg(di->dev, "Requested %s IRQ %d: %d\n",
			ab8500_btemp_irq[i].name, irq, ret);
	}

	platform_set_drvdata(pdev, di);

	
	ab8500_btemp_periodic(di, true);
	list_add_tail(&di->node, &ab8500_btemp_list);

	return ret;

free_irq:
	power_supply_unregister(&di->btemp_psy);

	
	for (i = i - 1; i >= 0; i--) {
		irq = platform_get_irq_byname(pdev, ab8500_btemp_irq[i].name);
		free_irq(irq, di);
	}
free_btemp_wq:
	destroy_workqueue(di->btemp_wq);
free_device_info:
	kfree(di);

	return ret;
}

static struct platform_driver ab8500_btemp_driver = {
	.probe = ab8500_btemp_probe,
	.remove = __devexit_p(ab8500_btemp_remove),
	.suspend = ab8500_btemp_suspend,
	.resume = ab8500_btemp_resume,
	.driver = {
		.name = "ab8500-btemp",
		.owner = THIS_MODULE,
	},
};

static int __init ab8500_btemp_init(void)
{
	return platform_driver_register(&ab8500_btemp_driver);
}

static void __exit ab8500_btemp_exit(void)
{
	platform_driver_unregister(&ab8500_btemp_driver);
}

subsys_initcall_sync(ab8500_btemp_init);
module_exit(ab8500_btemp_exit);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Johan Palsson, Karl Komierowski, Arun R Murthy");
MODULE_ALIAS("platform:ab8500-btemp");
MODULE_DESCRIPTION("AB8500 battery temperature driver");
