/* Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/gpio.h>
#include <linux/irq.h>
#include <linux/irqdomain.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/of_fdt.h>
#include <linux/of_irq.h>
#include <linux/memory.h>
#include <linux/persistent_ram.h>
#include <linux/regulator/qpnp-regulator.h>
#include <linux/msm_tsens.h>
#include <asm/mach/map.h>
#include <asm/hardware/gic.h>
#include <asm/mach/arch.h>
#include <asm/mach/time.h>
#include <mach/board.h>
#include <mach/gpiomux.h>
#include <mach/msm_iomap.h>
#include <mach/restart.h>
#ifdef CONFIG_ION_MSM
#include <mach/ion.h>
#endif
#include <mach/msm_memtypes.h>
#include <mach/socinfo.h>
#include <mach/board.h>
#include <mach/clk-provider.h>
#include <mach/msm_smd.h>
#include <mach/rpm-smd.h>
#include <mach/rpm-regulator-smd.h>
#include <mach/msm_smem.h>
#include <linux/msm_thermal.h>
#include "board-dt.h"
#include "clock.h"
#include "platsmp.h"
#include "spm.h"
#include "pm.h"
#include "modem_notifier.h"
#include <linux/usb/android.h>
#include "devices.h"
#ifdef CONFIG_HTC_POWER_DEBUG
#include <mach/htc_util.h>
#include <mach/devices_dtb.h>
#endif
#include <mach/cable_detect.h>
#include <mach/devices_cmdline.h>
#ifdef CONFIG_HTC_BATT_8960
#include "linux/mfd/pm8xxx/pm8921-charger.h"
#include "linux/mfd/pm8xxx/pm8921-bms.h"
#include "linux/mfd/pm8xxx/batt-alarm.h"
#include "mach/htc_battery_8960.h"
#include "mach/htc_battery_cell.h"
#include <linux/qpnp/qpnp-charger.h>
#include <linux/qpnp/qpnp-bms.h>
#endif

#ifdef CONFIG_BT
#include <mach/htc_bdaddress.h>
#endif

#ifdef CONFIG_HTC_BUILD_EDIAG
#include <linux/android_ediagpmem.h>
#endif

#ifdef CONFIG_LCD_KCAL
#include <mach/kcal.h>
#include <linux/module.h>
#include "../../../drivers/video/msm/mdss/mdss_fb.h"
extern int update_preset_lcdc_lut(void);

extern int g_kcal_r;
extern int g_kcal_g;
extern int g_kcal_b;

extern int g_kcal_min;

int kcal_set_values(int kcal_r, int kcal_g, int kcal_b)
{

	if (kcal_r > 255 || kcal_r < 0)
		kcal_r = kcal_r < 0 ? 0 : kcal_r;
		kcal_r = kcal_r > 255 ? 255 : kcal_r;
	if (kcal_g > 255 || kcal_g < 0)
		kcal_g = kcal_g < 0 ? 0 : kcal_g;
		kcal_g = kcal_g > 255 ? 255 : kcal_g;
	if (kcal_b > 255 || kcal_b < 0)
		kcal_b = kcal_b < 0 ? 0 : kcal_b;
		kcal_b = kcal_b > 255 ? 255 : kcal_b;

	g_kcal_r = kcal_r < g_kcal_min ? g_kcal_min : kcal_r;
	g_kcal_g = kcal_g < g_kcal_min ? g_kcal_min : kcal_g;
	g_kcal_b = kcal_b < g_kcal_min ? g_kcal_min : kcal_b;

	if (kcal_r < g_kcal_min || kcal_g < g_kcal_min || kcal_b < g_kcal_min)
		update_preset_lcdc_lut();

	return 0;
}

static int kcal_get_values(int *kcal_r, int *kcal_g, int *kcal_b)
{
	*kcal_r = g_kcal_r;
	*kcal_g = g_kcal_g;
	*kcal_b = g_kcal_b;
	return 0;
}

int kcal_set_min(int kcal_min)
{
	g_kcal_min = kcal_min;

	if (g_kcal_min > 255)
		g_kcal_min = 255;

	if (g_kcal_min < 0)
		g_kcal_min = 0;

	if (g_kcal_min > g_kcal_r || g_kcal_min > g_kcal_g || g_kcal_min > g_kcal_b) {
		g_kcal_r = g_kcal_r < g_kcal_min ? g_kcal_min : g_kcal_r;
		g_kcal_g = g_kcal_g < g_kcal_min ? g_kcal_min : g_kcal_g;
		g_kcal_b = g_kcal_b < g_kcal_min ? g_kcal_min : g_kcal_b;
		update_preset_lcdc_lut();
	}

	return 0;
}

static int kcal_get_min(int *kcal_min)
{
	*kcal_min = g_kcal_min;
	return 0;
}

static int kcal_refresh_values(void)
{
	return update_preset_lcdc_lut();
}

static struct kcal_platform_data kcal_pdata = {
	.set_values = kcal_set_values,
	.get_values = kcal_get_values,
	.refresh_display = kcal_refresh_values,
	.set_min = kcal_set_min,
	.get_min = kcal_get_min
};

static struct platform_device kcal_platrom_device = {
	.name = "kcal_ctrl",
	.dev = {
		.platform_data = &kcal_pdata,
	}
};

void __init add_lcd_kcal_devices(void)
{
	pr_info (" LCD_KCAL_DEBUG : %s \n", __func__);
	platform_device_register(&kcal_platrom_device);
};
#endif

#define HTC_8226_PERSISTENT_RAM_PHYS 0x05B00000
#ifdef CONFIG_HTC_BUILD_EDIAG
#define HTC_8226_PERSISTENT_RAM_SIZE (SZ_1M - SZ_128K - SZ_64K)
#else
#define HTC_8226_PERSISTENT_RAM_SIZE (SZ_1M - SZ_128K)
#endif
#define HTC_8226_RAM_CONSOLE_SIZE    HTC_8226_PERSISTENT_RAM_SIZE

extern int __init htc_8226_dsi_panel_power_register(void);

static struct persistent_ram_descriptor htc_8226_persistent_ram_descs[] = {
	{
		.name = "ram_console",
		.size = HTC_8226_RAM_CONSOLE_SIZE,
	},
};

static struct persistent_ram htc_8226_persistent_ram = {
	.start     = HTC_8226_PERSISTENT_RAM_PHYS,
	.size      = HTC_8226_PERSISTENT_RAM_SIZE,
	.num_descs = ARRAY_SIZE(htc_8226_persistent_ram_descs),
	.descs     = htc_8226_persistent_ram_descs,
};

#ifdef CONFIG_HTC_BUILD_EDIAG
#define MSM_HTC_PMEM_EDIAG_BASE 0x05BD0000
#define MSM_HTC_PMEM_EDIAG_SIZE SZ_64K
#define MSM_HTC_PMEM_EDIAG1_BASE MSM_HTC_PMEM_EDIAG_BASE
#define MSM_HTC_PMEM_EDIAG1_SIZE MSM_HTC_PMEM_EDIAG_SIZE
#define MSM_HTC_PMEM_EDIAG2_BASE MSM_HTC_PMEM_EDIAG_BASE
#define MSM_HTC_PMEM_EDIAG2_SIZE MSM_HTC_PMEM_EDIAG_SIZE
#define MSM_HTC_PMEM_EDIAG3_BASE MSM_HTC_PMEM_EDIAG_BASE
#define MSM_HTC_PMEM_EDIAG3_SIZE MSM_HTC_PMEM_EDIAG_SIZE

static struct android_pmem_platform_data android_pmem_ediag_pdata = {
	.name = "pmem_ediag",
	.start = MSM_HTC_PMEM_EDIAG_BASE,
	.size = MSM_HTC_PMEM_EDIAG_SIZE,
	.no_allocator = 0,
	.cached = 0,
};

static struct android_pmem_platform_data android_pmem_ediag1_pdata = {
	.name = "pmem_ediag1",
	.start = MSM_HTC_PMEM_EDIAG1_BASE,
	.size = MSM_HTC_PMEM_EDIAG1_SIZE,
	.no_allocator = 0,
	.cached = 0,
};

static struct android_pmem_platform_data android_pmem_ediag2_pdata = {
	.name = "pmem_ediag2",
	.start = MSM_HTC_PMEM_EDIAG2_BASE,
	.size = MSM_HTC_PMEM_EDIAG2_SIZE,
	.no_allocator = 0,
	.cached = 0,
};

static struct android_pmem_platform_data android_pmem_ediag3_pdata = {
	.name = "pmem_ediag3",
	.start = MSM_HTC_PMEM_EDIAG3_BASE,
	.size = MSM_HTC_PMEM_EDIAG3_SIZE,
	.no_allocator = 0,
	.cached = 0,
};

static struct platform_device android_pmem_ediag_device = {
	.name = "ediag_pmem",	.id = 1,
	.dev = { .platform_data = &android_pmem_ediag_pdata },
};

static struct platform_device android_pmem_ediag1_device = {
	.name = "ediag_pmem",	.id = 2,
	.dev = { .platform_data = &android_pmem_ediag1_pdata },
};

static struct platform_device android_pmem_ediag2_device = {
	.name = "ediag_pmem",	.id = 3,
	.dev = { .platform_data = &android_pmem_ediag2_pdata },
};

static struct platform_device android_pmem_ediag3_device = {
	.name = "ediag_pmem",	.id = 4,
	.dev = { .platform_data = &android_pmem_ediag3_pdata },
};
#endif

static struct memtype_reserve htc_8226_reserve_table[] __initdata = {
	[MEMTYPE_SMI] = {
	},
	[MEMTYPE_EBI0] = {
		.flags	=	MEMTYPE_FLAGS_1M_ALIGN,
	},
	[MEMTYPE_EBI1] = {
		.flags	=	MEMTYPE_FLAGS_1M_ALIGN,
	},
};

#ifdef CONFIG_HTC_POWER_DEBUG
static struct platform_device htc_8226_cpu_usage_stats_device = {
       .name = "cpu_usage_stats",
       .id = -1,
};

int __init htc_8226_cpu_usage_register(void)
{
       platform_device_register(&htc_8226_cpu_usage_stats_device);
       return 0;
}
#endif

static int htc_8226_paddr_to_memtype(unsigned int paddr)
{
	return MEMTYPE_EBI1;
}

static struct of_dev_auxdata htc_8226_auxdata_lookup[] __initdata = {
	OF_DEV_AUXDATA("qcom,msm-sdcc", 0xF9824000, \
			"msm_sdcc.1", NULL),
	OF_DEV_AUXDATA("qcom,msm-sdcc", 0xF98A4000, \
			"msm_sdcc.2", NULL),
	OF_DEV_AUXDATA("qcom,msm-sdcc", 0xF9864000, \
			"msm_sdcc.3", NULL),
	OF_DEV_AUXDATA("qcom,sdhci-msm", 0xF9824900, \
			"msm_sdcc.1", NULL),
	OF_DEV_AUXDATA("qcom,sdhci-msm", 0xF98A4900, \
			"msm_sdcc.2", NULL),
	OF_DEV_AUXDATA("qcom,sdhci-msm", 0xF9864900, \
			"msm_sdcc.3", NULL),
	{}
};

static struct reserve_info htc_8226_reserve_info __initdata = {
	.memtype_reserve_table = htc_8226_reserve_table,
	.paddr_to_memtype = htc_8226_paddr_to_memtype,
};

static void __init htc_8226_early_memory(void)
{
	reserve_info = &htc_8226_reserve_info;
	of_scan_flat_dt(dt_scan_for_memory_hole, htc_8226_reserve_table);
}

static void __init htc_8226_reserve(void)
{
	reserve_info = &htc_8226_reserve_info;
	of_scan_flat_dt(dt_scan_for_memory_reserve, htc_8226_reserve_table);
	msm_reserve();
}
#ifdef CONFIG_MACH_DUMMY
#define HTC_8x26_USB1_HS_ID_GPIO 54
#else
#define HTC_8x26_USB1_HS_ID_GPIO 1
#endif

static int64_t htc_8x26_get_usbid_adc(void)
{
	return htc_qpnp_adc_get_usbid_adc();
}


static uint32_t htc_8x26_usb_id_gpio_input[] = {
	GPIO_CFG(HTC_8x26_USB1_HS_ID_GPIO, 0, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA),
};
static uint32_t htc_8x26_usb_id_gpio_output[] = {
	GPIO_CFG(HTC_8x26_USB1_HS_ID_GPIO, 0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA),
};


static void htc_8x26_config_usb_id_gpios(bool output)
{
	if (output) {
		if (gpio_tlmm_config(htc_8x26_usb_id_gpio_output[0], GPIO_CFG_ENABLE)) {
			printk(KERN_ERR "[CABLE] fail to config usb id, output = %d\n",output);
			return;
		}
		gpio_set_value(HTC_8x26_USB1_HS_ID_GPIO, 1);
		pr_info("[CABLE] %s: %d output high\n",  __func__, HTC_8x26_USB1_HS_ID_GPIO);
	} else {
		if (gpio_tlmm_config(htc_8x26_usb_id_gpio_input[0], GPIO_CFG_ENABLE)) {
			printk(KERN_ERR "[CABLE] fail to config usb id, output = %d\n",output);
			return;
		}
		pr_info("[CABLE] %s: %d intput nopull\n",  __func__, HTC_8x26_USB1_HS_ID_GPIO);
	}
}

static struct cable_detect_platform_data cable_detect_pdata = {
	.detect_type            = CABLE_TYPE_PMIC_ADC,
	.usb_id_pin_type        = CABLE_TYPE_APP_GPIO,
	.usb_id_pin_gpio        = HTC_8x26_USB1_HS_ID_GPIO,
	.get_adc_cb             = htc_8x26_get_usbid_adc,
	.config_usb_id_gpios    = htc_8x26_config_usb_id_gpios,
#ifdef CONFIG_FB_MSM_HDMI_MHL
	.mhl_1v2_power = mhl_sii9234_1v2_power,
	.usb_dpdn_switch        = m7_usb_dpdn_switch,
#endif
#ifdef CONFIG_HTC_BATT_8960
	.is_pwr_src_plugged_in	= pm8941_is_pwr_src_plugged_in,
#endif
	.vbus_debounce_retry = 1,
};

static struct platform_device cable_detect_device = {
	.name   = "cable_detect",
	.id     = -1,
	.dev    = {
		.platform_data = &cable_detect_pdata,
	},
};

static void msm8x26_cable_detect_register(void)
{
	platform_device_register(&cable_detect_device);
}

static struct android_usb_platform_data android_usb_pdata = {
	.vendor_id      = 0x0bb4,
	.product_id     = 0x0dff,
	.product_name		= "Android Phone",
	.manufacturer_name	= "HTC",
	.serial_number = "123456789012",
	.usb_core_id = 0,
	.usb_rmnet_interface = "smd,bam",
	.usb_diag_interface = "diag",
	.fserial_init_string = "smd:modem,tty,tty:autobot,tty:serial,tty:autobot,tty:acm",
	.nluns = 1,
	.cdrom_lun = 0x1,
	.vzw_unmount_cdrom = 0,
};

#define QCT_ANDROID_USB_REGS 0xFC42B0C8
#define QCT_ANDROID_USB_SIZE 0xc8
static struct resource resources_android_usb[] = {
	{
		.start  = QCT_ANDROID_USB_REGS,
		.end    = QCT_ANDROID_USB_REGS + QCT_ANDROID_USB_SIZE - 1,
		.flags  = IORESOURCE_MEM,
	},
};

static struct platform_device android_usb_device = {
	.name   = "android_usb",
	.id     = -1,
	.num_resources  = ARRAY_SIZE(resources_android_usb),
	.resource       = resources_android_usb,
	.dev    = {
		.platform_data = &android_usb_pdata,
	},
};

static void msm8226_add_usb_devices(void)
{
	__maybe_unused int id;

	android_usb_pdata.serial_number = board_serialno();
#ifdef CONFIG_MACH_MEM_UL
	android_usb_pdata.product_id 	= 0x0629;
#elif defined(CONFIG_MACH_DUMMY)
	android_usb_pdata.product_id 	= 0x060c;
	android_usb_pdata.vzw_unmount_cdrom = 1;
	android_usb_pdata.nluns = 2;
        android_usb_pdata.cdrom_lun = 0x3;
#elif defined(CONFIG_MACH_DUMMY)
	android_usb_pdata.product_id 	= 0x063d;
#elif defined(CONFIG_MACH_DUMMY)
	android_usb_pdata.product_id 	= 0x063e;
	android_usb_pdata.vzw_unmount_cdrom = 1;
#elif defined(CONFIG_MACH_DUMMY)
	android_usb_pdata.product_id 	= 0x0647;
#else
	id = of_machine_projectid(0);
	switch (id) {
		case 297:
			android_usb_pdata.product_id 	= 0x062d;
			break;
		case 298:
			android_usb_pdata.product_id 	= 0x0633;
			break;
		case 290:
			android_usb_pdata.product_id 	= 0x0632;
			break;
		case 296:
			android_usb_pdata.product_id 	= 0x062e;
			break;
		case 295:
			android_usb_pdata.product_id 	= 0x0631;
			break;
		case 291:
			android_usb_pdata.product_id 	= 0x0630;
			android_usb_pdata.nluns     = 2;
			android_usb_pdata.cdrom_lun = 0x2;
			break;
		case 294:
			android_usb_pdata.product_id 	= 0x062f;
			break;
		case 318:
			android_usb_pdata.product_id 	= 0x0645;
			break;
		default:
			android_usb_pdata.product_id 	= 0x0dff;
			break;
	}
#endif
	platform_device_register(&android_usb_device);
}

#if defined(CONFIG_HTC_BATT_8960)
static int critical_alarm_voltage_mv[] = {3000, 3200, 3400};

static struct htc_battery_platform_data htc_battery_pdev_data = {
	.guage_driver = 0,
	.chg_limit_active_mask = HTC_BATT_CHG_LIMIT_BIT_TALK |
								HTC_BATT_CHG_LIMIT_BIT_NAVI |
								HTC_BATT_CHG_LIMIT_BIT_THRML,
	.critical_low_voltage_mv = 3200,
	.critical_alarm_vol_ptr = critical_alarm_voltage_mv,
	.critical_alarm_vol_cols = sizeof(critical_alarm_voltage_mv) / sizeof(int),
	.overload_vol_thr_mv = 4000,
	.overload_curr_thr_ma = 0,
	.smooth_chg_full_delay_min = 3,

	.icharger.name = "pm8x26",
	.icharger.get_charging_source = pm8941_get_charging_source,
	.icharger.get_charging_enabled = pm8941_get_charging_enabled,
	.icharger.set_charger_enable = pm8941_charger_enable,
	.icharger.set_pwrsrc_enable = pm8941_pwrsrc_enable,
	.icharger.set_pwrsrc_and_charger_enable =
						pm8941_set_pwrsrc_and_charger_enable,
	.icharger.set_limit_charge_enable = pm8941_limit_charge_enable,
	.icharger.set_limit_input_current = pm8941_limit_input_current,
	.icharger.set_chg_iusbmax = pm8941_set_chg_iusbmax,
	.icharger.set_chg_curr_settled = pm8941_set_chg_curr_settled,
	.icharger.set_chg_vin_min = pm8941_set_chg_vin_min,
	.icharger.is_ovp = pm8941_is_charger_ovp,
	.icharger.is_batt_temp_fault_disable_chg =
						pm8941_is_batt_temp_fault_disable_chg,
	.icharger.is_under_rating = pm8921_is_pwrsrc_under_rating,
	.icharger.charger_change_notifier_register =
						cable_detect_register_notifier,
	.icharger.dump_all = pm8941_dump_all,
	.icharger.is_safty_timer_timeout = pm8941_is_chg_safety_timer_timeout,
	.icharger.get_attr_text = pm8941_charger_get_attr_text,
	.icharger.is_battery_full_eoc_stop = pm8941_is_batt_full_eoc_stop,
	.icharger.get_charge_type = pm8941_get_charge_type,
	.icharger.get_chg_usb_iusbmax = pm8941_get_chg_usb_iusbmax,
	.icharger.get_chg_curr_settled = pm8941_get_chg_curr_settled,
	.icharger.get_chg_vinmin = pm8941_get_chg_vinmin,
	.icharger.get_input_voltage_regulation =
						pm8941_get_input_voltage_regulation,
	.icharger.store_battery_charger_data = pm8941_store_battery_charger_data_emmc,
	.icharger.set_ftm_charge_enable_type = pm8941_set_ftm_charge_enable_type,

	.igauge.name = "pm8x26",
	.igauge.get_battery_voltage = pm8941_get_batt_voltage,
	.igauge.get_battery_current = pm8941_bms_get_batt_current,
	.igauge.get_battery_temperature = pm8941_get_batt_temperature,
	.igauge.get_battery_id = pm8941_get_batt_id,
	.igauge.get_battery_id_mv = pm8941_get_batt_id_mv,
	.igauge.get_battery_soc = pm8941_bms_get_batt_soc,
	.igauge.get_battery_cc = pm8941_bms_get_batt_cc,
	.igauge.store_battery_gauge_data = pm8941_bms_store_battery_gauge_data_emmc,
	.igauge.store_battery_ui_soc = pm8941_bms_store_battery_ui_soc,
	.igauge.enter_qb_mode = pm8941_bms_enter_qb_mode,
	.igauge.exit_qb_mode = pm8941_bms_exit_qb_mode,
	.igauge.qb_mode_pwr_consumption_check = pm8941_qb_mode_pwr_consumption_check,
	.igauge.get_battery_ui_soc = pm8941_bms_get_battery_ui_soc,
	.igauge.is_battery_temp_fault = pm8941_is_batt_temperature_fault,
	.igauge.is_battery_full = pm8941_is_batt_full,
	.igauge.get_attr_text = pm8941_gauge_get_attr_text,
	.igauge.set_lower_voltage_alarm_threshold =
						pm8941_batt_lower_alarm_threshold_set,
	.igauge.check_soc_for_sw_ocv = pm8941_check_soc_for_sw_ocv,
};
static struct platform_device htc_battery_pdev = {
	.name = "htc_battery",
	.id = -1,
	.dev    = {
		.platform_data = &htc_battery_pdev_data,
	},
};

static void msm8x26_add_batt_devices(void)
{
	platform_device_register(&htc_battery_pdev);
}

static struct platform_device htc_battery_cell_pdev = {
	.name = "htc_battery_cell",
	.id = -1,
};

int __init htc_batt_cell_register(void)
{
	platform_device_register(&htc_battery_cell_pdev);
	return 0;
}
#endif

void __init htc_8226_add_drivers(void)
{
	msm_smem_init();
	msm_init_modem_notifier_list();
	msm_smd_init();
	msm_rpm_driver_init();
	msm_spm_device_init();
	msm_pm_sleep_status_init();
	rpm_regulator_smd_driver_init();
	qpnp_regulator_init();
	if (of_board_is_rumi())
		msm_clock_init(&msm8226_rumi_clock_init_data);
	else
		msm_clock_init(&msm8226_clock_init_data);
	tsens_tm_init_driver();
	msm_thermal_device_init();
	msm8x26_cable_detect_register();
#if defined(CONFIG_HTC_BATT_8960)
	htc_batt_cell_register();
	msm8x26_add_batt_devices();
#endif
	msm8226_add_usb_devices();
	htc_8226_dsi_panel_power_register();
#ifdef CONFIG_HTC_POWER_DEBUG
	htc_8226_cpu_usage_register();
#endif
}

static void __init htc_8226_map_io(void)
{
	msm_map_msm8226_io();
}

void __init htc_8226_init_early(void)
{
        persistent_ram_early_init(&htc_8226_persistent_ram);

}

void __init htc_8226_init(void)
{
	struct of_dev_auxdata *adata = htc_8226_auxdata_lookup;
	printk(KERN_INFO"[htc_8226_init]: Test to see htc_8226_init()\n");

	if (socinfo_init() < 0)
		pr_err("%s: socinfo_init() failed\n", __func__);

	pr_info("%s: pid=%d, pcbid=%d, socver=0x%x\n", __func__
		, of_machine_pid(), of_machine_pcbid(), of_machine_socver());

#ifdef CONFIG_BT
	bt_export_bd_address();
#endif
	msm8226_htc_init_gpiomux();
	board_dt_populate(adata);
	htc_8226_add_drivers();
#ifdef CONFIG_HTC_BUILD_EDIAG
	platform_device_register(&android_pmem_ediag_device);
	platform_device_register(&android_pmem_ediag1_device);
	platform_device_register(&android_pmem_ediag2_device);
	platform_device_register(&android_pmem_ediag3_device);
#endif
#ifdef CONFIG_PERFLOCK
	platform_device_register(&msm8226_device_perf_lock);
#endif
#ifdef CONFIG_HTC_POWER_DEBUG
	htc_monitor_init();
#endif
#ifdef CONFIG_LCD_KCAL
	add_lcd_kcal_devices();
#endif
}

static const char *htc_8226_dt_match[] __initconst = {
	"htc,memul",
	"htc,a5ul",
	"htc,a5dwg",
	NULL
};

DT_MACHINE_START(htc_8226_DT, "Qualcomm MSM 8226 (Flattened Device Tree)")
	.map_io = htc_8226_map_io,
	.init_early = htc_8226_init_early,
	.init_irq = msm_dt_init_irq,
	.init_machine = htc_8226_init,
	.handle_irq = gic_handle_irq,
	.timer = &msm_dt_timer,
	.dt_compat = htc_8226_dt_match,
	.reserve = htc_8226_reserve,
	.init_very_early = htc_8226_early_memory,
	.restart = msm_restart,
	.smp = &arm_smp_ops,
MACHINE_END
