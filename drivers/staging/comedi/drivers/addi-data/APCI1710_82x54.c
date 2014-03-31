/*
 *  Copyright (C) 2004,2005  ADDI-DATA GmbH for the source code of this module.
 *
 *	ADDI-DATA GmbH
 *	Dieselstrasse 3
 *	D-77833 Ottersweier
 *	Tel: +19(0)7223/9493-0
 *	Fax: +49(0)7223/9493-92
 *	http://www.addi-data.com
 *	info@addi-data.com
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 */

#include "APCI1710_82x54.h"

/*
+----------------------------------------------------------------------------+
| Function Name     : _INT_     i_APCI1710_InitTimer                         |
|                               (unsigned char_   b_BoardHandle,                      |
|                                unsigned char_   b_ModulNbr,                         |
|                                unsigned char_   b_TimerNbr,                         |
|                                unsigned char_   b_TimerMode,                        |
|                                ULONG_ ul_ReloadValue,                      |
|                                unsigned char_   b_InputClockSelection,              |
|                                unsigned char_   b_InputClockLevel,                  |
|                                unsigned char_   b_OutputLevel,                      |
|                                unsigned char_   b_HardwareGateLevel)
int i_InsnConfig_InitTimer(struct comedi_device *dev,struct comedi_subdevice *s,
	struct comedi_insn *insn,unsigned int *data)
|
+----------------------------------------------------------------------------+
| Task              : Configure the Timer (b_TimerNbr) operating mode        |
|                     (b_TimerMode) from selected module (b_ModulNbr).       |
|                     You must calling this function be for you call any     |
|                     other function witch access of the timer.              |
|                                                                            |
|                                                                            |
|                       Timer mode description table                         |
|                                                                            |
|+--------+-----------------------------+--------------+--------------------+|
||Selected+      Mode description       +u_ReloadValue | Hardware gate input||
||  mode  |                             |  description |      action        ||
|+--------+-----------------------------+--------------+--------------------+|
||        |Mode 0 is typically used     |              |                    ||
||        |for event counting. After    |              |                    ||
||        |the initialisation, OUT      |              |                    ||
||        |is initially low, and        |              |                    ||
||   0    |will remain low until the    |Start counting|   Hardware gate    ||
||        |counter reaches zero.        |   value      |                    ||
||        |OUT then goes high and       |              |                    ||
||        |remains high until a new     |              |                    ||
||        |count is written. See        |              |                    ||
||        |"i_APCI1710_WriteTimerValue" |              |                    ||
||        |function.                    |              |                    ||
|+--------+-----------------------------+--------------+--------------------+|
||        |Mode 1 is similar to mode 0  |              |                    ||
||        |except for the gate input    |              |                    ||
||   1    |action. The gate input is not|Start counting|  Hardware trigger  ||
||        |used for enabled or disabled |   value      |                    ||
||        |the timer.                   |              |                    ||
||        |The gate input is used for   |              |                    ||
||        |triggered the timer.         |              |                    ||
|+--------+-----------------------------+--------------+--------------------+|
||        |This mode functions like a   |              |                    ||
||        |divide-by-ul_ReloadValue     |              |                    ||
||        |counter. It is typically used|              |                    ||
||        |to generate a real time clock|              |                    ||
||        |interrupt. OUT will initially|              |                    ||
||   2    |be high after the            |   Division   |  Hardware gate     ||
||        |initialisation. When the     |    factor    |                    ||
||        |initial count has decremented|              |                    ||
||        |to 1, OUT goes low for one   |              |                    ||
||        |CLK pule. OUT then goes high |              |                    ||
||        |again, the counter reloads   |              |                    ||
||        |the initial count            |              |                    ||
||        |(ul_ReloadValue) and the     |              |                    ||
||        |process is repeated.         |              |                    ||
||        |This action can generated a  |              |                    ||
||        |interrupt. See function      |              |                    ||
||        |"i_APCI1710_SetBoardInt-     |              |                    ||
||        |RoutineX"                    |              |                    ||
||        |and "i_APCI1710_EnableTimer" |              |                    ||
|+--------+-----------------------------+--------------+--------------------+|
||        |Mode 3 is typically used for |              |                    ||
||        |baud rate generation. This   |              |                    ||
||        |mode is similar to mode 2    |              |                    ||
||        |except for the duty cycle of |              |                    ||
||   3    |OUT. OUT will initially be   |  Division    |   Hardware gate    ||
||        |high after the initialisation|   factor     |                    ||
||        |When half the initial count  |              |                    ||
||        |(ul_ReloadValue) has expired,|              |                    ||
||        |OUT goes low for the         |              |                    ||
||        |remainder of the count. The  |              |                    ||
||        |mode is periodic; the        |              |                    ||
||        |sequence above is repeated   |              |                    ||
||        |indefinitely.                |              |                    ||
|+--------+-----------------------------+--------------+--------------------+|
||        |OUT will be initially high   |              |                    ||
||        |after the initialisation.    |              |                    ||
||        |When the initial count       |              |                    ||
||   4    |expires OUT will go low for  |Start counting|  Hardware gate     ||
||        |one CLK pulse and then go    |    value     |                    ||
||        |high again.                  |              |                    ||
||        |The counting sequences is    |              |                    ||
||        |triggered by writing a new   |              |                    ||
||        |value. See                   |              |                    ||
||        |"i_APCI1710_WriteTimerValue" |              |                    ||
||        |function. If a new count is  |              |                    ||
||        |written during counting,     |              |                    ||
||        |it will be loaded on the     |              |                    ||
||        |next CLK pulse               |              |                    ||
|+--------+-----------------------------+--------------+--------------------+|
||        |Mode 5 is similar to mode 4  |              |                    ||
||        |except for the gate input    |              |                    ||
||        |action. The gate input is not|              |                    ||
||   5    |used for enabled or disabled |Start counting|  Hardware trigger  ||
||        |the timer. The gate input is |    value     |                    ||
||        |used for triggered the timer.|              |                    ||
|+--------+-----------------------------+--------------+--------------------+|
|                                                                            |
|                                                                            |
|                                                                            |
|                      Input clock selection table                           |
|                                                                            |
|  +--------------------------------+------------------------------------+   |
|  |       b_InputClockSelection    |           Description              |   |
|  |           parameter            |                                    |   |
|  +--------------------------------+------------------------------------+   |
|  |    APCI1710_PCI_BUS_CLOCK      | For the timer input clock, the PCI |   |
|  |                                | bus clock / 4 is used. This PCI bus|   |
|  |                                | clock can be 30MHz or 33MHz. For   |   |
|  |                                | Timer 0 only this selection are    |   |
|  |                                | available.                         |   |
|  +--------------------------------+------------------------------------+   |
|  | APCI1710_ FRONT_CONNECTOR_INPUT| Of the front connector you have the|   |
|  |                                | possibility to inject a input clock|   |
|  |                                | for Timer 1 or Timer 2. The source |   |
|  |                                | from this clock can eat the output |   |
|  |                                | clock from Timer 0 or any other    |   |
|  |                                | clock source.                      |   |
|  +--------------------------------+------------------------------------+   |
|                                                                            |
+----------------------------------------------------------------------------+
| Input Parameters  : unsigned char_   b_BoardHandle        : Handle of board         |
|                                                    APCI-1710               |
|                     unsigned char_   b_ModulNbr           : Module number to        |
|                                                    configure (0 to 3)      |
|                     unsigned char_   b_TimerNbr           : Timer number to         |
|                                                    configure (0 to 2)      |
|                     unsigned char_   b_TimerMode          : Timer mode selection    |
|                                                    (0 to 5)                |
|                                                    0: Interrupt on terminal|
|                                                       count                |
|                                                    1: Hardware             |
|                                                       retriggerable one-   |
|                                                       shot                 |
|                                                    2: Rate generator       |
|                                                    3: Square wave mode     |
|                                                    4: Software triggered   |
|                                                       strobe               |
|                                                    5: Hardware triggered   |
|                                                       strobe               |
|                                                       See timer mode       |
|                                                       description table.   |
|                     ULONG_ ul_ReloadValue         : Start counting value   |
|                                                     or division factor     |
|                                                     See timer mode         |
|                                                     description table.     |
|                     unsigned char_   b_InputClockSelection : Selection from input   |
|                                                     timer clock.           |
|                                                     See input clock        |
|                                                     selection table.       |
|                     unsigned char_   b_InputClockLevel     : Selection from input   |
|                                                     clock level.           |
|                                                     0 : Low active         |
|                                                         (Input inverted)   |
|                                                     1 : High active        |
|                     unsigned char_   b_OutputLevel,        : Selection from output  |
|                                                     clock level.           |
|                                                     0 : Low active         |
|                                                     1 : High active        |
|                                                         (Output inverted)  |
|                     unsigned char_   b_HardwareGateLevel   : Selection from         |
|                                                     hardware gate level.   |
|                                                     0 : Low active         |
|                                                         (Input inverted)   |
|                                                     1 : High active        |
|                                                     If you will not used   |
|                                                     the hardware gate set  |
|                                                     this value to 0.
|b_ModulNbr        = (unsigned char) CR_AREF(insn->chanspec);
	b_TimerNbr		  = (unsigned char) CR_CHAN(insn->chanspec);
	b_TimerMode		  = (unsigned char) data[0];
	ul_ReloadValue	  = (unsigned int) data[1];
	b_InputClockSelection	=(unsigned char) data[2];
	b_InputClockLevel		=(unsigned char) data[3];
	b_OutputLevel			=(unsigned char) data[4];
	b_HardwareGateLevel		=(unsigned char) data[5];
+----------------------------------------------------------------------------+
| Output Parameters : -                                                      |
+----------------------------------------------------------------------------+
| Return Value      : 0: No error                                            |
|                    -1: The handle parameter of the board is wrong          |
|                    -2: Module selection wrong                              |
|                    -3: Timer selection wrong                               |
|                    -4: The module is not a TIMER module                    |
|                    -5: Timer mode selection is wrong                       |
|                    -6: Input timer clock selection is wrong                |
|                    -7: Selection from input clock level is wrong           |
|                    -8: Selection from output clock level is wrong          |
|                    -9: Selection from hardware gate level is wrong         |
+----------------------------------------------------------------------------+
*/

