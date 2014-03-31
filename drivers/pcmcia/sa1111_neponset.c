#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/init.h>

#include <mach/hardware.h>
#include <asm/mach-types.h>
#include <mach/neponset.h>
#include <asm/hardware/sa1111.h>

#include "sa1111_generic.h"


static int
neponset_pcmcia_configure_socket(struct soc_pcmcia_socket *skt, const socket_state_t *state)
{
	struct sa1111_pcmcia_socket *s = to_skt(skt);
	unsigned int ncr_mask, ncr_set, pa_dwr_mask, pa_dwr_set;
	int ret;

	switch (skt->nr) {
	case 0:
		pa_dwr_mask = GPIO_A0 | GPIO_A1;
		ncr_mask = NCR_A0VPP | NCR_A1VPP;

		if (state->Vpp == 0)
			ncr_set = 0;
		else if (state->Vpp == 120)
			ncr_set = NCR_A1VPP;
		else if (state->Vpp == state->Vcc)
			ncr_set = NCR_A0VPP;
		else {
			printk(KERN_ERR "%s(): unrecognized VPP %u\n",
			       __func__, state->Vpp);
			return -1;
		}
		break;

	case 1:
		pa_dwr_mask = GPIO_A2 | GPIO_A3;
		ncr_mask = 0;
		ncr_set = 0;

		if (state->Vpp != state->Vcc && state->Vpp != 0) {
			printk(KERN_ERR "%s(): CF slot cannot support VPP %u\n",
			       __func__, state->Vpp);
			return -1;
		}
		break;

	default:
		return -1;
	}

	switch (state->Vcc) {
	default:
	case 0:  pa_dwr_set = 0;		break;
	case 33: pa_dwr_set = GPIO_A1|GPIO_A2;	break;
	case 50: pa_dwr_set = GPIO_A0|GPIO_A3;	break;
	}

	ret = sa1111_pcmcia_configure_socket(skt, state);
	if (ret == 0) {
		neponset_ncr_frob(ncr_mask, ncr_set);
		sa1111_set_io(s->dev, pa_dwr_mask, pa_dwr_set);
	}

	return ret;
}

static struct pcmcia_low_level neponset_pcmcia_ops = {
	.owner			= THIS_MODULE,
	.configure_socket	= neponset_pcmcia_configure_socket,
	.first			= 0,
	.nr			= 2,
};

int pcmcia_neponset_init(struct sa1111_dev *sadev)
{
	int ret = -ENODEV;

	if (machine_is_assabet()) {
		sa1111_set_io_dir(sadev, GPIO_A0|GPIO_A1|GPIO_A2|GPIO_A3, 0, 0);
		sa1111_set_io(sadev, GPIO_A0|GPIO_A1|GPIO_A2|GPIO_A3, 0);
		sa1111_set_sleep_io(sadev, GPIO_A0|GPIO_A1|GPIO_A2|GPIO_A3, 0);
		sa11xx_drv_pcmcia_ops(&neponset_pcmcia_ops);
		ret = sa1111_pcmcia_add(sadev, &neponset_pcmcia_ops,
				sa11xx_drv_pcmcia_add_one);
	}

	return ret;
}
