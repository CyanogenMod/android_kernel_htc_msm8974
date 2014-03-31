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

#include "hwdrv_apci035.h"
static int i_WatchdogNbr = 0;
static int i_Temp = 0;
static int i_Flag = 1;
int i_APCI035_ConfigTimerWatchdog(struct comedi_device *dev, struct comedi_subdevice *s,
	struct comedi_insn *insn, unsigned int *data)
{
	unsigned int ui_Status = 0;
	unsigned int ui_Command = 0;
	unsigned int ui_Mode = 0;
	i_Temp = 0;
	devpriv->tsk_Current = current;
	devpriv->b_TimerSelectMode = data[0];
	i_WatchdogNbr = data[1];
	if (data[0] == 0) {
		ui_Mode = 2;
	} else {
		ui_Mode = 0;
	}
	ui_Command = 0;
	outl(ui_Command, devpriv->iobase + ((i_WatchdogNbr - 1) * 32) + 12);
	ui_Command = 0;
	ui_Command = inl(devpriv->iobase + ((i_WatchdogNbr - 1) * 32) + 12);
	outl(data[3], devpriv->iobase + ((i_WatchdogNbr - 1) * 32) + 4);
	outl(data[2], devpriv->iobase + ((i_WatchdogNbr - 1) * 32) + 8);
	if (data[0] == ADDIDATA_TIMER) {

		 
		
		
		
		
		
		
		
		 

		ui_Command =
			(ui_Command & 0xFFF719E2UL) | ui_Mode << 13UL | 0x10UL;

	}			
	else {
		if (data[0] == ADDIDATA_WATCHDOG) {

		 
			
			
			
			
			
			
		 

			ui_Command = ui_Command & 0xFFF819E2UL;

		} else {
			printk("\n The parameter for Timer/watchdog selection is in error\n");
			return -EINVAL;
		}
	}
	outl(ui_Command, devpriv->iobase + ((i_WatchdogNbr - 1) * 32) + 12);
	ui_Command = 0;
	ui_Command = inl(devpriv->iobase + ((i_WatchdogNbr - 1) * 32) + 12);
	ui_Command = ui_Command & 0xFFFFF89FUL;
	if (data[4] == ADDIDATA_ENABLE) {
    
		
    
		ui_Command = ui_Command | (data[5] << 5);
	}
	outl(ui_Command, devpriv->iobase + ((i_WatchdogNbr - 1) * 32) + 12);
	ui_Command = 0;
	ui_Command = inl(devpriv->iobase + ((i_WatchdogNbr - 1) * 32) + 12);
	ui_Command = ui_Command & 0xFFFFF87FUL;
	if (data[6] == ADDIDATA_ENABLE) {
		ui_Command = ui_Command | (data[7] << 7);
	}
	outl(ui_Command, devpriv->iobase + ((i_WatchdogNbr - 1) * 32) + 12);
	ui_Command = 0;
	ui_Command = inl(devpriv->iobase + ((i_WatchdogNbr - 1) * 32) + 12);
	ui_Command = ui_Command & 0xFFFFF9FBUL;
	ui_Command = ui_Command | (data[8] << 2);
	outl(ui_Command, devpriv->iobase + ((i_WatchdogNbr - 1) * 32) + 12);
	if (data[9] == ADDIDATA_ENABLE) {
   
		
   
		outl(data[11],
			devpriv->iobase + ((i_WatchdogNbr - 1) * 32) + 24);
   
		
   
		outl(data[10],
			devpriv->iobase + ((i_WatchdogNbr - 1) * 32) + 28);
	}

	ui_Command = 0;
	ui_Command = inl(devpriv->iobase + ((i_WatchdogNbr - 1) * 32) + 12);
 
	
 
	ui_Command = ui_Command & 0xFFFFF9F7UL;
   
	
   
	ui_Command = ui_Command | (data[12] << 3);
	outl(ui_Command, devpriv->iobase + ((i_WatchdogNbr - 1) * 32) + 12);
 
 
 
	ui_Command = 0;
	ui_Command = inl(devpriv->iobase + ((i_WatchdogNbr - 1) * 32) + 12);
	ui_Status = inl(devpriv->iobase + ((i_WatchdogNbr - 1) * 32) + 16);

	ui_Command = (ui_Command & 0xFFFFF9FDUL) | (data[13] << 1);
	outl(ui_Command, devpriv->iobase + ((i_WatchdogNbr - 1) * 32) + 12);

	return insn->n;
}

int i_APCI035_StartStopWriteTimerWatchdog(struct comedi_device *dev,
	struct comedi_subdevice *s, struct comedi_insn *insn, unsigned int *data)
{
	unsigned int ui_Command = 0;
	int i_Count = 0;
	if (data[0] == 1) {
		ui_Command =
			inl(devpriv->iobase + ((i_WatchdogNbr - 1) * 32) + 12);
	 
		
	 
		ui_Command = (ui_Command & 0xFFFFF9FFUL) | 0x1UL;
		outl(ui_Command,
			devpriv->iobase + ((i_WatchdogNbr - 1) * 32) + 12);
	}			
	if (data[0] == 2) {
		ui_Command =
			inl(devpriv->iobase + ((i_WatchdogNbr - 1) * 32) + 12);
	 
		
	 
		ui_Command = (ui_Command & 0xFFFFF9FFUL) | 0x200UL;
		outl(ui_Command,
			devpriv->iobase + ((i_WatchdogNbr - 1) * 32) + 12);
	}

