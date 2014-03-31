/**
@verbatim

Copyright (C) 2004,2005  ADDI-DATA GmbH for the source code of this module.

	ADDI-DATA GmbH
	Dieselstrasse 3
	D-77833 Ottersweier
	Tel: +19(0)7223/9493-0
	Fax: +49(0)7223/9493-92
	http://www.addi-data.com
	info@addi-data.com

This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation; either version 2 of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with this program; if not, write to the Free Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA

You should also find the complete GPL in the COPYING file accompanying this source code.

@endverbatim
*/

#include "hwdrv_apci1032.h"
#include <linux/delay.h>

static unsigned int ui_InterruptStatus;


int i_APCI1032_ConfigDigitalInput(struct comedi_device *dev, struct comedi_subdevice *s,
	struct comedi_insn *insn, unsigned int *data)
{
	unsigned int ui_TmpValue;

	unsigned int ul_Command1 = 0;
	unsigned int ul_Command2 = 0;
	devpriv->tsk_Current = current;

  
	
  
	if (data[0] == ADDIDATA_ENABLE) {
		ul_Command1 = ul_Command1 | data[2];
		ul_Command2 = ul_Command2 | data[3];
		outl(ul_Command1,
			devpriv->iobase + APCI1032_DIGITAL_IP_INTERRUPT_MODE1);
		outl(ul_Command2,
			devpriv->iobase + APCI1032_DIGITAL_IP_INTERRUPT_MODE2);
		if (data[1] == ADDIDATA_OR) {
			outl(0x4, devpriv->iobase + APCI1032_DIGITAL_IP_IRQ);
			ui_TmpValue =
				inl(devpriv->iobase + APCI1032_DIGITAL_IP_IRQ);
		}		
		else
			outl(0x6, devpriv->iobase + APCI1032_DIGITAL_IP_IRQ);
				
	}			
	else {
		ul_Command1 = ul_Command1 & 0xFFFF0000;
		ul_Command2 = ul_Command2 & 0xFFFF0000;
		outl(ul_Command1,
			devpriv->iobase + APCI1032_DIGITAL_IP_INTERRUPT_MODE1);
		outl(ul_Command2,
			devpriv->iobase + APCI1032_DIGITAL_IP_INTERRUPT_MODE2);
		outl(0x0, devpriv->iobase + APCI1032_DIGITAL_IP_IRQ);
	}			

	return insn->n;
}

int i_APCI1032_Read1DigitalInput(struct comedi_device *dev, struct comedi_subdevice *s,
	struct comedi_insn *insn, unsigned int *data)
{
	unsigned int ui_TmpValue = 0;
	unsigned int ui_Channel;
	ui_Channel = CR_CHAN(insn->chanspec);
	if (ui_Channel <= 31) {
		ui_TmpValue = (unsigned int) inl(devpriv->iobase + APCI1032_DIGITAL_IP);
		*data = (ui_TmpValue >> ui_Channel) & 0x1;
	}			
	else {
		
		return -EINVAL;	
	}			
	return insn->n;
}


int i_APCI1032_ReadMoreDigitalInput(struct comedi_device *dev, struct comedi_subdevice *s,
	struct comedi_insn *insn, unsigned int *data)
{
	unsigned int ui_PortValue = data[0];
	unsigned int ui_Mask = 0;
	unsigned int ui_NoOfChannels;

	ui_NoOfChannels = CR_CHAN(insn->chanspec);
	if (data[1] == 0) {
		*data = (unsigned int) inl(devpriv->iobase + APCI1032_DIGITAL_IP);
		switch (ui_NoOfChannels) {
		case 2:
			ui_Mask = 3;
			*data = (*data >> (2 * ui_PortValue)) & ui_Mask;
			break;
		case 4:
			ui_Mask = 15;
			*data = (*data >> (4 * ui_PortValue)) & ui_Mask;
			break;
		case 8:
			ui_Mask = 255;
			*data = (*data >> (8 * ui_PortValue)) & ui_Mask;
			break;
		case 16:
			ui_Mask = 65535;
			*data = (*data >> (16 * ui_PortValue)) & ui_Mask;
			break;
		case 31:
			break;
		default:
			
			return -EINVAL;	
			break;
		}		
	}			
	else {
		if (data[1] == 1)
			*data = ui_InterruptStatus;
				
	}			
	return insn->n;
}

static void v_APCI1032_Interrupt(int irq, void *d)
{
	struct comedi_device *dev = d;

	unsigned int ui_Temp;
	
	ui_Temp = inl(devpriv->iobase + APCI1032_DIGITAL_IP_IRQ);
	outl(ui_Temp & APCI1032_DIGITAL_IP_INTERRUPT_DISABLE,
		devpriv->iobase + APCI1032_DIGITAL_IP_IRQ);
	ui_InterruptStatus =
		inl(devpriv->iobase + APCI1032_DIGITAL_IP_INTERRUPT_STATUS);
	ui_InterruptStatus = ui_InterruptStatus & 0X0000FFFF;
	send_sig(SIGIO, devpriv->tsk_Current, 0);	
	outl(ui_Temp, devpriv->iobase + APCI1032_DIGITAL_IP_IRQ);	
	return;
}


int i_APCI1032_Reset(struct comedi_device *dev)
{
	outl(0x0, devpriv->iobase + APCI1032_DIGITAL_IP_IRQ);	
	inl(devpriv->iobase + APCI1032_DIGITAL_IP_INTERRUPT_STATUS);	
	outl(0x0, devpriv->iobase + APCI1032_DIGITAL_IP_INTERRUPT_MODE1);	
	outl(0x0, devpriv->iobase + APCI1032_DIGITAL_IP_INTERRUPT_MODE2);
	return 0;
}
