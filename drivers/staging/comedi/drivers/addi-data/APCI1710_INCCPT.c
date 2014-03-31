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


#include "APCI1710_INCCPT.h"


int i_APCI1710_InsnConfigINCCPT(struct comedi_device *dev, struct comedi_subdevice *s,
	struct comedi_insn *insn, unsigned int *data)
{
	unsigned int ui_ConfigType;
	int i_ReturnValue = 0;
	ui_ConfigType = CR_CHAN(insn->chanspec);

	printk("\nINC_CPT");

	devpriv->tsk_Current = current;	
	switch (ui_ConfigType) {
	case APCI1710_INCCPT_INITCOUNTER:
		i_ReturnValue = i_APCI1710_InitCounter(dev,
			CR_AREF(insn->chanspec),
			(unsigned char) data[0],
			(unsigned char) data[1],
			(unsigned char) data[2], (unsigned char) data[3], (unsigned char) data[4]);
		break;

	case APCI1710_INCCPT_COUNTERAUTOTEST:
		i_ReturnValue = i_APCI1710_CounterAutoTest(dev,
			(unsigned char *) &data[0]);
		break;

	case APCI1710_INCCPT_INITINDEX:
		i_ReturnValue = i_APCI1710_InitIndex(dev,
			CR_AREF(insn->chanspec),
			(unsigned char) data[0],
			(unsigned char) data[1], (unsigned char) data[2], (unsigned char) data[3]);
		break;

	case APCI1710_INCCPT_INITREFERENCE:
		i_ReturnValue = i_APCI1710_InitReference(dev,
			CR_AREF(insn->chanspec), (unsigned char) data[0]);
		break;

	case APCI1710_INCCPT_INITEXTERNALSTROBE:
		i_ReturnValue = i_APCI1710_InitExternalStrobe(dev,
			CR_AREF(insn->chanspec),
			(unsigned char) data[0], (unsigned char) data[1]);
		break;

	case APCI1710_INCCPT_INITCOMPARELOGIC:
		i_ReturnValue = i_APCI1710_InitCompareLogic(dev,
			CR_AREF(insn->chanspec), (unsigned int) data[0]);
		break;

	case APCI1710_INCCPT_INITFREQUENCYMEASUREMENT:
		i_ReturnValue = i_APCI1710_InitFrequencyMeasurement(dev,
			CR_AREF(insn->chanspec),
			(unsigned char) data[0],
			(unsigned char) data[1], (unsigned int) data[2], (unsigned int *) &data[0]);
		break;

	default:
		printk("Insn Config : Config Parameter Wrong\n");

	}

	if (i_ReturnValue >= 0)
		i_ReturnValue = insn->n;
	return i_ReturnValue;
}


int i_APCI1710_InitCounter(struct comedi_device *dev,
	unsigned char b_ModulNbr,
	unsigned char b_CounterRange,
	unsigned char b_FirstCounterModus,
	unsigned char b_FirstCounterOption,
	unsigned char b_SecondCounterModus, unsigned char b_SecondCounterOption)
{
	int i_ReturnValue = 0;

	
	
	

	if ((devpriv->s_BoardInfos.
			dw_MolduleConfiguration[b_ModulNbr] & 0xFFFF0000UL) ==
		APCI1710_INCREMENTAL_COUNTER) {
	   
		
	   

		if (b_CounterRange == APCI1710_16BIT_COUNTER
			|| b_CounterRange == APCI1710_32BIT_COUNTER) {
	      
			
	      

			if (b_FirstCounterModus == APCI1710_QUADRUPLE_MODE ||
				b_FirstCounterModus == APCI1710_DOUBLE_MODE ||
				b_FirstCounterModus == APCI1710_SIMPLE_MODE ||
				b_FirstCounterModus == APCI1710_DIRECT_MODE) {
		 
				
		 

				if ((b_FirstCounterModus == APCI1710_DIRECT_MODE
						&& (b_FirstCounterOption ==
							APCI1710_INCREMENT
							|| b_FirstCounterOption
							== APCI1710_DECREMENT))
					|| (b_FirstCounterModus !=
						APCI1710_DIRECT_MODE
						&& (b_FirstCounterOption ==
							APCI1710_HYSTERESIS_ON
							|| b_FirstCounterOption
							==
							APCI1710_HYSTERESIS_OFF)))
				{
		    
					
		    

					if (b_CounterRange ==
						APCI1710_16BIT_COUNTER) {
		       
						
		       

						if ((b_FirstCounterModus !=
								APCI1710_DIRECT_MODE
								&&
								(b_SecondCounterModus
									==
									APCI1710_QUADRUPLE_MODE
									||
									b_SecondCounterModus
									==
									APCI1710_DOUBLE_MODE
									||
									b_SecondCounterModus
									==
									APCI1710_SIMPLE_MODE))
							|| (b_FirstCounterModus
								==
								APCI1710_DIRECT_MODE
								&&
								b_SecondCounterModus
								==
								APCI1710_DIRECT_MODE))
						{
			  
							
			  

							if ((b_SecondCounterModus == APCI1710_DIRECT_MODE && (b_SecondCounterOption == APCI1710_INCREMENT || b_SecondCounterOption == APCI1710_DECREMENT)) || (b_SecondCounterModus != APCI1710_DIRECT_MODE && (b_SecondCounterOption == APCI1710_HYSTERESIS_ON || b_SecondCounterOption == APCI1710_HYSTERESIS_OFF))) {
								i_ReturnValue =
									0;
							} else {
			     
								
			     

								DPRINTK("The selected second counter operating option is wrong\n");
								i_ReturnValue =
									-7;
							}
						} else {
			  
							
			  

							DPRINTK("The selected second counter operating mode is wrong\n");
							i_ReturnValue = -6;
						}
					}
				} else {
		    
					
		    

					DPRINTK("The selected first counter operating option is wrong\n");
					i_ReturnValue = -5;
				}
			} else {
		 
				
		 
				DPRINTK("The selected first counter operating mode is wrong\n");
				i_ReturnValue = -4;
			}
		} else {
	      
			
	      

			DPRINTK("The selected counter range is wrong\n");
			i_ReturnValue = -3;
		}

	   
		
	   

		if (i_ReturnValue == 0) {
	      
			
	      

			if (b_CounterRange == APCI1710_32BIT_COUNTER) {
				devpriv->
					s_ModuleInfo[b_ModulNbr].
					s_SiemensCounterInfo.
					s_ModeRegister.
					s_ByteModeRegister.
					b_ModeRegister1 = b_CounterRange |
					b_FirstCounterModus |
					b_FirstCounterOption;
			} else {
				devpriv->
					s_ModuleInfo[b_ModulNbr].
					s_SiemensCounterInfo.
					s_ModeRegister.
					s_ByteModeRegister.
					b_ModeRegister1 = b_CounterRange |
					(b_FirstCounterModus & 0x5) |
					(b_FirstCounterOption & 0x20) |
					(b_SecondCounterModus & 0xA) |
					(b_SecondCounterOption & 0x40);

		 
				
		 

				if (b_FirstCounterModus == APCI1710_DIRECT_MODE) {
					devpriv->
						s_ModuleInfo[b_ModulNbr].
						s_SiemensCounterInfo.
						s_ModeRegister.
						s_ByteModeRegister.
						b_ModeRegister1 = devpriv->
						s_ModuleInfo[b_ModulNbr].
						s_SiemensCounterInfo.
						s_ModeRegister.
						s_ByteModeRegister.
						b_ModeRegister1 |
						APCI1710_DIRECT_MODE;
				}
			}

	      
			
	      

			outl(devpriv->s_ModuleInfo[b_ModulNbr].
				s_SiemensCounterInfo.
				s_ModeRegister.
				dw_ModeRegister1_2_3_4,
				devpriv->s_BoardInfos.
				ui_Address + 20 + (64 * b_ModulNbr));

			devpriv->
				s_ModuleInfo[b_ModulNbr].
				s_SiemensCounterInfo.
				s_InitFlag.b_CounterInit = 1;
		}
	} else {
	   
		
	   

		DPRINTK("The module is not a counter module\n");
		i_ReturnValue = -2;
	}

	return i_ReturnValue;
}


int i_APCI1710_CounterAutoTest(struct comedi_device *dev, unsigned char *pb_TestStatus)
{
	unsigned char b_ModulCpt = 0;
	int i_ReturnValue = 0;
	unsigned int dw_LathchValue;

	*pb_TestStatus = 0;

	
	
	

	if ((devpriv->s_BoardInfos.
			dw_MolduleConfiguration[0] & 0xFFFF0000UL) ==
		APCI1710_INCREMENTAL_COUNTER
		|| (devpriv->s_BoardInfos.
			dw_MolduleConfiguration[1] & 0xFFFF0000UL) ==
		APCI1710_INCREMENTAL_COUNTER
		|| (devpriv->s_BoardInfos.
			dw_MolduleConfiguration[2] & 0xFFFF0000UL) ==
		APCI1710_INCREMENTAL_COUNTER
		|| (devpriv->s_BoardInfos.
			dw_MolduleConfiguration[3] & 0xFFFF0000UL) ==
		APCI1710_INCREMENTAL_COUNTER) {
		for (b_ModulCpt = 0; b_ModulCpt < 4; b_ModulCpt++) {
	      
			
	      

			if ((devpriv->s_BoardInfos.
					dw_MolduleConfiguration[b_ModulCpt] &
					0xFFFF0000UL) ==
				APCI1710_INCREMENTAL_COUNTER) {
		 
				
		 

				outl(3, devpriv->s_BoardInfos.
					ui_Address + 16 + (64 * b_ModulCpt));

		 
				
		 

				outl(1, devpriv->s_BoardInfos.
					ui_Address + (64 * b_ModulCpt));

		 
				
		 

				dw_LathchValue = inl(devpriv->s_BoardInfos.
					ui_Address + 4 + (64 * b_ModulCpt));

				if ((dw_LathchValue & 0xFF) !=
					((dw_LathchValue >> 8) & 0xFF)
					&& (dw_LathchValue & 0xFF) !=
					((dw_LathchValue >> 16) & 0xFF)
					&& (dw_LathchValue & 0xFF) !=
					((dw_LathchValue >> 24) & 0xFF)) {
					*pb_TestStatus =
						*pb_TestStatus | (1 <<
						b_ModulCpt);
				}

		 
				
		 

				outl(0, devpriv->s_BoardInfos.
					ui_Address + 16 + (64 * b_ModulCpt));
			}
		}
	} else {
	   
		
	   

		DPRINTK("No counter module found\n");
		i_ReturnValue = -2;
	}

	return i_ReturnValue;
}


