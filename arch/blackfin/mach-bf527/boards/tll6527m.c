/* File:	arch/blackfin/mach-bf527/boards/tll6527m.c
 * Based on:	arch/blackfin/mach-bf527/boards/ezkit.c
 * Author:	Ashish Gupta
 *
 * Copyright: 2010 - The Learning Labs Inc.
 *
 * Licensed under the GPL-2 or later.
 */

#include <linux/device.h>
#include <linux/export.h>
#include <linux/platform_device.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>
#include <linux/mtd/physmap.h>
#include <linux/spi/spi.h>
#include <linux/spi/flash.h>
#include <linux/i2c.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/usb/musb.h>
#include <linux/leds.h>
#include <linux/input.h>
#include <asm/dma.h>
#include <asm/bfin5xx_spi.h>
#include <asm/reboot.h>
#include <asm/nand.h>
#include <asm/portmux.h>
#include <asm/dpmc.h>

#if defined(CONFIG_TOUCHSCREEN_AD7879) \
	|| defined(CONFIG_TOUCHSCREEN_AD7879_MODULE)
#include <linux/spi/ad7879.h>
#define LCD_BACKLIGHT_GPIO 0x40
#endif

const char bfin_board_name[] = "TLL6527M";

#if defined(CONFIG_USB_MUSB_HDRC) || defined(CONFIG_USB_MUSB_HDRC_MODULE)
static struct resource musb_resources[] = {
	[0] = {
		.start	= 0xffc03800,
		.end	= 0xffc03cff,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {	
		.start	= IRQ_USB_INT0,
		.end	= IRQ_USB_INT0,
		.flags	= IORESOURCE_IRQ | IORESOURCE_IRQ_HIGHLEVEL,
	},
	[2] = {	
		.start	= IRQ_USB_DMA,
		.end	= IRQ_USB_DMA,
		.flags	= IORESOURCE_IRQ | IORESOURCE_IRQ_HIGHLEVEL,
	},
};

static struct musb_hdrc_config musb_config = {
	.multipoint	= 0,
	.dyn_fifo	= 0,
	.soft_con	= 1,
	.dma		= 1,
	.num_eps	= 8,
	.dma_channels	= 8,
	
