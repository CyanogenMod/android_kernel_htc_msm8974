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

#define NVCMD_BEGIN_READ 	(0x7 << 5)	
#define NVCMD_LOAD_LOW   	(0x4 << 5)	
#define NVCMD_LOAD_HIGH  	(0x5 << 5)	
#define EE76_CMD_LEN    	13	
#define EE_READ         	0x0180	

#define EEPROM_DIGITALINPUT 			0
#define EEPROM_DIGITALOUTPUT			1
#define EEPROM_ANALOGINPUT				2
#define EEPROM_ANALOGOUTPUT				3
#define EEPROM_TIMER					4
#define EEPROM_WATCHDOG					5
#define EEPROM_TIMER_WATCHDOG_COUNTER	10

struct str_Functionality {
	unsigned char b_Type;
	unsigned short w_Address;
};

struct str_MainHeader {
	unsigned short w_HeaderSize;
	unsigned char b_Nfunctions;
	struct str_Functionality s_Functions[7];
};

struct str_DigitalInputHeader {
	unsigned short w_Nchannel;
	unsigned char b_Interruptible;
	unsigned short w_NinterruptLogic;
};

struct str_DigitalOutputHeader {

	unsigned short w_Nchannel;
};



struct str_TimerDetails {

	unsigned short w_HeaderSize;
	unsigned char b_Resolution;
	unsigned char b_Mode;		
	unsigned short w_MinTiming;
	unsigned char b_TimeBase;
};

struct str_TimerMainHeader {


	unsigned short w_Ntimer;
	struct str_TimerDetails s_TimerDetails[4];	
};


struct str_AnalogOutputHeader {
	unsigned short w_Nchannel;
	unsigned char b_Resolution;
};

struct str_AnalogInputHeader {
	unsigned short w_Nchannel;
	unsigned short w_MinConvertTiming;
	unsigned short w_MinDelayTiming;
	unsigned char b_HasDma;
	unsigned char b_Resolution;
};


		
		
		

int i_EepromReadMainHeader(unsigned short w_PCIBoardEepromAddress,
	char *pc_PCIChipInformation, struct comedi_device *dev);

int i_EepromReadDigitalInputHeader(unsigned short w_PCIBoardEepromAddress,
	char *pc_PCIChipInformation, unsigned short w_Address,
	struct str_DigitalInputHeader *s_Header);

int i_EepromReadDigitalOutputHeader(unsigned short w_PCIBoardEepromAddress,
	char *pc_PCIChipInformation, unsigned short w_Address,
	struct str_DigitalOutputHeader *s_Header);

int i_EepromReadTimerHeader(unsigned short w_PCIBoardEepromAddress,
	char *pc_PCIChipInformation, unsigned short w_Address,
	struct str_TimerMainHeader *s_Header);

int i_EepromReadAnlogOutputHeader(unsigned short w_PCIBoardEepromAddress,
	char *pc_PCIChipInformation, unsigned short w_Address,
	struct str_AnalogOutputHeader *s_Header);

int i_EepromReadAnlogInputHeader(unsigned short w_PCIBoardEepromAddress,
	char *pc_PCIChipInformation, unsigned short w_Address,
	struct str_AnalogInputHeader *s_Header);

		
		
		
unsigned short w_EepromReadWord(unsigned short w_PCIBoardEepromAddress, char *pc_PCIChipInformation,
	unsigned short w_EepromStartAddress);
void v_EepromWaitBusy(unsigned short w_PCIBoardEepromAddress);
void v_EepromClock76(unsigned int dw_Address, unsigned int dw_RegisterValue);
void v_EepromWaitBusy(unsigned short w_PCIBoardEepromAddress);
void v_EepromSendCommand76(unsigned int dw_Address, unsigned int dw_EepromCommand,
	unsigned char b_DataLengthInBits);
void v_EepromCs76Read(unsigned int dw_Address, unsigned short w_offset, unsigned short *pw_Value);


