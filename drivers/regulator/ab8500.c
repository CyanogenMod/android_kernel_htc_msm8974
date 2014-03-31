/*
 * Copyright (C) ST-Ericsson SA 2010
 *
 * License Terms: GNU General Public License v2
 *
 * Authors: Sundar Iyer <sundar.iyer@stericsson.com> for ST-Ericsson
 *          Bengt Jonsson <bengt.g.jonsson@stericsson.com> for ST-Ericsson
 *
 * AB8500 peripheral regulators
 *
 * AB8500 supports the following regulators:
 *   VAUX1/2/3, VINTCORE, VTVOUT, VUSB, VAUDIO, VAMIC1/2, VDMIC, VANA
 */
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/mfd/abx500.h>
#include <linux/mfd/abx500/ab8500.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/ab8500.h>

struct ab8500_regulator_info {
	struct device		*dev;
	struct regulator_desc	desc;
	struct regulator_dev	*regulator;
	int max_uV;
	int min_uV;
	int fixed_uV;
	u8 update_bank;
	u8 update_reg;
	u8 update_mask;
	u8 update_val_enable;
	u8 voltage_bank;
	u8 voltage_reg;
	u8 voltage_mask;
	int const *voltages;
	int voltages_len;
	unsigned int delay;
};

static const int ldo_vauxn_voltages[] = {
	1100000,
	1200000,
	1300000,
	1400000,
	1500000,
	1800000,
	1850000,
	1900000,
	2500000,
	2650000,
	2700000,
	2750000,
	2800000,
	2900000,
	3000000,
	3300000,
};

static const int ldo_vaux3_voltages[] = {
	1200000,
	1500000,
	1800000,
	2100000,
	2500000,
	2750000,
	2790000,
	2910000,
};

static const int ldo_vintcore_voltages[] = {
	1200000,
	1225000,
	1250000,
	1275000,
	1300000,
	1325000,
	1350000,
};

static int ab8500_regulator_enable(struct regulator_dev *rdev)
{
	int ret;
	struct ab8500_regulator_info *info = rdev_get_drvdata(rdev);

	if (info == NULL) {
		dev_err(rdev_get_dev(rdev), "regulator info null pointer\n");
		return -EINVAL;
	}

	ret = abx500_mask_and_set_register_interruptible(info->dev,
		info->update_bank, info->update_reg,
		info->update_mask, info->update_val_enable);
	if (ret < 0)
		dev_err(rdev_get_dev(rdev),
			"couldn't set enable bits for regulator\n");

	dev_vdbg(rdev_get_dev(rdev),
		"%s-enable (bank, reg, mask, value): 0x%x, 0x%x, 0x%x, 0x%x\n",
		info->desc.name, info->update_bank, info->update_reg,
		info->update_mask, info->update_val_enable);

	return ret;
}

static int ab8500_regulator_disable(struct regulator_dev *rdev)
{
	int ret;
	struct ab8500_regulator_info *info = rdev_get_drvdata(rdev);

	if (info == NULL) {
		dev_err(rdev_get_dev(rdev), "regulator info null pointer\n");
		return -EINVAL;
	}

	ret = abx500_mask_and_set_register_interruptible(info->dev,
		info->update_bank, info->update_reg,
		info->update_mask, 0x0);
	if (ret < 0)
		dev_err(rdev_get_dev(rdev),
			"couldn't set disable bits for regulator\n");

	dev_vdbg(rdev_get_dev(rdev),
		"%s-disable (bank, reg, mask, value): 0x%x, 0x%x, 0x%x, 0x%x\n",
		info->desc.name, info->update_bank, info->update_reg,
		info->update_mask, 0x0);

	return ret;
}

static int ab8500_regulator_is_enabled(struct regulator_dev *rdev)
{
	int ret;
	struct ab8500_regulator_info *info = rdev_get_drvdata(rdev);
	u8 regval;

	if (info == NULL) {
		dev_err(rdev_get_dev(rdev), "regulator info null pointer\n");
		return -EINVAL;
	}

	ret = abx500_get_register_interruptible(info->dev,
		info->update_bank, info->update_reg, &regval);
	if (ret < 0) {
		dev_err(rdev_get_dev(rdev),
			"couldn't read 0x%x register\n", info->update_reg);
		return ret;
	}

	dev_vdbg(rdev_get_dev(rdev),
		"%s-is_enabled (bank, reg, mask, value): 0x%x, 0x%x, 0x%x,"
		" 0x%x\n",
		info->desc.name, info->update_bank, info->update_reg,
		info->update_mask, regval);

	if (regval & info->update_mask)
		return true;
	else
		return false;
}