	.gpio_vrsel_active	= 1,
};

static struct musb_hdrc_platform_data musb_plat = {
#if defined(CONFIG_USB_MUSB_OTG)
	.mode		= MUSB_OTG,
#elif defined(CONFIG_USB_MUSB_HDRC_HCD)
	.mode		= MUSB_HOST,
#elif defined(CONFIG_USB_GADGET_MUSB_HDRC)
	.mode		= MUSB_PERIPHERAL,
#endif
	.config		= &musb_config,
};

static u64 musb_dmamask = ~(u32)0;

static struct platform_device musb_device = {
	.name		= "musb-blackfin",
	.id		= 0,
	.dev = {
		.dma_mask		= &musb_dmamask,
		.coherent_dma_mask	= 0xffffffff,
		.platform_data		= &musb_plat,
	},
	.num_resources	= ARRAY_SIZE(musb_resources),
	.resource	= musb_resources,
};
#endif

#if defined(CONFIG_FB_BFIN_LQ035Q1) || defined(CONFIG_FB_BFIN_LQ035Q1_MODULE)
#include <asm/bfin-lq035q1.h>

static struct bfin_lq035q1fb_disp_info bfin_lq035q1_data = {
	.mode = LQ035_NORM | LQ035_RGB | LQ035_RL | LQ035_TB,
	.ppi_mode = USE_RGB565_16_BIT_PPI,
	.use_bl = 1,
	.gpio_bl = LCD_BACKLIGHT_GPIO,
};

static struct resource bfin_lq035q1_resources[] = {
	{
		.start = IRQ_PPI_ERROR,
		.end = IRQ_PPI_ERROR,
		.flags = IORESOURCE_IRQ,
	},
};

static struct platform_device bfin_lq035q1_device = {
	.name		= "bfin-lq035q1",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(bfin_lq035q1_resources),
	.resource	= bfin_lq035q1_resources,
	.dev		= {
		.platform_data = &bfin_lq035q1_data,
	},
};
#endif

#if defined(CONFIG_MTD_GPIO_ADDR) || defined(CONFIG_MTD_GPIO_ADDR_MODULE)
static struct mtd_partition tll6527m_partitions[] = {
	{
		.name       = "bootloader(nor)",
		.size       = 0xA0000,
		.offset     = 0,
	}, {
		.name       = "linux kernel(nor)",
		.size       = 0xD00000,
		.offset     = MTDPART_OFS_APPEND,
	}, {
		.name       = "file system(nor)",
		.size       = MTDPART_SIZ_FULL,
		.offset     = MTDPART_OFS_APPEND,
	}
};

static struct physmap_flash_data tll6527m_flash_data = {
	.width      = 2,
	.parts      = tll6527m_partitions,
	.nr_parts   = ARRAY_SIZE(tll6527m_partitions),
};

static unsigned tll6527m_flash_gpios[] = { GPIO_PG11, GPIO_PH11, GPIO_PH12 };

static struct resource tll6527m_flash_resource[] = {
	{
		.name  = "cfi_probe",
		.start = 0x20000000,
		.end   = 0x201fffff,
		.flags = IORESOURCE_MEM,
	}, {
		.start = (unsigned long)tll6527m_flash_gpios,
		.end   = ARRAY_SIZE(tll6527m_flash_gpios),
		.flags = IORESOURCE_IRQ,
	}
};

static struct platform_device tll6527m_flash_device = {
	.name          = "gpio-addr-flash",
	.id            = 0,
	.dev = {
		.platform_data = &tll6527m_flash_data,
	},
	.num_resources = ARRAY_SIZE(tll6527m_flash_resource),
	.resource      = tll6527m_flash_resource,
};
#endif

#if defined(CONFIG_GPIO_DECODER) || defined(CONFIG_GPIO_DECODER_MODULE)
#include <linux/gpio-decoder.h>
#define EXP_GPIO_SPISEL_BASE 0x64
static unsigned gpio_addr_inputs[] = {
	GPIO_PG1, GPIO_PH9, GPIO_PH10
};

static struct gpio_decoder_platform_data spi_decoded_cs = {
	.base		= EXP_GPIO_SPISEL_BASE,
	.input_addrs	= gpio_addr_inputs,
	.nr_input_addrs = ARRAY_SIZE(gpio_addr_inputs),
	.default_output	= 0,
};

static struct platform_device spi_decoded_gpio = {
	.name	= "gpio-decoder",
	.id	= 0,
	.dev	= {
		.platform_data = &spi_decoded_cs,
	},
};

#else
#define EXP_GPIO_SPISEL_BASE 0x0

#endif

#if defined(CONFIG_INPUT_ADXL34X) || defined(CONFIG_INPUT_ADXL34X_MODULE)
#include <linux/input/adxl34x.h>
static const struct adxl34x_platform_data adxl345_info = {
	.x_axis_offset = 0,
	.y_axis_offset = 0,
	.z_axis_offset = 0,
	.tap_threshold = 0x31,
	.tap_duration = 0x10,
	.tap_latency = 0x60,
	.tap_window = 0xF0,
	.tap_axis_control = ADXL_TAP_X_EN | ADXL_TAP_Y_EN | ADXL_TAP_Z_EN,
	.act_axis_control = 0xFF,
	.activity_threshold = 5,
	.inactivity_threshold = 2,
	.inactivity_time = 2,
	.free_fall_threshold = 0x7,
	.free_fall_time = 0x20,
	.data_rate = 0x8,
	.data_range = ADXL_FULL_RES,

	.ev_type = EV_ABS,
	.ev_code_x = ABS_X,		
	.ev_code_y = ABS_Y,		
	.ev_code_z = ABS_Z,		

	.ev_code_tap = {BTN_TOUCH, BTN_TOUCH, BTN_TOUCH}, 

		
	.ev_code_act_inactivity = KEY_A,	
	.use_int2 = 1,
	.power_mode = ADXL_AUTO_SLEEP | ADXL_LINK,
	.fifo_mode = ADXL_FIFO_STREAM,
};
#endif

#if defined(CONFIG_RTC_DRV_BFIN) || defined(CONFIG_RTC_DRV_BFIN_MODULE)
static struct platform_device rtc_device = {
	.name = "rtc-bfin",
	.id   = -1,
};
#endif

#if defined(CONFIG_BFIN_MAC) || defined(CONFIG_BFIN_MAC_MODULE)
#include <linux/bfin_mac.h>
static const unsigned short bfin_mac_peripherals[] = P_RMII0;

static struct bfin_phydev_platform_data bfin_phydev_data[] = {
	{
		.addr = 1,
		.irq = IRQ_MAC_PHYINT,
	},
};

static struct bfin_mii_bus_platform_data bfin_mii_bus_data = {
	.phydev_number = 1,
	.phydev_data = bfin_phydev_data,
	.phy_mode = PHY_INTERFACE_MODE_RMII,
	.mac_peripherals = bfin_mac_peripherals,
};

static struct platform_device bfin_mii_bus = {
	.name = "bfin_mii_bus",
	.dev = {
		.platform_data = &bfin_mii_bus_data,
	}
};

static struct platform_device bfin_mac_device = {
	.name = "bfin_mac",
	.dev = {
		.platform_data = &bfin_mii_bus,
	}
};
#endif

#if defined(CONFIG_MTD_M25P80) \
	|| defined(CONFIG_MTD_M25P80_MODULE)
static struct mtd_partition bfin_spi_flash_partitions[] = {
	{
		.name = "bootloader(spi)",
		.size = 0x00040000,
		.offset = 0,
		.mask_flags = MTD_CAP_ROM
	}, {
		.name = "linux kernel(spi)",
		.size = MTDPART_SIZ_FULL,
		.offset = MTDPART_OFS_APPEND,
	}
};

static struct flash_platform_data bfin_spi_flash_data = {
	.name = "m25p80",
	.parts = bfin_spi_flash_partitions,
	.nr_parts = ARRAY_SIZE(bfin_spi_flash_partitions),
	.type = "m25p16",
};

static struct bfin5xx_spi_chip spi_flash_chip_info = {
	.enable_dma = 0,         
};
#endif

#if defined(CONFIG_MMC_SPI) || defined(CONFIG_MMC_SPI_MODULE)
static struct bfin5xx_spi_chip  mmc_spi_chip_info = {
	.enable_dma = 0,
};
#endif

#if defined(CONFIG_TOUCHSCREEN_AD7879) \
	|| defined(CONFIG_TOUCHSCREEN_AD7879_MODULE)
static const struct ad7879_platform_data bfin_ad7879_ts_info = {
	.model			= 7879,	
	.x_plate_ohms		= 620,	
	.pressure_max		= 10000,
	.pressure_min		= 0,
	.first_conversion_delay = 3,
				
	.acquisition_time	= 1,	
	.median			= 2,	
	.averaging		= 1,
				
	.pen_down_acc_interval	= 255,	
	.gpio_export		= 1,	
	.gpio_base		= LCD_BACKLIGHT_GPIO,
};
#endif

#if defined(CONFIG_SND_BF5XX_I2S) || defined(CONFIG_SND_BF5XX_I2S_MODULE)
static struct platform_device bfin_i2s = {
	.name = "bfin-i2s",
	.id = CONFIG_SND_BF5XX_SPORT_NUM,
	
};
#endif

#if defined(CONFIG_GPIO_MCP23S08) || defined(CONFIG_GPIO_MCP23S08_MODULE)
#include <linux/spi/mcp23s08.h>
static const struct mcp23s08_platform_data bfin_mcp23s08_sys_gpio_info = {
	.chip[0].is_present = true,
	.base = 0x30,
};
static const struct mcp23s08_platform_data bfin_mcp23s08_usr_gpio_info = {
	.chip[2].is_present = true,
	.base = 0x38,
};
#endif

static struct spi_board_info bfin_spi_board_info[] __initdata = {
#if defined(CONFIG_MTD_M25P80) \
	|| defined(CONFIG_MTD_M25P80_MODULE)
	{
		
		.modalias = "m25p80", 
		.max_speed_hz = 25000000,
				
		.bus_num = 0, 
		.chip_select = EXP_GPIO_SPISEL_BASE + 0x04 + MAX_CTRL_CS,
		
		
		.platform_data = &bfin_spi_flash_data,
		.controller_data = &spi_flash_chip_info,
		.mode = SPI_MODE_3,
	},
#endif

#if defined(CONFIG_MMC_SPI) || defined(CONFIG_MMC_SPI_MODULE)
	{
		.modalias = "mmc_spi",
		.max_speed_hz = 10000000,
					
		.bus_num = 0,
		.chip_select = EXP_GPIO_SPISEL_BASE + 0x05 + MAX_CTRL_CS,
		.controller_data = &mmc_spi_chip_info,
		.mode = SPI_MODE_0,
	},
#endif
#if defined(CONFIG_TOUCHSCREEN_AD7879_SPI) \
	|| defined(CONFIG_TOUCHSCREEN_AD7879_SPI_MODULE)
	{
		.modalias = "ad7879",
		.platform_data = &bfin_ad7879_ts_info,
		.irq = IRQ_PH14,
		.max_speed_hz = 5000000,
					
		.bus_num = 0,
		.chip_select = EXP_GPIO_SPISEL_BASE + 0x07 + MAX_CTRL_CS,
		.mode = SPI_CPHA | SPI_CPOL,
	},
#endif
#if defined(CONFIG_SPI_SPIDEV) || defined(CONFIG_SPI_SPIDEV_MODULE)
	{
		.modalias = "spidev",
		.max_speed_hz = 10000000,
		
		.bus_num = 0,
		.chip_select = EXP_GPIO_SPISEL_BASE + 0x03 + MAX_CTRL_CS,
		.mode = SPI_CPHA | SPI_CPOL,
	},
#endif
#if defined(CONFIG_FB_BFIN_LQ035Q1) || defined(CONFIG_FB_BFIN_LQ035Q1_MODULE)
	{
		.modalias = "bfin-lq035q1-spi",
		.max_speed_hz = 20000000,
		.bus_num = 0,
		.chip_select = EXP_GPIO_SPISEL_BASE + 0x06 + MAX_CTRL_CS,
		.mode = SPI_CPHA | SPI_CPOL,
	},
#endif
#if defined(CONFIG_GPIO_MCP23S08) || defined(CONFIG_GPIO_MCP23S08_MODULE)
	{
		.modalias = "mcp23s08",
		.platform_data = &bfin_mcp23s08_sys_gpio_info,
		.max_speed_hz = 5000000, 
		.bus_num = 0,
		.chip_select = EXP_GPIO_SPISEL_BASE + 0x01 + MAX_CTRL_CS,
		.mode = SPI_CPHA | SPI_CPOL,
	},
	{
		.modalias = "mcp23s08",
		.platform_data = &bfin_mcp23s08_usr_gpio_info,
		.max_speed_hz = 5000000, 
		.bus_num = 0,
		.chip_select = EXP_GPIO_SPISEL_BASE + 0x02 + MAX_CTRL_CS,
		.mode = SPI_CPHA | SPI_CPOL,
	},
#endif
};

#if defined(CONFIG_SPI_BFIN5XX) || defined(CONFIG_SPI_BFIN5XX_MODULE)
static struct bfin5xx_spi_master bfin_spi0_info = {
	.num_chipselect = EXP_GPIO_SPISEL_BASE + 8 + MAX_CTRL_CS,
	
