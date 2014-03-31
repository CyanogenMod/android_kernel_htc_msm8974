/*
 *  Driver for Gravis UltraSound Extreme soundcards
 *  Copyright (c) by Jaroslav Kysela <perex@perex.cz>
 *
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */

#include <linux/init.h>
#include <linux/err.h>
#include <linux/isa.h>
#include <linux/delay.h>
#include <linux/time.h>
#include <linux/module.h>
#include <asm/dma.h>
#include <sound/core.h>
#include <sound/gus.h>
#include <sound/es1688.h>
#include <sound/mpu401.h>
#include <sound/opl3.h>
#define SNDRV_LEGACY_AUTO_PROBE
#define SNDRV_LEGACY_FIND_FREE_IRQ
#define SNDRV_LEGACY_FIND_FREE_DMA
#include <sound/initval.h>

#define CRD_NAME "Gravis UltraSound Extreme"
#define DEV_NAME "gusextreme"

MODULE_DESCRIPTION(CRD_NAME);
MODULE_AUTHOR("Jaroslav Kysela <perex@perex.cz>");
MODULE_LICENSE("GPL");
MODULE_SUPPORTED_DEVICE("{{Gravis,UltraSound Extreme}}");

static int index[SNDRV_CARDS] = SNDRV_DEFAULT_IDX;	
static char *id[SNDRV_CARDS] = SNDRV_DEFAULT_STR;	
static bool enable[SNDRV_CARDS] = SNDRV_DEFAULT_ENABLE;	
static long port[SNDRV_CARDS] = SNDRV_DEFAULT_PORT;	
static long gf1_port[SNDRV_CARDS] = {[0 ... (SNDRV_CARDS) - 1] = -1}; 
static long mpu_port[SNDRV_CARDS] = {[0 ... (SNDRV_CARDS) - 1] = -1}; 
static int irq[SNDRV_CARDS] = SNDRV_DEFAULT_IRQ;	
static int mpu_irq[SNDRV_CARDS] = SNDRV_DEFAULT_IRQ;	
static int gf1_irq[SNDRV_CARDS] = SNDRV_DEFAULT_IRQ;	
static int dma8[SNDRV_CARDS] = SNDRV_DEFAULT_DMA;	
static int dma1[SNDRV_CARDS] = SNDRV_DEFAULT_DMA;
static int joystick_dac[SNDRV_CARDS] = {[0 ... (SNDRV_CARDS - 1)] = 29};
				
static int channels[SNDRV_CARDS] = {[0 ... (SNDRV_CARDS - 1)] = 24};
static int pcm_channels[SNDRV_CARDS] = {[0 ... (SNDRV_CARDS - 1)] = 2};

module_param_array(index, int, NULL, 0444);
MODULE_PARM_DESC(index, "Index value for " CRD_NAME " soundcard.");
module_param_array(id, charp, NULL, 0444);
MODULE_PARM_DESC(id, "ID string for " CRD_NAME " soundcard.");
module_param_array(enable, bool, NULL, 0444);
MODULE_PARM_DESC(enable, "Enable " CRD_NAME " soundcard.");
module_param_array(port, long, NULL, 0444);
MODULE_PARM_DESC(port, "Port # for " CRD_NAME " driver.");
module_param_array(gf1_port, long, NULL, 0444);
MODULE_PARM_DESC(gf1_port, "GF1 port # for " CRD_NAME " driver (optional).");
module_param_array(mpu_port, long, NULL, 0444);
MODULE_PARM_DESC(mpu_port, "MPU-401 port # for " CRD_NAME " driver.");
module_param_array(irq, int, NULL, 0444);
MODULE_PARM_DESC(irq, "IRQ # for " CRD_NAME " driver.");
module_param_array(mpu_irq, int, NULL, 0444);
MODULE_PARM_DESC(mpu_irq, "MPU-401 IRQ # for " CRD_NAME " driver.");
module_param_array(gf1_irq, int, NULL, 0444);
MODULE_PARM_DESC(gf1_irq, "GF1 IRQ # for " CRD_NAME " driver.");
module_param_array(dma8, int, NULL, 0444);
MODULE_PARM_DESC(dma8, "8-bit DMA # for " CRD_NAME " driver.");
module_param_array(dma1, int, NULL, 0444);
MODULE_PARM_DESC(dma1, "GF1 DMA # for " CRD_NAME " driver.");
module_param_array(joystick_dac, int, NULL, 0444);
MODULE_PARM_DESC(joystick_dac, "Joystick DAC level 0.59V-4.52V or 0.389V-2.98V for " CRD_NAME " driver.");
module_param_array(channels, int, NULL, 0444);
MODULE_PARM_DESC(channels, "GF1 channels for " CRD_NAME " driver.");
module_param_array(pcm_channels, int, NULL, 0444);
MODULE_PARM_DESC(pcm_channels, "Reserved PCM channels for " CRD_NAME " driver.");

