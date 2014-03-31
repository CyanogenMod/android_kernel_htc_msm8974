
#include <linux/dma-mapping.h>
#include <linux/gpio.h>
#include <linux/gpio_keys.h>
#include <linux/init.h>
#include <linux/input.h>	
#include <linux/i2c.h>
#include <linux/io.h>
#include <linux/leds.h>
#include <linux/ata_platform.h>
#include <linux/mmc/host.h>
#include <linux/module.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/partitions.h>
#include <linux/platform_device.h>
#include <linux/smsc911x.h>

#include <asm/mach-au1x00/au1000.h>
#include <asm/mach-au1x00/au1100_mmc.h>
#include <asm/mach-au1x00/au1200fb.h>
#include <asm/mach-au1x00/au1xxx_dbdma.h>
#include <asm/mach-au1x00/au1xxx_psc.h>
#include <asm/mach-db1x00/db1300.h>
#include <asm/mach-db1x00/bcsr.h>
#include <asm/mach-au1x00/prom.h>

#include "platform.h"

static struct i2c_board_info db1300_i2c_devs[] __initdata = {
	{ I2C_BOARD_INFO("wm8731", 0x1b), },	
	{ I2C_BOARD_INFO("ne1619", 0x2d), },	
};

static int db1300_gpio_pins[] __initdata = {
	AU1300_PIN_LCDPWM0, AU1300_PIN_PSC2SYNC1, AU1300_PIN_WAKE1,
	AU1300_PIN_WAKE2, AU1300_PIN_WAKE3, AU1300_PIN_FG3AUX,
	AU1300_PIN_EXTCLK1,
	-1,	
};

static int db1300_dev_pins[] __initdata = {
	
	AU1300_PIN_WAKE0,
	
	AU1300_PIN_EXTCLK0,
	
	AU1300_PIN_SD0DAT4, AU1300_PIN_SD0DAT5, AU1300_PIN_SD0DAT6,
	AU1300_PIN_SD0DAT7,
	
	AU1300_PIN_U1RI, AU1300_PIN_U1DCD, AU1300_PIN_U1DSR,
	AU1300_PIN_U1CTS, AU1300_PIN_U1RTS, AU1300_PIN_U1DTR,
	AU1300_PIN_U1RX, AU1300_PIN_U1TX,
	
	AU1300_PIN_U0RI, AU1300_PIN_U0DCD, AU1300_PIN_U0DSR,
	AU1300_PIN_U0CTS, AU1300_PIN_U0RTS, AU1300_PIN_U0DTR,
	
	AU1300_PIN_U2RX, AU1300_PIN_U2TX,
	
	AU1300_PIN_U3RX, AU1300_PIN_U3TX,
	
	AU1300_PIN_LCDPWM1, AU1300_PIN_LCDCLKIN,
	
	AU1300_PIN_SD1DAT0, AU1300_PIN_SD1DAT1, AU1300_PIN_SD1DAT2,
	AU1300_PIN_SD1DAT3, AU1300_PIN_SD1CMD, AU1300_PIN_SD1CLK,
	
	AU1300_PIN_SD2DAT0, AU1300_PIN_SD2DAT1, AU1300_PIN_SD2DAT2,
	AU1300_PIN_SD2DAT3, AU1300_PIN_SD2CMD, AU1300_PIN_SD2CLK,
	
	AU1300_PIN_PSC0CLK, AU1300_PIN_PSC1CLK,
	
	AU1300_PIN_PSC0SYNC0, AU1300_PIN_PSC0SYNC1, AU1300_PIN_PSC0D0,
	AU1300_PIN_PSC0D1,
	AU1300_PIN_PSC1SYNC0, AU1300_PIN_PSC1SYNC1, AU1300_PIN_PSC1D0,
	AU1300_PIN_PSC1D1,
	AU1300_PIN_PSC2SYNC0,                       AU1300_PIN_PSC2D0,
	AU1300_PIN_PSC2D1,
	AU1300_PIN_PSC3SYNC0, AU1300_PIN_PSC3SYNC1, AU1300_PIN_PSC3D0,
	AU1300_PIN_PSC3D1,
	
	AU1300_PIN_PCE2, AU1300_PIN_PCE1, AU1300_PIN_PIOS16,
	AU1300_PIN_PIOR, AU1300_PIN_PWE, AU1300_PIN_PWAIT,
	AU1300_PIN_PREG, AU1300_PIN_POE, AU1300_PIN_PIOW,
	
	AU1300_PIN_CIMLS, AU1300_PIN_CIMFS,
	
	AU1300_PIN_PSC2CLK, AU1300_PIN_PSC3CLK,
	-1,	
};