unsigned short w_EepromReadWord(unsigned short w_PCIBoardEepromAddress, char *pc_PCIChipInformation,
	unsigned short w_EepromStartAddress)
{

	unsigned char b_Counter = 0;

	unsigned char b_ReadByte = 0;

	unsigned char b_ReadLowByte = 0;

	unsigned char b_ReadHighByte = 0;

	unsigned char b_SelectedAddressLow = 0;

	unsigned char b_SelectedAddressHigh = 0;

	unsigned short w_ReadWord = 0;

	

	

	

	if ((!strcmp(pc_PCIChipInformation, "S5920")) ||
		(!strcmp(pc_PCIChipInformation, "S5933")))
	{

		for (b_Counter = 0; b_Counter < 2; b_Counter++)
		{

			b_SelectedAddressLow = (w_EepromStartAddress + b_Counter) % 256;	

			b_SelectedAddressHigh = (w_EepromStartAddress + b_Counter) / 256;	

	      

			

	      

			outb(NVCMD_LOAD_LOW, w_PCIBoardEepromAddress + 0x3F);

	      

			

	      

			v_EepromWaitBusy(w_PCIBoardEepromAddress);

	      

			

	      

			outb(b_SelectedAddressLow,
				w_PCIBoardEepromAddress + 0x3E);

	      

			

	      

			v_EepromWaitBusy(w_PCIBoardEepromAddress);

	      

			

	      

			outb(NVCMD_LOAD_HIGH, w_PCIBoardEepromAddress + 0x3F);

	      

			

	      

			v_EepromWaitBusy(w_PCIBoardEepromAddress);

	      

			

	      

			outb(b_SelectedAddressHigh,
				w_PCIBoardEepromAddress + 0x3E);

	      

			

	      

			v_EepromWaitBusy(w_PCIBoardEepromAddress);

	      

			

	      

			outb(NVCMD_BEGIN_READ, w_PCIBoardEepromAddress + 0x3F);

	      

			

	      

			v_EepromWaitBusy(w_PCIBoardEepromAddress);

	      

			

	      

			b_ReadByte = inb(w_PCIBoardEepromAddress + 0x3E);

	      

			

	      

			v_EepromWaitBusy(w_PCIBoardEepromAddress);

	      

			

	      

			if (b_Counter == 0)
			{

				b_ReadLowByte = b_ReadByte;

			}	

			else
			{

				b_ReadHighByte = b_ReadByte;

			}	

		}		

		w_ReadWord = (b_ReadLowByte | (((unsigned short) b_ReadHighByte) * 256));

	}			

	if (!strcmp(pc_PCIChipInformation, "93C76"))
	{

	   

		

	   

		v_EepromCs76Read(w_PCIBoardEepromAddress, w_EepromStartAddress,
			&w_ReadWord);

	}

	return w_ReadWord;

}


void v_EepromWaitBusy(unsigned short w_PCIBoardEepromAddress)
{

	unsigned char b_EepromBusy = 0;

	do
	{

	   

		

	   

	   

		/* An error has been written in the AMCC 5933 book at the page B-13 */

		

		

		

		

	   

		b_EepromBusy = inb(w_PCIBoardEepromAddress + 0x3F);
		b_EepromBusy = b_EepromBusy & 0x80;

	} while (b_EepromBusy == 0x80);

}


void v_EepromClock76(unsigned int dw_Address, unsigned int dw_RegisterValue)
{

   

	

   

	outl(dw_RegisterValue & 0x6, dw_Address);

   

	

   

	udelay(100);

   

	

   

	outl(dw_RegisterValue | 0x1, dw_Address);

   

	

   

	udelay(100);

}


void v_EepromSendCommand76(unsigned int dw_Address, unsigned int dw_EepromCommand,
	unsigned char b_DataLengthInBits)
{

	char c_BitPos = 0;

	unsigned int dw_RegisterValue = 0;

   

	

   

	dw_RegisterValue = 0x2;

   

	

   

	outl(dw_RegisterValue, dw_Address);

   

	

   

	udelay(100);

   

	

   

	for (c_BitPos = (b_DataLengthInBits - 1); c_BitPos >= 0; c_BitPos--)
	{

      

		

      

		if (dw_EepromCommand & (1 << c_BitPos))
		{

	 

			

	 

			dw_RegisterValue = dw_RegisterValue | 0x4;

		}

		else
		{

	 

			

	 

			dw_RegisterValue = dw_RegisterValue & 0x3;

		}

      

		

      

		outl(dw_RegisterValue, dw_Address);

      

		

      

		udelay(100);

      

		

      

		v_EepromClock76(dw_Address, dw_RegisterValue);

	}

}


