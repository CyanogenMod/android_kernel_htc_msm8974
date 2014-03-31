/*
 * budget-patch.c: driver for Budget Patch,
 * hardware modification of DVB-S cards enabling full TS
 *
 * Written by Emard <emard@softhome.net>
 *
 * Original idea by Roberto Deza <rdeza@unav.es>
 *
 * Special thanks to Holger Waechtler, Michael Hunold, Marian Durkovic
 * and Metzlerbros
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 * Or, point your browser to http://www.gnu.org/copyleft/gpl.html
 *
 *
 * the project's page is at http://www.linuxtv.org/ 
 */

#include "av7110.h"
#include "av7110_hw.h"
#include "budget.h"
#include "stv0299.h"
#include "ves1x93.h"
#include "tda8083.h"

#include "bsru6.h"

DVB_DEFINE_MOD_OPT_ADAPTER_NR(adapter_nr);

#define budget_patch budget

static struct saa7146_extension budget_extension;

MAKE_BUDGET_INFO(ttbp, "TT-Budget/Patch DVB-S 1.x PCI", BUDGET_PATCH);

static struct pci_device_id pci_tbl[] = {
	MAKE_EXTENSION_PCI(ttbp,0x13c2, 0x0000),
	{
		.vendor    = 0,
	}
};

static void gpio_Set22K (struct budget *budget, int state)
{
	struct saa7146_dev *dev=budget->dev;
	dprintk(2, "budget: %p\n", budget);
	saa7146_setgpio(dev, 3, (state ? SAA7146_GPIO_OUTHI : SAA7146_GPIO_OUTLO));
}


static void DiseqcSendBit (struct budget *budget, int data)
{
	struct saa7146_dev *dev=budget->dev;
	dprintk(2, "budget: %p\n", budget);

	saa7146_setgpio(dev, 3, SAA7146_GPIO_OUTHI);
	udelay(data ? 500 : 1000);
	saa7146_setgpio(dev, 3, SAA7146_GPIO_OUTLO);
	udelay(data ? 1000 : 500);
}

static void DiseqcSendByte (struct budget *budget, int data)
{
	int i, par=1, d;

	dprintk(2, "budget: %p\n", budget);

	for (i=7; i>=0; i--) {
		d = (data>>i)&1;
		par ^= d;
		DiseqcSendBit(budget, d);
	}

	DiseqcSendBit(budget, par);
}

static int SendDiSEqCMsg (struct budget *budget, int len, u8 *msg, unsigned long burst)
{
	struct saa7146_dev *dev=budget->dev;
	int i;

	dprintk(2, "budget: %p\n", budget);

	saa7146_setgpio(dev, 3, SAA7146_GPIO_OUTLO);
	mdelay(16);

	for (i=0; i<len; i++)
		DiseqcSendByte(budget, msg[i]);

	mdelay(16);

	if (burst!=-1) {
		if (burst)
			DiseqcSendByte(budget, 0xff);
		else {
			saa7146_setgpio(dev, 3, SAA7146_GPIO_OUTHI);
			mdelay(12);
			udelay(500);
			saa7146_setgpio(dev, 3, SAA7146_GPIO_OUTLO);
		}
		msleep(20);
	}

	return 0;
}

static int budget_set_tone(struct dvb_frontend* fe, fe_sec_tone_mode_t tone)
{
	struct budget* budget = (struct budget*) fe->dvb->priv;

	switch (tone) {
	case SEC_TONE_ON:
		gpio_Set22K (budget, 1);
		break;

	case SEC_TONE_OFF:
		gpio_Set22K (budget, 0);
		break;

	default:
		return -EINVAL;
	}

	return 0;
}

static int budget_diseqc_send_master_cmd(struct dvb_frontend* fe, struct dvb_diseqc_master_cmd* cmd)
{
	struct budget* budget = (struct budget*) fe->dvb->priv;

	SendDiSEqCMsg (budget, cmd->msg_len, cmd->msg, 0);

	return 0;
}