static void __init db1300_gpio_config(void)
{
	int *i;

	i = &db1300_dev_pins[0];
	while (*i != -1)
		au1300_pinfunc_to_dev(*i++);

	i = &db1300_gpio_pins[0];
	while (*i != -1)
		au1300_gpio_direction_input(*i++);

	au1300_set_dbdma_gpio(1, AU1300_PIN_FG3AUX);
}

char *get_system_type(void)
{
	return "DB1300";
}


static void au1300_nand_cmd_ctrl(struct mtd_info *mtd, int cmd,
				 unsigned int ctrl)
{
	struct nand_chip *this = mtd->priv;
	unsigned long ioaddr = (unsigned long)this->IO_ADDR_W;

	ioaddr &= 0xffffff00;

	if (ctrl & NAND_CLE) {
		ioaddr += MEM_STNAND_CMD;
	} else if (ctrl & NAND_ALE) {
		ioaddr += MEM_STNAND_ADDR;
	} else {
		
		ioaddr += MEM_STNAND_DATA;
	}
	this->IO_ADDR_R = this->IO_ADDR_W = (void __iomem *)ioaddr;
	if (cmd != NAND_CMD_NONE) {
		__raw_writeb(cmd, this->IO_ADDR_W);
		wmb();
	}
}

static int au1300_nand_device_ready(struct mtd_info *mtd)
{
	return __raw_readl((void __iomem *)MEM_STSTAT) & 1;
}

static const char *db1300_part_probes[] = { "cmdlinepart", NULL };

static struct mtd_partition db1300_nand_parts[] = {
	{
		.name	= "NAND FS 0",
		.offset	= 0,
		.size	= 8 * 1024 * 1024,
	},
	{
		.name	= "NAND FS 1",
		.offset	= MTDPART_OFS_APPEND,
		.size	= MTDPART_SIZ_FULL
	},
};

struct platform_nand_data db1300_nand_platdata = {
	.chip = {
		.nr_chips	= 1,
		.chip_offset	= 0,
		.nr_partitions	= ARRAY_SIZE(db1300_nand_parts),
		.partitions	= db1300_nand_parts,
		.chip_delay	= 20,
		.part_probe_types = db1300_part_probes,
	},
	.ctrl = {
		.dev_ready	= au1300_nand_device_ready,
		.cmd_ctrl	= au1300_nand_cmd_ctrl,
	},
};

static struct resource db1300_nand_res[] = {
	[0] = {
		.start	= DB1300_NAND_PHYS_ADDR,
		.end	= DB1300_NAND_PHYS_ADDR + 0xff,
		.flags	= IORESOURCE_MEM,
	},
};

static struct platform_device db1300_nand_dev = {
	.name		= "gen_nand",
	.num_resources	= ARRAY_SIZE(db1300_nand_res),
	.resource	= db1300_nand_res,
	.id		= -1,
	.dev		= {
		.platform_data = &db1300_nand_platdata,
	}
};