int i_APCI1710_InsnConfigInitTimer(struct comedi_device *dev, struct comedi_subdevice *s,
				   struct comedi_insn *insn, unsigned int *data)
{

	int i_ReturnValue = 0;
	unsigned char b_ModulNbr;
	unsigned char b_TimerNbr;
	unsigned char b_TimerMode;
	unsigned int ul_ReloadValue;
	unsigned char b_InputClockSelection;
	unsigned char b_InputClockLevel;
	unsigned char b_OutputLevel;
	unsigned char b_HardwareGateLevel;

	
	unsigned int dw_Test = 0;
	

	i_ReturnValue = insn->n;
	b_ModulNbr = (unsigned char) CR_AREF(insn->chanspec);
	b_TimerNbr = (unsigned char) CR_CHAN(insn->chanspec);
	b_TimerMode = (unsigned char) data[0];
	ul_ReloadValue = (unsigned int) data[1];
	b_InputClockSelection = (unsigned char) data[2];
	b_InputClockLevel = (unsigned char) data[3];
	b_OutputLevel = (unsigned char) data[4];
	b_HardwareGateLevel = (unsigned char) data[5];

	
	if (b_ModulNbr < 4) {
		
		if ((devpriv->s_BoardInfos.dw_MolduleConfiguration[b_ModulNbr] & 0xFFFF0000UL) == APCI1710_82X54_TIMER) {
			

			if (b_TimerNbr <= 2) {
				
				if (b_TimerMode <= 5) {
					
					

					if (((b_TimerNbr == 0) &&
					     (b_InputClockSelection == APCI1710_PCI_BUS_CLOCK)) ||
					    ((b_TimerNbr == 0) &&
					     (b_InputClockSelection == APCI1710_10MHZ)) ||
					    ((b_TimerNbr != 0) &&
					     ((b_InputClockSelection == APCI1710_PCI_BUS_CLOCK) ||
					      (b_InputClockSelection == APCI1710_FRONT_CONNECTOR_INPUT) ||
					      (b_InputClockSelection == APCI1710_10MHZ)))) {
						
						if (((b_InputClockSelection == APCI1710_10MHZ) &&
						     ((devpriv->s_BoardInfos.dw_MolduleConfiguration[b_ModulNbr] & 0x0000FFFFUL) >= 0x3131)) ||
						     (b_InputClockSelection != APCI1710_10MHZ)) {
							
							

							if ((b_InputClockLevel == 0) ||
							    (b_InputClockLevel == 1)) {
								
								if ((b_OutputLevel == 0) || (b_OutputLevel == 1)) {
									
									if ((b_HardwareGateLevel == 0) || (b_HardwareGateLevel == 1)) {
										
										
										if ((b_InputClockSelection == APCI1710_10MHZ) && ((devpriv->s_BoardInfos.dw_MolduleConfiguration[b_ModulNbr] & 0x0000FFFFUL) > 0x3131)) {
											
											dw_Test = inl(devpriv->s_BoardInfos.ui_Address + (16 + (b_TimerNbr * 4) + (64 * b_ModulNbr)));

											dw_Test = (dw_Test >> 16) & 1;
										} else {
											dw_Test = 1;
										}

										
										if (dw_Test == 1) {
											
											
											devpriv->s_ModuleInfo[b_ModulNbr].s_82X54ModuleInfo.s_82X54TimerInfo[b_TimerNbr].b_82X54Init = 1;

											
											devpriv-> s_ModuleInfo[b_ModulNbr].s_82X54ModuleInfo.s_82X54TimerInfo[b_TimerNbr].b_InputClockSelection = b_InputClockSelection;

											
											devpriv->s_ModuleInfo[b_ModulNbr].s_82X54ModuleInfo.s_82X54TimerInfo[b_TimerNbr].b_InputClockLevel = ~b_InputClockLevel & 1;

											
											devpriv->s_ModuleInfo[b_ModulNbr].s_82X54ModuleInfo.s_82X54TimerInfo[b_TimerNbr].b_OutputLevel = ~b_OutputLevel & 1;

											
											devpriv->s_ModuleInfo[b_ModulNbr].s_82X54ModuleInfo.s_82X54TimerInfo[b_TimerNbr].b_HardwareGateLevel = b_HardwareGateLevel;

											
											
											
											if (b_InputClockSelection == APCI1710_10MHZ) {
												b_InputClockSelection = 2;
											}

											devpriv->s_ModuleInfo[b_ModulNbr].s_82X54ModuleInfo.s_82X54TimerInfo[b_TimerNbr].dw_ConfigurationWord = (unsigned int)(((b_HardwareGateLevel << 0) & 0x1) | ((b_InputClockLevel << 1) & 0x2) | (((~b_OutputLevel & 1) << 2) & 0x4) | ((b_InputClockSelection << 4) & 0x30));
											
											outl(devpriv->s_ModuleInfo[b_ModulNbr].s_82X54ModuleInfo.s_82X54TimerInfo[b_TimerNbr].dw_ConfigurationWord, devpriv->s_BoardInfos.ui_Address + 32 + (b_TimerNbr * 4) + (64 * b_ModulNbr));

											
											outl((unsigned int) b_TimerMode, devpriv->s_BoardInfos.ui_Address + 16 + (b_TimerNbr * 4) + (64 * b_ModulNbr));

											
											outl(ul_ReloadValue, devpriv->s_BoardInfos.ui_Address + 0 + (b_TimerNbr * 4) + (64 * b_ModulNbr));
											
										}	
										else {
											
											i_ReturnValue = -6;
										}	
										
									}	
									else {
										
										DPRINTK("Selection from hardware gate level is wrong\n");
										i_ReturnValue = -9;
									}	
								}	
								else {
									
									DPRINTK("Selection from output clock level is wrong\n");
									i_ReturnValue = -8;
								}	
							}	
							else {
								
								DPRINTK("Selection from input clock level is wrong\n");
								i_ReturnValue = -7;
							}	
						} else {
							
							DPRINTK("Input timer clock selection is wrong\n");
							i_ReturnValue = -6;
						}
					} else {
						
						DPRINTK("Input timer clock selection is wrong\n");
						i_ReturnValue = -6;
					}
				}	
				else {
					
					DPRINTK("Timer mode selection is wrong\n");
					i_ReturnValue = -5;
				}	
			}	
			else {
				
				DPRINTK("Timer selection wrong\n");
				i_ReturnValue = -3;
			}	
		} else {
			
			DPRINTK("The module is not a TIMER module\n");
			i_ReturnValue = -4;
		}
	} else {
		
		DPRINTK("Module number error\n");
		i_ReturnValue = -2;
	}

