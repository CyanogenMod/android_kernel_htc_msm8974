/*
 * 1-wire busmaster driver for DS1WM and ASICs with embedded DS1WMs
 * such as HP iPAQs (including h5xxx, h2200, and devices with ASIC3
 * like hx4700).
 *
 * Copyright (c) 2004-2005, Szabolcs Gyurko <szabolcs.gyurko@tlt.hu>
 * Copyright (c) 2004-2007, Matt Reimer <mreimer@vpop.net>
 *
 * Use consistent with the GNU GPL is permitted,
 * provided that this copyright notice is
 * preserved in its entirety in all copies and derived works.
 */

#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/pm.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/mfd/core.h>
#include <linux/mfd/ds1wm.h>
#include <linux/slab.h>

#include <asm/io.h>

#include "../w1.h"
#include "../w1_int.h"


#define DS1WM_CMD	0x00	
#define DS1WM_DATA	0x01	
#define DS1WM_INT	0x02	
#define DS1WM_INT_EN	0x03	
#define DS1WM_CLKDIV	0x04	
#define DS1WM_CNTRL	0x05	

#define DS1WM_CMD_1W_RESET  (1 << 0)	
#define DS1WM_CMD_SRA	    (1 << 1)	
#define DS1WM_CMD_DQ_OUTPUT (1 << 2)	
#define DS1WM_CMD_DQ_INPUT  (1 << 3)	
#define DS1WM_CMD_RST	    (1 << 5)	
#define DS1WM_CMD_OD	    (1 << 7)	

#define DS1WM_INT_PD	    (1 << 0)	
#define DS1WM_INT_PDR	    (1 << 1)	
#define DS1WM_INT_TBE	    (1 << 2)	
#define DS1WM_INT_TSRE	    (1 << 3)	
#define DS1WM_INT_RBF	    (1 << 4)	
#define DS1WM_INT_RSRF	    (1 << 5)	

#define DS1WM_INTEN_EPD	    (1 << 0)	
#define DS1WM_INTEN_IAS	    (1 << 1)	
#define DS1WM_INTEN_ETBE    (1 << 2)	
#define DS1WM_INTEN_ETMT    (1 << 3)	
#define DS1WM_INTEN_ERBF    (1 << 4)	
#define DS1WM_INTEN_ERSRF   (1 << 5)	
#define DS1WM_INTEN_DQO	    (1 << 6)	

#define DS1WM_INTEN_NOT_IAS (~DS1WM_INTEN_IAS)	

#define DS1WM_TIMEOUT (HZ * 5)

static struct {
	unsigned long freq;
	unsigned long divisor;
} freq[] = {
	{   1000000, 0x80 },
	{   2000000, 0x84 },
	{   3000000, 0x81 },
	{   4000000, 0x88 },
	{   5000000, 0x82 },
	{   6000000, 0x85 },
	{   7000000, 0x83 },
	{   8000000, 0x8c },
	{  10000000, 0x86 },
	{  12000000, 0x89 },
	{  14000000, 0x87 },
	{  16000000, 0x90 },
	{  20000000, 0x8a },
	{  24000000, 0x8d },
	{  28000000, 0x8b },
	{  32000000, 0x94 },
	{  40000000, 0x8e },
	{  48000000, 0x91 },
	{  56000000, 0x8f },
	{  64000000, 0x98 },
	{  80000000, 0x92 },
	{  96000000, 0x95 },
	{ 112000000, 0x93 },
	{ 128000000, 0x9c },
};

struct ds1wm_data {
	void     __iomem *map;
	int      bus_shift; 
	struct platform_device *pdev;
	const struct mfd_cell   *cell;
	int      irq;
	int      slave_present;
	void     *reset_complete;
	void     *read_complete;
	void     *write_complete;
	int      read_error;
	
	u8       read_byte;
	
	
	u8       int_en_reg_none;
	unsigned int reset_recover_delay; 
};

