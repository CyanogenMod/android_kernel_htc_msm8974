/*
 * linux/drivers/pcmcia/pxa2xx_lubbock.c
 *
 * Author:	George Davis
 * Created:	Jan 10, 2002
 * Copyright:	MontaVista Software Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Originally based upon linux/drivers/pcmcia/sa1100_neponset.c
 *
 * Lubbock PCMCIA specific routines.
 *
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/delay.h>

#include <mach/hardware.h>
#include <asm/hardware/sa1111.h>
#include <asm/mach-types.h>
#include <mach/lubbock.h>

#include "sa1111_generic.h"

static int
lubbock_pcmcia_configure_socket(struct soc_pcmcia_socket *skt,
				const socket_state_t *state)
{
	struct sa1111_pcmcia_socket *s = to_skt(skt);
	unsigned int pa_dwr_mask, pa_dwr_set, misc_mask, misc_set;
	int ret = 0;

	pa_dwr_mask = pa_dwr_set = misc_mask = misc_set = 0;


 again:
	switch (skt->nr) {
	case 0:
		pa_dwr_mask = GPIO_A0 | GPIO_A1 | GPIO_A2 | GPIO_A3;

		switch (state->Vcc) {
		case 0: 
			break;

		case 33: 
			pa_dwr_set |= GPIO_A3;
			break;

		case 50: 
			pa_dwr_set |= GPIO_A2;
			break;

		default:
			printk(KERN_ERR "%s(): unrecognized Vcc %u\n",
			       __func__, state->Vcc);
			ret = -1;
		}

		switch (state->Vpp) {
		case 0: 
			break;

		case 120: 
			pa_dwr_set |= GPIO_A1;
			break;

		default: 
			if (state->Vpp == state->Vcc)
				pa_dwr_set |= GPIO_A0;
			else {
				printk(KERN_ERR "%s(): unrecognized Vpp %u\n",
				       __func__, state->Vpp);
				ret = -1;
				break;
			}
		}
		break;

	case 1:
		misc_mask = (1 << 15) | (1 << 14);

		switch (state->Vcc) {
		case 0: 
			break;

		case 33: 
			misc_set |= 1 << 15;
			break;

		case 50: 
			misc_set |= 1 << 14;
			break;

		default:
			printk(KERN_ERR "%s(): unrecognized Vcc %u\n",
			       __func__, state->Vcc);
			ret = -1;
			break;
		}

		if (state->Vpp != state->Vcc && state->Vpp != 0) {
			printk(KERN_ERR "%s(): CF slot cannot support Vpp %u\n",
			       __func__, state->Vpp);
			ret = -1;
			break;
		}
		break;

	default:
		ret = -1;
	}

	if (ret == 0)
		ret = sa1111_pcmcia_configure_socket(skt, state);

	if (ret == 0) {
		lubbock_set_misc_wr(misc_mask, misc_set);
		sa1111_set_io(s->dev, pa_dwr_mask, pa_dwr_set);
	}

#if 1
	if (ret == 0 && state->Vcc == 33) {
		struct pcmcia_state new_state;

		mdelay(3);

		sa1111_pcmcia_socket_state(skt, &new_state);

		if (!new_state.vs_3v && !new_state.vs_Xv) {
			lubbock_set_misc_wr(misc_mask, 0);
			sa1111_set_io(s->dev, pa_dwr_mask, 0);

			mdelay(100);

			((socket_state_t *)state)->Vcc = 50;
			((socket_state_t *)state)->Vpp = 50;
			goto again;
		}
	}
#endif

	return ret;
}

static struct pcmcia_low_level lubbock_pcmcia_ops = {
	.owner			= THIS_MODULE,
	.configure_socket	= lubbock_pcmcia_configure_socket,
	.first			= 0,
	.nr			= 2,
};

#include "pxa2xx_base.h"

int pcmcia_lubbock_init(struct sa1111_dev *sadev)
{
	int ret = -ENODEV;

	if (machine_is_lubbock()) {
		sa1111_set_io_dir(sadev, GPIO_A0|GPIO_A1|GPIO_A2|GPIO_A3, 0, 0);
		sa1111_set_io(sadev, GPIO_A0|GPIO_A1|GPIO_A2|GPIO_A3, 0);
		sa1111_set_sleep_io(sadev, GPIO_A0|GPIO_A1|GPIO_A2|GPIO_A3, 0);

		
		lubbock_set_misc_wr((1 << 15) | (1 << 14), 0);

		pxa2xx_drv_pcmcia_ops(&lubbock_pcmcia_ops);
		pxa2xx_configure_sockets(&sadev->dev);
		ret = sa1111_pcmcia_add(sadev, &lubbock_pcmcia_ops,
				pxa2xx_drv_pcmcia_add_one);
	}

	return ret;
}

MODULE_LICENSE("GPL");