static int ab8500_list_voltage(struct regulator_dev *rdev, unsigned selector)
{
	struct ab8500_regulator_info *info = rdev_get_drvdata(rdev);

	if (info == NULL) {
		dev_err(rdev_get_dev(rdev), "regulator info null pointer\n");
		return -EINVAL;
	}

	
	if (info->fixed_uV)
		return info->fixed_uV;

	if (selector >= info->voltages_len)
		return -EINVAL;

	return info->voltages[selector];
}

static int ab8500_regulator_get_voltage_sel(struct regulator_dev *rdev)
{
	int ret, val;
	struct ab8500_regulator_info *info = rdev_get_drvdata(rdev);
	u8 regval;

	if (info == NULL) {
		dev_err(rdev_get_dev(rdev), "regulator info null pointer\n");
		return -EINVAL;
	}

	ret = abx500_get_register_interruptible(info->dev,
			info->voltage_bank, info->voltage_reg, &regval);
	if (ret < 0) {
		dev_err(rdev_get_dev(rdev),
			"couldn't read voltage reg for regulator\n");
		return ret;
	}

	dev_vdbg(rdev_get_dev(rdev),
		"%s-get_voltage (bank, reg, mask, value): 0x%x, 0x%x, 0x%x,"
		" 0x%x\n",
		info->desc.name, info->voltage_bank, info->voltage_reg,
		info->voltage_mask, regval);

	
	val = regval & info->voltage_mask;
	if (info->desc.id == AB8500_LDO_INTCORE)
		return val >> 0x3;
	else
		return val;
}

static int ab8500_get_best_voltage_index(struct regulator_dev *rdev,
		int min_uV, int max_uV)
{
	struct ab8500_regulator_info *info = rdev_get_drvdata(rdev);
	int i;

	
	for (i = 0; i < info->voltages_len; i++) {
		if ((info->voltages[i] >= min_uV) &&
		    (info->voltages[i] <= max_uV))
			return i;
	}

	return -EINVAL;
}

static int ab8500_regulator_set_voltage(struct regulator_dev *rdev,
					int min_uV, int max_uV,
					unsigned *selector)
{
	int ret;
	struct ab8500_regulator_info *info = rdev_get_drvdata(rdev);
	u8 regval;

	if (info == NULL) {
		dev_err(rdev_get_dev(rdev), "regulator info null pointer\n");
		return -EINVAL;
	}

	
	ret = ab8500_get_best_voltage_index(rdev, min_uV, max_uV);
	if (ret < 0) {
		dev_err(rdev_get_dev(rdev),
				"couldn't get best voltage for regulator\n");
		return ret;
	}

	*selector = ret;

	
	regval = (u8)ret;
	ret = abx500_mask_and_set_register_interruptible(info->dev,
			info->voltage_bank, info->voltage_reg,
			info->voltage_mask, regval);
	if (ret < 0)
		dev_err(rdev_get_dev(rdev),
		"couldn't set voltage reg for regulator\n");

	dev_vdbg(rdev_get_dev(rdev),
		"%s-set_voltage (bank, reg, mask, value): 0x%x, 0x%x, 0x%x,"
		" 0x%x\n",
		info->desc.name, info->voltage_bank, info->voltage_reg,
		info->voltage_mask, regval);

	return ret;
}

static int ab8500_regulator_enable_time(struct regulator_dev *rdev)
{
	struct ab8500_regulator_info *info = rdev_get_drvdata(rdev);

	return info->delay;
}

static int ab8500_regulator_set_voltage_time_sel(struct regulator_dev *rdev,
					     unsigned int old_sel,
					     unsigned int new_sel)
{
	struct ab8500_regulator_info *info = rdev_get_drvdata(rdev);
	int ret;

	
	ret = ab8500_regulator_is_enabled(rdev);
	if (ret < 0)
		return ret;
	if (!ret)
		return 0;
	return info->delay;
}