void v_EepromCs76Read(unsigned int dw_Address, unsigned short w_offset, unsigned short *pw_Value)
{

        char c_BitPos = 0;

	unsigned int dw_RegisterValue = 0;

	unsigned int dw_RegisterValueRead = 0;

   

	

   

	v_EepromSendCommand76(dw_Address, (EE_READ << 4) | (w_offset / 2),
		EE76_CMD_LEN);

   

	

   

	dw_RegisterValue = (((w_offset / 2) & 0x1) << 2) | 0x2;

   

	

   

	*pw_Value = 0;

   

	

   

	for (c_BitPos = 0; c_BitPos < 16; c_BitPos++)
	{

      

		

      

		v_EepromClock76(dw_Address, dw_RegisterValue);

      

		

      

		dw_RegisterValueRead = inl(dw_Address);

      

		

      

		udelay(100);

      

		

      

		if (dw_RegisterValueRead & 0x8)
		{

	 

			

	 

			*pw_Value = (*pw_Value << 1) | 0x1;

		}

		else
		{

	 

			

	 

			*pw_Value = (*pw_Value << 1);

		}

	}

   

	

   

	dw_RegisterValue = 0x0;

   

	

   

	outl(dw_RegisterValue, dw_Address);

   

	

   

	udelay(100);

}

	
	
	