int i_APCI1710_InitIndex(struct comedi_device *dev,
	unsigned char b_ModulNbr,
	unsigned char b_ReferenceAction,
	unsigned char b_IndexOperation, unsigned char b_AutoMode, unsigned char b_InterruptEnable)
{
	int i_ReturnValue = 0;

	
	
	

	if (b_ModulNbr < 4) {
	   
		
	   

		if (devpriv->
			s_ModuleInfo[b_ModulNbr].
			s_SiemensCounterInfo.s_InitFlag.b_CounterInit == 1) {
	      
			
	      

			if (b_ReferenceAction == APCI1710_ENABLE ||
				b_ReferenceAction == APCI1710_DISABLE) {
		 
				
		 

				if (b_IndexOperation ==
					APCI1710_HIGH_EDGE_LATCH_COUNTER
					|| b_IndexOperation ==
					APCI1710_LOW_EDGE_LATCH_COUNTER
					|| b_IndexOperation ==
					APCI1710_HIGH_EDGE_CLEAR_COUNTER
					|| b_IndexOperation ==
					APCI1710_LOW_EDGE_CLEAR_COUNTER
					|| b_IndexOperation ==
					APCI1710_HIGH_EDGE_LATCH_AND_CLEAR_COUNTER
					|| b_IndexOperation ==
					APCI1710_LOW_EDGE_LATCH_AND_CLEAR_COUNTER)
				{
		    
					
		    

					if (b_AutoMode == APCI1710_ENABLE ||
						b_AutoMode == APCI1710_DISABLE)
					{
		       
						
		       

						if (b_InterruptEnable ==
							APCI1710_ENABLE
							|| b_InterruptEnable ==
							APCI1710_DISABLE) {

			     
							
			     

							if (b_ReferenceAction ==
								APCI1710_ENABLE)
							{
								devpriv->
									s_ModuleInfo
									[b_ModulNbr].
									s_SiemensCounterInfo.
									s_ModeRegister.
									s_ByteModeRegister.
									b_ModeRegister2
									=
									devpriv->
									s_ModuleInfo
									[b_ModulNbr].
									s_SiemensCounterInfo.
									s_ModeRegister.
									s_ByteModeRegister.
									b_ModeRegister2
									|
									APCI1710_ENABLE_INDEX_ACTION;
							} else {
								devpriv->
									s_ModuleInfo
									[b_ModulNbr].
									s_SiemensCounterInfo.
									s_ModeRegister.
									s_ByteModeRegister.
									b_ModeRegister2
									=
									devpriv->
									s_ModuleInfo
									[b_ModulNbr].
									s_SiemensCounterInfo.
									s_ModeRegister.
									s_ByteModeRegister.
									b_ModeRegister2
									&
									APCI1710_DISABLE_INDEX_ACTION;
							}

			     
							
			     

							if (b_IndexOperation ==
								APCI1710_LOW_EDGE_LATCH_COUNTER
								||
								b_IndexOperation
								==
								APCI1710_LOW_EDGE_CLEAR_COUNTER
								||
								b_IndexOperation
								==
								APCI1710_LOW_EDGE_LATCH_AND_CLEAR_COUNTER)
							{
				
								
				

								devpriv->
									s_ModuleInfo
									[b_ModulNbr].
									s_SiemensCounterInfo.
									s_ModeRegister.
									s_ByteModeRegister.
									b_ModeRegister4
									=
									devpriv->
									s_ModuleInfo
									[b_ModulNbr].
									s_SiemensCounterInfo.
									s_ModeRegister.
									s_ByteModeRegister.
									b_ModeRegister4
									|
									APCI1710_SET_LOW_INDEX_LEVEL;
							} else {
				
								
				

								devpriv->
									s_ModuleInfo
									[b_ModulNbr].
									s_SiemensCounterInfo.
									s_ModeRegister.
									s_ByteModeRegister.
									b_ModeRegister4
									=
									devpriv->
									s_ModuleInfo
									[b_ModulNbr].
									s_SiemensCounterInfo.
									s_ModeRegister.
									s_ByteModeRegister.
									b_ModeRegister4
									&
									APCI1710_SET_HIGH_INDEX_LEVEL;
							}

			     
							
			     

							if (b_IndexOperation ==
								APCI1710_HIGH_EDGE_LATCH_AND_CLEAR_COUNTER
								||
								b_IndexOperation
								==
								APCI1710_LOW_EDGE_LATCH_AND_CLEAR_COUNTER)
							{
				
								
				

								devpriv->
									s_ModuleInfo
									[b_ModulNbr].
									s_SiemensCounterInfo.
									s_ModeRegister.
									s_ByteModeRegister.
									b_ModeRegister4
									=
									devpriv->
									s_ModuleInfo
									[b_ModulNbr].
									s_SiemensCounterInfo.
									s_ModeRegister.
									s_ByteModeRegister.
									b_ModeRegister4
									|
									APCI1710_ENABLE_LATCH_AND_CLEAR;
							}	
							else {
				
								
				

								devpriv->
									s_ModuleInfo
									[b_ModulNbr].
									s_SiemensCounterInfo.
									s_ModeRegister.
									s_ByteModeRegister.
									b_ModeRegister4
									=
									devpriv->
									s_ModuleInfo
									[b_ModulNbr].
									s_SiemensCounterInfo.
									s_ModeRegister.
									s_ByteModeRegister.
									b_ModeRegister4
									&
									APCI1710_DISABLE_LATCH_AND_CLEAR;

				
								
				

								if (b_IndexOperation == APCI1710_HIGH_EDGE_LATCH_COUNTER || b_IndexOperation == APCI1710_LOW_EDGE_LATCH_COUNTER) {
				   
									
				   

									devpriv->
										s_ModuleInfo
										[b_ModulNbr].
										s_SiemensCounterInfo.
										s_ModeRegister.
										s_ByteModeRegister.
										b_ModeRegister2
										=
										devpriv->
										s_ModuleInfo
										[b_ModulNbr].
										s_SiemensCounterInfo.
										s_ModeRegister.
										s_ByteModeRegister.
										b_ModeRegister2
										|
										APCI1710_INDEX_LATCH_COUNTER;
								} else {
				   
									
				   

									devpriv->
										s_ModuleInfo
										[b_ModulNbr].
										s_SiemensCounterInfo.
										s_ModeRegister.
										s_ByteModeRegister.
										b_ModeRegister2
										=
										devpriv->
										s_ModuleInfo
										[b_ModulNbr].
										s_SiemensCounterInfo.
										s_ModeRegister.
										s_ByteModeRegister.
										b_ModeRegister2
										&
										(~APCI1710_INDEX_LATCH_COUNTER);
								}
							}	

							if (b_AutoMode ==
								APCI1710_DISABLE)
							{
								devpriv->
									s_ModuleInfo
									[b_ModulNbr].
									s_SiemensCounterInfo.
									s_ModeRegister.
									s_ByteModeRegister.
									b_ModeRegister2
									=
									devpriv->
									s_ModuleInfo
									[b_ModulNbr].
									s_SiemensCounterInfo.
									s_ModeRegister.
									s_ByteModeRegister.
									b_ModeRegister2
									|
									APCI1710_INDEX_AUTO_MODE;
							} else {
								devpriv->
									s_ModuleInfo
									[b_ModulNbr].
									s_SiemensCounterInfo.
									s_ModeRegister.
									s_ByteModeRegister.
									b_ModeRegister2
									=
									devpriv->
									s_ModuleInfo
									[b_ModulNbr].
									s_SiemensCounterInfo.
									s_ModeRegister.
									s_ByteModeRegister.
									b_ModeRegister2
									&
									(~APCI1710_INDEX_AUTO_MODE);
							}

							if (b_InterruptEnable ==
								APCI1710_ENABLE)
							{
								devpriv->
									s_ModuleInfo
									[b_ModulNbr].
									s_SiemensCounterInfo.
									s_ModeRegister.
									s_ByteModeRegister.
									b_ModeRegister3
									=
									devpriv->
									s_ModuleInfo
									[b_ModulNbr].
									s_SiemensCounterInfo.
									s_ModeRegister.
									s_ByteModeRegister.
									b_ModeRegister3
									|
									APCI1710_ENABLE_INDEX_INT;
							} else {
								devpriv->
									s_ModuleInfo
									[b_ModulNbr].
									s_SiemensCounterInfo.
									s_ModeRegister.
									s_ByteModeRegister.
									b_ModeRegister3
									=
									devpriv->
									s_ModuleInfo
									[b_ModulNbr].
									s_SiemensCounterInfo.
									s_ModeRegister.
									s_ByteModeRegister.
									b_ModeRegister3
									&
									APCI1710_DISABLE_INDEX_INT;
							}

							devpriv->
								s_ModuleInfo
								[b_ModulNbr].
								s_SiemensCounterInfo.
								s_InitFlag.
								b_IndexInit = 1;

						} else {
			  
							
			  
							DPRINTK("Interrupt parameter is wrong\n");
							i_ReturnValue = -7;
						}
					} else {
		       
						
		       

						DPRINTK("The auto mode parameter is wrong\n");
						i_ReturnValue = -6;
					}
				} else {
		    
					
		    

					DPRINTK("The index operating mode parameter is wrong\n");
					i_ReturnValue = -5;
				}
			} else {
		 
				
		 

				DPRINTK("The reference action parameter is wrong\n");
				i_ReturnValue = -4;
			}
		} else {
	      
			
			
	      

			DPRINTK("Counter not initialised\n");
			i_ReturnValue = -3;
		}
	} else {
	   
		
	   

		DPRINTK("The selected module number parameter is wrong\n");
		i_ReturnValue = -2;
	}

	return i_ReturnValue;
}


int i_APCI1710_InitReference(struct comedi_device *dev,
	unsigned char b_ModulNbr, unsigned char b_ReferenceLevel)
{
	int i_ReturnValue = 0;

	
	
	

	if (b_ModulNbr < 4) {
	   
		
	   

		if (devpriv->
			s_ModuleInfo[b_ModulNbr].
			s_SiemensCounterInfo.s_InitFlag.b_CounterInit == 1) {
	      
			
	      

			if (b_ReferenceLevel == 0 || b_ReferenceLevel == 1) {
				if (b_ReferenceLevel == 1) {
					devpriv->
						s_ModuleInfo[b_ModulNbr].
						s_SiemensCounterInfo.
						s_ModeRegister.
						s_ByteModeRegister.
						b_ModeRegister2 = devpriv->
						s_ModuleInfo[b_ModulNbr].
						s_SiemensCounterInfo.
						s_ModeRegister.
						s_ByteModeRegister.
						b_ModeRegister2 |
						APCI1710_REFERENCE_HIGH;
				} else {
					devpriv->
						s_ModuleInfo[b_ModulNbr].
						s_SiemensCounterInfo.
						s_ModeRegister.
						s_ByteModeRegister.
						b_ModeRegister2 = devpriv->
						s_ModuleInfo[b_ModulNbr].
						s_SiemensCounterInfo.
						s_ModeRegister.
						s_ByteModeRegister.
						b_ModeRegister2 &
						APCI1710_REFERENCE_LOW;
				}

				outl(devpriv->s_ModuleInfo[b_ModulNbr].
					s_SiemensCounterInfo.
					s_ModeRegister.
					dw_ModeRegister1_2_3_4,
					devpriv->s_BoardInfos.ui_Address + 20 +
					(64 * b_ModulNbr));

				devpriv->
					s_ModuleInfo[b_ModulNbr].
					s_SiemensCounterInfo.
					s_InitFlag.b_ReferenceInit = 1;
			} else {
		 
				
		 

				DPRINTK("Reference level parameter is wrong\n");
				i_ReturnValue = -4;
			}
		} else {
	      
			
			
	      

			DPRINTK("Counter not initialised\n");
			i_ReturnValue = -3;
		}
	} else {
	   
		
	   

		DPRINTK("The selected module number parameter is wrong\n");
		i_ReturnValue = -2;
	}

	return i_ReturnValue;
}