	return i_ReturnValue;
}

+----------------------------------------------------------------------------+
| Output Parameters : -                                                      |
+----------------------------------------------------------------------------+
| Return Value      : 0: No error                                            |
|                    -1: The handle parameter of the board is wrong          |
|                    -2: Module selection wrong                              |
|                    -3: Timer selection wrong                               |
|                    -4: The module is not a TIMER module                    |
|                    -5: Timer not initialised see function                  |
|                        "i_APCI1710_InitTimer"                              |
|                    -6: Interrupt parameter is wrong                        |
|                    -7: Interrupt function not initialised.                 |
|                        See function "i_APCI1710_SetBoardIntRoutineX"       |
+----------------------------------------------------------------------------+
*/

int i_APCI1710_InsnWriteEnableDisableTimer(struct comedi_device *dev,
					   struct comedi_subdevice *s,
					   struct comedi_insn *insn, unsigned int *data)
{
	int i_ReturnValue = 0;
	unsigned int dw_DummyRead;
	unsigned char b_ModulNbr;
	unsigned char b_TimerNbr;
	unsigned char b_ActionType;
	unsigned char b_InterruptEnable;

	i_ReturnValue = insn->n;
	b_ModulNbr = (unsigned char) CR_AREF(insn->chanspec);
	b_TimerNbr = (unsigned char) CR_CHAN(insn->chanspec);
	b_ActionType = (unsigned char) data[0];	

	
	if (b_ModulNbr < 4) {
		
		if ((devpriv->s_BoardInfos.dw_MolduleConfiguration[b_ModulNbr] & 0xFFFF0000UL) == APCI1710_82X54_TIMER) {
			
			if (b_TimerNbr <= 2) {
				
				if (devpriv->s_ModuleInfo[b_ModulNbr].s_82X54ModuleInfo.s_82X54TimerInfo[b_TimerNbr].b_82X54Init == 1) {

					switch (b_ActionType) {
					case APCI1710_ENABLE:
						b_InterruptEnable = (unsigned char) data[1];
						
						if ((b_InterruptEnable == APCI1710_ENABLE) ||
						    (b_InterruptEnable == APCI1710_DISABLE)) {
							if (b_InterruptEnable == APCI1710_ENABLE) {

								dw_DummyRead = inl(devpriv->s_BoardInfos.ui_Address + 12 + (b_TimerNbr * 4) + (64 * b_ModulNbr));

								
								devpriv->s_ModuleInfo[b_ModulNbr].s_82X54ModuleInfo.s_82X54TimerInfo[b_TimerNbr].dw_ConfigurationWord = devpriv->s_ModuleInfo[b_ModulNbr].s_82X54ModuleInfo.s_82X54TimerInfo[b_TimerNbr].dw_ConfigurationWord | 0x8;

								outl(devpriv->s_ModuleInfo[b_ModulNbr].s_82X54ModuleInfo.s_82X54TimerInfo[b_TimerNbr].dw_ConfigurationWord, devpriv->s_BoardInfos.ui_Address + 32 + (b_TimerNbr * 4) + (64 * b_ModulNbr));
								devpriv->tsk_Current = current;	

							}	
							else {
								
								devpriv->s_ModuleInfo[b_ModulNbr].s_82X54ModuleInfo.s_82X54TimerInfo[b_TimerNbr].dw_ConfigurationWord = devpriv->s_ModuleInfo[b_ModulNbr].s_82X54ModuleInfo.s_82X54TimerInfo[b_TimerNbr].dw_ConfigurationWord & 0xF7;

								outl(devpriv->s_ModuleInfo[b_ModulNbr].s_82X54ModuleInfo.s_82X54TimerInfo[b_TimerNbr].dw_ConfigurationWord, devpriv->s_BoardInfos.ui_Address + 32 + (b_TimerNbr * 4) + (64 * b_ModulNbr));

								
								devpriv->s_ModuleInfo[b_ModulNbr].s_82X54ModuleInfo.b_InterruptMask = devpriv->s_ModuleInfo[b_ModulNbr].s_82X54ModuleInfo.b_InterruptMask & (0xFF - (1 << b_TimerNbr));
							}	

							
							if (i_ReturnValue >= 0) {
								
								devpriv->s_ModuleInfo[b_ModulNbr].s_82X54ModuleInfo.b_InterruptMask = devpriv->s_ModuleInfo[b_ModulNbr].s_82X54ModuleInfo.b_InterruptMask | ((1 & b_InterruptEnable) << b_TimerNbr);

								
								outl(1, devpriv->s_BoardInfos.ui_Address + 44 + (b_TimerNbr * 4) + (64 * b_ModulNbr));
							}
						} else {
							
							DPRINTK("\n");
							i_ReturnValue = -6;
						}
						break;
					case APCI1710_DISABLE:
						
						if (((devpriv->s_ModuleInfo[b_ModulNbr].s_82X54ModuleInfo.b_InterruptMask >> b_TimerNbr) & 1) == 1) {
							

							devpriv->s_ModuleInfo[b_ModulNbr].s_82X54ModuleInfo.s_82X54TimerInfo[b_TimerNbr]. dw_ConfigurationWord = devpriv->s_ModuleInfo[b_ModulNbr].s_82X54ModuleInfo.s_82X54TimerInfo[b_TimerNbr].dw_ConfigurationWord & 0xF7;

							outl(devpriv->s_ModuleInfo[b_ModulNbr].s_82X54ModuleInfo.s_82X54TimerInfo[b_TimerNbr].dw_ConfigurationWord, devpriv->s_BoardInfos.ui_Address + 32 + (b_TimerNbr * 4) + (64 * b_ModulNbr));

							
							devpriv->s_ModuleInfo[b_ModulNbr].s_82X54ModuleInfo.b_InterruptMask = devpriv->s_ModuleInfo[b_ModulNbr].s_82X54ModuleInfo.b_InterruptMask & (0xFF - (1 << b_TimerNbr));
						}

						
						outl(0, devpriv->s_BoardInfos.ui_Address + 44 + (b_TimerNbr * 4) + (64 * b_ModulNbr));
						break;
					}	
				} else {
					
					DPRINTK ("Timer not initialised see function\n");
					i_ReturnValue = -5;
				}
			} else {
				
				DPRINTK("Timer selection wrong\n");
				i_ReturnValue = -3;
			}	
		} else {
			
			DPRINTK("The module is not a TIMER module\n");
			i_ReturnValue = -4;
		}
	} else {
		
		DPRINTK("Module number error\n");
		i_ReturnValue = -2;
	}