static inline void ds1wm_write_register(struct ds1wm_data *ds1wm_data, u32 reg,
					u8 val)
{
	__raw_writeb(val, ds1wm_data->map + (reg << ds1wm_data->bus_shift));
}

static inline u8 ds1wm_read_register(struct ds1wm_data *ds1wm_data, u32 reg)
{
	return __raw_readb(ds1wm_data->map + (reg << ds1wm_data->bus_shift));
}


static irqreturn_t ds1wm_isr(int isr, void *data)
{
	struct ds1wm_data *ds1wm_data = data;
	u8 intr;
	u8 inten = ds1wm_read_register(ds1wm_data, DS1WM_INT_EN);
	if (!(inten & DS1WM_INTEN_NOT_IAS))
		return IRQ_NONE;

	ds1wm_write_register(ds1wm_data,
		DS1WM_INT_EN, ds1wm_data->int_en_reg_none);

	
	intr = ds1wm_read_register(ds1wm_data, DS1WM_INT);

	ds1wm_data->slave_present = (intr & DS1WM_INT_PDR) ? 0 : 1;

	if ((intr & DS1WM_INT_TSRE) && ds1wm_data->write_complete) {
		inten &= ~DS1WM_INTEN_ETMT;
		complete(ds1wm_data->write_complete);
	}
	if (intr & DS1WM_INT_RBF) {
		
		ds1wm_data->read_byte = ds1wm_read_register(ds1wm_data,
		DS1WM_DATA);
		inten &= ~DS1WM_INTEN_ERBF;
		if (ds1wm_data->read_complete)
			complete(ds1wm_data->read_complete);
	}
	if ((intr & DS1WM_INT_PD) && ds1wm_data->reset_complete) {
		inten &= ~DS1WM_INTEN_EPD;
		complete(ds1wm_data->reset_complete);
	}

	ds1wm_write_register(ds1wm_data, DS1WM_INT_EN, inten);
	return IRQ_HANDLED;
}

static int ds1wm_reset(struct ds1wm_data *ds1wm_data)
{
	unsigned long timeleft;
	DECLARE_COMPLETION_ONSTACK(reset_done);

	ds1wm_data->reset_complete = &reset_done;

	
	ds1wm_write_register(ds1wm_data, DS1WM_INT_EN, DS1WM_INTEN_EPD |
	ds1wm_data->int_en_reg_none);

	ds1wm_write_register(ds1wm_data, DS1WM_CMD, DS1WM_CMD_1W_RESET);

	timeleft = wait_for_completion_timeout(&reset_done, DS1WM_TIMEOUT);
	ds1wm_data->reset_complete = NULL;
	if (!timeleft) {
		dev_err(&ds1wm_data->pdev->dev, "reset failed, timed out\n");
		return 1;
	}

	if (!ds1wm_data->slave_present) {
		dev_dbg(&ds1wm_data->pdev->dev, "reset: no devices found\n");
		return 1;
	}

	if (ds1wm_data->reset_recover_delay)
		msleep(ds1wm_data->reset_recover_delay);

	return 0;
}

static int ds1wm_write(struct ds1wm_data *ds1wm_data, u8 data)
{
	unsigned long timeleft;
	DECLARE_COMPLETION_ONSTACK(write_done);
	ds1wm_data->write_complete = &write_done;

	ds1wm_write_register(ds1wm_data, DS1WM_INT_EN,
	ds1wm_data->int_en_reg_none | DS1WM_INTEN_ETMT);

	ds1wm_write_register(ds1wm_data, DS1WM_DATA, data);

	timeleft = wait_for_completion_timeout(&write_done, DS1WM_TIMEOUT);

	ds1wm_data->write_complete = NULL;
	if (!timeleft) {
		dev_err(&ds1wm_data->pdev->dev, "write failed, timed out\n");
		return -ETIMEDOUT;
	}

	return 0;
}