int i_APCI1710_InitExternalStrobe(struct comedi_device *dev,
	unsigned char b_ModulNbr, unsigned char b_ExternalStrobe, unsigned char b_ExternalStrobeLevel)
{
	int i_ReturnValue = 0;

	
	
	

	if (b_ModulNbr < 4) {
	   
		
	   

		if (devpriv->
			s_ModuleInfo[b_ModulNbr].
			s_SiemensCounterInfo.s_InitFlag.b_CounterInit == 1) {
	      
			
	      

			if (b_ExternalStrobe == 0 || b_ExternalStrobe == 1) {
		 
				
		 

				if ((b_ExternalStrobeLevel == APCI1710_HIGH) ||
					((b_ExternalStrobeLevel == APCI1710_LOW
							&& (devpriv->
								s_BoardInfos.
								dw_MolduleConfiguration
								[b_ModulNbr] &
								0xFFFF) >=
							0x3135))) {
		    
					
		    

					devpriv->
						s_ModuleInfo[b_ModulNbr].
						s_SiemensCounterInfo.
						s_ModeRegister.
						s_ByteModeRegister.
						b_ModeRegister4 = (devpriv->
						s_ModuleInfo[b_ModulNbr].
						s_SiemensCounterInfo.
						s_ModeRegister.
						s_ByteModeRegister.
						b_ModeRegister4 & (0xFF -
							(0x10 << b_ExternalStrobe))) | ((b_ExternalStrobeLevel ^ 1) << (4 + b_ExternalStrobe));
				} else {
		    
					
		    

					DPRINTK("External strobe level parameter is wrong\n");
					i_ReturnValue = -5;
				}
			}	
			else {
		 
				
		 

				DPRINTK("External strobe selection is wrong\n");
				i_ReturnValue = -4;
			}	
		} else {
	      
			
			
	      

			DPRINTK("Counter not initialised\n");
			i_ReturnValue = -3;
		}
	} else {
	   
		
	   

		DPRINTK("The selected module number parameter is wrong\n");
		i_ReturnValue = -2;
	}

	return i_ReturnValue;
}


int i_APCI1710_InitCompareLogic(struct comedi_device *dev,
	unsigned char b_ModulNbr, unsigned int ui_CompareValue)
{
	int i_ReturnValue = 0;

	
	
	

	if (b_ModulNbr < 4) {
	   
		
	   

		if (devpriv->
			s_ModuleInfo[b_ModulNbr].
			s_SiemensCounterInfo.s_InitFlag.b_CounterInit == 1) {

			outl(ui_CompareValue, devpriv->s_BoardInfos.
				ui_Address + 28 + (64 * b_ModulNbr));

			devpriv->
				s_ModuleInfo[b_ModulNbr].
				s_SiemensCounterInfo.
				s_InitFlag.b_CompareLogicInit = 1;
		} else {
	      
			
			
	      

			DPRINTK("Counter not initialised\n");
			i_ReturnValue = -3;
		}
	} else {
	   
		
	   

		DPRINTK("The selected module number parameter is wrong\n");
		i_ReturnValue = -2;
	}

	return i_ReturnValue;
}


int i_APCI1710_InitFrequencyMeasurement(struct comedi_device *dev,
	unsigned char b_ModulNbr,
	unsigned char b_PCIInputClock,
	unsigned char b_TimingUnity,
	unsigned int ul_TimingInterval, unsigned int *pul_RealTimingInterval)
{
	int i_ReturnValue = 0;
	unsigned int ul_TimerValue = 0;
	double d_RealTimingInterval;
	unsigned int dw_Status = 0;

	
	
	

	if (b_ModulNbr < 4) {
	   
		
	   

		if (devpriv->
			s_ModuleInfo[b_ModulNbr].
			s_SiemensCounterInfo.s_InitFlag.b_CounterInit == 1) {
	      
			
	      

			if ((b_PCIInputClock == APCI1710_30MHZ) ||
				(b_PCIInputClock == APCI1710_33MHZ) ||
				(b_PCIInputClock == APCI1710_40MHZ)) {
		 
				
		 

				if (b_TimingUnity <= 2) {
		    
					
		    

					if (((b_PCIInputClock == APCI1710_30MHZ)
							&& (b_TimingUnity == 0)
							&& (ul_TimingInterval >=
								266)
							&& (ul_TimingInterval <=
								8738133UL))
						|| ((b_PCIInputClock ==
								APCI1710_30MHZ)
							&& (b_TimingUnity == 1)
							&& (ul_TimingInterval >=
								1)
							&& (ul_TimingInterval <=
								8738UL))
						|| ((b_PCIInputClock ==
								APCI1710_30MHZ)
							&& (b_TimingUnity == 2)
							&& (ul_TimingInterval >=
								1)
							&& (ul_TimingInterval <=
								8UL))
						|| ((b_PCIInputClock ==
								APCI1710_33MHZ)
							&& (b_TimingUnity == 0)
							&& (ul_TimingInterval >=
								242)
							&& (ul_TimingInterval <=
								7943757UL))
						|| ((b_PCIInputClock ==
								APCI1710_33MHZ)
							&& (b_TimingUnity == 1)
							&& (ul_TimingInterval >=
								1)
							&& (ul_TimingInterval <=
								7943UL))
						|| ((b_PCIInputClock ==
								APCI1710_33MHZ)
							&& (b_TimingUnity == 2)
							&& (ul_TimingInterval >=
								1)
							&& (ul_TimingInterval <=
								7UL))
						|| ((b_PCIInputClock ==
								APCI1710_40MHZ)
							&& (b_TimingUnity == 0)
							&& (ul_TimingInterval >=
								200)
							&& (ul_TimingInterval <=
								6553500UL))
						|| ((b_PCIInputClock ==
								APCI1710_40MHZ)
							&& (b_TimingUnity == 1)
							&& (ul_TimingInterval >=
								1)
							&& (ul_TimingInterval <=
								6553UL))
						|| ((b_PCIInputClock ==
								APCI1710_40MHZ)
							&& (b_TimingUnity == 2)
							&& (ul_TimingInterval >=
								1)
							&& (ul_TimingInterval <=
								6UL))) {
		       
						
		       

						if (b_PCIInputClock ==
							APCI1710_40MHZ) {
			  
							
			  

							if ((devpriv->s_BoardInfos.dw_MolduleConfiguration[b_ModulNbr] & 0xFFFF) >= 0x3135) {
			     
								
			     

								dw_Status =
									inl
									(devpriv->
									s_BoardInfos.
									ui_Address
									+ 36 +
									(64 * b_ModulNbr));

			     
								
			     

								if ((dw_Status & 1) != 1) {
				
									
				

									DPRINTK("40MHz quartz not on board\n");
									i_ReturnValue
										=
										-7;
								}
							} else {
			     
								
			     
								DPRINTK("40MHz quartz not on board\n");
								i_ReturnValue =
									-7;
							}
						}	

		       
						
		       

						if (i_ReturnValue == 0) {
			  
							
			  

							if ((devpriv->s_BoardInfos.dw_MolduleConfiguration[b_ModulNbr] & 0xFFFF) >= 0x3131) {

				
								
				

								if (b_PCIInputClock == APCI1710_40MHZ) {
				   
									
				   

									devpriv->
										s_ModuleInfo
										[b_ModulNbr].
										s_SiemensCounterInfo.
										s_ModeRegister.
										s_ByteModeRegister.
										b_ModeRegister4
										=
										devpriv->
										s_ModuleInfo
										[b_ModulNbr].
										s_SiemensCounterInfo.
										s_ModeRegister.
										s_ByteModeRegister.
										b_ModeRegister4
										|
										APCI1710_ENABLE_40MHZ_FREQUENCY;
								}	
								else {
				   
									
				   

									devpriv->
										s_ModuleInfo
										[b_ModulNbr].
										s_SiemensCounterInfo.
										s_ModeRegister.
										s_ByteModeRegister.
										b_ModeRegister4
										=
										devpriv->
										s_ModuleInfo
										[b_ModulNbr].
										s_SiemensCounterInfo.
										s_ModeRegister.
										s_ByteModeRegister.
										b_ModeRegister4
										&
										APCI1710_DISABLE_40MHZ_FREQUENCY;

								}	

			     
								
			     

								fpu_begin();
								switch (b_TimingUnity) {
				
									
				

								case 0:

					
									
					

									ul_TimerValue
										=
										(unsigned int)
										(ul_TimingInterval
										*
										(0.00025 * b_PCIInputClock));

					
									
					

									if ((double)((double)ul_TimingInterval * (0.00025 * (double)b_PCIInputClock)) >= ((double)((double)ul_TimerValue + 0.5))) {
										ul_TimerValue
											=
											ul_TimerValue
											+
											1;
									}

					
									
					

									*pul_RealTimingInterval
										=
										(unsigned int)
										(ul_TimerValue
										/
										(0.00025 * (double)b_PCIInputClock));
									d_RealTimingInterval
										=
										(double)
										ul_TimerValue
										/
										(0.00025
										*
										(double)
										b_PCIInputClock);

									if ((double)((double)ul_TimerValue / (0.00025 * (double)b_PCIInputClock)) >= (double)((double)*pul_RealTimingInterval + 0.5)) {
										*pul_RealTimingInterval
											=
											*pul_RealTimingInterval
											+
											1;
									}

									ul_TimingInterval
										=
										ul_TimingInterval
										-
										1;
									ul_TimerValue
										=
										ul_TimerValue
										-
										2;

									break;

				
									
				

								case 1:

					
									
					

									ul_TimerValue
										=
										(unsigned int)
										(ul_TimingInterval
										*
										(0.25 * b_PCIInputClock));

					
									
					

									if ((double)((double)ul_TimingInterval * (0.25 * (double)b_PCIInputClock)) >= ((double)((double)ul_TimerValue + 0.5))) {
										ul_TimerValue
											=
											ul_TimerValue
											+
											1;
									}

					
									
					

									*pul_RealTimingInterval
										=
										(unsigned int)
										(ul_TimerValue
										/
										(0.25 * (double)b_PCIInputClock));
									d_RealTimingInterval
										=
										(double)
										ul_TimerValue
										/
										(
										(double)
										0.25
										*
										(double)
										b_PCIInputClock);

									if ((double)((double)ul_TimerValue / (0.25 * (double)b_PCIInputClock)) >= (double)((double)*pul_RealTimingInterval + 0.5)) {
										*pul_RealTimingInterval
											=
											*pul_RealTimingInterval
											+
											1;
									}

									ul_TimingInterval
										=
										ul_TimingInterval
										-
										1;
									ul_TimerValue
										=
										ul_TimerValue
										-
										2;

									break;

				
									
				

								case 2:

					
									
					

									ul_TimerValue
										=
										ul_TimingInterval
										*
										(250.0
										*
										b_PCIInputClock);

					
									
					

									if ((double)((double)ul_TimingInterval * (250.0 * (double)b_PCIInputClock)) >= ((double)((double)ul_TimerValue + 0.5))) {
										ul_TimerValue
											=
											ul_TimerValue
											+
											1;
									}

					
									
					

									*pul_RealTimingInterval
										=
										(unsigned int)
										(ul_TimerValue
										/
										(250.0 * (double)b_PCIInputClock));
									d_RealTimingInterval
										=
										(double)
										ul_TimerValue
										/
										(250.0
										*
										(double)
										b_PCIInputClock);

									if ((double)((double)ul_TimerValue / (250.0 * (double)b_PCIInputClock)) >= (double)((double)*pul_RealTimingInterval + 0.5)) {
										*pul_RealTimingInterval
											=
											*pul_RealTimingInterval
											+
											1;
									}

									ul_TimingInterval
										=
										ul_TimingInterval
										-
										1;
									ul_TimerValue
										=
										ul_TimerValue
										-
										2;

									break;
								}

								fpu_end();
			     
								
			     

								outl(ul_TimerValue, devpriv->s_BoardInfos.ui_Address + 32 + (64 * b_ModulNbr));

			     
								
			     

								devpriv->
									s_ModuleInfo
									[b_ModulNbr].
									s_SiemensCounterInfo.
									s_InitFlag.
									b_FrequencyMeasurementInit
									= 1;
							} else {
			     
								
			     

								DPRINTK("Counter not initialised\n");
								i_ReturnValue =
									-3;
							}
						}	
					} else {
		       
						
		       

						DPRINTK("Base timing selection is wrong\n");
						i_ReturnValue = -6;
					}
				} else {
		    
					
		    

					DPRINTK("Timing unity selection is wrong\n");
					i_ReturnValue = -5;
				}
			} else {
		 
				
		 

				DPRINTK("The selected PCI input clock is wrong\n");
				i_ReturnValue = -4;
			}
		} else {
	      
			
			
	      

			DPRINTK("Counter not initialised\n");
			i_ReturnValue = -3;
		}
	} else {
	   
		
	   

		DPRINTK("The selected module number parameter is wrong\n");
		i_ReturnValue = -2;
	}

	return i_ReturnValue;
}


							