static int __devinit snd_gusextreme_match(struct device *dev, unsigned int n)
{
	return enable[n];
}

static int __devinit snd_gusextreme_es1688_create(struct snd_card *card,
		struct snd_es1688 *chip, struct device *dev, unsigned int n)
{
	static long possible_ports[] = {0x220, 0x240, 0x260};
	static int possible_irqs[] = {5, 9, 10, 7, -1};
	static int possible_dmas[] = {1, 3, 0, -1};

	int i, error;

	if (irq[n] == SNDRV_AUTO_IRQ) {
		irq[n] = snd_legacy_find_free_irq(possible_irqs);
		if (irq[n] < 0) {
			dev_err(dev, "unable to find a free IRQ for ES1688\n");
			return -EBUSY;
		}
	}
	if (dma8[n] == SNDRV_AUTO_DMA) {
		dma8[n] = snd_legacy_find_free_dma(possible_dmas);
		if (dma8[n] < 0) {
			dev_err(dev, "unable to find a free DMA for ES1688\n");
			return -EBUSY;
		}
	}

	if (port[n] != SNDRV_AUTO_PORT)
		return snd_es1688_create(card, chip, port[n], mpu_port[n],
				irq[n], mpu_irq[n], dma8[n], ES1688_HW_1688);

	i = 0;
	do {
		port[n] = possible_ports[i];
		error = snd_es1688_create(card, chip, port[n], mpu_port[n],
				irq[n], mpu_irq[n], dma8[n], ES1688_HW_1688);
	} while (error < 0 && ++i < ARRAY_SIZE(possible_ports));

	return error;
}

static int __devinit snd_gusextreme_gus_card_create(struct snd_card *card,
		struct device *dev, unsigned int n, struct snd_gus_card **rgus)
{
	static int possible_irqs[] = {11, 12, 15, 9, 5, 7, 3, -1};
	static int possible_dmas[] = {5, 6, 7, 3, 1, -1};

	if (gf1_irq[n] == SNDRV_AUTO_IRQ) {
		gf1_irq[n] = snd_legacy_find_free_irq(possible_irqs);
		if (gf1_irq[n] < 0) {
			dev_err(dev, "unable to find a free IRQ for GF1\n");
			return -EBUSY;
		}
	}
	if (dma1[n] == SNDRV_AUTO_DMA) {
		dma1[n] = snd_legacy_find_free_dma(possible_dmas);
		if (dma1[n] < 0) {
			dev_err(dev, "unable to find a free DMA for GF1\n");
			return -EBUSY;
		}
	}
	return snd_gus_create(card, gf1_port[n], gf1_irq[n], dma1[n], -1,
			0, channels[n], pcm_channels[n], 0, rgus);
}

static int __devinit snd_gusextreme_detect(struct snd_gus_card *gus,
	struct snd_es1688 *es1688)
{
	unsigned long flags;
	unsigned char d;


	spin_lock_irqsave(&es1688->mixer_lock, flags);
	snd_es1688_mixer_write(es1688, 0x40, 0x0b);	
	spin_unlock_irqrestore(&es1688->mixer_lock, flags);

	spin_lock_irqsave(&es1688->reg_lock, flags);
	outb(gus->gf1.port & 0x040 ? 2 : 0, ES1688P(es1688, INIT1));
	outb(0, 0x201);
	outb(gus->gf1.port & 0x020 ? 2 : 0, ES1688P(es1688, INIT1));
	outb(0, 0x201);
	outb(gus->gf1.port & 0x010 ? 3 : 1, ES1688P(es1688, INIT1));
	spin_unlock_irqrestore(&es1688->reg_lock, flags);

	udelay(100);

	snd_gf1_i_write8(gus, SNDRV_GF1_GB_RESET, 0);	
	if (((d = snd_gf1_i_look8(gus, SNDRV_GF1_GB_RESET)) & 0x07) != 0) {
		snd_printdd("[0x%lx] check 1 failed - 0x%x\n", gus->gf1.port, d);
		return -EIO;
	}
	udelay(160);
	snd_gf1_i_write8(gus, SNDRV_GF1_GB_RESET, 1);	
	udelay(160);
	if (((d = snd_gf1_i_look8(gus, SNDRV_GF1_GB_RESET)) & 0x07) != 1) {
		snd_printdd("[0x%lx] check 2 failed - 0x%x\n", gus->gf1.port, d);
		return -EIO;
	}

	return 0;
}

