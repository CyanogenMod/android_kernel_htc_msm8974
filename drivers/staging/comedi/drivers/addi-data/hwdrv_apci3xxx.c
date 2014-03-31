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

#include "hwdrv_apci3xxx.h"


static int i_APCI3XXX_TestConversionStarted(struct comedi_device *dev)
{
	if ((readl(devpriv->dw_AiBase + 8) & 0x80000UL) == 0x80000UL)
		return 1;
	else
		return 0;

}

static int i_APCI3XXX_AnalogInputConfigOperatingMode(struct comedi_device *dev,
						     struct comedi_subdevice *s,
						     struct comedi_insn *insn,
						     unsigned int *data)
{
	int i_ReturnValue = insn->n;
	unsigned char b_TimeBase = 0;
	unsigned char b_SingleDiff = 0;
	unsigned int dw_ReloadValue = 0;
	unsigned int dw_TestReloadValue = 0;

	
	
	

	if (insn->n == 4) {
	   
		
	   

		b_SingleDiff = (unsigned char) data[1];

	   
		
	   

		b_TimeBase = (unsigned char) data[2];

	   
		
	   

		dw_ReloadValue = (unsigned int) data[3];

	   
		
	   

		if ((this_board->b_AvailableConvertUnit & (1 << b_TimeBase)) !=
			0) {
	      
			
	      

			if (dw_ReloadValue <= 65535) {
				dw_TestReloadValue = dw_ReloadValue;

				if (b_TimeBase == 1) {
					dw_TestReloadValue =
						dw_TestReloadValue * 1000UL;
				}
				if (b_TimeBase == 2) {
					dw_TestReloadValue =
						dw_TestReloadValue * 1000000UL;
				}

		 
				
		 

				if (dw_TestReloadValue >=
					devpriv->s_EeParameters.
					ui_MinAcquisitiontimeNs) {
					if ((b_SingleDiff == APCI3XXX_SINGLE)
						|| (b_SingleDiff ==
							APCI3XXX_DIFF)) {
						if (((b_SingleDiff == APCI3XXX_SINGLE)
						        && (devpriv->s_EeParameters.i_NbrAiChannel == 0))
						    || ((b_SingleDiff == APCI3XXX_DIFF)
							&& (this_board->i_NbrAiChannelDiff == 0))
						    ) {
			   
							
			   

							printk("Single/Diff selection error\n");
							i_ReturnValue = -1;
						} else {
			   
							
			   

							if (i_APCI3XXX_TestConversionStarted(dev) == 0) {
								devpriv->
									ui_EocEosConversionTime
									=
									(unsigned int)
									dw_ReloadValue;
								devpriv->
									b_EocEosConversionTimeBase
									=
									b_TimeBase;
								devpriv->
									b_SingelDiff
									=
									b_SingleDiff;
								devpriv->
									b_AiInitialisation
									= 1;

			      
								
			      

								writel((unsigned int)b_TimeBase,
									devpriv->dw_AiBase + 36);

			      
								
			      

								writel(dw_ReloadValue, devpriv->dw_AiBase + 32);
							} else {
			      
								
			      

								printk("Any conversion started\n");
								i_ReturnValue =
									-10;
							}
						}
					} else {
		       
						
		       

						printk("Single/Diff selection error\n");
						i_ReturnValue = -1;
					}
				} else {
		    
					
		    

					printk("Convert time value selection error\n");
					i_ReturnValue = -3;
				}
			} else {
		 
				
		 

				printk("Convert time value selection error\n");
				i_ReturnValue = -3;
			}
		} else {
	      
			
	      

			printk("Convert time base unity selection error\n");
			i_ReturnValue = -2;
		}
	} else {
	   
		
	   

		printk("Buffer size error\n");
		i_ReturnValue = -101;
	}

	return i_ReturnValue;
}

static int i_APCI3XXX_InsnConfigAnalogInput(struct comedi_device *dev,
					    struct comedi_subdevice *s,
					    struct comedi_insn *insn,
					    unsigned int *data)
{
	int i_ReturnValue = insn->n;

	
	
	

	if (insn->n >= 1) {
		switch ((unsigned char) data[0]) {
		case APCI3XXX_CONFIGURATION:
			i_ReturnValue =
				i_APCI3XXX_AnalogInputConfigOperatingMode(dev,
				s, insn, data);
			break;

		default:
			i_ReturnValue = -100;
			printk("Config command error %d\n", data[0]);
			break;
		}
	} else {
	   
		
	   

		printk("Buffer size error\n");
		i_ReturnValue = -101;
	}

	return i_ReturnValue;
}