	return i_ReturnValue;
}


int i_APCI1710_InsnReadAllTimerValue(struct comedi_device *dev, struct comedi_subdevice *s,
				     struct comedi_insn *insn, unsigned int *data)
{
	int i_ReturnValue = 0;
	unsigned char b_ModulNbr, b_ReadType;
	unsigned int *pul_TimerValueArray;

	b_ModulNbr = CR_AREF(insn->chanspec);
	b_ReadType = CR_CHAN(insn->chanspec);
	pul_TimerValueArray = (unsigned int *) data;
	i_ReturnValue = insn->n;

	switch (b_ReadType) {
	case APCI1710_TIMER_READINTERRUPT:

		data[0] = devpriv->s_InterruptParameters.s_FIFOInterruptParameters[devpriv->s_InterruptParameters.ui_Read].b_OldModuleMask;
		data[1] = devpriv->s_InterruptParameters.s_FIFOInterruptParameters[devpriv->s_InterruptParameters.ui_Read].ul_OldInterruptMask;
		data[2] = devpriv->s_InterruptParameters.s_FIFOInterruptParameters[devpriv->s_InterruptParameters.ui_Read].ul_OldCounterLatchValue;

		
		devpriv->s_InterruptParameters.ui_Read = (devpriv->s_InterruptParameters.ui_Read + 1) % APCI1710_SAVE_INTERRUPT;

		break;

	case APCI1710_TIMER_READALLTIMER:
		
		if (b_ModulNbr < 4) {
			
			if ((devpriv->s_BoardInfos.dw_MolduleConfiguration[b_ModulNbr] & 0xFFFF0000UL) == APCI1710_82X54_TIMER) {
				
				if (devpriv->s_ModuleInfo[b_ModulNbr].s_82X54ModuleInfo.s_82X54TimerInfo[0].b_82X54Init == 1) {
					
					if (devpriv->s_ModuleInfo[b_ModulNbr].s_82X54ModuleInfo.s_82X54TimerInfo[1].b_82X54Init == 1) {
						
						if (devpriv->s_ModuleInfo[b_ModulNbr].s_82X54ModuleInfo.s_82X54TimerInfo[2].b_82X54Init == 1) {
							
							outl(0x17, devpriv->s_BoardInfos.ui_Address + 12 + (64 * b_ModulNbr));

							
							pul_TimerValueArray[0] = inl(devpriv->s_BoardInfos.ui_Address + 0 + (64 * b_ModulNbr));

							
							pul_TimerValueArray[1] = inl(devpriv->s_BoardInfos.ui_Address + 4 + (64 * b_ModulNbr));

							
							pul_TimerValueArray[2] = inl(devpriv->s_BoardInfos.ui_Address + 8 + (64 * b_ModulNbr));
						} else {
							
							DPRINTK("Timer 2 not initialised see function\n");
							i_ReturnValue = -6;
						}
					} else {
						
						DPRINTK("Timer 1 not initialised see function\n");
						i_ReturnValue = -5;
					}
				} else {
					
					DPRINTK("Timer 0 not initialised see function\n");
					i_ReturnValue = -4;
				}
			} else {
				
				DPRINTK("The module is not a TIMER module\n");
				i_ReturnValue = -3;
			}
		} else {
			
			DPRINTK("Module number error\n");
			i_ReturnValue = -2;
		}

	}			
	return i_ReturnValue;
}


