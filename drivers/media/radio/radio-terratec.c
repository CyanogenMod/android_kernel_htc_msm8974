
#include <linux/module.h>	
#include <linux/init.h>		
#include <linux/ioport.h>	
#include <linux/videodev2.h>	
#include <linux/mutex.h>
#include <linux/io.h>		
#include <linux/slab.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ioctl.h>
#include "radio-isa.h"

MODULE_AUTHOR("R. Offermans & others");
MODULE_DESCRIPTION("A driver for the TerraTec ActiveRadio Standalone radio card.");
MODULE_LICENSE("GPL");
MODULE_VERSION("0.1.99");

static int io = 0x590;
static int radio_nr = -1;

module_param(radio_nr, int, 0444);
MODULE_PARM_DESC(radio_nr, "Radio device number");

#define WRT_DIS 	0x00
#define CLK_OFF		0x00
#define IIC_DATA	0x01
#define IIC_CLK		0x02
#define DATA		0x04
#define CLK_ON 		0x08
#define WRT_EN		0x10

static struct radio_isa_card *terratec_alloc(void)
{
	return kzalloc(sizeof(struct radio_isa_card), GFP_KERNEL);
}

static int terratec_s_mute_volume(struct radio_isa_card *isa, bool mute, int vol)
{
	int i;

	if (mute)
		vol = 0;
	vol = vol + (vol * 32); 
	for (i = 0; i < 8; i++) {
		if (vol & (0x80 >> i))
			outb(0x80, isa->io + 1);
		else
			outb(0x00, isa->io + 1);
	}
	return 0;
}



static int terratec_s_frequency(struct radio_isa_card *isa, u32 freq)
{
	int i;
	int p;
	int temp;
	long rest;
	unsigned char buffer[25];		

	freq = freq / 160;			
	memset(buffer, 0, sizeof(buffer));

	rest = freq * 10 + 10700;	
					
	i = 13;
	p = 10;
	temp = 102400;
	while (rest != 0) {
		if (rest % temp  == rest)
			buffer[i] = 0;
		else {
			buffer[i] = 1;
			rest = rest - temp;
		}
		i--;
		p--;
		temp = temp / 2;
	}

	for (i = 24; i > -1; i--) {	
		if (buffer[i] == 1) {
			outb(WRT_EN | DATA, isa->io);
			outb(WRT_EN | DATA | CLK_ON, isa->io);
			outb(WRT_EN | DATA, isa->io);
		} else {
			outb(WRT_EN | 0x00, isa->io);
			outb(WRT_EN | 0x00 | CLK_ON, isa->io);
		}
	}
	outb(0x00, isa->io);
	return 0;
}

static u32 terratec_g_signal(struct radio_isa_card *isa)
{
	
	return (inb(isa->io) & 2) ? 0 : 0xffff;
}

static const struct radio_isa_ops terratec_ops = {
	.alloc = terratec_alloc,
	.s_mute_volume = terratec_s_mute_volume,
	.s_frequency = terratec_s_frequency,
	.g_signal = terratec_g_signal,
};

static const int terratec_ioports[] = { 0x590 };

static struct radio_isa_driver terratec_driver = {
	.driver = {
		.match		= radio_isa_match,
		.probe		= radio_isa_probe,
		.remove		= radio_isa_remove,
		.driver		= {
			.name	= "radio-terratec",
		},
	},
	.io_params = &io,
	.radio_nr_params = &radio_nr,
	.io_ports = terratec_ioports,
	.num_of_io_ports = ARRAY_SIZE(terratec_ioports),
	.region_size = 2,
	.card = "TerraTec ActiveRadio",
	.ops = &terratec_ops,
	.has_stereo = true,
	.max_volume = 10,
};

static int __init terratec_init(void)
{
	return isa_register_driver(&terratec_driver.driver, 1);
}

static void __exit terratec_exit(void)
{
	isa_unregister_driver(&terratec_driver.driver);
}

module_init(terratec_init);
module_exit(terratec_exit);