static int i_APCI3XXX_InsnReadAnalogInput(struct comedi_device *dev,
					  struct comedi_subdevice *s,
					  struct comedi_insn *insn,
					  unsigned int *data)
{
	int i_ReturnValue = insn->n;
	unsigned char b_Configuration = (unsigned char) CR_RANGE(insn->chanspec);
	unsigned char b_Channel = (unsigned char) CR_CHAN(insn->chanspec);
	unsigned int dw_Temp = 0;
	unsigned int dw_Configuration = 0;
	unsigned int dw_AcquisitionCpt = 0;
	unsigned char b_Interrupt = 0;

	
	
	

	if (devpriv->b_AiInitialisation) {
	   
		
	   

		if (((b_Channel < devpriv->s_EeParameters.i_NbrAiChannel)
				&& (devpriv->b_SingelDiff == APCI3XXX_SINGLE))
			|| ((b_Channel < this_board->i_NbrAiChannelDiff)
				&& (devpriv->b_SingelDiff == APCI3XXX_DIFF))) {
	      
			
	      

			if (b_Configuration > 7) {
		 
				
		 

				i_ReturnValue = -4;
				printk("Channel %d range %d selection error\n",
					b_Channel, b_Configuration);
			}
		} else {
	      
			
	      

			i_ReturnValue = -3;
			printk("Channel %d selection error\n", b_Channel);
		}

	   
		
	   

		if (i_ReturnValue >= 0) {
	      
			
	      

			if ((b_Interrupt != 0) || ((b_Interrupt == 0)
					&& (insn->n >= 1))) {
		 
				
		 

				if (i_APCI3XXX_TestConversionStarted(dev) == 0) {
		    
					
		    

					writel(0x10000UL, devpriv->dw_AiBase + 12);

		    
					
		    

					dw_Temp = readl(devpriv->dw_AiBase + 4);
					dw_Temp = dw_Temp & 0xFFFFFEF0UL;

		    
					
		    

					writel(dw_Temp, devpriv->dw_AiBase + 4);

		    
					
		    

					dw_Configuration =
						(b_Configuration & 3) |
						((unsigned int) (b_Configuration >> 2)
						<< 6) | ((unsigned int) devpriv->
						b_SingelDiff << 7);

		    
					
		    

					writel(dw_Configuration,
					       devpriv->dw_AiBase + 0);

		    
					
		    

					writel(dw_Temp | 0x100UL,
					       devpriv->dw_AiBase + 4);
					writel((unsigned int) b_Channel,
					       devpriv->dw_AiBase + 0);

		    
					
		    

					writel(dw_Temp, devpriv->dw_AiBase + 4);

		    
					
		    

					writel(1, devpriv->dw_AiBase + 48);

		    
					
		    

					devpriv->b_EocEosInterrupt =
						b_Interrupt;

		    
					
		    

					devpriv->ui_AiNbrofChannels = 1;

		    
					
		    

					if (b_Interrupt == 0) {
						for (dw_AcquisitionCpt = 0;
							dw_AcquisitionCpt <
							insn->n;
							dw_AcquisitionCpt++) {
			  
							
			  

							writel(0x80000UL, devpriv->dw_AiBase + 8);

			  
							
			  

							do {
								dw_Temp = readl(devpriv->dw_AiBase + 20);
								dw_Temp = dw_Temp & 1;
							} while (dw_Temp != 1);

			  
							
			  

							data[dw_AcquisitionCpt] = (unsigned int)readl(devpriv->dw_AiBase + 28);
						}
					} else {
		       
						
		       

						writel(0x180000UL, devpriv->dw_AiBase + 8);
					}
				} else {
		    
					
		    

					printk("Any conversion started\n");
					i_ReturnValue = -10;
				}
			} else {
		 
				
		 

				printk("Buffer size error\n");
				i_ReturnValue = -101;
			}
		}
	} else {
	   
		
	   

		printk("Operating mode not configured\n");
		i_ReturnValue = -1;
	}
	return i_ReturnValue;
}