static struct regulator_ops ab8500_regulator_ops = {
	.enable		= ab8500_regulator_enable,
	.disable	= ab8500_regulator_disable,
	.is_enabled	= ab8500_regulator_is_enabled,
	.get_voltage_sel = ab8500_regulator_get_voltage_sel,
	.set_voltage	= ab8500_regulator_set_voltage,
	.list_voltage	= ab8500_list_voltage,
	.enable_time	= ab8500_regulator_enable_time,
	.set_voltage_time_sel = ab8500_regulator_set_voltage_time_sel,
};

static int ab8500_fixed_get_voltage(struct regulator_dev *rdev)
{
	struct ab8500_regulator_info *info = rdev_get_drvdata(rdev);

	if (info == NULL) {
		dev_err(rdev_get_dev(rdev), "regulator info null pointer\n");
		return -EINVAL;
	}

	return info->fixed_uV;
}

static struct regulator_ops ab8500_regulator_fixed_ops = {
	.enable		= ab8500_regulator_enable,
	.disable	= ab8500_regulator_disable,
	.is_enabled	= ab8500_regulator_is_enabled,
	.get_voltage	= ab8500_fixed_get_voltage,
	.list_voltage	= ab8500_list_voltage,
	.enable_time	= ab8500_regulator_enable_time,
	.set_voltage_time_sel = ab8500_regulator_set_voltage_time_sel,
};

