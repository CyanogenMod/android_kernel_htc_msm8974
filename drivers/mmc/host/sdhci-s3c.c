/* linux/drivers/mmc/host/sdhci-s3c.c
 *
 * Copyright 2008 Openmoko Inc.
 * Copyright 2008 Simtec Electronics
 *      Ben Dooks <ben@simtec.co.uk>
 *      http://armlinux.simtec.co.uk/
 *
 * SDHCI (HSMMC) support for Samsung SoC
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/gpio.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/pm.h>
#include <linux/pm_runtime.h>

#include <linux/mmc/host.h>

#include <plat/sdhci.h>
#include <plat/regs-sdhci.h>

#include "sdhci.h"

#define MAX_BUS_CLK	(4)

struct sdhci_s3c {
	struct sdhci_host	*host;
	struct platform_device	*pdev;
	struct resource		*ioarea;
	struct s3c_sdhci_platdata *pdata;
	unsigned int		cur_clk;
	int			ext_cd_irq;
	int			ext_cd_gpio;

	struct clk		*clk_io;
	struct clk		*clk_bus[MAX_BUS_CLK];
};

struct sdhci_s3c_drv_data {
	unsigned int	sdhci_quirks;
};

static inline struct sdhci_s3c *to_s3c(struct sdhci_host *host)
{
	return sdhci_priv(host);
}

static u32 get_curclk(u32 ctrl2)
{
	ctrl2 &= S3C_SDHCI_CTRL2_SELBASECLK_MASK;
	ctrl2 >>= S3C_SDHCI_CTRL2_SELBASECLK_SHIFT;

	return ctrl2;
}

static void sdhci_s3c_check_sclk(struct sdhci_host *host)
{
	struct sdhci_s3c *ourhost = to_s3c(host);
	u32 tmp = readl(host->ioaddr + S3C_SDHCI_CONTROL2);

	if (get_curclk(tmp) != ourhost->cur_clk) {
		dev_dbg(&ourhost->pdev->dev, "restored ctrl2 clock setting\n");

		tmp &= ~S3C_SDHCI_CTRL2_SELBASECLK_MASK;
		tmp |= ourhost->cur_clk << S3C_SDHCI_CTRL2_SELBASECLK_SHIFT;
		writel(tmp, host->ioaddr + S3C_SDHCI_CONTROL2);
	}
}

static unsigned int sdhci_s3c_get_max_clk(struct sdhci_host *host)
{
	struct sdhci_s3c *ourhost = to_s3c(host);
	struct clk *busclk;
	unsigned int rate, max;
	int clk;

	

	sdhci_s3c_check_sclk(host);

	for (max = 0, clk = 0; clk < MAX_BUS_CLK; clk++) {
		busclk = ourhost->clk_bus[clk];
		if (!busclk)
			continue;

		rate = clk_get_rate(busclk);
		if (rate > max)
			max = rate;
	}

	return max;
}

static unsigned int sdhci_s3c_consider_clock(struct sdhci_s3c *ourhost,
					     unsigned int src,
					     unsigned int wanted)
{
	unsigned long rate;
	struct clk *clksrc = ourhost->clk_bus[src];
	int div;

	if (!clksrc)
		return UINT_MAX;

	if (ourhost->host->quirks & SDHCI_QUIRK_NONSTANDARD_CLOCK) {
		rate = clk_round_rate(clksrc, wanted);
		return wanted - rate;
	}

	rate = clk_get_rate(clksrc);

	for (div = 1; div < 256; div *= 2) {
		if ((rate / div) <= wanted)
			break;
	}

	dev_dbg(&ourhost->pdev->dev, "clk %d: rate %ld, want %d, got %ld\n",
		src, rate, wanted, rate / div);

	return (wanted - (rate / div));
}

static void sdhci_s3c_set_clock(struct sdhci_host *host, unsigned int clock)
{
	struct sdhci_s3c *ourhost = to_s3c(host);
	unsigned int best = UINT_MAX;
	unsigned int delta;
	int best_src = 0;
	int src;
	u32 ctrl;

	
	if (clock == 0)
		return;

	for (src = 0; src < MAX_BUS_CLK; src++) {
		delta = sdhci_s3c_consider_clock(ourhost, src, clock);
		if (delta < best) {
			best = delta;
			best_src = src;
		}
	}

	dev_dbg(&ourhost->pdev->dev,
		"selected source %d, clock %d, delta %d\n",
		 best_src, clock, best);

	

	if (ourhost->cur_clk != best_src) {
		struct clk *clk = ourhost->clk_bus[best_src];

		
		writew(0, host->ioaddr + SDHCI_CLOCK_CONTROL);

		ourhost->cur_clk = best_src;
		host->max_clk = clk_get_rate(clk);

		ctrl = readl(host->ioaddr + S3C_SDHCI_CONTROL2);
		ctrl &= ~S3C_SDHCI_CTRL2_SELBASECLK_MASK;
		ctrl |= best_src << S3C_SDHCI_CTRL2_SELBASECLK_SHIFT;
		writel(ctrl, host->ioaddr + S3C_SDHCI_CONTROL2);
	}

	
	writel(S3C64XX_SDHCI_CONTROL4_DRIVE_9mA,
		host->ioaddr + S3C64XX_SDHCI_CONTROL4);

	ctrl = readl(host->ioaddr + S3C_SDHCI_CONTROL2);
	ctrl |= (S3C64XX_SDHCI_CTRL2_ENSTAASYNCCLR |
		  S3C64XX_SDHCI_CTRL2_ENCMDCNFMSK |
		  S3C_SDHCI_CTRL2_ENFBCLKRX |
		  S3C_SDHCI_CTRL2_DFCNT_NONE |
		  S3C_SDHCI_CTRL2_ENCLKOUTHOLD);
	writel(ctrl, host->ioaddr + S3C_SDHCI_CONTROL2);

	
	ctrl = (S3C_SDHCI_CTRL3_FCSEL1 | S3C_SDHCI_CTRL3_FCSEL0);
	if (clock < 25 * 1000000)
		ctrl |= (S3C_SDHCI_CTRL3_FCSEL3 | S3C_SDHCI_CTRL3_FCSEL2);
	writel(ctrl, host->ioaddr + S3C_SDHCI_CONTROL3);
}

static unsigned int sdhci_s3c_get_min_clock(struct sdhci_host *host)
{
	struct sdhci_s3c *ourhost = to_s3c(host);
	unsigned int delta, min = UINT_MAX;
	int src;

	for (src = 0; src < MAX_BUS_CLK; src++) {
		delta = sdhci_s3c_consider_clock(ourhost, src, 0);
		if (delta == UINT_MAX)
			continue;
		
		if (-delta < min)
			min = -delta;
	}
	return min;
}

static unsigned int sdhci_cmu_get_max_clock(struct sdhci_host *host)
{
	struct sdhci_s3c *ourhost = to_s3c(host);

	return clk_round_rate(ourhost->clk_bus[ourhost->cur_clk], UINT_MAX);
}

static unsigned int sdhci_cmu_get_min_clock(struct sdhci_host *host)
{
	struct sdhci_s3c *ourhost = to_s3c(host);

	return clk_round_rate(ourhost->clk_bus[ourhost->cur_clk], 400000);
}

static void sdhci_cmu_set_clock(struct sdhci_host *host, unsigned int clock)
{
	struct sdhci_s3c *ourhost = to_s3c(host);
	unsigned long timeout;
	u16 clk = 0;

	
	if (clock == 0)
		return;

	sdhci_s3c_set_clock(host, clock);

	clk_set_rate(ourhost->clk_bus[ourhost->cur_clk], clock);

	host->clock = clock;

	clk = SDHCI_CLOCK_INT_EN;
	sdhci_writew(host, clk, SDHCI_CLOCK_CONTROL);

	
	timeout = 20;
	while (!((clk = sdhci_readw(host, SDHCI_CLOCK_CONTROL))
		& SDHCI_CLOCK_INT_STABLE)) {
		if (timeout == 0) {
			printk(KERN_ERR "%s: Internal clock never "
				"stabilised.\n", mmc_hostname(host->mmc));
			return;
		}
		timeout--;
		mdelay(1);
	}

	clk |= SDHCI_CLOCK_CARD_EN;
	sdhci_writew(host, clk, SDHCI_CLOCK_CONTROL);
}

static int sdhci_s3c_platform_8bit_width(struct sdhci_host *host, int width)
{
	u8 ctrl;

	ctrl = sdhci_readb(host, SDHCI_HOST_CONTROL);

	switch (width) {
	case MMC_BUS_WIDTH_8:
		ctrl |= SDHCI_CTRL_8BITBUS;
		ctrl &= ~SDHCI_CTRL_4BITBUS;
		break;
	case MMC_BUS_WIDTH_4:
		ctrl |= SDHCI_CTRL_4BITBUS;
		ctrl &= ~SDHCI_CTRL_8BITBUS;
		break;
	default:
		ctrl &= ~SDHCI_CTRL_4BITBUS;
		ctrl &= ~SDHCI_CTRL_8BITBUS;
		break;
	}

	sdhci_writeb(host, ctrl, SDHCI_HOST_CONTROL);

	return 0;
}

static struct sdhci_ops sdhci_s3c_ops = {
	.get_max_clock		= sdhci_s3c_get_max_clk,
	.set_clock		= sdhci_s3c_set_clock,
	.get_min_clock		= sdhci_s3c_get_min_clock,
	.platform_8bit_width	= sdhci_s3c_platform_8bit_width,
};

static void sdhci_s3c_notify_change(struct platform_device *dev, int state)
{
	struct sdhci_host *host = platform_get_drvdata(dev);
	unsigned long flags;

	if (host) {
		spin_lock_irqsave(&host->lock, flags);
		if (state) {
			dev_dbg(&dev->dev, "card inserted.\n");
			host->flags &= ~SDHCI_DEVICE_DEAD;
			host->quirks |= SDHCI_QUIRK_BROKEN_CARD_DETECTION;
		} else {
			dev_dbg(&dev->dev, "card removed.\n");
			host->flags |= SDHCI_DEVICE_DEAD;
			host->quirks &= ~SDHCI_QUIRK_BROKEN_CARD_DETECTION;
		}
		tasklet_schedule(&host->card_tasklet);
		spin_unlock_irqrestore(&host->lock, flags);
	}
}

static irqreturn_t sdhci_s3c_gpio_card_detect_thread(int irq, void *dev_id)
{
	struct sdhci_s3c *sc = dev_id;
	int status = gpio_get_value(sc->ext_cd_gpio);
	if (sc->pdata->ext_cd_gpio_invert)
		status = !status;
	sdhci_s3c_notify_change(sc->pdev, status);
	return IRQ_HANDLED;
}

static void sdhci_s3c_setup_card_detect_gpio(struct sdhci_s3c *sc)
{
	struct s3c_sdhci_platdata *pdata = sc->pdata;
	struct device *dev = &sc->pdev->dev;

	if (gpio_request(pdata->ext_cd_gpio, "SDHCI EXT CD") == 0) {
		sc->ext_cd_gpio = pdata->ext_cd_gpio;
		sc->ext_cd_irq = gpio_to_irq(pdata->ext_cd_gpio);
		if (sc->ext_cd_irq &&
		    request_threaded_irq(sc->ext_cd_irq, NULL,
					 sdhci_s3c_gpio_card_detect_thread,
					 IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
					 dev_name(dev), sc) == 0) {
			int status = gpio_get_value(sc->ext_cd_gpio);
			if (pdata->ext_cd_gpio_invert)
				status = !status;
			sdhci_s3c_notify_change(sc->pdev, status);
		} else {
			dev_warn(dev, "cannot request irq for card detect\n");
			sc->ext_cd_irq = 0;
		}
	} else {
		dev_err(dev, "cannot request gpio for card detect\n");
	}
}

static inline struct sdhci_s3c_drv_data *sdhci_s3c_get_driver_data(
			struct platform_device *pdev)
{
	return (struct sdhci_s3c_drv_data *)
			platform_get_device_id(pdev)->driver_data;
}

static int __devinit sdhci_s3c_probe(struct platform_device *pdev)
{
	struct s3c_sdhci_platdata *pdata;
	struct sdhci_s3c_drv_data *drv_data;
	struct device *dev = &pdev->dev;
	struct sdhci_host *host;
	struct sdhci_s3c *sc;
	struct resource *res;
	int ret, irq, ptr, clks;

	if (!pdev->dev.platform_data) {
		dev_err(dev, "no device data specified\n");
		return -ENOENT;
	}

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		dev_err(dev, "no irq specified\n");
		return irq;
	}

	host = sdhci_alloc_host(dev, sizeof(struct sdhci_s3c));
	if (IS_ERR(host)) {
		dev_err(dev, "sdhci_alloc_host() failed\n");
		return PTR_ERR(host);
	}

	pdata = devm_kzalloc(&pdev->dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata) {
		ret = -ENOMEM;
		goto err_io_clk;
	}
	memcpy(pdata, pdev->dev.platform_data, sizeof(*pdata));

	drv_data = sdhci_s3c_get_driver_data(pdev);
	sc = sdhci_priv(host);

	sc->host = host;
	sc->pdev = pdev;
	sc->pdata = pdata;
	sc->ext_cd_gpio = -1; 

	platform_set_drvdata(pdev, host);

	sc->clk_io = clk_get(dev, "hsmmc");
	if (IS_ERR(sc->clk_io)) {
		dev_err(dev, "failed to get io clock\n");
		ret = PTR_ERR(sc->clk_io);
		goto err_io_clk;
	}

	
	clk_enable(sc->clk_io);

	for (clks = 0, ptr = 0; ptr < MAX_BUS_CLK; ptr++) {
		struct clk *clk;
		char name[14];

		snprintf(name, 14, "mmc_busclk.%d", ptr);
		clk = clk_get(dev, name);
		if (IS_ERR(clk)) {
			continue;
		}

		clks++;
		sc->clk_bus[ptr] = clk;

		sc->cur_clk = ptr;

		clk_enable(clk);

		dev_info(dev, "clock source %d: %s (%ld Hz)\n",
			 ptr, name, clk_get_rate(clk));
	}

	if (clks == 0) {
		dev_err(dev, "failed to find any bus clocks\n");
		ret = -ENOENT;
		goto err_no_busclks;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	host->ioaddr = devm_request_and_ioremap(&pdev->dev, res);
	if (!host->ioaddr) {
		dev_err(dev, "failed to map registers\n");
		ret = -ENXIO;
		goto err_req_regs;
	}

	
	if (pdata->cfg_gpio)
		pdata->cfg_gpio(pdev, pdata->max_width);

	host->hw_name = "samsung-hsmmc";
	host->ops = &sdhci_s3c_ops;
	host->quirks = 0;
	host->irq = irq;

	
	host->quirks |= SDHCI_QUIRK_NO_ENDATTR_IN_NOPDESC;
	host->quirks |= SDHCI_QUIRK_NO_HISPD_BIT;
	if (drv_data)
		host->quirks |= drv_data->sdhci_quirks;

#ifndef CONFIG_MMC_SDHCI_S3C_DMA

	host->quirks |= SDHCI_QUIRK_BROKEN_DMA;

#endif 

	host->quirks |= SDHCI_QUIRK_NO_BUSY_IRQ;

	
	host->quirks |= SDHCI_QUIRK_MULTIBLOCK_READ_ACMD12;

	
	host->quirks |= SDHCI_QUIRK_BROKEN_ADMA_ZEROLEN_DESC;

	if (pdata->cd_type == S3C_SDHCI_CD_NONE ||
	    pdata->cd_type == S3C_SDHCI_CD_PERMANENT)
		host->quirks |= SDHCI_QUIRK_BROKEN_CARD_DETECTION;

	if (pdata->cd_type == S3C_SDHCI_CD_PERMANENT)
		host->mmc->caps = MMC_CAP_NONREMOVABLE;

	switch (pdata->max_width) {
	case 8:
		host->mmc->caps |= MMC_CAP_8_BIT_DATA;
	case 4:
		host->mmc->caps |= MMC_CAP_4_BIT_DATA;
		break;
	}

	if (pdata->pm_caps)
		host->mmc->pm_caps |= pdata->pm_caps;

	host->quirks |= (SDHCI_QUIRK_32BIT_DMA_ADDR |
			 SDHCI_QUIRK_32BIT_DMA_SIZE);

	
	host->quirks |= SDHCI_QUIRK_DATA_TIMEOUT_USES_SDCLK;

	if (host->quirks & SDHCI_QUIRK_NONSTANDARD_CLOCK) {
		sdhci_s3c_ops.set_clock = sdhci_cmu_set_clock;
		sdhci_s3c_ops.get_min_clock = sdhci_cmu_get_min_clock;
		sdhci_s3c_ops.get_max_clock = sdhci_cmu_get_max_clock;
	}

	
	if (pdata->host_caps)
		host->mmc->caps |= pdata->host_caps;

	if (pdata->host_caps2)
		host->mmc->caps2 |= pdata->host_caps2;

	pm_runtime_enable(&pdev->dev);
	pm_runtime_set_autosuspend_delay(&pdev->dev, 50);
	pm_runtime_use_autosuspend(&pdev->dev);
	pm_suspend_ignore_children(&pdev->dev, 1);

	ret = sdhci_add_host(host);
	if (ret) {
		dev_err(dev, "sdhci_add_host() failed\n");
		pm_runtime_forbid(&pdev->dev);
		pm_runtime_get_noresume(&pdev->dev);
		goto err_req_regs;
	}

	if (pdata->cd_type == S3C_SDHCI_CD_EXTERNAL && pdata->ext_cd_init)
		pdata->ext_cd_init(&sdhci_s3c_notify_change);
	if (pdata->cd_type == S3C_SDHCI_CD_GPIO &&
	    gpio_is_valid(pdata->ext_cd_gpio))
		sdhci_s3c_setup_card_detect_gpio(sc);

	return 0;

 err_req_regs:
	for (ptr = 0; ptr < MAX_BUS_CLK; ptr++) {
		if (sc->clk_bus[ptr]) {
			clk_disable(sc->clk_bus[ptr]);
			clk_put(sc->clk_bus[ptr]);
		}
	}

 err_no_busclks:
	clk_disable(sc->clk_io);
	clk_put(sc->clk_io);

 err_io_clk:
	sdhci_free_host(host);

	return ret;
}

static int __devexit sdhci_s3c_remove(struct platform_device *pdev)
{
	struct s3c_sdhci_platdata *pdata = pdev->dev.platform_data;
	struct sdhci_host *host =  platform_get_drvdata(pdev);
	struct sdhci_s3c *sc = sdhci_priv(host);
	int ptr;

	if (pdata->cd_type == S3C_SDHCI_CD_EXTERNAL && pdata->ext_cd_cleanup)
		pdata->ext_cd_cleanup(&sdhci_s3c_notify_change);

	if (sc->ext_cd_irq)
		free_irq(sc->ext_cd_irq, sc);

	if (gpio_is_valid(sc->ext_cd_gpio))
		gpio_free(sc->ext_cd_gpio);

	sdhci_remove_host(host, 1);

	pm_runtime_disable(&pdev->dev);

	for (ptr = 0; ptr < 3; ptr++) {
		if (sc->clk_bus[ptr]) {
			clk_disable(sc->clk_bus[ptr]);
			clk_put(sc->clk_bus[ptr]);
		}
	}
	clk_disable(sc->clk_io);
	clk_put(sc->clk_io);

	sdhci_free_host(host);
	platform_set_drvdata(pdev, NULL);

	return 0;
}

#ifdef CONFIG_PM_SLEEP

static int sdhci_s3c_suspend(struct device *dev)
{
	struct sdhci_host *host = dev_get_drvdata(dev);

	return sdhci_suspend_host(host);
}

static int sdhci_s3c_resume(struct device *dev)
{
	struct sdhci_host *host = dev_get_drvdata(dev);

	return sdhci_resume_host(host);
}
#endif

#ifdef CONFIG_PM_RUNTIME
static int sdhci_s3c_runtime_suspend(struct device *dev)
{
	struct sdhci_host *host = dev_get_drvdata(dev);

	return sdhci_runtime_suspend_host(host);
}

static int sdhci_s3c_runtime_resume(struct device *dev)
{
	struct sdhci_host *host = dev_get_drvdata(dev);

	return sdhci_runtime_resume_host(host);
}
#endif

#ifdef CONFIG_PM
static const struct dev_pm_ops sdhci_s3c_pmops = {
	SET_SYSTEM_SLEEP_PM_OPS(sdhci_s3c_suspend, sdhci_s3c_resume)
	SET_RUNTIME_PM_OPS(sdhci_s3c_runtime_suspend, sdhci_s3c_runtime_resume,
			   NULL)
};

#define SDHCI_S3C_PMOPS (&sdhci_s3c_pmops)

static const struct dev_pm_ops sdhci_s3c_pmops = {
	.suspend	= sdhci_s3c_suspend,
	.resume		= sdhci_s3c_resume,
};

#define SDHCI_S3C_PMOPS (&sdhci_s3c_pmops)

#else
#define SDHCI_S3C_PMOPS NULL
#endif

#if defined(CONFIG_CPU_EXYNOS4210) || defined(CONFIG_SOC_EXYNOS4212)
static struct sdhci_s3c_drv_data exynos4_sdhci_drv_data = {
	.sdhci_quirks = SDHCI_QUIRK_NONSTANDARD_CLOCK,
};
#define EXYNOS4_SDHCI_DRV_DATA ((kernel_ulong_t)&exynos4_sdhci_drv_data)
#else
#define EXYNOS4_SDHCI_DRV_DATA ((kernel_ulong_t)NULL)
#endif

static struct platform_device_id sdhci_s3c_driver_ids[] = {
	{
		.name		= "s3c-sdhci",
		.driver_data	= (kernel_ulong_t)NULL,
	}, {
		.name		= "exynos4-sdhci",
		.driver_data	= EXYNOS4_SDHCI_DRV_DATA,
	},
	{ }
};
MODULE_DEVICE_TABLE(platform, sdhci_s3c_driver_ids);

static struct platform_driver sdhci_s3c_driver = {
	.probe		= sdhci_s3c_probe,
	.remove		= __devexit_p(sdhci_s3c_remove),
	.id_table	= sdhci_s3c_driver_ids,
	.driver		= {
		.owner	= THIS_MODULE,
		.name	= "s3c-sdhci",
		.pm	= SDHCI_S3C_PMOPS,
	},
};

module_platform_driver(sdhci_s3c_driver);

MODULE_DESCRIPTION("Samsung SDHCI (HSMMC) glue");
MODULE_AUTHOR("Ben Dooks, <ben@simtec.co.uk>");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:s3c-sdhci");