static struct resource db1300_eth_res[] = {
	[0] = {
		.start		= DB1300_ETH_PHYS_ADDR,
		.end		= DB1300_ETH_PHYS_END,
		.flags		= IORESOURCE_MEM,
	},
	[1] = {
		.start		= DB1300_ETH_INT,
		.end		= DB1300_ETH_INT,
		.flags		= IORESOURCE_IRQ,
	},
};

static struct smsc911x_platform_config db1300_eth_config = {
	.phy_interface		= PHY_INTERFACE_MODE_MII,
	.irq_polarity		= SMSC911X_IRQ_POLARITY_ACTIVE_LOW,
	.irq_type		= SMSC911X_IRQ_TYPE_PUSH_PULL,
	.flags			= SMSC911X_USE_32BIT,
};

static struct platform_device db1300_eth_dev = {
	.name			= "smsc911x",
	.id			= -1,
	.num_resources		= ARRAY_SIZE(db1300_eth_res),
	.resource		= db1300_eth_res,
	.dev = {
		.platform_data	= &db1300_eth_config,
	},
};


static struct resource au1300_psc1_res[] = {
	[0] = {
		.start	= AU1300_PSC1_PHYS_ADDR,
		.end	= AU1300_PSC1_PHYS_ADDR + 0x0fff,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= AU1300_PSC1_INT,
		.end	= AU1300_PSC1_INT,
		.flags	= IORESOURCE_IRQ,
	},
	[2] = {
		.start	= AU1300_DSCR_CMD0_PSC1_TX,
		.end	= AU1300_DSCR_CMD0_PSC1_TX,
		.flags	= IORESOURCE_DMA,
	},
	[3] = {
		.start	= AU1300_DSCR_CMD0_PSC1_RX,
		.end	= AU1300_DSCR_CMD0_PSC1_RX,
		.flags	= IORESOURCE_DMA,
	},
};

static struct platform_device db1300_ac97_dev = {
	.name		= "au1xpsc_ac97",
	.id		= 1,	
	.num_resources	= ARRAY_SIZE(au1300_psc1_res),
	.resource	= au1300_psc1_res,
};


static struct resource au1300_psc2_res[] = {
	[0] = {
		.start	= AU1300_PSC2_PHYS_ADDR,
		.end	= AU1300_PSC2_PHYS_ADDR + 0x0fff,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= AU1300_PSC2_INT,
		.end	= AU1300_PSC2_INT,
		.flags	= IORESOURCE_IRQ,
	},
	[2] = {
		.start	= AU1300_DSCR_CMD0_PSC2_TX,
		.end	= AU1300_DSCR_CMD0_PSC2_TX,
		.flags	= IORESOURCE_DMA,
	},
	[3] = {
		.start	= AU1300_DSCR_CMD0_PSC2_RX,
		.end	= AU1300_DSCR_CMD0_PSC2_RX,
		.flags	= IORESOURCE_DMA,
	},
};

static struct platform_device db1300_i2s_dev = {
	.name		= "au1xpsc_i2s",
	.id		= 2,	
	.num_resources	= ARRAY_SIZE(au1300_psc2_res),
	.resource	= au1300_psc2_res,
};


static struct resource au1300_psc3_res[] = {
	[0] = {
		.start	= AU1300_PSC3_PHYS_ADDR,
		.end	= AU1300_PSC3_PHYS_ADDR + 0x0fff,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= AU1300_PSC3_INT,
		.end	= AU1300_PSC3_INT,
		.flags	= IORESOURCE_IRQ,
	},
	[2] = {
		.start	= AU1300_DSCR_CMD0_PSC3_TX,
		.end	= AU1300_DSCR_CMD0_PSC3_TX,
		.flags	= IORESOURCE_DMA,
	},
	[3] = {
		.start	= AU1300_DSCR_CMD0_PSC3_RX,
		.end	= AU1300_DSCR_CMD0_PSC3_RX,
		.flags	= IORESOURCE_DMA,
	},
};