int i_APCI1710_InsnBitsINCCPT(struct comedi_device *dev, struct comedi_subdevice *s,
	struct comedi_insn *insn, unsigned int *data)
{
	unsigned int ui_BitsType;
	int i_ReturnValue = 0;
	ui_BitsType = CR_CHAN(insn->chanspec);
	devpriv->tsk_Current = current;	

	switch (ui_BitsType) {
	case APCI1710_INCCPT_CLEARCOUNTERVALUE:
		i_ReturnValue = i_APCI1710_ClearCounterValue(dev,
			(unsigned char) CR_AREF(insn->chanspec));
		break;

	case APCI1710_INCCPT_CLEARALLCOUNTERVALUE:
		i_ReturnValue = i_APCI1710_ClearAllCounterValue(dev);
		break;

	case APCI1710_INCCPT_SETINPUTFILTER:
		i_ReturnValue = i_APCI1710_SetInputFilter(dev,
			(unsigned char) CR_AREF(insn->chanspec),
			(unsigned char) data[0], (unsigned char) data[1]);
		break;

	case APCI1710_INCCPT_LATCHCOUNTER:
		i_ReturnValue = i_APCI1710_LatchCounter(dev,
			(unsigned char) CR_AREF(insn->chanspec), (unsigned char) data[0]);
		break;

	case APCI1710_INCCPT_SETINDEXANDREFERENCESOURCE:
		i_ReturnValue = i_APCI1710_SetIndexAndReferenceSource(dev,
			(unsigned char) CR_AREF(insn->chanspec), (unsigned char) data[0]);
		break;

	case APCI1710_INCCPT_SETDIGITALCHLON:
		i_ReturnValue = i_APCI1710_SetDigitalChlOn(dev,
			(unsigned char) CR_AREF(insn->chanspec));
		break;

	case APCI1710_INCCPT_SETDIGITALCHLOFF:
		i_ReturnValue = i_APCI1710_SetDigitalChlOff(dev,
			(unsigned char) CR_AREF(insn->chanspec));
		break;

	default:
		printk("Bits Config Parameter Wrong\n");
	}

	if (i_ReturnValue >= 0)
		i_ReturnValue = insn->n;
	return i_ReturnValue;
}


int i_APCI1710_ClearCounterValue(struct comedi_device *dev, unsigned char b_ModulNbr)
{
	int i_ReturnValue = 0;

	
	
	

	if (b_ModulNbr < 4) {
	   
		
	   

		if (devpriv->
			s_ModuleInfo[b_ModulNbr].
			s_SiemensCounterInfo.s_InitFlag.b_CounterInit == 1) {
	      
			
	      

			outl(1, devpriv->s_BoardInfos.
				ui_Address + 16 + (64 * b_ModulNbr));
		} else {
	      
			
			
	      

			DPRINTK("Counter not initialised\n");
			i_ReturnValue = -3;
		}
	} else {
	   
		
	   

		DPRINTK("The selected module number parameter is wrong\n");
		i_ReturnValue = -2;
	}

	return i_ReturnValue;
}


int i_APCI1710_ClearAllCounterValue(struct comedi_device *dev)
{
	unsigned char b_ModulCpt = 0;
	int i_ReturnValue = 0;

	
	
	

	if ((devpriv->s_BoardInfos.
			dw_MolduleConfiguration[0] & 0xFFFF0000UL) ==
		APCI1710_INCREMENTAL_COUNTER
		|| (devpriv->s_BoardInfos.
			dw_MolduleConfiguration[1] & 0xFFFF0000UL) ==
		APCI1710_INCREMENTAL_COUNTER
		|| (devpriv->s_BoardInfos.
			dw_MolduleConfiguration[2] & 0xFFFF0000UL) ==
		APCI1710_INCREMENTAL_COUNTER
		|| (devpriv->s_BoardInfos.
			dw_MolduleConfiguration[3] & 0xFFFF0000UL) ==
		APCI1710_INCREMENTAL_COUNTER) {
		for (b_ModulCpt = 0; b_ModulCpt < 4; b_ModulCpt++) {
	      
			
	      

			if ((devpriv->s_BoardInfos.
					dw_MolduleConfiguration[b_ModulCpt] &
					0xFFFF0000UL) ==
				APCI1710_INCREMENTAL_COUNTER) {
		 
				
		 

				outl(1, devpriv->s_BoardInfos.
					ui_Address + 16 + (64 * b_ModulCpt));
			}
		}
	} else {
	   
		
	   

		DPRINTK("No counter module found\n");
		i_ReturnValue = -2;
	}

	return i_ReturnValue;
}


int i_APCI1710_SetInputFilter(struct comedi_device *dev,
	unsigned char b_ModulNbr, unsigned char b_PCIInputClock, unsigned char b_Filter)
{
	int i_ReturnValue = 0;
	unsigned int dw_Status = 0;

	
	
	

	if (b_ModulNbr < 4) {
	   
		
	   

		if ((devpriv->s_BoardInfos.
				dw_MolduleConfiguration[b_ModulNbr] &
				0xFFFF0000UL) == APCI1710_INCREMENTAL_COUNTER) {
	      
			
	      

			if ((devpriv->s_BoardInfos.
					dw_MolduleConfiguration[b_ModulNbr] &
					0xFFFF) >= 0x3135) {
		 
				
		 

				if ((b_PCIInputClock == APCI1710_30MHZ) ||
					(b_PCIInputClock == APCI1710_33MHZ) ||
					(b_PCIInputClock == APCI1710_40MHZ)) {
		    
					
		    

					if (b_Filter < 16) {
		       
						
		       

						if (b_PCIInputClock ==
							APCI1710_40MHZ) {
			  
							
			  

							dw_Status =
								inl(devpriv->
								s_BoardInfos.
								ui_Address +
								36 +
								(64 * b_ModulNbr));

			  
							
			  

							if ((dw_Status & 1) !=
								1) {
			     
								
			     

								DPRINTK("40MHz quartz not on board\n");
								i_ReturnValue =
									-6;
							}
						}	

		       
						
		       

						if (i_ReturnValue == 0) {
			  
							
			  

							if (b_PCIInputClock ==
								APCI1710_40MHZ)
							{
			     
								
			     

								devpriv->
									s_ModuleInfo
									[b_ModulNbr].
									s_SiemensCounterInfo.
									s_ModeRegister.
									s_ByteModeRegister.
									b_ModeRegister4
									=
									devpriv->
									s_ModuleInfo
									[b_ModulNbr].
									s_SiemensCounterInfo.
									s_ModeRegister.
									s_ByteModeRegister.
									b_ModeRegister4
									|
									APCI1710_ENABLE_40MHZ_FILTER;

							}	
							else {
			     
								
			     

								devpriv->
									s_ModuleInfo
									[b_ModulNbr].
									s_SiemensCounterInfo.
									s_ModeRegister.
									s_ByteModeRegister.
									b_ModeRegister4
									=
									devpriv->
									s_ModuleInfo
									[b_ModulNbr].
									s_SiemensCounterInfo.
									s_ModeRegister.
									s_ByteModeRegister.
									b_ModeRegister4
									&
									APCI1710_DISABLE_40MHZ_FILTER;

							}	

			  
							
			  

							devpriv->
								s_ModuleInfo
								[b_ModulNbr].
								s_SiemensCounterInfo.
								s_ModeRegister.
								s_ByteModeRegister.
								b_ModeRegister3
								=
								(devpriv->
								s_ModuleInfo
								[b_ModulNbr].
								s_SiemensCounterInfo.
								s_ModeRegister.
								s_ByteModeRegister.
								b_ModeRegister3
								& 0x1F) |
								((b_Filter &
									0x7) <<
								5);

							devpriv->
								s_ModuleInfo
								[b_ModulNbr].
								s_SiemensCounterInfo.
								s_ModeRegister.
								s_ByteModeRegister.
								b_ModeRegister4
								=
								(devpriv->
								s_ModuleInfo
								[b_ModulNbr].
								s_SiemensCounterInfo.
								s_ModeRegister.
								s_ByteModeRegister.
								b_ModeRegister4
								& 0xFE) |
								((b_Filter &
									0x8) >>
								3);

			  
							
			  

							outl(devpriv->
								s_ModuleInfo
								[b_ModulNbr].
								s_SiemensCounterInfo.
								s_ModeRegister.
								dw_ModeRegister1_2_3_4,
								devpriv->
								s_BoardInfos.
								ui_Address +
								20 +
								(64 * b_ModulNbr));
						}	
					}	
					else {
		       
						
		       

						DPRINTK("The selected filter value is wrong\n");
						i_ReturnValue = -5;
					}	
				}	
				else {
		    
					
		    

					DPRINTK("The selected PCI input clock is wrong\n");
					i_ReturnValue = 4;
				}	
			} else {
		 
				
		 

				DPRINTK("The module is not a counter module\n");
				i_ReturnValue = -3;
			}
		} else {
	      
			
	      

			DPRINTK("The module is not a counter module\n");
			i_ReturnValue = -3;
		}
	} else {
	   
		
	   

		DPRINTK("The selected module number parameter is wrong\n");
		i_ReturnValue = -2;
	}

	return i_ReturnValue;
}


int i_APCI1710_LatchCounter(struct comedi_device *dev,
	unsigned char b_ModulNbr, unsigned char b_LatchReg)
{
	int i_ReturnValue = 0;

	
	
	

	if (b_ModulNbr < 4) {
	   
		
	   

		if (devpriv->
			s_ModuleInfo[b_ModulNbr].
			s_SiemensCounterInfo.s_InitFlag.b_CounterInit == 1) {
	      
			
	      

			if (b_LatchReg < 2) {
		 
				
		 

				outl(1 << (b_LatchReg * 4),
					devpriv->s_BoardInfos.ui_Address +
					(64 * b_ModulNbr));
			} else {
		 
				
		 

				DPRINTK("The selected latch register parameter is wrong\n");
				i_ReturnValue = -4;
			}
		} else {
	      
			
			
	      

			DPRINTK("Counter not initialised\n");
			i_ReturnValue = -3;
		}
	} else {
	   
		
	   

		DPRINTK("The selected module number parameter is wrong\n");
		i_ReturnValue = -2;
	}

	return i_ReturnValue;
}


