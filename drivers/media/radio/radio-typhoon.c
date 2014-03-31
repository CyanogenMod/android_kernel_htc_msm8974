
#include <linux/module.h>	
#include <linux/init.h>		
#include <linux/ioport.h>	
#include <linux/videodev2.h>	
#include <linux/io.h>		
#include <linux/slab.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ioctl.h>
#include "radio-isa.h"

#define DRIVER_VERSION "0.1.2"

MODULE_AUTHOR("Dr. Henrik Seidel");
MODULE_DESCRIPTION("A driver for the Typhoon radio card (a.k.a. EcoRadio).");
MODULE_LICENSE("GPL");
MODULE_VERSION("0.1.99");

#ifndef CONFIG_RADIO_TYPHOON_PORT
#define CONFIG_RADIO_TYPHOON_PORT -1
#endif

#ifndef CONFIG_RADIO_TYPHOON_MUTEFREQ
#define CONFIG_RADIO_TYPHOON_MUTEFREQ 87000
#endif

#define TYPHOON_MAX 2

static int io[TYPHOON_MAX] = { [0] = CONFIG_RADIO_TYPHOON_PORT,
			      [1 ... (TYPHOON_MAX - 1)] = -1 };
static int radio_nr[TYPHOON_MAX]	= { [0 ... (TYPHOON_MAX - 1)] = -1 };
static unsigned long mutefreq = CONFIG_RADIO_TYPHOON_MUTEFREQ;

module_param_array(io, int, NULL, 0444);
MODULE_PARM_DESC(io, "I/O addresses of the Typhoon card (0x316 or 0x336)");
module_param_array(radio_nr, int, NULL, 0444);
MODULE_PARM_DESC(radio_nr, "Radio device numbers");
module_param(mutefreq, ulong, 0);
MODULE_PARM_DESC(mutefreq, "Frequency used when muting the card (in kHz)");

struct typhoon {
	struct radio_isa_card isa;
	int muted;
};

static struct radio_isa_card *typhoon_alloc(void)
{
	struct typhoon *ty = kzalloc(sizeof(*ty), GFP_KERNEL);

	return ty ? &ty->isa : NULL;
}

static int typhoon_s_frequency(struct radio_isa_card *isa, u32 freq)
{
	unsigned long outval;
	unsigned long x;


	x = freq / 160;
	outval = (x * x + 2500) / 5000;
	outval = (outval * x + 5000) / 10000;
	outval -= (10 * x * x + 10433) / 20866;
	outval += 4 * x - 11505;

	outb_p((outval >> 8) & 0x01, isa->io + 4);
	outb_p(outval >> 9, isa->io + 6);
	outb_p(outval & 0xff, isa->io + 8);
	return 0;
}

static int typhoon_s_mute_volume(struct radio_isa_card *isa, bool mute, int vol)
{
	struct typhoon *ty = container_of(isa, struct typhoon, isa);

	if (mute)
		vol = 0;
	vol >>= 14;			
	vol &= 3;
	outb_p(vol / 2, isa->io);	
	outb_p(vol % 2, isa->io + 2);	

	if (vol == 0 && !ty->muted) {
		ty->muted = true;
		return typhoon_s_frequency(isa, mutefreq << 4);
	}
	if (vol && ty->muted) {
		ty->muted = false;
		return typhoon_s_frequency(isa, isa->freq);
	}
	return 0;
}

static const struct radio_isa_ops typhoon_ops = {
	.alloc = typhoon_alloc,
	.s_mute_volume = typhoon_s_mute_volume,
	.s_frequency = typhoon_s_frequency,
};

static const int typhoon_ioports[] = { 0x316, 0x336 };

static struct radio_isa_driver typhoon_driver = {
	.driver = {
		.match		= radio_isa_match,
		.probe		= radio_isa_probe,
		.remove		= radio_isa_remove,
		.driver		= {
			.name	= "radio-typhoon",
		},
	},
	.io_params = io,
	.radio_nr_params = radio_nr,
	.io_ports = typhoon_ioports,
	.num_of_io_ports = ARRAY_SIZE(typhoon_ioports),
	.region_size = 8,
	.card = "Typhoon Radio",
	.ops = &typhoon_ops,
	.has_stereo = true,
	.max_volume = 3,
};

static int __init typhoon_init(void)
{
	if (mutefreq < 87000 || mutefreq > 108000) {
		printk(KERN_ERR "%s: You must set a frequency (in kHz) used when muting the card,\n",
				typhoon_driver.driver.driver.name);
		printk(KERN_ERR "%s: e.g. with \"mutefreq=87500\" (87000 <= mutefreq <= 108000)\n",
				typhoon_driver.driver.driver.name);
		return -ENODEV;
	}
	return isa_register_driver(&typhoon_driver.driver, TYPHOON_MAX);
}

static void __exit typhoon_exit(void)
{
	isa_unregister_driver(&typhoon_driver.driver);
}


module_init(typhoon_init);
module_exit(typhoon_exit);