static void v_APCI3XXX_Interrupt(int irq, void *d)
{
	struct comedi_device *dev = d;
	unsigned char b_CopyCpt = 0;
	unsigned int dw_Status = 0;

	
	
	

	dw_Status = readl(devpriv->dw_AiBase + 16);
	if ( (dw_Status & 0x2UL) == 0x2UL) {
	   
		
	   

		writel(dw_Status, devpriv->dw_AiBase + 16);

	   
		
	   

		if (devpriv->b_EocEosInterrupt == 1) {
	      
			
	      

			for (b_CopyCpt = 0;
				b_CopyCpt < devpriv->ui_AiNbrofChannels;
				b_CopyCpt++) {
				devpriv->ui_AiReadData[b_CopyCpt] =
					(unsigned int)readl(devpriv->dw_AiBase + 28);
			}

	      
			
	      

			devpriv->b_EocEosInterrupt = 2;

	      
			
	      

			send_sig(SIGIO, devpriv->tsk_Current, 0);
		}
	}
}


static int i_APCI3XXX_InsnWriteAnalogOutput(struct comedi_device *dev,
					    struct comedi_subdevice *s,
					    struct comedi_insn *insn,
					    unsigned int *data)
{
	unsigned char b_Range = (unsigned char) CR_RANGE(insn->chanspec);
	unsigned char b_Channel = (unsigned char) CR_CHAN(insn->chanspec);
	unsigned int dw_Status = 0;
	int i_ReturnValue = insn->n;

	
	
	

	if (insn->n >= 1) {
	   
		
	   

		if (b_Channel < devpriv->s_EeParameters.i_NbrAoChannel) {
	      
			
	      

			if (b_Range < 2) {
		 
				
		 

				writel(b_Range, devpriv->dw_AiBase + 96);

		 
				
		 

				writel((data[0] << 8) | b_Channel,
					devpriv->dw_AiBase + 100);

		 
				
		 

				do {
					dw_Status = readl(devpriv->dw_AiBase + 96);
				} while ((dw_Status & 0x100) != 0x100);
			} else {
		 
				
		 

				i_ReturnValue = -4;
				printk("Channel %d range %d selection error\n",
					b_Channel, b_Range);
			}
		} else {
	      
			
	      

			i_ReturnValue = -3;
			printk("Channel %d selection error\n", b_Channel);
		}
	} else {
	   
		
	   

		printk("Buffer size error\n");
		i_ReturnValue = -101;
	}

	return i_ReturnValue;
}


static int i_APCI3XXX_InsnConfigInitTTLIO(struct comedi_device *dev,
					  struct comedi_subdevice *s,
					  struct comedi_insn *insn,
					  unsigned int *data)
{
	int i_ReturnValue = insn->n;
	unsigned char b_Command = 0;

	
	
	

	if (insn->n >= 1) {
	   
		
		

		b_Command = (unsigned char) data[0];

	   
		
	   

		if (b_Command == APCI3XXX_TTL_INIT_DIRECTION_PORT2) {
	      
			
	      

			if ((b_Command == APCI3XXX_TTL_INIT_DIRECTION_PORT2)
				&& (insn->n != 2)) {
		 
				
		 

				printk("Buffer size error\n");
				i_ReturnValue = -101;
			}
		} else {
	      
			
	      

			printk("Command selection error\n");
			i_ReturnValue = -100;
		}
	} else {
	   
		
	   

		printk("Buffer size error\n");
		i_ReturnValue = -101;
	}

	
	
	

	if ((i_ReturnValue >= 0)
		&& (b_Command == APCI3XXX_TTL_INIT_DIRECTION_PORT2)) {
	   
		
	   

		if ((data[1] == 0) || (data[1] == 0xFF)) {
	      
			
	      

			devpriv->ul_TTLPortConfiguration[0] =
				devpriv->ul_TTLPortConfiguration[0] | data[1];
		} else {
	      
			
	      

			printk("Port 2 direction selection error\n");
			i_ReturnValue = -1;
		}
	}

	
	
	

	if (i_ReturnValue >= 0) {
	   
		
	   

		if (b_Command == APCI3XXX_TTL_INIT_DIRECTION_PORT2) {
	      
			
	      

			outl(data[1], devpriv->iobase + 224);
		}
	}

	return i_ReturnValue;
}