	if (data[0] == 0)	
	{
		
		ui_Command = 0;
		outl(ui_Command,
			devpriv->iobase + ((i_WatchdogNbr - 1) * 32) + 12);
	}			
	if (data[0] == 3)	
	{
		ui_Command = 0;
		for (i_Count = 1; i_Count <= 4; i_Count++) {
			if (devpriv->b_TimerSelectMode == ADDIDATA_WATCHDOG) {
				ui_Command = 0x2UL;
			} else {
				ui_Command = 0x10UL;
			}
			i_WatchdogNbr = i_Count;
			outl(ui_Command,
				devpriv->iobase + ((i_WatchdogNbr - 1) * 32) +
				0);
		}

	}
	if (data[0] == 4)	
	{
		ui_Command = 0;
		for (i_Count = 1; i_Count <= 4; i_Count++) {
			if (devpriv->b_TimerSelectMode == ADDIDATA_WATCHDOG) {
				ui_Command = 0x1UL;
			} else {
				ui_Command = 0x8UL;
			}
			i_WatchdogNbr = i_Count;
			outl(ui_Command,
				devpriv->iobase + ((i_WatchdogNbr - 1) * 32) +
				0);
		}
	}
	if (data[0] == 5)	
	{
		ui_Command = 0;
		for (i_Count = 1; i_Count <= 4; i_Count++) {
			if (devpriv->b_TimerSelectMode == ADDIDATA_WATCHDOG) {
				ui_Command = 0x4UL;
			} else {
				ui_Command = 0x20UL;
			}

			i_WatchdogNbr = i_Count;
			outl(ui_Command,
				devpriv->iobase + ((i_WatchdogNbr - 1) * 32) +
				0);
		}
		i_Temp = 1;
	}
	return insn->n;
}

int i_APCI035_ReadTimerWatchdog(struct comedi_device *dev, struct comedi_subdevice *s,
	struct comedi_insn *insn, unsigned int *data)
{
	unsigned int ui_Status = 0;	
	i_WatchdogNbr = insn->unused[0];

	
	
	

	ui_Status = inl(devpriv->iobase + ((i_WatchdogNbr - 1) * 32) + 16);

	
	
	

	data[0] = ((ui_Status >> 1) & 1);
	
	
	
	data[1] = ((ui_Status >> 2) & 1);
	
	
	
	data[2] = ((ui_Status >> 3) & 1);
	
	
	
	data[3] = ((ui_Status >> 0) & 1);
	if (devpriv->b_TimerSelectMode == ADDIDATA_TIMER) {
		data[4] = inl(devpriv->iobase + ((i_WatchdogNbr - 1) * 32) + 0);

	}			

	return insn->n;
}

int i_APCI035_ConfigAnalogInput(struct comedi_device *dev, struct comedi_subdevice *s,
	struct comedi_insn *insn, unsigned int *data)
{
	devpriv->tsk_Current = current;
	outl(0x200 | 0, devpriv->iobase + 128 + 0x4);
	outl(0, devpriv->iobase + 128 + 0);
	outl(0x300 | 0, devpriv->iobase + 128 + 0x4);
	outl((data[0] << 8), devpriv->iobase + 128 + 0);
	outl(0x200000UL, devpriv->iobase + 128 + 12);

	return insn->n;
}

int i_APCI035_ReadAnalogInput(struct comedi_device *dev, struct comedi_subdevice *s,
	struct comedi_insn *insn, unsigned int *data)
{
	unsigned int ui_CommandRegister = 0;
	ui_CommandRegister = 0x80000;
 
	
 
	outl(ui_CommandRegister, devpriv->iobase + 128 + 8);

	data[0] = inl(devpriv->iobase + 128 + 28);
	return insn->n;
}

int i_APCI035_Reset(struct comedi_device *dev)
{
	int i_Count = 0;
	for (i_Count = 1; i_Count <= 4; i_Count++) {
		i_WatchdogNbr = i_Count;
		outl(0x0, devpriv->iobase + ((i_WatchdogNbr - 1) * 32) + 0);	
	}
	outl(0x0, devpriv->iobase + 128 + 12);	

	return 0;
}

static void v_APCI035_Interrupt(int irq, void *d)
{
	struct comedi_device *dev = d;
	unsigned int ui_StatusRegister1 = 0;
	unsigned int ui_StatusRegister2 = 0;
	unsigned int ui_ReadCommand = 0;
	unsigned int ui_ChannelNumber = 0;
	unsigned int ui_DigitalTemperature = 0;
	if (i_Temp == 1) {
		i_WatchdogNbr = i_Flag;
		i_Flag = i_Flag + 1;
	}
  
	
  
	ui_StatusRegister1 = inl(devpriv->iobase + 128 + 16);
  
	
   

	ui_StatusRegister2 =
		inl(devpriv->iobase + ((i_WatchdogNbr - 1) * 32) + 20);

	if ((((ui_StatusRegister1) & 0x8) == 0x8))	
	{
	
		
	
		ui_ReadCommand = inl(devpriv->iobase + 128 + 12);
		ui_ReadCommand = ui_ReadCommand & 0xFFDF0000UL;
		outl(ui_ReadCommand, devpriv->iobase + 128 + 12);
      
		
      
		ui_ChannelNumber = inl(devpriv->iobase + 128 + 60);
	
		
	
		ui_DigitalTemperature = inl(devpriv->iobase + 128 + 60);
		send_sig(SIGIO, devpriv->tsk_Current, 0);	
	}			

	else {
		if ((ui_StatusRegister2 & 0x1) == 0x1) {
			send_sig(SIGIO, devpriv->tsk_Current, 0);	
		}
	}			

	return;
}