int i_APCI1710_InsnBitsTimer(struct comedi_device *dev, struct comedi_subdevice *s,
			     struct comedi_insn *insn, unsigned int *data)
{
	unsigned char b_BitsType;
	int i_ReturnValue = 0;
	b_BitsType = data[0];

	printk("\n82X54");

	switch (b_BitsType) {
	case APCI1710_TIMER_READVALUE:
		i_ReturnValue = i_APCI1710_ReadTimerValue(dev,
							  (unsigned char)CR_AREF(insn->chanspec),
							  (unsigned char)CR_CHAN(insn->chanspec),
							  (unsigned int *) &data[0]);
		break;

	case APCI1710_TIMER_GETOUTPUTLEVEL:
		i_ReturnValue = i_APCI1710_GetTimerOutputLevel(dev,
							       (unsigned char)CR_AREF(insn->chanspec),
							       (unsigned char)CR_CHAN(insn->chanspec),
							       (unsigned char *) &data[0]);
		break;

	case APCI1710_TIMER_GETPROGRESSSTATUS:
		i_ReturnValue = i_APCI1710_GetTimerProgressStatus(dev,
								  (unsigned char)CR_AREF(insn->chanspec),
								  (unsigned char)CR_CHAN(insn->chanspec),
								  (unsigned char *)&data[0]);
		break;

	case APCI1710_TIMER_WRITEVALUE:
		i_ReturnValue = i_APCI1710_WriteTimerValue(dev,
							   (unsigned char)CR_AREF(insn->chanspec),
							   (unsigned char)CR_CHAN(insn->chanspec),
							   (unsigned int)data[1]);

		break;

	default:
		printk("Bits Config Parameter Wrong\n");
		i_ReturnValue = -1;
	}