static struct ab8500_regulator_info
		ab8500_regulator_info[AB8500_NUM_REGULATORS] = {
	[AB8500_LDO_AUX1] = {
		.desc = {
			.name		= "LDO-AUX1",
			.ops		= &ab8500_regulator_ops,
			.type		= REGULATOR_VOLTAGE,
			.id		= AB8500_LDO_AUX1,
			.owner		= THIS_MODULE,
			.n_voltages	= ARRAY_SIZE(ldo_vauxn_voltages),
		},
		.min_uV			= 1100000,
		.max_uV			= 3300000,
		.update_bank		= 0x04,
		.update_reg		= 0x09,
		.update_mask		= 0x03,
		.update_val_enable	= 0x01,
		.voltage_bank		= 0x04,
		.voltage_reg		= 0x1f,
		.voltage_mask		= 0x0f,
		.voltages		= ldo_vauxn_voltages,
		.voltages_len		= ARRAY_SIZE(ldo_vauxn_voltages),
	},
	[AB8500_LDO_AUX2] = {
		.desc = {
			.name		= "LDO-AUX2",
			.ops		= &ab8500_regulator_ops,
			.type		= REGULATOR_VOLTAGE,
			.id		= AB8500_LDO_AUX2,
			.owner		= THIS_MODULE,
			.n_voltages	= ARRAY_SIZE(ldo_vauxn_voltages),
		},
		.min_uV			= 1100000,
		.max_uV			= 3300000,
		.update_bank		= 0x04,
		.update_reg		= 0x09,
		.update_mask		= 0x0c,
		.update_val_enable	= 0x04,
		.voltage_bank		= 0x04,
		.voltage_reg		= 0x20,
		.voltage_mask		= 0x0f,
		.voltages		= ldo_vauxn_voltages,
		.voltages_len		= ARRAY_SIZE(ldo_vauxn_voltages),
	},
	[AB8500_LDO_AUX3] = {
		.desc = {
			.name		= "LDO-AUX3",
			.ops		= &ab8500_regulator_ops,
			.type		= REGULATOR_VOLTAGE,
			.id		= AB8500_LDO_AUX3,
			.owner		= THIS_MODULE,
			.n_voltages	= ARRAY_SIZE(ldo_vaux3_voltages),
		},
		.min_uV			= 1100000,
		.max_uV			= 3300000,
		.update_bank		= 0x04,
		.update_reg		= 0x0a,
		.update_mask		= 0x03,
		.update_val_enable	= 0x01,
		.voltage_bank		= 0x04,
		.voltage_reg		= 0x21,
		.voltage_mask		= 0x07,
		.voltages		= ldo_vaux3_voltages,
		.voltages_len		= ARRAY_SIZE(ldo_vaux3_voltages),
	},
	[AB8500_LDO_INTCORE] = {
		.desc = {
			.name		= "LDO-INTCORE",
			.ops		= &ab8500_regulator_ops,
			.type		= REGULATOR_VOLTAGE,
			.id		= AB8500_LDO_INTCORE,
			.owner		= THIS_MODULE,
			.n_voltages	= ARRAY_SIZE(ldo_vintcore_voltages),
		},
		.min_uV			= 1100000,
		.max_uV			= 3300000,
		.update_bank		= 0x03,
		.update_reg		= 0x80,
		.update_mask		= 0x44,
		.update_val_enable	= 0x04,
		.voltage_bank		= 0x03,
		.voltage_reg		= 0x80,
		.voltage_mask		= 0x38,
		.voltages		= ldo_vintcore_voltages,
		.voltages_len		= ARRAY_SIZE(ldo_vintcore_voltages),
	},

	[AB8500_LDO_TVOUT] = {
		.desc = {
			.name		= "LDO-TVOUT",
			.ops		= &ab8500_regulator_fixed_ops,
			.type		= REGULATOR_VOLTAGE,
			.id		= AB8500_LDO_TVOUT,
			.owner		= THIS_MODULE,
			.n_voltages	= 1,
		},
		.delay			= 10000,
		.fixed_uV		= 2000000,
		.update_bank		= 0x03,
		.update_reg		= 0x80,
		.update_mask		= 0x82,
		.update_val_enable	= 0x02,
	},
	[AB8500_LDO_USB] = {
		.desc = {
			.name           = "LDO-USB",
			.ops            = &ab8500_regulator_fixed_ops,
			.type           = REGULATOR_VOLTAGE,
			.id             = AB8500_LDO_USB,
			.owner          = THIS_MODULE,
			.n_voltages     = 1,
		},
		.fixed_uV               = 3300000,
		.update_bank            = 0x03,
		.update_reg             = 0x82,
		.update_mask            = 0x03,
		.update_val_enable      = 0x01,
	},
	[AB8500_LDO_AUDIO] = {
		.desc = {
			.name		= "LDO-AUDIO",
			.ops		= &ab8500_regulator_fixed_ops,
			.type		= REGULATOR_VOLTAGE,
			.id		= AB8500_LDO_AUDIO,
			.owner		= THIS_MODULE,
			.n_voltages	= 1,
		},
		.fixed_uV		= 2000000,
		.update_bank		= 0x03,
		.update_reg		= 0x83,
		.update_mask		= 0x02,
		.update_val_enable	= 0x02,
	},
	[AB8500_LDO_ANAMIC1] = {
		.desc = {
			.name		= "LDO-ANAMIC1",
			.ops		= &ab8500_regulator_fixed_ops,
			.type		= REGULATOR_VOLTAGE,
			.id		= AB8500_LDO_ANAMIC1,
			.owner		= THIS_MODULE,
			.n_voltages	= 1,
		},
		.fixed_uV		= 2050000,
		.update_bank		= 0x03,
		.update_reg		= 0x83,
		.update_mask		= 0x08,
		.update_val_enable	= 0x08,
	},
	[AB8500_LDO_ANAMIC2] = {
		.desc = {
			.name		= "LDO-ANAMIC2",
			.ops		= &ab8500_regulator_fixed_ops,
			.type		= REGULATOR_VOLTAGE,
			.id		= AB8500_LDO_ANAMIC2,
			.owner		= THIS_MODULE,
			.n_voltages	= 1,
		},
		.fixed_uV		= 2050000,
		.update_bank		= 0x03,
		.update_reg		= 0x83,
		.update_mask		= 0x10,
		.update_val_enable	= 0x10,
	},
	[AB8500_LDO_DMIC] = {
		.desc = {
			.name		= "LDO-DMIC",
			.ops		= &ab8500_regulator_fixed_ops,
			.type		= REGULATOR_VOLTAGE,
			.id		= AB8500_LDO_DMIC,
			.owner		= THIS_MODULE,
			.n_voltages	= 1,
		},
		.fixed_uV		= 1800000,
		.update_bank		= 0x03,
		.update_reg		= 0x83,
		.update_mask		= 0x04,
		.update_val_enable	= 0x04,
	},
	[AB8500_LDO_ANA] = {
		.desc = {
			.name		= "LDO-ANA",
			.ops		= &ab8500_regulator_fixed_ops,
			.type		= REGULATOR_VOLTAGE,
			.id		= AB8500_LDO_ANA,
			.owner		= THIS_MODULE,
			.n_voltages	= 1,
		},
		.fixed_uV		= 1200000,
		.update_bank		= 0x04,
		.update_reg		= 0x06,
		.update_mask		= 0x0c,
		.update_val_enable	= 0x04,
	},


};

