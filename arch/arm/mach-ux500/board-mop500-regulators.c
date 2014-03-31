/*
 * Copyright (C) ST-Ericsson SA 2010
 *
 * License Terms: GNU General Public License v2
 *
 * Authors: Sundar Iyer <sundar.iyer@stericsson.com>
 *          Bengt Jonsson <bengt.g.jonsson@stericsson.com>
 *
 * MOP500 board specific initialization for regulators
 */
#include <linux/kernel.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/ab8500.h>
#include "board-mop500-regulators.h"

static struct regulator_consumer_supply tps61052_vaudio_consumers[] = {
	REGULATOR_SUPPLY("vintdclassint", "ab8500-codec.0"),
};

struct regulator_init_data tps61052_regulator = {
	.constraints = {
		.name = "vaudio-hf",
		.min_uV = 4500000,
		.max_uV = 4500000,
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
	},
	.num_consumer_supplies = ARRAY_SIZE(tps61052_vaudio_consumers),
	.consumer_supplies = tps61052_vaudio_consumers,
};

static struct regulator_consumer_supply ab8500_vaux1_consumers[] = {
	
	REGULATOR_SUPPLY("vaux12v5", "mcde.0"),
	
	REGULATOR_SUPPLY("vcc", "gpio-keys.0"),
	
	REGULATOR_SUPPLY("vcc", "2-0029"),
	
	REGULATOR_SUPPLY("vdd", "3-0018"),
	
	REGULATOR_SUPPLY("vdd", "3-001e"),
	
	REGULATOR_SUPPLY("avdd", "3-005c"),
	REGULATOR_SUPPLY("avdd", "3-005d"),
	
	REGULATOR_SUPPLY("vdd", "3-004b"),
};

static struct regulator_consumer_supply ab8500_vaux2_consumers[] = {
	
	REGULATOR_SUPPLY("vmmc", "sdi4"),
	
	REGULATOR_SUPPLY("vcc-N2158", "ab8500-codec.0"),
};

static struct regulator_consumer_supply ab8500_vaux3_consumers[] = {
	
	REGULATOR_SUPPLY("vmmc", "sdi0"),
};

static struct regulator_consumer_supply ab8500_vtvout_consumers[] = {
	
	REGULATOR_SUPPLY("vtvout", "ab8500-denc.0"),
	
	REGULATOR_SUPPLY("vddadc", "ab8500-gpadc.0"),
};

static struct regulator_consumer_supply ab8500_vaud_consumers[] = {
	
	REGULATOR_SUPPLY("vaud", "ab8500-codec.0"),
};

static struct regulator_consumer_supply ab8500_vamic1_consumers[] = {
	
	REGULATOR_SUPPLY("vamic1", "ab8500-codec.0"),
};

static struct regulator_consumer_supply ab8500_vamic2_consumers[] = {
	
	REGULATOR_SUPPLY("vamic2", "ab8500-codec.0"),
};

static struct regulator_consumer_supply ab8500_vdmic_consumers[] = {
	
	REGULATOR_SUPPLY("vdmic", "ab8500-codec.0"),
};

static struct regulator_consumer_supply ab8500_vintcore_consumers[] = {
	
	REGULATOR_SUPPLY("v-intcore", NULL),
	
	REGULATOR_SUPPLY("vddulpivio18", "ab8500-usb.0"),
};

static struct regulator_consumer_supply ab8500_vana_consumers[] = {
	
	REGULATOR_SUPPLY("vsmps2", "mcde.0"),
};

struct ab8500_regulator_reg_init
ab8500_regulator_reg_init[AB8500_NUM_REGULATOR_REGISTERS] = {
	INIT_REGULATOR_REGISTER(AB8500_REGUREQUESTCTRL2, 0x00),
	INIT_REGULATOR_REGISTER(AB8500_REGUREQUESTCTRL3, 0x00),
	INIT_REGULATOR_REGISTER(AB8500_REGUREQUESTCTRL4, 0x00),
	INIT_REGULATOR_REGISTER(AB8500_REGUSYSCLKREQ1HPVALID1, 0x00),
	INIT_REGULATOR_REGISTER(AB8500_REGUSYSCLKREQ1HPVALID2, 0x40),
	INIT_REGULATOR_REGISTER(AB8500_REGUHWHPREQ1VALID1, 0x00),
	INIT_REGULATOR_REGISTER(AB8500_REGUHWHPREQ1VALID2, 0x00),
	INIT_REGULATOR_REGISTER(AB8500_REGUHWHPREQ2VALID1, 0x00),
	INIT_REGULATOR_REGISTER(AB8500_REGUHWHPREQ2VALID2, 0x04),
	INIT_REGULATOR_REGISTER(AB8500_REGUSWHPREQVALID1, 0x00),
	INIT_REGULATOR_REGISTER(AB8500_REGUSWHPREQVALID2, 0x00),
	INIT_REGULATOR_REGISTER(AB8500_REGUSYSCLKREQVALID1, 0x2a),
	INIT_REGULATOR_REGISTER(AB8500_REGUSYSCLKREQVALID2, 0x20),
	INIT_REGULATOR_REGISTER(AB8500_REGUMISC1, 0x10),
	INIT_REGULATOR_REGISTER(AB8500_VAUDIOSUPPLY, 0x00),
	INIT_REGULATOR_REGISTER(AB8500_REGUCTRL1VAMIC, 0x00),
	INIT_REGULATOR_REGISTER(AB8500_VPLLVANAREGU, 0x02),
	INIT_REGULATOR_REGISTER(AB8500_VREFDDR, 0x00),
	INIT_REGULATOR_REGISTER(AB8500_EXTSUPPLYREGU, 0x2a),
	INIT_REGULATOR_REGISTER(AB8500_VAUX12REGU, 0x01),
	INIT_REGULATOR_REGISTER(AB8500_VRF1VAUX3REGU, 0x00),
	INIT_REGULATOR_REGISTER(AB8500_VSMPS1SEL1, 0x24),
	INIT_REGULATOR_REGISTER(AB8500_VAUX1SEL, 0x08),
	INIT_REGULATOR_REGISTER(AB8500_VAUX2SEL, 0x0d),
	INIT_REGULATOR_REGISTER(AB8500_VRF1VAUX3SEL, 0x07),
	INIT_REGULATOR_REGISTER(AB8500_REGUCTRL2SPARE, 0x00),
	INIT_REGULATOR_REGISTER(AB8500_REGUCTRLDISCH, 0x00),
	INIT_REGULATOR_REGISTER(AB8500_REGUCTRLDISCH2, 0x00),
};