	if (i_ReturnValue >= 0)
		i_ReturnValue = insn->n;
	return i_ReturnValue;
}


int i_APCI1710_ReadTimerValue(struct comedi_device *dev,
			      unsigned char b_ModulNbr, unsigned char b_TimerNbr,
			      unsigned int *pul_TimerValue)
{
	int i_ReturnValue = 0;

	
	if (b_ModulNbr < 4) {
		
		if ((devpriv->s_BoardInfos.
		     dw_MolduleConfiguration[b_ModulNbr] &
		     0xFFFF0000UL) == APCI1710_82X54_TIMER) {
			
			if (b_TimerNbr <= 2) {
				
				if (devpriv->
				    s_ModuleInfo[b_ModulNbr].
				    s_82X54ModuleInfo.
				    s_82X54TimerInfo[b_TimerNbr].
				    b_82X54Init == 1) {
					
					outl((2 << b_TimerNbr) | 0xD0,
					     devpriv->s_BoardInfos.
					     ui_Address + 12 +
					     (64 * b_ModulNbr));

					
					*pul_TimerValue =
					    inl(devpriv->s_BoardInfos.
						ui_Address + (b_TimerNbr * 4) +
						(64 * b_ModulNbr));
				} else {
					
					DPRINTK("Timer not initialised see function\n");
					i_ReturnValue = -5;
				}
			} else {
				
				DPRINTK("Timer selection wrong\n");
				i_ReturnValue = -3;
			}	
		} else {
			
			DPRINTK("The module is not a TIMER module\n");
			i_ReturnValue = -4;
		}
	} else {
		
		DPRINTK("Module number error\n");
		i_ReturnValue = -2;
	}

	return i_ReturnValue;
}