static struct platform_device db1300_i2c_dev = {
	.name		= "au1xpsc_smbus",
	.id		= 0,	
	.num_resources	= ARRAY_SIZE(au1300_psc3_res),
	.resource	= au1300_psc3_res,
};


static struct gpio_keys_button db1300_5waysw_arrowkeys[] = {
	{
		.code			= KEY_DOWN,
		.gpio			= AU1300_PIN_LCDPWM0,
		.type			= EV_KEY,
		.debounce_interval	= 1,
		.active_low		= 1,
		.desc			= "5waysw-down",
	},
	{
		.code			= KEY_UP,
		.gpio			= AU1300_PIN_PSC2SYNC1,
		.type			= EV_KEY,
		.debounce_interval	= 1,
		.active_low		= 1,
		.desc			= "5waysw-up",
	},
	{
		.code			= KEY_RIGHT,
		.gpio			= AU1300_PIN_WAKE3,
		.type			= EV_KEY,
		.debounce_interval	= 1,
		.active_low		= 1,
		.desc			= "5waysw-right",
	},
	{
		.code			= KEY_LEFT,
		.gpio			= AU1300_PIN_WAKE2,
		.type			= EV_KEY,
		.debounce_interval	= 1,
		.active_low		= 1,
		.desc			= "5waysw-left",
	},
	{
		.code			= KEY_ENTER,
		.gpio			= AU1300_PIN_WAKE1,
		.type			= EV_KEY,
		.debounce_interval	= 1,
		.active_low		= 1,
		.desc			= "5waysw-push",
	},
};

static struct gpio_keys_platform_data db1300_5waysw_data = {
	.buttons	= db1300_5waysw_arrowkeys,
	.nbuttons	= ARRAY_SIZE(db1300_5waysw_arrowkeys),
	.rep		= 1,
	.name		= "db1300-5wayswitch",
};

static struct platform_device db1300_5waysw_dev = {
	.name		= "gpio-keys",
	.dev	= {
		.platform_data	= &db1300_5waysw_data,
	},
};


static struct pata_platform_info db1300_ide_info = {
	.ioport_shift	= DB1300_IDE_REG_SHIFT,
};

