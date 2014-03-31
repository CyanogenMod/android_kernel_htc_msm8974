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

#include "hwdrv_apci3200.h"
#include "addi_amcc_S5920.h"


  int i_InterruptFlag=0;
  int i_ADDIDATAPolarity;
  int i_ADDIDATAGain;
  int i_AutoCalibration=0;   
  int i_ADDIDATAConversionTime;
  int i_ADDIDATAConversionTimeUnit;
  int i_ADDIDATAType;
  int i_ChannelNo;
  int i_ChannelCount=0;
  int i_ScanType;
  int i_FirstChannel;
  int i_LastChannel;
  int i_Sum=0;
  int i_Offset;
  unsigned int ui_Channel_num=0;
  static int i_Count=0;
  int i_Initialised=0;
  unsigned int ui_InterruptChannelValue[96]; 
*/
struct str_BoardInfos s_BoardInfos[100];	



int i_AddiHeaderRW_ReadEeprom(int i_NbOfWordsToRead,
	unsigned int dw_PCIBoardEepromAddress,
	unsigned short w_EepromStartAddress, unsigned short *pw_DataRead)
{
	unsigned int dw_eeprom_busy = 0;
	int i_Counter = 0;
	int i_WordCounter;
	int i;
	unsigned char pb_ReadByte[1];
	unsigned char b_ReadLowByte = 0;
	unsigned char b_ReadHighByte = 0;
	unsigned char b_SelectedAddressLow = 0;
	unsigned char b_SelectedAddressHigh = 0;
	unsigned short w_ReadWord = 0;

	for (i_WordCounter = 0; i_WordCounter < i_NbOfWordsToRead;
		i_WordCounter++) {
		do {
			dw_eeprom_busy =
				inl(dw_PCIBoardEepromAddress +
				AMCC_OP_REG_MCSR);
			dw_eeprom_busy = dw_eeprom_busy & EEPROM_BUSY;
		} while (dw_eeprom_busy == EEPROM_BUSY);

		for (i_Counter = 0; i_Counter < 2; i_Counter++) {
			b_SelectedAddressLow = (w_EepromStartAddress + i_Counter) % 256;	
			b_SelectedAddressHigh = (w_EepromStartAddress + i_Counter) / 256;	

			
			outb(NVCMD_LOAD_LOW,
				dw_PCIBoardEepromAddress + AMCC_OP_REG_MCSR +
				3);

			
			do {
				dw_eeprom_busy =
					inl(dw_PCIBoardEepromAddress +
					AMCC_OP_REG_MCSR);
				dw_eeprom_busy = dw_eeprom_busy & EEPROM_BUSY;
			} while (dw_eeprom_busy == EEPROM_BUSY);

			
			outb(b_SelectedAddressLow,
				dw_PCIBoardEepromAddress + AMCC_OP_REG_MCSR +
				2);

			
			do {
				dw_eeprom_busy =
					inl(dw_PCIBoardEepromAddress +
					AMCC_OP_REG_MCSR);
				dw_eeprom_busy = dw_eeprom_busy & EEPROM_BUSY;
			} while (dw_eeprom_busy == EEPROM_BUSY);

			
			outb(NVCMD_LOAD_HIGH,
				dw_PCIBoardEepromAddress + AMCC_OP_REG_MCSR +
				3);

			
			do {
				dw_eeprom_busy =
					inl(dw_PCIBoardEepromAddress +
					AMCC_OP_REG_MCSR);
				dw_eeprom_busy = dw_eeprom_busy & EEPROM_BUSY;
			} while (dw_eeprom_busy == EEPROM_BUSY);

			
			outb(b_SelectedAddressHigh,
				dw_PCIBoardEepromAddress + AMCC_OP_REG_MCSR +
				2);

			
			do {
				dw_eeprom_busy =
					inl(dw_PCIBoardEepromAddress +
					AMCC_OP_REG_MCSR);
				dw_eeprom_busy = dw_eeprom_busy & EEPROM_BUSY;
			} while (dw_eeprom_busy == EEPROM_BUSY);

			
			outb(NVCMD_BEGIN_READ,
				dw_PCIBoardEepromAddress + AMCC_OP_REG_MCSR +
				3);

			
			do {
				dw_eeprom_busy =
					inl(dw_PCIBoardEepromAddress +
					AMCC_OP_REG_MCSR);
				dw_eeprom_busy = dw_eeprom_busy & EEPROM_BUSY;
			} while (dw_eeprom_busy == EEPROM_BUSY);

			
			*pb_ReadByte =
				inb(dw_PCIBoardEepromAddress +
				AMCC_OP_REG_MCSR + 2);

			
			do {
				dw_eeprom_busy =
					inl(dw_PCIBoardEepromAddress +
					AMCC_OP_REG_MCSR);
				dw_eeprom_busy = dw_eeprom_busy & EEPROM_BUSY;
			} while (dw_eeprom_busy == EEPROM_BUSY);

			
			if (i_Counter == 0)
				b_ReadLowByte = pb_ReadByte[0];
			else
				b_ReadHighByte = pb_ReadByte[0];


			
			msleep(1);

		}
		w_ReadWord =
			(b_ReadLowByte | (((unsigned short)b_ReadHighByte) *
				256));

		pw_DataRead[i_WordCounter] = w_ReadWord;

		w_EepromStartAddress += 2;	

	}			
	return 0;
}


void v_GetAPCI3200EepromCalibrationValue(unsigned int dw_PCIBoardEepromAddress,
	struct str_BoardInfos *BoardInformations)
{
	unsigned short w_AnalogInputMainHeaderAddress;
	unsigned short w_AnalogInputComponentAddress;
	unsigned short w_NumberOfModuls = 0;
	unsigned short w_CurrentSources[2];
	unsigned short w_ModulCounter = 0;
	unsigned short w_FirstHeaderSize = 0;
	unsigned short w_NumberOfInputs = 0;
	unsigned short w_CJCFlag = 0;
	unsigned short w_NumberOfGainValue = 0;
	unsigned short w_SingleHeaderAddress = 0;
	unsigned short w_SingleHeaderSize = 0;
	unsigned short w_Input = 0;
	unsigned short w_GainFactorAddress = 0;
	unsigned short w_GainFactorValue[2];
	unsigned short w_GainIndex = 0;
	unsigned short w_GainValue = 0;

  
  
  
	i_AddiHeaderRW_ReadEeprom(1,	
		dw_PCIBoardEepromAddress, 0x116,	
		&w_AnalogInputMainHeaderAddress);

  
  
  
	w_AnalogInputMainHeaderAddress = w_AnalogInputMainHeaderAddress + 0x100;

  
  
  
	i_AddiHeaderRW_ReadEeprom(1,	
		dw_PCIBoardEepromAddress, w_AnalogInputMainHeaderAddress + 0x02,	
		&w_NumberOfModuls);

	for (w_ModulCounter = 0; w_ModulCounter < w_NumberOfModuls;
		w_ModulCounter++) {
      
      
      
		w_AnalogInputComponentAddress =
			w_AnalogInputMainHeaderAddress +
			(w_FirstHeaderSize * w_ModulCounter) + 0x04;

      
      
      
		i_AddiHeaderRW_ReadEeprom(1,	
			dw_PCIBoardEepromAddress, w_AnalogInputComponentAddress,	
			&w_FirstHeaderSize);

		w_FirstHeaderSize = w_FirstHeaderSize >> 4;

      
      
      
		i_AddiHeaderRW_ReadEeprom(1,	
			dw_PCIBoardEepromAddress, w_AnalogInputComponentAddress + 0x06,	
			&w_NumberOfInputs);

		w_NumberOfInputs = w_NumberOfInputs >> 4;

      
      
      
		i_AddiHeaderRW_ReadEeprom(1,	
			dw_PCIBoardEepromAddress, w_AnalogInputComponentAddress + 0x08,	
			&w_CJCFlag);

		w_CJCFlag = (w_CJCFlag >> 3) & 0x1;	

      
      
      
		i_AddiHeaderRW_ReadEeprom(1,	
			dw_PCIBoardEepromAddress, w_AnalogInputComponentAddress + 0x44,	
			&w_NumberOfGainValue);

		w_NumberOfGainValue = w_NumberOfGainValue & 0xFF;

      
      
      
		w_SingleHeaderAddress =
			w_AnalogInputComponentAddress + 0x46 +
			(((w_NumberOfGainValue / 16) + 1) * 2) +
			(6 * w_NumberOfGainValue) +
			(4 * (((w_NumberOfGainValue / 16) + 1) * 2));

      
      
      
		i_AddiHeaderRW_ReadEeprom(1,	
			dw_PCIBoardEepromAddress, w_SingleHeaderAddress,	
			&w_SingleHeaderSize);

		w_SingleHeaderSize = w_SingleHeaderSize >> 4;

      
      
      
		w_GainFactorAddress = w_AnalogInputComponentAddress;

		for (w_GainIndex = 0; w_GainIndex < w_NumberOfGainValue;
			w_GainIndex++) {
	  
	  
	  
			i_AddiHeaderRW_ReadEeprom(1,	
				dw_PCIBoardEepromAddress, w_AnalogInputComponentAddress + 70 + (2 * (1 + (w_NumberOfGainValue / 16))) + (0x02 * w_GainIndex),	
				&w_GainValue);

			BoardInformations->s_Module[w_ModulCounter].
				w_GainValue[w_GainIndex] = w_GainValue;

#             ifdef PRINT_INFO
			printk("\n Gain value = %d",
				BoardInformations->s_Module[w_ModulCounter].
				w_GainValue[w_GainIndex]);
#             endif

	  
	  
	  
			i_AddiHeaderRW_ReadEeprom(2,	
				dw_PCIBoardEepromAddress, w_AnalogInputComponentAddress + 70 + ((2 * w_NumberOfGainValue) + (2 * (1 + (w_NumberOfGainValue / 16)))) + (0x04 * w_GainIndex),	
				w_GainFactorValue);

			BoardInformations->s_Module[w_ModulCounter].
				ul_GainFactor[w_GainIndex] =
				(w_GainFactorValue[1] << 16) +
				w_GainFactorValue[0];

#             ifdef PRINT_INFO
			printk("\n w_GainFactorValue [%d] = %lu", w_GainIndex,
				BoardInformations->s_Module[w_ModulCounter].
				ul_GainFactor[w_GainIndex]);
#             endif
		}

      
      
      
		for (w_Input = 0; w_Input < w_NumberOfInputs; w_Input++) {
	  
	  
	  
			i_AddiHeaderRW_ReadEeprom(2,	
				dw_PCIBoardEepromAddress,
				(w_Input * w_SingleHeaderSize) +
				w_SingleHeaderAddress + 0x0C, w_CurrentSources);

	  
	  
	  
			BoardInformations->s_Module[w_ModulCounter].
				ul_CurrentSource[w_Input] =
				(w_CurrentSources[0] +
				((w_CurrentSources[1] & 0xFFF) << 16));

#             ifdef PRINT_INFO
			printk("\n Current sources [%d] = %lu", w_Input,
				BoardInformations->s_Module[w_ModulCounter].
				ul_CurrentSource[w_Input]);
#             endif
		}

      
      
      
		i_AddiHeaderRW_ReadEeprom(2,	
			dw_PCIBoardEepromAddress,
			(w_Input * w_SingleHeaderSize) + w_SingleHeaderAddress +
			0x0C, w_CurrentSources);

      
      
      
		BoardInformations->s_Module[w_ModulCounter].
			ul_CurrentSourceCJC =
			(w_CurrentSources[0] +
			((w_CurrentSources[1] & 0xFFF) << 16));

#          ifdef PRINT_INFO
		printk("\n Current sources CJC = %lu",
			BoardInformations->s_Module[w_ModulCounter].
			ul_CurrentSourceCJC);
#          endif
	}
}