	.enable_dma = 1,  
	.pin_req = {P_SPI0_SCK, P_SPI0_MISO, P_SPI0_MOSI, 0},
};

static struct resource bfin_spi0_resource[] = {
	[0] = {
		.start = SPI0_REGBASE,
		.end   = SPI0_REGBASE + 0xFF,
		.flags = IORESOURCE_MEM,
		},
	[1] = {
		.start = CH_SPI,
		.end   = CH_SPI,
		.flags = IORESOURCE_DMA,
	},
	[2] = {
		.start = IRQ_SPI,
		.end   = IRQ_SPI,
		.flags = IORESOURCE_IRQ,
	},
};

static struct platform_device bfin_spi0_device = {
	.name = "bfin-spi",
	.id = 0, 
	.num_resources = ARRAY_SIZE(bfin_spi0_resource),
	.resource = bfin_spi0_resource,
	.dev = {
		.platform_data = &bfin_spi0_info, 
	},
};
#endif  

#if defined(CONFIG_SERIAL_BFIN) || defined(CONFIG_SERIAL_BFIN_MODULE)
#ifdef CONFIG_SERIAL_BFIN_UART0
static struct resource bfin_uart0_resources[] = {
	{
		.start = UART0_THR,
		.end = UART0_GCTL+2,
		.flags = IORESOURCE_MEM,
	},
	{
		.start = IRQ_UART0_TX,
		.end = IRQ_UART0_TX,
		.flags = IORESOURCE_IRQ,
	},
	{
		.start = IRQ_UART0_RX,
		.end = IRQ_UART0_RX,
		.flags = IORESOURCE_IRQ,
	},
	{
		.start = IRQ_UART0_ERROR,
		.end = IRQ_UART0_ERROR,
		.flags = IORESOURCE_IRQ,
	},
	{
		.start = CH_UART0_TX,
		.end = CH_UART0_TX,
		.flags = IORESOURCE_DMA,
	},
	{
		.start = CH_UART0_RX,
		.end = CH_UART0_RX,
		.flags = IORESOURCE_DMA,
	},
};

static unsigned short bfin_uart0_peripherals[] = {
	P_UART0_TX, P_UART0_RX, 0
};

static struct platform_device bfin_uart0_device = {
	.name = "bfin-uart",
	.id = 0,
	.num_resources = ARRAY_SIZE(bfin_uart0_resources),
	.resource = bfin_uart0_resources,
	.dev = {
		.platform_data = &bfin_uart0_peripherals,
					
	},
};
#endif
#ifdef CONFIG_SERIAL_BFIN_UART1
static struct resource bfin_uart1_resources[] = {
	{
		.start = UART1_THR,
		.end = UART1_GCTL+2,
		.flags = IORESOURCE_MEM,
	},
	{
		.start = IRQ_UART1_TX,
		.end = IRQ_UART1_TX,
		.flags = IORESOURCE_IRQ,
	},
	{
		.start = IRQ_UART1_RX,
		.end = IRQ_UART1_RX,
		.flags = IORESOURCE_IRQ,
	},
	{
		.start = IRQ_UART1_ERROR,
		.end = IRQ_UART1_ERROR,
		.flags = IORESOURCE_IRQ,
	},
	{
		.start = CH_UART1_TX,
		.end = CH_UART1_TX,
		.flags = IORESOURCE_DMA,
	},
	{
		.start = CH_UART1_RX,
		.end = CH_UART1_RX,
		.flags = IORESOURCE_DMA,
	},
#ifdef CONFIG_BFIN_UART1_CTSRTS
	{	
		.start = GPIO_PF9,
		.end = GPIO_PF9,
		.flags = IORESOURCE_IO,
	},
	{	
		.start = GPIO_PF10,
		.end = GPIO_PF10,
		.flags = IORESOURCE_IO,
	},
#endif
};

static unsigned short bfin_uart1_peripherals[] = {
	P_UART1_TX, P_UART1_RX, 0
};

static struct platform_device bfin_uart1_device = {
	.name = "bfin-uart",
	.id = 1,
	.num_resources = ARRAY_SIZE(bfin_uart1_resources),
	.resource = bfin_uart1_resources,
	.dev = {
		.platform_data = &bfin_uart1_peripherals,
						
	},
};
#endif
#endif

#if defined(CONFIG_BFIN_SIR) || defined(CONFIG_BFIN_SIR_MODULE)
#ifdef CONFIG_BFIN_SIR0
static struct resource bfin_sir0_resources[] = {
	{
		.start = 0xFFC00400,
		.end = 0xFFC004FF,
		.flags = IORESOURCE_MEM,
	},
	{
		.start = IRQ_UART0_RX,
		.end = IRQ_UART0_RX+1,
		.flags = IORESOURCE_IRQ,
	},
	{
		.start = CH_UART0_RX,
		.end = CH_UART0_RX+1,
		.flags = IORESOURCE_DMA,
	},
};

static struct platform_device bfin_sir0_device = {
	.name = "bfin_sir",
	.id = 0,
	.num_resources = ARRAY_SIZE(bfin_sir0_resources),
	.resource = bfin_sir0_resources,
};
#endif
#ifdef CONFIG_BFIN_SIR1
static struct resource bfin_sir1_resources[] = {
	{
		.start = 0xFFC02000,
		.end = 0xFFC020FF,
		.flags = IORESOURCE_MEM,
	},
	{
		.start = IRQ_UART1_RX,
		.end = IRQ_UART1_RX+1,
		.flags = IORESOURCE_IRQ,
	},
	{
		.start = CH_UART1_RX,
		.end = CH_UART1_RX+1,
		.flags = IORESOURCE_DMA,
	},
};

static struct platform_device bfin_sir1_device = {
	.name = "bfin_sir",
	.id = 1,
	.num_resources = ARRAY_SIZE(bfin_sir1_resources),
	.resource = bfin_sir1_resources,
};
#endif
#endif

#if defined(CONFIG_I2C_BLACKFIN_TWI) || defined(CONFIG_I2C_BLACKFIN_TWI_MODULE)
static struct resource bfin_twi0_resource[] = {
	[0] = {
		.start = TWI0_REGBASE,
		.end   = TWI0_REGBASE,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = IRQ_TWI,
		.end   = IRQ_TWI,
		.flags = IORESOURCE_IRQ,
	},
};

static struct platform_device i2c_bfin_twi_device = {
	.name = "i2c-bfin-twi",
	.id = 0,
	.num_resources = ARRAY_SIZE(bfin_twi0_resource),
	.resource = bfin_twi0_resource,
};
#endif

static struct i2c_board_info __initdata bfin_i2c_board_info[] = {
#if defined(CONFIG_BFIN_TWI_LCD) || defined(CONFIG_BFIN_TWI_LCD_MODULE)
	{
		I2C_BOARD_INFO("pcf8574_lcd", 0x22),
	},
#endif

#if defined(CONFIG_FB_BFIN_7393) || defined(CONFIG_FB_BFIN_7393_MODULE)
	{
		I2C_BOARD_INFO("bfin-adv7393", 0x2B),
	},
#endif
#if defined(CONFIG_TOUCHSCREEN_AD7879_I2C) \
	|| defined(CONFIG_TOUCHSCREEN_AD7879_I2C_MODULE)
	{
		I2C_BOARD_INFO("ad7879", 0x2C),
		.irq = IRQ_PH14,
		.platform_data = (void *)&bfin_ad7879_ts_info,
	},
#endif
#if defined(CONFIG_SND_SOC_SSM2602) || defined(CONFIG_SND_SOC_SSM2602_MODULE)
	{
		I2C_BOARD_INFO("ssm2602", 0x1b),
	},
#endif
	{
		I2C_BOARD_INFO("adm1192", 0x2e),
	},

