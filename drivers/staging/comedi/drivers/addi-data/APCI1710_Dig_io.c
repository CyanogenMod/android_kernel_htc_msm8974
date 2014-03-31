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

#include "APCI1710_Dig_io.h"


int i_APCI1710_InsnConfigDigitalIO(struct comedi_device *dev, struct comedi_subdevice *s,
	struct comedi_insn *insn, unsigned int *data)
{
	unsigned char b_ModulNbr, b_ChannelAMode, b_ChannelBMode;
	unsigned char b_MemoryOnOff, b_ConfigType;
	int i_ReturnValue = 0;
	unsigned int dw_WriteConfig = 0;

	b_ModulNbr = (unsigned char) CR_AREF(insn->chanspec);
	b_ConfigType = (unsigned char) data[0];	
	b_ChannelAMode = (unsigned char) data[1];
	b_ChannelBMode = (unsigned char) data[2];
	b_MemoryOnOff = (unsigned char) data[1];	
	i_ReturnValue = insn->n;

		
	
		

	if (b_ModulNbr >= 4) {
		DPRINTK("Module Number invalid\n");
		i_ReturnValue = -2;
		return i_ReturnValue;
	}
	switch (b_ConfigType) {
	case APCI1710_DIGIO_MEMORYONOFF:

		if (b_MemoryOnOff)	
		{
		 
			
		 

			devpriv->s_ModuleInfo[b_ModulNbr].
				s_DigitalIOInfo.b_OutputMemoryEnabled = 1;

		 
			
		 
			devpriv->s_ModuleInfo[b_ModulNbr].
				s_DigitalIOInfo.dw_OutputMemory = 0;
		} else		
		{
		 
			
		 

			devpriv->s_ModuleInfo[b_ModulNbr].
				s_DigitalIOInfo.b_OutputMemoryEnabled = 0;
		}
		break;

	case APCI1710_DIGIO_INIT:

	
		
	

		if ((devpriv->s_BoardInfos.
				dw_MolduleConfiguration[b_ModulNbr] &
				0xFFFF0000UL) == APCI1710_DIGITAL_IO) {

	
			
	

			if ((b_ChannelAMode == 0) || (b_ChannelAMode == 1)) {
	
				
	

				if ((b_ChannelBMode == 0)
					|| (b_ChannelBMode == 1)) {
					devpriv->s_ModuleInfo[b_ModulNbr].
						s_DigitalIOInfo.b_DigitalInit =
						1;

	
					
	

					devpriv->s_ModuleInfo[b_ModulNbr].
						s_DigitalIOInfo.
						b_ChannelAMode = b_ChannelAMode;

	
					
	

					devpriv->s_ModuleInfo[b_ModulNbr].
						s_DigitalIOInfo.
						b_ChannelBMode = b_ChannelBMode;

	
					
	

					dw_WriteConfig =
						(unsigned int) (b_ChannelAMode |
						(b_ChannelBMode * 2));

	
					
	

					outl(dw_WriteConfig,
						devpriv->s_BoardInfos.
						ui_Address + 4 +
						(64 * b_ModulNbr));

				} else {
	
					
	
					DPRINTK("Bi-directional channel B configuration error\n");
					i_ReturnValue = -5;
				}

			} else {
	
				
	
				DPRINTK("Bi-directional channel A configuration error\n");
				i_ReturnValue = -4;

			}

		} else {
	
			
	
			DPRINTK("The module is not a digital I/O module\n");
			i_ReturnValue = -3;
		}
	}			
	printk("Return Value %d\n", i_ReturnValue);
	return i_ReturnValue;
}



int i_APCI1710_InsnReadDigitalIOChlValue(struct comedi_device *dev,
	struct comedi_subdevice *s, struct comedi_insn *insn, unsigned int *data)
{
	int i_ReturnValue = 0;
	unsigned int dw_StatusReg;
	unsigned char b_ModulNbr, b_InputChannel;
	unsigned char *pb_ChannelStatus;
	b_ModulNbr = (unsigned char) CR_AREF(insn->chanspec);
	b_InputChannel = (unsigned char) CR_CHAN(insn->chanspec);
	data[0] = 0;
	pb_ChannelStatus = (unsigned char *) &data[0];
	i_ReturnValue = insn->n;

	
	
	