int i_APCI1710_GetTimerOutputLevel(struct comedi_device *dev,
				   unsigned char b_ModulNbr, unsigned char b_TimerNbr,
				   unsigned char *pb_OutputLevel)
{
	int i_ReturnValue = 0;
	unsigned int dw_TimerStatus;

	
	if (b_ModulNbr < 4) {
		
		if ((devpriv->s_BoardInfos.dw_MolduleConfiguration[b_ModulNbr] & 0xFFFF0000UL) == APCI1710_82X54_TIMER) {
			
			if (b_TimerNbr <= 2) {
				
				if (devpriv->s_ModuleInfo[b_ModulNbr].s_82X54ModuleInfo.s_82X54TimerInfo[b_TimerNbr].b_82X54Init == 1) {
					
					outl((2 << b_TimerNbr) | 0xE0, devpriv->s_BoardInfos.ui_Address + 12 + (64 * b_ModulNbr));

					
					dw_TimerStatus = inl(devpriv->s_BoardInfos.ui_Address + 16 + (b_TimerNbr * 4) + (64 * b_ModulNbr));

					*pb_OutputLevel = (unsigned char) (((dw_TimerStatus >> 7) & 1) ^ devpriv-> s_ModuleInfo[b_ModulNbr].s_82X54ModuleInfo.s_82X54TimerInfo[b_TimerNbr].b_OutputLevel);
				} else {
					
					DPRINTK("Timer not initialised see function\n");
					i_ReturnValue = -5;
				}
			} else {
				
				DPRINTK("Timer selection wrong\n");
				i_ReturnValue = -3;
			}	
		} else {
			
			DPRINTK("The module is not a TIMER module\n");
			i_ReturnValue = -4;
		}
	} else {
		
		DPRINTK("Module number error\n");
		i_ReturnValue = -2;
	}

	return i_ReturnValue;
}