#define IDE_ALT_START	(14 << DB1300_IDE_REG_SHIFT)
static struct resource db1300_ide_res[] = {
	[0] = {
		.start	= DB1300_IDE_PHYS_ADDR,
		.end	= DB1300_IDE_PHYS_ADDR + IDE_ALT_START - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= DB1300_IDE_PHYS_ADDR + IDE_ALT_START,
		.end	= DB1300_IDE_PHYS_ADDR + DB1300_IDE_PHYS_LEN - 1,
		.flags	= IORESOURCE_MEM,
	},
	[2] = {
		.start	= DB1300_IDE_INT,
		.end	= DB1300_IDE_INT,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct platform_device db1300_ide_dev = {
	.dev	= {
		.platform_data	= &db1300_ide_info,
	},
	.name		= "pata_platform",
	.resource	= db1300_ide_res,
	.num_resources	= ARRAY_SIZE(db1300_ide_res),
};


static irqreturn_t db1300_mmc_cd(int irq, void *ptr)
{
	void(*mmc_cd)(struct mmc_host *, unsigned long);

	
	if (irq == DB1300_SD1_INSERT_INT) {
		disable_irq_nosync(DB1300_SD1_INSERT_INT);
		enable_irq(DB1300_SD1_EJECT_INT);
	} else {
		disable_irq_nosync(DB1300_SD1_EJECT_INT);
		enable_irq(DB1300_SD1_INSERT_INT);
	}

	mmc_cd = symbol_get(mmc_detect_change);
	mmc_cd(ptr, msecs_to_jiffies(500));
	symbol_put(mmc_detect_change);

	return IRQ_HANDLED;
}

static int db1300_mmc_card_readonly(void *mmc_host)
{
	
	return bcsr_read(BCSR_STATUS) & BCSR_STATUS_SD0WP;
}

static int db1300_mmc_card_inserted(void *mmc_host)
{
	return bcsr_read(BCSR_SIGSTAT) & (1 << 12); 
}

static int db1300_mmc_cd_setup(void *mmc_host, int en)
{
	int ret;

	if (en) {
		ret = request_irq(DB1300_SD1_INSERT_INT, db1300_mmc_cd, 0,
				  "sd_insert", mmc_host);
		if (ret)
			goto out;

		ret = request_irq(DB1300_SD1_EJECT_INT, db1300_mmc_cd, 0,
				  "sd_eject", mmc_host);
		if (ret) {
			free_irq(DB1300_SD1_INSERT_INT, mmc_host);
			goto out;
		}

		if (db1300_mmc_card_inserted(mmc_host))
			enable_irq(DB1300_SD1_EJECT_INT);
		else
			enable_irq(DB1300_SD1_INSERT_INT);

	} else {
		free_irq(DB1300_SD1_INSERT_INT, mmc_host);
		free_irq(DB1300_SD1_EJECT_INT, mmc_host);
	}
	ret = 0;
out:
	return ret;
}

static void db1300_mmcled_set(struct led_classdev *led,
			      enum led_brightness brightness)
{
	if (brightness != LED_OFF)
		bcsr_mod(BCSR_LEDS, BCSR_LEDS_LED0, 0);
	else
		bcsr_mod(BCSR_LEDS, 0, BCSR_LEDS_LED0);
}

static struct led_classdev db1300_mmc_led = {
	.brightness_set	= db1300_mmcled_set,
};

struct au1xmmc_platform_data db1300_sd1_platdata = {
	.cd_setup	= db1300_mmc_cd_setup,
	.card_inserted	= db1300_mmc_card_inserted,
	.card_readonly	= db1300_mmc_card_readonly,
	.led		= &db1300_mmc_led,
};

static struct resource au1300_sd1_res[] = {
	[0] = {
		.start	= AU1300_SD1_PHYS_ADDR,
		.end	= AU1300_SD1_PHYS_ADDR,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= AU1300_SD1_INT,
		.end	= AU1300_SD1_INT,
		.flags	= IORESOURCE_IRQ,
	},
	[2] = {
		.start	= AU1300_DSCR_CMD0_SDMS_TX1,
		.end	= AU1300_DSCR_CMD0_SDMS_TX1,
		.flags	= IORESOURCE_DMA,
	},
	[3] = {
		.start	= AU1300_DSCR_CMD0_SDMS_RX1,
		.end	= AU1300_DSCR_CMD0_SDMS_RX1,
		.flags	= IORESOURCE_DMA,
	},
};

static struct platform_device db1300_sd1_dev = {
	.dev = {
		.platform_data	= &db1300_sd1_platdata,
	},
	.name		= "au1xxx-mmc",
	.id		= 1,
	.resource	= au1300_sd1_res,
	.num_resources	= ARRAY_SIZE(au1300_sd1_res),
};


static int db1300_movinand_inserted(void *mmc_host)
{
	return 0; 
}

static int db1300_movinand_readonly(void *mmc_host)
{
	return 0;
}

static void db1300_movinand_led_set(struct led_classdev *led,
				    enum led_brightness brightness)
{
	if (brightness != LED_OFF)
		bcsr_mod(BCSR_LEDS, BCSR_LEDS_LED1, 0);
	else
		bcsr_mod(BCSR_LEDS, 0, BCSR_LEDS_LED1);
}

static struct led_classdev db1300_movinand_led = {
	.brightness_set		= db1300_movinand_led_set,
};

struct au1xmmc_platform_data db1300_sd0_platdata = {
	.card_inserted		= db1300_movinand_inserted,
	.card_readonly		= db1300_movinand_readonly,
	.led			= &db1300_movinand_led,
	.mask_host_caps		= MMC_CAP_NEEDS_POLL,
};

static struct resource au1300_sd0_res[] = {
	[0] = {
		.start	= AU1100_SD0_PHYS_ADDR,
		.end	= AU1100_SD0_PHYS_ADDR,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= AU1300_SD0_INT,
		.end	= AU1300_SD0_INT,
		.flags	= IORESOURCE_IRQ,
	},
	[2] = {
		.start	= AU1300_DSCR_CMD0_SDMS_TX0,
		.end	= AU1300_DSCR_CMD0_SDMS_TX0,
		.flags	= IORESOURCE_DMA,
	},
	[3] = {
		.start	= AU1300_DSCR_CMD0_SDMS_RX0,
		.end	= AU1300_DSCR_CMD0_SDMS_RX0,
		.flags	= IORESOURCE_DMA,
	},
};

static struct platform_device db1300_sd0_dev = {
	.dev = {
		.platform_data	= &db1300_sd0_platdata,
	},
	.name		= "au1xxx-mmc",
	.id		= 0,
	.resource	= au1300_sd0_res,
	.num_resources	= ARRAY_SIZE(au1300_sd0_res),
};


static struct platform_device db1300_wm9715_dev = {
	.name		= "wm9712-codec",
	.id		= 1,	
};

static struct platform_device db1300_ac97dma_dev = {
	.name		= "au1xpsc-pcm",
	.id		= 1,	
};

static struct platform_device db1300_i2sdma_dev = {
	.name		= "au1xpsc-pcm",
	.id		= 2,	
};

static struct platform_device db1300_sndac97_dev = {
	.name		= "db1300-ac97",
};

static struct platform_device db1300_sndi2s_dev = {
	.name		= "db1300-i2s",
};


static int db1300fb_panel_index(void)
{
	return 9;	
}

static int db1300fb_panel_init(void)
{
	
	bcsr_mod(BCSR_BOARD, BCSR_BOARD_LCDVEE | BCSR_BOARD_LCDVDD,
			     BCSR_BOARD_LCDBL);
	return 0;
}

static int db1300fb_panel_shutdown(void)
{
	
	bcsr_mod(BCSR_BOARD, BCSR_BOARD_LCDBL,
			     BCSR_BOARD_LCDVEE | BCSR_BOARD_LCDVDD);
	return 0;
}

static struct au1200fb_platdata db1300fb_pd = {
	.panel_index	= db1300fb_panel_index,
	.panel_init	= db1300fb_panel_init,
	.panel_shutdown	= db1300fb_panel_shutdown,
};

static struct resource au1300_lcd_res[] = {
	[0] = {
		.start	= AU1200_LCD_PHYS_ADDR,
		.end	= AU1200_LCD_PHYS_ADDR + 0x800 - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= AU1300_LCD_INT,
		.end	= AU1300_LCD_INT,
		.flags	= IORESOURCE_IRQ,
	}
};

static u64 au1300_lcd_dmamask = DMA_BIT_MASK(32);

static struct platform_device db1300_lcd_dev = {
	.name		= "au1200-lcd",
	.id		= 0,
	.dev = {
		.dma_mask		= &au1300_lcd_dmamask,
		.coherent_dma_mask	= DMA_BIT_MASK(32),
		.platform_data		= &db1300fb_pd,
	},
	.num_resources	= ARRAY_SIZE(au1300_lcd_res),
	.resource	= au1300_lcd_res,
};


static struct platform_device *db1300_dev[] __initdata = {
	&db1300_eth_dev,
	&db1300_i2c_dev,
	&db1300_5waysw_dev,
	&db1300_nand_dev,
	&db1300_ide_dev,
	&db1300_sd0_dev,
	&db1300_sd1_dev,
	&db1300_lcd_dev,
	&db1300_ac97_dev,
	&db1300_i2s_dev,
	&db1300_wm9715_dev,
	&db1300_ac97dma_dev,
	&db1300_i2sdma_dev,
	&db1300_sndac97_dev,
	&db1300_sndi2s_dev,
};

static int __init db1300_device_init(void)
{
	int swapped, cpldirq;

	
	cpldirq = au1300_gpio_to_irq(AU1300_PIN_EXTCLK1);
	irq_set_irq_type(cpldirq, IRQ_TYPE_LEVEL_HIGH);
	bcsr_init_irq(DB1300_FIRST_INT, DB1300_LAST_INT, cpldirq);

	irq_set_status_flags(DB1300_SD1_INSERT_INT, IRQ_NOAUTOEN);
	irq_set_status_flags(DB1300_SD1_EJECT_INT, IRQ_NOAUTOEN);
	irq_set_status_flags(DB1300_CF_INSERT_INT, IRQ_NOAUTOEN);
	irq_set_status_flags(DB1300_CF_EJECT_INT, IRQ_NOAUTOEN);

	prom_get_ethernet_addr(&db1300_eth_config.mac[0]);

	i2c_register_board_info(0, db1300_i2c_devs,
				ARRAY_SIZE(db1300_i2c_devs));

	
	__raw_writel(PSC_SEL_CLK_SERCLK,
	    (void __iomem *)KSEG1ADDR(AU1300_PSC1_PHYS_ADDR) + PSC_SEL_OFFSET);
	wmb();
	__raw_writel(PSC_SEL_CLK_SERCLK,
	    (void __iomem *)KSEG1ADDR(AU1300_PSC2_PHYS_ADDR) + PSC_SEL_OFFSET);
	wmb();
	
	__raw_writel(PSC_SEL_CLK_INTCLK,
	    (void __iomem *)KSEG1ADDR(AU1300_PSC3_PHYS_ADDR) + PSC_SEL_OFFSET);
	wmb();

	
	bcsr_mod(BCSR_RESETS, 0, BCSR_RESETS_USBHPWR | BCSR_RESETS_OTGPWR);

	db1x_register_pcmcia_socket(
		AU1000_PCMCIA_ATTR_PHYS_ADDR,
		AU1000_PCMCIA_ATTR_PHYS_ADDR + 0x00400000 - 1,
		AU1000_PCMCIA_MEM_PHYS_ADDR,
		AU1000_PCMCIA_MEM_PHYS_ADDR  + 0x00400000 - 1,
		AU1000_PCMCIA_IO_PHYS_ADDR,
		AU1000_PCMCIA_IO_PHYS_ADDR   + 0x00010000 - 1,
		DB1300_CF_INT, DB1300_CF_INSERT_INT, 0, DB1300_CF_EJECT_INT, 1);

	swapped = bcsr_read(BCSR_STATUS) & BCSR_STATUS_DB1200_SWAPBOOT;
	db1x_register_norflash(64 << 20, 2, swapped);

	return platform_add_devices(db1300_dev, ARRAY_SIZE(db1300_dev));
}
device_initcall(db1300_device_init);


void __init board_setup(void)
{
	unsigned short whoami;

	db1300_gpio_config();
	bcsr_init(DB1300_BCSR_PHYS_ADDR,
		  DB1300_BCSR_PHYS_ADDR + DB1300_BCSR_HEXLED_OFS);

	whoami = bcsr_read(BCSR_WHOAMI);
	printk(KERN_INFO "NetLogic DBAu1300 Development Platform.\n\t"
		"BoardID %d   CPLD Rev %d   DaughtercardID %d\n",
		BCSR_WHOAMI_BOARD(whoami), BCSR_WHOAMI_CPLD(whoami),
		BCSR_WHOAMI_DCID(whoami));

	
	alchemy_uart_enable(AU1300_UART0_PHYS_ADDR);
	alchemy_uart_enable(AU1300_UART1_PHYS_ADDR);
	alchemy_uart_enable(AU1300_UART3_PHYS_ADDR);
}