	if (b_ModulNbr < 4) {
	   
		
	   

		if ((devpriv->s_BoardInfos.
				dw_MolduleConfiguration[b_ModulNbr] &
				0xFFFF0000UL) == APCI1710_DIGITAL_IO) {
	      
			
	      

			if (b_InputChannel <= 6) {
		 
				
		 

				if (devpriv->s_ModuleInfo[b_ModulNbr].
					s_DigitalIOInfo.b_DigitalInit == 1) {
		    
					
		    

					if (b_InputChannel > 4) {
		       
						
		       

						if (b_InputChannel == 5) {
			  
							
			  

							if (devpriv->
								s_ModuleInfo
								[b_ModulNbr].
								s_DigitalIOInfo.
								b_ChannelAMode
								!= 0) {
			     
								
			     

								i_ReturnValue =
									-6;
							}
						}	
						else {
			  
							
			  

							if (devpriv->
								s_ModuleInfo
								[b_ModulNbr].
								s_DigitalIOInfo.
								b_ChannelBMode
								!= 0) {
			     
								
			     

								i_ReturnValue =
									-7;
							}
						}	
					}	

		    
					
		    

					if (i_ReturnValue >= 0) {
		       
						
		       


						dw_StatusReg =
							inl(devpriv->
							s_BoardInfos.
							ui_Address +
							(64 * b_ModulNbr));

						*pb_ChannelStatus =
							(unsigned char) ((dw_StatusReg ^
								0x1C) >>
							b_InputChannel) & 1;

					}	
				} else {
		    
					
		    
					DPRINTK("Digital I/O not initialised\n");
					i_ReturnValue = -5;
				}
			} else {
		 
				
		 
				DPRINTK("Selected digital input error\n");
				i_ReturnValue = -4;
			}
		} else {
	      
			
	      
			DPRINTK("The module is not a digital I/O module\n");
			i_ReturnValue = -3;
		}
	} else {
	   
		
	   
		DPRINTK("Module number error\n");
		i_ReturnValue = -2;
	}

	return i_ReturnValue;
}



int i_APCI1710_InsnWriteDigitalIOChlOnOff(struct comedi_device *dev,
	struct comedi_subdevice *s, struct comedi_insn *insn, unsigned int *data)
{
	int i_ReturnValue = 0;
	unsigned int dw_WriteValue = 0;
	unsigned char b_ModulNbr, b_OutputChannel;
	i_ReturnValue = insn->n;
	b_ModulNbr = CR_AREF(insn->chanspec);
	b_OutputChannel = CR_CHAN(insn->chanspec);

	
	
	

	if (b_ModulNbr < 4) {
	   
		
	   

		if ((devpriv->s_BoardInfos.
				dw_MolduleConfiguration[b_ModulNbr] &
				0xFFFF0000UL) == APCI1710_DIGITAL_IO) {
	      
			
	      

			if (devpriv->s_ModuleInfo[b_ModulNbr].
				s_DigitalIOInfo.b_DigitalInit == 1) {
		 
				
		 

				switch (b_OutputChannel) {
		    
					
		    

				case 0:
					break;

		    
					
		    

				case 1:
					if (devpriv->s_ModuleInfo[b_ModulNbr].
						s_DigitalIOInfo.
						b_ChannelAMode != 1) {
			    
						
			    

						i_ReturnValue = -6;
					}
					break;

		    
					
		    

				case 2:
					if (devpriv->s_ModuleInfo[b_ModulNbr].
						s_DigitalIOInfo.
						b_ChannelBMode != 1) {
			    
						
			    

						i_ReturnValue = -7;
					}
					break;

				default:
			 
					
			 

					i_ReturnValue = -4;
					break;
				}

		 
				
		 

				if (i_ReturnValue >= 0) {

			
					
		    
					if (data[0]) {
		    
						
		    

						if (devpriv->
							s_ModuleInfo
							[b_ModulNbr].
							s_DigitalIOInfo.
							b_OutputMemoryEnabled ==
							1) {
							dw_WriteValue =
								devpriv->
								s_ModuleInfo
								[b_ModulNbr].
								s_DigitalIOInfo.
								dw_OutputMemory
								| (1 <<
								b_OutputChannel);

							devpriv->
								s_ModuleInfo
								[b_ModulNbr].
								s_DigitalIOInfo.
								dw_OutputMemory
								= dw_WriteValue;
						} else {
							dw_WriteValue =
								1 <<
								b_OutputChannel;
						}
					}	
					else {
						if (devpriv->
							s_ModuleInfo
							[b_ModulNbr].
							s_DigitalIOInfo.
							b_OutputMemoryEnabled ==
							1) {
							dw_WriteValue =
								devpriv->
								s_ModuleInfo
								[b_ModulNbr].
								s_DigitalIOInfo.
								dw_OutputMemory
								& (0xFFFFFFFFUL
								-
								(1 << b_OutputChannel));

							devpriv->
								s_ModuleInfo
								[b_ModulNbr].
								s_DigitalIOInfo.
								dw_OutputMemory
								= dw_WriteValue;
						} else {
							
							
							
							
							i_ReturnValue = -8;
						}

					}
					
					
					

*/
					outl(dw_WriteValue,
						devpriv->s_BoardInfos.
						ui_Address + (64 * b_ModulNbr));
				}
			} else {
		 
				
		 

				i_ReturnValue = -5;
			}
		} else {
	      
			
	      

			i_ReturnValue = -3;
		}
	} else {
	   
		
	   

		i_ReturnValue = -2;
	}

	return i_ReturnValue;
}