struct ab8500_reg_init {
	u8 bank;
	u8 addr;
	u8 mask;
};

#define REG_INIT(_id, _bank, _addr, _mask)	\
	[_id] = {				\
		.bank = _bank,			\
		.addr = _addr,			\
		.mask = _mask,			\
	}

static struct ab8500_reg_init ab8500_reg_init[] = {
	REG_INIT(AB8500_REGUREQUESTCTRL2,	0x03, 0x04, 0xfc),
	REG_INIT(AB8500_REGUREQUESTCTRL3,	0x03, 0x05, 0xff),
	REG_INIT(AB8500_REGUREQUESTCTRL4,	0x03, 0x06, 0x07),
	REG_INIT(AB8500_REGUSYSCLKREQ1HPVALID1,	0x03, 0x07, 0xe8),
	REG_INIT(AB8500_REGUSYSCLKREQ1HPVALID2,	0x03, 0x08, 0x70),
	REG_INIT(AB8500_REGUHWHPREQ1VALID1,	0x03, 0x09, 0xe8),
	REG_INIT(AB8500_REGUHWHPREQ1VALID2,	0x03, 0x0a, 0x07),
	REG_INIT(AB8500_REGUHWHPREQ2VALID1,	0x03, 0x0b, 0xe8),
	REG_INIT(AB8500_REGUHWHPREQ2VALID2,	0x03, 0x0c, 0x07),
	REG_INIT(AB8500_REGUSWHPREQVALID1,	0x03, 0x0d, 0xa0),
	REG_INIT(AB8500_REGUSWHPREQVALID2,	0x03, 0x0e, 0x1f),
	REG_INIT(AB8500_REGUSYSCLKREQVALID1,	0x03, 0x0f, 0xfe),
	REG_INIT(AB8500_REGUSYSCLKREQVALID2,	0x03, 0x10, 0xfe),
	REG_INIT(AB8500_REGUMISC1,		0x03, 0x80, 0xfe),
	REG_INIT(AB8500_VAUDIOSUPPLY,		0x03, 0x83, 0x1e),
	REG_INIT(AB8500_REGUCTRL1VAMIC,		0x03, 0x84, 0x03),
	REG_INIT(AB8500_VPLLVANAREGU,		0x04, 0x06, 0x0f),
	REG_INIT(AB8500_VREFDDR,		0x04, 0x07, 0x03),
	REG_INIT(AB8500_EXTSUPPLYREGU,		0x04, 0x08, 0xff),
	REG_INIT(AB8500_VAUX12REGU,		0x04, 0x09, 0x0f),
	REG_INIT(AB8500_VRF1VAUX3REGU,		0x04, 0x0a, 0x03),
	REG_INIT(AB8500_VSMPS1SEL1,		0x04, 0x13, 0x3f),
	REG_INIT(AB8500_VAUX1SEL,		0x04, 0x1f, 0x0f),
	REG_INIT(AB8500_VAUX2SEL,		0x04, 0x20, 0x0f),
	REG_INIT(AB8500_VRF1VAUX3SEL,		0x04, 0x21, 0x07),
	REG_INIT(AB8500_REGUCTRL2SPARE,		0x04, 0x22, 0x01),
	REG_INIT(AB8500_REGUCTRLDISCH,		0x04, 0x43, 0xfc),
	REG_INIT(AB8500_REGUCTRLDISCH2,		0x04, 0x44, 0x16),
};