static int i_APCI3XXX_InsnBitsTTLIO(struct comedi_device *dev,
				    struct comedi_subdevice *s,
				    struct comedi_insn *insn,
				    unsigned int *data)
{
	int i_ReturnValue = insn->n;
	unsigned char b_ChannelCpt = 0;
	unsigned int dw_ChannelMask = 0;
	unsigned int dw_BitMask = 0;
	unsigned int dw_Status = 0;

	
	
	

	if (insn->n >= 2) {
	   
		
	   

		dw_ChannelMask = data[0];
		dw_BitMask = data[1];

	   
		
	   

		if (((dw_ChannelMask & 0XFF00FF00) == 0) &&
			(((devpriv->ul_TTLPortConfiguration[0] & 0xFF) == 0xFF)
				|| (((devpriv->ul_TTLPortConfiguration[0] &
							0xFF) == 0)
					&& ((dw_ChannelMask & 0XFF0000) ==
						0)))) {
	      
			
	      

			if (dw_ChannelMask) {
		 
				
		 

				if (dw_ChannelMask & 0xFF) {
		    
					
		    

					dw_Status = inl(devpriv->iobase + 80);

					for (b_ChannelCpt = 0; b_ChannelCpt < 8;
						b_ChannelCpt++) {
						if ((dw_ChannelMask >>
								b_ChannelCpt) &
							1) {
							dw_Status =
								(dw_Status &
								(0xFF - (1 << b_ChannelCpt))) | (dw_BitMask & (1 << b_ChannelCpt));
						}
					}

					outl(dw_Status, devpriv->iobase + 80);
				}

		 
				
		 

				if (dw_ChannelMask & 0xFF0000) {
					dw_BitMask = dw_BitMask >> 16;
					dw_ChannelMask = dw_ChannelMask >> 16;

		    
					
		    

					dw_Status = inl(devpriv->iobase + 112);

					for (b_ChannelCpt = 0; b_ChannelCpt < 8;
						b_ChannelCpt++) {
						if ((dw_ChannelMask >>
								b_ChannelCpt) &
							1) {
							dw_Status =
								(dw_Status &
								(0xFF - (1 << b_ChannelCpt))) | (dw_BitMask & (1 << b_ChannelCpt));
						}
					}

					outl(dw_Status, devpriv->iobase + 112);
				}
			}

	      
			
	      

			data[1] = inl(devpriv->iobase + 80);

	      
			
	      

			data[1] = data[1] | (inl(devpriv->iobase + 64) << 8);

	      
			
	      

			if ((devpriv->ul_TTLPortConfiguration[0] & 0xFF) == 0) {
				data[1] =
					data[1] | (inl(devpriv->iobase +
						96) << 16);
			} else {
				data[1] =
					data[1] | (inl(devpriv->iobase +
						112) << 16);
			}
		} else {
	      
			
	      

			printk("Channel mask error\n");
			i_ReturnValue = -4;
		}
	} else {
	   
		
	   

		printk("Buffer size error\n");
		i_ReturnValue = -101;
	}

	return i_ReturnValue;
}

static int i_APCI3XXX_InsnReadTTLIO(struct comedi_device *dev,
				    struct comedi_subdevice *s,
				    struct comedi_insn *insn,
				    unsigned int *data)
{
	unsigned char b_Channel = (unsigned char) CR_CHAN(insn->chanspec);
	int i_ReturnValue = insn->n;
	unsigned int *pls_ReadData = data;

	
	
	

	if (insn->n >= 1) {
	   
		
	   

		if (b_Channel < 8) {
	      
			
	      

			pls_ReadData[0] = inl(devpriv->iobase + 80);
			pls_ReadData[0] = (pls_ReadData[0] >> b_Channel) & 1;
		} else {
	      
			
	      

			if ((b_Channel > 7) && (b_Channel < 16)) {
		 
				
		 

				pls_ReadData[0] = inl(devpriv->iobase + 64);
				pls_ReadData[0] =
					(pls_ReadData[0] >> (b_Channel -
						8)) & 1;
			} else {
		 
				
		 

				if ((b_Channel > 15) && (b_Channel < 24)) {
		    
					
		    

					if ((devpriv->ul_TTLPortConfiguration[0]
							& 0xFF) == 0) {
						pls_ReadData[0] =
							inl(devpriv->iobase +
							96);
						pls_ReadData[0] =
							(pls_ReadData[0] >>
							(b_Channel - 16)) & 1;
					} else {
						pls_ReadData[0] =
							inl(devpriv->iobase +
							112);
						pls_ReadData[0] =
							(pls_ReadData[0] >>
							(b_Channel - 16)) & 1;
					}
				} else {
		    
					
		    

					i_ReturnValue = -3;
					printk("Channel %d selection error\n",
						b_Channel);
				}
			}
		}
	} else {
	   
		
	   

		printk("Buffer size error\n");
		i_ReturnValue = -101;
	}

	return i_ReturnValue;
}