int i_APCI3200_GetChannelCalibrationValue(struct comedi_device *dev,
	unsigned int ui_Channel_num, unsigned int *CJCCurrentSource,
	unsigned int *ChannelCurrentSource, unsigned int *ChannelGainFactor)
{
	int i_DiffChannel = 0;
	int i_Module = 0;

#ifdef PRINT_INFO
	printk("\n Channel = %u", ui_Channel_num);
#endif

	
	if (s_BoardInfos[dev->minor].i_ConnectionType == 1) {
		

		if (ui_Channel_num <= 1)
			i_DiffChannel = ui_Channel_num, i_Module = 0;
		else if ((ui_Channel_num >= 2) && (ui_Channel_num <= 3))
			i_DiffChannel = ui_Channel_num - 2, i_Module = 1;
		else if ((ui_Channel_num >= 4) && (ui_Channel_num <= 5))
			i_DiffChannel = ui_Channel_num - 4, i_Module = 2;
		else if ((ui_Channel_num >= 6) && (ui_Channel_num <= 7))
			i_DiffChannel = ui_Channel_num - 6, i_Module = 3;

	} else {
		
		if ((ui_Channel_num == 0) || (ui_Channel_num == 1))
			i_DiffChannel = 0, i_Module = 0;
		else if ((ui_Channel_num == 2) || (ui_Channel_num == 3))
			i_DiffChannel = 1, i_Module = 0;
		else if ((ui_Channel_num == 4) || (ui_Channel_num == 5))
			i_DiffChannel = 0, i_Module = 1;
		else if ((ui_Channel_num == 6) || (ui_Channel_num == 7))
			i_DiffChannel = 1, i_Module = 1;
		else if ((ui_Channel_num == 8) || (ui_Channel_num == 9))
			i_DiffChannel = 0, i_Module = 2;
		else if ((ui_Channel_num == 10) || (ui_Channel_num == 11))
			i_DiffChannel = 1, i_Module = 2;
		else if ((ui_Channel_num == 12) || (ui_Channel_num == 13))
			i_DiffChannel = 0, i_Module = 3;
		else if ((ui_Channel_num == 14) || (ui_Channel_num == 15))
			i_DiffChannel = 1, i_Module = 3;
	}

	
	*CJCCurrentSource =
		s_BoardInfos[dev->minor].s_Module[i_Module].ul_CurrentSourceCJC;
#ifdef PRINT_INFO
	printk("\n CJCCurrentSource = %lu", *CJCCurrentSource);
#endif

	*ChannelCurrentSource =
		s_BoardInfos[dev->minor].s_Module[i_Module].
		ul_CurrentSource[i_DiffChannel];
#ifdef PRINT_INFO
	printk("\n ChannelCurrentSource = %lu", *ChannelCurrentSource);
#endif
	
	

	
	*ChannelGainFactor =
		s_BoardInfos[dev->minor].s_Module[i_Module].
		ul_GainFactor[s_BoardInfos[dev->minor].i_ADDIDATAGain];
#ifdef PRINT_INFO
	printk("\n ChannelGainFactor = %lu", *ChannelGainFactor);
#endif
	

	return 0;
}



int i_APCI3200_ReadDigitalInput(struct comedi_device *dev, struct comedi_subdevice *s,
	struct comedi_insn *insn, unsigned int *data)
{
	unsigned int ui_Temp = 0;
	unsigned int ui_NoOfChannel = 0;
	ui_NoOfChannel = CR_CHAN(insn->chanspec);
	ui_Temp = data[0];
	*data = inl(devpriv->i_IobaseReserved);