static u8 ds1wm_read(struct ds1wm_data *ds1wm_data, unsigned char write_data)
{
	unsigned long timeleft;
	u8 intEnable = DS1WM_INTEN_ERBF | ds1wm_data->int_en_reg_none;
	DECLARE_COMPLETION_ONSTACK(read_done);

	ds1wm_read_register(ds1wm_data, DS1WM_DATA);

	ds1wm_data->read_complete = &read_done;
	ds1wm_write_register(ds1wm_data, DS1WM_INT_EN, intEnable);

	ds1wm_write_register(ds1wm_data, DS1WM_DATA, write_data);
	timeleft = wait_for_completion_timeout(&read_done, DS1WM_TIMEOUT);

	ds1wm_data->read_complete = NULL;
	if (!timeleft) {
		dev_err(&ds1wm_data->pdev->dev, "read failed, timed out\n");
		ds1wm_data->read_error = -ETIMEDOUT;
		return 0xFF;
	}
	ds1wm_data->read_error = 0;
	return ds1wm_data->read_byte;
}

static int ds1wm_find_divisor(int gclk)
{
	int i;

	for (i = ARRAY_SIZE(freq)-1; i >= 0; --i)
		if (gclk >= freq[i].freq)
			return freq[i].divisor;

	return 0;
}

static void ds1wm_up(struct ds1wm_data *ds1wm_data)
{
	int divisor;
	struct ds1wm_driver_data *plat = ds1wm_data->pdev->dev.platform_data;

	if (ds1wm_data->cell->enable)
		ds1wm_data->cell->enable(ds1wm_data->pdev);

	divisor = ds1wm_find_divisor(plat->clock_rate);
	dev_dbg(&ds1wm_data->pdev->dev,
		"found divisor 0x%x for clock %d\n", divisor, plat->clock_rate);
	if (divisor == 0) {
		dev_err(&ds1wm_data->pdev->dev,
			"no suitable divisor for %dHz clock\n",
			plat->clock_rate);
		return;
	}
	ds1wm_write_register(ds1wm_data, DS1WM_CLKDIV, divisor);

	
	msleep(1);

	ds1wm_reset(ds1wm_data);
}

static void ds1wm_down(struct ds1wm_data *ds1wm_data)
{
	ds1wm_reset(ds1wm_data);

	
	ds1wm_write_register(ds1wm_data, DS1WM_INT_EN,
		ds1wm_data->int_en_reg_none);

	if (ds1wm_data->cell->disable)
		ds1wm_data->cell->disable(ds1wm_data->pdev);
}


static u8 ds1wm_read_byte(void *data)
{
	struct ds1wm_data *ds1wm_data = data;

	return ds1wm_read(ds1wm_data, 0xff);
}

static void ds1wm_write_byte(void *data, u8 byte)
{
	struct ds1wm_data *ds1wm_data = data;

	ds1wm_write(ds1wm_data, byte);
}

static u8 ds1wm_reset_bus(void *data)
{
	struct ds1wm_data *ds1wm_data = data;

	ds1wm_reset(ds1wm_data);

	return 0;
}