static int i_APCI3XXX_InsnWriteTTLIO(struct comedi_device *dev,
				     struct comedi_subdevice *s,
				     struct comedi_insn *insn,
				     unsigned int *data)
{
	int i_ReturnValue = insn->n;
	unsigned char b_Channel = (unsigned char) CR_CHAN(insn->chanspec);
	unsigned char b_State = 0;
	unsigned int dw_Status = 0;

	
	
	

	if (insn->n >= 1) {
		b_State = (unsigned char) data[0];

	   
		
	   

		if (b_Channel < 8) {
	      
			
	      

			dw_Status = inl(devpriv->iobase + 80);
			dw_Status =
				(dw_Status & (0xFF -
					(1 << b_Channel))) | ((b_State & 1) <<
				b_Channel);
			outl(dw_Status, devpriv->iobase + 80);
		} else {
	      
			
	      

			if ((b_Channel > 15) && (b_Channel < 24)) {
		 
				
		 

				if ((devpriv->ul_TTLPortConfiguration[0] & 0xFF)
					== 0xFF) {
		    
					
		    

					dw_Status = inl(devpriv->iobase + 112);
					dw_Status =
						(dw_Status & (0xFF -
							(1 << (b_Channel -
									16)))) |
						((b_State & 1) << (b_Channel -
							16));
					outl(dw_Status, devpriv->iobase + 112);
				} else {
		    
					
		    

					i_ReturnValue = -3;
					printk("Channel %d selection error\n",
						b_Channel);
				}
			} else {
		 
				
		 

				i_ReturnValue = -3;
				printk("Channel %d selection error\n",
					b_Channel);
			}
		}
	} else {
	   
		
	   

		printk("Buffer size error\n");
		i_ReturnValue = -101;
	}

	return i_ReturnValue;
}



static int i_APCI3XXX_InsnReadDigitalInput(struct comedi_device *dev,
					   struct comedi_subdevice *s,
					   struct comedi_insn *insn,
					   unsigned int *data)
{
	int i_ReturnValue = insn->n;
	unsigned char b_Channel = (unsigned char) CR_CHAN(insn->chanspec);
	unsigned int dw_Temp = 0;

	
	
	

	if (b_Channel <= devpriv->s_EeParameters.i_NbrDiChannel) {
	   
		
	   

		if (insn->n >= 1) {
			dw_Temp = inl(devpriv->iobase + 32);
			*data = (dw_Temp >> b_Channel) & 1;
		} else {
	      
			
	      

			printk("Buffer size error\n");
			i_ReturnValue = -101;
		}
	} else {
	   
		
	   

		printk("Channel selection error\n");
		i_ReturnValue = -3;
	}

	return i_ReturnValue;
}

static int i_APCI3XXX_InsnBitsDigitalInput(struct comedi_device *dev,
					   struct comedi_subdevice *s,
					   struct comedi_insn *insn,
					   unsigned int *data)
{
	int i_ReturnValue = insn->n;
	unsigned int dw_Temp = 0;

	
	
	

	if (insn->n >= 1) {
		dw_Temp = inl(devpriv->iobase + 32);
		*data = dw_Temp & 0xf;
	} else {
	   
		
	   

		printk("Buffer size error\n");
		i_ReturnValue = -101;
	}

	return i_ReturnValue;
}


