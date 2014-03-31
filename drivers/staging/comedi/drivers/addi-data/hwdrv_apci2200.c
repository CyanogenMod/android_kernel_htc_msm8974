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

#include "hwdrv_apci2200.h"

int i_APCI2200_Read1DigitalInput(struct comedi_device *dev, struct comedi_subdevice *s,
	struct comedi_insn *insn, unsigned int *data)
{
	unsigned int ui_TmpValue = 0;
	unsigned int ui_Channel;
	ui_Channel = CR_CHAN(insn->chanspec);
	if (ui_Channel <= 7) {
		ui_TmpValue = (unsigned int) inw(devpriv->iobase + APCI2200_DIGITAL_IP);
		*data = (ui_TmpValue >> ui_Channel) & 0x1;
	}			
	else {
		printk("\nThe specified channel does not exist\n");
		return -EINVAL;	
	}			

	return insn->n;
}


int i_APCI2200_ReadMoreDigitalInput(struct comedi_device *dev, struct comedi_subdevice *s,
	struct comedi_insn *insn, unsigned int *data)
{

	unsigned int ui_PortValue = data[0];
	unsigned int ui_Mask = 0;
	unsigned int ui_NoOfChannels;

	ui_NoOfChannels = CR_CHAN(insn->chanspec);

	*data = (unsigned int) inw(devpriv->iobase + APCI2200_DIGITAL_IP);
	switch (ui_NoOfChannels) {
	case 2:
		ui_Mask = 3;
		*data = (*data >> (2 * ui_PortValue)) & ui_Mask;
		break;
	case 4:
		ui_Mask = 15;
		*data = (*data >> (4 * ui_PortValue)) & ui_Mask;
		break;
	case 7:
		break;

	default:
		printk("\nWrong parameters\n");
		return -EINVAL;	
		break;
	}			

	return insn->n;
}

int i_APCI2200_ConfigDigitalOutput(struct comedi_device *dev, struct comedi_subdevice *s,
	struct comedi_insn *insn, unsigned int *data)
{
	devpriv->b_OutputMemoryStatus = data[0];
	return insn->n;
}


int i_APCI2200_WriteDigitalOutput(struct comedi_device *dev, struct comedi_subdevice *s,
	struct comedi_insn *insn, unsigned int *data)
{
	unsigned int ui_Temp, ui_Temp1;
	unsigned int ui_NoOfChannel = CR_CHAN(insn->chanspec);	
	if (devpriv->b_OutputMemoryStatus) {
		ui_Temp = inw(devpriv->iobase + APCI2200_DIGITAL_OP);

	}			
	else {
		ui_Temp = 0;
	}			
	if (data[3] == 0) {
		if (data[1] == 0) {
			data[0] = (data[0] << ui_NoOfChannel) | ui_Temp;
			outw(data[0], devpriv->iobase + APCI2200_DIGITAL_OP);
		}		
		else {
			if (data[1] == 1) {
				switch (ui_NoOfChannel) {

				case 2:
					data[0] =
						(data[0] << (2 *
							data[2])) | ui_Temp;
					break;

				case 4:
					data[0] =
						(data[0] << (4 *
							data[2])) | ui_Temp;
					break;

				case 8:
					data[0] =
						(data[0] << (8 *
							data[2])) | ui_Temp;
					break;
				case 15:
					data[0] = data[0] | ui_Temp;
					break;
				default:
					comedi_error(dev, " chan spec wrong");
					return -EINVAL;	

				}	

				outw(data[0],
					devpriv->iobase + APCI2200_DIGITAL_OP);
			}	
			else {
				printk("\nSpecified channel not supported\n");
			}	
		}		
	}			
	else {
		if (data[3] == 1) {
			if (data[1] == 0) {
				data[0] = ~data[0] & 0x1;
				ui_Temp1 = 1;
				ui_Temp1 = ui_Temp1 << ui_NoOfChannel;
				ui_Temp = ui_Temp | ui_Temp1;
				data[0] = (data[0] << ui_NoOfChannel) ^ 0xffff;
				data[0] = data[0] & ui_Temp;
				outw(data[0],
					devpriv->iobase + APCI2200_DIGITAL_OP);
			}	
			else {
				if (data[1] == 1) {
					switch (ui_NoOfChannel) {

					case 2:
						data[0] = ~data[0] & 0x3;
						ui_Temp1 = 3;
						ui_Temp1 =
							ui_Temp1 << 2 * data[2];
						ui_Temp = ui_Temp | ui_Temp1;
						data[0] =
							((data[0] << (2 *
									data
									[2])) ^
							0xffff) & ui_Temp;
						break;

					case 4:
						data[0] = ~data[0] & 0xf;
						ui_Temp1 = 15;
						ui_Temp1 =
							ui_Temp1 << 4 * data[2];
						ui_Temp = ui_Temp | ui_Temp1;
						data[0] =
							((data[0] << (4 *
									data
									[2])) ^
							0xffff) & ui_Temp;
						break;

					case 8:
						data[0] = ~data[0] & 0xff;
						ui_Temp1 = 255;
						ui_Temp1 =
							ui_Temp1 << 8 * data[2];
						ui_Temp = ui_Temp | ui_Temp1;
						data[0] =
							((data[0] << (8 *
									data
									[2])) ^
							0xffff) & ui_Temp;
						break;
					case 15:
						break;

					default:
						comedi_error(dev,
							" chan spec wrong");
						return -EINVAL;	

					}	

					outw(data[0],
						devpriv->iobase +
						APCI2200_DIGITAL_OP);
				}	
				else {
					printk("\nSpecified channel not supported\n");
				}	
			}	
		}		
		else {
			printk("\nSpecified functionality does not exist\n");
			return -EINVAL;
		}		
	}			
	return insn->n;
}