static int budget_diseqc_send_burst(struct dvb_frontend* fe, fe_sec_mini_cmd_t minicmd)
{
	struct budget* budget = (struct budget*) fe->dvb->priv;

	SendDiSEqCMsg (budget, 0, NULL, minicmd);

	return 0;
}

static int budget_av7110_send_fw_cmd(struct budget_patch *budget, u16* buf, int length)
{
	int i;

	dprintk(2, "budget: %p\n", budget);

	for (i = 2; i < length; i++)
	{
		  ttpci_budget_debiwrite(budget, DEBINOSWAP, COMMAND + 2*i, 2, (u32) buf[i], 0,0);
		  msleep(5);
	}
	if (length)
		  ttpci_budget_debiwrite(budget, DEBINOSWAP, COMMAND + 2, 2, (u32) buf[1], 0,0);
	else
		  ttpci_budget_debiwrite(budget, DEBINOSWAP, COMMAND + 2, 2, 0, 0,0);
	msleep(5);
	ttpci_budget_debiwrite(budget, DEBINOSWAP, COMMAND, 2, (u32) buf[0], 0,0);
	msleep(5);
	return 0;
}

static void av7110_set22k(struct budget_patch *budget, int state)
{
	u16 buf[2] = {( COMTYPE_AUDIODAC << 8) | (state ? ON22K : OFF22K), 0};

	dprintk(2, "budget: %p\n", budget);
	budget_av7110_send_fw_cmd(budget, buf, 2);
}