int i_EepromReadMainHeader(unsigned short w_PCIBoardEepromAddress,
	char *pc_PCIChipInformation, struct comedi_device *dev)
{
	unsigned short w_Temp, i, w_Count = 0;
	unsigned int ui_Temp;
	struct str_MainHeader s_MainHeader;
	struct str_DigitalInputHeader s_DigitalInputHeader;
	struct str_DigitalOutputHeader s_DigitalOutputHeader;
	
	struct str_AnalogOutputHeader s_AnalogOutputHeader;
	struct str_AnalogInputHeader s_AnalogInputHeader;

	
	s_MainHeader.w_HeaderSize =
		w_EepromReadWord(w_PCIBoardEepromAddress, pc_PCIChipInformation,
		0x100 + 8);

	
	w_Temp = w_EepromReadWord(w_PCIBoardEepromAddress,
		pc_PCIChipInformation, 0x100 + 10);
	s_MainHeader.b_Nfunctions = (unsigned char) w_Temp & 0x00FF;

	
	for (i = 0; i < s_MainHeader.b_Nfunctions; i++) {
		
		w_Temp = w_EepromReadWord(w_PCIBoardEepromAddress,
			pc_PCIChipInformation, 0x100 + 12 + w_Count);
		s_MainHeader.s_Functions[i].b_Type = (unsigned char) w_Temp & 0x3F;
		w_Count = w_Count + 2;
		
		s_MainHeader.s_Functions[i].w_Address =
			w_EepromReadWord(w_PCIBoardEepromAddress,
			pc_PCIChipInformation, 0x100 + 12 + w_Count);
		w_Count = w_Count + 2;
	}

	
	for (i = 0; i < s_MainHeader.b_Nfunctions; i++) {

		switch (s_MainHeader.s_Functions[i].b_Type) {
		case EEPROM_DIGITALINPUT:
			i_EepromReadDigitalInputHeader(w_PCIBoardEepromAddress,
				pc_PCIChipInformation,
				s_MainHeader.s_Functions[i].w_Address,
				&s_DigitalInputHeader);
			devpriv->s_EeParameters.i_NbrDiChannel =
				s_DigitalInputHeader.w_Nchannel;
			break;

		case EEPROM_DIGITALOUTPUT:
			i_EepromReadDigitalOutputHeader(w_PCIBoardEepromAddress,
				pc_PCIChipInformation,
				s_MainHeader.s_Functions[i].w_Address,
				&s_DigitalOutputHeader);
			devpriv->s_EeParameters.i_NbrDoChannel =
				s_DigitalOutputHeader.w_Nchannel;
			ui_Temp = 0xffffffff;
			devpriv->s_EeParameters.i_DoMaxdata =
				ui_Temp >> (32 -
					devpriv->s_EeParameters.i_NbrDoChannel);
			break;

		case EEPROM_ANALOGINPUT:
			i_EepromReadAnlogInputHeader(w_PCIBoardEepromAddress,
				pc_PCIChipInformation,
				s_MainHeader.s_Functions[i].w_Address,
				&s_AnalogInputHeader);
			if (!(strcmp(this_board->pc_DriverName, "apci3200")))
				devpriv->s_EeParameters.i_NbrAiChannel =
					s_AnalogInputHeader.w_Nchannel * 4;
			else
				devpriv->s_EeParameters.i_NbrAiChannel =
					s_AnalogInputHeader.w_Nchannel;
			devpriv->s_EeParameters.i_Dma =
				s_AnalogInputHeader.b_HasDma;
			devpriv->s_EeParameters.ui_MinAcquisitiontimeNs =
				(unsigned int) s_AnalogInputHeader.w_MinConvertTiming *
				1000;
			devpriv->s_EeParameters.ui_MinDelaytimeNs =
				(unsigned int) s_AnalogInputHeader.w_MinDelayTiming *
				1000;
			ui_Temp = 0xffff;
			devpriv->s_EeParameters.i_AiMaxdata =
				ui_Temp >> (16 -
				s_AnalogInputHeader.b_Resolution);
			break;

		case EEPROM_ANALOGOUTPUT:
			i_EepromReadAnlogOutputHeader(w_PCIBoardEepromAddress,
				pc_PCIChipInformation,
				s_MainHeader.s_Functions[i].w_Address,
				&s_AnalogOutputHeader);
			devpriv->s_EeParameters.i_NbrAoChannel =
				s_AnalogOutputHeader.w_Nchannel;
			ui_Temp = 0xffff;
			devpriv->s_EeParameters.i_AoMaxdata =
				ui_Temp >> (16 -
				s_AnalogOutputHeader.b_Resolution);
			break;

		case EEPROM_TIMER:
			
			devpriv->s_EeParameters.i_Timer = 1;
			break;

		case EEPROM_WATCHDOG:
			
			devpriv->s_EeParameters.i_Timer = 1;
			break;

		case EEPROM_TIMER_WATCHDOG_COUNTER:
			
			devpriv->s_EeParameters.i_Timer = 1;
			break;
		}
	}

	return 0;
}

int i_EepromReadDigitalInputHeader(unsigned short w_PCIBoardEepromAddress,
	char *pc_PCIChipInformation, unsigned short w_Address,
	struct str_DigitalInputHeader *s_Header)
{
	unsigned short w_Temp;

	
	s_Header->w_Nchannel =
		w_EepromReadWord(w_PCIBoardEepromAddress, pc_PCIChipInformation,
		0x100 + w_Address + 6);

	
	w_Temp = w_EepromReadWord(w_PCIBoardEepromAddress,
		pc_PCIChipInformation, 0x100 + w_Address + 8);
	s_Header->b_Interruptible = (unsigned char) (w_Temp >> 7) & 0x01;

	s_Header->w_NinterruptLogic =
		w_EepromReadWord(w_PCIBoardEepromAddress, pc_PCIChipInformation,
		0x100 + w_Address + 10);

	return 0;
}

int i_EepromReadDigitalOutputHeader(unsigned short w_PCIBoardEepromAddress,
	char *pc_PCIChipInformation, unsigned short w_Address,
	struct str_DigitalOutputHeader *s_Header)
{
	s_Header->w_Nchannel =
		w_EepromReadWord(w_PCIBoardEepromAddress, pc_PCIChipInformation,
		0x100 + w_Address + 6);
	return 0;
}

int i_EepromReadTimerHeader(unsigned short w_PCIBoardEepromAddress,
	char *pc_PCIChipInformation, unsigned short w_Address,
	struct str_TimerMainHeader *s_Header)
{

