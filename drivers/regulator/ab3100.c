/*
 * drivers/regulator/ab3100.c
 *
 * Copyright (C) 2008-2009 ST-Ericsson AB
 * License terms: GNU General Public License (GPL) version 2
 * Low-level control of the AB3100 IC Low Dropout (LDO)
 * regulators, external regulator and buck converter
 * Author: Mattias Wallin <mattias.wallin@stericsson.com>
 * Author: Linus Walleij <linus.walleij@stericsson.com>
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/regulator/driver.h>
#include <linux/mfd/abx500.h>

#define AB3100_LDO_A		0x40
#define AB3100_LDO_C		0x41
#define AB3100_LDO_D		0x42
#define AB3100_LDO_E		0x43
#define AB3100_LDO_E_SLEEP	0x44
#define AB3100_LDO_F		0x45
#define AB3100_LDO_G		0x46
#define AB3100_LDO_H		0x47
#define AB3100_LDO_H_SLEEP_MODE	0
#define AB3100_LDO_H_SLEEP_EN	2
#define AB3100_LDO_ON		4
#define AB3100_LDO_H_VSEL_AC	5
#define AB3100_LDO_K		0x48
#define AB3100_LDO_EXT		0x49
#define AB3100_BUCK		0x4A
#define AB3100_BUCK_SLEEP	0x4B
#define AB3100_REG_ON_MASK	0x10

struct ab3100_regulator {
	struct regulator_dev *rdev;
	struct device *dev;
	struct ab3100_platform_data *plfdata;
	u8 regreg;
	int fixed_voltage;
	int const *typ_voltages;
	u8 voltages_len;
};

static const u8 ab3100_reg_init_order[AB3100_NUM_REGULATORS+2] = {
	AB3100_LDO_A,
	AB3100_LDO_C,
	AB3100_LDO_E,
	AB3100_LDO_E_SLEEP,
	AB3100_LDO_F,
	AB3100_LDO_G,
	AB3100_LDO_H,
	AB3100_LDO_K,
	AB3100_LDO_EXT,
	AB3100_BUCK,
	AB3100_BUCK_SLEEP,
	AB3100_LDO_D,
};

#define LDO_A_VOLTAGE 2750000
#define LDO_C_VOLTAGE 2650000
#define LDO_D_VOLTAGE 2650000

static const int ldo_e_buck_typ_voltages[] = {
	1800000,
	1400000,
	1300000,
	1200000,
	1100000,
	1050000,
	900000,
};

static const int ldo_f_typ_voltages[] = {
	1800000,
	1400000,
	1300000,
	1200000,
	1100000,
	1050000,
	2500000,
	2650000,
};

static const int ldo_g_typ_voltages[] = {
	2850000,
	2750000,
	1800000,
	1500000,
};

static const int ldo_h_typ_voltages[] = {
	2750000,
	1800000,
	1500000,
	1200000,
};

static const int ldo_k_typ_voltages[] = {
	2750000,
	1800000,
};


static struct ab3100_regulator
ab3100_regulators[AB3100_NUM_REGULATORS] = {
	{
		.regreg = AB3100_LDO_A,
		.fixed_voltage = LDO_A_VOLTAGE,
	},
	{
		.regreg = AB3100_LDO_C,
		.fixed_voltage = LDO_C_VOLTAGE,
	},
	{
		.regreg = AB3100_LDO_D,
		.fixed_voltage = LDO_D_VOLTAGE,
	},
	{
		.regreg = AB3100_LDO_E,
		.typ_voltages = ldo_e_buck_typ_voltages,
		.voltages_len = ARRAY_SIZE(ldo_e_buck_typ_voltages),
	},
	{
		.regreg = AB3100_LDO_F,
		.typ_voltages = ldo_f_typ_voltages,
		.voltages_len = ARRAY_SIZE(ldo_f_typ_voltages),
	},
	{
		.regreg = AB3100_LDO_G,
		.typ_voltages = ldo_g_typ_voltages,
		.voltages_len = ARRAY_SIZE(ldo_g_typ_voltages),
	},
	{
		.regreg = AB3100_LDO_H,
		.typ_voltages = ldo_h_typ_voltages,
		.voltages_len = ARRAY_SIZE(ldo_h_typ_voltages),
	},
	{
		.regreg = AB3100_LDO_K,
		.typ_voltages = ldo_k_typ_voltages,
		.voltages_len = ARRAY_SIZE(ldo_k_typ_voltages),
	},
	{
		.regreg = AB3100_LDO_EXT,
		
	},
	{
		.regreg = AB3100_BUCK,
		.typ_voltages = ldo_e_buck_typ_voltages,
		.voltages_len = ARRAY_SIZE(ldo_e_buck_typ_voltages),
	},
};

static int ab3100_enable_regulator(struct regulator_dev *reg)
{
	struct ab3100_regulator *abreg = reg->reg_data;
	int err;
	u8 regval;

	err = abx500_get_register_interruptible(abreg->dev, 0, abreg->regreg,
						&regval);
	if (err) {
		dev_warn(&reg->dev, "failed to get regid %d value\n",
			 abreg->regreg);
		return err;
	}

	
	if (regval & AB3100_REG_ON_MASK)
		return 0;

	regval |= AB3100_REG_ON_MASK;

	err = abx500_set_register_interruptible(abreg->dev, 0, abreg->regreg,
						regval);
	if (err) {
		dev_warn(&reg->dev, "failed to set regid %d value\n",
			 abreg->regreg);
		return err;
	}

	return 0;
}

static int ab3100_disable_regulator(struct regulator_dev *reg)
{
	struct ab3100_regulator *abreg = reg->reg_data;
	int err;
	u8 regval;

	pr_info("Called ab3100_disable_regulator\n");
	if (abreg->regreg == AB3100_LDO_D) {
		dev_info(&reg->dev, "disabling LDO D - shut down system\n");
		
		return abx500_set_register_interruptible(abreg->dev, 0,
							 AB3100_LDO_D, 0x00U);
	}

	err = abx500_get_register_interruptible(abreg->dev, 0, abreg->regreg,
						&regval);
	if (err) {
		dev_err(&reg->dev, "unable to get register 0x%x\n",
			abreg->regreg);
		return err;
	}
	regval &= ~AB3100_REG_ON_MASK;
	return abx500_set_register_interruptible(abreg->dev, 0, abreg->regreg,
						 regval);
}

static int ab3100_is_enabled_regulator(struct regulator_dev *reg)
{
	struct ab3100_regulator *abreg = reg->reg_data;
	u8 regval;
	int err;

	err = abx500_get_register_interruptible(abreg->dev, 0, abreg->regreg,
						&regval);
	if (err) {
		dev_err(&reg->dev, "unable to get register 0x%x\n",
			abreg->regreg);
		return err;
	}

	return regval & AB3100_REG_ON_MASK;
}

static int ab3100_list_voltage_regulator(struct regulator_dev *reg,
					 unsigned selector)
{
	struct ab3100_regulator *abreg = reg->reg_data;

	if (selector >= abreg->voltages_len)
		return -EINVAL;
	return abreg->typ_voltages[selector];
}

static int ab3100_get_voltage_regulator(struct regulator_dev *reg)
{
	struct ab3100_regulator *abreg = reg->reg_data;
	u8 regval;
	int err;

	
	if (abreg->fixed_voltage)
		return abreg->fixed_voltage;

	err = abx500_get_register_interruptible(abreg->dev, 0,
						abreg->regreg, &regval);
	if (err) {
		dev_warn(&reg->dev,
			 "failed to get regulator value in register %02x\n",
			 abreg->regreg);
		return err;
	}

	
	regval &= 0xE0;
	regval >>= 5;

	if (regval >= abreg->voltages_len) {
		dev_err(&reg->dev,
			"regulator register %02x contains an illegal voltage setting\n",
			abreg->regreg);
		return -EINVAL;
	}

	return abreg->typ_voltages[regval];
}

static int ab3100_get_best_voltage_index(struct regulator_dev *reg,
				   int min_uV, int max_uV)
{
	struct ab3100_regulator *abreg = reg->reg_data;
	int i;
	int bestmatch;
	int bestindex;

	bestmatch = INT_MAX;
	bestindex = -1;
	for (i = 0; i < abreg->voltages_len; i++) {
		if (abreg->typ_voltages[i] <= max_uV &&
		    abreg->typ_voltages[i] >= min_uV &&
		    abreg->typ_voltages[i] < bestmatch) {
			bestmatch = abreg->typ_voltages[i];
			bestindex = i;
		}
	}

	if (bestindex < 0) {
		dev_warn(&reg->dev, "requested %d<=x<=%d uV, out of range!\n",
			 min_uV, max_uV);
		return -EINVAL;
	}
	return bestindex;
}

static int ab3100_set_voltage_regulator(struct regulator_dev *reg,
					int min_uV, int max_uV,
					unsigned *selector)
{
	struct ab3100_regulator *abreg = reg->reg_data;
	u8 regval;
	int err;
	int bestindex;

	bestindex = ab3100_get_best_voltage_index(reg, min_uV, max_uV);
	if (bestindex < 0)
		return bestindex;

	*selector = bestindex;

	err = abx500_get_register_interruptible(abreg->dev, 0,
						abreg->regreg, &regval);
	if (err) {
		dev_warn(&reg->dev,
			 "failed to get regulator register %02x\n",
			 abreg->regreg);
		return err;
	}

	
	regval &= ~0xE0;
	regval |= (bestindex << 5);

	err = abx500_set_register_interruptible(abreg->dev, 0,
						abreg->regreg, regval);
	if (err)
		dev_warn(&reg->dev, "failed to set regulator register %02x\n",
			abreg->regreg);

	return err;
}

static int ab3100_set_suspend_voltage_regulator(struct regulator_dev *reg,
						int uV)
{
	struct ab3100_regulator *abreg = reg->reg_data;
	u8 regval;
	int err;
	int bestindex;
	u8 targetreg;

	if (abreg->regreg == AB3100_LDO_E)
		targetreg = AB3100_LDO_E_SLEEP;
	else if (abreg->regreg == AB3100_BUCK)
		targetreg = AB3100_BUCK_SLEEP;
	else
		return -EINVAL;

	
	bestindex = ab3100_get_best_voltage_index(reg, uV, uV);

	err = abx500_get_register_interruptible(abreg->dev, 0,
						targetreg, &regval);
	if (err) {
		dev_warn(&reg->dev,
			 "failed to get regulator register %02x\n",
			 targetreg);
		return err;
	}

	
	regval &= ~0xE0;
	regval |= (bestindex << 5);

	err = abx500_set_register_interruptible(abreg->dev, 0,
						targetreg, regval);
	if (err)
		dev_warn(&reg->dev, "failed to set regulator register %02x\n",
			abreg->regreg);

	return err;
}

static int ab3100_get_voltage_regulator_external(struct regulator_dev *reg)
{
	struct ab3100_regulator *abreg = reg->reg_data;

	return abreg->plfdata->external_voltage;
}

static int ab3100_enable_time_regulator(struct regulator_dev *reg)
{
	struct ab3100_regulator *abreg = reg->reg_data;

	
	switch (abreg->regreg) {
	case AB3100_LDO_A: 
	case AB3100_LDO_C: 
	case AB3100_LDO_D: 
	case AB3100_LDO_E: 
	case AB3100_LDO_H: 
	case AB3100_LDO_K:
		return 200;
	case AB3100_LDO_F:
		return 600;
	case AB3100_LDO_G:
		return 400;
	case AB3100_BUCK:
		return 1000;
	default:
		break;
	}
	return 0;
}

static struct regulator_ops regulator_ops_fixed = {
	.enable      = ab3100_enable_regulator,
	.disable     = ab3100_disable_regulator,
	.is_enabled  = ab3100_is_enabled_regulator,
	.get_voltage = ab3100_get_voltage_regulator,
	.enable_time = ab3100_enable_time_regulator,
};

static struct regulator_ops regulator_ops_variable = {
	.enable      = ab3100_enable_regulator,
	.disable     = ab3100_disable_regulator,
	.is_enabled  = ab3100_is_enabled_regulator,
	.get_voltage = ab3100_get_voltage_regulator,
	.set_voltage = ab3100_set_voltage_regulator,
	.list_voltage = ab3100_list_voltage_regulator,
	.enable_time = ab3100_enable_time_regulator,
};

static struct regulator_ops regulator_ops_variable_sleepable = {
	.enable      = ab3100_enable_regulator,
	.disable     = ab3100_disable_regulator,
	.is_enabled  = ab3100_is_enabled_regulator,
	.get_voltage = ab3100_get_voltage_regulator,
	.set_voltage = ab3100_set_voltage_regulator,
	.set_suspend_voltage = ab3100_set_suspend_voltage_regulator,
	.list_voltage = ab3100_list_voltage_regulator,
	.enable_time = ab3100_enable_time_regulator,
};

static struct regulator_ops regulator_ops_external = {
	.enable      = ab3100_enable_regulator,
	.disable     = ab3100_disable_regulator,
	.is_enabled  = ab3100_is_enabled_regulator,
	.get_voltage = ab3100_get_voltage_regulator_external,
};

static struct regulator_desc
ab3100_regulator_desc[AB3100_NUM_REGULATORS] = {
	{
		.name = "LDO_A",
		.id   = AB3100_LDO_A,
		.ops  = &regulator_ops_fixed,
		.type = REGULATOR_VOLTAGE,
		.owner = THIS_MODULE,
	},
	{
		.name = "LDO_C",
		.id   = AB3100_LDO_C,
		.ops  = &regulator_ops_fixed,
		.type = REGULATOR_VOLTAGE,
		.owner = THIS_MODULE,
	},
	{
		.name = "LDO_D",
		.id   = AB3100_LDO_D,
		.ops  = &regulator_ops_fixed,
		.type = REGULATOR_VOLTAGE,
		.owner = THIS_MODULE,
	},
	{
		.name = "LDO_E",
		.id   = AB3100_LDO_E,
		.ops  = &regulator_ops_variable_sleepable,
		.n_voltages = ARRAY_SIZE(ldo_e_buck_typ_voltages),
		.type = REGULATOR_VOLTAGE,
		.owner = THIS_MODULE,
	},
	{
		.name = "LDO_F",
		.id   = AB3100_LDO_F,
		.ops  = &regulator_ops_variable,
		.n_voltages = ARRAY_SIZE(ldo_f_typ_voltages),
		.type = REGULATOR_VOLTAGE,
		.owner = THIS_MODULE,
	},
	{
		.name = "LDO_G",
		.id   = AB3100_LDO_G,
		.ops  = &regulator_ops_variable,
		.n_voltages = ARRAY_SIZE(ldo_g_typ_voltages),
		.type = REGULATOR_VOLTAGE,
		.owner = THIS_MODULE,
	},
	{
		.name = "LDO_H",
		.id   = AB3100_LDO_H,
		.ops  = &regulator_ops_variable,
		.n_voltages = ARRAY_SIZE(ldo_h_typ_voltages),
		.type = REGULATOR_VOLTAGE,
		.owner = THIS_MODULE,
	},
	{
		.name = "LDO_K",
		.id   = AB3100_LDO_K,
		.ops  = &regulator_ops_variable,
		.n_voltages = ARRAY_SIZE(ldo_k_typ_voltages),
		.type = REGULATOR_VOLTAGE,
		.owner = THIS_MODULE,
	},
	{
		.name = "LDO_EXT",
		.id   = AB3100_LDO_EXT,
		.ops  = &regulator_ops_external,
		.type = REGULATOR_VOLTAGE,
		.owner = THIS_MODULE,
	},
	{
		.name = "BUCK",
		.id   = AB3100_BUCK,
		.ops  = &regulator_ops_variable_sleepable,
		.n_voltages = ARRAY_SIZE(ldo_e_buck_typ_voltages),
		.type = REGULATOR_VOLTAGE,
		.owner = THIS_MODULE,
	},
};


static int __devinit ab3100_regulators_probe(struct platform_device *pdev)
{
	struct ab3100_platform_data *plfdata = pdev->dev.platform_data;
	int err = 0;
	u8 data;
	int i;

	
	err = abx500_get_register_interruptible(&pdev->dev, 0,
						AB3100_LDO_D, &data);
	if (err) {
		dev_err(&pdev->dev, "could not read initial status of LDO_D\n");
		return err;
	}
	if (data & 0x10)
		dev_notice(&pdev->dev,
			   "chip is already in active mode (Warm start)\n");
	else
		dev_notice(&pdev->dev,
			   "chip is in inactive mode (Cold start)\n");

	
	for (i = 0; i < ARRAY_SIZE(ab3100_reg_init_order); i++) {
		err = abx500_set_register_interruptible(&pdev->dev, 0,
					ab3100_reg_init_order[i],
					plfdata->reg_initvals[i]);
		if (err) {
			dev_err(&pdev->dev, "regulator initialization failed with error %d\n",
				err);
			return err;
		}
	}

	
	for (i = 0; i < AB3100_NUM_REGULATORS; i++) {
		struct ab3100_regulator *reg = &ab3100_regulators[i];
		struct regulator_dev *rdev;

		reg->dev = &pdev->dev;
		reg->plfdata = plfdata;

		rdev = regulator_register(&ab3100_regulator_desc[i],
					  &pdev->dev,
					  &plfdata->reg_constraints[i],
					  reg, NULL);

		if (IS_ERR(rdev)) {
			err = PTR_ERR(rdev);
			dev_err(&pdev->dev,
				"%s: failed to register regulator %s err %d\n",
				__func__, ab3100_regulator_desc[i].name,
				err);
			
			while (--i >= 0)
				regulator_unregister(ab3100_regulators[i].rdev);
			return err;
		}

		
		reg->rdev = rdev;
	}

	return 0;
}

static int __devexit ab3100_regulators_remove(struct platform_device *pdev)
{
	int i;

	for (i = 0; i < AB3100_NUM_REGULATORS; i++) {
		struct ab3100_regulator *reg = &ab3100_regulators[i];

		regulator_unregister(reg->rdev);
	}
	return 0;
}

static struct platform_driver ab3100_regulators_driver = {
	.driver = {
		.name  = "ab3100-regulators",
		.owner = THIS_MODULE,
	},
	.probe = ab3100_regulators_probe,
	.remove = __devexit_p(ab3100_regulators_remove),
};

static __init int ab3100_regulators_init(void)
{
	return platform_driver_register(&ab3100_regulators_driver);
}

static __exit void ab3100_regulators_exit(void)
{
	platform_driver_unregister(&ab3100_regulators_driver);
}

subsys_initcall(ab3100_regulators_init);
module_exit(ab3100_regulators_exit);

MODULE_AUTHOR("Mattias Wallin <mattias.wallin@stericsson.com>");
MODULE_DESCRIPTION("AB3100 Regulator driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:ab3100-regulators");