static int av7110_send_diseqc_msg(struct budget_patch *budget, int len, u8 *msg, int burst)
{
	int i;
	u16 buf[18] = { ((COMTYPE_AUDIODAC << 8) | SendDiSEqC),
		16, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

	dprintk(2, "budget: %p\n", budget);

	if (len>10)
		len=10;

	buf[1] = len+2;
	buf[2] = len;

	if (burst != -1)
		buf[3]=burst ? 0x01 : 0x00;
	else
		buf[3]=0xffff;

	for (i=0; i<len; i++)
		buf[i+4]=msg[i];

	budget_av7110_send_fw_cmd(budget, buf, 18);
	return 0;
}

static int budget_patch_set_tone(struct dvb_frontend* fe, fe_sec_tone_mode_t tone)
{
	struct budget_patch* budget = (struct budget_patch*) fe->dvb->priv;

	switch (tone) {
	case SEC_TONE_ON:
		av7110_set22k (budget, 1);
		break;

	case SEC_TONE_OFF:
		av7110_set22k (budget, 0);
		break;

	default:
		return -EINVAL;
	}

	return 0;
}

static int budget_patch_diseqc_send_master_cmd(struct dvb_frontend* fe, struct dvb_diseqc_master_cmd* cmd)
{
	struct budget_patch* budget = (struct budget_patch*) fe->dvb->priv;

	av7110_send_diseqc_msg (budget, cmd->msg_len, cmd->msg, 0);

	return 0;
}

static int budget_patch_diseqc_send_burst(struct dvb_frontend* fe, fe_sec_mini_cmd_t minicmd)
{
	struct budget_patch* budget = (struct budget_patch*) fe->dvb->priv;

	av7110_send_diseqc_msg (budget, 0, NULL, minicmd);

	return 0;
}

static int alps_bsrv2_tuner_set_params(struct dvb_frontend *fe)
{
	struct dtv_frontend_properties *p = &fe->dtv_property_cache;
	struct budget_patch* budget = (struct budget_patch*) fe->dvb->priv;
	u8 pwr = 0;
	u8 buf[4];
	struct i2c_msg msg = { .addr = 0x61, .flags = 0, .buf = buf, .len = sizeof(buf) };
	u32 div = (p->frequency + 479500) / 125;

	if (p->frequency > 2000000)
		pwr = 3;
	else if (p->frequency > 1800000)
		pwr = 2;
	else if (p->frequency > 1600000)
		pwr = 1;
	else if (p->frequency > 1200000)
		pwr = 0;
	else if (p->frequency >= 1100000)
		pwr = 1;
	else pwr = 2;

	buf[0] = (div >> 8) & 0x7f;
	buf[1] = div & 0xff;
	buf[2] = ((div & 0x18000) >> 10) | 0x95;
	buf[3] = (pwr << 6) | 0x30;

	
	

	if (fe->ops.i2c_gate_ctrl)
		fe->ops.i2c_gate_ctrl(fe, 1);
	if (i2c_transfer (&budget->i2c_adap, &msg, 1) != 1)
		return -EIO;
	return 0;
}

static struct ves1x93_config alps_bsrv2_config = {
	.demod_address = 0x08,
	.xin = 90100000UL,
	.invert_pwm = 0,
};

static int grundig_29504_451_tuner_set_params(struct dvb_frontend *fe)
{
	struct dtv_frontend_properties *p = &fe->dtv_property_cache;
	struct budget_patch* budget = (struct budget_patch*) fe->dvb->priv;
	u32 div;
	u8 data[4];
	struct i2c_msg msg = { .addr = 0x61, .flags = 0, .buf = data, .len = sizeof(data) };

	div = p->frequency / 125;
	data[0] = (div >> 8) & 0x7f;
	data[1] = div & 0xff;
	data[2] = 0x8e;
	data[3] = 0x00;

	if (fe->ops.i2c_gate_ctrl)
		fe->ops.i2c_gate_ctrl(fe, 1);
	if (i2c_transfer (&budget->i2c_adap, &msg, 1) != 1)
		return -EIO;
	return 0;
}

static struct tda8083_config grundig_29504_451_config = {
	.demod_address = 0x68,
};

static void frontend_init(struct budget_patch* budget)
{
	switch(budget->dev->pci->subsystem_device) {
	case 0x0000: 
	case 0x1013: 

		
		budget->dvb_frontend = dvb_attach(ves1x93_attach, &alps_bsrv2_config, &budget->i2c_adap);
		if (budget->dvb_frontend) {
			budget->dvb_frontend->ops.tuner_ops.set_params = alps_bsrv2_tuner_set_params;
			budget->dvb_frontend->ops.diseqc_send_master_cmd = budget_patch_diseqc_send_master_cmd;
			budget->dvb_frontend->ops.diseqc_send_burst = budget_patch_diseqc_send_burst;
			budget->dvb_frontend->ops.set_tone = budget_patch_set_tone;
			break;
		}

		
		budget->dvb_frontend = dvb_attach(stv0299_attach, &alps_bsru6_config, &budget->i2c_adap);
		if (budget->dvb_frontend) {
			budget->dvb_frontend->ops.tuner_ops.set_params = alps_bsru6_tuner_set_params;
			budget->dvb_frontend->tuner_priv = &budget->i2c_adap;

			budget->dvb_frontend->ops.diseqc_send_master_cmd = budget_diseqc_send_master_cmd;
			budget->dvb_frontend->ops.diseqc_send_burst = budget_diseqc_send_burst;
			budget->dvb_frontend->ops.set_tone = budget_set_tone;
			break;
		}

		
		budget->dvb_frontend = dvb_attach(tda8083_attach, &grundig_29504_451_config, &budget->i2c_adap);
		if (budget->dvb_frontend) {
			budget->dvb_frontend->ops.tuner_ops.set_params = grundig_29504_451_tuner_set_params;
			budget->dvb_frontend->ops.diseqc_send_master_cmd = budget_diseqc_send_master_cmd;
			budget->dvb_frontend->ops.diseqc_send_burst = budget_diseqc_send_burst;
			budget->dvb_frontend->ops.set_tone = budget_set_tone;
			break;
		}
		break;
	}

	if (budget->dvb_frontend == NULL) {
		printk("dvb-ttpci: A frontend driver was not found for device [%04x:%04x] subsystem [%04x:%04x]\n",
		       budget->dev->pci->vendor,
		       budget->dev->pci->device,
		       budget->dev->pci->subsystem_vendor,
		       budget->dev->pci->subsystem_device);
	} else {
		if (dvb_register_frontend(&budget->dvb_adapter, budget->dvb_frontend)) {
			printk("budget-av: Frontend registration failed!\n");
			dvb_frontend_detach(budget->dvb_frontend);
			budget->dvb_frontend = NULL;
		}
	}
}

/* written by Emard */
static int budget_patch_attach (struct saa7146_dev* dev, struct saa7146_pci_extension_data *info)
{
	struct budget_patch *budget;
	int err;
	int count = 0;
	int detected = 0;

#define PATCH_RESET 0
#define RPS_IRQ 0
#define HPS_SETUP 0
#if PATCH_RESET
	saa7146_write(dev, MC1, MASK_31);
	msleep(40);
#endif
#if HPS_SETUP
	
	
	saa7146_write(dev, DD1_STREAM_B, 0);
	
	saa7146_write(dev, DD1_INIT, 0x00000200);  
	saa7146_write(dev, BRS_CTRL, 0x00000000);  

	
	

	
	saa7146_write(dev, HPS_H_PRESCALE, 0);                  
	saa7146_write(dev, HPS_H_SCALE, 0);                     
	saa7146_write(dev, BCS_CTRL, 0);                        
	saa7146_write(dev, HPS_V_SCALE, 0);                     
	saa7146_write(dev, HPS_V_GAIN, 0);                      
	saa7146_write(dev, CHROMA_KEY_RANGE, 0);                
	saa7146_write(dev, CLIP_FORMAT_CTRL, 0);                
	
	saa7146_write(dev, HPS_CTRL, (1<<30) | (0<<29) | (1<<28) | (0<<12) );
	saa7146_write(dev, MC2,
	  0 * (MASK_08 | MASK_24)  |   
	  0 * (MASK_09 | MASK_25)  |   
	  0 * (MASK_10 | MASK_26)  |   
	  1 * (MASK_06 | MASK_22)  |   
	  1 * (MASK_05 | MASK_21)  |   
	  0 * (MASK_01 | MASK_15)      
	   );
#endif
	
	saa7146_write(dev, MC1, ( MASK_29 | MASK_28));
	
	saa7146_write(dev, RPS_TOV1, 0);

	
	
	
	
	
	
	count = 0;
#if 0
	WRITE_RPS1(CMD_UPLOAD |
	  MASK_10 | MASK_09 | MASK_08 | MASK_06 | MASK_05 | MASK_04 | MASK_03 | MASK_02 );
#endif
	WRITE_RPS1(CMD_PAUSE | EVT_VBI_B);
	WRITE_RPS1(CMD_WR_REG_MASK | (GPIO_CTRL>>2));
	WRITE_RPS1(GPIO3_MSK);
	WRITE_RPS1(SAA7146_GPIO_OUTLO<<24);
#if RPS_IRQ
	
	WRITE_RPS1(CMD_INTERRUPT);
	
	WRITE_RPS1(CMD_NOP);
	
	WRITE_RPS1(CMD_INTERRUPT);
#endif
	WRITE_RPS1(CMD_STOP);

#if RPS_IRQ
	
	
	
	saa7146_write(dev, EC1SSR, (0x03<<2) | 3 );
	
	saa7146_write(dev, ECT1R,  0x3fff );
#endif
	
	saa7146_setgpio(dev, 3, SAA7146_GPIO_OUTLO);
	
	saa7146_write(dev, RPS_ADDR1, dev->d_rps1.dma_handle);
	
	saa7146_write(dev, MC1, (MASK_13 | MASK_29 ));


	mdelay(50);
	saa7146_setgpio(dev, 3, SAA7146_GPIO_OUTHI);
	mdelay(150);


	if( (saa7146_read(dev, GPIO_CTRL) & 0x10000000) == 0)
		detected = 1;

#if RPS_IRQ
	printk("Event Counter 1 0x%04x\n", saa7146_read(dev, EC1R) & 0x3fff );
#endif
	
	saa7146_write(dev, MC1, ( MASK_29 ));

	if(detected == 0)
		printk("budget-patch not detected or saa7146 in non-default state.\n"
		       "try enabling ressetting of 7146 with MASK_31 in MC1 register\n");

	else
		printk("BUDGET-PATCH DETECTED.\n");




	
	count = 0;


	
	WRITE_RPS1(CMD_PAUSE | EVT_HS);
	
	WRITE_RPS1(CMD_WR_REG_MASK | (GPIO_CTRL>>2));
	WRITE_RPS1(GPIO3_MSK);
	WRITE_RPS1(SAA7146_GPIO_OUTHI<<24);
#if RPS_IRQ
	
	WRITE_RPS1(CMD_INTERRUPT);
#endif
	
	WRITE_RPS1(CMD_PAUSE | RPS_INV | EVT_HS);
	
	WRITE_RPS1(CMD_WR_REG_MASK | (GPIO_CTRL>>2));
	WRITE_RPS1(GPIO3_MSK);
	WRITE_RPS1(SAA7146_GPIO_OUTLO<<24);
#if RPS_IRQ
	
	WRITE_RPS1(CMD_INTERRUPT);
#endif
	
	WRITE_RPS1(CMD_JUMP);
	WRITE_RPS1(dev->d_rps1.dma_handle);

	
	saa7146_setgpio(dev, 3, SAA7146_GPIO_OUTLO);
	
	saa7146_write(dev, RPS_ADDR1, dev->d_rps1.dma_handle);

	if (!(budget = kmalloc (sizeof(struct budget_patch), GFP_KERNEL)))
		return -ENOMEM;

	dprintk(2, "budget: %p\n", budget);

	err = ttpci_budget_init(budget, dev, info, THIS_MODULE, adapter_nr);
	if (err) {
		kfree(budget);
		return err;
	}

	
	
	
	
	
	
	
	
	saa7146_write(dev, RPS_THRESH1, budget->buffer_height | MASK_12 );

	
	
	saa7146_write(dev, MC1, (MASK_13 | MASK_29));


	dev->ext_priv = budget;

	budget->dvb_adapter.priv = budget;
	frontend_init(budget);

	ttpci_budget_init_hooks(budget);

	return 0;
}

static int budget_patch_detach (struct saa7146_dev* dev)
{
	struct budget_patch *budget = (struct budget_patch*) dev->ext_priv;
	int err;

	if (budget->dvb_frontend) {
		dvb_unregister_frontend(budget->dvb_frontend);
		dvb_frontend_detach(budget->dvb_frontend);
	}
	err = ttpci_budget_deinit (budget);

	kfree (budget);

	return err;
}

static int __init budget_patch_init(void)
{
	return saa7146_register_extension(&budget_extension);
}

static void __exit budget_patch_exit(void)
{
	saa7146_unregister_extension(&budget_extension);
}

static struct saa7146_extension budget_extension = {
	.name           = "budget_patch dvb",
	.flags          = 0,

	.module         = THIS_MODULE,
	.pci_tbl        = pci_tbl,
	.attach         = budget_patch_attach,
	.detach         = budget_patch_detach,

	.irq_mask       = MASK_10,
	.irq_func       = ttpci_budget_irq10_handler,
};

module_init(budget_patch_init);
module_exit(budget_patch_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Emard, Roberto Deza, Holger Waechtler, Michael Hunold, others");
MODULE_DESCRIPTION("Driver for full TS modified DVB-S SAA7146+AV7110 "
		   "based so-called Budget Patch cards");