static int __devinit snd_gusextreme_mixer(struct snd_card *card)
{
	struct snd_ctl_elem_id id1, id2;
	int error;

	memset(&id1, 0, sizeof(id1));
	memset(&id2, 0, sizeof(id2));
	id1.iface = id2.iface = SNDRV_CTL_ELEM_IFACE_MIXER;

	
	strcpy(id1.name, "Aux Playback Volume");
	strcpy(id2.name, "Synth Playback Volume");
	error = snd_ctl_rename_id(card, &id1, &id2);
	if (error < 0)
		return error;

	
	strcpy(id1.name, "Master Playback Switch");
	strcpy(id2.name, "Synth Playback Switch");
	error = snd_ctl_rename_id(card, &id1, &id2);
	if (error < 0)
		return error;

	return 0;
}

static int __devinit snd_gusextreme_probe(struct device *dev, unsigned int n)
{
	struct snd_card *card;
	struct snd_gus_card *gus;
	struct snd_es1688 *es1688;
	struct snd_opl3 *opl3;
	int error;

	error = snd_card_create(index[n], id[n], THIS_MODULE,
				sizeof(struct snd_es1688), &card);
	if (error < 0)
		return error;

	es1688 = card->private_data;

	if (mpu_port[n] == SNDRV_AUTO_PORT)
		mpu_port[n] = 0;

	if (mpu_irq[n] > 15)
		mpu_irq[n] = -1;

	error = snd_gusextreme_es1688_create(card, es1688, dev, n);
	if (error < 0)
		goto out;

	if (gf1_port[n] < 0)
		gf1_port[n] = es1688->port + 0x20;

	error = snd_gusextreme_gus_card_create(card, dev, n, &gus);
	if (error < 0)
		goto out;

	error = snd_gusextreme_detect(gus, es1688);
	if (error < 0)
		goto out;

	gus->joystick_dac = joystick_dac[n];

	error = snd_gus_initialize(gus);
	if (error < 0)
		goto out;

	error = -ENODEV;
	if (!gus->ess_flag) {
		dev_err(dev, "GUS Extreme soundcard was not "
			"detected at 0x%lx\n", gus->gf1.port);
		goto out;
	}
	gus->codec_flag = 1;

	error = snd_es1688_pcm(card, es1688, 0, NULL);
	if (error < 0)
		goto out;

	error = snd_es1688_mixer(card, es1688);
	if (error < 0)
		goto out;

	snd_component_add(card, "ES1688");

	if (pcm_channels[n] > 0) {
		error = snd_gf1_pcm_new(gus, 1, 1, NULL);
		if (error < 0)
			goto out;
	}

	error = snd_gf1_new_mixer(gus);
	if (error < 0)
		goto out;

	error = snd_gusextreme_mixer(card);
	if (error < 0)
		goto out;

	if (snd_opl3_create(card, es1688->port, es1688->port + 2,
			OPL3_HW_OPL3, 0, &opl3) < 0)
		dev_warn(dev, "opl3 not detected at 0x%lx\n", es1688->port);
	else {
		error = snd_opl3_hwdep_new(opl3, 0, 2, NULL);
		if (error < 0)
			goto out;
	}

	if (es1688->mpu_port >= 0x300) {
		error = snd_mpu401_uart_new(card, 0, MPU401_HW_ES1688,
				es1688->mpu_port, 0, mpu_irq[n], NULL);
		if (error < 0)
			goto out;
	}

	sprintf(card->longname, "Gravis UltraSound Extreme at 0x%lx, "
		"irq %i&%i, dma %i&%i", es1688->port,
		gus->gf1.irq, es1688->irq, gus->gf1.dma1, es1688->dma8);

	snd_card_set_dev(card, dev);

	error = snd_card_register(card);
	if (error < 0)
		goto out;

	dev_set_drvdata(dev, card);
	return 0;

out:	snd_card_free(card);
	return error;
}

static int __devexit snd_gusextreme_remove(struct device *dev, unsigned int n)
{
	snd_card_free(dev_get_drvdata(dev));
	dev_set_drvdata(dev, NULL);
	return 0;
}

static struct isa_driver snd_gusextreme_driver = {
	.match		= snd_gusextreme_match,
	.probe		= snd_gusextreme_probe,
	.remove		= __devexit_p(snd_gusextreme_remove),
#if 0	
	.suspend	= snd_gusextreme_suspend,
	.resume		= snd_gusextreme_resume,
#endif
	.driver		= {
		.name	= DEV_NAME
	}
};

static int __init alsa_card_gusextreme_init(void)
{
	return isa_register_driver(&snd_gusextreme_driver, SNDRV_CARDS);
}

static void __exit alsa_card_gusextreme_exit(void)
{
	isa_unregister_driver(&snd_gusextreme_driver);
}

module_init(alsa_card_gusextreme_init);
module_exit(alsa_card_gusextreme_exit);