	if (ui_Temp == 0) {
		*data = (*data >> ui_NoOfChannel) & 0x1;
	}			
	else {
		if (ui_Temp == 1) {
			if (data[1] < 0 || data[1] > 1) {
				printk("\nThe port number is in error\n");
				return -EINVAL;
			}	
			switch (ui_NoOfChannel) {

			case 2:
				*data = (*data >> (2 * data[1])) & 0x3;
				break;
			case 3:
				*data = (*data & 15);
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

int i_APCI3200_ConfigDigitalOutput(struct comedi_device *dev, struct comedi_subdevice *s,
	struct comedi_insn *insn, unsigned int *data)
{

	if ((data[0] != 0) && (data[0] != 1)) {
		comedi_error(dev,
			"Not a valid Data !!! ,Data should be 1 or 0\n");
		return -EINVAL;
	}			
	if (data[0]) {
		devpriv->b_OutputMemoryStatus = ADDIDATA_ENABLE;
	}			
	else {
		devpriv->b_OutputMemoryStatus = ADDIDATA_DISABLE;
	}			
	return insn->n;
}

int i_APCI3200_WriteDigitalOutput(struct comedi_device *dev, struct comedi_subdevice *s,
	struct comedi_insn *insn, unsigned int *data)
{
	unsigned int ui_Temp = 0, ui_Temp1 = 0;
	unsigned int ui_NoOfChannel = CR_CHAN(insn->chanspec);	
	if (devpriv->b_OutputMemoryStatus) {
		ui_Temp = inl(devpriv->i_IobaseAddon);

	}			
	else {
		ui_Temp = 0;
	}			
	if (data[3] == 0) {
		if (data[1] == 0) {
			data[0] = (data[0] << ui_NoOfChannel) | ui_Temp;
			outl(data[0], devpriv->i_IobaseAddon);
		}		
		else {
			if (data[1] == 1) {
				switch (ui_NoOfChannel) {

				case 2:
					data[0] =
						(data[0] << (2 *
							data[2])) | ui_Temp;
					break;
				case 3:
					data[0] = (data[0] | ui_Temp);
					break;
				}	

				outl(data[0], devpriv->i_IobaseAddon);
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
				data[0] = (data[0] << ui_NoOfChannel) ^ 0xf;
				data[0] = data[0] & ui_Temp;
				outl(data[0], devpriv->i_IobaseAddon);
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
							0xf) & ui_Temp;

						break;
					case 3:
						break;

					default:
						comedi_error(dev,
							" chan spec wrong");
						return -EINVAL;	
					}	

					outl(data[0], devpriv->i_IobaseAddon);
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

int i_APCI3200_ReadDigitalOutput(struct comedi_device *dev, struct comedi_subdevice *s,
	struct comedi_insn *insn, unsigned int *data)
{
	unsigned int ui_Temp;
	unsigned int ui_NoOfChannel;
	ui_NoOfChannel = CR_CHAN(insn->chanspec);
	ui_Temp = data[0];
	*data = inl(devpriv->i_IobaseAddon);
	if (ui_Temp == 0) {
		*data = (*data >> ui_NoOfChannel) & 0x1;
	}			
	else {
		if (ui_Temp == 1) {
			if (data[1] < 0 || data[1] > 1) {
				printk("\nThe port selection is in error\n");
				return -EINVAL;
			}	
			switch (ui_NoOfChannel) {
			case 2:
				*data = (*data >> (2 * data[1])) & 3;
				break;

			case 3:
				break;

			default:
				comedi_error(dev, " chan spec wrong");
				return -EINVAL;	
				break;
			}	
		}		
		else {
			printk("\nSpecified channel not supported \n");
		}		
	}			
	return insn->n;
}

int i_APCI3200_ConfigAnalogInput(struct comedi_device *dev, struct comedi_subdevice *s,
	struct comedi_insn *insn, unsigned int *data)
{

	unsigned int ul_Config = 0, ul_Temp = 0;
	unsigned int ui_ChannelNo = 0;
	unsigned int ui_Dummy = 0;
	int i_err = 0;

	

#ifdef PRINT_INFO
	int i = 0, i2 = 0;
#endif
	

	
	
	if (s_BoardInfos[dev->minor].b_StructInitialized != 1) {
		s_BoardInfos[dev->minor].i_CJCAvailable = 1;
		s_BoardInfos[dev->minor].i_CJCPolarity = 0;
		s_BoardInfos[dev->minor].i_CJCGain = 2;	
		s_BoardInfos[dev->minor].i_InterruptFlag = 0;
		s_BoardInfos[dev->minor].i_AutoCalibration = 0;	
		s_BoardInfos[dev->minor].i_ChannelCount = 0;
		s_BoardInfos[dev->minor].i_Sum = 0;
		s_BoardInfos[dev->minor].ui_Channel_num = 0;
		s_BoardInfos[dev->minor].i_Count = 0;
		s_BoardInfos[dev->minor].i_Initialised = 0;
		s_BoardInfos[dev->minor].b_StructInitialized = 1;

		
		s_BoardInfos[dev->minor].i_ConnectionType = 0;
		

		
		memset(s_BoardInfos[dev->minor].s_Module, 0,
			sizeof(s_BoardInfos[dev->minor].s_Module[MAX_MODULE]));

		v_GetAPCI3200EepromCalibrationValue(devpriv->i_IobaseAmcc,
			&s_BoardInfos[dev->minor]);

#ifdef PRINT_INFO
		for (i = 0; i < MAX_MODULE; i++) {
			printk("\n s_Module[%i].ul_CurrentSourceCJC = %lu", i,
				s_BoardInfos[dev->minor].s_Module[i].
				ul_CurrentSourceCJC);

			for (i2 = 0; i2 < 5; i2++) {
				printk("\n s_Module[%i].ul_CurrentSource [%i] = %lu", i, i2, s_BoardInfos[dev->minor].s_Module[i].ul_CurrentSource[i2]);
			}

			for (i2 = 0; i2 < 8; i2++) {
				printk("\n s_Module[%i].ul_GainFactor [%i] = %lu", i, i2, s_BoardInfos[dev->minor].s_Module[i].ul_GainFactor[i2]);
			}

			for (i2 = 0; i2 < 8; i2++) {
				printk("\n s_Module[%i].w_GainValue [%i] = %u",
					i, i2,
					s_BoardInfos[dev->minor].s_Module[i].
					w_GainValue[i2]);
			}
		}
#endif
		
	}

	if (data[0] != 0 && data[0] != 1 && data[0] != 2) {
		printk("\nThe selection of acquisition type is in error\n");
		i_err++;
	}			
	if (data[0] == 1) {
		if (data[14] != 0 && data[14] != 1 && data[14] != 2
			&& data[14] != 4) {
			printk("\n Error in selection of RTD connection type\n");
			i_err++;
		}		
	}			
	if (data[1] < 0 || data[1] > 7) {
		printk("\nThe selection of gain is in error\n");
		i_err++;
	}			
	if (data[2] != 0 && data[2] != 1) {
		printk("\nThe selection of polarity is in error\n");
		i_err++;
	}			
	if (data[3] != 0) {
		printk("\nThe selection of offset range  is in error\n");
		i_err++;
	}			
	if (data[4] != 0 && data[4] != 1) {
		printk("\nThe selection of coupling is in error\n");
		i_err++;
	}			
	if (data[5] != 0 && data[5] != 1) {
		printk("\nThe selection of single/differential mode is in error\n");
		i_err++;
	}			
	if (data[8] != 0 && data[8] != 1 && data[2] != 2) {
		printk("\nError in selection of functionality\n");
	}			
	if (data[12] == 0 || data[12] == 1) {
		if (data[6] != 20 && data[6] != 40 && data[6] != 80
			&& data[6] != 160) {
			printk("\nThe selection of conversion time reload value is in error\n");
			i_err++;
		}		
		if (data[7] != 2) {
			printk("\nThe selection of conversion time unit  is in error\n");
			i_err++;
		}		
	}
	if (data[9] != 0 && data[9] != 1) {
		printk("\nThe selection of interrupt enable is in error\n");
		i_err++;
	}			
	if (data[11] < 0 || data[11] > 4) {
		printk("\nThe selection of module is in error\n");
		i_err++;
	}			
	if (data[12] < 0 || data[12] > 3) {
		printk("\nThe selection of singlechannel/scan selection is in error\n");
		i_err++;
	}			
	if (data[13] < 0 || data[13] > 16) {
		printk("\nThe selection of number of channels is in error\n");
		i_err++;
	}			

	

	
	s_BoardInfos[dev->minor].i_ChannelCount = data[13];
	s_BoardInfos[dev->minor].i_ScanType = data[12];
	s_BoardInfos[dev->minor].i_ADDIDATAPolarity = data[2];
	s_BoardInfos[dev->minor].i_ADDIDATAGain = data[1];
	s_BoardInfos[dev->minor].i_ADDIDATAConversionTime = data[6];
	s_BoardInfos[dev->minor].i_ADDIDATAConversionTimeUnit = data[7];
	s_BoardInfos[dev->minor].i_ADDIDATAType = data[0];
	
	s_BoardInfos[dev->minor].i_ConnectionType = data[5];
	
	

	
	memset(s_BoardInfos[dev->minor].ui_ScanValueArray, 0, (7 + 12) * sizeof(unsigned int));	
	

	
	
	while (s_BoardInfos[dev->minor].i_InterruptFlag == 1) {
#ifndef MSXBOX
		udelay(1);
#else
		
		
		
		
		printk("");
#endif
	}
	

	ui_ChannelNo = CR_CHAN(insn->chanspec);	
	
	
	

	s_BoardInfos[dev->minor].i_ChannelNo = ui_ChannelNo;
	s_BoardInfos[dev->minor].ui_Channel_num = ui_ChannelNo;

	

	if (data[5] == 0) {
		if (ui_ChannelNo < 0 || ui_ChannelNo > 15) {
			printk("\nThe Selection of the channel is in error\n");
			i_err++;
		}		
	}			
	else {
		if (data[14] == 2) {
			if (ui_ChannelNo < 0 || ui_ChannelNo > 3) {
				printk("\nThe Selection of the channel is in error\n");
				i_err++;
			}	
		}		
		else {
			if (ui_ChannelNo < 0 || ui_ChannelNo > 7) {
				printk("\nThe Selection of the channel is in error\n");
				i_err++;
			}	
		}		
	}			
	if (data[12] == 0 || data[12] == 1) {
		switch (data[5]) {
		case 0:
			if (ui_ChannelNo >= 0 && ui_ChannelNo <= 3) {
				
				
				s_BoardInfos[dev->minor].i_Offset = 0;
				
			}	
			if (ui_ChannelNo >= 4 && ui_ChannelNo <= 7) {
				
				
				s_BoardInfos[dev->minor].i_Offset = 64;
				
			}	
			if (ui_ChannelNo >= 8 && ui_ChannelNo <= 11) {
				
				
				s_BoardInfos[dev->minor].i_Offset = 128;
				
			}	
			if (ui_ChannelNo >= 12 && ui_ChannelNo <= 15) {
				
				
				s_BoardInfos[dev->minor].i_Offset = 192;
				
			}	
			break;
		case 1:
			if (data[14] == 2) {
				if (ui_ChannelNo == 0) {
					
					
					s_BoardInfos[dev->minor].i_Offset = 0;
					
				}	
				if (ui_ChannelNo == 1) {
					
					
					s_BoardInfos[dev->minor].i_Offset = 64;
					
				}	
				if (ui_ChannelNo == 2) {
					
					
					s_BoardInfos[dev->minor].i_Offset = 128;
					
				}	
				if (ui_ChannelNo == 3) {
					
					
					s_BoardInfos[dev->minor].i_Offset = 192;
					
				}	

				
				
				s_BoardInfos[dev->minor].i_ChannelNo = 0;
				
				ui_ChannelNo = 0;
				break;
			}	
			if (ui_ChannelNo >= 0 && ui_ChannelNo <= 1) {
				
				
				s_BoardInfos[dev->minor].i_Offset = 0;
				
			}	
			if (ui_ChannelNo >= 2 && ui_ChannelNo <= 3) {
				
				
				
				s_BoardInfos[dev->minor].i_ChannelNo =
					s_BoardInfos[dev->minor].i_ChannelNo -
					2;
				s_BoardInfos[dev->minor].i_Offset = 64;
				
				ui_ChannelNo = ui_ChannelNo - 2;
			}	
			if (ui_ChannelNo >= 4 && ui_ChannelNo <= 5) {
				
				
				
				s_BoardInfos[dev->minor].i_ChannelNo =
					s_BoardInfos[dev->minor].i_ChannelNo -
					4;
				s_BoardInfos[dev->minor].i_Offset = 128;
				
				ui_ChannelNo = ui_ChannelNo - 4;
			}	
			if (ui_ChannelNo >= 6 && ui_ChannelNo <= 7) {
				
				
				
				s_BoardInfos[dev->minor].i_ChannelNo =
					s_BoardInfos[dev->minor].i_ChannelNo -
					6;
				s_BoardInfos[dev->minor].i_Offset = 192;
				
				ui_ChannelNo = ui_ChannelNo - 6;
			}	
			break;

		default:
			printk("\n This selection of polarity does not exist\n");
			i_err++;
		}		
	}			
	else {
		switch (data[11]) {
		case 1:
			
			
			s_BoardInfos[dev->minor].i_Offset = 0;
			
			break;
		case 2:
			
			
			s_BoardInfos[dev->minor].i_Offset = 64;
			
			break;
		case 3:
			
			
			s_BoardInfos[dev->minor].i_Offset = 128;
			
			break;
		case 4:
			
			
			s_BoardInfos[dev->minor].i_Offset = 192;
			
			break;
		default:
			printk("\nError in module selection\n");
			i_err++;
		}		
	}			
	if (i_err) {
		i_APCI3200_Reset(dev);
		return -EINVAL;
	}
	
	if (s_BoardInfos[dev->minor].i_ScanType != 1) {
		
		
		
		s_BoardInfos[dev->minor].i_Count = 0;
		s_BoardInfos[dev->minor].i_Sum = 0;
		
	}			

	ul_Config =
		data[1] | (data[2] << 6) | (data[5] << 7) | (data[3] << 8) |
		(data[4] << 9);
	
	
	while (((inl(devpriv->iobase + s_BoardInfos[dev->minor].i_Offset +
					12) >> 19) & 1) != 1) ;
	
  
	
  
	
	
	outl(0 | ui_ChannelNo,
		devpriv->iobase + s_BoardInfos[dev->minor].i_Offset + 0x4);
	

	
	
	while (((inl(devpriv->iobase + s_BoardInfos[dev->minor].i_Offset +
					12) >> 19) & 1) != 1) ;
	
  
	
  
	
	
	outl(0, devpriv->iobase + s_BoardInfos[dev->minor].i_Offset + 0x0);
	

	
	
	while (((inl(devpriv->iobase + s_BoardInfos[dev->minor].i_Offset +
					12) >> 19) & 1) != 1) ;
	

  
	
  
	
	
	outl(ul_Config,
		devpriv->iobase + s_BoardInfos[dev->minor].i_Offset + 0x0);
	

  
	
  
	
	
	ul_Temp = inl(devpriv->iobase + s_BoardInfos[dev->minor].i_Offset + 12);
	

	
	
	while (((inl(devpriv->iobase + s_BoardInfos[dev->minor].i_Offset +
					12) >> 19) & 1) != 1) ;
	

	
	
	outl((ul_Temp & 0xFFF9FFFF),
		devpriv->iobase + s_BoardInfos[dev->minor].i_Offset + 12);
	

	if (data[9] == 1) {
		devpriv->tsk_Current = current;
		
		
		s_BoardInfos[dev->minor].i_InterruptFlag = 1;
		
	}			
	else {
		
		
		s_BoardInfos[dev->minor].i_InterruptFlag = 0;
		
	}			

	
	
	s_BoardInfos[dev->minor].i_Initialised = 1;
	

	
	
	if (s_BoardInfos[dev->minor].i_ScanType == 1)
		
	{
		
		
		s_BoardInfos[dev->minor].i_Sum =
			s_BoardInfos[dev->minor].i_Sum + 1;
		

		insn->unused[0] = 0;
		i_APCI3200_ReadAnalogInput(dev, s, insn, &ui_Dummy);
	}

	return insn->n;
}

int i_APCI3200_ReadAnalogInput(struct comedi_device *dev, struct comedi_subdevice *s,
	struct comedi_insn *insn, unsigned int *data)
{
	unsigned int ui_DummyValue = 0;
	int i_ConvertCJCCalibration;
	int i = 0;

	
	
	if (s_BoardInfos[dev->minor].i_Initialised == 0)
		
	{
		i_APCI3200_Reset(dev);
		return -EINVAL;
	}			

#ifdef PRINT_INFO
	printk("\n insn->unused[0] = %i", insn->unused[0]);
#endif

	switch (insn->unused[0]) {
	case 0:

		i_APCI3200_Read1AnalogInputChannel(dev, s, insn,
			&ui_DummyValue);
		
		
		s_BoardInfos[dev->minor].
			ui_InterruptChannelValue[s_BoardInfos[dev->minor].
			i_Count + 0] = ui_DummyValue;
		

		
		i_APCI3200_GetChannelCalibrationValue(dev,
			s_BoardInfos[dev->minor].ui_Channel_num,
			&s_BoardInfos[dev->minor].
			ui_InterruptChannelValue[s_BoardInfos[dev->minor].
				i_Count + 6],
			&s_BoardInfos[dev->minor].
			ui_InterruptChannelValue[s_BoardInfos[dev->minor].
				i_Count + 7],
			&s_BoardInfos[dev->minor].
			ui_InterruptChannelValue[s_BoardInfos[dev->minor].
				i_Count + 8]);

#ifdef PRINT_INFO
		printk("\n s_BoardInfos [dev->minor].ui_InterruptChannelValue[s_BoardInfos [dev->minor].i_Count+6] = %lu", s_BoardInfos[dev->minor].ui_InterruptChannelValue[s_BoardInfos[dev->minor].i_Count + 6]);

		printk("\n s_BoardInfos [dev->minor].ui_InterruptChannelValue[s_BoardInfos [dev->minor].i_Count+7] = %lu", s_BoardInfos[dev->minor].ui_InterruptChannelValue[s_BoardInfos[dev->minor].i_Count + 7]);

		printk("\n s_BoardInfos [dev->minor].ui_InterruptChannelValue[s_BoardInfos [dev->minor].i_Count+8] = %lu", s_BoardInfos[dev->minor].ui_InterruptChannelValue[s_BoardInfos[dev->minor].i_Count + 8]);
#endif

		

		
		
		if ((s_BoardInfos[dev->minor].i_ADDIDATAType == 2)
			&& (s_BoardInfos[dev->minor].i_InterruptFlag == FALSE)
			&& (s_BoardInfos[dev->minor].i_CJCAvailable == 1))
			
		{
			i_APCI3200_ReadCJCValue(dev, &ui_DummyValue);
			
			
			s_BoardInfos[dev->minor].
				ui_InterruptChannelValue[s_BoardInfos[dev->
					minor].i_Count + 3] = ui_DummyValue;
			
		}		
		else {
			
			
			s_BoardInfos[dev->minor].
				ui_InterruptChannelValue[s_BoardInfos[dev->
					minor].i_Count + 3] = 0;
			
		}		

		
		
		if ((s_BoardInfos[dev->minor].i_AutoCalibration == FALSE)
			&& (s_BoardInfos[dev->minor].i_InterruptFlag == FALSE))
			
		{
			i_APCI3200_ReadCalibrationOffsetValue(dev,
				&ui_DummyValue);
			
			
			s_BoardInfos[dev->minor].
				ui_InterruptChannelValue[s_BoardInfos[dev->
					minor].i_Count + 1] = ui_DummyValue;
			
			i_APCI3200_ReadCalibrationGainValue(dev,
				&ui_DummyValue);
			
			
			s_BoardInfos[dev->minor].
				ui_InterruptChannelValue[s_BoardInfos[dev->
					minor].i_Count + 2] = ui_DummyValue;
			
		}		

		
		
		if ((s_BoardInfos[dev->minor].i_ADDIDATAType == 2)
			&& (s_BoardInfos[dev->minor].i_InterruptFlag == FALSE)
			&& (s_BoardInfos[dev->minor].i_CJCAvailable == 1))
			
		{
	  
			
	  
	  
			
	  
			
			
			if (s_BoardInfos[dev->minor].i_CJCPolarity !=
				s_BoardInfos[dev->minor].i_ADDIDATAPolarity)
				
			{
				i_ConvertCJCCalibration = 1;
			}	
			else {
				
				
				if (s_BoardInfos[dev->minor].i_CJCGain ==
					s_BoardInfos[dev->minor].i_ADDIDATAGain)
					
				{
					i_ConvertCJCCalibration = 0;
				}	
				else {
					i_ConvertCJCCalibration = 1;
				}	
			}	
			if (i_ConvertCJCCalibration == 1) {
				i_APCI3200_ReadCJCCalOffset(dev,
					&ui_DummyValue);
				
				
				s_BoardInfos[dev->minor].
					ui_InterruptChannelValue[s_BoardInfos
					[dev->minor].i_Count + 4] =
					ui_DummyValue;
				

				i_APCI3200_ReadCJCCalGain(dev, &ui_DummyValue);

				
				
				s_BoardInfos[dev->minor].
					ui_InterruptChannelValue[s_BoardInfos
					[dev->minor].i_Count + 5] =
					ui_DummyValue;
				
			}	
			else {
				
				
				

				s_BoardInfos[dev->minor].
					ui_InterruptChannelValue[s_BoardInfos
					[dev->minor].i_Count + 4] = 0;
				s_BoardInfos[dev->minor].
					ui_InterruptChannelValue[s_BoardInfos
					[dev->minor].i_Count + 5] = 0;
				
			}	
		}		

		
		
		if (s_BoardInfos[dev->minor].i_ScanType != 1) {
			
			s_BoardInfos[dev->minor].i_Count = 0;
		}		
		else {
			
			
			
			s_BoardInfos[dev->minor].i_Count =
				s_BoardInfos[dev->minor].i_Count + 9;
			
		}		

		
		if ((s_BoardInfos[dev->minor].i_ScanType == 1)
			&& (s_BoardInfos[dev->minor].i_InterruptFlag == 1)) {
			
			
			
			s_BoardInfos[dev->minor].i_Count =
				s_BoardInfos[dev->minor].i_Count - 9;
			
		}
		
		if (s_BoardInfos[dev->minor].i_ScanType == 0) {
#ifdef PRINT_INFO
			printk("\n data[0]= s_BoardInfos [dev->minor].ui_InterruptChannelValue[0];");
#endif
			data[0] =
				s_BoardInfos[dev->minor].
				ui_InterruptChannelValue[0];
			data[1] =
				s_BoardInfos[dev->minor].
				ui_InterruptChannelValue[1];
			data[2] =
				s_BoardInfos[dev->minor].
				ui_InterruptChannelValue[2];
			data[3] =
				s_BoardInfos[dev->minor].
				ui_InterruptChannelValue[3];
			data[4] =
				s_BoardInfos[dev->minor].
				ui_InterruptChannelValue[4];
			data[5] =
				s_BoardInfos[dev->minor].
				ui_InterruptChannelValue[5];

			
			
			i_APCI3200_GetChannelCalibrationValue(dev,
				s_BoardInfos[dev->minor].ui_Channel_num,
				&data[6], &data[7], &data[8]);
			
		}
		break;
	case 1:

		for (i = 0; i < insn->n; i++) {
			
			data[i] =
				s_BoardInfos[dev->minor].
				ui_InterruptChannelValue[i];
		}

		
		
		
		s_BoardInfos[dev->minor].i_Count = 0;
		s_BoardInfos[dev->minor].i_Sum = 0;
		if (s_BoardInfos[dev->minor].i_ScanType == 1) {
			
			
			s_BoardInfos[dev->minor].i_Initialised = 0;
			s_BoardInfos[dev->minor].i_InterruptFlag = 0;
			
		}
		break;
	default:
		printk("\nThe parameters passed are in error\n");
		i_APCI3200_Reset(dev);
		return -EINVAL;
	}			

	return insn->n;
}

int i_APCI3200_Read1AnalogInputChannel(struct comedi_device *dev,
	struct comedi_subdevice *s, struct comedi_insn *insn, unsigned int *data)
{
	unsigned int ui_EOC = 0;
	unsigned int ui_ChannelNo = 0;
	unsigned int ui_CommandRegister = 0;

	
	
	ui_ChannelNo = s_BoardInfos[dev->minor].i_ChannelNo;

	
	while (((inl(devpriv->iobase + s_BoardInfos[dev->minor].i_Offset +
					12) >> 19) & 1) != 1) ;
  
	
  
	
	
	
	outl(0 | s_BoardInfos[dev->minor].i_ChannelNo,
		devpriv->iobase + s_BoardInfos[dev->minor].i_Offset + 0x4);
	

  
	
  
	
	while (((inl(devpriv->iobase + s_BoardInfos[dev->minor].i_Offset +
					12) >> 19) & 1) != 1) ;

	
	outl(s_BoardInfos[dev->minor].i_ADDIDATAConversionTimeUnit,
		devpriv->iobase + s_BoardInfos[dev->minor].i_Offset + 36);

  
	
  
	
	while (((inl(devpriv->iobase + s_BoardInfos[dev->minor].i_Offset +
					12) >> 19) & 1) != 1) ;

	
	outl(s_BoardInfos[dev->minor].i_ADDIDATAConversionTime,
		devpriv->iobase + s_BoardInfos[dev->minor].i_Offset + 32);

  
	
  

	ui_CommandRegister = ui_ChannelNo | (ui_ChannelNo << 8) | 0x80000;

  
	
  

	
	if (s_BoardInfos[dev->minor].i_InterruptFlag == ADDIDATA_ENABLE) {
      
		
      
		ui_CommandRegister = ui_CommandRegister | 0x00100000;
	}			

  
	
  
	
	while (((inl(devpriv->iobase + s_BoardInfos[dev->minor].i_Offset +
					12) >> 19) & 1) != 1) ;

	
	outl(ui_CommandRegister,
		devpriv->iobase + s_BoardInfos[dev->minor].i_Offset + 8);

  
	
  
	
	if (s_BoardInfos[dev->minor].i_InterruptFlag == ADDIDATA_DISABLE) {
		do {
	  
			
	  

			
			ui_EOC = inl(devpriv->iobase +
				s_BoardInfos[dev->minor].i_Offset + 20) & 1;

		} while (ui_EOC != 1);

      
		
      

		
		data[0] =
			inl(devpriv->iobase +
			s_BoardInfos[dev->minor].i_Offset + 28);
		

	}			
	return 0;
}

int i_APCI3200_ReadCalibrationOffsetValue(struct comedi_device *dev, unsigned int *data)
{
	unsigned int ui_Temp = 0, ui_EOC = 0;
	unsigned int ui_CommandRegister = 0;

	
	
	while (((inl(devpriv->iobase + s_BoardInfos[dev->minor].i_Offset +
					12) >> 19) & 1) != 1) ;
  
	
  
	
	
	
	

  
	
  
	
	while (((inl(devpriv->iobase + s_BoardInfos[dev->minor].i_Offset +
					12) >> 19) & 1) != 1) ;
	
	outl(s_BoardInfos[dev->minor].i_ADDIDATAConversionTimeUnit,
		devpriv->iobase + s_BoardInfos[dev->minor].i_Offset + 36);
  
	
  
	
	while (((inl(devpriv->iobase + s_BoardInfos[dev->minor].i_Offset +
					12) >> 19) & 1) != 1) ;
	
	outl(s_BoardInfos[dev->minor].i_ADDIDATAConversionTime,
		devpriv->iobase + s_BoardInfos[dev->minor].i_Offset + 32);
  
	
  
	
	ui_Temp = inl(devpriv->iobase + s_BoardInfos[dev->minor].i_Offset + 12);

  
	
  
	
	while (((inl(devpriv->iobase + s_BoardInfos[dev->minor].i_Offset +
					12) >> 19) & 1) != 1) ;
	
	outl((ui_Temp | 0x00020000),
		devpriv->iobase + s_BoardInfos[dev->minor].i_Offset + 12);
  
	
  

	ui_CommandRegister = 0;

  
	
  

	
	if (s_BoardInfos[dev->minor].i_InterruptFlag == ADDIDATA_ENABLE) {

      
		
      

		ui_CommandRegister = ui_CommandRegister | 0x00100000;

	}			

  
	
  
	ui_CommandRegister = ui_CommandRegister | 0x00080000;

  
	
  
	
	while (((inl(devpriv->iobase + s_BoardInfos[dev->minor].i_Offset +
					12) >> 19) & 1) != 1) ;
	
	outl(ui_CommandRegister,
		devpriv->iobase + s_BoardInfos[dev->minor].i_Offset + 8);

  
	
  

	
	if (s_BoardInfos[dev->minor].i_InterruptFlag == ADDIDATA_DISABLE) {

		do {
	  
			
	  

			
			ui_EOC = inl(devpriv->iobase +
				s_BoardInfos[dev->minor].i_Offset + 20) & 1;

		} while (ui_EOC != 1);

      
		
      

		
		data[0] =
			inl(devpriv->iobase +
			s_BoardInfos[dev->minor].i_Offset + 28);
	}			
	return 0;
}

int i_APCI3200_ReadCalibrationGainValue(struct comedi_device *dev, unsigned int *data)
{
	unsigned int ui_EOC = 0;
	int ui_CommandRegister = 0;

	
	while (((inl(devpriv->iobase + s_BoardInfos[dev->minor].i_Offset +
					12) >> 19) & 1) != 1) ;
  
	
  
	
	
	
	

  
	
  
  
	
  
	
	while (((inl(devpriv->iobase + s_BoardInfos[dev->minor].i_Offset +
					12) >> 19) & 1) != 1) ;
	
	outl(s_BoardInfos[dev->minor].i_ADDIDATAConversionTimeUnit,
		devpriv->iobase + s_BoardInfos[dev->minor].i_Offset + 36);
  
	
  
	
	while (((inl(devpriv->iobase + s_BoardInfos[dev->minor].i_Offset +
					12) >> 19) & 1) != 1) ;
	
	outl(s_BoardInfos[dev->minor].i_ADDIDATAConversionTime,
		devpriv->iobase + s_BoardInfos[dev->minor].i_Offset + 32);
  
	
  
	
	while (((inl(devpriv->iobase + s_BoardInfos[dev->minor].i_Offset +
					12) >> 19) & 1) != 1) ;
	
	outl(0x00040000,
		devpriv->iobase + s_BoardInfos[dev->minor].i_Offset + 12);

  
	
  

	ui_CommandRegister = 0;

  
	
  

	
	if (s_BoardInfos[dev->minor].i_InterruptFlag == ADDIDATA_ENABLE) {

      
		
      

		ui_CommandRegister = ui_CommandRegister | 0x00100000;

	}			

  
	
  

	ui_CommandRegister = ui_CommandRegister | 0x00080000;
  
	
  
	
	while (((inl(devpriv->iobase + s_BoardInfos[dev->minor].i_Offset +
					12) >> 19) & 1) != 1) ;
	
	outl(ui_CommandRegister,
		devpriv->iobase + s_BoardInfos[dev->minor].i_Offset + 8);

  
	
  

	
	if (s_BoardInfos[dev->minor].i_InterruptFlag == ADDIDATA_DISABLE) {

		do {

	  
			
	  

			
			ui_EOC = inl(devpriv->iobase +
				s_BoardInfos[dev->minor].i_Offset + 20) & 1;

		} while (ui_EOC != 1);

      
		
      

		
		data[0] =
			inl(devpriv->iobase +
			s_BoardInfos[dev->minor].i_Offset + 28);

	}			
	return 0;
}


int i_APCI3200_ReadCJCValue(struct comedi_device *dev, unsigned int *data)
{
	unsigned int ui_EOC = 0;
	int ui_CommandRegister = 0;

  
	
  

	
	while (((inl(devpriv->iobase + s_BoardInfos[dev->minor].i_Offset +
					12) >> 19) & 1) != 1) ;

	
	outl(s_BoardInfos[dev->minor].i_ADDIDATAConversionTimeUnit,
		devpriv->iobase + s_BoardInfos[dev->minor].i_Offset + 36);
  
	
  
	
	while (((inl(devpriv->iobase + s_BoardInfos[dev->minor].i_Offset +
					12) >> 19) & 1) != 1) ;

	
	outl(s_BoardInfos[dev->minor].i_ADDIDATAConversionTime,
		devpriv->iobase + s_BoardInfos[dev->minor].i_Offset + 32);

  
	
  
	
	while (((inl(devpriv->iobase + s_BoardInfos[dev->minor].i_Offset +
					12) >> 19) & 1) != 1) ;

	
	outl(0x00000400,
		devpriv->iobase + s_BoardInfos[dev->minor].i_Offset + 4);
  
	
  
	ui_CommandRegister = 0;
  
	
  
	
	if (s_BoardInfos[dev->minor].i_InterruptFlag == ADDIDATA_ENABLE) {
      
		
      
		ui_CommandRegister = ui_CommandRegister | 0x00100000;
	}

  
	
  

	ui_CommandRegister = ui_CommandRegister | 0x00080000;

  
	
  
	
	while (((inl(devpriv->iobase + s_BoardInfos[dev->minor].i_Offset +
					12) >> 19) & 1) != 1) ;
	
	outl(ui_CommandRegister,
		devpriv->iobase + s_BoardInfos[dev->minor].i_Offset + 8);

  
	
  

	
	if (s_BoardInfos[dev->minor].i_InterruptFlag == ADDIDATA_DISABLE) {
		do {

	  
			
	  

			
			ui_EOC = inl(devpriv->iobase +
				s_BoardInfos[dev->minor].i_Offset + 20) & 1;

		} while (ui_EOC != 1);

      
		
      

		
		data[0] =
			inl(devpriv->iobase +
			s_BoardInfos[dev->minor].i_Offset + 28);

	}			
	return 0;
}

int i_APCI3200_ReadCJCCalOffset(struct comedi_device *dev, unsigned int *data)
{
	unsigned int ui_EOC = 0;
	int ui_CommandRegister = 0;
  
	
  
  
	
  
	
	while (((inl(devpriv->iobase + s_BoardInfos[dev->minor].i_Offset +
					12) >> 19) & 1) != 1) ;
	
	outl(s_BoardInfos[dev->minor].i_ADDIDATAConversionTimeUnit,
		devpriv->iobase + s_BoardInfos[dev->minor].i_Offset + 36);
  
	
  
	
	while (((inl(devpriv->iobase + s_BoardInfos[dev->minor].i_Offset +
					12) >> 19) & 1) != 1) ;
	
	outl(s_BoardInfos[dev->minor].i_ADDIDATAConversionTime,
		devpriv->iobase + s_BoardInfos[dev->minor].i_Offset + 32);
  
	
  
	
	while (((inl(devpriv->iobase + s_BoardInfos[dev->minor].i_Offset +
					12) >> 19) & 1) != 1) ;
	
	outl(0x00000400,
		devpriv->iobase + s_BoardInfos[dev->minor].i_Offset + 4);
  
	
  
	
	while (((inl(devpriv->iobase + s_BoardInfos[dev->minor].i_Offset +
					12) >> 19) & 1) != 1) ;
	
	outl(0x00020000,
		devpriv->iobase + s_BoardInfos[dev->minor].i_Offset + 12);
  
	
  
	ui_CommandRegister = 0;
  
	
  

	
	if (s_BoardInfos[dev->minor].i_InterruptFlag == ADDIDATA_ENABLE) {
      
		
      
		ui_CommandRegister = ui_CommandRegister | 0x00100000;

	}

  
	
  
	ui_CommandRegister = ui_CommandRegister | 0x00080000;
  
	
  
	
	while (((inl(devpriv->iobase + s_BoardInfos[dev->minor].i_Offset +
					12) >> 19) & 1) != 1) ;
	
	outl(ui_CommandRegister,
		devpriv->iobase + s_BoardInfos[dev->minor].i_Offset + 8);
	
	if (s_BoardInfos[dev->minor].i_InterruptFlag == ADDIDATA_DISABLE) {
		do {
	  
			
	  
			
			ui_EOC = inl(devpriv->iobase +
				s_BoardInfos[dev->minor].i_Offset + 20) & 1;
		} while (ui_EOC != 1);

      
		
      
		
		data[0] =
			inl(devpriv->iobase +
			s_BoardInfos[dev->minor].i_Offset + 28);
	}			
	return 0;
}

int i_APCI3200_ReadCJCCalGain(struct comedi_device *dev, unsigned int *data)
{
	unsigned int ui_EOC = 0;
	int ui_CommandRegister = 0;
  
	
  
	
	while (((inl(devpriv->iobase + s_BoardInfos[dev->minor].i_Offset +
					12) >> 19) & 1) != 1) ;
	
	outl(s_BoardInfos[dev->minor].i_ADDIDATAConversionTimeUnit,
		devpriv->iobase + s_BoardInfos[dev->minor].i_Offset + 36);
  
	
  
	
	while (((inl(devpriv->iobase + s_BoardInfos[dev->minor].i_Offset +
					12) >> 19) & 1) != 1) ;
	
	outl(s_BoardInfos[dev->minor].i_ADDIDATAConversionTime,
		devpriv->iobase + s_BoardInfos[dev->minor].i_Offset + 32);
  
	
  
	
	while (((inl(devpriv->iobase + s_BoardInfos[dev->minor].i_Offset +
					12) >> 19) & 1) != 1) ;
	
	outl(0x00000400,
		devpriv->iobase + s_BoardInfos[dev->minor].i_Offset + 4);
  
	
  
	
	while (((inl(devpriv->iobase + s_BoardInfos[dev->minor].i_Offset +
					12) >> 19) & 1) != 1) ;
	
	outl(0x00040000,
		devpriv->iobase + s_BoardInfos[dev->minor].i_Offset + 12);

  
	
  
	ui_CommandRegister = 0;
  
	
  
	
	if (s_BoardInfos[dev->minor].i_InterruptFlag == ADDIDATA_ENABLE) {
      
		
      
		ui_CommandRegister = ui_CommandRegister | 0x00100000;
	}
  
	
  
	ui_CommandRegister = ui_CommandRegister | 0x00080000;
  
	
  
	
	while (((inl(devpriv->iobase + s_BoardInfos[dev->minor].i_Offset +
					12) >> 19) & 1) != 1) ;
	
	outl(ui_CommandRegister,
		devpriv->iobase + s_BoardInfos[dev->minor].i_Offset + 8);
	
	if (s_BoardInfos[dev->minor].i_InterruptFlag == ADDIDATA_DISABLE) {
		do {
	  
			
	  
			
			ui_EOC = inl(devpriv->iobase +
				s_BoardInfos[dev->minor].i_Offset + 20) & 1;
		} while (ui_EOC != 1);
      
		
      
		
		data[0] =
			inl(devpriv->iobase +
			s_BoardInfos[dev->minor].i_Offset + 28);
	}			
	return 0;
}


int i_APCI3200_InsnBits_AnalogInput_Test(struct comedi_device *dev,
	struct comedi_subdevice *s, struct comedi_insn *insn, unsigned int *data)
{
	unsigned int ui_Configuration = 0;
	int i_Temp;		
	

	if (s_BoardInfos[dev->minor].i_Initialised == 0) {
		i_APCI3200_Reset(dev);
		return -EINVAL;
	}			
	if (data[0] != 0 && data[0] != 1) {
		printk("\nError in selection of functionality\n");
		i_APCI3200_Reset(dev);
		return -EINVAL;
	}			

	if (data[0] == 1)	
	{
      
		
      
		
		while (((inl(devpriv->iobase + s_BoardInfos[dev->minor].
						i_Offset + 12) >> 19) & 1) !=
			1) ;
		
		outl((0x00001000 | s_BoardInfos[dev->minor].i_ChannelNo),
			devpriv->iobase + s_BoardInfos[dev->minor].i_Offset +
			4);
      
		
      
		
		i_Temp = s_BoardInfos[dev->minor].i_InterruptFlag;
		
		s_BoardInfos[dev->minor].i_InterruptFlag = ADDIDATA_DISABLE;
		i_APCI3200_Read1AnalogInputChannel(dev, s, insn, data);
		
		if (s_BoardInfos[dev->minor].i_AutoCalibration == FALSE) {
			
			while (((inl(devpriv->iobase + s_BoardInfos[dev->minor].
							i_Offset +
							12) >> 19) & 1) != 1) ;

			
			outl((0x00001000 | s_BoardInfos[dev->minor].
					i_ChannelNo),
				devpriv->iobase +
				s_BoardInfos[dev->minor].i_Offset + 4);
			data++;
			i_APCI3200_ReadCalibrationOffsetValue(dev, data);
			data++;
			i_APCI3200_ReadCalibrationGainValue(dev, data);
		}
	} else {
		
		while (((inl(devpriv->iobase + s_BoardInfos[dev->minor].
						i_Offset + 12) >> 19) & 1) !=
			1) ;
		
		outl((0x00000800 | s_BoardInfos[dev->minor].i_ChannelNo),
			devpriv->iobase + s_BoardInfos[dev->minor].i_Offset +
			4);
		
		ui_Configuration =
			inl(devpriv->iobase +
			s_BoardInfos[dev->minor].i_Offset + 0);
      
		
      
		
		i_Temp = s_BoardInfos[dev->minor].i_InterruptFlag;
		
		s_BoardInfos[dev->minor].i_InterruptFlag = ADDIDATA_DISABLE;
		i_APCI3200_Read1AnalogInputChannel(dev, s, insn, data);
		
		if (s_BoardInfos[dev->minor].i_AutoCalibration == FALSE) {
			
			while (((inl(devpriv->iobase + s_BoardInfos[dev->minor].
							i_Offset +
							12) >> 19) & 1) != 1) ;
			
			outl((0x00000800 | s_BoardInfos[dev->minor].
					i_ChannelNo),
				devpriv->iobase +
				s_BoardInfos[dev->minor].i_Offset + 4);
			data++;
			i_APCI3200_ReadCalibrationOffsetValue(dev, data);
			data++;
			i_APCI3200_ReadCalibrationGainValue(dev, data);
		}
	}
	
	s_BoardInfos[dev->minor].i_InterruptFlag = i_Temp;
	
	return insn->n;
}


int i_APCI3200_InsnWriteReleaseAnalogInput(struct comedi_device *dev,
	struct comedi_subdevice *s, struct comedi_insn *insn, unsigned int *data)
{
	i_APCI3200_Reset(dev);
	return insn->n;
}


int i_APCI3200_CommandTestAnalogInput(struct comedi_device *dev, struct comedi_subdevice *s,
	struct comedi_cmd *cmd)
{

	int err = 0;
	int tmp;		
	unsigned int ui_ConvertTime = 0;
	unsigned int ui_ConvertTimeBase = 0;
	unsigned int ui_DelayTime = 0;
	unsigned int ui_DelayTimeBase = 0;
	int i_Triggermode = 0;
	int i_TriggerEdge = 0;
	int i_NbrOfChannel = 0;
	int i_Cpt = 0;
	double d_ConversionTimeForAllChannels = 0.0;
	double d_SCANTimeNewUnit = 0.0;
	

	tmp = cmd->start_src;
	cmd->start_src &= TRIG_NOW | TRIG_EXT;
	if (!cmd->start_src || tmp != cmd->start_src)
		err++;
	tmp = cmd->scan_begin_src;
	cmd->scan_begin_src &= TRIG_TIMER | TRIG_FOLLOW;
	if (!cmd->scan_begin_src || tmp != cmd->scan_begin_src)
		err++;
	tmp = cmd->convert_src;
	cmd->convert_src &= TRIG_TIMER;
	if (!cmd->convert_src || tmp != cmd->convert_src)
		err++;
	tmp = cmd->scan_end_src;
	cmd->scan_end_src &= TRIG_COUNT;
	if (!cmd->scan_end_src || tmp != cmd->scan_end_src)
		err++;
	tmp = cmd->stop_src;
	cmd->stop_src &= TRIG_COUNT | TRIG_NONE;
	if (!cmd->stop_src || tmp != cmd->stop_src)
		err++;
	
	if (s_BoardInfos[dev->minor].i_InterruptFlag == 0) {
		err++;
		
	}
	if (err) {
		i_APCI3200_Reset(dev);
		return 1;
	}

	if (cmd->start_src != TRIG_NOW && cmd->start_src != TRIG_EXT) {
		err++;
	}
	if (cmd->start_src == TRIG_EXT) {
		i_TriggerEdge = cmd->start_arg & 0xFFFF;
		i_Triggermode = cmd->start_arg >> 16;
		if (i_TriggerEdge < 1 || i_TriggerEdge > 3) {
			err++;
			printk("\nThe trigger edge selection is in error\n");
		}
		if (i_Triggermode != 2) {
			err++;
			printk("\nThe trigger mode selection is in error\n");
		}
	}

	if (cmd->scan_begin_src != TRIG_TIMER &&
		cmd->scan_begin_src != TRIG_FOLLOW)
		err++;

	if (cmd->convert_src != TRIG_TIMER)
		err++;

	if (cmd->scan_end_src != TRIG_COUNT) {
		cmd->scan_end_src = TRIG_COUNT;
		err++;
	}

	if (cmd->stop_src != TRIG_NONE && cmd->stop_src != TRIG_COUNT)
		err++;

	if (err) {
		i_APCI3200_Reset(dev);
		return 2;
	}
	
	s_BoardInfos[dev->minor].i_FirstChannel = cmd->chanlist[0];
	
	s_BoardInfos[dev->minor].i_LastChannel = cmd->chanlist[1];

	if (cmd->convert_src == TRIG_TIMER) {
		ui_ConvertTime = cmd->convert_arg & 0xFFFF;
		ui_ConvertTimeBase = cmd->convert_arg >> 16;
		if (ui_ConvertTime != 20 && ui_ConvertTime != 40
			&& ui_ConvertTime != 80 && ui_ConvertTime != 160)
		{
			printk("\nThe selection of conversion time reload value is in error\n");
			err++;
		}		
		if (ui_ConvertTimeBase != 2) {
			printk("\nThe selection of conversion time unit  is in error\n");
			err++;
		}		
	} else {
		ui_ConvertTime = 0;
		ui_ConvertTimeBase = 0;
	}
	if (cmd->scan_begin_src == TRIG_FOLLOW) {
		ui_DelayTime = 0;
		ui_DelayTimeBase = 0;
	}			
	else {
		ui_DelayTime = cmd->scan_begin_arg & 0xFFFF;
		ui_DelayTimeBase = cmd->scan_begin_arg >> 16;
		if (ui_DelayTimeBase != 2 && ui_DelayTimeBase != 3) {
			err++;
			printk("\nThe Delay time base selection is in error\n");
		}
		if (ui_DelayTime < 1 || ui_DelayTime > 1023) {
			err++;
			printk("\nThe Delay time value is in error\n");
		}
		if (err) {
			i_APCI3200_Reset(dev);
			return 3;
		}
		fpu_begin();
		d_SCANTimeNewUnit = (double)ui_DelayTime;
		
		i_NbrOfChannel =
			s_BoardInfos[dev->minor].i_LastChannel -
			s_BoardInfos[dev->minor].i_FirstChannel + 4;
      
		
      
		d_ConversionTimeForAllChannels =
			(double)((double)ui_ConvertTime /
			(double)i_NbrOfChannel);

      
		
      
		d_ConversionTimeForAllChannels =
			(double)1.0 / d_ConversionTimeForAllChannels;
		ui_ConvertTimeBase = 3;
      
		
      

		if (ui_DelayTimeBase <= ui_ConvertTimeBase) {

			for (i_Cpt = 0;
				i_Cpt < (ui_ConvertTimeBase - ui_DelayTimeBase);
				i_Cpt++) {

				d_ConversionTimeForAllChannels =
					d_ConversionTimeForAllChannels * 1000;
				d_ConversionTimeForAllChannels =
					d_ConversionTimeForAllChannels + 1;
			}
		} else {
			for (i_Cpt = 0;
				i_Cpt < (ui_DelayTimeBase - ui_ConvertTimeBase);
				i_Cpt++) {
				d_SCANTimeNewUnit = d_SCANTimeNewUnit * 1000;

			}
		}

		if (d_ConversionTimeForAllChannels >= d_SCANTimeNewUnit) {

			printk("\nSCAN Delay value cannot be used\n");
	  
			
	  
			err++;
		}
		fpu_end();
	}			

	if (err) {
		i_APCI3200_Reset(dev);
		return 4;
	}

	return 0;
}


int i_APCI3200_StopCyclicAcquisition(struct comedi_device *dev, struct comedi_subdevice *s)
{
	unsigned int ui_Configuration = 0;
	
	
	
	
	s_BoardInfos[dev->minor].i_InterruptFlag = 0;
	s_BoardInfos[dev->minor].i_Initialised = 0;
	s_BoardInfos[dev->minor].i_Count = 0;
	s_BoardInfos[dev->minor].i_Sum = 0;

  
	
  
	
	ui_Configuration =
		inl(devpriv->iobase + s_BoardInfos[dev->minor].i_Offset + 8);
  
	
  
	
	while (((inl(devpriv->iobase + s_BoardInfos[dev->minor].i_Offset +
					12) >> 19) & 1) != 1) ;
	
	outl((ui_Configuration & 0xFFE7FFFF),
		devpriv->iobase + s_BoardInfos[dev->minor].i_Offset + 8);
	return 0;
}


int i_APCI3200_CommandAnalogInput(struct comedi_device *dev, struct comedi_subdevice *s)
{
	struct comedi_cmd *cmd = &s->async->cmd;
	unsigned int ui_Configuration = 0;
	
	unsigned int ui_Trigger = 0;
	unsigned int ui_TriggerEdge = 0;
	unsigned int ui_Triggermode = 0;
	unsigned int ui_ScanMode = 0;
	unsigned int ui_ConvertTime = 0;
	unsigned int ui_ConvertTimeBase = 0;
	unsigned int ui_DelayTime = 0;
	unsigned int ui_DelayTimeBase = 0;
	unsigned int ui_DelayMode = 0;
	
	
	s_BoardInfos[dev->minor].i_FirstChannel = cmd->chanlist[0];
	s_BoardInfos[dev->minor].i_LastChannel = cmd->chanlist[1];
	if (cmd->start_src == TRIG_EXT) {
		ui_Trigger = 1;
		ui_TriggerEdge = cmd->start_arg & 0xFFFF;
		ui_Triggermode = cmd->start_arg >> 16;
	}			
	else {
		ui_Trigger = 0;
	}			

	if (cmd->stop_src == TRIG_COUNT) {
		ui_ScanMode = 0;
	}			
	else {
		ui_ScanMode = 2;
	}			

	if (cmd->scan_begin_src == TRIG_FOLLOW) {
		ui_DelayTime = 0;
		ui_DelayTimeBase = 0;
		ui_DelayMode = 0;
	}			
	else {
		ui_DelayTime = cmd->scan_begin_arg & 0xFFFF;
		ui_DelayTimeBase = cmd->scan_begin_arg >> 16;
		ui_DelayMode = 1;
	}			
	
	
	if (cmd->convert_src == TRIG_TIMER) {
		ui_ConvertTime = cmd->convert_arg & 0xFFFF;
		ui_ConvertTimeBase = cmd->convert_arg >> 16;
	} else {
		ui_ConvertTime = 0;
		ui_ConvertTimeBase = 0;
	}

	
	
  
	
  
	
	ui_Configuration =
		inl(devpriv->iobase + s_BoardInfos[dev->minor].i_Offset + 12);
  
	
  
	
	while (((inl(devpriv->iobase + s_BoardInfos[dev->minor].i_Offset +
					12) >> 19) & 1) != 1) ;
	
	outl((ui_Configuration & 0xFFC00000),
		devpriv->iobase + s_BoardInfos[dev->minor].i_Offset + 12);
	
	ui_Configuration = 0;
	
	
	
	
	
	
	

	
	ui_Configuration =
		s_BoardInfos[dev->minor].i_FirstChannel | (s_BoardInfos[dev->
			minor].
		i_LastChannel << 8) | 0x00100000 | (ui_Trigger << 24) |
		(ui_TriggerEdge << 25) | (ui_Triggermode << 27) | (ui_DelayMode
		<< 18) | (ui_ScanMode << 16);

  
	
  
	
	while (((inl(devpriv->iobase + s_BoardInfos[dev->minor].i_Offset +
					12) >> 19) & 1) != 1) ;
	
	outl(ui_Configuration,
		devpriv->iobase + s_BoardInfos[dev->minor].i_Offset + 0x8);
  
	
  
	
	while (((inl(devpriv->iobase + s_BoardInfos[dev->minor].i_Offset +
					12) >> 19) & 1) != 1) ;
	
	outl(ui_DelayTime,
		devpriv->iobase + s_BoardInfos[dev->minor].i_Offset + 40);
  
	
  
	
	while (((inl(devpriv->iobase + s_BoardInfos[dev->minor].i_Offset +
					12) >> 19) & 1) != 1) ;
	
	outl(ui_DelayTimeBase,
		devpriv->iobase + s_BoardInfos[dev->minor].i_Offset + 44);
  
	
  
	
	while (((inl(devpriv->iobase + s_BoardInfos[dev->minor].i_Offset +
					12) >> 19) & 1) != 1) ;
	
	outl(ui_ConvertTime,
		devpriv->iobase + s_BoardInfos[dev->minor].i_Offset + 32);

  
	
  
	
	while (((inl(devpriv->iobase + s_BoardInfos[dev->minor].i_Offset +
					12) >> 19) & 1) != 1) ;
	
	outl(ui_ConvertTimeBase,
		devpriv->iobase + s_BoardInfos[dev->minor].i_Offset + 36);
  
	
  
	
	ui_Configuration =
		inl(devpriv->iobase + s_BoardInfos[dev->minor].i_Offset + 4);
  
	
  
	
	while (((inl(devpriv->iobase + s_BoardInfos[dev->minor].i_Offset +
					12) >> 19) & 1) != 1) ;

	
	outl(((ui_Configuration & 0x1E0FF) | 0x00002000),
		devpriv->iobase + s_BoardInfos[dev->minor].i_Offset + 4);
  
	
  
	ui_Configuration = 0;
	
	ui_Configuration =
		inl(devpriv->iobase + s_BoardInfos[dev->minor].i_Offset + 8);

  
	
  
	
	while (((inl(devpriv->iobase + s_BoardInfos[dev->minor].i_Offset +
					12) >> 19) & 1) != 1) ;
	
	outl((ui_Configuration | 0x00080000),
		devpriv->iobase + s_BoardInfos[dev->minor].i_Offset + 8);
	return 0;
}


int i_APCI3200_Reset(struct comedi_device *dev)
{
	int i_Temp;
	unsigned int dw_Dummy;
	
	
	
	

	s_BoardInfos[dev->minor].i_InterruptFlag = 0;
	s_BoardInfos[dev->minor].i_Initialised = 0;
	s_BoardInfos[dev->minor].i_Count = 0;
	s_BoardInfos[dev->minor].i_Sum = 0;
	s_BoardInfos[dev->minor].b_StructInitialized = 0;

	outl(0x83838383, devpriv->i_IobaseAmcc + 0x60);

	
	dw_Dummy = inl(devpriv->i_IobaseAmcc + 0x38);
	outl(dw_Dummy | 0x2000, devpriv->i_IobaseAmcc + 0x38);
	outl(0, devpriv->i_IobaseAddon);	
  
	
  
	for (i_Temp = 0; i_Temp <= 95; i_Temp++) {
		
		s_BoardInfos[dev->minor].ui_InterruptChannelValue[i_Temp] = 0;
	}			
  
	
  
	for (i_Temp = 0; i_Temp <= 192;) {
		while (((inl(devpriv->iobase + i_Temp + 12) >> 19) & 1) != 1) ;
		outl(0, devpriv->iobase + i_Temp + 8);
		i_Temp = i_Temp + 64;
	}			
	return 0;
}

void v_APCI3200_Interrupt(int irq, void *d)
{
	struct comedi_device *dev = d;
	unsigned int ui_StatusRegister = 0;
	unsigned int ui_ChannelNumber = 0;
	int i_CalibrationFlag = 0;
	int i_CJCFlag = 0;
	unsigned int ui_DummyValue = 0;
	unsigned int ui_DigitalTemperature = 0;
	unsigned int ui_DigitalInput = 0;
	int i_ConvertCJCCalibration;

	
	int i_ReturnValue = 0;
	

	

	
	switch (s_BoardInfos[dev->minor].i_ScanType) {
	case 0:
	case 1:
		
		switch (s_BoardInfos[dev->minor].i_ADDIDATAType) {
		case 0:
		case 1:

	  
			
	  
			
			ui_StatusRegister =
				inl(devpriv->iobase +
				s_BoardInfos[dev->minor].i_Offset + 16);
			if ((ui_StatusRegister & 0x2) == 0x2) {
				
				i_CalibrationFlag =
					((inl(devpriv->iobase +
							s_BoardInfos[dev->
								minor].
							i_Offset +
							12) & 0x00060000) >>
					17);
	      
				
	      
				

	      
				
	      
				
				ui_DigitalInput =
					inl(devpriv->iobase +
					s_BoardInfos[dev->minor].i_Offset + 28);

	      
				
	      
				if (i_CalibrationFlag == 0) {
					
					s_BoardInfos[dev->minor].
						ui_InterruptChannelValue
						[s_BoardInfos[dev->minor].
						i_Count + 0] = ui_DigitalInput;

					
					

		  
					
		  
					i_APCI3200_ReadCalibrationOffsetValue
						(dev, &ui_DummyValue);
				}	
	      
				
	      

				if (i_CalibrationFlag == 1) {

		  
					
		  

					
					s_BoardInfos[dev->minor].
						ui_InterruptChannelValue
						[s_BoardInfos[dev->minor].
						i_Count + 1] = ui_DigitalInput;

		  
					
		  
					i_APCI3200_ReadCalibrationGainValue(dev,
						&ui_DummyValue);
				}	
	      
				
	      

				if (i_CalibrationFlag == 2) {

		  
					
		  
					
					s_BoardInfos[dev->minor].
						ui_InterruptChannelValue
						[s_BoardInfos[dev->minor].
						i_Count + 2] = ui_DigitalInput;
					
					if (s_BoardInfos[dev->minor].
						i_ScanType == 1) {

						
						s_BoardInfos[dev->minor].
							i_InterruptFlag = 0;
						
						
						
						s_BoardInfos[dev->minor].
							i_Count =
							s_BoardInfos[dev->
							minor].i_Count + 9;
						
					}	
					else {
						
						s_BoardInfos[dev->minor].
							i_Count = 0;
					}	
					
					if (s_BoardInfos[dev->minor].
						i_ScanType != 1) {
						i_ReturnValue = send_sig(SIGIO, devpriv->tsk_Current, 0);	
					}	
					else {
						
						if (s_BoardInfos[dev->minor].
							i_ChannelCount ==
							s_BoardInfos[dev->
								minor].i_Sum) {
							send_sig(SIGIO, devpriv->tsk_Current, 0);	
						}
					}	
				}	
			}	

			break;

		case 2:
	  
			
	  

			
			ui_StatusRegister =
				inl(devpriv->iobase +
				s_BoardInfos[dev->minor].i_Offset + 16);
	  
			
	  

			if ((ui_StatusRegister & 0x2) == 0x2) {

				
				i_CJCFlag =
					((inl(devpriv->iobase +
							s_BoardInfos[dev->
								minor].
							i_Offset +
							4) & 0x00000400) >> 10);

				
				i_CalibrationFlag =
					((inl(devpriv->iobase +
							s_BoardInfos[dev->
								minor].
							i_Offset +
							12) & 0x00060000) >>
					17);

	      
				
	      

				
				ui_ChannelNumber =
					inl(devpriv->iobase +
					s_BoardInfos[dev->minor].i_Offset + 24);
				
				s_BoardInfos[dev->minor].ui_Channel_num =
					ui_ChannelNumber;
				

	      
				
	      
				
				ui_DigitalTemperature =
					inl(devpriv->iobase +
					s_BoardInfos[dev->minor].i_Offset + 28);

	      
				
	      

				if ((i_CalibrationFlag == 0)
					&& (i_CJCFlag == 0)) {
					
					s_BoardInfos[dev->minor].
						ui_InterruptChannelValue
						[s_BoardInfos[dev->minor].
						i_Count + 0] =
						ui_DigitalTemperature;

		  
					
		  
					i_APCI3200_ReadCJCValue(dev,
						&ui_DummyValue);

				}	

		 
				
		 

				if ((i_CJCFlag == 1)
					&& (i_CalibrationFlag == 0)) {
					
					s_BoardInfos[dev->minor].
						ui_InterruptChannelValue
						[s_BoardInfos[dev->minor].
						i_Count + 3] =
						ui_DigitalTemperature;

		  
					
		  
					i_APCI3200_ReadCalibrationOffsetValue
						(dev, &ui_DummyValue);
				}	

		 
				
		 

				if ((i_CalibrationFlag == 1)
					&& (i_CJCFlag == 0)) {
					
					s_BoardInfos[dev->minor].
						ui_InterruptChannelValue
						[s_BoardInfos[dev->minor].
						i_Count + 1] =
						ui_DigitalTemperature;

		  
					
		  
					i_APCI3200_ReadCalibrationGainValue(dev,
						&ui_DummyValue);

				}	

	      
				
	      

				if ((i_CalibrationFlag == 2)
					&& (i_CJCFlag == 0)) {
					
					s_BoardInfos[dev->minor].
						ui_InterruptChannelValue
						[s_BoardInfos[dev->minor].
						i_Count + 2] =
						ui_DigitalTemperature;

		  
					
		  

					
		  
					
					if (s_BoardInfos[dev->minor].
						i_CJCPolarity !=
						s_BoardInfos[dev->minor].
						i_ADDIDATAPolarity) {
						i_ConvertCJCCalibration = 1;
					}	
					else {
						
						if (s_BoardInfos[dev->minor].
							i_CJCGain ==
							s_BoardInfos[dev->
								minor].
							i_ADDIDATAGain) {
							i_ConvertCJCCalibration
								= 0;
						}	
						else {
							i_ConvertCJCCalibration
								= 1;
						}	
					}	
					if (i_ConvertCJCCalibration == 1) {
		      
						
		      
						i_APCI3200_ReadCJCCalOffset(dev,
							&ui_DummyValue);

					}	
					else {
						
						
						s_BoardInfos[dev->minor].
							ui_InterruptChannelValue
							[s_BoardInfos[dev->
								minor].i_Count +
							4] = 0;
						s_BoardInfos[dev->minor].
							ui_InterruptChannelValue
							[s_BoardInfos[dev->
								minor].i_Count +
							5] = 0;
					}	
				}	

		 
				
		 

				if ((i_CalibrationFlag == 1)
					&& (i_CJCFlag == 1)) {
					
					s_BoardInfos[dev->minor].
						ui_InterruptChannelValue
						[s_BoardInfos[dev->minor].
						i_Count + 4] =
						ui_DigitalTemperature;

		  
					
		  
					i_APCI3200_ReadCJCCalGain(dev,
						&ui_DummyValue);

				}	

	      
				
	      

				if ((i_CalibrationFlag == 2)
					&& (i_CJCFlag == 1)) {
					
					s_BoardInfos[dev->minor].
						ui_InterruptChannelValue
						[s_BoardInfos[dev->minor].
						i_Count + 5] =
						ui_DigitalTemperature;

					
					if (s_BoardInfos[dev->minor].
						i_ScanType == 1) {

						
						s_BoardInfos[dev->minor].
							i_InterruptFlag = 0;
						
						
						
						s_BoardInfos[dev->minor].
							i_Count =
							s_BoardInfos[dev->
							minor].i_Count + 9;
						
					}	
					else {
						
						s_BoardInfos[dev->minor].
							i_Count = 0;
					}	

					
					if (s_BoardInfos[dev->minor].
						i_ScanType != 1) {
						send_sig(SIGIO, devpriv->tsk_Current, 0);	
					}	
					else {
						
						if (s_BoardInfos[dev->minor].
							i_ChannelCount ==
							s_BoardInfos[dev->
								minor].i_Sum) {
							send_sig(SIGIO, devpriv->tsk_Current, 0);	

						}	
					}	
				}	

			}	
			break;
		}		
		break;
	case 2:
	case 3:
		i_APCI3200_InterruptHandleEos(dev);
		break;
	}			
	return;
}

int i_APCI3200_InterruptHandleEos(struct comedi_device *dev)
{
	unsigned int ui_StatusRegister = 0;
	struct comedi_subdevice *s = dev->subdevices + 0;

	
	
	
	
	int n = 0, i = 0;
	

  
	
  
	
	ui_StatusRegister =
		inl(devpriv->iobase + s_BoardInfos[dev->minor].i_Offset + 16);

  
	
  

	if ((ui_StatusRegister & 0x2) == 0x2) {
      
		
      
		
		
		
		
		s->async->events = 0;
		

      
		
      

		
		
		
		s_BoardInfos[dev->minor].ui_ScanValueArray[s_BoardInfos[dev->
				minor].i_Count] =
			inl(devpriv->iobase +
			s_BoardInfos[dev->minor].i_Offset + 28);
		

		
		if ((s_BoardInfos[dev->minor].i_Count ==
				(s_BoardInfos[dev->minor].i_LastChannel -
					s_BoardInfos[dev->minor].
					i_FirstChannel + 3))) {

			
			s_BoardInfos[dev->minor].i_Count++;

			for (i = s_BoardInfos[dev->minor].i_FirstChannel;
				i <= s_BoardInfos[dev->minor].i_LastChannel;
				i++) {
				i_APCI3200_GetChannelCalibrationValue(dev, i,
					&s_BoardInfos[dev->minor].
					ui_ScanValueArray[s_BoardInfos[dev->
							minor].i_Count + ((i -
								s_BoardInfos
								[dev->minor].
								i_FirstChannel)
							* 3)],
					&s_BoardInfos[dev->minor].
					ui_ScanValueArray[s_BoardInfos[dev->
							minor].i_Count + ((i -
								s_BoardInfos
								[dev->minor].
								i_FirstChannel)
							* 3) + 1],
					&s_BoardInfos[dev->minor].
					ui_ScanValueArray[s_BoardInfos[dev->
							minor].i_Count + ((i -
								s_BoardInfos
								[dev->minor].
								i_FirstChannel)
							* 3) + 2]);
			}

			

			

			s_BoardInfos[dev->minor].i_Count = -1;

			
			
			
			
			
			
			
			

			
			s->async->events |= COMEDI_CB_EOS;

			
			
			n = comedi_buf_write_alloc(s->async,
				(7 + 12) * sizeof(unsigned int));

			
			if (n > ((7 + 12) * sizeof(unsigned int))) {
				printk("\ncomedi_buf_write_alloc n = %i", n);
				s->async->events |= COMEDI_CB_ERROR;
			}
			
			comedi_buf_memcpy_to(s->async, 0,
				(unsigned int *) s_BoardInfos[dev->minor].
				ui_ScanValueArray, (7 + 12) * sizeof(unsigned int));

			
			comedi_buf_write_free(s->async,
				(7 + 12) * sizeof(unsigned int));

			
			comedi_event(dev, s);
			

			
			
			
			
			 */
			
			
			
			
		}
		
		s_BoardInfos[dev->minor].i_Count++;
	}
	
	s_BoardInfos[dev->minor].i_InterruptFlag = 0;
	return 0;
}