int i_APCI1710_GetTimerProgressStatus(struct comedi_device *dev,
				      unsigned char b_ModulNbr, unsigned char b_TimerNbr,
				      unsigned char *pb_TimerStatus)
{
	int i_ReturnValue = 0;
	unsigned int dw_TimerStatus;

	
	if (b_ModulNbr < 4) {
		

		if ((devpriv->s_BoardInfos.dw_MolduleConfiguration[b_ModulNbr] & 0xFFFF0000UL) == APCI1710_82X54_TIMER) {
			
			if (b_TimerNbr <= 2) {
				
				if (devpriv->s_ModuleInfo[b_ModulNbr].s_82X54ModuleInfo.s_82X54TimerInfo[b_TimerNbr].b_82X54Init == 1) {
					
					outl((2 << b_TimerNbr) | 0xE0, devpriv->s_BoardInfos.ui_Address + 12 + (64 * b_ModulNbr));

					
					dw_TimerStatus = inl(devpriv->s_BoardInfos.ui_Address + 16 + (b_TimerNbr * 4) + (64 * b_ModulNbr));

					*pb_TimerStatus = (unsigned char) ((dw_TimerStatus) >> 8) & 1;
					printk("ProgressStatus : %d", *pb_TimerStatus);
				} else {
					
					i_ReturnValue = -5;
				}
			} else {
				
				i_ReturnValue = -3;
			}	
		} else {
			

			i_ReturnValue = -4;
		}
	} else {
		

		i_ReturnValue = -2;
	}

	return i_ReturnValue;
}


int i_APCI1710_WriteTimerValue(struct comedi_device *dev,
			       unsigned char b_ModulNbr, unsigned char b_TimerNbr,
			       unsigned int ul_WriteValue)
{
	int i_ReturnValue = 0;

	
	if (b_ModulNbr < 4) {
		
		if ((devpriv->s_BoardInfos.dw_MolduleConfiguration[b_ModulNbr] & 0xFFFF0000UL) == APCI1710_82X54_TIMER) {
			
			if (b_TimerNbr <= 2) {
				
				if (devpriv->s_ModuleInfo[b_ModulNbr].s_82X54ModuleInfo.s_82X54TimerInfo[b_TimerNbr].b_82X54Init == 1) {
					
					outl(ul_WriteValue, devpriv->s_BoardInfos.ui_Address + (b_TimerNbr * 4) + (64 * b_ModulNbr));
				} else {
					
					DPRINTK("Timer not initialised see function\n");
					i_ReturnValue = -5;
				}
			} else {
				
				DPRINTK("Timer selection wrong\n");
				i_ReturnValue = -3;
			}	
		} else {
			
			DPRINTK("The module is not a TIMER module\n");
			i_ReturnValue = -4;
		}
	} else {
		
		DPRINTK("Module number error\n");
		i_ReturnValue = -2;
	}

	return i_ReturnValue;
}