	unsigned short i, w_Size = 0, w_Temp;

	s_Header->w_Ntimer =
		w_EepromReadWord(w_PCIBoardEepromAddress, pc_PCIChipInformation,
		0x100 + w_Address + 6);

	for (i = 0; i < s_Header->w_Ntimer; i++) {
		s_Header->s_TimerDetails[i].w_HeaderSize =
			w_EepromReadWord(w_PCIBoardEepromAddress,
			pc_PCIChipInformation,
			0x100 + w_Address + 8 + w_Size + 0);
		w_Temp = w_EepromReadWord(w_PCIBoardEepromAddress,
			pc_PCIChipInformation,
			0x100 + w_Address + 8 + w_Size + 2);

		
		s_Header->s_TimerDetails[i].b_Resolution =
			(unsigned char) (w_Temp >> 10) & 0x3F;

		
		s_Header->s_TimerDetails[i].b_Mode =
			(unsigned char) (w_Temp >> 4) & 0x3F;

		w_Temp = w_EepromReadWord(w_PCIBoardEepromAddress,
			pc_PCIChipInformation,
			0x100 + w_Address + 8 + w_Size + 4);

		
		s_Header->s_TimerDetails[i].w_MinTiming = (w_Temp >> 6) & 0x3FF;

		
		s_Header->s_TimerDetails[i].b_TimeBase = (unsigned char) (w_Temp) & 0x3F;
		w_Size += s_Header->s_TimerDetails[i].w_HeaderSize;
	}

	return 0;
}


int i_EepromReadAnlogOutputHeader(unsigned short w_PCIBoardEepromAddress,
	char *pc_PCIChipInformation, unsigned short w_Address,
	struct str_AnalogOutputHeader *s_Header)
{
	unsigned short w_Temp;
	
	w_Temp = w_EepromReadWord(w_PCIBoardEepromAddress,
		pc_PCIChipInformation, 0x100 + w_Address + 10);
	s_Header->w_Nchannel = (w_Temp >> 4) & 0x03FF;
	
	w_Temp = w_EepromReadWord(w_PCIBoardEepromAddress,
		pc_PCIChipInformation, 0x100 + w_Address + 16);
	s_Header->b_Resolution = (unsigned char) (w_Temp >> 8) & 0xFF;
	return 0;
}


int i_EepromReadAnlogInputHeader(unsigned short w_PCIBoardEepromAddress,
	char *pc_PCIChipInformation, unsigned short w_Address,
	struct str_AnalogInputHeader *s_Header)
{
	unsigned short w_Temp, w_Offset;
	w_Temp = w_EepromReadWord(w_PCIBoardEepromAddress,
		pc_PCIChipInformation, 0x100 + w_Address + 10);
	s_Header->w_Nchannel = (w_Temp >> 4) & 0x03FF;
	s_Header->w_MinConvertTiming =
		w_EepromReadWord(w_PCIBoardEepromAddress, pc_PCIChipInformation,
		0x100 + w_Address + 16);
	s_Header->w_MinDelayTiming =
		w_EepromReadWord(w_PCIBoardEepromAddress, pc_PCIChipInformation,
		0x100 + w_Address + 30);
	w_Temp = w_EepromReadWord(w_PCIBoardEepromAddress,
		pc_PCIChipInformation, 0x100 + w_Address + 20);
	s_Header->b_HasDma = (w_Temp >> 13) & 0x01;	

	w_Temp = w_EepromReadWord(w_PCIBoardEepromAddress, pc_PCIChipInformation, 0x100 + w_Address + 72);	
	w_Temp = w_Temp & 0x00FF;
	if (w_Temp)		
	{
		w_Offset = 74 + (2 * w_Temp) + (10 * (1 + (w_Temp / 16)));	
		w_Offset = w_Offset + 2;	
	} else			
	{
		w_Offset = 74;
		w_Offset = w_Offset + 2;	
	}

	w_Temp = w_EepromReadWord(w_PCIBoardEepromAddress,
		pc_PCIChipInformation, 0x100 + w_Address + w_Offset);
	s_Header->b_Resolution = w_Temp & 0x001F;	

	return 0;
}