int i_APCI1710_SetIndexAndReferenceSource(struct comedi_device *dev,
	unsigned char b_ModulNbr, unsigned char b_SourceSelection)
{
	int i_ReturnValue = 0;

	
	
	

	if (b_ModulNbr < 4) {
	   
		
	   

		if ((devpriv->s_BoardInfos.
				dw_MolduleConfiguration[b_ModulNbr] &
				0xFFFF0000UL) == APCI1710_INCREMENTAL_COUNTER) {
	      
			
	      

			if ((devpriv->s_BoardInfos.
					dw_MolduleConfiguration[b_ModulNbr] &
					0xFFFF) >= 0x3135) {
		 
				
		 

				if (b_SourceSelection == APCI1710_SOURCE_0 ||
					b_SourceSelection == APCI1710_SOURCE_1)
				{
		    
					
		    

					if (b_SourceSelection ==
						APCI1710_SOURCE_1) {
		       
						
		       

						devpriv->
							s_ModuleInfo
							[b_ModulNbr].
							s_SiemensCounterInfo.
							s_ModeRegister.
							s_ByteModeRegister.
							b_ModeRegister4 =
							devpriv->
							s_ModuleInfo
							[b_ModulNbr].
							s_SiemensCounterInfo.
							s_ModeRegister.
							s_ByteModeRegister.
							b_ModeRegister4 |
							APCI1710_INVERT_INDEX_RFERENCE;
					} else {
		       
						
		       

						devpriv->
							s_ModuleInfo
							[b_ModulNbr].
							s_SiemensCounterInfo.
							s_ModeRegister.
							s_ByteModeRegister.
							b_ModeRegister4 =
							devpriv->
							s_ModuleInfo
							[b_ModulNbr].
							s_SiemensCounterInfo.
							s_ModeRegister.
							s_ByteModeRegister.
							b_ModeRegister4 &
							APCI1710_DEFAULT_INDEX_RFERENCE;
					}
				}	
				else {
		    
					
		    

					DPRINTK("The source selection is wrong\n");
					i_ReturnValue = -4;
				}	
			} else {
		 
				
		 

				DPRINTK("The module is not a counter module\n");
				i_ReturnValue = -3;
			}
		} else {
	      
			
	      

			DPRINTK("The module is not a counter module\n");
			i_ReturnValue = -3;
		}
	} else {
	   
		
	   

		DPRINTK("The selected module number is wrong\n");
		i_ReturnValue = -2;
	}

	return i_ReturnValue;
}


int i_APCI1710_SetDigitalChlOn(struct comedi_device *dev, unsigned char b_ModulNbr)
{
	int i_ReturnValue = 0;

	
	
	

	if (b_ModulNbr < 4) {
	   
		
	   

		if (devpriv->
			s_ModuleInfo[b_ModulNbr].
			s_SiemensCounterInfo.s_InitFlag.b_CounterInit == 1) {
			devpriv->
				s_ModuleInfo[b_ModulNbr].
				s_SiemensCounterInfo.
				s_ModeRegister.
				s_ByteModeRegister.
				b_ModeRegister3 = devpriv->
				s_ModuleInfo[b_ModulNbr].
				s_SiemensCounterInfo.
				s_ModeRegister.
				s_ByteModeRegister.b_ModeRegister3 | 0x10;

	      
			
	      

			outl(devpriv->s_ModuleInfo[b_ModulNbr].
				s_SiemensCounterInfo.
				s_ModeRegister.
				dw_ModeRegister1_2_3_4, devpriv->s_BoardInfos.
				ui_Address + 20 + (64 * b_ModulNbr));
		} else {
	      
			
			
	      

			DPRINTK("Counter not initialised\n");
			i_ReturnValue = -3;
		}
	} else {
	   
		
	   

		DPRINTK("The selected module number parameter is wrong\n");
		i_ReturnValue = -2;
	}

	return i_ReturnValue;
}


int i_APCI1710_SetDigitalChlOff(struct comedi_device *dev, unsigned char b_ModulNbr)
{
	int i_ReturnValue = 0;

	
	
	

	if (b_ModulNbr < 4) {
	   
		
	   

		if (devpriv->
			s_ModuleInfo[b_ModulNbr].
			s_SiemensCounterInfo.s_InitFlag.b_CounterInit == 1) {
			devpriv->
				s_ModuleInfo[b_ModulNbr].
				s_SiemensCounterInfo.
				s_ModeRegister.
				s_ByteModeRegister.
				b_ModeRegister3 = devpriv->
				s_ModuleInfo[b_ModulNbr].
				s_SiemensCounterInfo.
				s_ModeRegister.
				s_ByteModeRegister.b_ModeRegister3 & 0xEF;

	      
			
	      

			outl(devpriv->s_ModuleInfo[b_ModulNbr].
				s_SiemensCounterInfo.
				s_ModeRegister.
				dw_ModeRegister1_2_3_4, devpriv->s_BoardInfos.
				ui_Address + 20 + (64 * b_ModulNbr));
		} else {
	      
			
			
	      

			DPRINTK("Counter not initialised\n");
			i_ReturnValue = -3;
		}
	} else {
	   
		
	   

		DPRINTK("The selected module number parameter is wrong\n");
		i_ReturnValue = -2;
	}

	return i_ReturnValue;
}


							

int i_APCI1710_InsnWriteINCCPT(struct comedi_device *dev, struct comedi_subdevice *s,
	struct comedi_insn *insn, unsigned int *data)
{
	unsigned int ui_WriteType;
	int i_ReturnValue = 0;

	ui_WriteType = CR_CHAN(insn->chanspec);
	devpriv->tsk_Current = current;	

	switch (ui_WriteType) {
	case APCI1710_INCCPT_ENABLELATCHINTERRUPT:
		i_ReturnValue = i_APCI1710_EnableLatchInterrupt(dev,
			(unsigned char) CR_AREF(insn->chanspec));
		break;

	case APCI1710_INCCPT_DISABLELATCHINTERRUPT:
		i_ReturnValue = i_APCI1710_DisableLatchInterrupt(dev,
			(unsigned char) CR_AREF(insn->chanspec));
		break;

	case APCI1710_INCCPT_WRITE16BITCOUNTERVALUE:
		i_ReturnValue = i_APCI1710_Write16BitCounterValue(dev,
			(unsigned char) CR_AREF(insn->chanspec),
			(unsigned char) data[0], (unsigned int) data[1]);
		break;

	case APCI1710_INCCPT_WRITE32BITCOUNTERVALUE:
		i_ReturnValue = i_APCI1710_Write32BitCounterValue(dev,
			(unsigned char) CR_AREF(insn->chanspec), (unsigned int) data[0]);

		break;

	case APCI1710_INCCPT_ENABLEINDEX:
		i_APCI1710_EnableIndex(dev, (unsigned char) CR_AREF(insn->chanspec));
		break;

	case APCI1710_INCCPT_DISABLEINDEX:
		i_ReturnValue = i_APCI1710_DisableIndex(dev,
			(unsigned char) CR_AREF(insn->chanspec));
		break;

	case APCI1710_INCCPT_ENABLECOMPARELOGIC:
		i_ReturnValue = i_APCI1710_EnableCompareLogic(dev,
			(unsigned char) CR_AREF(insn->chanspec));
		break;

	case APCI1710_INCCPT_DISABLECOMPARELOGIC:
		i_ReturnValue = i_APCI1710_DisableCompareLogic(dev,
			(unsigned char) CR_AREF(insn->chanspec));
		break;

	case APCI1710_INCCPT_ENABLEFREQUENCYMEASUREMENT:
		i_ReturnValue = i_APCI1710_EnableFrequencyMeasurement(dev,
			(unsigned char) CR_AREF(insn->chanspec), (unsigned char) data[0]);
		break;

	case APCI1710_INCCPT_DISABLEFREQUENCYMEASUREMENT:
		i_ReturnValue = i_APCI1710_DisableFrequencyMeasurement(dev,
			(unsigned char) CR_AREF(insn->chanspec));
		break;

	default:
		printk("Write Config Parameter Wrong\n");
	}

	if (i_ReturnValue >= 0)
		i_ReturnValue = insn->n;
	return i_ReturnValue;
}


int i_APCI1710_EnableLatchInterrupt(struct comedi_device *dev, unsigned char b_ModulNbr)
{
	int i_ReturnValue = 0;

	
	
	

	if (b_ModulNbr < 4) {
	   
		
	   

		if (devpriv->s_ModuleInfo[b_ModulNbr].
			s_SiemensCounterInfo.s_InitFlag.b_CounterInit == 1) {

		 
			
		 

			devpriv->s_ModuleInfo[b_ModulNbr].
				s_SiemensCounterInfo.
				s_ModeRegister.
				s_ByteModeRegister.
				b_ModeRegister2 = devpriv->
				s_ModuleInfo[b_ModulNbr].
				s_SiemensCounterInfo.
				s_ModeRegister.
				s_ByteModeRegister.
				b_ModeRegister2 | APCI1710_ENABLE_LATCH_INT;

		 
			
		 

			outl(devpriv->s_ModuleInfo[b_ModulNbr].
				s_SiemensCounterInfo.
				s_ModeRegister.
				dw_ModeRegister1_2_3_4, devpriv->s_BoardInfos.
				ui_Address + 20 + (64 * b_ModulNbr));
		} else {
	      
			
			
	      

			DPRINTK("Counter not initialised\n");
			i_ReturnValue = -3;
		}
	} else {
	   
		
	   

		DPRINTK("The selected module number parameter is wrong\n");
		i_ReturnValue = -2;
	}

	return i_ReturnValue;
}


int i_APCI1710_DisableLatchInterrupt(struct comedi_device *dev, unsigned char b_ModulNbr)
{
	int i_ReturnValue = 0;

	
	
	

	if (b_ModulNbr < 4) {
	   
		
	   

		if (devpriv->
			s_ModuleInfo[b_ModulNbr].
			s_SiemensCounterInfo.s_InitFlag.b_CounterInit == 1) {

		 
			
		 

			outl(devpriv->s_ModuleInfo[b_ModulNbr].
				s_SiemensCounterInfo.
				s_ModeRegister.
				dw_ModeRegister1_2_3_4 &
				((APCI1710_DISABLE_LATCH_INT << 8) | 0xFF),
				devpriv->s_BoardInfos.ui_Address + 20 +
				(64 * b_ModulNbr));

			mdelay(1000);

		 
			
		 

			devpriv->
				s_ModuleInfo[b_ModulNbr].
				s_SiemensCounterInfo.
				s_ModeRegister.
				s_ByteModeRegister.
				b_ModeRegister2 = devpriv->
				s_ModuleInfo[b_ModulNbr].
				s_SiemensCounterInfo.
				s_ModeRegister.
				s_ByteModeRegister.
				b_ModeRegister2 & APCI1710_DISABLE_LATCH_INT;

		} else {
	      
			
			
	      

			DPRINTK("Counter not initialised\n");
			i_ReturnValue = -3;
		}
	} else {
	   
		
	   

		DPRINTK("The selected module number parameter is wrong\n");
		i_ReturnValue = -2;
	}

	return i_ReturnValue;
}


int i_APCI1710_Write16BitCounterValue(struct comedi_device *dev,
	unsigned char b_ModulNbr, unsigned char b_SelectedCounter, unsigned int ui_WriteValue)
{
	int i_ReturnValue = 0;

	
	
	

	if (b_ModulNbr < 4) {
	   
		
	   

		if (devpriv->
			s_ModuleInfo[b_ModulNbr].
			s_SiemensCounterInfo.s_InitFlag.b_CounterInit == 1) {
	      
			
	      

			if (b_SelectedCounter < 2) {
		 
				
		 

				outl((unsigned int) ((unsigned int) (ui_WriteValue) << (16 *
							b_SelectedCounter)),
					devpriv->s_BoardInfos.ui_Address + 8 +
					(b_SelectedCounter * 4) +
					(64 * b_ModulNbr));
			} else {
		 
				
		 

				DPRINTK("The selected 16-Bit counter parameter is wrong\n");
				i_ReturnValue = -4;
			}
		} else {
	      
			
			
	      

			DPRINTK("Counter not initialised\n");
			i_ReturnValue = -3;
		}
	} else {
	   
		
	   

		DPRINTK("The selected module number parameter is wrong\n");
		i_ReturnValue = -2;
	}

	return i_ReturnValue;
}


int i_APCI1710_Write32BitCounterValue(struct comedi_device *dev,
	unsigned char b_ModulNbr, unsigned int ul_WriteValue)
{
	int i_ReturnValue = 0;

	
	
	

	if (b_ModulNbr < 4) {
	   
		
	   

		if (devpriv->
			s_ModuleInfo[b_ModulNbr].
			s_SiemensCounterInfo.s_InitFlag.b_CounterInit == 1) {
	      
			
	      

			outl(ul_WriteValue, devpriv->s_BoardInfos.
				ui_Address + 4 + (64 * b_ModulNbr));
		} else {
	      
			
			
	      

			DPRINTK("Counter not initialised\n");
			i_ReturnValue = -3;
		}
	} else {
	   
		
	   

		DPRINTK("The selected module number parameter is wrong\n");
		i_ReturnValue = -2;
	}

	return i_ReturnValue;
}