static int i_APCI3XXX_InsnBitsDigitalOutput(struct comedi_device *dev,
					    struct comedi_subdevice *s,
					    struct comedi_insn *insn,
					    unsigned int *data)
{
	int i_ReturnValue = insn->n;
	unsigned char b_ChannelCpt = 0;
	unsigned int dw_ChannelMask = 0;
	unsigned int dw_BitMask = 0;
	unsigned int dw_Status = 0;

	
	
	

	if (insn->n >= 2) {
	   
		
	   

		dw_ChannelMask = data[0];
		dw_BitMask = data[1];

	   
		
	   

		if ((dw_ChannelMask & 0XFFFFFFF0) == 0) {
	      
			
	      

			if (dw_ChannelMask & 0xF) {
		 
				
		 

				dw_Status = inl(devpriv->iobase + 48);

				for (b_ChannelCpt = 0; b_ChannelCpt < 4;
					b_ChannelCpt++) {
					if ((dw_ChannelMask >> b_ChannelCpt) &
						1) {
						dw_Status =
							(dw_Status & (0xF -
								(1 << b_ChannelCpt))) | (dw_BitMask & (1 << b_ChannelCpt));
					}
				}

				outl(dw_Status, devpriv->iobase + 48);
			}

	      
			
	      

			data[1] = inl(devpriv->iobase + 48);
		} else {
	      
			
	      

			printk("Channel mask error\n");
			i_ReturnValue = -4;
		}
	} else {
	   
		
	   

		printk("Buffer size error\n");
		i_ReturnValue = -101;
	}

	return i_ReturnValue;
}


static int i_APCI3XXX_InsnWriteDigitalOutput(struct comedi_device *dev,
					     struct comedi_subdevice *s,
					     struct comedi_insn *insn,
					     unsigned int *data)
{
	int i_ReturnValue = insn->n;
	unsigned char b_Channel = CR_CHAN(insn->chanspec);
	unsigned char b_State = 0;
	unsigned int dw_Status = 0;

	
	
	

	if (insn->n >= 1) {
	   
		
	   

		if (b_Channel < devpriv->s_EeParameters.i_NbrDoChannel) {
	      
			
	      

			b_State = (unsigned char) data[0];

	      
			
	      

			dw_Status = inl(devpriv->iobase + 48);

			dw_Status =
				(dw_Status & (0xF -
					(1 << b_Channel))) | ((b_State & 1) <<
				b_Channel);
			outl(dw_Status, devpriv->iobase + 48);
		} else {
	      
			
	      

			printk("Channel selection error\n");
			i_ReturnValue = -3;
		}
	} else {
	   
		
	   

		printk("Buffer size error\n");
		i_ReturnValue = -101;
	}

	return i_ReturnValue;
}


static int i_APCI3XXX_InsnReadDigitalOutput(struct comedi_device *dev,
					    struct comedi_subdevice *s,
					    struct comedi_insn *insn,
					    unsigned int *data)
{
	int i_ReturnValue = insn->n;
	unsigned char b_Channel = CR_CHAN(insn->chanspec);
	unsigned int dw_Status = 0;

	
	
	

	if (insn->n >= 1) {
	   
		
	   

		if (b_Channel < devpriv->s_EeParameters.i_NbrDoChannel) {
	      
			
	      

			dw_Status = inl(devpriv->iobase + 48);

			dw_Status = (dw_Status >> b_Channel) & 1;
			*data = dw_Status;
		} else {
	      
			
	      

			printk("Channel selection error\n");
			i_ReturnValue = -3;
		}
	} else {
	   
		
	   

		printk("Buffer size error\n");
		i_ReturnValue = -101;
	}

	return i_ReturnValue;
}


static int i_APCI3XXX_Reset(struct comedi_device *dev)
{
	unsigned char b_Cpt = 0;

	
	
	

	disable_irq(dev->irq);

	
	
	

	devpriv->b_EocEosInterrupt = 0;

	
	
	

	writel(0, devpriv->dw_AiBase + 8);

	
	
	

	writel(readl(devpriv->dw_AiBase + 16), devpriv->dw_AiBase + 16);

	
	
	

	readl(devpriv->dw_AiBase + 20);

	
	
	

	for (b_Cpt = 0; b_Cpt < 16; b_Cpt++) {
		readl(devpriv->dw_AiBase + 28);
	}

	
	
	

	enable_irq(dev->irq);

	return 0;
}
