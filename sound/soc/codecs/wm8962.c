/*
 * wm8962.c  --  WM8962 ALSA SoC Audio driver
 *
 * Copyright 2010 Wolfson Microelectronics plc
 *
 * Author: Mark Brown <broonie@opensource.wolfsonmicro.com>
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/gcd.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/pm_runtime.h>
#include <linux/regmap.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <sound/core.h>
#include <sound/jack.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/initval.h>
#include <sound/tlv.h>
#include <sound/wm8962.h>
#include <trace/events/asoc.h>

#include "wm8962.h"

#define WM8962_NUM_SUPPLIES 8
static const char *wm8962_supply_names[WM8962_NUM_SUPPLIES] = {
	"DCVDD",
	"DBVDD",
	"AVDD",
	"CPVDD",
	"MICVDD",
	"PLLVDD",
	"SPKVDD1",
	"SPKVDD2",
};

struct wm8962_priv {
	struct regmap *regmap;
	struct snd_soc_codec *codec;

	int sysclk;
	int sysclk_rate;

	int bclk;  
	int lrclk;

	struct completion fll_lock;
	int fll_src;
	int fll_fref;
	int fll_fout;

	u16 dsp2_ena;

	struct delayed_work mic_work;
	struct snd_soc_jack *jack;

	struct regulator_bulk_data supplies[WM8962_NUM_SUPPLIES];
	struct notifier_block disable_nb[WM8962_NUM_SUPPLIES];

#if defined(CONFIG_INPUT) || defined(CONFIG_INPUT_MODULE)
	struct input_dev *beep;
	struct work_struct beep_work;
	int beep_rate;
#endif

#ifdef CONFIG_GPIOLIB
	struct gpio_chip gpio_chip;
#endif

	int irq;
};

#define WM8962_REGULATOR_EVENT(n) \
static int wm8962_regulator_event_##n(struct notifier_block *nb, \
				    unsigned long event, void *data)	\
{ \
	struct wm8962_priv *wm8962 = container_of(nb, struct wm8962_priv, \
						  disable_nb[n]); \
	if (event & REGULATOR_EVENT_DISABLE) { \
		regcache_mark_dirty(wm8962->regmap);	\
	} \
	return 0; \
}

WM8962_REGULATOR_EVENT(0)
WM8962_REGULATOR_EVENT(1)
WM8962_REGULATOR_EVENT(2)
WM8962_REGULATOR_EVENT(3)
WM8962_REGULATOR_EVENT(4)
WM8962_REGULATOR_EVENT(5)
WM8962_REGULATOR_EVENT(6)
WM8962_REGULATOR_EVENT(7)

static struct reg_default wm8962_reg[] = {
	{ 0, 0x009F },   
	{ 1, 0x049F },   
	{ 2, 0x0000 },   
	{ 3, 0x0000 },   

	{ 5, 0x0018 },   
	{ 6, 0x2008 },   
	{ 7, 0x000A },   

	{ 9, 0x0300 },   
	{ 10, 0x00C0 },  
	{ 11, 0x00C0 },  

	{ 14, 0x0040 },   
	{ 15, 0x6243 },   

	{ 17, 0x007B },   

	{ 19, 0x1C32 },   
	{ 20, 0x3200 },   
	{ 21, 0x00C0 },   
	{ 22, 0x00C0 },   
	{ 23, 0x0160 },   
	{ 24, 0x0000 },   
	{ 25, 0x0000 },   
	{ 26, 0x0000 },   
	{ 27, 0x0010 },   
	{ 28, 0x0000 },   

	{ 30, 0x005E },   
	{ 31, 0x0000 },   
	{ 32, 0x0145 },   
	{ 33, 0x0145 },   
	{ 34, 0x0009 },   
	{ 35, 0x0003 },   
	{ 37, 0x0008 },   
	{ 38, 0x0008 },   

	{ 40, 0x0000 },   
	{ 41, 0x0000 },   

	{ 51, 0x0003 },   

	{ 56, 0x0506 },   
	{ 57, 0x0000 },   
	{ 58, 0x0000 },   

	{ 60, 0x0300 },   
	{ 61, 0x0300 },   

	{ 64, 0x0810 },   

	{ 68, 0x001B },   
	{ 69, 0x0000 },   

	{ 71, 0x01FB },   
	{ 72, 0x0000 },   

	{ 82, 0x0004 },   

	{ 87, 0x0000 },   

	{ 90, 0x0000 },   

	{ 93, 0x0000 },   
	{ 94, 0x0000 },   

	{ 99, 0x0000 },   
	{ 100, 0x0000 },   
	{ 101, 0x0000 },   
	{ 102, 0x013F },   
	{ 103, 0x013F },   

	{ 105, 0x0000 },   
	{ 106, 0x0000 },   
	{ 107, 0x013F },   
	{ 108, 0x013F },   
	{ 109, 0x0003 },   
	{ 110, 0x0002 },   

	{ 115, 0x0006 },   
	{ 116, 0x0026 },   

	{ 119, 0x0000 },   

	{ 124, 0x0011 },   
	{ 125, 0x004B },   
	{ 126, 0x000D },   
	{ 127, 0x0000 },   

	{ 131, 0x0000 },   

	{ 136, 0x0067 },   
	{ 137, 0x001C },   
	{ 138, 0x0071 },   
	{ 139, 0x00C7 },   
	{ 140, 0x0067 },   
	{ 141, 0x0048 },   
	{ 142, 0x0022 },   
	{ 143, 0x0097 },   

	{ 155, 0x000C },   
	{ 156, 0x0039 },   
	{ 157, 0x0180 },   

	{ 159, 0x0032 },   
	{ 160, 0x0018 },   
	{ 161, 0x007D },   
	{ 162, 0x0008 },   

	{ 252, 0x0005 },   

	{ 256, 0x0000 },   
	{ 257, 0x0000 },   
	{ 258, 0x0000 },   
	{ 259, 0x0000 },   
	{ 260, 0x0000 },   
	{ 261, 0x0000 },   
	{ 262, 0x0000 },   

	{ 264, 0x0000 },   
	{ 265, 0x0000 },   

	{ 268, 0x0000 },   
	{ 269, 0x0000 },   
	{ 270, 0x0000 },   
	{ 271, 0x0000 },   

	{ 276, 0x000C },   
	{ 277, 0x0925 },   
	{ 278, 0x0000 },   
	{ 279, 0x0000 },   
	{ 280, 0x0000 },   

	{ 285, 0x0000 },   

	{ 335, 0x0004 },   
	{ 336, 0x6318 },   
	{ 337, 0x6300 },   
	{ 338, 0x0FCA },   
	{ 339, 0x0400 },   
	{ 340, 0x00D8 },   
	{ 341, 0x1EB5 },   
	{ 342, 0xF145 },   
	{ 343, 0x0B75 },   
	{ 344, 0x01C5 },   
	{ 345, 0x1C58 },   
	{ 346, 0xF373 },   
	{ 347, 0x0A54 },   
	{ 348, 0x0558 },   
	{ 349, 0x168E },   
	{ 350, 0xF829 },   
	{ 351, 0x07AD },   
	{ 352, 0x1103 },   
	{ 353, 0x0564 },   
	{ 354, 0x0559 },   
	{ 355, 0x4000 },   
	{ 356, 0x6318 },   
	{ 357, 0x6300 },   
	{ 358, 0x0FCA },   
	{ 359, 0x0400 },   
	{ 360, 0x00D8 },   
	{ 361, 0x1EB5 },   
	{ 362, 0xF145 },   
	{ 363, 0x0B75 },   
	{ 364, 0x01C5 },   
	{ 365, 0x1C58 },   
	{ 366, 0xF373 },   
	{ 367, 0x0A54 },   
	{ 368, 0x0558 },   
	{ 369, 0x168E },   
	{ 370, 0xF829 },   
	{ 371, 0x07AD },   
	{ 372, 0x1103 },   
	{ 373, 0x0564 },   
	{ 374, 0x0559 },   
	{ 375, 0x4000 },   

	{ 513, 0x0000 },   
	{ 514, 0x0000 },   

	{ 516, 0x8100 },   
	{ 517, 0x8100 },   

	{ 568, 0x0030 },   
	{ 569, 0xFFED },   

	{ 576, 0x0000 },   

	{ 584, 0x002D },   

	{ 586, 0x0000 },   

	{ 768, 0x1C00 },   

	{ 8192, 0x0000 },   

	{ 9216, 0x0030 },   
	{ 9217, 0x0000 },   
	{ 9218, 0x0000 },   

	{ 12288, 0x0000 },   
	{ 12289, 0x0000 },   

	{ 13312, 0x0000 },   
	{ 13313, 0x0000 },   

	{ 14336, 0x0000 },   
	{ 14337, 0x0000 },   

	{ 15360, 0x000A },   

	{ 16384, 0x0000 },   
	{ 16385, 0x0000 },   
	{ 16386, 0x0000 },   
	{ 16387, 0x0000 },   
	{ 16388, 0x0000 },   
	{ 16389, 0x0000 },   

	{ 16896, 0x0002 },   
	{ 16897, 0xBD12 },   
	{ 16898, 0x007C },   
	{ 16899, 0x586C },   
	{ 16900, 0x0053 },   
	{ 16901, 0x8121 },   
	{ 16902, 0x003F },   
	{ 16903, 0x8BD8 },   
	{ 16904, 0x0032 },   
	{ 16905, 0xF52D },   
	{ 16906, 0x0065 },   
	{ 16907, 0xAC8C },   
	{ 16908, 0x006B },   
	{ 16909, 0xE087 },   
	{ 16910, 0x0072 },   
	{ 16911, 0x1483 },   
	{ 16912, 0x0072 },   
	{ 16913, 0x1483 },   
	{ 16914, 0x0043 },   
	{ 16915, 0x3525 },   
	{ 16916, 0x0006 },   
	{ 16917, 0x6A4A },   
	{ 16918, 0x0043 },   
	{ 16919, 0x6079 },   
	{ 16920, 0x0008 },   
	{ 16921, 0x0000 },   
	{ 16922, 0x0001 },   
	{ 16923, 0x0000 },   
	{ 16924, 0x0059 },   
	{ 16925, 0x999A },   

	{ 17048, 0x0083 },   
	{ 17049, 0x98AD },   

	{ 17920, 0x007F },   
	{ 17921, 0xFFFF },   
	{ 17922, 0x0000 },   
	{ 17923, 0x0000 },   
	{ 17924, 0x0000 },   
	{ 17925, 0x0000 },   
	{ 17926, 0x0000 },   
	{ 17927, 0x0000 },   
	{ 17928, 0x0000 },   
	{ 17929, 0x0000 },   
	{ 17930, 0x0000 },   
	{ 17931, 0x0000 },   
	{ 17932, 0x0000 },   
	{ 17933, 0x0000 },   
	{ 17934, 0x0000 },   
	{ 17935, 0x0000 },   
	{ 17936, 0x0000 },   
	{ 17937, 0x0000 },   
	{ 17938, 0x0000 },   
	{ 17939, 0x0000 },   
	{ 17940, 0x0000 },   
	{ 17941, 0x0000 },   
	{ 17942, 0x0000 },   
	{ 17943, 0x0000 },   
	{ 17944, 0x0000 },   
	{ 17945, 0x0000 },   
	{ 17946, 0x0000 },   
	{ 17947, 0x0000 },   
	{ 17948, 0x0000 },   
	{ 17949, 0x0000 },   
	{ 17950, 0x0000 },   
	{ 17951, 0x0000 },   
	{ 17952, 0x0000 },   
	{ 17953, 0x0000 },   
	{ 17954, 0x0000 },   
	{ 17955, 0x0000 },   
	{ 17956, 0x0000 },   
	{ 17957, 0x0000 },   
	{ 17958, 0x0000 },   
	{ 17959, 0x0000 },   
	{ 17960, 0x0000 },   
	{ 17961, 0x0000 },   
	{ 17962, 0x0000 },   
	{ 17963, 0x0000 },   
	{ 17964, 0x0000 },   
	{ 17965, 0x0000 },   
	{ 17966, 0x0000 },   
	{ 17967, 0x0000 },   
	{ 17968, 0x0000 },   
	{ 17969, 0x0000 },   
	{ 17970, 0x0000 },   
	{ 17971, 0x0000 },   
	{ 17972, 0x0000 },   
	{ 17973, 0x0000 },   
	{ 17974, 0x0000 },   
	{ 17975, 0x0000 },   
	{ 17976, 0x0000 },   
	{ 17977, 0x0000 },   
	{ 17978, 0x0000 },   
	{ 17979, 0x0000 },   
	{ 17980, 0x0000 },   
	{ 17981, 0x0000 },   
	{ 17982, 0x0000 },   
	{ 17983, 0x0000 },   

	{ 18432, 0x0020 },   
	{ 18433, 0x0000 },   
	{ 18434, 0x0040 },   
	{ 18435, 0x0000 },   

	{ 18944, 0x007F },   
	{ 18945, 0xFFFF },   
	{ 18946, 0x0000 },   
	{ 18947, 0x0000 },   
	{ 18948, 0x0000 },   
	{ 18949, 0x0000 },   
	{ 18950, 0x0000 },   
	{ 18951, 0x0000 },   
	{ 18952, 0x0000 },   
	{ 18953, 0x0000 },   
	{ 18954, 0x0000 },   
	{ 18955, 0x0000 },   
	{ 18956, 0x0000 },   
	{ 18957, 0x0000 },   
	{ 18958, 0x0000 },   
	{ 18959, 0x0000 },   
	{ 18960, 0x0000 },   
	{ 18961, 0x0000 },   
	{ 18962, 0x0000 },   
	{ 18963, 0x0000 },   
	{ 18964, 0x0000 },   
	{ 18965, 0x0000 },   
	{ 18966, 0x0000 },   
	{ 18967, 0x0000 },   
	{ 18968, 0x0000 },   
	{ 18969, 0x0000 },   
	{ 18970, 0x0000 },   
	{ 18971, 0x0000 },   
	{ 18972, 0x0000 },   
	{ 18973, 0x0000 },   
	{ 18974, 0x0000 },   
	{ 18975, 0x0000 },   
	{ 18976, 0x0000 },   
	{ 18977, 0x0000 },   
	{ 18978, 0x0000 },   
	{ 18979, 0x0000 },   
	{ 18980, 0x0000 },   
	{ 18981, 0x0000 },   
	{ 18982, 0x0000 },   
	{ 18983, 0x0000 },   
	{ 18984, 0x0000 },   
	{ 18985, 0x0000 },   
	{ 18986, 0x0000 },   
	{ 18987, 0x0000 },   
	{ 18988, 0x0000 },   
	{ 18989, 0x0000 },   
	{ 18990, 0x0000 },   
	{ 18991, 0x0000 },   
	{ 18992, 0x0000 },   
	{ 18993, 0x0000 },   
	{ 18994, 0x0000 },   
	{ 18995, 0x0000 },   
	{ 18996, 0x0000 },   
	{ 18997, 0x0000 },   
	{ 18998, 0x0000 },   
	{ 18999, 0x0000 },   
	{ 19000, 0x0000 },   
	{ 19001, 0x0000 },   
	{ 19002, 0x0000 },   
	{ 19003, 0x0000 },   
	{ 19004, 0x0000 },   
	{ 19005, 0x0000 },   
	{ 19006, 0x0000 },   
	{ 19007, 0x0000 },   

	{ 19456, 0x007F },   
	{ 19457, 0xFFFF },   
	{ 19458, 0x0000 },   
	{ 19459, 0x0000 },   
	{ 19460, 0x0000 },   
	{ 19461, 0x0000 },   
	{ 19462, 0x0000 },   
	{ 19463, 0x0000 },   
	{ 19464, 0x0000 },   
	{ 19465, 0x0000 },   
	{ 19466, 0x0000 },   
	{ 19467, 0x0000 },   
	{ 19468, 0x0000 },   
	{ 19469, 0x0000 },   
	{ 19470, 0x0000 },   
	{ 19471, 0x0000 },   
	{ 19472, 0x0000 },   
	{ 19473, 0x0000 },   
	{ 19474, 0x0000 },   
	{ 19475, 0x0000 },   
	{ 19476, 0x0000 },   
	{ 19477, 0x0000 },   
	{ 19478, 0x0000 },   
	{ 19479, 0x0000 },   
	{ 19480, 0x0000 },   
	{ 19481, 0x0000 },   
	{ 19482, 0x0000 },   
	{ 19483, 0x0000 },   
	{ 19484, 0x0000 },   
	{ 19485, 0x0000 },   
	{ 19486, 0x0000 },   
	{ 19487, 0x0000 },   
	{ 19488, 0x0000 },   
	{ 19489, 0x0000 },   
	{ 19490, 0x0000 },   
	{ 19491, 0x0000 },   
	{ 19492, 0x0000 },   
	{ 19493, 0x0000 },   
	{ 19494, 0x0000 },   
	{ 19495, 0x0000 },   
	{ 19496, 0x0000 },   
	{ 19497, 0x0000 },   
	{ 19498, 0x0000 },   
	{ 19499, 0x0000 },   
	{ 19500, 0x0000 },   
	{ 19501, 0x0000 },   
	{ 19502, 0x0000 },   
	{ 19503, 0x0000 },   
	{ 19504, 0x0000 },   
	{ 19505, 0x0000 },   
	{ 19506, 0x0000 },   
	{ 19507, 0x0000 },   
	{ 19508, 0x0000 },   
	{ 19509, 0x0000 },   
	{ 19510, 0x0000 },   
	{ 19511, 0x0000 },   
	{ 19512, 0x0000 },   
	{ 19513, 0x0000 },   
	{ 19514, 0x0000 },   
	{ 19515, 0x0000 },   
	{ 19516, 0x0000 },   
	{ 19517, 0x0000 },   
	{ 19518, 0x0000 },   
	{ 19519, 0x0000 },   

	{ 19968, 0x0020 },   
	{ 19969, 0x0000 },   
	{ 19970, 0x0040 },   
	{ 19971, 0x0000 },   

	{ 20480, 0x007F },   
	{ 20481, 0xFFFF },   
	{ 20482, 0x0000 },   
	{ 20483, 0x0000 },   
	{ 20484, 0x0000 },   
	{ 20485, 0x0000 },   
	{ 20486, 0x0000 },   
	{ 20487, 0x0000 },   
	{ 20488, 0x0000 },   
	{ 20489, 0x0000 },   
	{ 20490, 0x0000 },   
	{ 20491, 0x0000 },   
	{ 20492, 0x0000 },   
	{ 20493, 0x0000 },   
	{ 20494, 0x0000 },   
	{ 20495, 0x0000 },   
	{ 20496, 0x0000 },   
	{ 20497, 0x0000 },   
	{ 20498, 0x0000 },   
	{ 20499, 0x0000 },   
	{ 20500, 0x0000 },   
	{ 20501, 0x0000 },   
	{ 20502, 0x0000 },   
	{ 20503, 0x0000 },   
	{ 20504, 0x0000 },   
	{ 20505, 0x0000 },   
	{ 20506, 0x0000 },   
	{ 20507, 0x0000 },   
	{ 20508, 0x0000 },   
	{ 20509, 0x0000 },   
	{ 20510, 0x0000 },   
	{ 20511, 0x0000 },   
	{ 20512, 0x0000 },   
	{ 20513, 0x0000 },   
	{ 20514, 0x0000 },   
	{ 20515, 0x0000 },   
	{ 20516, 0x0000 },   
	{ 20517, 0x0000 },   
	{ 20518, 0x0000 },   
	{ 20519, 0x0000 },   
	{ 20520, 0x0000 },   
	{ 20521, 0x0000 },   
	{ 20522, 0x0000 },   
	{ 20523, 0x0000 },   
	{ 20524, 0x0000 },   
	{ 20525, 0x0000 },   
	{ 20526, 0x0000 },   
	{ 20527, 0x0000 },   
	{ 20528, 0x0000 },   
	{ 20529, 0x0000 },   
	{ 20530, 0x0000 },   
	{ 20531, 0x0000 },   
	{ 20532, 0x0000 },   
	{ 20533, 0x0000 },   
	{ 20534, 0x0000 },   
	{ 20535, 0x0000 },   
	{ 20536, 0x0000 },   
	{ 20537, 0x0000 },   
	{ 20538, 0x0000 },   
	{ 20539, 0x0000 },   
	{ 20540, 0x0000 },   
	{ 20541, 0x0000 },   
	{ 20542, 0x0000 },   
	{ 20543, 0x0000 },   

	{ 20992, 0x008C },   
	{ 20993, 0x0200 },   
	{ 20994, 0x0035 },   
	{ 20995, 0x0700 },   
	{ 20996, 0x003A },   
	{ 20997, 0x4100 },   
	{ 20998, 0x008B },   
	{ 20999, 0x7D00 },   
	{ 21000, 0x003A },   
	{ 21001, 0x4100 },   
	{ 21002, 0x008C },   
	{ 21003, 0xFEE8 },   
	{ 21004, 0x0078 },   
	{ 21005, 0x0000 },   
	{ 21006, 0x003F },   
	{ 21007, 0xB260 },   
	{ 21008, 0x002D },   
	{ 21009, 0x1818 },   
	{ 21010, 0x0020 },   
	{ 21011, 0x0000 },   
	{ 21012, 0x00F1 },   
	{ 21013, 0x8340 },   
	{ 21014, 0x00FB },   
	{ 21015, 0x8300 },   
	{ 21016, 0x00EE },   
	{ 21017, 0xAEC0 },   
	{ 21018, 0x00FB },   
	{ 21019, 0xAC40 },   
	{ 21020, 0x00F1 },   
	{ 21021, 0x7F80 },   
	{ 21022, 0x00F4 },   
	{ 21023, 0x3B40 },   
	{ 21024, 0x00F5 },   
	{ 21025, 0xFB00 },   
	{ 21026, 0x00EA },   
	{ 21027, 0x10C0 },   
	{ 21028, 0x00FC },   
	{ 21029, 0xC580 },   
	{ 21030, 0x00E2 },   
	{ 21031, 0x75C0 },   
	{ 21032, 0x0004 },   
	{ 21033, 0xB480 },   
	{ 21034, 0x00D4 },   
	{ 21035, 0xF980 },   
	{ 21036, 0x0004 },   
	{ 21037, 0x9140 },   
	{ 21038, 0x00D8 },   
	{ 21039, 0xA480 },   
	{ 21040, 0x0002 },   
	{ 21041, 0x3DC0 },   
	{ 21042, 0x00CF },   
	{ 21043, 0x7A80 },   
	{ 21044, 0x00DC },   
	{ 21045, 0x0600 },   
	{ 21046, 0x00F2 },   
	{ 21047, 0xDAC0 },   
	{ 21048, 0x00BA },   
	{ 21049, 0xF340 },   
	{ 21050, 0x000A },   
	{ 21051, 0x7940 },   
	{ 21052, 0x001C },   
	{ 21053, 0x0680 },   
	{ 21054, 0x00FD },   
	{ 21055, 0x2D00 },   
	{ 21056, 0x001C },   
	{ 21057, 0xE840 },   
	{ 21058, 0x000D },   
	{ 21059, 0xDC40 },   
	{ 21060, 0x00FC },   
	{ 21061, 0x9D00 },   
	{ 21062, 0x0009 },   
	{ 21063, 0x5580 },   
	{ 21064, 0x00FE },   
	{ 21065, 0x7E80 },   
	{ 21066, 0x000E },   
	{ 21067, 0xAB40 },   
	{ 21068, 0x00F9 },   
	{ 21069, 0x9880 },   
	{ 21070, 0x0009 },   
	{ 21071, 0x87C0 },   
	{ 21072, 0x00FD },   
	{ 21073, 0x2C40 },   
	{ 21074, 0x0009 },   
	{ 21075, 0x4800 },   
	{ 21076, 0x0003 },   
	{ 21077, 0x5F40 },   
	{ 21078, 0x0000 },   
	{ 21079, 0x8700 },   
	{ 21080, 0x00FA },   
	{ 21081, 0xE4C0 },   
	{ 21082, 0x0000 },   
	{ 21083, 0x0B40 },   
	{ 21084, 0x0004 },   
	{ 21085, 0xE180 },   
	{ 21086, 0x0001 },   
	{ 21087, 0x1F40 },   
	{ 21088, 0x00F8 },   
	{ 21089, 0xB000 },   
	{ 21090, 0x00FB },   
	{ 21091, 0xCBC0 },   
	{ 21092, 0x0004 },   
	{ 21093, 0xF380 },   
	{ 21094, 0x0007 },   
	{ 21095, 0xDF40 },   
	{ 21096, 0x00FF },   
	{ 21097, 0x0700 },   
	{ 21098, 0x00EF },   
	{ 21099, 0xD700 },   
	{ 21100, 0x00FB },   
	{ 21101, 0xAF40 },   
	{ 21102, 0x0010 },   
	{ 21103, 0x8A80 },   
	{ 21104, 0x0011 },   
	{ 21105, 0x07C0 },   
	{ 21106, 0x00E0 },   
	{ 21107, 0x0800 },   
	{ 21108, 0x00D2 },   
	{ 21109, 0x7600 },   
	{ 21110, 0x0020 },   
	{ 21111, 0xCF40 },   
	{ 21112, 0x0030 },   
	{ 21113, 0x2340 },   
	{ 21114, 0x00FD },   
	{ 21115, 0x69C0 },   
	{ 21116, 0x0028 },   
	{ 21117, 0x3500 },   
	{ 21118, 0x0006 },   
	{ 21119, 0x3300 },   
	{ 21120, 0x00D9 },   
	{ 21121, 0xF6C0 },   
	{ 21122, 0x00F3 },   
	{ 21123, 0x3340 },   
	{ 21124, 0x000F },   
	{ 21125, 0x4200 },   
	{ 21126, 0x0004 },   
	{ 21127, 0x0C80 },   
	{ 21128, 0x00FB },   
	{ 21129, 0x3F80 },   
	{ 21130, 0x00F7 },   
	{ 21131, 0x57C0 },   
	{ 21132, 0x0003 },   
	{ 21133, 0x5400 },   
	{ 21134, 0x0000 },   
	{ 21135, 0xC6C0 },   
	{ 21136, 0x0003 },   
	{ 21137, 0x12C0 },   
	{ 21138, 0x00FD },   
	{ 21139, 0x8580 },   
};

static bool wm8962_volatile_register(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case WM8962_CLOCKING1:
	case WM8962_CLOCKING2:
	case WM8962_SOFTWARE_RESET:
	case WM8962_ALC2:
	case WM8962_THERMAL_SHUTDOWN_STATUS:
	case WM8962_ADDITIONAL_CONTROL_4:
	case WM8962_CLASS_D_CONTROL_1:
	case WM8962_DC_SERVO_6:
	case WM8962_INTERRUPT_STATUS_1:
	case WM8962_INTERRUPT_STATUS_2:
	case WM8962_DSP2_EXECCONTROL:
		return true;
	default:
		return false;
	}
}

static bool wm8962_readable_register(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case WM8962_LEFT_INPUT_VOLUME:
	case WM8962_RIGHT_INPUT_VOLUME:
	case WM8962_HPOUTL_VOLUME:
	case WM8962_HPOUTR_VOLUME:
	case WM8962_CLOCKING1:
	case WM8962_ADC_DAC_CONTROL_1:
	case WM8962_ADC_DAC_CONTROL_2:
	case WM8962_AUDIO_INTERFACE_0:
	case WM8962_CLOCKING2:
	case WM8962_AUDIO_INTERFACE_1:
	case WM8962_LEFT_DAC_VOLUME:
	case WM8962_RIGHT_DAC_VOLUME:
	case WM8962_AUDIO_INTERFACE_2:
	case WM8962_SOFTWARE_RESET:
	case WM8962_ALC1:
	case WM8962_ALC2:
	case WM8962_ALC3:
	case WM8962_NOISE_GATE:
	case WM8962_LEFT_ADC_VOLUME:
	case WM8962_RIGHT_ADC_VOLUME:
	case WM8962_ADDITIONAL_CONTROL_1:
	case WM8962_ADDITIONAL_CONTROL_2:
	case WM8962_PWR_MGMT_1:
	case WM8962_PWR_MGMT_2:
	case WM8962_ADDITIONAL_CONTROL_3:
	case WM8962_ANTI_POP:
	case WM8962_CLOCKING_3:
	case WM8962_INPUT_MIXER_CONTROL_1:
	case WM8962_LEFT_INPUT_MIXER_VOLUME:
	case WM8962_RIGHT_INPUT_MIXER_VOLUME:
	case WM8962_INPUT_MIXER_CONTROL_2:
	case WM8962_INPUT_BIAS_CONTROL:
	case WM8962_LEFT_INPUT_PGA_CONTROL:
	case WM8962_RIGHT_INPUT_PGA_CONTROL:
	case WM8962_SPKOUTL_VOLUME:
	case WM8962_SPKOUTR_VOLUME:
	case WM8962_THERMAL_SHUTDOWN_STATUS:
	case WM8962_ADDITIONAL_CONTROL_4:
	case WM8962_CLASS_D_CONTROL_1:
	case WM8962_CLASS_D_CONTROL_2:
	case WM8962_CLOCKING_4:
	case WM8962_DAC_DSP_MIXING_1:
	case WM8962_DAC_DSP_MIXING_2:
	case WM8962_DC_SERVO_0:
	case WM8962_DC_SERVO_1:
	case WM8962_DC_SERVO_4:
	case WM8962_DC_SERVO_6:
	case WM8962_ANALOGUE_PGA_BIAS:
	case WM8962_ANALOGUE_HP_0:
	case WM8962_ANALOGUE_HP_2:
	case WM8962_CHARGE_PUMP_1:
	case WM8962_CHARGE_PUMP_B:
	case WM8962_WRITE_SEQUENCER_CONTROL_1:
	case WM8962_WRITE_SEQUENCER_CONTROL_2:
	case WM8962_WRITE_SEQUENCER_CONTROL_3:
	case WM8962_CONTROL_INTERFACE:
	case WM8962_MIXER_ENABLES:
	case WM8962_HEADPHONE_MIXER_1:
	case WM8962_HEADPHONE_MIXER_2:
	case WM8962_HEADPHONE_MIXER_3:
	case WM8962_HEADPHONE_MIXER_4:
	case WM8962_SPEAKER_MIXER_1:
	case WM8962_SPEAKER_MIXER_2:
	case WM8962_SPEAKER_MIXER_3:
	case WM8962_SPEAKER_MIXER_4:
	case WM8962_SPEAKER_MIXER_5:
	case WM8962_BEEP_GENERATOR_1:
	case WM8962_OSCILLATOR_TRIM_3:
	case WM8962_OSCILLATOR_TRIM_4:
	case WM8962_OSCILLATOR_TRIM_7:
	case WM8962_ANALOGUE_CLOCKING1:
	case WM8962_ANALOGUE_CLOCKING2:
	case WM8962_ANALOGUE_CLOCKING3:
	case WM8962_PLL_SOFTWARE_RESET:
	case WM8962_PLL2:
	case WM8962_PLL_4:
	case WM8962_PLL_9:
	case WM8962_PLL_10:
	case WM8962_PLL_11:
	case WM8962_PLL_12:
	case WM8962_PLL_13:
	case WM8962_PLL_14:
	case WM8962_PLL_15:
	case WM8962_PLL_16:
	case WM8962_FLL_CONTROL_1:
	case WM8962_FLL_CONTROL_2:
	case WM8962_FLL_CONTROL_3:
	case WM8962_FLL_CONTROL_5:
	case WM8962_FLL_CONTROL_6:
	case WM8962_FLL_CONTROL_7:
	case WM8962_FLL_CONTROL_8:
	case WM8962_GENERAL_TEST_1:
	case WM8962_DF1:
	case WM8962_DF2:
	case WM8962_DF3:
	case WM8962_DF4:
	case WM8962_DF5:
	case WM8962_DF6:
	case WM8962_DF7:
	case WM8962_LHPF1:
	case WM8962_LHPF2:
	case WM8962_THREED1:
	case WM8962_THREED2:
	case WM8962_THREED3:
	case WM8962_THREED4:
	case WM8962_DRC_1:
	case WM8962_DRC_2:
	case WM8962_DRC_3:
	case WM8962_DRC_4:
	case WM8962_DRC_5:
	case WM8962_TLOOPBACK:
	case WM8962_EQ1:
	case WM8962_EQ2:
	case WM8962_EQ3:
	case WM8962_EQ4:
	case WM8962_EQ5:
	case WM8962_EQ6:
	case WM8962_EQ7:
	case WM8962_EQ8:
	case WM8962_EQ9:
	case WM8962_EQ10:
	case WM8962_EQ11:
	case WM8962_EQ12:
	case WM8962_EQ13:
	case WM8962_EQ14:
	case WM8962_EQ15:
	case WM8962_EQ16:
	case WM8962_EQ17:
	case WM8962_EQ18:
	case WM8962_EQ19:
	case WM8962_EQ20:
	case WM8962_EQ21:
	case WM8962_EQ22:
	case WM8962_EQ23:
	case WM8962_EQ24:
	case WM8962_EQ25:
	case WM8962_EQ26:
	case WM8962_EQ27:
	case WM8962_EQ28:
	case WM8962_EQ29:
	case WM8962_EQ30:
	case WM8962_EQ31:
	case WM8962_EQ32:
	case WM8962_EQ33:
	case WM8962_EQ34:
	case WM8962_EQ35:
	case WM8962_EQ36:
	case WM8962_EQ37:
	case WM8962_EQ38:
	case WM8962_EQ39:
	case WM8962_EQ40:
	case WM8962_EQ41:
	case WM8962_GPIO_BASE:
	case WM8962_GPIO_2:
	case WM8962_GPIO_3:
	case WM8962_GPIO_5:
	case WM8962_GPIO_6:
	case WM8962_INTERRUPT_STATUS_1:
	case WM8962_INTERRUPT_STATUS_2:
	case WM8962_INTERRUPT_STATUS_1_MASK:
	case WM8962_INTERRUPT_STATUS_2_MASK:
	case WM8962_INTERRUPT_CONTROL:
	case WM8962_IRQ_DEBOUNCE:
	case WM8962_MICINT_SOURCE_POL:
	case WM8962_DSP2_POWER_MANAGEMENT:
	case WM8962_DSP2_EXECCONTROL:
	case WM8962_DSP2_INSTRUCTION_RAM_0:
	case WM8962_DSP2_ADDRESS_RAM_2:
	case WM8962_DSP2_ADDRESS_RAM_1:
	case WM8962_DSP2_ADDRESS_RAM_0:
	case WM8962_DSP2_DATA1_RAM_1:
	case WM8962_DSP2_DATA1_RAM_0:
	case WM8962_DSP2_DATA2_RAM_1:
	case WM8962_DSP2_DATA2_RAM_0:
	case WM8962_DSP2_DATA3_RAM_1:
	case WM8962_DSP2_DATA3_RAM_0:
	case WM8962_DSP2_COEFF_RAM_0:
	case WM8962_RETUNEADC_SHARED_COEFF_1:
	case WM8962_RETUNEADC_SHARED_COEFF_0:
	case WM8962_RETUNEDAC_SHARED_COEFF_1:
	case WM8962_RETUNEDAC_SHARED_COEFF_0:
	case WM8962_SOUNDSTAGE_ENABLES_1:
	case WM8962_SOUNDSTAGE_ENABLES_0:
	case WM8962_HDBASS_AI_1:
	case WM8962_HDBASS_AI_0:
	case WM8962_HDBASS_AR_1:
	case WM8962_HDBASS_AR_0:
	case WM8962_HDBASS_B_1:
	case WM8962_HDBASS_B_0:
	case WM8962_HDBASS_K_1:
	case WM8962_HDBASS_K_0:
	case WM8962_HDBASS_N1_1:
	case WM8962_HDBASS_N1_0:
	case WM8962_HDBASS_N2_1:
	case WM8962_HDBASS_N2_0:
	case WM8962_HDBASS_N3_1:
	case WM8962_HDBASS_N3_0:
	case WM8962_HDBASS_N4_1:
	case WM8962_HDBASS_N4_0:
	case WM8962_HDBASS_N5_1:
	case WM8962_HDBASS_N5_0:
	case WM8962_HDBASS_X1_1:
	case WM8962_HDBASS_X1_0:
	case WM8962_HDBASS_X2_1:
	case WM8962_HDBASS_X2_0:
	case WM8962_HDBASS_X3_1:
	case WM8962_HDBASS_X3_0:
	case WM8962_HDBASS_ATK_1:
	case WM8962_HDBASS_ATK_0:
	case WM8962_HDBASS_DCY_1:
	case WM8962_HDBASS_DCY_0:
	case WM8962_HDBASS_PG_1:
	case WM8962_HDBASS_PG_0:
	case WM8962_HPF_C_1:
	case WM8962_HPF_C_0:
	case WM8962_ADCL_RETUNE_C1_1:
	case WM8962_ADCL_RETUNE_C1_0:
	case WM8962_ADCL_RETUNE_C2_1:
	case WM8962_ADCL_RETUNE_C2_0:
	case WM8962_ADCL_RETUNE_C3_1:
	case WM8962_ADCL_RETUNE_C3_0:
	case WM8962_ADCL_RETUNE_C4_1:
	case WM8962_ADCL_RETUNE_C4_0:
	case WM8962_ADCL_RETUNE_C5_1:
	case WM8962_ADCL_RETUNE_C5_0:
	case WM8962_ADCL_RETUNE_C6_1:
	case WM8962_ADCL_RETUNE_C6_0:
	case WM8962_ADCL_RETUNE_C7_1:
	case WM8962_ADCL_RETUNE_C7_0:
	case WM8962_ADCL_RETUNE_C8_1:
	case WM8962_ADCL_RETUNE_C8_0:
	case WM8962_ADCL_RETUNE_C9_1:
	case WM8962_ADCL_RETUNE_C9_0:
	case WM8962_ADCL_RETUNE_C10_1:
	case WM8962_ADCL_RETUNE_C10_0:
	case WM8962_ADCL_RETUNE_C11_1:
	case WM8962_ADCL_RETUNE_C11_0:
	case WM8962_ADCL_RETUNE_C12_1:
	case WM8962_ADCL_RETUNE_C12_0:
	case WM8962_ADCL_RETUNE_C13_1:
	case WM8962_ADCL_RETUNE_C13_0:
	case WM8962_ADCL_RETUNE_C14_1:
	case WM8962_ADCL_RETUNE_C14_0:
	case WM8962_ADCL_RETUNE_C15_1:
	case WM8962_ADCL_RETUNE_C15_0:
	case WM8962_ADCL_RETUNE_C16_1:
	case WM8962_ADCL_RETUNE_C16_0:
	case WM8962_ADCL_RETUNE_C17_1:
	case WM8962_ADCL_RETUNE_C17_0:
	case WM8962_ADCL_RETUNE_C18_1:
	case WM8962_ADCL_RETUNE_C18_0:
	case WM8962_ADCL_RETUNE_C19_1:
	case WM8962_ADCL_RETUNE_C19_0:
	case WM8962_ADCL_RETUNE_C20_1:
	case WM8962_ADCL_RETUNE_C20_0:
	case WM8962_ADCL_RETUNE_C21_1:
	case WM8962_ADCL_RETUNE_C21_0:
	case WM8962_ADCL_RETUNE_C22_1:
	case WM8962_ADCL_RETUNE_C22_0:
	case WM8962_ADCL_RETUNE_C23_1:
	case WM8962_ADCL_RETUNE_C23_0:
	case WM8962_ADCL_RETUNE_C24_1:
	case WM8962_ADCL_RETUNE_C24_0:
	case WM8962_ADCL_RETUNE_C25_1:
	case WM8962_ADCL_RETUNE_C25_0:
	case WM8962_ADCL_RETUNE_C26_1:
	case WM8962_ADCL_RETUNE_C26_0:
	case WM8962_ADCL_RETUNE_C27_1:
	case WM8962_ADCL_RETUNE_C27_0:
	case WM8962_ADCL_RETUNE_C28_1:
	case WM8962_ADCL_RETUNE_C28_0:
	case WM8962_ADCL_RETUNE_C29_1:
	case WM8962_ADCL_RETUNE_C29_0:
	case WM8962_ADCL_RETUNE_C30_1:
	case WM8962_ADCL_RETUNE_C30_0:
	case WM8962_ADCL_RETUNE_C31_1:
	case WM8962_ADCL_RETUNE_C31_0:
	case WM8962_ADCL_RETUNE_C32_1:
	case WM8962_ADCL_RETUNE_C32_0:
	case WM8962_RETUNEADC_PG2_1:
	case WM8962_RETUNEADC_PG2_0:
	case WM8962_RETUNEADC_PG_1:
	case WM8962_RETUNEADC_PG_0:
	case WM8962_ADCR_RETUNE_C1_1:
	case WM8962_ADCR_RETUNE_C1_0:
	case WM8962_ADCR_RETUNE_C2_1:
	case WM8962_ADCR_RETUNE_C2_0:
	case WM8962_ADCR_RETUNE_C3_1:
	case WM8962_ADCR_RETUNE_C3_0:
	case WM8962_ADCR_RETUNE_C4_1:
	case WM8962_ADCR_RETUNE_C4_0:
	case WM8962_ADCR_RETUNE_C5_1:
	case WM8962_ADCR_RETUNE_C5_0:
	case WM8962_ADCR_RETUNE_C6_1:
	case WM8962_ADCR_RETUNE_C6_0:
	case WM8962_ADCR_RETUNE_C7_1:
	case WM8962_ADCR_RETUNE_C7_0:
	case WM8962_ADCR_RETUNE_C8_1:
	case WM8962_ADCR_RETUNE_C8_0:
	case WM8962_ADCR_RETUNE_C9_1:
	case WM8962_ADCR_RETUNE_C9_0:
	case WM8962_ADCR_RETUNE_C10_1:
	case WM8962_ADCR_RETUNE_C10_0:
	case WM8962_ADCR_RETUNE_C11_1:
	case WM8962_ADCR_RETUNE_C11_0:
	case WM8962_ADCR_RETUNE_C12_1:
	case WM8962_ADCR_RETUNE_C12_0:
	case WM8962_ADCR_RETUNE_C13_1:
	case WM8962_ADCR_RETUNE_C13_0:
	case WM8962_ADCR_RETUNE_C14_1:
	case WM8962_ADCR_RETUNE_C14_0:
	case WM8962_ADCR_RETUNE_C15_1:
	case WM8962_ADCR_RETUNE_C15_0:
	case WM8962_ADCR_RETUNE_C16_1:
	case WM8962_ADCR_RETUNE_C16_0:
	case WM8962_ADCR_RETUNE_C17_1:
	case WM8962_ADCR_RETUNE_C17_0:
	case WM8962_ADCR_RETUNE_C18_1:
	case WM8962_ADCR_RETUNE_C18_0:
	case WM8962_ADCR_RETUNE_C19_1:
	case WM8962_ADCR_RETUNE_C19_0:
	case WM8962_ADCR_RETUNE_C20_1:
	case WM8962_ADCR_RETUNE_C20_0:
	case WM8962_ADCR_RETUNE_C21_1:
	case WM8962_ADCR_RETUNE_C21_0:
	case WM8962_ADCR_RETUNE_C22_1:
	case WM8962_ADCR_RETUNE_C22_0:
	case WM8962_ADCR_RETUNE_C23_1:
	case WM8962_ADCR_RETUNE_C23_0:
	case WM8962_ADCR_RETUNE_C24_1:
	case WM8962_ADCR_RETUNE_C24_0:
	case WM8962_ADCR_RETUNE_C25_1:
	case WM8962_ADCR_RETUNE_C25_0:
	case WM8962_ADCR_RETUNE_C26_1:
	case WM8962_ADCR_RETUNE_C26_0:
	case WM8962_ADCR_RETUNE_C27_1:
	case WM8962_ADCR_RETUNE_C27_0:
	case WM8962_ADCR_RETUNE_C28_1:
	case WM8962_ADCR_RETUNE_C28_0:
	case WM8962_ADCR_RETUNE_C29_1:
	case WM8962_ADCR_RETUNE_C29_0:
	case WM8962_ADCR_RETUNE_C30_1:
	case WM8962_ADCR_RETUNE_C30_0:
	case WM8962_ADCR_RETUNE_C31_1:
	case WM8962_ADCR_RETUNE_C31_0:
	case WM8962_ADCR_RETUNE_C32_1:
	case WM8962_ADCR_RETUNE_C32_0:
	case WM8962_DACL_RETUNE_C1_1:
	case WM8962_DACL_RETUNE_C1_0:
	case WM8962_DACL_RETUNE_C2_1:
	case WM8962_DACL_RETUNE_C2_0:
	case WM8962_DACL_RETUNE_C3_1:
	case WM8962_DACL_RETUNE_C3_0:
	case WM8962_DACL_RETUNE_C4_1:
	case WM8962_DACL_RETUNE_C4_0:
	case WM8962_DACL_RETUNE_C5_1:
	case WM8962_DACL_RETUNE_C5_0:
	case WM8962_DACL_RETUNE_C6_1:
	case WM8962_DACL_RETUNE_C6_0:
	case WM8962_DACL_RETUNE_C7_1:
	case WM8962_DACL_RETUNE_C7_0:
	case WM8962_DACL_RETUNE_C8_1:
	case WM8962_DACL_RETUNE_C8_0:
	case WM8962_DACL_RETUNE_C9_1:
	case WM8962_DACL_RETUNE_C9_0:
	case WM8962_DACL_RETUNE_C10_1:
	case WM8962_DACL_RETUNE_C10_0:
	case WM8962_DACL_RETUNE_C11_1:
	case WM8962_DACL_RETUNE_C11_0:
	case WM8962_DACL_RETUNE_C12_1:
	case WM8962_DACL_RETUNE_C12_0:
	case WM8962_DACL_RETUNE_C13_1:
	case WM8962_DACL_RETUNE_C13_0:
	case WM8962_DACL_RETUNE_C14_1:
	case WM8962_DACL_RETUNE_C14_0:
	case WM8962_DACL_RETUNE_C15_1:
	case WM8962_DACL_RETUNE_C15_0:
	case WM8962_DACL_RETUNE_C16_1:
	case WM8962_DACL_RETUNE_C16_0:
	case WM8962_DACL_RETUNE_C17_1:
	case WM8962_DACL_RETUNE_C17_0:
	case WM8962_DACL_RETUNE_C18_1:
	case WM8962_DACL_RETUNE_C18_0:
	case WM8962_DACL_RETUNE_C19_1:
	case WM8962_DACL_RETUNE_C19_0:
	case WM8962_DACL_RETUNE_C20_1:
	case WM8962_DACL_RETUNE_C20_0:
	case WM8962_DACL_RETUNE_C21_1:
	case WM8962_DACL_RETUNE_C21_0:
	case WM8962_DACL_RETUNE_C22_1:
	case WM8962_DACL_RETUNE_C22_0:
	case WM8962_DACL_RETUNE_C23_1:
	case WM8962_DACL_RETUNE_C23_0:
	case WM8962_DACL_RETUNE_C24_1:
	case WM8962_DACL_RETUNE_C24_0:
	case WM8962_DACL_RETUNE_C25_1:
	case WM8962_DACL_RETUNE_C25_0:
	case WM8962_DACL_RETUNE_C26_1:
	case WM8962_DACL_RETUNE_C26_0:
	case WM8962_DACL_RETUNE_C27_1:
	case WM8962_DACL_RETUNE_C27_0:
	case WM8962_DACL_RETUNE_C28_1:
	case WM8962_DACL_RETUNE_C28_0:
	case WM8962_DACL_RETUNE_C29_1:
	case WM8962_DACL_RETUNE_C29_0:
	case WM8962_DACL_RETUNE_C30_1:
	case WM8962_DACL_RETUNE_C30_0:
	case WM8962_DACL_RETUNE_C31_1:
	case WM8962_DACL_RETUNE_C31_0:
	case WM8962_DACL_RETUNE_C32_1:
	case WM8962_DACL_RETUNE_C32_0:
	case WM8962_RETUNEDAC_PG2_1:
	case WM8962_RETUNEDAC_PG2_0:
	case WM8962_RETUNEDAC_PG_1:
	case WM8962_RETUNEDAC_PG_0:
	case WM8962_DACR_RETUNE_C1_1:
	case WM8962_DACR_RETUNE_C1_0:
	case WM8962_DACR_RETUNE_C2_1:
	case WM8962_DACR_RETUNE_C2_0:
	case WM8962_DACR_RETUNE_C3_1:
	case WM8962_DACR_RETUNE_C3_0:
	case WM8962_DACR_RETUNE_C4_1:
	case WM8962_DACR_RETUNE_C4_0:
	case WM8962_DACR_RETUNE_C5_1:
	case WM8962_DACR_RETUNE_C5_0:
	case WM8962_DACR_RETUNE_C6_1:
	case WM8962_DACR_RETUNE_C6_0:
	case WM8962_DACR_RETUNE_C7_1:
	case WM8962_DACR_RETUNE_C7_0:
	case WM8962_DACR_RETUNE_C8_1:
	case WM8962_DACR_RETUNE_C8_0:
	case WM8962_DACR_RETUNE_C9_1:
	case WM8962_DACR_RETUNE_C9_0:
	case WM8962_DACR_RETUNE_C10_1:
	case WM8962_DACR_RETUNE_C10_0:
	case WM8962_DACR_RETUNE_C11_1:
	case WM8962_DACR_RETUNE_C11_0:
	case WM8962_DACR_RETUNE_C12_1:
	case WM8962_DACR_RETUNE_C12_0:
	case WM8962_DACR_RETUNE_C13_1:
	case WM8962_DACR_RETUNE_C13_0:
	case WM8962_DACR_RETUNE_C14_1:
	case WM8962_DACR_RETUNE_C14_0:
	case WM8962_DACR_RETUNE_C15_1:
	case WM8962_DACR_RETUNE_C15_0:
	case WM8962_DACR_RETUNE_C16_1:
	case WM8962_DACR_RETUNE_C16_0:
	case WM8962_DACR_RETUNE_C17_1:
	case WM8962_DACR_RETUNE_C17_0:
	case WM8962_DACR_RETUNE_C18_1:
	case WM8962_DACR_RETUNE_C18_0:
	case WM8962_DACR_RETUNE_C19_1:
	case WM8962_DACR_RETUNE_C19_0:
	case WM8962_DACR_RETUNE_C20_1:
	case WM8962_DACR_RETUNE_C20_0:
	case WM8962_DACR_RETUNE_C21_1:
	case WM8962_DACR_RETUNE_C21_0:
	case WM8962_DACR_RETUNE_C22_1:
	case WM8962_DACR_RETUNE_C22_0:
	case WM8962_DACR_RETUNE_C23_1:
	case WM8962_DACR_RETUNE_C23_0:
	case WM8962_DACR_RETUNE_C24_1:
	case WM8962_DACR_RETUNE_C24_0:
	case WM8962_DACR_RETUNE_C25_1:
	case WM8962_DACR_RETUNE_C25_0:
	case WM8962_DACR_RETUNE_C26_1:
	case WM8962_DACR_RETUNE_C26_0:
	case WM8962_DACR_RETUNE_C27_1:
	case WM8962_DACR_RETUNE_C27_0:
	case WM8962_DACR_RETUNE_C28_1:
	case WM8962_DACR_RETUNE_C28_0:
	case WM8962_DACR_RETUNE_C29_1:
	case WM8962_DACR_RETUNE_C29_0:
	case WM8962_DACR_RETUNE_C30_1:
	case WM8962_DACR_RETUNE_C30_0:
	case WM8962_DACR_RETUNE_C31_1:
	case WM8962_DACR_RETUNE_C31_0:
	case WM8962_DACR_RETUNE_C32_1:
	case WM8962_DACR_RETUNE_C32_0:
	case WM8962_VSS_XHD2_1:
	case WM8962_VSS_XHD2_0:
	case WM8962_VSS_XHD3_1:
	case WM8962_VSS_XHD3_0:
	case WM8962_VSS_XHN1_1:
	case WM8962_VSS_XHN1_0:
	case WM8962_VSS_XHN2_1:
	case WM8962_VSS_XHN2_0:
	case WM8962_VSS_XHN3_1:
	case WM8962_VSS_XHN3_0:
	case WM8962_VSS_XLA_1:
	case WM8962_VSS_XLA_0:
	case WM8962_VSS_XLB_1:
	case WM8962_VSS_XLB_0:
	case WM8962_VSS_XLG_1:
	case WM8962_VSS_XLG_0:
	case WM8962_VSS_PG2_1:
	case WM8962_VSS_PG2_0:
	case WM8962_VSS_PG_1:
	case WM8962_VSS_PG_0:
	case WM8962_VSS_XTD1_1:
	case WM8962_VSS_XTD1_0:
	case WM8962_VSS_XTD2_1:
	case WM8962_VSS_XTD2_0:
	case WM8962_VSS_XTD3_1:
	case WM8962_VSS_XTD3_0:
	case WM8962_VSS_XTD4_1:
	case WM8962_VSS_XTD4_0:
	case WM8962_VSS_XTD5_1:
	case WM8962_VSS_XTD5_0:
	case WM8962_VSS_XTD6_1:
	case WM8962_VSS_XTD6_0:
	case WM8962_VSS_XTD7_1:
	case WM8962_VSS_XTD7_0:
	case WM8962_VSS_XTD8_1:
	case WM8962_VSS_XTD8_0:
	case WM8962_VSS_XTD9_1:
	case WM8962_VSS_XTD9_0:
	case WM8962_VSS_XTD10_1:
	case WM8962_VSS_XTD10_0:
	case WM8962_VSS_XTD11_1:
	case WM8962_VSS_XTD11_0:
	case WM8962_VSS_XTD12_1:
	case WM8962_VSS_XTD12_0:
	case WM8962_VSS_XTD13_1:
	case WM8962_VSS_XTD13_0:
	case WM8962_VSS_XTD14_1:
	case WM8962_VSS_XTD14_0:
	case WM8962_VSS_XTD15_1:
	case WM8962_VSS_XTD15_0:
	case WM8962_VSS_XTD16_1:
	case WM8962_VSS_XTD16_0:
	case WM8962_VSS_XTD17_1:
	case WM8962_VSS_XTD17_0:
	case WM8962_VSS_XTD18_1:
	case WM8962_VSS_XTD18_0:
	case WM8962_VSS_XTD19_1:
	case WM8962_VSS_XTD19_0:
	case WM8962_VSS_XTD20_1:
	case WM8962_VSS_XTD20_0:
	case WM8962_VSS_XTD21_1:
	case WM8962_VSS_XTD21_0:
	case WM8962_VSS_XTD22_1:
	case WM8962_VSS_XTD22_0:
	case WM8962_VSS_XTD23_1:
	case WM8962_VSS_XTD23_0:
	case WM8962_VSS_XTD24_1:
	case WM8962_VSS_XTD24_0:
	case WM8962_VSS_XTD25_1:
	case WM8962_VSS_XTD25_0:
	case WM8962_VSS_XTD26_1:
	case WM8962_VSS_XTD26_0:
	case WM8962_VSS_XTD27_1:
	case WM8962_VSS_XTD27_0:
	case WM8962_VSS_XTD28_1:
	case WM8962_VSS_XTD28_0:
	case WM8962_VSS_XTD29_1:
	case WM8962_VSS_XTD29_0:
	case WM8962_VSS_XTD30_1:
	case WM8962_VSS_XTD30_0:
	case WM8962_VSS_XTD31_1:
	case WM8962_VSS_XTD31_0:
	case WM8962_VSS_XTD32_1:
	case WM8962_VSS_XTD32_0:
	case WM8962_VSS_XTS1_1:
	case WM8962_VSS_XTS1_0:
	case WM8962_VSS_XTS2_1:
	case WM8962_VSS_XTS2_0:
	case WM8962_VSS_XTS3_1:
	case WM8962_VSS_XTS3_0:
	case WM8962_VSS_XTS4_1:
	case WM8962_VSS_XTS4_0:
	case WM8962_VSS_XTS5_1:
	case WM8962_VSS_XTS5_0:
	case WM8962_VSS_XTS6_1:
	case WM8962_VSS_XTS6_0:
	case WM8962_VSS_XTS7_1:
	case WM8962_VSS_XTS7_0:
	case WM8962_VSS_XTS8_1:
	case WM8962_VSS_XTS8_0:
	case WM8962_VSS_XTS9_1:
	case WM8962_VSS_XTS9_0:
	case WM8962_VSS_XTS10_1:
	case WM8962_VSS_XTS10_0:
	case WM8962_VSS_XTS11_1:
	case WM8962_VSS_XTS11_0:
	case WM8962_VSS_XTS12_1:
	case WM8962_VSS_XTS12_0:
	case WM8962_VSS_XTS13_1:
	case WM8962_VSS_XTS13_0:
	case WM8962_VSS_XTS14_1:
	case WM8962_VSS_XTS14_0:
	case WM8962_VSS_XTS15_1:
	case WM8962_VSS_XTS15_0:
	case WM8962_VSS_XTS16_1:
	case WM8962_VSS_XTS16_0:
	case WM8962_VSS_XTS17_1:
	case WM8962_VSS_XTS17_0:
	case WM8962_VSS_XTS18_1:
	case WM8962_VSS_XTS18_0:
	case WM8962_VSS_XTS19_1:
	case WM8962_VSS_XTS19_0:
	case WM8962_VSS_XTS20_1:
	case WM8962_VSS_XTS20_0:
	case WM8962_VSS_XTS21_1:
	case WM8962_VSS_XTS21_0:
	case WM8962_VSS_XTS22_1:
	case WM8962_VSS_XTS22_0:
	case WM8962_VSS_XTS23_1:
	case WM8962_VSS_XTS23_0:
	case WM8962_VSS_XTS24_1:
	case WM8962_VSS_XTS24_0:
	case WM8962_VSS_XTS25_1:
	case WM8962_VSS_XTS25_0:
	case WM8962_VSS_XTS26_1:
	case WM8962_VSS_XTS26_0:
	case WM8962_VSS_XTS27_1:
	case WM8962_VSS_XTS27_0:
	case WM8962_VSS_XTS28_1:
	case WM8962_VSS_XTS28_0:
	case WM8962_VSS_XTS29_1:
	case WM8962_VSS_XTS29_0:
	case WM8962_VSS_XTS30_1:
	case WM8962_VSS_XTS30_0:
	case WM8962_VSS_XTS31_1:
	case WM8962_VSS_XTS31_0:
	case WM8962_VSS_XTS32_1:
	case WM8962_VSS_XTS32_0:
		return true;
	default:
		return false;
	}
}

static int wm8962_reset(struct wm8962_priv *wm8962)
{
	int ret;

	ret = regmap_write(wm8962->regmap, WM8962_SOFTWARE_RESET, 0x6243);
	if (ret != 0)
		return ret;

	return regmap_write(wm8962->regmap, WM8962_PLL_SOFTWARE_RESET, 0);
}

static const DECLARE_TLV_DB_SCALE(inpga_tlv, -2325, 75, 0);
static const DECLARE_TLV_DB_SCALE(mixin_tlv, -1500, 300, 0);
static const unsigned int mixinpga_tlv[] = {
	TLV_DB_RANGE_HEAD(5),
	0, 1, TLV_DB_SCALE_ITEM(0, 600, 0),
	2, 2, TLV_DB_SCALE_ITEM(1300, 1300, 0),
	3, 4, TLV_DB_SCALE_ITEM(1800, 200, 0),
	5, 5, TLV_DB_SCALE_ITEM(2400, 0, 0),
	6, 7, TLV_DB_SCALE_ITEM(2700, 300, 0),
};
static const DECLARE_TLV_DB_SCALE(beep_tlv, -9600, 600, 1);
static const DECLARE_TLV_DB_SCALE(digital_tlv, -7200, 75, 1);
static const DECLARE_TLV_DB_SCALE(st_tlv, -3600, 300, 0);
static const DECLARE_TLV_DB_SCALE(inmix_tlv, -600, 600, 0);
static const DECLARE_TLV_DB_SCALE(bypass_tlv, -1500, 300, 0);
static const DECLARE_TLV_DB_SCALE(out_tlv, -12100, 100, 1);
static const DECLARE_TLV_DB_SCALE(hp_tlv, -700, 100, 0);
static const unsigned int classd_tlv[] = {
	TLV_DB_RANGE_HEAD(2),
	0, 6, TLV_DB_SCALE_ITEM(0, 150, 0),
	7, 7, TLV_DB_SCALE_ITEM(1200, 0, 0),
};
static const DECLARE_TLV_DB_SCALE(eq_tlv, -1200, 100, 0);

static int wm8962_dsp2_write_config(struct snd_soc_codec *codec)
{
	return 0;
}

static int wm8962_dsp2_set_enable(struct snd_soc_codec *codec, u16 val)
{
	u16 adcl = snd_soc_read(codec, WM8962_LEFT_ADC_VOLUME);
	u16 adcr = snd_soc_read(codec, WM8962_RIGHT_ADC_VOLUME);
	u16 dac = snd_soc_read(codec, WM8962_ADC_DAC_CONTROL_1);

	
	snd_soc_write(codec, WM8962_LEFT_ADC_VOLUME, 0);
	snd_soc_write(codec, WM8962_RIGHT_ADC_VOLUME, WM8962_ADC_VU);
	snd_soc_update_bits(codec, WM8962_ADC_DAC_CONTROL_1,
			    WM8962_DAC_MUTE, WM8962_DAC_MUTE);

	snd_soc_write(codec, WM8962_SOUNDSTAGE_ENABLES_0, val);

	
	snd_soc_write(codec, WM8962_LEFT_ADC_VOLUME, adcl);
	snd_soc_write(codec, WM8962_RIGHT_ADC_VOLUME, adcr);
	snd_soc_update_bits(codec, WM8962_ADC_DAC_CONTROL_1,
			    WM8962_DAC_MUTE, dac);

	return 0;
}

static int wm8962_dsp2_start(struct snd_soc_codec *codec)
{
	struct wm8962_priv *wm8962 = snd_soc_codec_get_drvdata(codec);

	wm8962_dsp2_write_config(codec);

	snd_soc_write(codec, WM8962_DSP2_EXECCONTROL, WM8962_DSP2_RUNR);

	wm8962_dsp2_set_enable(codec, wm8962->dsp2_ena);

	return 0;
}

static int wm8962_dsp2_stop(struct snd_soc_codec *codec)
{
	wm8962_dsp2_set_enable(codec, 0);

	snd_soc_write(codec, WM8962_DSP2_EXECCONTROL, WM8962_DSP2_STOP);

	return 0;
}

#define WM8962_DSP2_ENABLE(xname, xshift) \
{	.iface = SNDRV_CTL_ELEM_IFACE_MIXER, .name = xname, \
	.info = wm8962_dsp2_ena_info, \
	.get = wm8962_dsp2_ena_get, .put = wm8962_dsp2_ena_put, \
	.private_value = xshift }

static int wm8962_dsp2_ena_info(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_BOOLEAN;

	uinfo->count = 1;
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = 1;

	return 0;
}

static int wm8962_dsp2_ena_get(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	int shift = kcontrol->private_value;
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct wm8962_priv *wm8962 = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = !!(wm8962->dsp2_ena & 1 << shift);

	return 0;
}

static int wm8962_dsp2_ena_put(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	int shift = kcontrol->private_value;
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct wm8962_priv *wm8962 = snd_soc_codec_get_drvdata(codec);
	int old = wm8962->dsp2_ena;
	int ret = 0;
	int dsp2_running = snd_soc_read(codec, WM8962_DSP2_POWER_MANAGEMENT) &
		WM8962_DSP2_ENA;

	mutex_lock(&codec->mutex);

	if (ucontrol->value.integer.value[0])
		wm8962->dsp2_ena |= 1 << shift;
	else
		wm8962->dsp2_ena &= ~(1 << shift);

	if (wm8962->dsp2_ena == old)
		goto out;

	ret = 1;

	if (dsp2_running) {
		if (wm8962->dsp2_ena)
			wm8962_dsp2_set_enable(codec, wm8962->dsp2_ena);
		else
			wm8962_dsp2_stop(codec);
	}

out:
	mutex_unlock(&codec->mutex);

	return ret;
}

static int wm8962_put_hp_sw(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	u16 *reg_cache = codec->reg_cache;
	int ret;

	
        ret = snd_soc_put_volsw(kcontrol, ucontrol);
	if (ret == 0)
		return 0;

	
	if (snd_soc_read(codec, WM8962_PWR_MGMT_2) & WM8962_HPOUTL_PGA_ENA)
		return snd_soc_write(codec, WM8962_HPOUTL_VOLUME,
				     reg_cache[WM8962_HPOUTL_VOLUME]);

	
	if (snd_soc_read(codec, WM8962_PWR_MGMT_2) & WM8962_HPOUTR_PGA_ENA)
		return snd_soc_write(codec, WM8962_HPOUTR_VOLUME,
				     reg_cache[WM8962_HPOUTR_VOLUME]);

	return 0;
}

static int wm8962_put_spk_sw(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	int ret;

	
        ret = snd_soc_put_volsw(kcontrol, ucontrol);
	if (ret == 0)
		return 0;

	
	ret = snd_soc_read(codec, WM8962_PWR_MGMT_2);
	if (ret & WM8962_SPKOUTL_PGA_ENA) {
		snd_soc_write(codec, WM8962_SPKOUTL_VOLUME,
			      snd_soc_read(codec, WM8962_SPKOUTL_VOLUME));
		return 1;
	}

	
	if (ret & WM8962_SPKOUTR_PGA_ENA)
		snd_soc_write(codec, WM8962_SPKOUTR_VOLUME,
			      snd_soc_read(codec, WM8962_SPKOUTR_VOLUME));

	return 1;
}

static const char *cap_hpf_mode_text[] = {
	"Hi-fi", "Application"
};

static const struct soc_enum cap_hpf_mode =
	SOC_ENUM_SINGLE(WM8962_ADC_DAC_CONTROL_2, 10, 2, cap_hpf_mode_text);


static const char *cap_lhpf_mode_text[] = {
	"LPF", "HPF"
};

static const struct soc_enum cap_lhpf_mode =
	SOC_ENUM_SINGLE(WM8962_LHPF1, 1, 2, cap_lhpf_mode_text);

static const struct snd_kcontrol_new wm8962_snd_controls[] = {
SOC_DOUBLE("Input Mixer Switch", WM8962_INPUT_MIXER_CONTROL_1, 3, 2, 1, 1),

SOC_SINGLE_TLV("MIXINL IN2L Volume", WM8962_LEFT_INPUT_MIXER_VOLUME, 6, 7, 0,
	       mixin_tlv),
SOC_SINGLE_TLV("MIXINL PGA Volume", WM8962_LEFT_INPUT_MIXER_VOLUME, 3, 7, 0,
	       mixinpga_tlv),
SOC_SINGLE_TLV("MIXINL IN3L Volume", WM8962_LEFT_INPUT_MIXER_VOLUME, 0, 7, 0,
	       mixin_tlv),

SOC_SINGLE_TLV("MIXINR IN2R Volume", WM8962_RIGHT_INPUT_MIXER_VOLUME, 6, 7, 0,
	       mixin_tlv),
SOC_SINGLE_TLV("MIXINR PGA Volume", WM8962_RIGHT_INPUT_MIXER_VOLUME, 3, 7, 0,
	       mixinpga_tlv),
SOC_SINGLE_TLV("MIXINR IN3R Volume", WM8962_RIGHT_INPUT_MIXER_VOLUME, 0, 7, 0,
	       mixin_tlv),

SOC_DOUBLE_R_TLV("Digital Capture Volume", WM8962_LEFT_ADC_VOLUME,
		 WM8962_RIGHT_ADC_VOLUME, 1, 127, 0, digital_tlv),
SOC_DOUBLE_R_TLV("Capture Volume", WM8962_LEFT_INPUT_VOLUME,
		 WM8962_RIGHT_INPUT_VOLUME, 0, 63, 0, inpga_tlv),
SOC_DOUBLE_R("Capture Switch", WM8962_LEFT_INPUT_VOLUME,
	     WM8962_RIGHT_INPUT_VOLUME, 7, 1, 1),
SOC_DOUBLE_R("Capture ZC Switch", WM8962_LEFT_INPUT_VOLUME,
	     WM8962_RIGHT_INPUT_VOLUME, 6, 1, 1),
SOC_SINGLE("Capture HPF Switch", WM8962_ADC_DAC_CONTROL_1, 0, 1, 1),
SOC_ENUM("Capture HPF Mode", cap_hpf_mode),
SOC_SINGLE("Capture HPF Cutoff", WM8962_ADC_DAC_CONTROL_2, 7, 7, 0),
SOC_SINGLE("Capture LHPF Switch", WM8962_LHPF1, 0, 1, 0),
SOC_ENUM("Capture LHPF Mode", cap_lhpf_mode),

SOC_DOUBLE_R_TLV("Sidetone Volume", WM8962_DAC_DSP_MIXING_1,
		 WM8962_DAC_DSP_MIXING_2, 4, 12, 0, st_tlv),

SOC_DOUBLE_R_TLV("Digital Playback Volume", WM8962_LEFT_DAC_VOLUME,
		 WM8962_RIGHT_DAC_VOLUME, 1, 127, 0, digital_tlv),
SOC_SINGLE("DAC High Performance Switch", WM8962_ADC_DAC_CONTROL_2, 0, 1, 0),
SOC_SINGLE("DAC L/R Swap Switch", WM8962_AUDIO_INTERFACE_0, 5, 1, 0),
SOC_SINGLE("ADC L/R Swap Switch", WM8962_AUDIO_INTERFACE_0, 8, 1, 0),

SOC_SINGLE("ADC High Performance Switch", WM8962_ADDITIONAL_CONTROL_1,
	   5, 1, 0),

SOC_SINGLE_TLV("Beep Volume", WM8962_BEEP_GENERATOR_1, 4, 15, 0, beep_tlv),

SOC_DOUBLE_R_TLV("Headphone Volume", WM8962_HPOUTL_VOLUME,
		 WM8962_HPOUTR_VOLUME, 0, 127, 0, out_tlv),
SOC_DOUBLE_EXT("Headphone Switch", WM8962_PWR_MGMT_2, 1, 0, 1, 1,
	       snd_soc_get_volsw, wm8962_put_hp_sw),
SOC_DOUBLE_R("Headphone ZC Switch", WM8962_HPOUTL_VOLUME, WM8962_HPOUTR_VOLUME,
	     7, 1, 0),
SOC_DOUBLE_TLV("Headphone Aux Volume", WM8962_ANALOGUE_HP_2, 3, 6, 7, 0,
	       hp_tlv),

SOC_DOUBLE_R("Headphone Mixer Switch", WM8962_HEADPHONE_MIXER_3,
	     WM8962_HEADPHONE_MIXER_4, 8, 1, 1),

SOC_SINGLE_TLV("HPMIXL IN4L Volume", WM8962_HEADPHONE_MIXER_3,
	       3, 7, 0, bypass_tlv),
SOC_SINGLE_TLV("HPMIXL IN4R Volume", WM8962_HEADPHONE_MIXER_3,
	       0, 7, 0, bypass_tlv),
SOC_SINGLE_TLV("HPMIXL MIXINL Volume", WM8962_HEADPHONE_MIXER_3,
	       7, 1, 1, inmix_tlv),
SOC_SINGLE_TLV("HPMIXL MIXINR Volume", WM8962_HEADPHONE_MIXER_3,
	       6, 1, 1, inmix_tlv),

SOC_SINGLE_TLV("HPMIXR IN4L Volume", WM8962_HEADPHONE_MIXER_4,
	       3, 7, 0, bypass_tlv),
SOC_SINGLE_TLV("HPMIXR IN4R Volume", WM8962_HEADPHONE_MIXER_4,
	       0, 7, 0, bypass_tlv),
SOC_SINGLE_TLV("HPMIXR MIXINL Volume", WM8962_HEADPHONE_MIXER_4,
	       7, 1, 1, inmix_tlv),
SOC_SINGLE_TLV("HPMIXR MIXINR Volume", WM8962_HEADPHONE_MIXER_4,
	       6, 1, 1, inmix_tlv),

SOC_SINGLE_TLV("Speaker Boost Volume", WM8962_CLASS_D_CONTROL_2, 0, 7, 0,
	       classd_tlv),

SOC_SINGLE("EQ Switch", WM8962_EQ1, WM8962_EQ_ENA_SHIFT, 1, 0),
SOC_DOUBLE_R_TLV("EQ1 Volume", WM8962_EQ2, WM8962_EQ22,
		 WM8962_EQL_B1_GAIN_SHIFT, 31, 0, eq_tlv),
SOC_DOUBLE_R_TLV("EQ2 Volume", WM8962_EQ2, WM8962_EQ22,
		 WM8962_EQL_B2_GAIN_SHIFT, 31, 0, eq_tlv),
SOC_DOUBLE_R_TLV("EQ3 Volume", WM8962_EQ2, WM8962_EQ22,
		 WM8962_EQL_B3_GAIN_SHIFT, 31, 0, eq_tlv),
SOC_DOUBLE_R_TLV("EQ4 Volume", WM8962_EQ3, WM8962_EQ23,
		 WM8962_EQL_B4_GAIN_SHIFT, 31, 0, eq_tlv),
SOC_DOUBLE_R_TLV("EQ5 Volume", WM8962_EQ3, WM8962_EQ23,
		 WM8962_EQL_B5_GAIN_SHIFT, 31, 0, eq_tlv),

WM8962_DSP2_ENABLE("VSS Switch", WM8962_VSS_ENA_SHIFT),
WM8962_DSP2_ENABLE("HPF1 Switch", WM8962_HPF1_ENA_SHIFT),
WM8962_DSP2_ENABLE("HPF2 Switch", WM8962_HPF2_ENA_SHIFT),
WM8962_DSP2_ENABLE("HD Bass Switch", WM8962_HDBASS_ENA_SHIFT),
};

static const struct snd_kcontrol_new wm8962_spk_mono_controls[] = {
SOC_SINGLE_TLV("Speaker Volume", WM8962_SPKOUTL_VOLUME, 0, 127, 0, out_tlv),
SOC_SINGLE_EXT("Speaker Switch", WM8962_CLASS_D_CONTROL_1, 1, 1, 1,
	       snd_soc_get_volsw, wm8962_put_spk_sw),
SOC_SINGLE("Speaker ZC Switch", WM8962_SPKOUTL_VOLUME, 7, 1, 0),

SOC_SINGLE("Speaker Mixer Switch", WM8962_SPEAKER_MIXER_3, 8, 1, 1),
SOC_SINGLE_TLV("Speaker Mixer IN4L Volume", WM8962_SPEAKER_MIXER_3,
	       3, 7, 0, bypass_tlv),
SOC_SINGLE_TLV("Speaker Mixer IN4R Volume", WM8962_SPEAKER_MIXER_3,
	       0, 7, 0, bypass_tlv),
SOC_SINGLE_TLV("Speaker Mixer MIXINL Volume", WM8962_SPEAKER_MIXER_3,
	       7, 1, 1, inmix_tlv),
SOC_SINGLE_TLV("Speaker Mixer MIXINR Volume", WM8962_SPEAKER_MIXER_3,
	       6, 1, 1, inmix_tlv),
SOC_SINGLE_TLV("Speaker Mixer DACL Volume", WM8962_SPEAKER_MIXER_5,
	       7, 1, 0, inmix_tlv),
SOC_SINGLE_TLV("Speaker Mixer DACR Volume", WM8962_SPEAKER_MIXER_5,
	       6, 1, 0, inmix_tlv),
};

static const struct snd_kcontrol_new wm8962_spk_stereo_controls[] = {
SOC_DOUBLE_R_TLV("Speaker Volume", WM8962_SPKOUTL_VOLUME,
		 WM8962_SPKOUTR_VOLUME, 0, 127, 0, out_tlv),
SOC_DOUBLE_EXT("Speaker Switch", WM8962_CLASS_D_CONTROL_1, 1, 0, 1, 1,
	       snd_soc_get_volsw, wm8962_put_spk_sw),
SOC_DOUBLE_R("Speaker ZC Switch", WM8962_SPKOUTL_VOLUME, WM8962_SPKOUTR_VOLUME,
	     7, 1, 0),

SOC_DOUBLE_R("Speaker Mixer Switch", WM8962_SPEAKER_MIXER_3,
	     WM8962_SPEAKER_MIXER_4, 8, 1, 1),

SOC_SINGLE_TLV("SPKOUTL Mixer IN4L Volume", WM8962_SPEAKER_MIXER_3,
	       3, 7, 0, bypass_tlv),
SOC_SINGLE_TLV("SPKOUTL Mixer IN4R Volume", WM8962_SPEAKER_MIXER_3,
	       0, 7, 0, bypass_tlv),
SOC_SINGLE_TLV("SPKOUTL Mixer MIXINL Volume", WM8962_SPEAKER_MIXER_3,
	       7, 1, 1, inmix_tlv),
SOC_SINGLE_TLV("SPKOUTL Mixer MIXINR Volume", WM8962_SPEAKER_MIXER_3,
	       6, 1, 1, inmix_tlv),
SOC_SINGLE_TLV("SPKOUTL Mixer DACL Volume", WM8962_SPEAKER_MIXER_5,
	       7, 1, 0, inmix_tlv),
SOC_SINGLE_TLV("SPKOUTL Mixer DACR Volume", WM8962_SPEAKER_MIXER_5,
	       6, 1, 0, inmix_tlv),

SOC_SINGLE_TLV("SPKOUTR Mixer IN4L Volume", WM8962_SPEAKER_MIXER_4,
	       3, 7, 0, bypass_tlv),
SOC_SINGLE_TLV("SPKOUTR Mixer IN4R Volume", WM8962_SPEAKER_MIXER_4,
	       0, 7, 0, bypass_tlv),
SOC_SINGLE_TLV("SPKOUTR Mixer MIXINL Volume", WM8962_SPEAKER_MIXER_4,
	       7, 1, 1, inmix_tlv),
SOC_SINGLE_TLV("SPKOUTR Mixer MIXINR Volume", WM8962_SPEAKER_MIXER_4,
	       6, 1, 1, inmix_tlv),
SOC_SINGLE_TLV("SPKOUTR Mixer DACL Volume", WM8962_SPEAKER_MIXER_5,
	       5, 1, 0, inmix_tlv),
SOC_SINGLE_TLV("SPKOUTR Mixer DACR Volume", WM8962_SPEAKER_MIXER_5,
	       4, 1, 0, inmix_tlv),
};

static int cp_event(struct snd_soc_dapm_widget *w,
		    struct snd_kcontrol *kcontrol, int event)
{
	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		msleep(5);
		break;

	default:
		BUG();
		return -EINVAL;
	}

	return 0;
}

static int hp_event(struct snd_soc_dapm_widget *w,
		    struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	int timeout;
	int reg;
	int expected = (WM8962_DCS_STARTUP_DONE_HP1L |
			WM8962_DCS_STARTUP_DONE_HP1R);

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		snd_soc_update_bits(codec, WM8962_ANALOGUE_HP_0,
				    WM8962_HP1L_ENA | WM8962_HP1R_ENA,
				    WM8962_HP1L_ENA | WM8962_HP1R_ENA);
		udelay(20);

		snd_soc_update_bits(codec, WM8962_ANALOGUE_HP_0,
				    WM8962_HP1L_ENA_DLY | WM8962_HP1R_ENA_DLY,
				    WM8962_HP1L_ENA_DLY | WM8962_HP1R_ENA_DLY);

		
		snd_soc_update_bits(codec, WM8962_DC_SERVO_1,
				    WM8962_HP1L_DCS_ENA | WM8962_HP1R_DCS_ENA |
				    WM8962_HP1L_DCS_STARTUP |
				    WM8962_HP1R_DCS_STARTUP,
				    WM8962_HP1L_DCS_ENA | WM8962_HP1R_DCS_ENA |
				    WM8962_HP1L_DCS_STARTUP |
				    WM8962_HP1R_DCS_STARTUP);

		
		timeout = 0;
		do {
			msleep(1);
			reg = snd_soc_read(codec, WM8962_DC_SERVO_6);
			if (reg < 0) {
				dev_err(codec->dev,
					"Failed to read DCS status: %d\n",
					reg);
				continue;
			}
			dev_dbg(codec->dev, "DCS status: %x\n", reg);
		} while (++timeout < 200 && (reg & expected) != expected);

		if ((reg & expected) != expected)
			dev_err(codec->dev, "DC servo timed out\n");
		else
			dev_dbg(codec->dev, "DC servo complete after %dms\n",
				timeout);

		snd_soc_update_bits(codec, WM8962_ANALOGUE_HP_0,
				    WM8962_HP1L_ENA_OUTP |
				    WM8962_HP1R_ENA_OUTP,
				    WM8962_HP1L_ENA_OUTP |
				    WM8962_HP1R_ENA_OUTP);
		udelay(20);

		snd_soc_update_bits(codec, WM8962_ANALOGUE_HP_0,
				    WM8962_HP1L_RMV_SHORT |
				    WM8962_HP1R_RMV_SHORT,
				    WM8962_HP1L_RMV_SHORT |
				    WM8962_HP1R_RMV_SHORT);
		break;

	case SND_SOC_DAPM_PRE_PMD:
		snd_soc_update_bits(codec, WM8962_ANALOGUE_HP_0,
				    WM8962_HP1L_RMV_SHORT |
				    WM8962_HP1R_RMV_SHORT, 0);

		udelay(20);

		snd_soc_update_bits(codec, WM8962_DC_SERVO_1,
				    WM8962_HP1L_DCS_ENA | WM8962_HP1R_DCS_ENA |
				    WM8962_HP1L_DCS_STARTUP |
				    WM8962_HP1R_DCS_STARTUP,
				    0);

		snd_soc_update_bits(codec, WM8962_ANALOGUE_HP_0,
				    WM8962_HP1L_ENA | WM8962_HP1R_ENA |
				    WM8962_HP1L_ENA_DLY | WM8962_HP1R_ENA_DLY |
				    WM8962_HP1L_ENA_OUTP |
				    WM8962_HP1R_ENA_OUTP, 0);
				    
		break;

	default:
		BUG();
		return -EINVAL;
	
	}

	return 0;
}

static int out_pga_event(struct snd_soc_dapm_widget *w,
			 struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	int reg;

	switch (w->shift) {
	case WM8962_HPOUTR_PGA_ENA_SHIFT:
		reg = WM8962_HPOUTR_VOLUME;
		break;
	case WM8962_HPOUTL_PGA_ENA_SHIFT:
		reg = WM8962_HPOUTL_VOLUME;
		break;
	case WM8962_SPKOUTR_PGA_ENA_SHIFT:
		reg = WM8962_SPKOUTR_VOLUME;
		break;
	case WM8962_SPKOUTL_PGA_ENA_SHIFT:
		reg = WM8962_SPKOUTL_VOLUME;
		break;
	default:
		BUG();
		return -EINVAL;
	}

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		return snd_soc_write(codec, reg, snd_soc_read(codec, reg));
	default:
		BUG();
		return -EINVAL;
	}
}

static int dsp2_event(struct snd_soc_dapm_widget *w,
		      struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	struct wm8962_priv *wm8962 = snd_soc_codec_get_drvdata(codec);

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		if (wm8962->dsp2_ena)
			wm8962_dsp2_start(codec);
		break;

	case SND_SOC_DAPM_PRE_PMD:
		if (wm8962->dsp2_ena)
			wm8962_dsp2_stop(codec);
		break;

	default:
		BUG();
		return -EINVAL;
	}

	return 0;
}

static const char *st_text[] = { "None", "Left", "Right" };

static const struct soc_enum str_enum =
	SOC_ENUM_SINGLE(WM8962_DAC_DSP_MIXING_1, 2, 3, st_text);

static const struct snd_kcontrol_new str_mux =
	SOC_DAPM_ENUM("Right Sidetone", str_enum);

static const struct soc_enum stl_enum =
	SOC_ENUM_SINGLE(WM8962_DAC_DSP_MIXING_2, 2, 3, st_text);

static const struct snd_kcontrol_new stl_mux =
	SOC_DAPM_ENUM("Left Sidetone", stl_enum);

static const char *outmux_text[] = { "DAC", "Mixer" };

static const struct soc_enum spkoutr_enum =
	SOC_ENUM_SINGLE(WM8962_SPEAKER_MIXER_2, 7, 2, outmux_text);

static const struct snd_kcontrol_new spkoutr_mux =
	SOC_DAPM_ENUM("SPKOUTR Mux", spkoutr_enum);

static const struct soc_enum spkoutl_enum =
	SOC_ENUM_SINGLE(WM8962_SPEAKER_MIXER_1, 7, 2, outmux_text);

static const struct snd_kcontrol_new spkoutl_mux =
	SOC_DAPM_ENUM("SPKOUTL Mux", spkoutl_enum);

static const struct soc_enum hpoutr_enum =
	SOC_ENUM_SINGLE(WM8962_HEADPHONE_MIXER_2, 7, 2, outmux_text);

static const struct snd_kcontrol_new hpoutr_mux =
	SOC_DAPM_ENUM("HPOUTR Mux", hpoutr_enum);

static const struct soc_enum hpoutl_enum =
	SOC_ENUM_SINGLE(WM8962_HEADPHONE_MIXER_1, 7, 2, outmux_text);

static const struct snd_kcontrol_new hpoutl_mux =
	SOC_DAPM_ENUM("HPOUTL Mux", hpoutl_enum);

static const struct snd_kcontrol_new inpgal[] = {
SOC_DAPM_SINGLE("IN1L Switch", WM8962_LEFT_INPUT_PGA_CONTROL, 3, 1, 0),
SOC_DAPM_SINGLE("IN2L Switch", WM8962_LEFT_INPUT_PGA_CONTROL, 2, 1, 0),
SOC_DAPM_SINGLE("IN3L Switch", WM8962_LEFT_INPUT_PGA_CONTROL, 1, 1, 0),
SOC_DAPM_SINGLE("IN4L Switch", WM8962_LEFT_INPUT_PGA_CONTROL, 0, 1, 0),
};

static const struct snd_kcontrol_new inpgar[] = {
SOC_DAPM_SINGLE("IN1R Switch", WM8962_RIGHT_INPUT_PGA_CONTROL, 3, 1, 0),
SOC_DAPM_SINGLE("IN2R Switch", WM8962_RIGHT_INPUT_PGA_CONTROL, 2, 1, 0),
SOC_DAPM_SINGLE("IN3R Switch", WM8962_RIGHT_INPUT_PGA_CONTROL, 1, 1, 0),
SOC_DAPM_SINGLE("IN4R Switch", WM8962_RIGHT_INPUT_PGA_CONTROL, 0, 1, 0),
};

static const struct snd_kcontrol_new mixinl[] = {
SOC_DAPM_SINGLE("IN2L Switch", WM8962_INPUT_MIXER_CONTROL_2, 5, 1, 0),
SOC_DAPM_SINGLE("IN3L Switch", WM8962_INPUT_MIXER_CONTROL_2, 4, 1, 0),
SOC_DAPM_SINGLE("PGA Switch", WM8962_INPUT_MIXER_CONTROL_2, 3, 1, 0),
};

static const struct snd_kcontrol_new mixinr[] = {
SOC_DAPM_SINGLE("IN2R Switch", WM8962_INPUT_MIXER_CONTROL_2, 2, 1, 0),
SOC_DAPM_SINGLE("IN3R Switch", WM8962_INPUT_MIXER_CONTROL_2, 1, 1, 0),
SOC_DAPM_SINGLE("PGA Switch", WM8962_INPUT_MIXER_CONTROL_2, 0, 1, 0),
};

static const struct snd_kcontrol_new hpmixl[] = {
SOC_DAPM_SINGLE("DACL Switch", WM8962_HEADPHONE_MIXER_1, 5, 1, 0),
SOC_DAPM_SINGLE("DACR Switch", WM8962_HEADPHONE_MIXER_1, 4, 1, 0),
SOC_DAPM_SINGLE("MIXINL Switch", WM8962_HEADPHONE_MIXER_1, 3, 1, 0),
SOC_DAPM_SINGLE("MIXINR Switch", WM8962_HEADPHONE_MIXER_1, 2, 1, 0),
SOC_DAPM_SINGLE("IN4L Switch", WM8962_HEADPHONE_MIXER_1, 1, 1, 0),
SOC_DAPM_SINGLE("IN4R Switch", WM8962_HEADPHONE_MIXER_1, 0, 1, 0),
};

static const struct snd_kcontrol_new hpmixr[] = {
SOC_DAPM_SINGLE("DACL Switch", WM8962_HEADPHONE_MIXER_2, 5, 1, 0),
SOC_DAPM_SINGLE("DACR Switch", WM8962_HEADPHONE_MIXER_2, 4, 1, 0),
SOC_DAPM_SINGLE("MIXINL Switch", WM8962_HEADPHONE_MIXER_2, 3, 1, 0),
SOC_DAPM_SINGLE("MIXINR Switch", WM8962_HEADPHONE_MIXER_2, 2, 1, 0),
SOC_DAPM_SINGLE("IN4L Switch", WM8962_HEADPHONE_MIXER_2, 1, 1, 0),
SOC_DAPM_SINGLE("IN4R Switch", WM8962_HEADPHONE_MIXER_2, 0, 1, 0),
};

static const struct snd_kcontrol_new spkmixl[] = {
SOC_DAPM_SINGLE("DACL Switch", WM8962_SPEAKER_MIXER_1, 5, 1, 0),
SOC_DAPM_SINGLE("DACR Switch", WM8962_SPEAKER_MIXER_1, 4, 1, 0),
SOC_DAPM_SINGLE("MIXINL Switch", WM8962_SPEAKER_MIXER_1, 3, 1, 0),
SOC_DAPM_SINGLE("MIXINR Switch", WM8962_SPEAKER_MIXER_1, 2, 1, 0),
SOC_DAPM_SINGLE("IN4L Switch", WM8962_SPEAKER_MIXER_1, 1, 1, 0),
SOC_DAPM_SINGLE("IN4R Switch", WM8962_SPEAKER_MIXER_1, 0, 1, 0),
};

static const struct snd_kcontrol_new spkmixr[] = {
SOC_DAPM_SINGLE("DACL Switch", WM8962_SPEAKER_MIXER_2, 5, 1, 0),
SOC_DAPM_SINGLE("DACR Switch", WM8962_SPEAKER_MIXER_2, 4, 1, 0),
SOC_DAPM_SINGLE("MIXINL Switch", WM8962_SPEAKER_MIXER_2, 3, 1, 0),
SOC_DAPM_SINGLE("MIXINR Switch", WM8962_SPEAKER_MIXER_2, 2, 1, 0),
SOC_DAPM_SINGLE("IN4L Switch", WM8962_SPEAKER_MIXER_2, 1, 1, 0),
SOC_DAPM_SINGLE("IN4R Switch", WM8962_SPEAKER_MIXER_2, 0, 1, 0),
};

static const struct snd_soc_dapm_widget wm8962_dapm_widgets[] = {
SND_SOC_DAPM_INPUT("IN1L"),
SND_SOC_DAPM_INPUT("IN1R"),
SND_SOC_DAPM_INPUT("IN2L"),
SND_SOC_DAPM_INPUT("IN2R"),
SND_SOC_DAPM_INPUT("IN3L"),
SND_SOC_DAPM_INPUT("IN3R"),
SND_SOC_DAPM_INPUT("IN4L"),
SND_SOC_DAPM_INPUT("IN4R"),
SND_SOC_DAPM_SIGGEN("Beep"),
SND_SOC_DAPM_INPUT("DMICDAT"),

SND_SOC_DAPM_SUPPLY("MICBIAS", WM8962_PWR_MGMT_1, 1, 0, NULL, 0),

SND_SOC_DAPM_SUPPLY("Class G", WM8962_CHARGE_PUMP_B, 0, 1, NULL, 0),
SND_SOC_DAPM_SUPPLY("SYSCLK", WM8962_CLOCKING2, 5, 0, NULL, 0),
SND_SOC_DAPM_SUPPLY("Charge Pump", WM8962_CHARGE_PUMP_1, 0, 0, cp_event,
		    SND_SOC_DAPM_POST_PMU),
SND_SOC_DAPM_SUPPLY("TOCLK", WM8962_ADDITIONAL_CONTROL_1, 0, 0, NULL, 0),
SND_SOC_DAPM_SUPPLY_S("DSP2", 1, WM8962_DSP2_POWER_MANAGEMENT,
		      WM8962_DSP2_ENA_SHIFT, 0, dsp2_event,
		      SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD),
SND_SOC_DAPM_SUPPLY("TEMP_HP", WM8962_ADDITIONAL_CONTROL_4, 2, 0, NULL, 0),
SND_SOC_DAPM_SUPPLY("TEMP_SPK", WM8962_ADDITIONAL_CONTROL_4, 1, 0, NULL, 0),

SND_SOC_DAPM_MIXER("INPGAL", WM8962_LEFT_INPUT_PGA_CONTROL, 4, 0,
		   inpgal, ARRAY_SIZE(inpgal)),
SND_SOC_DAPM_MIXER("INPGAR", WM8962_RIGHT_INPUT_PGA_CONTROL, 4, 0,
		   inpgar, ARRAY_SIZE(inpgar)),
SND_SOC_DAPM_MIXER("MIXINL", WM8962_PWR_MGMT_1, 5, 0,
		   mixinl, ARRAY_SIZE(mixinl)),
SND_SOC_DAPM_MIXER("MIXINR", WM8962_PWR_MGMT_1, 4, 0,
		   mixinr, ARRAY_SIZE(mixinr)),

SND_SOC_DAPM_AIF_IN("DMIC_ENA", NULL, 0, WM8962_PWR_MGMT_1, 10, 0),

SND_SOC_DAPM_ADC("ADCL", "Capture", WM8962_PWR_MGMT_1, 3, 0),
SND_SOC_DAPM_ADC("ADCR", "Capture", WM8962_PWR_MGMT_1, 2, 0),

SND_SOC_DAPM_MUX("STL", SND_SOC_NOPM, 0, 0, &stl_mux),
SND_SOC_DAPM_MUX("STR", SND_SOC_NOPM, 0, 0, &str_mux),

SND_SOC_DAPM_DAC("DACL", "Playback", WM8962_PWR_MGMT_2, 8, 0),
SND_SOC_DAPM_DAC("DACR", "Playback", WM8962_PWR_MGMT_2, 7, 0),

SND_SOC_DAPM_PGA("Left Bypass", SND_SOC_NOPM, 0, 0, NULL, 0),
SND_SOC_DAPM_PGA("Right Bypass", SND_SOC_NOPM, 0, 0, NULL, 0),

SND_SOC_DAPM_MIXER("HPMIXL", WM8962_MIXER_ENABLES, 3, 0,
		   hpmixl, ARRAY_SIZE(hpmixl)),
SND_SOC_DAPM_MIXER("HPMIXR", WM8962_MIXER_ENABLES, 2, 0,
		   hpmixr, ARRAY_SIZE(hpmixr)),

SND_SOC_DAPM_MUX_E("HPOUTL PGA", WM8962_PWR_MGMT_2, 6, 0, &hpoutl_mux,
		   out_pga_event, SND_SOC_DAPM_POST_PMU),
SND_SOC_DAPM_MUX_E("HPOUTR PGA", WM8962_PWR_MGMT_2, 5, 0, &hpoutr_mux,
		   out_pga_event, SND_SOC_DAPM_POST_PMU),

SND_SOC_DAPM_PGA_E("HPOUT", SND_SOC_NOPM, 0, 0, NULL, 0, hp_event,
		   SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD),

SND_SOC_DAPM_OUTPUT("HPOUTL"),
SND_SOC_DAPM_OUTPUT("HPOUTR"),
};

static const struct snd_soc_dapm_widget wm8962_dapm_spk_mono_widgets[] = {
SND_SOC_DAPM_MIXER("Speaker Mixer", WM8962_MIXER_ENABLES, 1, 0,
		   spkmixl, ARRAY_SIZE(spkmixl)),
SND_SOC_DAPM_MUX_E("Speaker PGA", WM8962_PWR_MGMT_2, 4, 0, &spkoutl_mux,
		   out_pga_event, SND_SOC_DAPM_POST_PMU),
SND_SOC_DAPM_PGA("Speaker Output", WM8962_CLASS_D_CONTROL_1, 7, 0, NULL, 0),
SND_SOC_DAPM_OUTPUT("SPKOUT"),
};

static const struct snd_soc_dapm_widget wm8962_dapm_spk_stereo_widgets[] = {
SND_SOC_DAPM_MIXER("SPKOUTL Mixer", WM8962_MIXER_ENABLES, 1, 0,
		   spkmixl, ARRAY_SIZE(spkmixl)),
SND_SOC_DAPM_MIXER("SPKOUTR Mixer", WM8962_MIXER_ENABLES, 0, 0,
		   spkmixr, ARRAY_SIZE(spkmixr)),

SND_SOC_DAPM_MUX_E("SPKOUTL PGA", WM8962_PWR_MGMT_2, 4, 0, &spkoutl_mux,
		   out_pga_event, SND_SOC_DAPM_POST_PMU),
SND_SOC_DAPM_MUX_E("SPKOUTR PGA", WM8962_PWR_MGMT_2, 3, 0, &spkoutr_mux,
		   out_pga_event, SND_SOC_DAPM_POST_PMU),

SND_SOC_DAPM_PGA("SPKOUTR Output", WM8962_CLASS_D_CONTROL_1, 7, 0, NULL, 0),
SND_SOC_DAPM_PGA("SPKOUTL Output", WM8962_CLASS_D_CONTROL_1, 6, 0, NULL, 0),

SND_SOC_DAPM_OUTPUT("SPKOUTL"),
SND_SOC_DAPM_OUTPUT("SPKOUTR"),
};

static const struct snd_soc_dapm_route wm8962_intercon[] = {
	{ "INPGAL", "IN1L Switch", "IN1L" },
	{ "INPGAL", "IN2L Switch", "IN2L" },
	{ "INPGAL", "IN3L Switch", "IN3L" },
	{ "INPGAL", "IN4L Switch", "IN4L" },

	{ "INPGAR", "IN1R Switch", "IN1R" },
	{ "INPGAR", "IN2R Switch", "IN2R" },
	{ "INPGAR", "IN3R Switch", "IN3R" },
	{ "INPGAR", "IN4R Switch", "IN4R" },

	{ "MIXINL", "IN2L Switch", "IN2L" },
	{ "MIXINL", "IN3L Switch", "IN3L" },
	{ "MIXINL", "PGA Switch", "INPGAL" },

	{ "MIXINR", "IN2R Switch", "IN2R" },
	{ "MIXINR", "IN3R Switch", "IN3R" },
	{ "MIXINR", "PGA Switch", "INPGAR" },

	{ "MICBIAS", NULL, "SYSCLK" },

	{ "DMIC_ENA", NULL, "DMICDAT" },

	{ "ADCL", NULL, "SYSCLK" },
	{ "ADCL", NULL, "TOCLK" },
	{ "ADCL", NULL, "MIXINL" },
	{ "ADCL", NULL, "DMIC_ENA" },
	{ "ADCL", NULL, "DSP2" },

	{ "ADCR", NULL, "SYSCLK" },
	{ "ADCR", NULL, "TOCLK" },
	{ "ADCR", NULL, "MIXINR" },
	{ "ADCR", NULL, "DMIC_ENA" },
	{ "ADCR", NULL, "DSP2" },

	{ "STL", "Left", "ADCL" },
	{ "STL", "Right", "ADCR" },
	{ "STL", NULL, "Class G" },

	{ "STR", "Left", "ADCL" },
	{ "STR", "Right", "ADCR" },
	{ "STR", NULL, "Class G" },

	{ "DACL", NULL, "SYSCLK" },
	{ "DACL", NULL, "TOCLK" },
	{ "DACL", NULL, "Beep" },
	{ "DACL", NULL, "STL" },
	{ "DACL", NULL, "DSP2" },

	{ "DACR", NULL, "SYSCLK" },
	{ "DACR", NULL, "TOCLK" },
	{ "DACR", NULL, "Beep" },
	{ "DACR", NULL, "STR" },
	{ "DACR", NULL, "DSP2" },

	{ "HPMIXL", "IN4L Switch", "IN4L" },
	{ "HPMIXL", "IN4R Switch", "IN4R" },
	{ "HPMIXL", "DACL Switch", "DACL" },
	{ "HPMIXL", "DACR Switch", "DACR" },
	{ "HPMIXL", "MIXINL Switch", "MIXINL" },
	{ "HPMIXL", "MIXINR Switch", "MIXINR" },

	{ "HPMIXR", "IN4L Switch", "IN4L" },
	{ "HPMIXR", "IN4R Switch", "IN4R" },
	{ "HPMIXR", "DACL Switch", "DACL" },
	{ "HPMIXR", "DACR Switch", "DACR" },
	{ "HPMIXR", "MIXINL Switch", "MIXINL" },
	{ "HPMIXR", "MIXINR Switch", "MIXINR" },

	{ "Left Bypass", NULL, "HPMIXL" },
	{ "Left Bypass", NULL, "Class G" },

	{ "Right Bypass", NULL, "HPMIXR" },
	{ "Right Bypass", NULL, "Class G" },

	{ "HPOUTL PGA", "Mixer", "Left Bypass" },
	{ "HPOUTL PGA", "DAC", "DACL" },

	{ "HPOUTR PGA", "Mixer", "Right Bypass" },
	{ "HPOUTR PGA", "DAC", "DACR" },

	{ "HPOUT", NULL, "HPOUTL PGA" },
	{ "HPOUT", NULL, "HPOUTR PGA" },
	{ "HPOUT", NULL, "Charge Pump" },
	{ "HPOUT", NULL, "SYSCLK" },
	{ "HPOUT", NULL, "TOCLK" },

	{ "HPOUTL", NULL, "HPOUT" },
	{ "HPOUTR", NULL, "HPOUT" },

	{ "HPOUTL", NULL, "TEMP_HP" },
	{ "HPOUTR", NULL, "TEMP_HP" },
};

static const struct snd_soc_dapm_route wm8962_spk_mono_intercon[] = {
	{ "Speaker Mixer", "IN4L Switch", "IN4L" },
	{ "Speaker Mixer", "IN4R Switch", "IN4R" },
	{ "Speaker Mixer", "DACL Switch", "DACL" },
	{ "Speaker Mixer", "DACR Switch", "DACR" },
	{ "Speaker Mixer", "MIXINL Switch", "MIXINL" },
	{ "Speaker Mixer", "MIXINR Switch", "MIXINR" },

	{ "Speaker PGA", "Mixer", "Speaker Mixer" },
	{ "Speaker PGA", "DAC", "DACL" },

	{ "Speaker Output", NULL, "Speaker PGA" },
	{ "Speaker Output", NULL, "SYSCLK" },
	{ "Speaker Output", NULL, "TOCLK" },
	{ "Speaker Output", NULL, "TEMP_SPK" },

	{ "SPKOUT", NULL, "Speaker Output" },
};

static const struct snd_soc_dapm_route wm8962_spk_stereo_intercon[] = {
	{ "SPKOUTL Mixer", "IN4L Switch", "IN4L" },
	{ "SPKOUTL Mixer", "IN4R Switch", "IN4R" },
	{ "SPKOUTL Mixer", "DACL Switch", "DACL" },
	{ "SPKOUTL Mixer", "DACR Switch", "DACR" },
	{ "SPKOUTL Mixer", "MIXINL Switch", "MIXINL" },
	{ "SPKOUTL Mixer", "MIXINR Switch", "MIXINR" },

	{ "SPKOUTR Mixer", "IN4L Switch", "IN4L" },
	{ "SPKOUTR Mixer", "IN4R Switch", "IN4R" },
	{ "SPKOUTR Mixer", "DACL Switch", "DACL" },
	{ "SPKOUTR Mixer", "DACR Switch", "DACR" },
	{ "SPKOUTR Mixer", "MIXINL Switch", "MIXINL" },
	{ "SPKOUTR Mixer", "MIXINR Switch", "MIXINR" },

	{ "SPKOUTL PGA", "Mixer", "SPKOUTL Mixer" },
	{ "SPKOUTL PGA", "DAC", "DACL" },

	{ "SPKOUTR PGA", "Mixer", "SPKOUTR Mixer" },
	{ "SPKOUTR PGA", "DAC", "DACR" },

	{ "SPKOUTL Output", NULL, "SPKOUTL PGA" },
	{ "SPKOUTL Output", NULL, "SYSCLK" },
	{ "SPKOUTL Output", NULL, "TOCLK" },
	{ "SPKOUTL Output", NULL, "TEMP_SPK" },

	{ "SPKOUTR Output", NULL, "SPKOUTR PGA" },
	{ "SPKOUTR Output", NULL, "SYSCLK" },
	{ "SPKOUTR Output", NULL, "TOCLK" },
	{ "SPKOUTR Output", NULL, "TEMP_SPK" },

	{ "SPKOUTL", NULL, "SPKOUTL Output" },
	{ "SPKOUTR", NULL, "SPKOUTR Output" },
};

static int wm8962_add_widgets(struct snd_soc_codec *codec)
{
	struct wm8962_pdata *pdata = dev_get_platdata(codec->dev);
	struct snd_soc_dapm_context *dapm = &codec->dapm;

	snd_soc_add_codec_controls(codec, wm8962_snd_controls,
			     ARRAY_SIZE(wm8962_snd_controls));
	if (pdata && pdata->spk_mono)
		snd_soc_add_codec_controls(codec, wm8962_spk_mono_controls,
				     ARRAY_SIZE(wm8962_spk_mono_controls));
	else
		snd_soc_add_codec_controls(codec, wm8962_spk_stereo_controls,
				     ARRAY_SIZE(wm8962_spk_stereo_controls));


	snd_soc_dapm_new_controls(dapm, wm8962_dapm_widgets,
				  ARRAY_SIZE(wm8962_dapm_widgets));
	if (pdata && pdata->spk_mono)
		snd_soc_dapm_new_controls(dapm, wm8962_dapm_spk_mono_widgets,
					  ARRAY_SIZE(wm8962_dapm_spk_mono_widgets));
	else
		snd_soc_dapm_new_controls(dapm, wm8962_dapm_spk_stereo_widgets,
					  ARRAY_SIZE(wm8962_dapm_spk_stereo_widgets));

	snd_soc_dapm_add_routes(dapm, wm8962_intercon,
				ARRAY_SIZE(wm8962_intercon));
	if (pdata && pdata->spk_mono)
		snd_soc_dapm_add_routes(dapm, wm8962_spk_mono_intercon,
					ARRAY_SIZE(wm8962_spk_mono_intercon));
	else
		snd_soc_dapm_add_routes(dapm, wm8962_spk_stereo_intercon,
					ARRAY_SIZE(wm8962_spk_stereo_intercon));


	snd_soc_dapm_disable_pin(dapm, "Beep");

	return 0;
}

static const int bclk_divs[] = {
	1, -1, 2, 3, 4, -1, 6, 8, -1, 12, 16, 24, -1, 32, 32, 32
};

static const int sysclk_rates[] = {
	64, 128, 192, 256, 384, 512, 768, 1024, 1408, 1536, 3072, 6144
};

static void wm8962_configure_bclk(struct snd_soc_codec *codec)
{
	struct wm8962_priv *wm8962 = snd_soc_codec_get_drvdata(codec);
	int dspclk, i;
	int clocking2 = 0;
	int clocking4 = 0;
	int aif2 = 0;

	if (!wm8962->sysclk_rate) {
		dev_dbg(codec->dev, "No SYSCLK configured\n");
		return;
	}

	if (!wm8962->bclk || !wm8962->lrclk) {
		dev_dbg(codec->dev, "No audio clocks configured\n");
		return;
	}

	for (i = 0; i < ARRAY_SIZE(sysclk_rates); i++) {
		if (sysclk_rates[i] == wm8962->sysclk_rate / wm8962->lrclk) {
			clocking4 |= i << WM8962_SYSCLK_RATE_SHIFT;
			break;
		}
	}

	if (i == ARRAY_SIZE(sysclk_rates)) {
		dev_err(codec->dev, "Unsupported sysclk ratio %d\n",
			wm8962->sysclk_rate / wm8962->lrclk);
		return;
	}

	dev_dbg(codec->dev, "Selected sysclk ratio %d\n", sysclk_rates[i]);

	snd_soc_update_bits(codec, WM8962_CLOCKING_4,
			    WM8962_SYSCLK_RATE_MASK, clocking4);

	dspclk = snd_soc_read(codec, WM8962_CLOCKING1);
	if (dspclk < 0) {
		dev_err(codec->dev, "Failed to read DSPCLK: %d\n", dspclk);
		return;
	}

	dspclk = (dspclk & WM8962_DSPCLK_DIV_MASK) >> WM8962_DSPCLK_DIV_SHIFT;
	switch (dspclk) {
	case 0:
		dspclk = wm8962->sysclk_rate;
		break;
	case 1:
		dspclk = wm8962->sysclk_rate / 2;
		break;
	case 2:
		dspclk = wm8962->sysclk_rate / 4;
		break;
	default:
		dev_warn(codec->dev, "Unknown DSPCLK divisor read back\n");
		dspclk = wm8962->sysclk;
	}

	dev_dbg(codec->dev, "DSPCLK is %dHz, BCLK %d\n", dspclk, wm8962->bclk);

	
	for (i = 0; i < ARRAY_SIZE(bclk_divs); i++) {
		if (bclk_divs[i] < 0)
			continue;

		if (dspclk / bclk_divs[i] == wm8962->bclk) {
			dev_dbg(codec->dev, "Selected BCLK_DIV %d for %dHz\n",
				bclk_divs[i], wm8962->bclk);
			clocking2 |= i;
			break;
		}
	}
	if (i == ARRAY_SIZE(bclk_divs)) {
		dev_err(codec->dev, "Unsupported BCLK ratio %d\n",
			dspclk / wm8962->bclk);
		return;
	}

	aif2 |= wm8962->bclk / wm8962->lrclk;
	dev_dbg(codec->dev, "Selected LRCLK divisor %d for %dHz\n",
		wm8962->bclk / wm8962->lrclk, wm8962->lrclk);

	snd_soc_update_bits(codec, WM8962_CLOCKING2,
			    WM8962_BCLK_DIV_MASK, clocking2);
	snd_soc_update_bits(codec, WM8962_AUDIO_INTERFACE_2,
			    WM8962_AIF_RATE_MASK, aif2);
}

static int wm8962_set_bias_level(struct snd_soc_codec *codec,
				 enum snd_soc_bias_level level)
{
	if (level == codec->dapm.bias_level)
		return 0;

	switch (level) {
	case SND_SOC_BIAS_ON:
		break;

	case SND_SOC_BIAS_PREPARE:
		
		snd_soc_update_bits(codec, WM8962_PWR_MGMT_1,
				    WM8962_VMID_SEL_MASK, 0x80);

		wm8962_configure_bclk(codec);
		break;

	case SND_SOC_BIAS_STANDBY:
		
		snd_soc_update_bits(codec, WM8962_PWR_MGMT_1,
				    WM8962_VMID_SEL_MASK, 0x100);
		break;

	case SND_SOC_BIAS_OFF:
		break;
	}

	codec->dapm.bias_level = level;
	return 0;
}

static const struct {
	int rate;
	int reg;
} sr_vals[] = {
	{ 48000, 0 },
	{ 44100, 0 },
	{ 32000, 1 },
	{ 22050, 2 },
	{ 24000, 2 },
	{ 16000, 3 },
	{ 11025, 4 },
	{ 12000, 4 },
	{ 8000,  5 },
	{ 88200, 6 },
	{ 96000, 6 },
};

static int wm8962_hw_params(struct snd_pcm_substream *substream,
			    struct snd_pcm_hw_params *params,
			    struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_codec *codec = rtd->codec;
	struct wm8962_priv *wm8962 = snd_soc_codec_get_drvdata(codec);
	int i;
	int aif0 = 0;
	int adctl3 = 0;

	wm8962->bclk = snd_soc_params_to_bclk(params);
	if (params_channels(params) == 1)
		wm8962->bclk *= 2;

	wm8962->lrclk = params_rate(params);

	for (i = 0; i < ARRAY_SIZE(sr_vals); i++) {
		if (sr_vals[i].rate == wm8962->lrclk) {
			adctl3 |= sr_vals[i].reg;
			break;
		}
	}
	if (i == ARRAY_SIZE(sr_vals)) {
		dev_err(codec->dev, "Unsupported rate %dHz\n", wm8962->lrclk);
		return -EINVAL;
	}

	if (wm8962->lrclk % 8000 == 0)
		adctl3 |= WM8962_SAMPLE_RATE_INT_MODE;

	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		break;
	case SNDRV_PCM_FORMAT_S20_3LE:
		aif0 |= 0x4;
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
		aif0 |= 0x8;
		break;
	case SNDRV_PCM_FORMAT_S32_LE:
		aif0 |= 0xc;
		break;
	default:
		return -EINVAL;
	}

	snd_soc_update_bits(codec, WM8962_AUDIO_INTERFACE_0,
			    WM8962_WL_MASK, aif0);
	snd_soc_update_bits(codec, WM8962_ADDITIONAL_CONTROL_3,
			    WM8962_SAMPLE_RATE_INT_MODE |
			    WM8962_SAMPLE_RATE_MASK, adctl3);

	if (codec->dapm.bias_level == SND_SOC_BIAS_ON)
		wm8962_configure_bclk(codec);

	return 0;
}

static int wm8962_set_dai_sysclk(struct snd_soc_dai *dai, int clk_id,
				 unsigned int freq, int dir)
{
	struct snd_soc_codec *codec = dai->codec;
	struct wm8962_priv *wm8962 = snd_soc_codec_get_drvdata(codec);
	int src;

	switch (clk_id) {
	case WM8962_SYSCLK_MCLK:
		wm8962->sysclk = WM8962_SYSCLK_MCLK;
		src = 0;
		break;
	case WM8962_SYSCLK_FLL:
		wm8962->sysclk = WM8962_SYSCLK_FLL;
		src = 1 << WM8962_SYSCLK_SRC_SHIFT;
		break;
	default:
		return -EINVAL;
	}

	snd_soc_update_bits(codec, WM8962_CLOCKING2, WM8962_SYSCLK_SRC_MASK,
			    src);

	wm8962->sysclk_rate = freq;

	wm8962_configure_bclk(codec);

	return 0;
}

static int wm8962_set_dai_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
	struct snd_soc_codec *codec = dai->codec;
	int aif0 = 0;

	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_DSP_B:
		aif0 |= WM8962_LRCLK_INV | 3;
	case SND_SOC_DAIFMT_DSP_A:
		aif0 |= 3;

		switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
		case SND_SOC_DAIFMT_NB_NF:
		case SND_SOC_DAIFMT_IB_NF:
			break;
		default:
			return -EINVAL;
		}
		break;

	case SND_SOC_DAIFMT_RIGHT_J:
		break;
	case SND_SOC_DAIFMT_LEFT_J:
		aif0 |= 1;
		break;
	case SND_SOC_DAIFMT_I2S:
		aif0 |= 2;
		break;
	default:
		return -EINVAL;
	}

	switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
	case SND_SOC_DAIFMT_NB_NF:
		break;
	case SND_SOC_DAIFMT_IB_NF:
		aif0 |= WM8962_BCLK_INV;
		break;
	case SND_SOC_DAIFMT_NB_IF:
		aif0 |= WM8962_LRCLK_INV;
		break;
	case SND_SOC_DAIFMT_IB_IF:
		aif0 |= WM8962_BCLK_INV | WM8962_LRCLK_INV;
		break;
	default:
		return -EINVAL;
	}

	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBM_CFM:
		aif0 |= WM8962_MSTR;
		break;
	case SND_SOC_DAIFMT_CBS_CFS:
		break;
	default:
		return -EINVAL;
	}

	snd_soc_update_bits(codec, WM8962_AUDIO_INTERFACE_0,
			    WM8962_FMT_MASK | WM8962_BCLK_INV | WM8962_MSTR |
			    WM8962_LRCLK_INV, aif0);

	return 0;
}

struct _fll_div {
	u16 fll_fratio;
	u16 fll_outdiv;
	u16 fll_refclk_div;
	u16 n;
	u16 theta;
	u16 lambda;
};

#define FIXED_FLL_SIZE ((1 << 16) * 10)

static struct {
	unsigned int min;
	unsigned int max;
	u16 fll_fratio;
	int ratio;
} fll_fratios[] = {
	{       0,    64000, 4, 16 },
	{   64000,   128000, 3,  8 },
	{  128000,   256000, 2,  4 },
	{  256000,  1000000, 1,  2 },
	{ 1000000, 13500000, 0,  1 },
};

static int fll_factors(struct _fll_div *fll_div, unsigned int Fref,
		       unsigned int Fout)
{
	unsigned int target;
	unsigned int div;
	unsigned int fratio, gcd_fll;
	int i;

	
	div = 1;
	fll_div->fll_refclk_div = 0;
	while ((Fref / div) > 13500000) {
		div *= 2;
		fll_div->fll_refclk_div++;

		if (div > 4) {
			pr_err("Can't scale %dMHz input down to <=13.5MHz\n",
			       Fref);
			return -EINVAL;
		}
	}

	pr_debug("FLL Fref=%u Fout=%u\n", Fref, Fout);

	
	Fref /= div;

	
	div = 2;
	while (Fout * div < 90000000) {
		div++;
		if (div > 64) {
			pr_err("Unable to find FLL_OUTDIV for Fout=%uHz\n",
			       Fout);
			return -EINVAL;
		}
	}
	target = Fout * div;
	fll_div->fll_outdiv = div - 1;

	pr_debug("FLL Fvco=%dHz\n", target);

	
	for (i = 0; i < ARRAY_SIZE(fll_fratios); i++) {
		if (fll_fratios[i].min <= Fref && Fref <= fll_fratios[i].max) {
			fll_div->fll_fratio = fll_fratios[i].fll_fratio;
			fratio = fll_fratios[i].ratio;
			break;
		}
	}
	if (i == ARRAY_SIZE(fll_fratios)) {
		pr_err("Unable to find FLL_FRATIO for Fref=%uHz\n", Fref);
		return -EINVAL;
	}

	fll_div->n = target / (fratio * Fref);

	if (target % Fref == 0) {
		fll_div->theta = 0;
		fll_div->lambda = 0;
	} else {
		gcd_fll = gcd(target, fratio * Fref);

		fll_div->theta = (target - (fll_div->n * fratio * Fref))
			/ gcd_fll;
		fll_div->lambda = (fratio * Fref) / gcd_fll;
	}

	pr_debug("FLL N=%x THETA=%x LAMBDA=%x\n",
		 fll_div->n, fll_div->theta, fll_div->lambda);
	pr_debug("FLL_FRATIO=%x FLL_OUTDIV=%x FLL_REFCLK_DIV=%x\n",
		 fll_div->fll_fratio, fll_div->fll_outdiv,
		 fll_div->fll_refclk_div);

	return 0;
}

static int wm8962_set_fll(struct snd_soc_codec *codec, int fll_id, int source,
			  unsigned int Fref, unsigned int Fout)
{
	struct wm8962_priv *wm8962 = snd_soc_codec_get_drvdata(codec);
	struct _fll_div fll_div;
	unsigned long timeout;
	int ret;
	int fll1 = 0;

	
	if (source == wm8962->fll_src && Fref == wm8962->fll_fref &&
	    Fout == wm8962->fll_fout)
		return 0;

	if (Fout == 0) {
		dev_dbg(codec->dev, "FLL disabled\n");

		wm8962->fll_fref = 0;
		wm8962->fll_fout = 0;

		snd_soc_update_bits(codec, WM8962_FLL_CONTROL_1,
				    WM8962_FLL_ENA, 0);

		pm_runtime_put(codec->dev);

		return 0;
	}

	ret = fll_factors(&fll_div, Fref, Fout);
	if (ret != 0)
		return ret;

	
	snd_soc_update_bits(codec, WM8962_FLL_CONTROL_1, WM8962_FLL_ENA, 0);

	switch (fll_id) {
	case WM8962_FLL_MCLK:
	case WM8962_FLL_BCLK:
	case WM8962_FLL_OSC:
		fll1 |= (fll_id - 1) << WM8962_FLL_REFCLK_SRC_SHIFT;
		break;
	case WM8962_FLL_INT:
		snd_soc_update_bits(codec, WM8962_FLL_CONTROL_1,
				    WM8962_FLL_OSC_ENA, WM8962_FLL_OSC_ENA);
		snd_soc_update_bits(codec, WM8962_FLL_CONTROL_5,
				    WM8962_FLL_FRC_NCO, WM8962_FLL_FRC_NCO);
		break;
	default:
		dev_err(codec->dev, "Unknown FLL source %d\n", ret);
		return -EINVAL;
	}

	if (fll_div.theta || fll_div.lambda)
		fll1 |= WM8962_FLL_FRAC;

	
	snd_soc_update_bits(codec, WM8962_FLL_CONTROL_1, WM8962_FLL_ENA, 0);

	snd_soc_update_bits(codec, WM8962_FLL_CONTROL_2,
			    WM8962_FLL_OUTDIV_MASK |
			    WM8962_FLL_REFCLK_DIV_MASK,
			    (fll_div.fll_outdiv << WM8962_FLL_OUTDIV_SHIFT) |
			    (fll_div.fll_refclk_div));

	snd_soc_update_bits(codec, WM8962_FLL_CONTROL_3,
			    WM8962_FLL_FRATIO_MASK, fll_div.fll_fratio);

	snd_soc_write(codec, WM8962_FLL_CONTROL_6, fll_div.theta);
	snd_soc_write(codec, WM8962_FLL_CONTROL_7, fll_div.lambda);
	snd_soc_write(codec, WM8962_FLL_CONTROL_8, fll_div.n);

	try_wait_for_completion(&wm8962->fll_lock);

	pm_runtime_get_sync(codec->dev);

	snd_soc_update_bits(codec, WM8962_FLL_CONTROL_1,
			    WM8962_FLL_FRAC | WM8962_FLL_REFCLK_SRC_MASK |
			    WM8962_FLL_ENA, fll1 | WM8962_FLL_ENA);

	dev_dbg(codec->dev, "FLL configured for %dHz->%dHz\n", Fref, Fout);

	ret = 0;

	if (fll1 & WM8962_FLL_ENA) {
		if (wm8962->irq)
			timeout = msecs_to_jiffies(5);
		else
			timeout = msecs_to_jiffies(1);

		timeout = wait_for_completion_timeout(&wm8962->fll_lock,
						      timeout);

		if (timeout == 0 && wm8962->irq) {
			dev_err(codec->dev, "FLL lock timed out");
			ret = -ETIMEDOUT;
		}
	}

	wm8962->fll_fref = Fref;
	wm8962->fll_fout = Fout;
	wm8962->fll_src = source;

	return ret;
}

static int wm8962_mute(struct snd_soc_dai *dai, int mute)
{
	struct snd_soc_codec *codec = dai->codec;
	int val;

	if (mute)
		val = WM8962_DAC_MUTE;
	else
		val = 0;

	return snd_soc_update_bits(codec, WM8962_ADC_DAC_CONTROL_1,
				   WM8962_DAC_MUTE, val);
}

#define WM8962_RATES SNDRV_PCM_RATE_8000_96000

#define WM8962_FORMATS (SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S20_3LE |\
			SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S32_LE)

static const struct snd_soc_dai_ops wm8962_dai_ops = {
	.hw_params = wm8962_hw_params,
	.set_sysclk = wm8962_set_dai_sysclk,
	.set_fmt = wm8962_set_dai_fmt,
	.digital_mute = wm8962_mute,
};

static struct snd_soc_dai_driver wm8962_dai = {
	.name = "wm8962",
	.playback = {
		.stream_name = "Playback",
		.channels_min = 1,
		.channels_max = 2,
		.rates = WM8962_RATES,
		.formats = WM8962_FORMATS,
	},
	.capture = {
		.stream_name = "Capture",
		.channels_min = 1,
		.channels_max = 2,
		.rates = WM8962_RATES,
		.formats = WM8962_FORMATS,
	},
	.ops = &wm8962_dai_ops,
	.symmetric_rates = 1,
};

static void wm8962_mic_work(struct work_struct *work)
{
	struct wm8962_priv *wm8962 = container_of(work,
						  struct wm8962_priv,
						  mic_work.work);
	struct snd_soc_codec *codec = wm8962->codec;
	int status = 0;
	int irq_pol = 0;
	int reg;

	reg = snd_soc_read(codec, WM8962_ADDITIONAL_CONTROL_4);

	if (reg & WM8962_MICDET_STS) {
		status |= SND_JACK_MICROPHONE;
		irq_pol |= WM8962_MICD_IRQ_POL;
	}

	if (reg & WM8962_MICSHORT_STS) {
		status |= SND_JACK_BTN_0;
		irq_pol |= WM8962_MICSCD_IRQ_POL;
	}

	snd_soc_jack_report(wm8962->jack, status,
			    SND_JACK_MICROPHONE | SND_JACK_BTN_0);

	snd_soc_update_bits(codec, WM8962_MICINT_SOURCE_POL,
			    WM8962_MICSCD_IRQ_POL |
			    WM8962_MICD_IRQ_POL, irq_pol);
}

static irqreturn_t wm8962_irq(int irq, void *data)
{
	struct device *dev = data;
	struct wm8962_priv *wm8962 = dev_get_drvdata(dev);
	unsigned int mask;
	unsigned int active;
	int reg, ret;

	ret = regmap_read(wm8962->regmap, WM8962_INTERRUPT_STATUS_2_MASK,
			  &mask);
	if (ret != 0) {
		dev_err(dev, "Failed to read interrupt mask: %d\n",
			ret);
		return IRQ_NONE;
	}

	ret = regmap_read(wm8962->regmap, WM8962_INTERRUPT_STATUS_2, &active);
	if (ret != 0) {
		dev_err(dev, "Failed to read interrupt: %d\n", ret);
		return IRQ_NONE;
	}

	active &= ~mask;

	if (!active)
		return IRQ_NONE;

	
	ret = regmap_write(wm8962->regmap, WM8962_INTERRUPT_STATUS_2, active);
	if (ret != 0)
		dev_warn(dev, "Failed to ack interrupt: %d\n", ret);

	if (active & WM8962_FLL_LOCK_EINT) {
		dev_dbg(dev, "FLL locked\n");
		complete(&wm8962->fll_lock);
	}

	if (active & WM8962_FIFOS_ERR_EINT)
		dev_err(dev, "FIFO error\n");

	if (active & WM8962_TEMP_SHUT_EINT) {
		dev_crit(dev, "Thermal shutdown\n");

		ret = regmap_read(wm8962->regmap,
				  WM8962_THERMAL_SHUTDOWN_STATUS,  &reg);
		if (ret != 0) {
			dev_warn(dev, "Failed to read thermal status: %d\n",
				 ret);
			reg = 0;
		}

		if (reg & WM8962_TEMP_ERR_HP)
			dev_crit(dev, "Headphone thermal error\n");
		if (reg & WM8962_TEMP_WARN_HP)
			dev_crit(dev, "Headphone thermal warning\n");
		if (reg & WM8962_TEMP_ERR_SPK)
			dev_crit(dev, "Speaker thermal error\n");
		if (reg & WM8962_TEMP_WARN_SPK)
			dev_crit(dev, "Speaker thermal warning\n");
	}

	if (active & (WM8962_MICSCD_EINT | WM8962_MICD_EINT)) {
		dev_dbg(dev, "Microphone event detected\n");

#ifndef CONFIG_SND_SOC_WM8962_MODULE
		trace_snd_soc_jack_irq(dev_name(dev));
#endif

		pm_wakeup_event(dev, 300);

		schedule_delayed_work(&wm8962->mic_work,
				      msecs_to_jiffies(250));
	}

	return IRQ_HANDLED;
}

int wm8962_mic_detect(struct snd_soc_codec *codec, struct snd_soc_jack *jack)
{
	struct wm8962_priv *wm8962 = snd_soc_codec_get_drvdata(codec);
	int irq_mask, enable;

	wm8962->jack = jack;
	if (jack) {
		irq_mask = 0;
		enable = WM8962_MICDET_ENA;
	} else {
		irq_mask = WM8962_MICD_EINT | WM8962_MICSCD_EINT;
		enable = 0;
	}

	snd_soc_update_bits(codec, WM8962_INTERRUPT_STATUS_2_MASK,
			    WM8962_MICD_EINT | WM8962_MICSCD_EINT, irq_mask);
	snd_soc_update_bits(codec, WM8962_ADDITIONAL_CONTROL_4,
			    WM8962_MICDET_ENA, enable);

	
	snd_soc_jack_report(wm8962->jack, 0,
			    SND_JACK_MICROPHONE | SND_JACK_BTN_0);

	if (jack) {
		snd_soc_dapm_force_enable_pin(&codec->dapm, "SYSCLK");
		snd_soc_dapm_force_enable_pin(&codec->dapm, "MICBIAS");
	} else {
		snd_soc_dapm_disable_pin(&codec->dapm, "SYSCLK");
		snd_soc_dapm_disable_pin(&codec->dapm, "MICBIAS");
	}

	return 0;
}
EXPORT_SYMBOL_GPL(wm8962_mic_detect);

#if defined(CONFIG_INPUT) || defined(CONFIG_INPUT_MODULE)
static int beep_rates[] = {
	500, 1000, 2000, 4000,
};

static void wm8962_beep_work(struct work_struct *work)
{
	struct wm8962_priv *wm8962 =
		container_of(work, struct wm8962_priv, beep_work);
	struct snd_soc_codec *codec = wm8962->codec;
	struct snd_soc_dapm_context *dapm = &codec->dapm;
	int i;
	int reg = 0;
	int best = 0;

	if (wm8962->beep_rate) {
		for (i = 0; i < ARRAY_SIZE(beep_rates); i++) {
			if (abs(wm8962->beep_rate - beep_rates[i]) <
			    abs(wm8962->beep_rate - beep_rates[best]))
				best = i;
		}

		dev_dbg(codec->dev, "Set beep rate %dHz for requested %dHz\n",
			beep_rates[best], wm8962->beep_rate);

		reg = WM8962_BEEP_ENA | (best << WM8962_BEEP_RATE_SHIFT);

		snd_soc_dapm_enable_pin(dapm, "Beep");
	} else {
		dev_dbg(codec->dev, "Disabling beep\n");
		snd_soc_dapm_disable_pin(dapm, "Beep");
	}

	snd_soc_update_bits(codec, WM8962_BEEP_GENERATOR_1,
			    WM8962_BEEP_ENA | WM8962_BEEP_RATE_MASK, reg);

	snd_soc_dapm_sync(dapm);
}

static int wm8962_beep_event(struct input_dev *dev, unsigned int type,
			     unsigned int code, int hz)
{
	struct snd_soc_codec *codec = input_get_drvdata(dev);
	struct wm8962_priv *wm8962 = snd_soc_codec_get_drvdata(codec);

	dev_dbg(codec->dev, "Beep event %x %x\n", code, hz);

	switch (code) {
	case SND_BELL:
		if (hz)
			hz = 1000;
	case SND_TONE:
		break;
	default:
		return -1;
	}

	
	wm8962->beep_rate = hz;
	schedule_work(&wm8962->beep_work);
	return 0;
}

static ssize_t wm8962_beep_set(struct device *dev,
			       struct device_attribute *attr,
			       const char *buf, size_t count)
{
	struct wm8962_priv *wm8962 = dev_get_drvdata(dev);
	long int time;
	int ret;

	ret = strict_strtol(buf, 10, &time);
	if (ret != 0)
		return ret;

	input_event(wm8962->beep, EV_SND, SND_TONE, time);

	return count;
}

static DEVICE_ATTR(beep, 0200, NULL, wm8962_beep_set);

static void wm8962_init_beep(struct snd_soc_codec *codec)
{
	struct wm8962_priv *wm8962 = snd_soc_codec_get_drvdata(codec);
	int ret;

	wm8962->beep = input_allocate_device();
	if (!wm8962->beep) {
		dev_err(codec->dev, "Failed to allocate beep device\n");
		return;
	}

	INIT_WORK(&wm8962->beep_work, wm8962_beep_work);
	wm8962->beep_rate = 0;

	wm8962->beep->name = "WM8962 Beep Generator";
	wm8962->beep->phys = dev_name(codec->dev);
	wm8962->beep->id.bustype = BUS_I2C;

	wm8962->beep->evbit[0] = BIT_MASK(EV_SND);
	wm8962->beep->sndbit[0] = BIT_MASK(SND_BELL) | BIT_MASK(SND_TONE);
	wm8962->beep->event = wm8962_beep_event;
	wm8962->beep->dev.parent = codec->dev;
	input_set_drvdata(wm8962->beep, codec);

	ret = input_register_device(wm8962->beep);
	if (ret != 0) {
		input_free_device(wm8962->beep);
		wm8962->beep = NULL;
		dev_err(codec->dev, "Failed to register beep device\n");
	}

	ret = device_create_file(codec->dev, &dev_attr_beep);
	if (ret != 0) {
		dev_err(codec->dev, "Failed to create keyclick file: %d\n",
			ret);
	}
}

static void wm8962_free_beep(struct snd_soc_codec *codec)
{
	struct wm8962_priv *wm8962 = snd_soc_codec_get_drvdata(codec);

	device_remove_file(codec->dev, &dev_attr_beep);
	input_unregister_device(wm8962->beep);
	cancel_work_sync(&wm8962->beep_work);
	wm8962->beep = NULL;

	snd_soc_update_bits(codec, WM8962_BEEP_GENERATOR_1, WM8962_BEEP_ENA,0);
}
#else
static void wm8962_init_beep(struct snd_soc_codec *codec)
{
}

static void wm8962_free_beep(struct snd_soc_codec *codec)
{
}
#endif

static void wm8962_set_gpio_mode(struct snd_soc_codec *codec, int gpio)
{
	int mask = 0;
	int val = 0;

	switch (gpio) {
	case 2:
		mask = WM8962_CLKOUT2_SEL_MASK;
		val = 1 << WM8962_CLKOUT2_SEL_SHIFT;
		break;
	case 3:
		mask = WM8962_CLKOUT3_SEL_MASK;
		val = 1 << WM8962_CLKOUT3_SEL_SHIFT;
		break;
	default:
		break;
	}

	if (mask)
		snd_soc_update_bits(codec, WM8962_ANALOGUE_CLOCKING1,
				    mask, val);
}

#ifdef CONFIG_GPIOLIB
static inline struct wm8962_priv *gpio_to_wm8962(struct gpio_chip *chip)
{
	return container_of(chip, struct wm8962_priv, gpio_chip);
}

static int wm8962_gpio_request(struct gpio_chip *chip, unsigned offset)
{
	struct wm8962_priv *wm8962 = gpio_to_wm8962(chip);
	struct snd_soc_codec *codec = wm8962->codec;

	switch (offset + 1) {
	case 2:
	case 3:
	case 5:
	case 6:
		break;
	default:
		return -EINVAL;
	}

	wm8962_set_gpio_mode(codec, offset + 1);

	return 0;
}

static void wm8962_gpio_set(struct gpio_chip *chip, unsigned offset, int value)
{
	struct wm8962_priv *wm8962 = gpio_to_wm8962(chip);
	struct snd_soc_codec *codec = wm8962->codec;

	snd_soc_update_bits(codec, WM8962_GPIO_BASE + offset,
			    WM8962_GP2_LVL, !!value << WM8962_GP2_LVL_SHIFT);
}

static int wm8962_gpio_direction_out(struct gpio_chip *chip,
				     unsigned offset, int value)
{
	struct wm8962_priv *wm8962 = gpio_to_wm8962(chip);
	struct snd_soc_codec *codec = wm8962->codec;
	int ret, val;

	
	val = (1 << WM8962_GP2_FN_SHIFT) | (value << WM8962_GP2_LVL_SHIFT);

	ret = snd_soc_update_bits(codec, WM8962_GPIO_BASE + offset,
				  WM8962_GP2_FN_MASK | WM8962_GP2_LVL, val);
	if (ret < 0)
		return ret;

	return 0;
}

static struct gpio_chip wm8962_template_chip = {
	.label			= "wm8962",
	.owner			= THIS_MODULE,
	.request		= wm8962_gpio_request,
	.direction_output	= wm8962_gpio_direction_out,
	.set			= wm8962_gpio_set,
	.can_sleep		= 1,
};

static void wm8962_init_gpio(struct snd_soc_codec *codec)
{
	struct wm8962_priv *wm8962 = snd_soc_codec_get_drvdata(codec);
	struct wm8962_pdata *pdata = dev_get_platdata(codec->dev);
	int ret;

	wm8962->gpio_chip = wm8962_template_chip;
	wm8962->gpio_chip.ngpio = WM8962_MAX_GPIO;
	wm8962->gpio_chip.dev = codec->dev;

	if (pdata && pdata->gpio_base)
		wm8962->gpio_chip.base = pdata->gpio_base;
	else
		wm8962->gpio_chip.base = -1;

	ret = gpiochip_add(&wm8962->gpio_chip);
	if (ret != 0)
		dev_err(codec->dev, "Failed to add GPIOs: %d\n", ret);
}

static void wm8962_free_gpio(struct snd_soc_codec *codec)
{
	struct wm8962_priv *wm8962 = snd_soc_codec_get_drvdata(codec);
	int ret;

	ret = gpiochip_remove(&wm8962->gpio_chip);
	if (ret != 0)
		dev_err(codec->dev, "Failed to remove GPIOs: %d\n", ret);
}
#else
static void wm8962_init_gpio(struct snd_soc_codec *codec)
{
}

static void wm8962_free_gpio(struct snd_soc_codec *codec)
{
}
#endif

static int wm8962_probe(struct snd_soc_codec *codec)
{
	int ret;
	struct wm8962_priv *wm8962 = snd_soc_codec_get_drvdata(codec);
	struct wm8962_pdata *pdata = dev_get_platdata(codec->dev);
	u16 *reg_cache = codec->reg_cache;
	int i, trigger, irq_pol;
	bool dmicclk, dmicdat;

	wm8962->codec = codec;
	codec->control_data = wm8962->regmap;

	ret = snd_soc_codec_set_cache_io(codec, 16, 16, SND_SOC_REGMAP);
	if (ret != 0) {
		dev_err(codec->dev, "Failed to set cache I/O: %d\n", ret);
		return ret;
	}

	wm8962->disable_nb[0].notifier_call = wm8962_regulator_event_0;
	wm8962->disable_nb[1].notifier_call = wm8962_regulator_event_1;
	wm8962->disable_nb[2].notifier_call = wm8962_regulator_event_2;
	wm8962->disable_nb[3].notifier_call = wm8962_regulator_event_3;
	wm8962->disable_nb[4].notifier_call = wm8962_regulator_event_4;
	wm8962->disable_nb[5].notifier_call = wm8962_regulator_event_5;
	wm8962->disable_nb[6].notifier_call = wm8962_regulator_event_6;
	wm8962->disable_nb[7].notifier_call = wm8962_regulator_event_7;

	
	for (i = 0; i < ARRAY_SIZE(wm8962->supplies); i++) {
		ret = regulator_register_notifier(wm8962->supplies[i].consumer,
						  &wm8962->disable_nb[i]);
		if (ret != 0) {
			dev_err(codec->dev,
				"Failed to register regulator notifier: %d\n",
				ret);
		}
	}

	snd_soc_update_bits(codec, WM8962_CLOCKING2, WM8962_SYSCLK_ENA, 0);

	
	snd_soc_update_bits(codec, WM8962_CLOCKING2,
			    WM8962_CLKREG_OVD, WM8962_CLKREG_OVD);

	
	snd_soc_update_bits(codec, WM8962_PLL2,
			    WM8962_OSC_ENA | WM8962_PLL2_ENA | WM8962_PLL3_ENA,
			    0);

	if (pdata) {
		
		for (i = 0; i < ARRAY_SIZE(pdata->gpio_init); i++)
			if (pdata->gpio_init[i]) {
				wm8962_set_gpio_mode(codec, i + 1);
				snd_soc_write(codec, 0x200 + i,
					      pdata->gpio_init[i] & 0xffff);
			}

		
		if (pdata->spk_mono)
			reg_cache[WM8962_CLASS_D_CONTROL_2]
				|= WM8962_SPK_MONO;

		if (pdata->mic_cfg)
			snd_soc_update_bits(codec, WM8962_ADDITIONAL_CONTROL_4,
					    WM8962_MICDET_ENA |
					    WM8962_MICDET_THR_MASK |
					    WM8962_MICSHORT_THR_MASK |
					    WM8962_MICBIAS_LVL,
					    pdata->mic_cfg);
	}

	
	snd_soc_update_bits(codec, WM8962_LEFT_INPUT_VOLUME,
			    WM8962_IN_VU, WM8962_IN_VU);
	snd_soc_update_bits(codec, WM8962_RIGHT_INPUT_VOLUME,
			    WM8962_IN_VU, WM8962_IN_VU);
	snd_soc_update_bits(codec, WM8962_LEFT_ADC_VOLUME,
			    WM8962_ADC_VU, WM8962_ADC_VU);
	snd_soc_update_bits(codec, WM8962_RIGHT_ADC_VOLUME,
			    WM8962_ADC_VU, WM8962_ADC_VU);
	snd_soc_update_bits(codec, WM8962_LEFT_DAC_VOLUME,
			    WM8962_DAC_VU, WM8962_DAC_VU);
	snd_soc_update_bits(codec, WM8962_RIGHT_DAC_VOLUME,
			    WM8962_DAC_VU, WM8962_DAC_VU);
	snd_soc_update_bits(codec, WM8962_SPKOUTL_VOLUME,
			    WM8962_SPKOUT_VU, WM8962_SPKOUT_VU);
	snd_soc_update_bits(codec, WM8962_SPKOUTR_VOLUME,
			    WM8962_SPKOUT_VU, WM8962_SPKOUT_VU);
	snd_soc_update_bits(codec, WM8962_HPOUTL_VOLUME,
			    WM8962_HPOUT_VU, WM8962_HPOUT_VU);
	snd_soc_update_bits(codec, WM8962_HPOUTR_VOLUME,
			    WM8962_HPOUT_VU, WM8962_HPOUT_VU);

	
	snd_soc_update_bits(codec, WM8962_EQ1, WM8962_EQ_SHARED_COEFF, 0);

	
	snd_soc_update_bits(codec, WM8962_IRQ_DEBOUNCE,
			    WM8962_FLL_LOCK_DB | WM8962_PLL3_LOCK_DB |
			    WM8962_PLL2_LOCK_DB | WM8962_TEMP_SHUT_DB,
			    0);

	wm8962_add_widgets(codec);

	
	dmicclk = false;
	dmicdat = false;
	for (i = 0; i < WM8962_MAX_GPIO; i++) {
		switch (snd_soc_read(codec, WM8962_GPIO_BASE + i)
			& WM8962_GP2_FN_MASK) {
		case WM8962_GPIO_FN_DMICCLK:
			dmicclk = true;
			break;
		case WM8962_GPIO_FN_DMICDAT:
			dmicdat = true;
			break;
		default:
			break;
		}
	}
	if (!dmicclk || !dmicdat) {
		dev_dbg(codec->dev, "DMIC not in use, disabling\n");
		snd_soc_dapm_nc_pin(&codec->dapm, "DMICDAT");
	}
	if (dmicclk != dmicdat)
		dev_warn(codec->dev, "DMIC GPIOs partially configured\n");

	wm8962_init_beep(codec);
	wm8962_init_gpio(codec);

	if (wm8962->irq) {
		if (pdata && pdata->irq_active_low) {
			trigger = IRQF_TRIGGER_LOW;
			irq_pol = WM8962_IRQ_POL;
		} else {
			trigger = IRQF_TRIGGER_HIGH;
			irq_pol = 0;
		}

		snd_soc_update_bits(codec, WM8962_INTERRUPT_CONTROL,
				    WM8962_IRQ_POL, irq_pol);

		ret = request_threaded_irq(wm8962->irq, NULL, wm8962_irq,
					   trigger | IRQF_ONESHOT,
					   "wm8962", codec->dev);
		if (ret != 0) {
			dev_err(codec->dev, "Failed to request IRQ %d: %d\n",
				wm8962->irq, ret);
			wm8962->irq = 0;
			
		} else {
			
			snd_soc_update_bits(codec,
					    WM8962_INTERRUPT_STATUS_2_MASK,
					    WM8962_FLL_LOCK_EINT |
					    WM8962_TEMP_SHUT_EINT |
					    WM8962_FIFOS_ERR_EINT, 0);
		}
	}

	return 0;
}

static int wm8962_remove(struct snd_soc_codec *codec)
{
	struct wm8962_priv *wm8962 = snd_soc_codec_get_drvdata(codec);
	int i;

	if (wm8962->irq)
		free_irq(wm8962->irq, codec);

	cancel_delayed_work_sync(&wm8962->mic_work);

	wm8962_free_gpio(codec);
	wm8962_free_beep(codec);
	for (i = 0; i < ARRAY_SIZE(wm8962->supplies); i++)
		regulator_unregister_notifier(wm8962->supplies[i].consumer,
					      &wm8962->disable_nb[i]);

	return 0;
}

static struct snd_soc_codec_driver soc_codec_dev_wm8962 = {
	.probe =	wm8962_probe,
	.remove =	wm8962_remove,
	.set_bias_level = wm8962_set_bias_level,
	.set_pll = wm8962_set_fll,
	.idle_bias_off = true,
};

static const struct reg_default wm8962_dc_measure[] = {
	{ 0xfd, 0x1 },
	{ 0xcc, 0x40 },
	{ 0xfd, 0 },
};

static const struct regmap_config wm8962_regmap = {
	.reg_bits = 16,
	.val_bits = 16,

	.max_register = WM8962_MAX_REGISTER,
	.reg_defaults = wm8962_reg,
	.num_reg_defaults = ARRAY_SIZE(wm8962_reg),
	.volatile_reg = wm8962_volatile_register,
	.readable_reg = wm8962_readable_register,
	.cache_type = REGCACHE_RBTREE,
};

static __devinit int wm8962_i2c_probe(struct i2c_client *i2c,
				      const struct i2c_device_id *id)
{
	struct wm8962_pdata *pdata = dev_get_platdata(&i2c->dev);
	struct wm8962_priv *wm8962;
	unsigned int reg;
	int ret, i;

	wm8962 = devm_kzalloc(&i2c->dev, sizeof(struct wm8962_priv),
			      GFP_KERNEL);
	if (wm8962 == NULL)
		return -ENOMEM;

	i2c_set_clientdata(i2c, wm8962);

	INIT_DELAYED_WORK(&wm8962->mic_work, wm8962_mic_work);
	init_completion(&wm8962->fll_lock);
	wm8962->irq = i2c->irq;

	for (i = 0; i < ARRAY_SIZE(wm8962->supplies); i++)
		wm8962->supplies[i].supply = wm8962_supply_names[i];

	ret = regulator_bulk_get(&i2c->dev, ARRAY_SIZE(wm8962->supplies),
				 wm8962->supplies);
	if (ret != 0) {
		dev_err(&i2c->dev, "Failed to request supplies: %d\n", ret);
		goto err;
	}

	ret = regulator_bulk_enable(ARRAY_SIZE(wm8962->supplies),
				    wm8962->supplies);
	if (ret != 0) {
		dev_err(&i2c->dev, "Failed to enable supplies: %d\n", ret);
		goto err_get;
	}

	wm8962->regmap = regmap_init_i2c(i2c, &wm8962_regmap);
	if (IS_ERR(wm8962->regmap)) {
		ret = PTR_ERR(wm8962->regmap);
		dev_err(&i2c->dev, "Failed to allocate regmap: %d\n", ret);
		goto err_enable;
	}

	regcache_cache_bypass(wm8962->regmap, true);

	ret = regmap_read(wm8962->regmap, WM8962_SOFTWARE_RESET, &reg);
	if (ret < 0) {
		dev_err(&i2c->dev, "Failed to read ID register\n");
		goto err_regmap;
	}
	if (reg != 0x6243) {
		dev_err(&i2c->dev,
			"Device is not a WM8962, ID %x != 0x6243\n", reg);
		ret = -EINVAL;
		goto err_regmap;
	}

	ret = regmap_read(wm8962->regmap, WM8962_RIGHT_INPUT_VOLUME, &reg);
	if (ret < 0) {
		dev_err(&i2c->dev, "Failed to read device revision: %d\n",
			ret);
		goto err_regmap;
	}

	dev_info(&i2c->dev, "customer id %x revision %c\n",
		 (reg & WM8962_CUST_ID_MASK) >> WM8962_CUST_ID_SHIFT,
		 ((reg & WM8962_CHIP_REV_MASK) >> WM8962_CHIP_REV_SHIFT)
		 + 'A');

	regcache_cache_bypass(wm8962->regmap, false);

	ret = wm8962_reset(wm8962);
	if (ret < 0) {
		dev_err(&i2c->dev, "Failed to issue reset\n");
		goto err_regmap;
	}

	if (pdata && pdata->in4_dc_measure) {
		ret = regmap_register_patch(wm8962->regmap,
					    wm8962_dc_measure,
					    ARRAY_SIZE(wm8962_dc_measure));
		if (ret != 0)
			dev_err(&i2c->dev,
				"Failed to configure for DC mesurement: %d\n",
				ret);
	}

	pm_runtime_enable(&i2c->dev);
	pm_request_idle(&i2c->dev);

	ret = snd_soc_register_codec(&i2c->dev,
				     &soc_codec_dev_wm8962, &wm8962_dai, 1);
	if (ret < 0)
		goto err_regmap;

	
	regulator_bulk_disable(ARRAY_SIZE(wm8962->supplies), wm8962->supplies);

	return 0;

err_regmap:
	regmap_exit(wm8962->regmap);
err_enable:
	regulator_bulk_disable(ARRAY_SIZE(wm8962->supplies), wm8962->supplies);
err_get:
	regulator_bulk_free(ARRAY_SIZE(wm8962->supplies), wm8962->supplies);
err:
	return ret;
}

static __devexit int wm8962_i2c_remove(struct i2c_client *client)
{
	struct wm8962_priv *wm8962 = dev_get_drvdata(&client->dev);

	snd_soc_unregister_codec(&client->dev);
	regmap_exit(wm8962->regmap);
	regulator_bulk_free(ARRAY_SIZE(wm8962->supplies), wm8962->supplies);
	return 0;
}

#ifdef CONFIG_PM_RUNTIME
static int wm8962_runtime_resume(struct device *dev)
{
	struct wm8962_priv *wm8962 = dev_get_drvdata(dev);
	int ret;

	ret = regulator_bulk_enable(ARRAY_SIZE(wm8962->supplies),
				    wm8962->supplies);
	if (ret != 0) {
		dev_err(dev,
			"Failed to enable supplies: %d\n", ret);
		return ret;
	}

	regcache_cache_only(wm8962->regmap, false);
	regcache_sync(wm8962->regmap);

	regmap_update_bits(wm8962->regmap, WM8962_ANTI_POP,
			   WM8962_STARTUP_BIAS_ENA | WM8962_VMID_BUF_ENA,
			   WM8962_STARTUP_BIAS_ENA | WM8962_VMID_BUF_ENA);

	
	regmap_update_bits(wm8962->regmap, WM8962_PWR_MGMT_1,
			   WM8962_VMID_SEL_MASK | WM8962_BIAS_ENA,
			   WM8962_BIAS_ENA | 0x180);

	msleep(5);

	
	regmap_update_bits(wm8962->regmap, WM8962_PWR_MGMT_1,
			   WM8962_VMID_SEL_MASK, 0x100);

	return 0;
}

static int wm8962_runtime_suspend(struct device *dev)
{
	struct wm8962_priv *wm8962 = dev_get_drvdata(dev);

	regmap_update_bits(wm8962->regmap, WM8962_PWR_MGMT_1,
			   WM8962_VMID_SEL_MASK | WM8962_BIAS_ENA, 0);

	regmap_update_bits(wm8962->regmap, WM8962_ANTI_POP,
			   WM8962_STARTUP_BIAS_ENA |
			   WM8962_VMID_BUF_ENA, 0);

	regcache_cache_only(wm8962->regmap, true);

	regulator_bulk_disable(ARRAY_SIZE(wm8962->supplies),
			       wm8962->supplies);

	return 0;
}
#endif

static struct dev_pm_ops wm8962_pm = {
	SET_RUNTIME_PM_OPS(wm8962_runtime_suspend, wm8962_runtime_resume, NULL)
};

static const struct i2c_device_id wm8962_i2c_id[] = {
	{ "wm8962", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, wm8962_i2c_id);

static struct i2c_driver wm8962_i2c_driver = {
	.driver = {
		.name = "wm8962",
		.owner = THIS_MODULE,
		.pm = &wm8962_pm,
	},
	.probe =    wm8962_i2c_probe,
	.remove =   __devexit_p(wm8962_i2c_remove),
	.id_table = wm8962_i2c_id,
};

module_i2c_driver(wm8962_i2c_driver);

MODULE_DESCRIPTION("ASoC WM8962 driver");
MODULE_AUTHOR("Mark Brown <broonie@opensource.wolfsonmicro.com>");
MODULE_LICENSE("GPL");