	{
		I2C_BOARD_INFO("ltc3576", 0x09),
	},
#if defined(CONFIG_INPUT_ADXL34X_I2C) \
	|| defined(CONFIG_INPUT_ADXL34X_I2C_MODULE)
	{
		I2C_BOARD_INFO("adxl34x", 0x53),
		.irq = IRQ_PH13,
		.platform_data = (void *)&adxl345_info,
	},
#endif
};

#if defined(CONFIG_SERIAL_BFIN_SPORT) \
	|| defined(CONFIG_SERIAL_BFIN_SPORT_MODULE)
#ifdef CONFIG_SERIAL_BFIN_SPORT0_UART
static struct resource bfin_sport0_uart_resources[] = {
	{
		.start = SPORT0_TCR1,
		.end = SPORT0_MRCS3+4,
		.flags = IORESOURCE_MEM,
	},
	{
		.start = IRQ_SPORT0_RX,
		.end = IRQ_SPORT0_RX+1,
		.flags = IORESOURCE_IRQ,
	},
	{
		.start = IRQ_SPORT0_ERROR,
		.end = IRQ_SPORT0_ERROR,
		.flags = IORESOURCE_IRQ,
	},
};

static unsigned short bfin_sport0_peripherals[] = {
	P_SPORT0_TFS, P_SPORT0_DTPRI, P_SPORT0_TSCLK, P_SPORT0_RFS,
	P_SPORT0_DRPRI, P_SPORT0_RSCLK, 0
};

static struct platform_device bfin_sport0_uart_device = {
	.name = "bfin-sport-uart",
	.id = 0,
	.num_resources = ARRAY_SIZE(bfin_sport0_uart_resources),
	.resource = bfin_sport0_uart_resources,
	.dev = {
		.platform_data = &bfin_sport0_peripherals,
		
	},
};
#endif
#ifdef CONFIG_SERIAL_BFIN_SPORT1_UART
static struct resource bfin_sport1_uart_resources[] = {
	{
		.start = SPORT1_TCR1,
		.end = SPORT1_MRCS3+4,
		.flags = IORESOURCE_MEM,
	},
	{
		.start = IRQ_SPORT1_RX,
		.end = IRQ_SPORT1_RX+1,
		.flags = IORESOURCE_IRQ,
	},
	{
		.start = IRQ_SPORT1_ERROR,
		.end = IRQ_SPORT1_ERROR,
		.flags = IORESOURCE_IRQ,
	},
};

static unsigned short bfin_sport1_peripherals[] = {
	P_SPORT1_TFS, P_SPORT1_DTPRI, P_SPORT1_TSCLK, P_SPORT1_RFS,
	P_SPORT1_DRPRI, P_SPORT1_RSCLK, 0
};

static struct platform_device bfin_sport1_uart_device = {
	.name = "bfin-sport-uart",
	.id = 1,
	.num_resources = ARRAY_SIZE(bfin_sport1_uart_resources),
	.resource = bfin_sport1_uart_resources,
	.dev = {
		.platform_data = &bfin_sport1_peripherals,
		
	},
};
#endif
#endif

static const unsigned int cclk_vlev_datasheet[] = {
	VRPAIR(VLEV_100, 400000000),
	VRPAIR(VLEV_105, 426000000),
	VRPAIR(VLEV_110, 500000000),
	VRPAIR(VLEV_115, 533000000),
	VRPAIR(VLEV_120, 600000000),
};

static struct bfin_dpmc_platform_data bfin_dmpc_vreg_data = {
	.tuple_tab = cclk_vlev_datasheet,
	.tabsize = ARRAY_SIZE(cclk_vlev_datasheet),
	.vr_settling_time = 25 ,
};

static struct platform_device bfin_dpmc = {
	.name = "bfin dpmc",
	.dev = {
		.platform_data = &bfin_dmpc_vreg_data,
	},
};

static struct platform_device *tll6527m_devices[] __initdata = {