static void ds1wm_search(void *data, struct w1_master *master_dev,
			u8 search_type, w1_slave_found_callback slave_found)
{
	struct ds1wm_data *ds1wm_data = data;
	int i;
	int ms_discrep_bit = -1;
	u64 r = 0; 
	u64 r_prime, d;
	unsigned slaves_found = 0;
	unsigned int pass = 0;

	dev_dbg(&ds1wm_data->pdev->dev, "search begin\n");
	while (true) {
		++pass;
		if (pass > 100) {
			dev_dbg(&ds1wm_data->pdev->dev,
				"too many attempts (100), search aborted\n");
			return;
		}

		if (ds1wm_reset(ds1wm_data)) {
			dev_dbg(&ds1wm_data->pdev->dev,
				"pass: %d reset error (or no slaves)\n", pass);
			break;
		}

		dev_dbg(&ds1wm_data->pdev->dev,
			"pass: %d r : %0#18llx writing SEARCH_ROM\n", pass, r);
		ds1wm_write(ds1wm_data, search_type);
		dev_dbg(&ds1wm_data->pdev->dev,
			"pass: %d entering ASM\n", pass);
		ds1wm_write_register(ds1wm_data, DS1WM_CMD, DS1WM_CMD_SRA);
		dev_dbg(&ds1wm_data->pdev->dev,
			"pass: %d begining nibble loop\n", pass);

		r_prime = 0;
		d = 0;
		
		
		for (i = 0; i < 16; i++) {

			unsigned char resp, _r, _r_prime, _d;

			_r = (r >> (4*i)) & 0xf;
			_r = ((_r & 0x1) << 1) |
			((_r & 0x2) << 2) |
			((_r & 0x4) << 3) |
			((_r & 0x8) << 4);

			
			resp = ds1wm_read(ds1wm_data, _r);

			if (ds1wm_data->read_error) {
				dev_err(&ds1wm_data->pdev->dev,
				"pass: %d nibble: %d read error\n", pass, i);
				break;
			}

			_r_prime = ((resp & 0x02) >> 1) |
			((resp & 0x08) >> 2) |
			((resp & 0x20) >> 3) |
			((resp & 0x80) >> 4);

			_d = ((resp & 0x01) >> 0) |
			((resp & 0x04) >> 1) |
			((resp & 0x10) >> 2) |
			((resp & 0x40) >> 3);

			r_prime |= (unsigned long long) _r_prime << (i * 4);
			d |= (unsigned long long) _d << (i * 4);

		}
		if (ds1wm_data->read_error) {
			dev_err(&ds1wm_data->pdev->dev,
				"pass: %d read error, retrying\n", pass);
			break;
		}
		dev_dbg(&ds1wm_data->pdev->dev,
			"pass: %d r\': %0#18llx d:%0#18llx\n",
			pass, r_prime, d);
		dev_dbg(&ds1wm_data->pdev->dev,
			"pass: %d nibble loop complete, exiting ASM\n", pass);
		ds1wm_write_register(ds1wm_data, DS1WM_CMD, ~DS1WM_CMD_SRA);
		dev_dbg(&ds1wm_data->pdev->dev,
			"pass: %d resetting bus\n", pass);
		ds1wm_reset(ds1wm_data);
		if ((r_prime & ((u64)1 << 63)) && (d & ((u64)1 << 63))) {
			dev_err(&ds1wm_data->pdev->dev,
				"pass: %d bus error, retrying\n", pass);
			continue; 
		}


		dev_dbg(&ds1wm_data->pdev->dev,
			"pass: %d found %0#18llx\n", pass, r_prime);
		slave_found(master_dev, r_prime);
		++slaves_found;
		dev_dbg(&ds1wm_data->pdev->dev,
			"pass: %d complete, preparing next pass\n", pass);

		d &= ~r;
		
		ms_discrep_bit = fls64(d) - 1;
		dev_dbg(&ds1wm_data->pdev->dev,
			"pass: %d new d:%0#18llx MS discrep bit:%d\n",
			pass, d, ms_discrep_bit);

		if (ms_discrep_bit == -1)
			break;

		r = (r &  ~(~0ull << (ms_discrep_bit))) | 1 << ms_discrep_bit;
	} 
	dev_dbg(&ds1wm_data->pdev->dev,
		"pass: %d total: %d search done ms d bit pos: %d\n", pass,
		slaves_found, ms_discrep_bit);
}


static struct w1_bus_master ds1wm_master = {
	.read_byte  = ds1wm_read_byte,
	.write_byte = ds1wm_write_byte,
	.reset_bus  = ds1wm_reset_bus,
	.search	    = ds1wm_search,
};