struct regulator_init_data ab8500_regulators[AB8500_NUM_REGULATORS] = {
	
	[AB8500_LDO_AUX1] = {
		.constraints = {
			.name = "V-DISPLAY",
			.min_uV = 2500000,
			.max_uV = 2900000,
			.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE |
					  REGULATOR_CHANGE_STATUS,
			.boot_on = 1, 
			.always_on = 1,
		},
		.num_consumer_supplies = ARRAY_SIZE(ab8500_vaux1_consumers),
		.consumer_supplies = ab8500_vaux1_consumers,
	},
	
	[AB8500_LDO_AUX2] = {
		.constraints = {
			.name = "V-eMMC1",
			.min_uV = 1100000,
			.max_uV = 3300000,
			.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE |
					  REGULATOR_CHANGE_STATUS,
		},
		.num_consumer_supplies = ARRAY_SIZE(ab8500_vaux2_consumers),
		.consumer_supplies = ab8500_vaux2_consumers,
	},
	
	[AB8500_LDO_AUX3] = {
		.constraints = {
			.name = "V-MMC-SD",
			.min_uV = 1100000,
			.max_uV = 3300000,
			.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE |
					  REGULATOR_CHANGE_STATUS,
		},
		.num_consumer_supplies = ARRAY_SIZE(ab8500_vaux3_consumers),
		.consumer_supplies = ab8500_vaux3_consumers,
	},
	
	[AB8500_LDO_TVOUT] = {
		.constraints = {
			.name = "V-TVOUT",
			.valid_ops_mask = REGULATOR_CHANGE_STATUS,
		},
		.num_consumer_supplies = ARRAY_SIZE(ab8500_vtvout_consumers),
		.consumer_supplies = ab8500_vtvout_consumers,
	},
	
	[AB8500_LDO_AUDIO] = {
		.constraints = {
			.name = "V-AUD",
			.valid_ops_mask = REGULATOR_CHANGE_STATUS,
		},
		.num_consumer_supplies = ARRAY_SIZE(ab8500_vaud_consumers),
		.consumer_supplies = ab8500_vaud_consumers,
	},
	
	[AB8500_LDO_ANAMIC1] = {
		.constraints = {
			.name = "V-AMIC1",
			.valid_ops_mask = REGULATOR_CHANGE_STATUS,
		},
		.num_consumer_supplies = ARRAY_SIZE(ab8500_vamic1_consumers),
		.consumer_supplies = ab8500_vamic1_consumers,
	},
	
	[AB8500_LDO_ANAMIC2] = {
		.constraints = {
			.name = "V-AMIC2",
			.valid_ops_mask = REGULATOR_CHANGE_STATUS,
		},
		.num_consumer_supplies = ARRAY_SIZE(ab8500_vamic2_consumers),
		.consumer_supplies = ab8500_vamic2_consumers,
	},
	
	[AB8500_LDO_DMIC] = {
		.constraints = {
			.name = "V-DMIC",
			.valid_ops_mask = REGULATOR_CHANGE_STATUS,
		},
		.num_consumer_supplies = ARRAY_SIZE(ab8500_vdmic_consumers),
		.consumer_supplies = ab8500_vdmic_consumers,
	},
	
	[AB8500_LDO_INTCORE] = {
		.constraints = {
			.name = "V-INTCORE",
			.valid_ops_mask = REGULATOR_CHANGE_STATUS,
		},
		.num_consumer_supplies = ARRAY_SIZE(ab8500_vintcore_consumers),
		.consumer_supplies = ab8500_vintcore_consumers,
	},
	
	[AB8500_LDO_ANA] = {
		.constraints = {
			.name = "V-CSI/DSI",
			.valid_ops_mask = REGULATOR_CHANGE_STATUS,
		},
		.num_consumer_supplies = ARRAY_SIZE(ab8500_vana_consumers),
		.consumer_supplies = ab8500_vana_consumers,
	},
};