int i_APCI2200_ReadDigitalOutput(struct comedi_device *dev, struct comedi_subdevice *s,
	struct comedi_insn *insn, unsigned int *data)
{

	unsigned int ui_Temp;
	unsigned int ui_NoOfChannel = CR_CHAN(insn->chanspec);	
	ui_Temp = data[0];
	*data = inw(devpriv->iobase + APCI2200_DIGITAL_OP);
	if (ui_Temp == 0) {
		*data = (*data >> ui_NoOfChannel) & 0x1;
	}			
	else {
		if (ui_Temp == 1) {
			switch (ui_NoOfChannel) {

			case 2:
				*data = (*data >> (2 * data[1])) & 3;
				break;

			case 4:
				*data = (*data >> (4 * data[1])) & 15;
				break;

			case 8:
				*data = (*data >> (8 * data[1])) & 255;
				break;

			case 15:
				break;

			default:
				comedi_error(dev, " chan spec wrong");
				return -EINVAL;	

			}	
		}		
		else {
			printk("\nSpecified channel not supported \n");
		}		
	}			
	return insn->n;
}


int i_APCI2200_ConfigWatchdog(struct comedi_device *dev, struct comedi_subdevice *s,
	struct comedi_insn *insn, unsigned int *data)
{
	if (data[0] == 0) {
		
		outw(0x0,
			devpriv->iobase + APCI2200_WATCHDOG +
			APCI2200_WATCHDOG_ENABLEDISABLE);
		
		outw(data[1],
			devpriv->iobase + APCI2200_WATCHDOG +
			APCI2200_WATCHDOG_RELOAD_VALUE);
		data[1] = data[1] >> 16;
		outw(data[1],
			devpriv->iobase + APCI2200_WATCHDOG +
			APCI2200_WATCHDOG_RELOAD_VALUE + 2);
	}			
	else {
		printk("\nThe input parameters are wrong\n");
		return -EINVAL;
	}			

	return insn->n;
}


int i_APCI2200_StartStopWriteWatchdog(struct comedi_device *dev, struct comedi_subdevice *s,
	struct comedi_insn *insn, unsigned int *data)
{
	switch (data[0]) {
	case 0:		
		outw(0x0, devpriv->iobase + APCI2200_WATCHDOG + APCI2200_WATCHDOG_ENABLEDISABLE);	
		break;
	case 1:		
		outw(0x0001,
			devpriv->iobase + APCI2200_WATCHDOG +
			APCI2200_WATCHDOG_ENABLEDISABLE);
		break;
	case 2:		
		outw(0x0201,
			devpriv->iobase + APCI2200_WATCHDOG +
			APCI2200_WATCHDOG_ENABLEDISABLE);
		break;
	default:
		printk("\nSpecified functionality does not exist\n");
		return -EINVAL;
	}			
	return insn->n;
}


int i_APCI2200_ReadWatchdog(struct comedi_device *dev, struct comedi_subdevice *s,
	struct comedi_insn *insn, unsigned int *data)
{
	data[0] =
		inw(devpriv->iobase + APCI2200_WATCHDOG +
		APCI2200_WATCHDOG_STATUS) & 0x1;
	return insn->n;
}


int i_APCI2200_Reset(struct comedi_device *dev)
{
	outw(0x0, devpriv->iobase + APCI2200_DIGITAL_OP);	
	outw(0x0,
		devpriv->iobase + APCI2200_WATCHDOG +
		APCI2200_WATCHDOG_ENABLEDISABLE);
	outw(0x0,
		devpriv->iobase + APCI2200_WATCHDOG +
		APCI2200_WATCHDOG_RELOAD_VALUE);
	outw(0x0,
		devpriv->iobase + APCI2200_WATCHDOG +
		APCI2200_WATCHDOG_RELOAD_VALUE + 2);
	return 0;
}