static int ds1wm_probe(struct platform_device *pdev)
{
	struct ds1wm_data *ds1wm_data;
	struct ds1wm_driver_data *plat;
	struct resource *res;
	int ret;

	if (!pdev)
		return -ENODEV;

	ds1wm_data = kzalloc(sizeof(*ds1wm_data), GFP_KERNEL);
	if (!ds1wm_data)
		return -ENOMEM;

	platform_set_drvdata(pdev, ds1wm_data);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		ret = -ENXIO;
		goto err0;
	}
	ds1wm_data->map = ioremap(res->start, resource_size(res));
	if (!ds1wm_data->map) {
		ret = -ENOMEM;
		goto err0;
	}

	
	ds1wm_data->bus_shift = resource_size(res) >> 3;

	ds1wm_data->pdev = pdev;
	ds1wm_data->cell = mfd_get_cell(pdev);
	if (!ds1wm_data->cell) {
		ret = -ENODEV;
		goto err1;
	}
	plat = pdev->dev.platform_data;
	if (!plat) {
		ret = -ENODEV;
		goto err1;
	}

	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (!res) {
		ret = -ENXIO;
		goto err1;
	}
	ds1wm_data->irq = res->start;
	ds1wm_data->int_en_reg_none = (plat->active_high ? DS1WM_INTEN_IAS : 0);
	ds1wm_data->reset_recover_delay = plat->reset_recover_delay;

	if (res->flags & IORESOURCE_IRQ_HIGHEDGE)
		irq_set_irq_type(ds1wm_data->irq, IRQ_TYPE_EDGE_RISING);
	if (res->flags & IORESOURCE_IRQ_LOWEDGE)
		irq_set_irq_type(ds1wm_data->irq, IRQ_TYPE_EDGE_FALLING);

	ret = request_irq(ds1wm_data->irq, ds1wm_isr,
			IRQF_DISABLED | IRQF_SHARED, "ds1wm", ds1wm_data);
	if (ret)
		goto err1;

	ds1wm_up(ds1wm_data);

	ds1wm_master.data = (void *)ds1wm_data;

	ret = w1_add_master_device(&ds1wm_master);
	if (ret)
		goto err2;

	return 0;

err2:
	ds1wm_down(ds1wm_data);
	free_irq(ds1wm_data->irq, ds1wm_data);
err1:
	iounmap(ds1wm_data->map);
err0:
	kfree(ds1wm_data);

	return ret;
}

#ifdef CONFIG_PM
static int ds1wm_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct ds1wm_data *ds1wm_data = platform_get_drvdata(pdev);

	ds1wm_down(ds1wm_data);

	return 0;
}

static int ds1wm_resume(struct platform_device *pdev)
{
	struct ds1wm_data *ds1wm_data = platform_get_drvdata(pdev);

	ds1wm_up(ds1wm_data);

	return 0;
}
#else
#define ds1wm_suspend NULL
#define ds1wm_resume NULL
#endif

static int ds1wm_remove(struct platform_device *pdev)
{
	struct ds1wm_data *ds1wm_data = platform_get_drvdata(pdev);

	w1_remove_master_device(&ds1wm_master);
	ds1wm_down(ds1wm_data);
	free_irq(ds1wm_data->irq, ds1wm_data);
	iounmap(ds1wm_data->map);
	kfree(ds1wm_data);

	return 0;
}

static struct platform_driver ds1wm_driver = {
	.driver   = {
		.name = "ds1wm",
	},
	.probe    = ds1wm_probe,
	.remove   = ds1wm_remove,
	.suspend  = ds1wm_suspend,
	.resume   = ds1wm_resume
};

static int __init ds1wm_init(void)
{
	printk("DS1WM w1 busmaster driver - (c) 2004 Szabolcs Gyurko\n");
	return platform_driver_register(&ds1wm_driver);
}

static void __exit ds1wm_exit(void)
{
	platform_driver_unregister(&ds1wm_driver);
}

module_init(ds1wm_init);
module_exit(ds1wm_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Szabolcs Gyurko <szabolcs.gyurko@tlt.hu>, "
	"Matt Reimer <mreimer@vpop.net>,"
	"Jean-Francois Dagenais <dagenaisj@sonatest.com>");
MODULE_DESCRIPTION("DS1WM w1 busmaster driver");