int i_APCI1710_InsnBitsDigitalIOPortOnOff(struct comedi_device *dev,
	struct comedi_subdevice *s, struct comedi_insn *insn, unsigned int *data)
{
	int i_ReturnValue = 0;
	unsigned int dw_WriteValue = 0;
	unsigned int dw_StatusReg;
	unsigned char b_ModulNbr, b_PortValue;
	unsigned char b_PortOperation, b_PortOnOFF;

	unsigned char *pb_PortValue;

	b_ModulNbr = (unsigned char) CR_AREF(insn->chanspec);
	b_PortOperation = (unsigned char) data[0];	
	b_PortOnOFF = (unsigned char) data[1];	
	b_PortValue = (unsigned char) data[2];	
	i_ReturnValue = insn->n;
	pb_PortValue = (unsigned char *) &data[0];

	switch (b_PortOperation) {
	case APCI1710_INPUT:
		
		
		

		if (b_ModulNbr < 4) {
			
			
			

			if ((devpriv->s_BoardInfos.
					dw_MolduleConfiguration[b_ModulNbr] &
					0xFFFF0000UL) == APCI1710_DIGITAL_IO) {
				
				
				

				if (devpriv->s_ModuleInfo[b_ModulNbr].
					s_DigitalIOInfo.b_DigitalInit == 1) {
					
					
					


					dw_StatusReg =
						inl(devpriv->s_BoardInfos.
						ui_Address + (64 * b_ModulNbr));
					*pb_PortValue =
						(unsigned char) (dw_StatusReg ^ 0x1C);

				} else {
					
					
					

					i_ReturnValue = -4;
				}
			} else {
				
				
				

				i_ReturnValue = -3;
			}
		} else {
	   
			
	   

			i_ReturnValue = -2;
		}

		break;

	case APCI1710_OUTPUT:
	
		
	

		if (b_ModulNbr < 4) {
	   
			
	   

			if ((devpriv->s_BoardInfos.
					dw_MolduleConfiguration[b_ModulNbr] &
					0xFFFF0000UL) == APCI1710_DIGITAL_IO) {
	      
				
	      

				if (devpriv->s_ModuleInfo[b_ModulNbr].
					s_DigitalIOInfo.b_DigitalInit == 1) {
		 
					
		 

					if (b_PortValue <= 7) {
		    
						
		    

		    
						
		    

						if ((b_PortValue & 2) == 2) {
							if (devpriv->
								s_ModuleInfo
								[b_ModulNbr].
								s_DigitalIOInfo.
								b_ChannelAMode
								!= 1) {
			  
								
			  

								i_ReturnValue =
									-6;
							}
						}	

						
						
						

						if ((b_PortValue & 4) == 4) {
							if (devpriv->
								s_ModuleInfo
								[b_ModulNbr].
								s_DigitalIOInfo.
								b_ChannelBMode
								!= 1) {
								
								
								

								i_ReturnValue =
									-7;
							}
						}	

						
						
						

						if (i_ReturnValue >= 0) {

							

							switch (b_PortOnOFF) {
								
								
								

							case APCI1710_ON:

								
								
								

								if (devpriv->
									s_ModuleInfo
									[b_ModulNbr].
									s_DigitalIOInfo.
									b_OutputMemoryEnabled
									== 1) {
									dw_WriteValue
										=
										devpriv->
										s_ModuleInfo
										[b_ModulNbr].
										s_DigitalIOInfo.
										dw_OutputMemory
										|
										b_PortValue;

									devpriv->
										s_ModuleInfo
										[b_ModulNbr].
										s_DigitalIOInfo.
										dw_OutputMemory
										=
										dw_WriteValue;
								} else {
									dw_WriteValue
										=
										b_PortValue;
								}
								break;

								
							case APCI1710_OFF:

			   
								
		       

								if (devpriv->
									s_ModuleInfo
									[b_ModulNbr].
									s_DigitalIOInfo.
									b_OutputMemoryEnabled
									== 1) {
									dw_WriteValue
										=
										devpriv->
										s_ModuleInfo
										[b_ModulNbr].
										s_DigitalIOInfo.
										dw_OutputMemory
										&
										(0xFFFFFFFFUL
										-
										b_PortValue);

									devpriv->
										s_ModuleInfo
										[b_ModulNbr].
										s_DigitalIOInfo.
										dw_OutputMemory
										=
										dw_WriteValue;
								} else {
									
									
									

									i_ReturnValue
										=
										-8;
								}
							}	

							
							
							


							outl(dw_WriteValue,
								devpriv->
								s_BoardInfos.
								ui_Address +
								(64 * b_ModulNbr));
						}
					} else {
						
						
						

						i_ReturnValue = -4;
					}
				} else {
					
					
					

					i_ReturnValue = -5;
				}
			} else {
	      
				
	      

				i_ReturnValue = -3;
			}
		} else {
	   
			
	   

			i_ReturnValue = -2;
		}
		break;

	default:
		i_ReturnValue = -9;
		DPRINTK("NO INPUT/OUTPUT specified\n");
	}			
	return i_ReturnValue;
}