int i_APCI1710_EnableIndex(struct comedi_device *dev, unsigned char b_ModulNbr)
{
	int i_ReturnValue = 0;
	unsigned int ul_InterruptLatchReg;

	
	
	

	if (b_ModulNbr < 4) {
	   
		
	   

		if (devpriv->
			s_ModuleInfo[b_ModulNbr].
			s_SiemensCounterInfo.s_InitFlag.b_CounterInit == 1) {
	      
			
	      

			if (devpriv->
				s_ModuleInfo[b_ModulNbr].
				s_SiemensCounterInfo.s_InitFlag.b_IndexInit) {
				devpriv->
					s_ModuleInfo[b_ModulNbr].
					s_SiemensCounterInfo.
					s_ModeRegister.
					s_ByteModeRegister.
					b_ModeRegister2 = devpriv->
					s_ModuleInfo[b_ModulNbr].
					s_SiemensCounterInfo.
					s_ModeRegister.
					s_ByteModeRegister.
					b_ModeRegister2 | APCI1710_ENABLE_INDEX;

				ul_InterruptLatchReg =
					inl(devpriv->s_BoardInfos.ui_Address +
					24 + (64 * b_ModulNbr));

				outl(devpriv->s_ModuleInfo[b_ModulNbr].
					s_SiemensCounterInfo.
					s_ModeRegister.
					dw_ModeRegister1_2_3_4,
					devpriv->s_BoardInfos.ui_Address + 20 +
					(64 * b_ModulNbr));
			} else {
		 
				
		 

				DPRINTK("Index not initialised \n");
				i_ReturnValue = -4;
			}
		} else {
	      
			
			
	      

			DPRINTK("Counter not initialised\n");
			i_ReturnValue = -3;
		}
	} else {
	   
		
	   

		DPRINTK("The selected module number parameter is wrong\n");
		i_ReturnValue = -2;
	}

	return i_ReturnValue;
}


int i_APCI1710_DisableIndex(struct comedi_device *dev, unsigned char b_ModulNbr)
{
	int i_ReturnValue = 0;

	
	
	

	if (b_ModulNbr < 4) {
	   
		
	   

		if (devpriv->
			s_ModuleInfo[b_ModulNbr].
			s_SiemensCounterInfo.s_InitFlag.b_CounterInit == 1) {
	      
			
	      

			if (devpriv->
				s_ModuleInfo[b_ModulNbr].
				s_SiemensCounterInfo.s_InitFlag.b_IndexInit) {
				devpriv->
					s_ModuleInfo[b_ModulNbr].
					s_SiemensCounterInfo.
					s_ModeRegister.
					s_ByteModeRegister.
					b_ModeRegister2 = devpriv->
					s_ModuleInfo[b_ModulNbr].
					s_SiemensCounterInfo.
					s_ModeRegister.
					s_ByteModeRegister.
					b_ModeRegister2 &
					APCI1710_DISABLE_INDEX;

				outl(devpriv->s_ModuleInfo[b_ModulNbr].
					s_SiemensCounterInfo.
					s_ModeRegister.
					dw_ModeRegister1_2_3_4,
					devpriv->s_BoardInfos.ui_Address + 20 +
					(64 * b_ModulNbr));
			} else {
		 
				
		 

				DPRINTK("Index not initialised  \n");
				i_ReturnValue = -4;
			}
		} else {
	      
			
			
	      

			DPRINTK("Counter not initialised\n");
			i_ReturnValue = -3;
		}
	} else {
	   
		
	   

		DPRINTK("The selected module number parameter is wrong\n");
		i_ReturnValue = -2;
	}

	return i_ReturnValue;
}


int i_APCI1710_EnableCompareLogic(struct comedi_device *dev, unsigned char b_ModulNbr)
{
	int i_ReturnValue = 0;

	
	
	

	if (b_ModulNbr < 4) {
	   
		
	   

		if (devpriv->
			s_ModuleInfo[b_ModulNbr].
			s_SiemensCounterInfo.s_InitFlag.b_CounterInit == 1) {
	      
			
	      

			if (devpriv->
				s_ModuleInfo[b_ModulNbr].
				s_SiemensCounterInfo.
				s_InitFlag.b_CompareLogicInit == 1) {
				devpriv->
					s_ModuleInfo[b_ModulNbr].
					s_SiemensCounterInfo.
					s_ModeRegister.
					s_ByteModeRegister.
					b_ModeRegister3 = devpriv->
					s_ModuleInfo[b_ModulNbr].
					s_SiemensCounterInfo.
					s_ModeRegister.
					s_ByteModeRegister.
					b_ModeRegister3 |
					APCI1710_ENABLE_COMPARE_INT;

		    
				
		    

				outl(devpriv->s_ModuleInfo[b_ModulNbr].
					s_SiemensCounterInfo.
					s_ModeRegister.
					dw_ModeRegister1_2_3_4,
					devpriv->s_BoardInfos.ui_Address + 20 +
					(64 * b_ModulNbr));
			} else {
		 
				
		 

				DPRINTK("Compare logic not initialised\n");
				i_ReturnValue = -4;
			}
		} else {
	      
			
			
	      

			DPRINTK("Counter not initialised\n");
			i_ReturnValue = -3;
		}
	} else {
	   
		
	   

		DPRINTK("The selected module number parameter is wrong\n");
		i_ReturnValue = -2;
	}

	return i_ReturnValue;
}


int i_APCI1710_DisableCompareLogic(struct comedi_device *dev, unsigned char b_ModulNbr)
{
	int i_ReturnValue = 0;

	
	
	

	if (b_ModulNbr < 4) {
	   
		
	   

		if (devpriv->
			s_ModuleInfo[b_ModulNbr].
			s_SiemensCounterInfo.s_InitFlag.b_CounterInit == 1) {
	      
			
	      

			if (devpriv->
				s_ModuleInfo[b_ModulNbr].
				s_SiemensCounterInfo.
				s_InitFlag.b_CompareLogicInit == 1) {
				devpriv->
					s_ModuleInfo[b_ModulNbr].
					s_SiemensCounterInfo.
					s_ModeRegister.
					s_ByteModeRegister.
					b_ModeRegister3 = devpriv->
					s_ModuleInfo[b_ModulNbr].
					s_SiemensCounterInfo.
					s_ModeRegister.
					s_ByteModeRegister.
					b_ModeRegister3 &
					APCI1710_DISABLE_COMPARE_INT;

		 
				
		 

				outl(devpriv->s_ModuleInfo[b_ModulNbr].
					s_SiemensCounterInfo.
					s_ModeRegister.
					dw_ModeRegister1_2_3_4,
					devpriv->s_BoardInfos.ui_Address + 20 +
					(64 * b_ModulNbr));
			} else {
		 
				
		 

				DPRINTK("Compare logic not initialised\n");
				i_ReturnValue = -4;
			}
		} else {
	      
			
			
	      

			DPRINTK("Counter not initialised\n");
			i_ReturnValue = -3;
		}
	} else {
	   
		
	   

		DPRINTK("The selected module number parameter is wrong\n");
		i_ReturnValue = -2;
	}

	return i_ReturnValue;
}


int i_APCI1710_EnableFrequencyMeasurement(struct comedi_device *dev,
	unsigned char b_ModulNbr, unsigned char b_InterruptEnable)
{
	int i_ReturnValue = 0;

	
	
	

	if (b_ModulNbr < 4) {
	   
		
	   

		if (devpriv->
			s_ModuleInfo[b_ModulNbr].
			s_SiemensCounterInfo.s_InitFlag.b_CounterInit == 1) {
	      
			
	      

			if (devpriv->
				s_ModuleInfo[b_ModulNbr].
				s_SiemensCounterInfo.
				s_InitFlag.b_FrequencyMeasurementInit == 1) {
		 
				
		 

				if ((b_InterruptEnable == APCI1710_DISABLE) ||
					(b_InterruptEnable == APCI1710_ENABLE))
				{

		       
					
		       

					devpriv->
						s_ModuleInfo[b_ModulNbr].
						s_SiemensCounterInfo.
						s_ModeRegister.
						s_ByteModeRegister.
						b_ModeRegister3 = devpriv->
						s_ModuleInfo[b_ModulNbr].
						s_SiemensCounterInfo.
						s_ModeRegister.
						s_ByteModeRegister.
						b_ModeRegister3 |
						APCI1710_ENABLE_FREQUENCY;

		       
					
		       

					devpriv->
						s_ModuleInfo[b_ModulNbr].
						s_SiemensCounterInfo.
						s_ModeRegister.
						s_ByteModeRegister.
						b_ModeRegister3 = (devpriv->
						s_ModuleInfo[b_ModulNbr].
						s_SiemensCounterInfo.
						s_ModeRegister.
						s_ByteModeRegister.
						b_ModeRegister3 &
						APCI1710_DISABLE_FREQUENCY_INT)
						| (b_InterruptEnable << 3);

		       
					
		       

					outl(devpriv->s_ModuleInfo[b_ModulNbr].
						s_SiemensCounterInfo.
						s_ModeRegister.
						dw_ModeRegister1_2_3_4,
						devpriv->s_BoardInfos.
						ui_Address + 20 +
						(64 * b_ModulNbr));

					devpriv->
						s_ModuleInfo[b_ModulNbr].
						s_SiemensCounterInfo.
						s_InitFlag.
						b_FrequencyMeasurementEnable =
						1;
				} else {
		    
					
		    

					DPRINTK("Interrupt parameter is wrong\n");
					i_ReturnValue = -5;
				}
			} else {
		 
				
		 

				DPRINTK("Frequency measurement logic not initialised\n");
				i_ReturnValue = -4;
			}
		} else {
	      
			
			
	      

			DPRINTK("Counter not initialised\n");
			i_ReturnValue = -3;
		}
	} else {
	   
		
	   

		DPRINTK("The selected module number parameter is wrong\n");
		i_ReturnValue = -2;
	}

	return i_ReturnValue;
}


int i_APCI1710_DisableFrequencyMeasurement(struct comedi_device *dev, unsigned char b_ModulNbr)
{
	int i_ReturnValue = 0;

	
	
	

	if (b_ModulNbr < 4) {
	   
		
	   

		if (devpriv->
			s_ModuleInfo[b_ModulNbr].
			s_SiemensCounterInfo.s_InitFlag.b_CounterInit == 1) {
	      
			
	      

			if (devpriv->
				s_ModuleInfo[b_ModulNbr].
				s_SiemensCounterInfo.
				s_InitFlag.b_FrequencyMeasurementInit == 1) {
		 
				
		 

				devpriv->
					s_ModuleInfo[b_ModulNbr].
					s_SiemensCounterInfo.
					s_ModeRegister.
					s_ByteModeRegister.
					b_ModeRegister3 = devpriv->
					s_ModuleInfo[b_ModulNbr].
					s_SiemensCounterInfo.
					s_ModeRegister.
					s_ByteModeRegister.
					b_ModeRegister3 &
					APCI1710_DISABLE_FREQUENCY
					
					& APCI1710_DISABLE_FREQUENCY_INT;
				

		 
				
		 

				outl(devpriv->s_ModuleInfo[b_ModulNbr].
					s_SiemensCounterInfo.
					s_ModeRegister.
					dw_ModeRegister1_2_3_4,
					devpriv->s_BoardInfos.ui_Address + 20 +
					(64 * b_ModulNbr));

		 
				
		 

				devpriv->
					s_ModuleInfo[b_ModulNbr].
					s_SiemensCounterInfo.
					s_InitFlag.
					b_FrequencyMeasurementEnable = 0;
			} else {
		 
				
		 

				DPRINTK("Frequency measurement logic not initialised\n");
				i_ReturnValue = -4;
			}
		} else {
	      
			
			
	      

			DPRINTK("Counter not initialised\n");
			i_ReturnValue = -3;
		}
	} else {
	   
		
	   

		DPRINTK("The selected module number parameter is wrong\n");
		i_ReturnValue = -2;
	}

	return i_ReturnValue;
}


							


