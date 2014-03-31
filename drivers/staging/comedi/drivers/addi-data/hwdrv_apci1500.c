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
#include "hwdrv_apci1500.h"

static int i_TimerCounter1Init = 0;
static int i_TimerCounter2Init = 0;
static int i_WatchdogCounter3Init = 0;
static int i_Event1Status = 0, i_Event2Status = 0;
static int i_TimerCounterWatchdogInterrupt = 0;
static int i_Logic = 0, i_CounterLogic = 0;
static int i_InterruptMask = 0;
static int i_InputChannel = 0;
static int i_TimerCounter1Enabled = 0, i_TimerCounter2Enabled = 0,
	   i_WatchdogCounter3Enabled = 0;

static int i_APCI1500_ConfigDigitalInputEvent(struct comedi_device *dev,
					      struct comedi_subdevice *s,
					      struct comedi_insn *insn,
					      unsigned int *data)
{
	int i_PatternPolarity = 0, i_PatternTransition = 0, i_PatternMask = 0;
	int i_MaxChannel = 0, i_Count = 0, i_EventMask = 0;
	int i_PatternTransitionCount = 0, i_RegValue;
	int i;

      
	
      
	outb(APCI1500_RW_MASTER_INTERRUPT_CONTROL,
		devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
      
	
      
	outb(0x00, devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);

	if (data[0] == 1) {
		i_MaxChannel = 8;
	}			
	else {
		if (data[0] == 2) {
			i_MaxChannel = 6;
		}		
		else {
			printk("\nThe specified port event  does not exist\n");
			return -EINVAL;
		}		
	}			
	switch (data[1]) {
	case 0:
		data[1] = APCI1500_AND;
		break;
	case 1:
		data[1] = APCI1500_OR;
		break;
	case 2:
		data[1] = APCI1500_OR_PRIORITY;
		break;
	default:
		printk("\nThe specified interrupt logic does not exist\n");
		return -EINVAL;
	}			

	i_Logic = data[1];
	for (i_Count = i_MaxChannel, i = 0; i_Count > 0; i_Count--, i++) {
		i_EventMask = data[2 + i];
		switch (i_EventMask) {
		case 0:
			i_PatternMask =
				i_PatternMask | (1 << (i_MaxChannel - i_Count));
			break;
		case 1:
			i_PatternMask =
				i_PatternMask | (1 << (i_MaxChannel - i_Count));
			i_PatternPolarity =
				i_PatternPolarity | (1 << (i_MaxChannel -
					i_Count));
			break;
		case 2:
			i_PatternMask =
				i_PatternMask | (1 << (i_MaxChannel - i_Count));
			i_PatternTransition =
				i_PatternTransition | (1 << (i_MaxChannel -
					i_Count));
			break;
		case 3:
			i_PatternMask =
				i_PatternMask | (1 << (i_MaxChannel - i_Count));
			i_PatternPolarity =
				i_PatternPolarity | (1 << (i_MaxChannel -
					i_Count));
			i_PatternTransition =
				i_PatternTransition | (1 << (i_MaxChannel -
					i_Count));
			break;
		case 4:
			i_PatternTransition =
				i_PatternTransition | (1 << (i_MaxChannel -
					i_Count));
			break;
		case 5:
			break;
		default:
			printk("\nThe option indicated in the event mask does not exist\n");
			return -EINVAL;
		}		
	}			

	if (data[0] == 1) {
		    
		
		    

		if (data[1] == APCI1500_AND ||
			data[1] == APCI1500_OR ||
			data[1] == APCI1500_OR_PRIORITY) {
		       
			
			
		       

			if (data[1] == APCI1500_OR_PRIORITY
				&& i_PatternTransition != 0) {
			      
				
			      
				printk("\nTransition error on an OR PRIORITY logic\n");
				return -EINVAL;
			}	

		       
			
			
		       

			if (data[1] == APCI1500_AND) {
				for (i_Count = 0; i_Count < 8; i_Count++) {
					i_PatternTransitionCount =
						i_PatternTransitionCount +
						((i_PatternTransition >>
							i_Count) & 0x1);

				}	

				if (i_PatternTransitionCount > 1) {
				  
					
				  
					printk("\n Transition error on an AND logic\n");
					return -EINVAL;
				}	
			}	

			    
			
			    
			outb(APCI1500_RW_MASTER_CONFIGURATION_CONTROL,
				devpriv->iobase +
				APCI1500_Z8536_CONTROL_REGISTER);
			
			
			    
			outb(0xF0,
				devpriv->iobase +
				APCI1500_Z8536_CONTROL_REGISTER);
			
			
			    
			outb(APCI1500_RW_PORT_A_PATTERN_POLARITY,
				devpriv->iobase +
				APCI1500_Z8536_CONTROL_REGISTER);
			outb(i_PatternPolarity,
				devpriv->iobase +
				APCI1500_Z8536_CONTROL_REGISTER);

			
			
			
			    
			outb(APCI1500_RW_PORT_A_PATTERN_MASK,
				devpriv->iobase +
				APCI1500_Z8536_CONTROL_REGISTER);
			outb(i_PatternMask,
				devpriv->iobase +
				APCI1500_Z8536_CONTROL_REGISTER);
			
			
			
			    
			outb(APCI1500_RW_PORT_A_PATTERN_TRANSITION,
				devpriv->iobase +
				APCI1500_Z8536_CONTROL_REGISTER);
			outb(i_PatternTransition,
				devpriv->iobase +
				APCI1500_Z8536_CONTROL_REGISTER);

		      
			
			
			  
			outb(APCI1500_RW_PORT_A_SPECIFICATION,
				devpriv->iobase +
				APCI1500_Z8536_CONTROL_REGISTER);
			i_RegValue =
				inb(devpriv->iobase +
				APCI1500_Z8536_CONTROL_REGISTER);

		      
			
			
			  
			outb(APCI1500_RW_PORT_A_SPECIFICATION,
				devpriv->iobase +
				APCI1500_Z8536_CONTROL_REGISTER);

		      
			
			  

			i_RegValue = (i_RegValue & 0xF9) | data[1] | 0x9;
			outb(i_RegValue,
				devpriv->iobase +
				APCI1500_Z8536_CONTROL_REGISTER);

			i_Event1Status = 1;

		      
			
			  

			outb(APCI1500_RW_MASTER_CONFIGURATION_CONTROL,
				devpriv->iobase +
				APCI1500_Z8536_CONTROL_REGISTER);
		      
			
			  
			outb(0xF4,
				devpriv->iobase +
				APCI1500_Z8536_CONTROL_REGISTER);

		}		
		else {
			printk("\nThe choice for interrupt logic does not exist\n");
			return -EINVAL;
		}		
	}			

		 
	
		 

	if (data[0] == 2) {
		    
		
		    

		if (data[1] == APCI1500_OR) {
		       
			
		       
			outb(APCI1500_RW_MASTER_CONFIGURATION_CONTROL,
				devpriv->iobase +
				APCI1500_Z8536_CONTROL_REGISTER);
		       
			
		       
			outb(0x74,
				devpriv->iobase +
				APCI1500_Z8536_CONTROL_REGISTER);
		       
			
			
		       
			outb(APCI1500_RW_PORT_B_SPECIFICATION,
				devpriv->iobase +
				APCI1500_Z8536_CONTROL_REGISTER);
			i_RegValue =
				inb(devpriv->iobase +
				APCI1500_Z8536_CONTROL_REGISTER);

		       
			
			
		       
			outb(APCI1500_RW_PORT_B_SPECIFICATION,
				devpriv->iobase +
				APCI1500_Z8536_CONTROL_REGISTER);
			i_RegValue = i_RegValue & 0xF9;
			outb(i_RegValue,
				devpriv->iobase +
				APCI1500_Z8536_CONTROL_REGISTER);

		       
			
		       

			i_PatternMask = (i_PatternMask | 0xC0);
			i_PatternPolarity = (i_PatternPolarity | 0xC0);
			i_PatternTransition = (i_PatternTransition | 0xC0);

		       
			
		       
			outb(APCI1500_RW_PORT_B_PATTERN_POLARITY,
				devpriv->iobase +
				APCI1500_Z8536_CONTROL_REGISTER);
			outb(i_PatternPolarity,
				devpriv->iobase +
				APCI1500_Z8536_CONTROL_REGISTER);
		       
			
			
		       
			outb(APCI1500_RW_PORT_B_PATTERN_TRANSITION,
				devpriv->iobase +
				APCI1500_Z8536_CONTROL_REGISTER);
			outb(i_PatternTransition,
				devpriv->iobase +
				APCI1500_Z8536_CONTROL_REGISTER);
		       
			
			
		       

			outb(APCI1500_RW_PORT_B_PATTERN_MASK,
				devpriv->iobase +
				APCI1500_Z8536_CONTROL_REGISTER);
			outb(i_PatternMask,
				devpriv->iobase +
				APCI1500_Z8536_CONTROL_REGISTER);

		       
			
			
		       
			outb(APCI1500_RW_PORT_B_SPECIFICATION,
				devpriv->iobase +
				APCI1500_Z8536_CONTROL_REGISTER);
			i_RegValue =
				inb(devpriv->iobase +
				APCI1500_Z8536_CONTROL_REGISTER);
		       
			
			
		       
			outb(APCI1500_RW_PORT_B_SPECIFICATION,
				devpriv->iobase +
				APCI1500_Z8536_CONTROL_REGISTER);
			i_RegValue = (i_RegValue & 0xF9) | 4;
			outb(i_RegValue,
				devpriv->iobase +
				APCI1500_Z8536_CONTROL_REGISTER);

			i_Event2Status = 1;
		       
			
		       

			outb(APCI1500_RW_MASTER_CONFIGURATION_CONTROL,
				devpriv->iobase +
				APCI1500_Z8536_CONTROL_REGISTER);
		       
			
		       

			outb(0xF4,
				devpriv->iobase +
				APCI1500_Z8536_CONTROL_REGISTER);
		}		
		else {
			printk("\nThe choice for interrupt logic does not exist\n");
			return -EINVAL;
		}		
	}			

	return insn->n;
}

static int i_APCI1500_StartStopInputEvent(struct comedi_device *dev,
					  struct comedi_subdevice *s,
					  struct comedi_insn *insn,
					  unsigned int *data)
{
	int i_Event1InterruptStatus = 0, i_Event2InterruptStatus =
		0, i_RegValue;
	switch (data[0]) {
	case START:
	      
		
	      

		if (data[1] == 1 || data[1] == 2) {
		  
			
		  

			if (data[1] == 1) {
		    
				
		    
				if (i_Event1Status == 1) {
		       
					
		       
					outb(APCI1500_RW_MASTER_CONFIGURATION_CONTROL, devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
		       
					
		       
					outb(0xF0,
						devpriv->iobase +
						APCI1500_Z8536_CONTROL_REGISTER);
		       
					
					
		       
					outb(APCI1500_RW_PORT_A_COMMAND_AND_STATUS, devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
		       
					
		       
					outb(0xC0,
						devpriv->iobase +
						APCI1500_Z8536_CONTROL_REGISTER);
		       
					
		       
					outb(APCI1500_RW_MASTER_CONFIGURATION_CONTROL, devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
		       
					
		       
					outb(0xF4,
						devpriv->iobase +
						APCI1500_Z8536_CONTROL_REGISTER);
					i_Event1InterruptStatus = 1;
					outb(APCI1500_RW_PORT_A_SPECIFICATION,
						devpriv->iobase +
						APCI1500_Z8536_CONTROL_REGISTER);
					i_RegValue =
						inb(devpriv->iobase +
						APCI1500_Z8536_CONTROL_REGISTER);

					
		       
					outb(APCI1500_RW_MASTER_INTERRUPT_CONTROL, devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
		       
					
		       
					outb(0xD0,
						devpriv->iobase +
						APCI1500_Z8536_CONTROL_REGISTER);

				}	
				else {
					printk("\nEvent 1 not initialised\n");
					return -EINVAL;
				}	
			}	
			if (data[1] == 2) {

				if (i_Event2Status == 1) {
			    
					
			    
					outb(APCI1500_RW_MASTER_CONFIGURATION_CONTROL, devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
		       
					
		       
					outb(0x74,
						devpriv->iobase +
						APCI1500_Z8536_CONTROL_REGISTER);
		       
					
					
		       
					outb(APCI1500_RW_PORT_B_COMMAND_AND_STATUS, devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
		       
					
		       
					outb(0xC0,
						devpriv->iobase +
						APCI1500_Z8536_CONTROL_REGISTER);
		       
					
		       
					outb(APCI1500_RW_MASTER_CONFIGURATION_CONTROL, devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
		       
					
		       
					outb(0xF4,
						devpriv->iobase +
						APCI1500_Z8536_CONTROL_REGISTER);

					
		       
					outb(APCI1500_RW_MASTER_INTERRUPT_CONTROL, devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
		       
					
		       
					outb(0xD0,
						devpriv->iobase +
						APCI1500_Z8536_CONTROL_REGISTER);
					i_Event2InterruptStatus = 1;
				}	
				else {
					printk("\nEvent 2 not initialised\n");
					return -EINVAL;
				}	
			}	
		}		
		else {
			printk("\nThe port parameter is in error\n");
			return -EINVAL;
		}		

		break;

	case STOP:
		  
		
		  

		if (data[1] == 1 || data[1] == 2) {
		  
			
		  

			if (data[1] == 1) {
		    
				
		    
				if (i_Event1Status == 1) {
		       
					
		       
					outb(APCI1500_RW_MASTER_CONFIGURATION_CONTROL, devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
		       
					
		       
					outb(0xF0,
						devpriv->iobase +
						APCI1500_Z8536_CONTROL_REGISTER);
		       
					
					
		       
					outb(APCI1500_RW_PORT_A_COMMAND_AND_STATUS, devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
		       
					
		       
					outb(0xE0,
						devpriv->iobase +
						APCI1500_Z8536_CONTROL_REGISTER);
		       
					
		       
					outb(APCI1500_RW_MASTER_CONFIGURATION_CONTROL, devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
		       
					
		       
					outb(0xF4,
						devpriv->iobase +
						APCI1500_Z8536_CONTROL_REGISTER);
					i_Event1InterruptStatus = 0;
				}	
				else {
					printk("\nEvent 1 not initialised\n");
					return -EINVAL;
				}	
			}	
			if (data[1] == 2) {
			 
				
			 
				if (i_Event2Status == 1) {
			  
					
			  
					outb(APCI1500_RW_MASTER_CONFIGURATION_CONTROL, devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
			  
					
			  
					outb(0x74,
						devpriv->iobase +
						APCI1500_Z8536_CONTROL_REGISTER);
			  
					
					
			  
					outb(APCI1500_RW_PORT_B_COMMAND_AND_STATUS, devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
		       
					
		       
					outb(0xE0,
						devpriv->iobase +
						APCI1500_Z8536_CONTROL_REGISTER);
		       
					
		       
					outb(APCI1500_RW_MASTER_CONFIGURATION_CONTROL, devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
		       
					
		       
					outb(0xF4,
						devpriv->iobase +
						APCI1500_Z8536_CONTROL_REGISTER);
					i_Event2InterruptStatus = 0;
				}	
				else {
					printk("\nEvent 2 not initialised\n");
					return -EINVAL;
				}	
			}	

		}		
		else {
			printk("\nThe port parameter is in error\n");
			return -EINVAL;
		}		
		break;
	default:
		printk("\nThe option of START/STOP logic does not exist\n");
		return -EINVAL;
	}			

	return insn->n;
}

static int i_APCI1500_Initialisation(struct comedi_device *dev,
				     struct comedi_subdevice *s,
				     struct comedi_insn *insn,
				     unsigned int *data)
{
	int i_DummyRead = 0;
    
	
    
	i_DummyRead = inb(devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
	outb(0, devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
	i_DummyRead = inb(devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
	outb(0, devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
	outb(1, devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
	outb(0, devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);

 
	
 
	outb(APCI1500_RW_MASTER_CONFIGURATION_CONTROL,
		devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
	outb(0xF4, devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);

	
	
	
	outb(APCI1500_RW_PORT_A_SPECIFICATION,
		devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
	outb(0x10, devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);

	
	outb(APCI1500_RW_PORT_A_DATA_PCITCH_POLARITY,
		devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
	
	outb(0xFF, devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);

	
	outb(APCI1500_RW_PORT_A_DATA_DIRECTION,
		devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
	
	outb(0xFF, devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
	
	outb(APCI1500_RW_PORT_A_COMMAND_AND_STATUS,
		devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
	
	outb(0x20, devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
	
	outb(APCI1500_RW_PORT_A_COMMAND_AND_STATUS,
		devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
	
	outb(0xE0, devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
	
	outb(APCI1500_RW_PORT_A_HANDSHAKE_SPECIFICATION,
		devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
	
	outb(0, devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);

	 
	
	 
	outb(APCI1500_RW_PORT_B_SPECIFICATION,
		devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
	outb(0x10, devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
	
	outb(APCI1500_RW_PORT_B_DATA_PCITCH_POLARITY,
		devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
	
	outb(0x7F, devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
	
	outb(APCI1500_RW_PORT_B_DATA_DIRECTION,
		devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
	
	outb(0xFF, devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
	
	outb(APCI1500_RW_PORT_B_COMMAND_AND_STATUS,
		devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
	
	outb(0x20, devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
	
	outb(APCI1500_RW_PORT_B_COMMAND_AND_STATUS,
		devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
	
	outb(0xE0, devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
	
	outb(APCI1500_RW_PORT_B_HANDSHAKE_SPECIFICATION,
		devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
	
	outb(0, devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);

	   
	
	   
	outb(APCI1500_RW_PORT_C_DATA_PCITCH_POLARITY,
		devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
	
	outb(0x9, devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
	
	outb(APCI1500_RW_PORT_C_DATA_DIRECTION,
		devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
	
	outb(0x0E, devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
	
	outb(APCI1500_RW_PORT_C_SPECIAL_IO_CONTROL,
		devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
	
	outb(0, devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
	   
	
	   
	outb(APCI1500_RW_CPT_TMR1_CMD_STATUS,
		devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
	
	outb(0x20, devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
	
	outb(APCI1500_RW_CPT_TMR1_CMD_STATUS,
		devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
	
	outb(0xE0, devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
	   
	
	   
	outb(APCI1500_RW_CPT_TMR2_CMD_STATUS,
		devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
	
	outb(0x20, devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
	
	outb(APCI1500_RW_CPT_TMR2_CMD_STATUS,
		devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
	
	outb(0xE0, devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
	  
	
	  
	outb(APCI1500_RW_CPT_TMR3_CMD_STATUS,
		devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
	
	outb(0x20, devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
	
	outb(APCI1500_RW_CPT_TMR3_CMD_STATUS,
		devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
	
	outb(0xE0, devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
	 
	
	 
	outb(APCI1500_RW_MASTER_INTERRUPT_CONTROL,
		devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
	
	outb(0, devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
	return insn->n;
}

static int i_APCI1500_ReadMoreDigitalInput(struct comedi_device *dev,
					   struct comedi_subdevice *s,
					   struct comedi_insn *insn,
					   unsigned int *data)
{
	unsigned int ui_PortValue = data[1];
	unsigned int ui_Mask = 0;
	unsigned int ui_Channel;
	unsigned int ui_TmpValue = 0;
	ui_Channel = CR_CHAN(insn->chanspec);

	switch (data[0]) {
	case 0:
		if (ui_Channel <= 15) {
			ui_TmpValue =
				(unsigned int) inw(devpriv->i_IobaseAddon +
				APCI1500_DIGITAL_IP);
			*data = (ui_TmpValue >> ui_Channel) & 0x1;
		}		
		else {
			printk("\nThe channel specification are in error\n");
			return -EINVAL;	
		}		
		break;
	case 1:

		*data = (unsigned int) inw(devpriv->i_IobaseAddon +
			APCI1500_DIGITAL_IP);
		switch (ui_Channel) {
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
		case 15:
			break;

		default:
			printk("\nSpecified channel cannot be read \n");
			return -EINVAL;	
			break;
		}		
		break;
	default:
		printk("\nThe specified functionality does not exist\n");
		return -EINVAL;
	}			
	return insn->n;
}

static int i_APCI1500_ConfigDigitalOutputErrorInterrupt(struct comedi_device *dev,
							struct comedi_subdevice *s,
							struct comedi_insn *insn,
							unsigned int *data)
{
	devpriv->b_OutputMemoryStatus = data[0];
	return insn->n;
}

static int i_APCI1500_WriteDigitalOutput(struct comedi_device *dev,
					 struct comedi_subdevice *s,
					 struct comedi_insn *insn,
					 unsigned int *data)
{
	static unsigned int ui_Temp = 0;
	unsigned int ui_Temp1;

	unsigned int ui_NoOfChannel = CR_CHAN(insn->chanspec);	

	if (!devpriv->b_OutputMemoryStatus) {
		ui_Temp = 0;

	}			
	if (data[3] == 0) {
		if (data[1] == 0) {
			data[0] = (data[0] << ui_NoOfChannel) | ui_Temp;
			outw(data[0],
				devpriv->i_IobaseAddon + APCI1500_DIGITAL_OP);
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
					devpriv->i_IobaseAddon +
					APCI1500_DIGITAL_OP);
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
				data[0] =
					(data[0] << ui_NoOfChannel) ^
					0xffffffff;
				data[0] = data[0] & ui_Temp;
				outw(data[0],
					devpriv->i_IobaseAddon +
					APCI1500_DIGITAL_OP);
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
							0xffffffff) & ui_Temp;
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
							0xffffffff) & ui_Temp;
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
							0xffffffff) & ui_Temp;
						break;

					case 15:
						break;

					default:
						comedi_error(dev,
							" chan spec wrong");
						return -EINVAL;	

					}	

					outw(data[0],
						devpriv->i_IobaseAddon +
						APCI1500_DIGITAL_OP);
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
	ui_Temp = data[0];
	return insn->n;
}

static int i_APCI1500_ConfigCounterTimerWatchdog(struct comedi_device *dev,
						 struct comedi_subdevice *s,
						 struct comedi_insn *insn,
						 unsigned int *data)
{
	int i_TimerCounterMode, i_MasterConfiguration;

	devpriv->tsk_Current = current;

	if (data[0] == 0 || data[0] == 1 || data[0] == 2) {
		outw(data[0], devpriv->i_IobaseAddon + APCI1500_CLK_SELECT);
	}			
	else {
		if (data[0] != 3) {
			printk("\nThe option for input clock selection does not exist\n");
			return -EINVAL;
		}		
	}			
	
	switch (data[1]) {
	case COUNTER1:
		
		switch (data[2]) {
		case 0:
			data[2] = APCI1500_COUNTER;
			break;
		case 1:
			data[2] = APCI1500_TIMER;
			break;
		default:
			printk("\nThis choice is not a timer nor a counter\n");
			return -EINVAL;
		}		

		
		switch (data[4]) {
		case 0:
			data[4] = APCI1500_CONTINUOUS;
			break;
		case 1:
			data[4] = APCI1500_SINGLE;
			break;
		default:
			printk("\nThis option for single/continuous mode does not exist\n");
			return -EINVAL;
		}		

		i_TimerCounterMode = data[2] | data[4] | 7;
			 
		
			 

		if ((data[3] >= 0) && (data[3] <= 65535)) {
			if (data[7] == APCI1500_ENABLE
				|| data[7] == APCI1500_DISABLE) {

				
				
				
				outb(APCI1500_RW_CPT_TMR1_MODE_SPECIFICATION,
					devpriv->iobase +
					APCI1500_Z8536_CONTROL_REGISTER);
				
				
				
				outb(i_TimerCounterMode,
					devpriv->iobase +
					APCI1500_Z8536_CONTROL_REGISTER);

				
				
				

				outb(APCI1500_RW_CPT_TMR1_TIME_CST_LOW,
					devpriv->iobase +
					APCI1500_Z8536_CONTROL_REGISTER);

				  
				
				  

				outb(data[3],
					devpriv->iobase +
					APCI1500_Z8536_CONTROL_REGISTER);

				   
				
				   

				outb(APCI1500_RW_CPT_TMR1_TIME_CST_HIGH,
					devpriv->iobase +
					APCI1500_Z8536_CONTROL_REGISTER);

				  
				
				  

				data[3] = data[3] >> 8;
				outb(data[3],
					devpriv->iobase +
					APCI1500_Z8536_CONTROL_REGISTER);

				     
				
				     

				outb(APCI1500_RW_MASTER_CONFIGURATION_CONTROL,
					devpriv->iobase +
					APCI1500_Z8536_CONTROL_REGISTER);

				     
				
				     

				i_MasterConfiguration =
					inb(devpriv->iobase +
					APCI1500_Z8536_CONTROL_REGISTER);

				       
				
				       

				i_MasterConfiguration =
					i_MasterConfiguration | 0x40;

				    
				
				    
				outb(APCI1500_RW_MASTER_CONFIGURATION_CONTROL,
					devpriv->iobase +
					APCI1500_Z8536_CONTROL_REGISTER);

				      
				
				      
				outb(i_MasterConfiguration,
					devpriv->iobase +
					APCI1500_Z8536_CONTROL_REGISTER);
					 
				
				
					 

				outb(APCI1500_RW_CPT_TMR1_CMD_STATUS,
					devpriv->iobase +
					APCI1500_Z8536_CONTROL_REGISTER);

				       
				
				       

				outb(0x0,
					devpriv->iobase +
					APCI1500_Z8536_CONTROL_REGISTER);
					  
				
				
					  
				outb(APCI1500_RW_CPT_TMR1_CMD_STATUS,
					devpriv->iobase +
					APCI1500_Z8536_CONTROL_REGISTER);

				      
				
				      
				outb(0x2,
					devpriv->iobase +
					APCI1500_Z8536_CONTROL_REGISTER);
			}	
			else {
				printk("\nError in selection of interrupt enable or disable\n");
				return -EINVAL;
			}	
		}		
		else {
			printk("\nError in selection of reload value\n");
			return -EINVAL;
		}		
		i_TimerCounterWatchdogInterrupt = data[7];
		i_TimerCounter1Init = 1;
		break;

	case COUNTER2:		
		switch (data[2]) {
		case 0:
			data[2] = APCI1500_COUNTER;
			break;
		case 1:
			data[2] = APCI1500_TIMER;
			break;
		default:
			printk("\nThis choice is not a timer nor a counter\n");
			return -EINVAL;
		}		

		
		switch (data[4]) {
		case 0:
			data[4] = APCI1500_CONTINUOUS;
			break;
		case 1:
			data[4] = APCI1500_SINGLE;
			break;
		default:
			printk("\nThis option for single/continuous mode does not exist\n");
			return -EINVAL;
		}		

		
		switch (data[5]) {
		case 0:
			data[5] = APCI1500_SOFTWARE_TRIGGER;
			break;
		case 1:
			data[5] = APCI1500_HARDWARE_TRIGGER;
			break;
		default:
			printk("\nThis choice for software or hardware trigger does not exist\n");
			return -EINVAL;
		}		

		
		switch (data[6]) {
		case 0:
			data[6] = APCI1500_SOFTWARE_GATE;
			break;
		case 1:
			data[6] = APCI1500_HARDWARE_GATE;
			break;
		default:
			printk("\nThis choice for software or hardware gate does not exist\n");
			return -EINVAL;
		}		

		i_TimerCounterMode = data[2] | data[4] | data[5] | data[6] | 7;

			     
		
			     

		if ((data[3] >= 0) && (data[3] <= 65535)) {
			if (data[7] == APCI1500_ENABLE
				|| data[7] == APCI1500_DISABLE) {

				
				
				
				outb(APCI1500_RW_CPT_TMR2_MODE_SPECIFICATION,
					devpriv->iobase +
					APCI1500_Z8536_CONTROL_REGISTER);
				
				
				
				outb(i_TimerCounterMode,
					devpriv->iobase +
					APCI1500_Z8536_CONTROL_REGISTER);

				
				
				

				outb(APCI1500_RW_CPT_TMR2_TIME_CST_LOW,
					devpriv->iobase +
					APCI1500_Z8536_CONTROL_REGISTER);

				  
				
				  

				outb(data[3],
					devpriv->iobase +
					APCI1500_Z8536_CONTROL_REGISTER);

				   
				
				   

				outb(APCI1500_RW_CPT_TMR2_TIME_CST_HIGH,
					devpriv->iobase +
					APCI1500_Z8536_CONTROL_REGISTER);

				  
				
				  

				data[3] = data[3] >> 8;
				outb(data[3],
					devpriv->iobase +
					APCI1500_Z8536_CONTROL_REGISTER);

				     
				
				     

				outb(APCI1500_RW_MASTER_CONFIGURATION_CONTROL,
					devpriv->iobase +
					APCI1500_Z8536_CONTROL_REGISTER);

				     
				
				     

				i_MasterConfiguration =
					inb(devpriv->iobase +
					APCI1500_Z8536_CONTROL_REGISTER);

				       
				
				       

				i_MasterConfiguration =
					i_MasterConfiguration | 0x20;

				    
				
				    
				outb(APCI1500_RW_MASTER_CONFIGURATION_CONTROL,
					devpriv->iobase +
					APCI1500_Z8536_CONTROL_REGISTER);

				      
				
				      
				outb(i_MasterConfiguration,
					devpriv->iobase +
					APCI1500_Z8536_CONTROL_REGISTER);
					 
				
				
					 

				outb(APCI1500_RW_CPT_TMR2_CMD_STATUS,
					devpriv->iobase +
					APCI1500_Z8536_CONTROL_REGISTER);

				       
				
				       

				outb(0x0,
					devpriv->iobase +
					APCI1500_Z8536_CONTROL_REGISTER);
					  
				
				
					  
				outb(APCI1500_RW_CPT_TMR2_CMD_STATUS,
					devpriv->iobase +
					APCI1500_Z8536_CONTROL_REGISTER);

				      
				
				      
				outb(0x2,
					devpriv->iobase +
					APCI1500_Z8536_CONTROL_REGISTER);
			}	
			else {
				printk("\nError in selection of interrupt enable or disable\n");
				return -EINVAL;
			}	
		}		
		else {
			printk("\nError in selection of reload value\n");
			return -EINVAL;
		}		
		i_TimerCounterWatchdogInterrupt = data[7];
		i_TimerCounter2Init = 1;
		break;

	case COUNTER3:		
		switch (data[2]) {
		case 0:
			data[2] = APCI1500_COUNTER;
			break;
		case 1:
			data[2] = APCI1500_WATCHDOG;
			break;
		default:
			printk("\nThis choice is not a watchdog nor a counter\n");
			return -EINVAL;
		}		

		
		switch (data[4]) {
		case 0:
			data[4] = APCI1500_CONTINUOUS;
			break;
		case 1:
			data[4] = APCI1500_SINGLE;
			break;
		default:
			printk("\nThis option for single/continuous mode does not exist\n");
			return -EINVAL;
		}		

		
		switch (data[6]) {
		case 0:
			data[6] = APCI1500_SOFTWARE_GATE;
			break;
		case 1:
			data[6] = APCI1500_HARDWARE_GATE;
			break;
		default:
			printk("\nThis choice for software or hardware gate does not exist\n");
			return -EINVAL;
		}		

		      
		
			  

		if (data[2] == APCI1500_WATCHDOG) {
			     
			
			
			
			     
			i_TimerCounterMode = data[2] | data[4] | 0x54;
		}		
		else {
			i_TimerCounterMode = data[2] | data[4] | data[6] | 7;
		}		
				 
		
			     

		if ((data[3] >= 0) && (data[3] <= 65535)) {
			if (data[7] == APCI1500_ENABLE
				|| data[7] == APCI1500_DISABLE) {

				
				
				
				outb(APCI1500_RW_CPT_TMR3_MODE_SPECIFICATION,
					devpriv->iobase +
					APCI1500_Z8536_CONTROL_REGISTER);
				
				
				
				outb(i_TimerCounterMode,
					devpriv->iobase +
					APCI1500_Z8536_CONTROL_REGISTER);

				
				
				

				outb(APCI1500_RW_CPT_TMR3_TIME_CST_LOW,
					devpriv->iobase +
					APCI1500_Z8536_CONTROL_REGISTER);

				  
				
				  

				outb(data[3],
					devpriv->iobase +
					APCI1500_Z8536_CONTROL_REGISTER);

				   
				
				   

				outb(APCI1500_RW_CPT_TMR3_TIME_CST_HIGH,
					devpriv->iobase +
					APCI1500_Z8536_CONTROL_REGISTER);

				  
				
				  

				data[3] = data[3] >> 8;
				outb(data[3],
					devpriv->iobase +
					APCI1500_Z8536_CONTROL_REGISTER);

				     
				
				     

				outb(APCI1500_RW_MASTER_CONFIGURATION_CONTROL,
					devpriv->iobase +
					APCI1500_Z8536_CONTROL_REGISTER);

				     
				
				     

				i_MasterConfiguration =
					inb(devpriv->iobase +
					APCI1500_Z8536_CONTROL_REGISTER);

				       
				
				       

				i_MasterConfiguration =
					i_MasterConfiguration | 0x10;

				    
				
				    
				outb(APCI1500_RW_MASTER_CONFIGURATION_CONTROL,
					devpriv->iobase +
					APCI1500_Z8536_CONTROL_REGISTER);

				      
				
				      
				outb(i_MasterConfiguration,
					devpriv->iobase +
					APCI1500_Z8536_CONTROL_REGISTER);

				      
				
					  
				if (data[2] == APCI1500_COUNTER) {

					    
					
					
						 
					outb(APCI1500_RW_CPT_TMR3_CMD_STATUS,
						devpriv->iobase +
						APCI1500_Z8536_CONTROL_REGISTER);
					      
					
						  
					outb(0x0,
						devpriv->iobase +
						APCI1500_Z8536_CONTROL_REGISTER);

					      
					
					
						  

					outb(APCI1500_RW_CPT_TMR3_CMD_STATUS,
						devpriv->iobase +
						APCI1500_Z8536_CONTROL_REGISTER);
					     
					
						 
					outb(0x2,
						devpriv->iobase +
						APCI1500_Z8536_CONTROL_REGISTER);

				}	

			}	
			else {
				printk("\nError in selection of interrupt enable or disable\n");
				return -EINVAL;
			}	
		}		
		else {
			printk("\nError in selection of reload value\n");
			return -EINVAL;
		}		
		i_TimerCounterWatchdogInterrupt = data[7];
		i_WatchdogCounter3Init = 1;
		break;

	default:
		printk("\nThe specified counter\timer option does not exist\n");
	}			
	i_CounterLogic = data[2];
	return insn->n;
}

static int i_APCI1500_StartStopTriggerTimerCounterWatchdog(struct comedi_device *dev,
							   struct comedi_subdevice *s,
							   struct comedi_insn *insn,
							   unsigned int *data)
{
	int i_CommandAndStatusValue;

	switch (data[0]) {
	case COUNTER1:
		switch (data[1]) {
		case START:
			if (i_TimerCounter1Init == 1) {
				if (i_TimerCounterWatchdogInterrupt == 1) {
					i_CommandAndStatusValue = 0xC4;	
				}	
				else {
					i_CommandAndStatusValue = 0xE4;	
				}	
					      
				
					      
				i_TimerCounter1Enabled = 1;
						
				
						
				outb(APCI1500_RW_CPT_TMR1_CMD_STATUS,
					devpriv->iobase +
					APCI1500_Z8536_CONTROL_REGISTER);
				outb(i_CommandAndStatusValue,
					devpriv->iobase +
					APCI1500_Z8536_CONTROL_REGISTER);
			}	
			else {
				printk("\nCounter/Timer1 not configured\n");
				return -EINVAL;
			}
			break;

		case STOP:

					      
			
					      

						
			
						
			outb(APCI1500_RW_CPT_TMR1_CMD_STATUS,
				devpriv->iobase +
				APCI1500_Z8536_CONTROL_REGISTER);
			outb(0x00,
				devpriv->iobase +
				APCI1500_Z8536_CONTROL_REGISTER);
			i_TimerCounter1Enabled = 0;
			break;

		case TRIGGER:
			if (i_TimerCounter1Init == 1) {
				if (i_TimerCounter1Enabled == 1) {
						 
					
						 

					i_CommandAndStatusValue = 0x6;
				}	
				else {
						   
					
						   

					i_CommandAndStatusValue = 0x2;
				}	

						
				
						
				outb(APCI1500_RW_CPT_TMR1_CMD_STATUS,
					devpriv->iobase +
					APCI1500_Z8536_CONTROL_REGISTER);
				outb(i_CommandAndStatusValue,
					devpriv->iobase +
					APCI1500_Z8536_CONTROL_REGISTER);
			}	
			else {
				printk("\nCounter/Timer1 not configured\n");
				return -EINVAL;
			}
			break;

		default:
			printk("\nThe specified option for start/stop/trigger does not exist\n");
			return -EINVAL;
		}		
		break;

	case COUNTER2:
		switch (data[1]) {
		case START:
			if (i_TimerCounter2Init == 1) {
				if (i_TimerCounterWatchdogInterrupt == 1) {
					i_CommandAndStatusValue = 0xC4;	
				}	
				else {
					i_CommandAndStatusValue = 0xE4;	
				}	
					      
				
					      
				i_TimerCounter2Enabled = 1;
						
				
						
				outb(APCI1500_RW_CPT_TMR2_CMD_STATUS,
					devpriv->iobase +
					APCI1500_Z8536_CONTROL_REGISTER);
				outb(i_CommandAndStatusValue,
					devpriv->iobase +
					APCI1500_Z8536_CONTROL_REGISTER);
			}	
			else {
				printk("\nCounter/Timer2 not configured\n");
				return -EINVAL;
			}
			break;

		case STOP:

					      
			
					      

						
			
						
			outb(APCI1500_RW_CPT_TMR2_CMD_STATUS,
				devpriv->iobase +
				APCI1500_Z8536_CONTROL_REGISTER);
			outb(0x00,
				devpriv->iobase +
				APCI1500_Z8536_CONTROL_REGISTER);
			i_TimerCounter2Enabled = 0;
			break;
		case TRIGGER:
			if (i_TimerCounter2Init == 1) {
				if (i_TimerCounter2Enabled == 1) {
						 
					
						 

					i_CommandAndStatusValue = 0x6;
				}	
				else {
						   
					
						   

					i_CommandAndStatusValue = 0x2;
				}	

						
				
						
				outb(APCI1500_RW_CPT_TMR2_CMD_STATUS,
					devpriv->iobase +
					APCI1500_Z8536_CONTROL_REGISTER);
				outb(i_CommandAndStatusValue,
					devpriv->iobase +
					APCI1500_Z8536_CONTROL_REGISTER);
			}	
			else {
				printk("\nCounter/Timer2 not configured\n");
				return -EINVAL;
			}
			break;
		default:
			printk("\nThe specified option for start/stop/trigger does not exist\n");
			return -EINVAL;
		}		
		break;
	case COUNTER3:
		switch (data[1]) {
		case START:
			if (i_WatchdogCounter3Init == 1) {

				if (i_TimerCounterWatchdogInterrupt == 1) {
					i_CommandAndStatusValue = 0xC4;	
				}	
				else {
					i_CommandAndStatusValue = 0xE4;	
				}	
					      
				
					      
				i_WatchdogCounter3Enabled = 1;
						
				
						
				outb(APCI1500_RW_CPT_TMR3_CMD_STATUS,
					devpriv->iobase +
					APCI1500_Z8536_CONTROL_REGISTER);
				outb(i_CommandAndStatusValue,
					devpriv->iobase +
					APCI1500_Z8536_CONTROL_REGISTER);

			}	
			else {
				printk("\nWatchdog/Counter3 not configured\n");
				return -EINVAL;
			}
			break;

		case STOP:

					      
			
					      

						
			
						
			outb(APCI1500_RW_CPT_TMR3_CMD_STATUS,
				devpriv->iobase +
				APCI1500_Z8536_CONTROL_REGISTER);
			outb(0x00,
				devpriv->iobase +
				APCI1500_Z8536_CONTROL_REGISTER);
			i_WatchdogCounter3Enabled = 0;
			break;

		case TRIGGER:
			switch (data[2]) {
			case 0:	
				if (i_WatchdogCounter3Init == 1) {
					if (i_WatchdogCounter3Enabled == 1) {
							       
						
							       

						i_CommandAndStatusValue = 0x6;
					}	
					else {
							   
						
							   

						i_CommandAndStatusValue = 0x2;
					}	

						
					
						
					outb(APCI1500_RW_CPT_TMR3_CMD_STATUS,
						devpriv->iobase +
						APCI1500_Z8536_CONTROL_REGISTER);
					outb(i_CommandAndStatusValue,
						devpriv->iobase +
						APCI1500_Z8536_CONTROL_REGISTER);
				}	
				else {
					printk("\nCounter3 not configured\n");
					return -EINVAL;
				}
				break;
			case 1:
				
				if (i_WatchdogCounter3Init == 1) {

						
					
						
					outb(APCI1500_RW_CPT_TMR3_CMD_STATUS,
						devpriv->iobase +
						APCI1500_Z8536_CONTROL_REGISTER);
					outb(0x6,
						devpriv->iobase +
						APCI1500_Z8536_CONTROL_REGISTER);
				}	
				else {
					printk("\nWatchdog 3 not configured\n");
					return -EINVAL;
				}
				break;
			default:
				printk("\nWrong choice of watchdog/counter3\n");
				return -EINVAL;
			}	
			break;
		default:
			printk("\nThe specified option for start/stop/trigger does not exist\n");
			return -EINVAL;
		}		
		break;
	default:
		printk("\nThe specified choice for counter/watchdog/timer does not exist\n");
		return -EINVAL;
	}			
	return insn->n;
}

static int i_APCI1500_ReadCounterTimerWatchdog(struct comedi_device *dev,
					       struct comedi_subdevice *s,
					       struct comedi_insn *insn,
					       unsigned int *data)
{
	int i_CommandAndStatusValue;
	switch (data[0]) {
	case COUNTER1:
		
		if (i_TimerCounter1Init == 1) {
			if (i_TimerCounter1Enabled == 1) {
		  
				
		  

				i_CommandAndStatusValue = 0xC;
			}	
			else {
		    
				
		    

				i_CommandAndStatusValue = 0x8;
			}	

		
			
		
			outb(APCI1500_RW_CPT_TMR1_CMD_STATUS,
				devpriv->iobase +
				APCI1500_Z8536_CONTROL_REGISTER);
			outb(i_CommandAndStatusValue,
				devpriv->iobase +
				APCI1500_Z8536_CONTROL_REGISTER);

		 
			
		 
			outb(APCI1500_R_CPT_TMR1_VALUE_HIGH,
				devpriv->iobase +
				APCI1500_Z8536_CONTROL_REGISTER);
			data[0] =
				inb(devpriv->iobase +
				APCI1500_Z8536_CONTROL_REGISTER);
			data[0] = data[0] << 8;
			data[0] = data[0] & 0xff00;
			outb(APCI1500_R_CPT_TMR1_VALUE_LOW,
				devpriv->iobase +
				APCI1500_Z8536_CONTROL_REGISTER);
			data[0] =
				data[0] | inb(devpriv->iobase +
				APCI1500_Z8536_CONTROL_REGISTER);
		}		
		else {
			printk("\nTimer/Counter1 not configured\n");
			return -EINVAL;
		}		
		break;
	case COUNTER2:
		
		if (i_TimerCounter2Init == 1) {
			if (i_TimerCounter2Enabled == 1) {
		  
				
		  

				i_CommandAndStatusValue = 0xC;
			}	
			else {
		    
				
		    

				i_CommandAndStatusValue = 0x8;
			}	

		
			
		
			outb(APCI1500_RW_CPT_TMR2_CMD_STATUS,
				devpriv->iobase +
				APCI1500_Z8536_CONTROL_REGISTER);
			outb(i_CommandAndStatusValue,
				devpriv->iobase +
				APCI1500_Z8536_CONTROL_REGISTER);

		 
			
		 
			outb(APCI1500_R_CPT_TMR2_VALUE_HIGH,
				devpriv->iobase +
				APCI1500_Z8536_CONTROL_REGISTER);
			data[0] =
				inb(devpriv->iobase +
				APCI1500_Z8536_CONTROL_REGISTER);
			data[0] = data[0] << 8;
			data[0] = data[0] & 0xff00;
			outb(APCI1500_R_CPT_TMR2_VALUE_LOW,
				devpriv->iobase +
				APCI1500_Z8536_CONTROL_REGISTER);
			data[0] =
				data[0] | inb(devpriv->iobase +
				APCI1500_Z8536_CONTROL_REGISTER);
		}		
		else {
			printk("\nTimer/Counter2 not configured\n");
			return -EINVAL;
		}		
		break;
	case COUNTER3:
		
		if (i_WatchdogCounter3Init == 1) {
			if (i_WatchdogCounter3Enabled == 1) {
		  
				
		  

				i_CommandAndStatusValue = 0xC;
			}	
			else {
		    
				
		    

				i_CommandAndStatusValue = 0x8;
			}	

		
			
		
			outb(APCI1500_RW_CPT_TMR3_CMD_STATUS,
				devpriv->iobase +
				APCI1500_Z8536_CONTROL_REGISTER);
			outb(i_CommandAndStatusValue,
				devpriv->iobase +
				APCI1500_Z8536_CONTROL_REGISTER);

		 
			
		 
			outb(APCI1500_R_CPT_TMR3_VALUE_HIGH,
				devpriv->iobase +
				APCI1500_Z8536_CONTROL_REGISTER);
			data[0] =
				inb(devpriv->iobase +
				APCI1500_Z8536_CONTROL_REGISTER);
			data[0] = data[0] << 8;
			data[0] = data[0] & 0xff00;
			outb(APCI1500_R_CPT_TMR3_VALUE_LOW,
				devpriv->iobase +
				APCI1500_Z8536_CONTROL_REGISTER);
			data[0] =
				data[0] | inb(devpriv->iobase +
				APCI1500_Z8536_CONTROL_REGISTER);
		}		
		else {
			printk("\nWatchdogCounter3 not configured\n");
			return -EINVAL;
		}		
		break;
	default:
		printk("\nThe choice of timer/counter/watchdog does not exist\n");
		return -EINVAL;
	}			

	return insn->n;
}

static int i_APCI1500_ReadInterruptMask(struct comedi_device *dev,
					struct comedi_subdevice *s,
					struct comedi_insn *insn,
					unsigned int *data)
{
	data[0] = i_InterruptMask;
	data[1] = i_InputChannel;
	i_InterruptMask = 0;
	return insn->n;
}

static int i_APCI1500_ConfigureInterrupt(struct comedi_device *dev,
					 struct comedi_subdevice *s,
					 struct comedi_insn *insn,
					 unsigned int *data)
{
	unsigned int ui_Status;
	int i_RegValue;
	int i_Constant;
	devpriv->tsk_Current = current;
	outl(0x0, devpriv->i_IobaseAmcc + 0x38);
	if (data[0] == 1) {
		i_Constant = 0xC0;
	}			
	else {
		if (data[0] == 0) {
			i_Constant = 0x00;
		}		
		else {
			printk("\nThe parameter passed to driver is in error for enabling the voltage interrupt\n");
			return -EINVAL;
		}		
	}			

	 
	
	 
	outb(APCI1500_RW_PORT_B_SPECIFICATION,
		devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
	i_RegValue = inb(devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
	outb(APCI1500_RW_PORT_B_SPECIFICATION,
		devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
      
	
      
	i_RegValue = (i_RegValue & 0xF9) | APCI1500_OR;

	outb(i_RegValue, devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
       
	
       
	outb(APCI1500_RW_PORT_B_COMMAND_AND_STATUS,
		devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
	
	
	
	outb(0xC0, devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
	
	
	
	outb(APCI1500_RW_PORT_B_PATTERN_POLARITY,
		devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
	outb(i_Constant, devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
	
	
	
	outb(APCI1500_RW_PORT_B_PATTERN_TRANSITION,
		devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
	outb(i_Constant, devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
	
	
	
	outb(APCI1500_RW_PORT_B_PATTERN_MASK,
		devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
	outb(i_Constant, devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);

	
	
	
	outb(APCI1500_RW_PORT_A_COMMAND_AND_STATUS,
		devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
	i_RegValue = inb(devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
	outb(APCI1500_RW_PORT_A_COMMAND_AND_STATUS,
		devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
	 
	
	 

	i_RegValue = (i_RegValue & 0x0F) | 0x20;
	outb(i_RegValue, devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
	
	
	
	outb(APCI1500_RW_PORT_B_COMMAND_AND_STATUS,
		devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
	i_RegValue = inb(devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
	outb(APCI1500_RW_PORT_B_COMMAND_AND_STATUS,
		devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
	 
	
	 

	i_RegValue = (i_RegValue & 0x0F) | 0x20;
	outb(i_RegValue, devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);

	
	
	
	outb(APCI1500_RW_CPT_TMR1_CMD_STATUS,
		devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
	i_RegValue = inb(devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
	outb(APCI1500_RW_CPT_TMR1_CMD_STATUS,
		devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
	 
	
	 

	i_RegValue = (i_RegValue & 0x0F) | 0x20;
	outb(i_RegValue, devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);

	 
	
	
	outb(APCI1500_RW_CPT_TMR2_CMD_STATUS,
		devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
	i_RegValue = inb(devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
	outb(APCI1500_RW_CPT_TMR2_CMD_STATUS,
		devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
	 
	
	 

	i_RegValue = (i_RegValue & 0x0F) | 0x20;
	outb(i_RegValue, devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);

	
	
	
	outb(APCI1500_RW_CPT_TMR3_CMD_STATUS,
		devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
	i_RegValue = inb(devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
	outb(APCI1500_RW_CPT_TMR3_CMD_STATUS,
		devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
	 
	
	 

	i_RegValue = (i_RegValue & 0x0F) | 0x20;
	outb(i_RegValue, devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);

	 
	
	 
	outb(APCI1500_RW_MASTER_INTERRUPT_CONTROL,
		devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
	
	
	
	outb(0xD0, devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);

      
	
      
	outl(0x3000, devpriv->i_IobaseAmcc + 0x38);
	ui_Status = inl(devpriv->i_IobaseAmcc + 0x10);
	ui_Status = inl(devpriv->i_IobaseAmcc + 0x38);
	outl(0x23000, devpriv->i_IobaseAmcc + 0x38);

	return insn->n;
}

static void v_APCI1500_Interrupt(int irq, void *d)
{

	struct comedi_device *dev = d;
	unsigned int ui_InterruptStatus = 0;
	int i_RegValue = 0;
	i_InterruptMask = 0;

 
	
 
	ui_InterruptStatus = inl(devpriv->i_IobaseAmcc + 0x38);

  
	
  
	if ((ui_InterruptStatus & 0x800000) == 0x800000) {
      
		
      
      
		
      
		
	
		
	
		

   
		
   
		outb(APCI1500_RW_PORT_A_COMMAND_AND_STATUS,
			devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
		i_RegValue =
			inb(devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
		if ((i_RegValue & 0x60) == 0x60) {
	   
			
	   
			outb(APCI1500_RW_PORT_A_COMMAND_AND_STATUS,
				devpriv->iobase +
				APCI1500_Z8536_CONTROL_REGISTER);
	    
			
	    
			i_RegValue = (i_RegValue & 0x0F) | 0x20;
			outb(i_RegValue,
				devpriv->iobase +
				APCI1500_Z8536_CONTROL_REGISTER);
			i_InterruptMask = i_InterruptMask | 1;
			if (i_Logic == APCI1500_OR_PRIORITY) {
				outb(APCI1500_RW_PORT_A_SPECIFICATION,
					devpriv->iobase +
					APCI1500_Z8536_CONTROL_REGISTER);
				i_RegValue =
					inb(devpriv->iobase +
					APCI1500_Z8536_CONTROL_REGISTER);

	      
				
	      
				outb(APCI1500_RW_PORT_A_INTERRUPT_CONTROL,
					devpriv->iobase +
					APCI1500_Z8536_CONTROL_REGISTER);
				i_RegValue =
					inb(devpriv->iobase +
					APCI1500_Z8536_CONTROL_REGISTER);

				i_InputChannel = 1 + (i_RegValue >> 1);

			}	
			else {
				i_InputChannel = 0;
			}	
		}		

	   
		
	   
		outb(APCI1500_RW_PORT_B_COMMAND_AND_STATUS,
			devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
		i_RegValue =
			inb(devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
		if ((i_RegValue & 0x60) == 0x60) {
	     
			
	     
			outb(APCI1500_RW_PORT_B_COMMAND_AND_STATUS,
				devpriv->iobase +
				APCI1500_Z8536_CONTROL_REGISTER);
	     
			
	     
			i_RegValue = (i_RegValue & 0x0F) | 0x20;
			outb(i_RegValue,
				devpriv->iobase +
				APCI1500_Z8536_CONTROL_REGISTER);
			printk("\n\n\n");
	     
			
	     
			i_RegValue =
				inb((unsigned int) devpriv->iobase +
				APCI1500_Z8536_PORT_B);

			i_RegValue = i_RegValue & 0xC0;
	      
			
	      

			if (i_RegValue) {
				
		     
				
		     
				outl(0x0, devpriv->i_IobaseAmcc + 0x38);

				if (i_RegValue & 0x80) {
					i_InterruptMask =
						i_InterruptMask | 0x40;
				}	

				if (i_RegValue & 0x40) {
					i_InterruptMask =
						i_InterruptMask | 0x80;
				}	
			}	
			else {
				i_InterruptMask = i_InterruptMask | 2;
			}	
		}		

		
		
		
		outb(APCI1500_RW_CPT_TMR1_CMD_STATUS,
			devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
		i_RegValue =
			inb(devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
		if ((i_RegValue & 0x60) == 0x60) {
		   
			
		   
			outb(APCI1500_RW_CPT_TMR1_CMD_STATUS,
				devpriv->iobase +
				APCI1500_Z8536_CONTROL_REGISTER);
		   
			
		   
			i_RegValue = (i_RegValue & 0x0F) | 0x20;
			outb(i_RegValue,
				devpriv->iobase +
				APCI1500_Z8536_CONTROL_REGISTER);
			i_InterruptMask = i_InterruptMask | 4;
		}		
		
		
		
		outb(APCI1500_RW_CPT_TMR2_CMD_STATUS,
			devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
		i_RegValue =
			inb(devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
		if ((i_RegValue & 0x60) == 0x60) {
		   
			
		   
			outb(APCI1500_RW_CPT_TMR2_CMD_STATUS,
				devpriv->iobase +
				APCI1500_Z8536_CONTROL_REGISTER);
		   
			
		   
			i_RegValue = (i_RegValue & 0x0F) | 0x20;
			outb(i_RegValue,
				devpriv->iobase +
				APCI1500_Z8536_CONTROL_REGISTER);
			i_InterruptMask = i_InterruptMask | 8;
		}		

		
		
		
		outb(APCI1500_RW_CPT_TMR3_CMD_STATUS,
			devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
		i_RegValue =
			inb(devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
		if ((i_RegValue & 0x60) == 0x60) {
		   
			
		   
			outb(APCI1500_RW_CPT_TMR3_CMD_STATUS,
				devpriv->iobase +
				APCI1500_Z8536_CONTROL_REGISTER);
		   
			
		   
			i_RegValue = (i_RegValue & 0x0F) | 0x20;
			outb(i_RegValue,
				devpriv->iobase +
				APCI1500_Z8536_CONTROL_REGISTER);
			if (i_CounterLogic == APCI1500_COUNTER) {
				i_InterruptMask = i_InterruptMask | 0x10;
			}	
			else {
				i_InterruptMask = i_InterruptMask | 0x20;
			}
		}		

		send_sig(SIGIO, devpriv->tsk_Current, 0);	
	       
		
	       

	       
		
	       
		outb(APCI1500_RW_MASTER_INTERRUPT_CONTROL,
			devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
	       
		
	       
		outb(0xD0, devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
	}			
	else {
		printk("\nInterrupt from unknown source\n");

	}			
	return;
}

static int i_APCI1500_Reset(struct comedi_device *dev)
{
	int i_DummyRead = 0;
	i_TimerCounter1Init = 0;
	i_TimerCounter2Init = 0;
	i_WatchdogCounter3Init = 0;
	i_Event1Status = 0;
	i_Event2Status = 0;
	i_TimerCounterWatchdogInterrupt = 0;
	i_Logic = 0;
	i_CounterLogic = 0;
	i_InterruptMask = 0;
	i_InputChannel = 0;
	i_TimerCounter1Enabled = 0;
	i_TimerCounter2Enabled = 0;
	i_WatchdogCounter3Enabled = 0;

    
	
    
	i_DummyRead = inb(devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
	outb(0, devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
	i_DummyRead = inb(devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
	outb(0, devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
	outb(1, devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
	outb(0, devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);

 
	
 
	outb(APCI1500_RW_MASTER_CONFIGURATION_CONTROL,
		devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
	outb(0xF4, devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);

	
	
	
	outb(APCI1500_RW_PORT_A_SPECIFICATION,
		devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
	outb(0x10, devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);

	
	outb(APCI1500_RW_PORT_A_DATA_PCITCH_POLARITY,
		devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
	
	outb(0xFF, devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);

	
	outb(APCI1500_RW_PORT_A_DATA_DIRECTION,
		devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
	
	outb(0xFF, devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
	
	outb(APCI1500_RW_PORT_A_COMMAND_AND_STATUS,
		devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
	
	outb(0x20, devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
	
	outb(APCI1500_RW_PORT_A_COMMAND_AND_STATUS,
		devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
	
	outb(0xE0, devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
	
	outb(APCI1500_RW_PORT_A_HANDSHAKE_SPECIFICATION,
		devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
	
	outb(0, devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);

	 
	
	 
	outb(APCI1500_RW_PORT_B_SPECIFICATION,
		devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
	outb(0x10, devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
	
	outb(APCI1500_RW_PORT_B_DATA_PCITCH_POLARITY,
		devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
	
	outb(0x7F, devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
	
	outb(APCI1500_RW_PORT_B_DATA_DIRECTION,
		devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
	
	outb(0xFF, devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
	
	outb(APCI1500_RW_PORT_B_COMMAND_AND_STATUS,
		devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
	
	outb(0x20, devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
	
	outb(APCI1500_RW_PORT_B_COMMAND_AND_STATUS,
		devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
	
	outb(0xE0, devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
	
	outb(APCI1500_RW_PORT_B_HANDSHAKE_SPECIFICATION,
		devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
	
	outb(0, devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);

	   
	
	   
	outb(APCI1500_RW_PORT_C_DATA_PCITCH_POLARITY,
		devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
	
	outb(0x9, devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
	
	outb(APCI1500_RW_PORT_C_DATA_DIRECTION,
		devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
	
	outb(0x0E, devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
	
	outb(APCI1500_RW_PORT_C_SPECIAL_IO_CONTROL,
		devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
	
	outb(0, devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
	   
	
	   
	outb(APCI1500_RW_CPT_TMR1_CMD_STATUS,
		devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
	
	outb(0x20, devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
	
	outb(APCI1500_RW_CPT_TMR1_CMD_STATUS,
		devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
	
	outb(0xE0, devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
	   
	
	   
	outb(APCI1500_RW_CPT_TMR2_CMD_STATUS,
		devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
	
	outb(0x20, devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
	
	outb(APCI1500_RW_CPT_TMR2_CMD_STATUS,
		devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
	
	outb(0xE0, devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
	  
	
	  
	outb(APCI1500_RW_CPT_TMR3_CMD_STATUS,
		devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
	
	outb(0x20, devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
	
	outb(APCI1500_RW_CPT_TMR3_CMD_STATUS,
		devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
	
	outb(0xE0, devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
	 
	
	 
	outb(APCI1500_RW_MASTER_INTERRUPT_CONTROL,
		devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
	
	outb(0, devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
	
	outw(0x0, devpriv->i_IobaseAddon + APCI1500_DIGITAL_OP);
 
	
 
	outb(APCI1500_RW_MASTER_INTERRUPT_CONTROL,
		devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
	outb(0, devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
 
	
 
	outb(APCI1500_RW_PORT_A_COMMAND_AND_STATUS,
		devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
	outb(0x00, devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
	
 
	outb(APCI1500_RW_PORT_B_COMMAND_AND_STATUS,
		devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
	outb(0x00, devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
	
 
	outb(APCI1500_RW_CPT_TMR1_CMD_STATUS,
		devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
	outb(0x00, devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
	
 
	outb(APCI1500_RW_CPT_TMR2_CMD_STATUS,
		devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
	outb(0x00, devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
	outb(APCI1500_RW_CPT_TMR3_CMD_STATUS,
		devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
	outb(0x00, devpriv->iobase + APCI1500_Z8536_CONTROL_REGISTER);
	return 0;
}