	&bfin_dpmc,

#if defined(CONFIG_RTC_DRV_BFIN) || defined(CONFIG_RTC_DRV_BFIN_MODULE)
	&rtc_device,
#endif

#if defined(CONFIG_USB_MUSB_HDRC) || defined(CONFIG_USB_MUSB_HDRC_MODULE)
	&musb_device,
#endif

#if defined(CONFIG_BFIN_MAC) || defined(CONFIG_BFIN_MAC_MODULE)
	&bfin_mii_bus,
	&bfin_mac_device,
#endif

#if defined(CONFIG_SPI_BFIN5XX) || defined(CONFIG_SPI_BFIN5XX_MODULE)
	&bfin_spi0_device,
#endif

#if defined(CONFIG_FB_BFIN_LQ035Q1) || defined(CONFIG_FB_BFIN_LQ035Q1_MODULE)
	&bfin_lq035q1_device,
#endif

#if defined(CONFIG_SERIAL_BFIN) || defined(CONFIG_SERIAL_BFIN_MODULE)
#ifdef CONFIG_SERIAL_BFIN_UART0
	&bfin_uart0_device,
#endif
#ifdef CONFIG_SERIAL_BFIN_UART1
	&bfin_uart1_device,
#endif
#endif

#if defined(CONFIG_BFIN_SIR) || defined(CONFIG_BFIN_SIR_MODULE)
#ifdef CONFIG_BFIN_SIR0
	&bfin_sir0_device,
#endif
#ifdef CONFIG_BFIN_SIR1
	&bfin_sir1_device,
#endif
#endif

#if defined(CONFIG_I2C_BLACKFIN_TWI) || defined(CONFIG_I2C_BLACKFIN_TWI_MODULE)
	&i2c_bfin_twi_device,
#endif

#if defined(CONFIG_SERIAL_BFIN_SPORT) \
	|| defined(CONFIG_SERIAL_BFIN_SPORT_MODULE)
#ifdef CONFIG_SERIAL_BFIN_SPORT0_UART
	&bfin_sport0_uart_device,
#endif
#ifdef CONFIG_SERIAL_BFIN_SPORT1_UART
	&bfin_sport1_uart_device,
#endif
#endif

#if defined(CONFIG_MTD_GPIO_ADDR) || defined(CONFIG_MTD_GPIO_ADDR_MODULE)
	&tll6527m_flash_device,
#endif

#if defined(CONFIG_SND_BF5XX_I2S) || defined(CONFIG_SND_BF5XX_I2S_MODULE)
	&bfin_i2s,
#endif

#if defined(CONFIG_GPIO_DECODER) || defined(CONFIG_GPIO_DECODER_MODULE)
	&spi_decoded_gpio,
#endif
};

static int __init tll6527m_init(void)
{
	printk(KERN_INFO "%s(): registering device resources\n", __func__);
	i2c_register_board_info(0, bfin_i2c_board_info,
				ARRAY_SIZE(bfin_i2c_board_info));
	platform_add_devices(tll6527m_devices, ARRAY_SIZE(tll6527m_devices));
	spi_register_board_info(bfin_spi_board_info,
				ARRAY_SIZE(bfin_spi_board_info));
	return 0;
}

arch_initcall(tll6527m_init);

static struct platform_device *tll6527m_early_devices[] __initdata = {
#if defined(CONFIG_SERIAL_BFIN_CONSOLE) || defined(CONFIG_EARLY_PRINTK)
#ifdef CONFIG_SERIAL_BFIN_UART0
	&bfin_uart0_device,
#endif
#ifdef CONFIG_SERIAL_BFIN_UART1
	&bfin_uart1_device,
#endif
#endif

#if defined(CONFIG_SERIAL_BFIN_SPORT_CONSOLE)
#ifdef CONFIG_SERIAL_BFIN_SPORT0_UART
	&bfin_sport0_uart_device,
#endif
#ifdef CONFIG_SERIAL_BFIN_SPORT1_UART
	&bfin_sport1_uart_device,
#endif
#endif
};

void __init native_machine_early_platform_add_devices(void)
{
	printk(KERN_INFO "register early platform devices\n");
	early_platform_add_devices(tll6527m_early_devices,
		ARRAY_SIZE(tll6527m_early_devices));
}

void native_machine_restart(char *cmd)
{
	
	if ((bfin_read_SYSCR() & 0x7) == 0x3)
		bfin_reset_boot_spi_cs(P_DEFAULT_BOOT_SPI_CS);
}

int bfin_get_ether_addr(char *addr)
{
	
	u32 ret;
	u64 otp_mac;
	u32 (*otp_read)(u32 page, u32 flags,
			u64 *page_content) = (void *)0xEF00001A;

	ret = otp_read(0xDF, 0x00, &otp_mac);
	if (!(ret & 0x1)) {
		char *otp_mac_p = (char *)&otp_mac;
		for (ret = 0; ret < 6; ++ret)
			addr[ret] = otp_mac_p[5 - ret];
	}
	return 0;
}
EXPORT_SYMBOL(bfin_get_ether_addr);