int i_APCI1710_InsnReadINCCPT(struct comedi_device *dev, struct comedi_subdevice *s,
	struct comedi_insn *insn, unsigned int *data)
{
	unsigned int ui_ReadType;
	int i_ReturnValue = 0;

	ui_ReadType = CR_CHAN(insn->chanspec);

	devpriv->tsk_Current = current;	
	switch (ui_ReadType) {
	case APCI1710_INCCPT_READLATCHREGISTERSTATUS:
		i_ReturnValue = i_APCI1710_ReadLatchRegisterStatus(dev,
			(unsigned char) CR_AREF(insn->chanspec),
			(unsigned char) CR_RANGE(insn->chanspec), (unsigned char *) &data[0]);
		break;

	case APCI1710_INCCPT_READLATCHREGISTERVALUE:
		i_ReturnValue = i_APCI1710_ReadLatchRegisterValue(dev,
			(unsigned char) CR_AREF(insn->chanspec),
			(unsigned char) CR_RANGE(insn->chanspec), (unsigned int *) &data[0]);
		printk("Latch Register Value %d\n", data[0]);
		break;

	case APCI1710_INCCPT_READ16BITCOUNTERVALUE:
		i_ReturnValue = i_APCI1710_Read16BitCounterValue(dev,
			(unsigned char) CR_AREF(insn->chanspec),
			(unsigned char) CR_RANGE(insn->chanspec), (unsigned int *) &data[0]);
		break;

	case APCI1710_INCCPT_READ32BITCOUNTERVALUE:
		i_ReturnValue = i_APCI1710_Read32BitCounterValue(dev,
			(unsigned char) CR_AREF(insn->chanspec), (unsigned int *) &data[0]);
		break;

	case APCI1710_INCCPT_GETINDEXSTATUS:
		i_ReturnValue = i_APCI1710_GetIndexStatus(dev,
			(unsigned char) CR_AREF(insn->chanspec), (unsigned char *) &data[0]);
		break;

	case APCI1710_INCCPT_GETREFERENCESTATUS:
		i_ReturnValue = i_APCI1710_GetReferenceStatus(dev,
			(unsigned char) CR_AREF(insn->chanspec), (unsigned char *) &data[0]);
		break;

	case APCI1710_INCCPT_GETUASSTATUS:
		i_ReturnValue = i_APCI1710_GetUASStatus(dev,
			(unsigned char) CR_AREF(insn->chanspec), (unsigned char *) &data[0]);
		break;

	case APCI1710_INCCPT_GETCBSTATUS:
		i_ReturnValue = i_APCI1710_GetCBStatus(dev,
			(unsigned char) CR_AREF(insn->chanspec), (unsigned char *) &data[0]);
		break;

	case APCI1710_INCCPT_GET16BITCBSTATUS:
		i_ReturnValue = i_APCI1710_Get16BitCBStatus(dev,
			(unsigned char) CR_AREF(insn->chanspec),
			(unsigned char *) &data[0], (unsigned char *) &data[1]);
		break;

	case APCI1710_INCCPT_GETUDSTATUS:
		i_ReturnValue = i_APCI1710_GetUDStatus(dev,
			(unsigned char) CR_AREF(insn->chanspec), (unsigned char *) &data[0]);

		break;

	case APCI1710_INCCPT_GETINTERRUPTUDLATCHEDSTATUS:
		i_ReturnValue = i_APCI1710_GetInterruptUDLatchedStatus(dev,
			(unsigned char) CR_AREF(insn->chanspec), (unsigned char *) &data[0]);
		break;

	case APCI1710_INCCPT_READFREQUENCYMEASUREMENT:
		i_ReturnValue = i_APCI1710_ReadFrequencyMeasurement(dev,
			(unsigned char) CR_AREF(insn->chanspec),
			(unsigned char *) &data[0],
			(unsigned char *) &data[1], (unsigned int *) &data[2]);
		break;

	case APCI1710_INCCPT_READINTERRUPT:
		data[0] = devpriv->s_InterruptParameters.
			s_FIFOInterruptParameters[devpriv->
			s_InterruptParameters.ui_Read].b_OldModuleMask;
		data[1] = devpriv->s_InterruptParameters.
			s_FIFOInterruptParameters[devpriv->
			s_InterruptParameters.ui_Read].ul_OldInterruptMask;
		data[2] = devpriv->s_InterruptParameters.
			s_FIFOInterruptParameters[devpriv->
			s_InterruptParameters.ui_Read].ul_OldCounterLatchValue;

		
		
		

		devpriv->
			s_InterruptParameters.
			ui_Read = (devpriv->s_InterruptParameters.
			ui_Read + 1) % APCI1710_SAVE_INTERRUPT;

		break;

	default:
		printk("ReadType Parameter wrong\n");
	}

	if (i_ReturnValue >= 0)
		i_ReturnValue = insn->n;
	return i_ReturnValue;

}


int i_APCI1710_ReadLatchRegisterStatus(struct comedi_device *dev,
	unsigned char b_ModulNbr, unsigned char b_LatchReg, unsigned char *pb_LatchStatus)
{
	int i_ReturnValue = 0;
	unsigned int dw_LatchReg;

	
	
	

	if (b_ModulNbr < 4) {
	   
		
	   

		if (devpriv->
			s_ModuleInfo[b_ModulNbr].
			s_SiemensCounterInfo.s_InitFlag.b_CounterInit == 1) {
	      
			
	      

			if (b_LatchReg < 2) {
				dw_LatchReg = inl(devpriv->s_BoardInfos.
					ui_Address + (64 * b_ModulNbr));

				*pb_LatchStatus =
					(unsigned char) ((dw_LatchReg >> (b_LatchReg *
							4)) & 0x3);
			} else {
		 
				
		 

				DPRINTK("The selected latch register parameter is wrong\n");
				i_ReturnValue = -4;
			}
		} else {
	      
			
			
	      

			DPRINTK("Counter not initialised\n");
			i_ReturnValue = -3;
		}
	} else {
	   
		
	   

		DPRINTK("The selected module number parameter is wrong\n");
		i_ReturnValue = -2;
	}

	return i_ReturnValue;
}


int i_APCI1710_ReadLatchRegisterValue(struct comedi_device *dev,
	unsigned char b_ModulNbr, unsigned char b_LatchReg, unsigned int *pul_LatchValue)
{
	int i_ReturnValue = 0;

	
	
	

	if (b_ModulNbr < 4) {
	   
		
	   

		if (devpriv->
			s_ModuleInfo[b_ModulNbr].
			s_SiemensCounterInfo.s_InitFlag.b_CounterInit == 1) {
	      
			
	      

			if (b_LatchReg < 2) {
				*pul_LatchValue = inl(devpriv->s_BoardInfos.
					ui_Address + ((b_LatchReg + 1) * 4) +
					(64 * b_ModulNbr));

			} else {
		 
				
		 

				DPRINTK("The selected latch register parameter is wrong\n");
				i_ReturnValue = -4;
			}
		} else {
	      
			
			
	      

			DPRINTK("Counter not initialised\n");
			i_ReturnValue = -3;
		}
	} else {
	   
		
	   

		DPRINTK("The selected module number parameter is wrong\n");
		i_ReturnValue = -2;
	}

	return i_ReturnValue;
}


int i_APCI1710_Read16BitCounterValue(struct comedi_device *dev,
	unsigned char b_ModulNbr, unsigned char b_SelectedCounter, unsigned int *pui_CounterValue)
{
	int i_ReturnValue = 0;
	unsigned int dw_LathchValue = 0;

	
	
	

	if (b_ModulNbr < 4) {
	   
		
	   

		if (devpriv->
			s_ModuleInfo[b_ModulNbr].
			s_SiemensCounterInfo.s_InitFlag.b_CounterInit == 1) {
	      
			
	      

			if (b_SelectedCounter < 2) {
		 
				
		 

				outl(1, devpriv->s_BoardInfos.
					ui_Address + (64 * b_ModulNbr));

		 
				
		 

				dw_LathchValue = inl(devpriv->s_BoardInfos.
					ui_Address + 4 + (64 * b_ModulNbr));

				*pui_CounterValue =
					(unsigned int) ((dw_LathchValue >> (16 *
							b_SelectedCounter)) &
					0xFFFFU);
			} else {
		 
				
		 

				DPRINTK("The selected 16-Bit counter parameter is wrong\n");
				i_ReturnValue = -4;
			}
		} else {
	      
			
			
	      

			DPRINTK("Counter not initialised\n");
			i_ReturnValue = -3;
		}
	} else {
	   
		
	   

		DPRINTK("The selected module number parameter is wrong\n");
		i_ReturnValue = -2;
	}

	return i_ReturnValue;
}


int i_APCI1710_Read32BitCounterValue(struct comedi_device *dev,
	unsigned char b_ModulNbr, unsigned int *pul_CounterValue)
{
	int i_ReturnValue = 0;

	
	
	

	if (b_ModulNbr < 4) {
	   
		
	   

		if (devpriv->
			s_ModuleInfo[b_ModulNbr].
			s_SiemensCounterInfo.s_InitFlag.b_CounterInit == 1) {
	      
			
	      

			outl(1, devpriv->s_BoardInfos.
				ui_Address + (64 * b_ModulNbr));

	      
			
	      

			*pul_CounterValue = inl(devpriv->s_BoardInfos.
				ui_Address + 4 + (64 * b_ModulNbr));
		} else {
	      
			
			
	      

			DPRINTK("Counter not initialised\n");
			i_ReturnValue = -3;
		}
	} else {
	   
		
	   

		DPRINTK("The selected module number parameter is wrong\n");
		i_ReturnValue = -2;
	}

	return i_ReturnValue;
}


int i_APCI1710_GetIndexStatus(struct comedi_device *dev,
	unsigned char b_ModulNbr, unsigned char *pb_IndexStatus)
{
	int i_ReturnValue = 0;
	unsigned int dw_StatusReg = 0;

	
	
	

	if (b_ModulNbr < 4) {
	   
		
	   

		if (devpriv->
			s_ModuleInfo[b_ModulNbr].
			s_SiemensCounterInfo.s_InitFlag.b_CounterInit == 1) {
	      
			
	      

			if (devpriv->
				s_ModuleInfo[b_ModulNbr].
				s_SiemensCounterInfo.s_InitFlag.b_IndexInit) {
				dw_StatusReg = inl(devpriv->s_BoardInfos.
					ui_Address + 12 + (64 * b_ModulNbr));

				*pb_IndexStatus = (unsigned char) (dw_StatusReg & 1);
			} else {
		 
				
		 

				DPRINTK("Index not initialised\n");
				i_ReturnValue = -4;
			}
		} else {
	      
			
			
	      

			DPRINTK("Counter not initialised\n");
			i_ReturnValue = -3;
		}
	} else {
	   
		
	   

		DPRINTK("The selected module number parameter is wrong\n");
		i_ReturnValue = -2;
	}

	return i_ReturnValue;
}