static __devinit int ab8500_regulator_probe(struct platform_device *pdev)
{
	struct ab8500 *ab8500 = dev_get_drvdata(pdev->dev.parent);
	struct ab8500_platform_data *pdata;
	int i, err;

	if (!ab8500) {
		dev_err(&pdev->dev, "null mfd parent\n");
		return -EINVAL;
	}
	pdata = dev_get_platdata(ab8500->dev);
	if (!pdata) {
		dev_err(&pdev->dev, "null pdata\n");
		return -EINVAL;
	}

	
	if (pdata->num_regulator != ARRAY_SIZE(ab8500_regulator_info)) {
		dev_err(&pdev->dev, "Configuration error: size mismatch.\n");
		return -EINVAL;
	}

	
	for (i = 0; i < pdata->num_regulator_reg_init; i++) {
		int id;
		u8 value;

		id = pdata->regulator_reg_init[i].id;
		value = pdata->regulator_reg_init[i].value;

		
		if (id >= AB8500_NUM_REGULATOR_REGISTERS) {
			dev_err(&pdev->dev,
				"Configuration error: id outside range.\n");
			return -EINVAL;
		}
		if (value & ~ab8500_reg_init[id].mask) {
			dev_err(&pdev->dev,
				"Configuration error: value outside mask.\n");
			return -EINVAL;
		}

		
		err = abx500_mask_and_set_register_interruptible(&pdev->dev,
			ab8500_reg_init[id].bank,
			ab8500_reg_init[id].addr,
			ab8500_reg_init[id].mask,
			value);
		if (err < 0) {
			dev_err(&pdev->dev,
				"Failed to initialize 0x%02x, 0x%02x.\n",
				ab8500_reg_init[id].bank,
				ab8500_reg_init[id].addr);
			return err;
		}
		dev_vdbg(&pdev->dev,
			"  init: 0x%02x, 0x%02x, 0x%02x, 0x%02x\n",
			ab8500_reg_init[id].bank,
			ab8500_reg_init[id].addr,
			ab8500_reg_init[id].mask,
			value);
	}

	
	for (i = 0; i < ARRAY_SIZE(ab8500_regulator_info); i++) {
		struct ab8500_regulator_info *info = NULL;

		
		info = &ab8500_regulator_info[i];
		info->dev = &pdev->dev;

		
		if (abx500_get_chip_id(info->dev) < 0x20) {
			if (info->desc.id == AB8500_LDO_AUX3) {
				info->desc.n_voltages =
					ARRAY_SIZE(ldo_vauxn_voltages);
				info->voltages = ldo_vauxn_voltages;
				info->voltages_len =
					ARRAY_SIZE(ldo_vauxn_voltages);
				info->voltage_mask = 0xf;
			}
		}

		
		info->regulator = regulator_register(&info->desc, &pdev->dev,
				&pdata->regulator[i], info, NULL);
		if (IS_ERR(info->regulator)) {
			err = PTR_ERR(info->regulator);
			dev_err(&pdev->dev, "failed to register regulator %s\n",
					info->desc.name);
			
			while (--i >= 0) {
				info = &ab8500_regulator_info[i];
				regulator_unregister(info->regulator);
			}
			return err;
		}

		dev_vdbg(rdev_get_dev(info->regulator),
			"%s-probed\n", info->desc.name);
	}

	return 0;
}

static __devexit int ab8500_regulator_remove(struct platform_device *pdev)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(ab8500_regulator_info); i++) {
		struct ab8500_regulator_info *info = NULL;
		info = &ab8500_regulator_info[i];

		dev_vdbg(rdev_get_dev(info->regulator),
			"%s-remove\n", info->desc.name);

		regulator_unregister(info->regulator);
	}

	return 0;
}

static struct platform_driver ab8500_regulator_driver = {
	.probe = ab8500_regulator_probe,
	.remove = __devexit_p(ab8500_regulator_remove),
	.driver         = {
		.name   = "ab8500-regulator",
		.owner  = THIS_MODULE,
	},
};

static int __init ab8500_regulator_init(void)
{
	int ret;

	ret = platform_driver_register(&ab8500_regulator_driver);
	if (ret != 0)
		pr_err("Failed to register ab8500 regulator: %d\n", ret);

	return ret;
}
subsys_initcall(ab8500_regulator_init);

static void __exit ab8500_regulator_exit(void)
{
	platform_driver_unregister(&ab8500_regulator_driver);
}
module_exit(ab8500_regulator_exit);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Sundar Iyer <sundar.iyer@stericsson.com>");
MODULE_DESCRIPTION("Regulator Driver for ST-Ericsson AB8500 Mixed-Sig PMIC");
MODULE_ALIAS("platform:ab8500-regulator");