int i_APCI1710_GetReferenceStatus(struct comedi_device *dev,
	unsigned char b_ModulNbr, unsigned char *pb_ReferenceStatus)
{
	int i_ReturnValue = 0;
	unsigned int dw_StatusReg = 0;

	
	
	

	if (b_ModulNbr < 4) {
	   
		
	   

		if (devpriv->
			s_ModuleInfo[b_ModulNbr].
			s_SiemensCounterInfo.s_InitFlag.b_CounterInit == 1) {
	      
			
	      

			if (devpriv->
				s_ModuleInfo[b_ModulNbr].
				s_SiemensCounterInfo.
				s_InitFlag.b_ReferenceInit) {
				dw_StatusReg = inl(devpriv->s_BoardInfos.
					ui_Address + 24 + (64 * b_ModulNbr));

				*pb_ReferenceStatus =
					(unsigned char) (~dw_StatusReg & 1);
			} else {
		 
				
		 

				DPRINTK("Reference not initialised\n");
				i_ReturnValue = -4;
			}
		} else {
	      
			
			
	      

			DPRINTK("Counter not initialised\n");
			i_ReturnValue = -3;
		}
	} else {
	   
		
	   

		DPRINTK("The selected module number parameter is wrong\n");
		i_ReturnValue = -2;
	}

	return i_ReturnValue;
}


int i_APCI1710_GetUASStatus(struct comedi_device *dev,
	unsigned char b_ModulNbr, unsigned char *pb_UASStatus)
{
	int i_ReturnValue = 0;
	unsigned int dw_StatusReg = 0;

	
	
	

	if (b_ModulNbr < 4) {
	   
		
	   

		if (devpriv->
			s_ModuleInfo[b_ModulNbr].
			s_SiemensCounterInfo.s_InitFlag.b_CounterInit == 1) {
			dw_StatusReg = inl(devpriv->s_BoardInfos.
				ui_Address + 24 + (64 * b_ModulNbr));

			*pb_UASStatus = (unsigned char) ((dw_StatusReg >> 1) & 1);
		} else {
	      
			
			
	      

			DPRINTK("Counter not initialised\n");
			i_ReturnValue = -3;
		}
	} else {
	   
		
	   

		DPRINTK("The selected module number parameter is wrong\n");
		i_ReturnValue = -2;

	}

	return i_ReturnValue;
}


int i_APCI1710_GetCBStatus(struct comedi_device *dev,
	unsigned char b_ModulNbr, unsigned char *pb_CBStatus)
{
	int i_ReturnValue = 0;
	unsigned int dw_StatusReg = 0;

	
	
	

	if (b_ModulNbr < 4) {
	   
		
	   

		if (devpriv->
			s_ModuleInfo[b_ModulNbr].
			s_SiemensCounterInfo.s_InitFlag.b_CounterInit == 1) {
			dw_StatusReg = inl(devpriv->s_BoardInfos.
				ui_Address + 16 + (64 * b_ModulNbr));

			*pb_CBStatus = (unsigned char) (dw_StatusReg & 1);

		} else {
	      
			
			
	      

			DPRINTK("Counter not initialised\n");
			i_ReturnValue = -3;
		}
	} else {
	   
		
	   

		DPRINTK("The selected module number parameter is wrong\n");
		i_ReturnValue = -2;
	}

	return i_ReturnValue;
}


int i_APCI1710_Get16BitCBStatus(struct comedi_device *dev,
	unsigned char b_ModulNbr, unsigned char *pb_CBStatusCounter0, unsigned char *pb_CBStatusCounter1)
{
	int i_ReturnValue = 0;
	unsigned int dw_StatusReg = 0;

	
	
	

	if (b_ModulNbr < 4) {
	   
		
	   

		if (devpriv->
			s_ModuleInfo[b_ModulNbr].
			s_SiemensCounterInfo.s_InitFlag.b_CounterInit == 1) {
	      
			
	      

			if ((devpriv->s_ModuleInfo[b_ModulNbr].
					s_SiemensCounterInfo.
					s_ModeRegister.
					s_ByteModeRegister.
					b_ModeRegister1 & 0x10) == 0x10) {
		 
				
		 

				if ((devpriv->s_BoardInfos.
						dw_MolduleConfiguration
						[b_ModulNbr] & 0xFFFF) >=
					0x3136) {
					dw_StatusReg =
						inl(devpriv->s_BoardInfos.
						ui_Address + 16 +
						(64 * b_ModulNbr));

					*pb_CBStatusCounter1 =
						(unsigned char) ((dw_StatusReg >> 0) &
						1);
					*pb_CBStatusCounter0 =
						(unsigned char) ((dw_StatusReg >> 1) &
						1);
				}	
				else {
		    
					
		    

					i_ReturnValue = -5;
				}	
			}	
			else {
		 
				
				
		 

				DPRINTK("Counter not initialised\n");
				i_ReturnValue = -4;
			}	
		}		
		else {
	      
			
			
	      

			DPRINTK("Counter not initialised\n");
			i_ReturnValue = -3;
		}		
	}			
	else {
	   
		
	   

		DPRINTK("The selected module number parameter is wrong\n");
		i_ReturnValue = -2;
	}			

	return i_ReturnValue;
}


int i_APCI1710_GetUDStatus(struct comedi_device *dev,
	unsigned char b_ModulNbr, unsigned char *pb_UDStatus)
{
	int i_ReturnValue = 0;
	unsigned int dw_StatusReg = 0;

	
	
	

	if (b_ModulNbr < 4) {
	   
		
	   

		if (devpriv->
			s_ModuleInfo[b_ModulNbr].
			s_SiemensCounterInfo.s_InitFlag.b_CounterInit == 1) {
			dw_StatusReg = inl(devpriv->s_BoardInfos.
				ui_Address + 24 + (64 * b_ModulNbr));

			*pb_UDStatus = (unsigned char) ((dw_StatusReg >> 2) & 1);

		} else {
	      
			
			
	      

			DPRINTK("Counter not initialised\n");
			i_ReturnValue = -3;
		}
	} else {
	   
		
	   

		DPRINTK("The selected module number parameter is wrong\n");
		i_ReturnValue = -2;
	}

	return i_ReturnValue;
}


int i_APCI1710_GetInterruptUDLatchedStatus(struct comedi_device *dev,
	unsigned char b_ModulNbr, unsigned char *pb_UDStatus)
{
	int i_ReturnValue = 0;
	unsigned int dw_StatusReg = 0;

	
	
	

	if (b_ModulNbr < 4) {
	   
		
	   

		if (devpriv->
			s_ModuleInfo[b_ModulNbr].
			s_SiemensCounterInfo.s_InitFlag.b_CounterInit == 1) {
		 
			
		 

			if (devpriv->
				s_ModuleInfo[b_ModulNbr].
				s_SiemensCounterInfo.
				s_InitFlag.b_IndexInterruptOccur == 1) {
				devpriv->
					s_ModuleInfo[b_ModulNbr].
					s_SiemensCounterInfo.
					s_InitFlag.b_IndexInterruptOccur = 0;

				dw_StatusReg = inl(devpriv->s_BoardInfos.
					ui_Address + 12 + (64 * b_ModulNbr));

				*pb_UDStatus = (unsigned char) ((dw_StatusReg >> 1) & 1);
			} else {
		    
				
		    

				*pb_UDStatus = 2;
			}
		} else {
	      
			
			
	      

			DPRINTK("Counter not initialised\n");
			i_ReturnValue = -3;
		}
	} else {
	   
		
	   

		DPRINTK("The selected module number parameter is wrong\n");
		i_ReturnValue = -2;
	}

	return i_ReturnValue;
}


int i_APCI1710_ReadFrequencyMeasurement(struct comedi_device *dev,
	unsigned char b_ModulNbr,
	unsigned char *pb_Status, unsigned char *pb_UDStatus, unsigned int *pul_ReadValue)
{
	int i_ReturnValue = 0;
	unsigned int ui_16BitValue;
	unsigned int dw_StatusReg;

	
	
	

	if (b_ModulNbr < 4) {
	   
		
	   

		if (devpriv->
			s_ModuleInfo[b_ModulNbr].
			s_SiemensCounterInfo.s_InitFlag.b_CounterInit == 1) {
	      
			
	      

			if (devpriv->
				s_ModuleInfo[b_ModulNbr].
				s_SiemensCounterInfo.
				s_InitFlag.b_FrequencyMeasurementInit == 1) {
		 
				
		 

				if (devpriv->
					s_ModuleInfo[b_ModulNbr].
					s_SiemensCounterInfo.
					s_InitFlag.
					b_FrequencyMeasurementEnable == 1) {
		    
					
		    

					dw_StatusReg =
						inl(devpriv->s_BoardInfos.
						ui_Address + 32 +
						(64 * b_ModulNbr));

		    
					
		    

					if (dw_StatusReg & 1) {
						*pb_Status = 2;
						*pb_UDStatus =
							(unsigned char) ((dw_StatusReg >>
								1) & 3);

		       
						
		       

						*pul_ReadValue =
							inl(devpriv->
							s_BoardInfos.
							ui_Address + 28 +
							(64 * b_ModulNbr));

						if (*pb_UDStatus == 0) {
			  
							
			  

							if ((devpriv->s_ModuleInfo[b_ModulNbr].s_SiemensCounterInfo.s_ModeRegister.s_ByteModeRegister.b_ModeRegister1 & APCI1710_16BIT_COUNTER) == APCI1710_16BIT_COUNTER) {
			     
								
			     

								if ((*pul_ReadValue & 0xFFFFU) != 0) {
									ui_16BitValue
										=
										(unsigned int)
										*
										pul_ReadValue
										&
										0xFFFFU;
									*pul_ReadValue
										=
										(*pul_ReadValue
										&
										0xFFFF0000UL)
										|
										(0xFFFFU
										-
										ui_16BitValue);
								}

			     
								
			     

								if ((*pul_ReadValue & 0xFFFF0000UL) != 0) {
									ui_16BitValue
										=
										(unsigned int)
										(
										(*pul_ReadValue
											>>
											16)
										&
										0xFFFFU);
									*pul_ReadValue
										=
										(*pul_ReadValue
										&
										0xFFFFUL)
										|
										(
										(0xFFFFU - ui_16BitValue) << 16);
								}
							} else {
								if (*pul_ReadValue != 0) {
									*pul_ReadValue
										=
										0xFFFFFFFFUL
										-
										*pul_ReadValue;
								}
							}
						} else {
							if (*pb_UDStatus == 1) {
			     
								
			     

								if ((*pul_ReadValue & 0xFFFF0000UL) != 0) {
									ui_16BitValue
										=
										(unsigned int)
										(
										(*pul_ReadValue
											>>
											16)
										&
										0xFFFFU);
									*pul_ReadValue
										=
										(*pul_ReadValue
										&
										0xFFFFUL)
										|
										(
										(0xFFFFU - ui_16BitValue) << 16);
								}
							} else {
								if (*pb_UDStatus
									== 2) {
				
									
				

									if ((*pul_ReadValue & 0xFFFFU) != 0) {
										ui_16BitValue
											=
											(unsigned int)
											*
											pul_ReadValue
											&
											0xFFFFU;
										*pul_ReadValue
											=
											(*pul_ReadValue
											&
											0xFFFF0000UL)
											|
											(0xFFFFU
											-
											ui_16BitValue);
									}
								}
							}
						}
					} else {
						*pb_Status = 1;
						*pb_UDStatus = 0;
					}
				} else {
					*pb_Status = 0;
					*pb_UDStatus = 0;
				}
			} else {
		 
				
		 

				DPRINTK("Frequency measurement logic not initialised\n");
				i_ReturnValue = -4;
			}
		} else {
	      
			
			
	      

			DPRINTK("Counter not initialised\n");
			i_ReturnValue = -3;
		}
	} else {
	   
		
	   

		DPRINTK("The selected module number parameter is wrong\n");
		i_ReturnValue = -2;
	}

	return i_ReturnValue;
}
